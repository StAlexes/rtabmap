/*
Copyright (c) 2010-2014, Mathieu Labbe - IntRoLab - Universite de Sherbrooke
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Universite de Sherbrooke nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "rtabmap/core/Signature.h"
#include "rtabmap/core/EpipolarGeometry.h"
#include "rtabmap/core/Memory.h"
#include "rtabmap/core/util3d.h"
#include <opencv2/highgui/highgui.hpp>

#include <rtabmap/utilite/UtiLite.h>

namespace rtabmap
{

Signature::~Signature()
{
	ULOGGER_DEBUG("id=%d", _id);
}

Signature::Signature(
		int id,
		int mapId,
		const std::multimap<int, cv::KeyPoint> & words,
		const std::multimap<int, pcl::PointXYZ> & words3, // in base_link frame (localTransform applied)
		const Transform & pose,
		const std::vector<unsigned char> & depth2D, // in base_link frame
		const std::vector<unsigned char> & image, // in camera_link frame
		const std::vector<unsigned char> & depth, // in camera_link frame
		float fx,
		float fy,
		float cx,
		float cy,
		const Transform & localTransform) :
	_id(id),
	_mapId(mapId),
	_weight(0),
	_saved(false),
	_modified(true),
	_neighborsModified(true),
	_words(words),
	_enabled(false),
	_image(image),
	_depth(depth),
	_depth2D(depth2D),
	_fx(fx),
	_fy(fy),
	_cx(cx),
	_cy(cy),
	_pose(pose),
	_localTransform(localTransform),
	_words3(words3)
{
}

void Signature::addNeighbors(const std::map<int, Transform> & neighbors)
{
	for(std::map<int, Transform>::const_iterator i=neighbors.begin(); i!=neighbors.end(); ++i)
	{
		this->addNeighbor(i->first, i->second);
	}
}

void Signature::addNeighbor(int neighbor, const Transform & transform)
{
	UDEBUG("Add neighbor %d to %d", neighbor, this->id());
	_neighbors.insert(std::pair<int, Transform>(neighbor, transform));
	_neighborsModified = true;
}

void Signature::removeNeighbor(int neighborId)
{
	int count = _neighbors.erase(neighborId);
	if(count)
	{
		_neighborsModified = true;
	}
}

void Signature::removeNeighbors()
{
	if(_neighbors.size())
		_neighborsModified = true;
	_neighbors.clear();
}

void Signature::changeNeighborIds(int idFrom, int idTo)
{
	std::map<int, Transform>::iterator iter = _neighbors.find(idFrom);
	if(iter != _neighbors.end())
	{
		Transform t = iter->second;
		_neighbors.erase(iter);
		_neighbors.insert(std::pair<int, Transform>(idTo, t));
		_neighborsModified = true;
	}
	UDEBUG("(%d) neighbor ids changed from %d to %d", _id, idFrom, idTo);
}

void Signature::addLoopClosureId(int loopClosureId, const Transform & transform)
{
	if(loopClosureId && _loopClosureIds.insert(std::pair<int, Transform>(loopClosureId, transform)).second)
	{
		_neighborsModified=true;
	}
}

void Signature::addChildLoopClosureId(int childLoopClosureId, const Transform & transform)
{
	if(childLoopClosureId && _childLoopClosureIds.insert(std::pair<int, Transform>(childLoopClosureId, transform)).second)
	{
		_neighborsModified=true;
	}
}

void Signature::changeLoopClosureId(int idFrom, int idTo)
{
	std::map<int, Transform>::iterator iter = _loopClosureIds.find(idFrom);
	if(iter != _loopClosureIds.end())
	{
		Transform t = iter->second;
		_loopClosureIds.erase(iter);
		_loopClosureIds.insert(std::pair<int, Transform>(idTo, t));
		_neighborsModified = true;
	}
	UDEBUG("(%d) loop closure ids changed from %d to %d", _id, idFrom, idTo);
}


float Signature::compareTo(const Signature * s) const
{
	float similarity = 0.0f;
	const std::multimap<int, cv::KeyPoint> & words = s->getWords();
	if(words.size() != 0 && _words.size() != 0)
	{
		std::list<std::pair<int, std::pair<cv::KeyPoint, cv::KeyPoint> > > pairs;
		int totalWords = _words.size()>words.size()?_words.size():words.size();
		EpipolarGeometry::findPairs(words, _words, pairs);

		similarity = float(pairs.size()) / float(totalWords);
	}
	return similarity;
}

void Signature::changeWordsRef(int oldWordId, int activeWordId)
{
	std::list<cv::KeyPoint> kps = uValues(_words, oldWordId);
	if(kps.size())
	{
		std::list<pcl::PointXYZ> pts = uValues(_words3, oldWordId);
		_words.erase(oldWordId);
		_words3.erase(oldWordId);
		_wordsChanged.insert(std::make_pair(oldWordId, activeWordId));
		for(std::list<cv::KeyPoint>::const_iterator iter=kps.begin(); iter!=kps.end(); ++iter)
		{
			_words.insert(std::pair<int, cv::KeyPoint>(activeWordId, (*iter)));
		}
		for(std::list<pcl::PointXYZ>::const_iterator iter=pts.begin(); iter!=pts.end(); ++iter)
		{
			_words3.insert(std::pair<int, pcl::PointXYZ>(activeWordId, (*iter)));
		}
	}
}

bool Signature::isBadSignature() const
{
	return !_words.size();
}

void Signature::removeAllWords()
{
	_words.clear();
	_words3.clear();
}

void Signature::removeWord(int wordId)
{
	_words.erase(wordId);
	_words3.erase(wordId);
}

void Signature::setDepth(const std::vector<unsigned char> & depth, float fx, float fy, float cx, float cy)
{
	UASSERT_MSG(depth.empty() || (!depth.empty() && fx > 0.0f && fy > 0.0f && cx >= 0.0f && cy >= 0.0f), uFormat("fx=%f fy=%f cx=%f cy=%f",fx,fy,cx,cy).c_str());
	_depth = depth;
	_fx=fx;
	_fy=fy;
	_cx=cx;
	_cy=cy;
}

} //namespace rtabmap
