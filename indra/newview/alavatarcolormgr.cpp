// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
		mCustomColors.emplace(id, color_val);
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
		const EAvatarColors color_val = user_col_it->second;
		switch (color_val)
		{
		case E_FIRST_COLOR:
			return LLUIColorTable::instance().getColor("AvatarCustomColor1", LLColor4::red).get();
			break;
		case E_SECOND_COLOR:
			return LLUIColorTable::instance().getColor("AvatarCustomColor2", LLColor4::green).get();
			break;
		case E_THIRD_COLOR:
			return LLUIColorTable::instance().getColor("AvatarCustomColor3", LLColor4::blue).get();
			break;
		case E_FOURTH_COLOR:
			return LLUIColorTable::instance().getColor("AvatarCustomColor4", LLColor4::yellow).get();
			break;
		default:
			return LLColor4::white;
		}
	}
	else
	{
		LLAvatarName av_name;
		LLAvatarNameCache::get(id, &av_name);
		if (LLMuteList::instance().isLinden(av_name.getUserName())) // linden
		{
			return LLUIColorTable::instance().getColor("AvatarLindenColor", LLColor4::cyan).get();
		}
		else if (LLMuteList::instance().isMuted(id, av_name.getUserName())) // muted
		{
			return LLUIColorTable::instance().getColor("AvatarMutedColor", LLColor4::grey4).get();
		}
		else if (LLAvatarTracker::instance().isBuddy(id)) // friend
		{
			return LLUIColorTable::instance().getColor("AvatarFriendColor", LLColor4::yellow).get();
		}
		else // everyone else
		{
			return LLUIColorTable::instance().getColor("AvatarDefaultColor", LLColor4::green).get();
		}
	}
}
