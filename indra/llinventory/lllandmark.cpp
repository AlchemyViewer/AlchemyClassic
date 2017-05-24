// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file lllandmark.cpp
 * @brief Landmark asset class
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "lllandmark.h"

#include <errno.h>

#include "message.h"
#include "llregionhandle.h"

std::pair<LLUUID, U64> LLLandmark::mLocalRegion;
LLLandmark::region_map_t LLLandmark::mRegions;
LLLandmark::region_callback_map_t LLLandmark::sRegionCallbackMap;

LLLandmark::LLLandmark() :
	mGlobalPositionKnown(false)
{
}

LLLandmark::LLLandmark(const LLVector3d& pos) :
	mGlobalPositionKnown(true),
	mGlobalPos( pos )
{
}

bool LLLandmark::getGlobalPos(LLVector3d& pos)
{
	if(mGlobalPositionKnown)
	{
		pos = mGlobalPos;
	}
	else if(mRegionID.notNull())
	{
		F32 g_x = -1.0;
		F32 g_y = -1.0;
		if(mRegionID == mLocalRegion.first)
		{
			from_region_handle(mLocalRegion.second, &g_x, &g_y);
		}
		else
		{
			region_map_t::iterator it = mRegions.find(mRegionID);
			if(it != mRegions.end())
			{
				from_region_handle((*it).second.mRegionHandle, &g_x, &g_y);
			}
		}
		if((g_x > 0.f) && (g_y > 0.f))
		{
			pos.mdV[0] = g_x + mRegionPos.mV[0];
			pos.mdV[1] = g_y + mRegionPos.mV[1];
			pos.mdV[2] = mRegionPos.mV[2];
			setGlobalPos(pos);
		}
	}
	return mGlobalPositionKnown;
}

void LLLandmark::setGlobalPos(const LLVector3d& pos)
{
	mGlobalPos = pos;
	mGlobalPositionKnown = true;
}

bool LLLandmark::getRegionID(LLUUID& region_id)
{
	if(mRegionID.notNull())
	{
		region_id = mRegionID;
		return true;
	}
	return false;
}

LLVector3 LLLandmark::getRegionPos() const
{
	return mRegionPos;
}


// static
LLLandmark* LLLandmark::constructFromString(const char *buffer)
{
	const char* cur = buffer;
	S32 chars_read = 0;
	S32 count = 0;
	U32 version = 0;

	// read version 
	count = sscanf( cur, "Landmark version %u\n%n", &version, &chars_read );
	if(count != 1)
	{
		goto error;
	}

	if(version == 1)
	{
		LLVector3d pos;
		cur += chars_read;
		// read position
		count = sscanf( cur, "position %lf %lf %lf\n%n", pos.mdV+VX, pos.mdV+VY, pos.mdV+VZ, &chars_read );
		if( count != 3 )
		{
			goto error;
		}
		//cur += chars_read;
		// LL_INFOS() << "Landmark read: " << pos << LL_ENDL;
		
		return new LLLandmark(pos);
	}
	else if(version == 2)
	{
		// *NOTE: Changing the buffer size will require changing the
		// scanf call below.
		char region_id_str[MAX_STRING];	/* Flawfinder: ignore */
		LLVector3 pos;
		cur += chars_read;
		count = sscanf(	/* Flawfinder: ignore */
			cur,
			"region_id %254s\n%n",
			region_id_str, &chars_read);
		if(count != 1) goto error;
		cur += chars_read;
		count = sscanf(cur, "local_pos %f %f %f\n%n", pos.mV+VX, pos.mV+VY, pos.mV+VZ, &chars_read);
		if(count != 3) goto error;
		//cur += chars_read;
		LLLandmark* lm = new LLLandmark;
		lm->mRegionID.set(region_id_str);
		lm->mRegionPos = pos;
		return lm;
	}

 error:
	LL_INFOS() << "Bad Landmark Asset: bad _DATA_ block." << LL_ENDL;
	return nullptr;
}


// static
void LLLandmark::registerCallbacks(LLMessageSystem* msg)
{
	msg->setHandlerFunc("RegionIDAndHandleReply", &processRegionIDAndHandle);
}

// static
void LLLandmark::requestRegionHandle(
	LLMessageSystem* msg,
	const LLHost& upstream_host,
	const LLUUID& region_id,
	region_handle_callback_t callback)
{
	if(region_id.isNull())
	{
		// don't bother with checking - it's 0.
		LL_DEBUGS() << "requestRegionHandle: null" << LL_ENDL;
		if(callback)
		{
			const U64 U64_ZERO = 0;
			callback(region_id, U64_ZERO);
		}
	}
	else
	{
		if(region_id == mLocalRegion.first)
		{
			LL_DEBUGS() << "requestRegionHandle: local" << LL_ENDL;
			if(callback)
			{
				callback(region_id, mLocalRegion.second);
			}
		}
		else
		{
			region_map_t::iterator it = mRegions.find(region_id);
			if(it == mRegions.end())
			{
				LL_DEBUGS() << "requestRegionHandle: upstream" << LL_ENDL;
				if(callback)
				{
					region_callback_map_t::value_type vt(region_id, callback);
					sRegionCallbackMap.insert(vt);
				}
				LL_DEBUGS() << "Landmark requesting information about: "
						 << region_id << LL_ENDL;
				msg->newMessage("RegionHandleRequest");
				msg->nextBlock("RequestBlock");
				msg->addUUID("RegionID", region_id);
				msg->sendReliable(upstream_host);
			}
			else if(callback)
			{
				// we have the answer locally - just call the callack.
				LL_DEBUGS() << "requestRegionHandle: ready" << LL_ENDL;
				callback(region_id, (*it).second.mRegionHandle);
			}
		}
	}

	// As good a place as any to expire old entries.
	expireOldEntries();
}

// static
void LLLandmark::setRegionHandle(const LLUUID& region_id, U64 region_handle)
{
	mLocalRegion.first = region_id;
	mLocalRegion.second = region_handle;
}


// static
void LLLandmark::processRegionIDAndHandle(LLMessageSystem* msg, void**)
{
	LLUUID region_id;
	msg->getUUID("ReplyBlock", "RegionID", region_id);
	mRegions.erase(region_id);
	CacheInfo info;
	const F32 CACHE_EXPIRY_SECONDS = 60.0f * 10.0f; // ten minutes
	info.mTimer.setTimerExpirySec(CACHE_EXPIRY_SECONDS);
	msg->getU64("ReplyBlock", "RegionHandle", info.mRegionHandle);
	region_map_t::value_type vt(region_id, info);
	mRegions.insert(vt);

#if LL_DEBUG
	U32 grid_x, grid_y;
	grid_from_region_handle(info.mRegionHandle, &grid_x, &grid_y);
	LL_DEBUGS() << "Landmark got reply for region: " << region_id << " "
			 << grid_x << "," << grid_y << LL_ENDL;
#endif

	// make all the callbacks here.
	region_callback_map_t::iterator it;
	while((it = sRegionCallbackMap.find(region_id)) != sRegionCallbackMap.end())
	{
		(*it).second(region_id, info.mRegionHandle);
		sRegionCallbackMap.erase(it);
	}
}

// static
void LLLandmark::expireOldEntries()
{
	for(region_map_t::iterator it = mRegions.begin(); it != mRegions.end(); )
	{
		if((*it).second.mTimer.hasExpired())
		{
			mRegions.erase(it++);
		}
		else
		{
			++it;
		}
	}
}
