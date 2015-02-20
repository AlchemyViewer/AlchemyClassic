/**
* @file alavatarcolormgr.cpp
* @brief ALChatCommand implementation for chat input commands
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Copyright (C) 2013 Drake Arconis
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
* $/LicenseInfo$
**/

#include "llviewerprecompiledheaders.h"

#include "alavatarcolormgr.h"

// system includes

// lib includes
#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "lluicolor.h"
#include "lluicolortable.h"
#include "lluuid.h"
#include "v4color.h"

// viewer includes
#include "llcallingcard.h"
#include "llmutelist.h"

void ALAvatarColorMgr::addOrUpdateCustomColor(const LLUUID& id, EAvatarColors color_val)
{
	uuid_color_umap_t::iterator it = mCustomColors.find(id);
	if (it != mCustomColors.cend())
	{
		it->second = color_val;
	}
	else
	{
		mCustomColors.insert(std::make_pair(id, color_val));
	}
}

void ALAvatarColorMgr::clearCustomColor(const LLUUID& id)
{
	const uuid_color_umap_t::const_iterator it = mCustomColors.find(id);
	if (it != mCustomColors.cend())
	{
		mCustomColors.erase(it);
	}
}

const LLColor4& ALAvatarColorMgr::getColor(const LLUUID& id)
{
	uuid_color_umap_t::const_iterator user_col_it = mCustomColors.find(id);
	if (user_col_it != mCustomColors.cend())
	{
		static LLUIColor avatar_color_1 = LLUIColorTable::instance().getColor("AvatarColor1", LLColor4::red);
		static LLUIColor avatar_color_2 = LLUIColorTable::instance().getColor("AvatarColor2", LLColor4::green);
		static LLUIColor avatar_color_3 = LLUIColorTable::instance().getColor("AvatarColor3", LLColor4::blue);
		static LLUIColor avatar_color_4 = LLUIColorTable::instance().getColor("AvatarColor4", LLColor4::yellow);
		const EAvatarColors color_val = user_col_it->second;
		switch (color_val)
		{
		case E_FIRST_COLOR:
			return  avatar_color_1.get();
			break;
		case E_SECOND_COLOR:
			return avatar_color_2.get();
			break;
		case E_THIRD_COLOR:
			return avatar_color_3.get();
			break;
		case E_FOURTH_COLOR:
			return avatar_color_4.get();
			break;
		default:
			return LLColor4::white;
		}
	}
	else
	{
		static LLUIColor map_avatar_color = LLUIColorTable::instance().getColor("MapAvatarColor", LLColor4::white);
		static LLUIColor map_avatar_friend_color = LLUIColorTable::instance().getColor("MapAvatarFriendColor", LLColor4::yellow);
		static LLUIColor map_avatar_linden_color = LLUIColorTable::instance().getColor("MapAvatarLindenColor", LLColor4::cyan);
		LLAvatarName av_name;
		LLAvatarNameCache::get(id, &av_name);
		bool is_linden = LLMuteList::instance().isLinden(av_name.getUserName());
		bool is_muted = LLMuteList::instance().isMuted(id, av_name.getUserName());
		bool show_as_friend = LLAvatarTracker::instance().isBuddy(id);

		if (is_linden)
		{
			return map_avatar_linden_color.get();
		}
		else if (is_muted)
		{
			return LLColor4::grey;
		}
		else if (show_as_friend)
		{
			return map_avatar_friend_color.get();
		}
		else
		{
			return map_avatar_color.get();
		}
	}
}
