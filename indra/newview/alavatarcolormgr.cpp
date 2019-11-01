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
#include "rlvactions.h"

void ALAvatarColorMgr::addOrUpdateCustomColor(const LLUUID& id, EAvatarColors color_val)
{
	auto it = mCustomColors.find(id);
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
	auto it = mCustomColors.find(id);
	if (it != mCustomColors.cend())
	{
		mCustomColors.erase(it);
	}
}

const LLColor4& ALAvatarColorMgr::getColor(const LLUUID& id)
{
	if (!mCustomColors.empty())
	{
		auto user_col_it = mCustomColors.find(id);
		if (user_col_it != mCustomColors.cend())
		{
			static LLUIColor av_custom_col1 = LLUIColorTable::instance().getColor("AvatarCustomColor1", LLColor4::red);
			static LLUIColor av_custom_col2 = LLUIColorTable::instance().getColor("AvatarCustomColor2", LLColor4::green);
			static LLUIColor av_custom_col3 = LLUIColorTable::instance().getColor("AvatarCustomColor3", LLColor4::blue);
			static LLUIColor av_custom_col4 = LLUIColorTable::instance().getColor("AvatarCustomColor4", LLColor4::yellow);

			const EAvatarColors color_val = user_col_it->second;
			switch (color_val)
			{
			case E_FIRST_COLOR:
				return av_custom_col1.get();
			case E_SECOND_COLOR:
				return av_custom_col2.get();
			case E_THIRD_COLOR:
				return av_custom_col3.get();
			case E_FOURTH_COLOR:
				return av_custom_col4.get();
			default:
				return LLColor4::white;
			}
		}
	}


	{
		static LLUIColor av_linden_color = LLUIColorTable::instance().getColor("AvatarLindenColor", LLColor4::cyan);
		static LLUIColor av_muted_color = LLUIColorTable::instance().getColor("AvatarMutedColor", LLColor4::grey4);
		static LLUIColor av_friend_color = LLUIColorTable::instance().getColor("AvatarFriendColor", LLColor4::yellow);
		static LLUIColor av_default_color = LLUIColorTable::instance().getColor("AvatarDefaultColor", LLColor4::green);

		LLAvatarName av_name;
		LLAvatarNameCache::get(id, &av_name);

		const bool rlv_show_name = RlvActions::canShowName(RlvActions::SNC_DEFAULT, id);
		if (LLMuteList::instance().isLinden(av_name.getUserName()) && rlv_show_name) // linden
		{
			return av_linden_color.get();
		}
		else if (LLMuteList::instance().isMuted(id, av_name.getUserName()) && rlv_show_name) // muted
		{
			return av_muted_color.get();
		}
		else if (LLAvatarTracker::instance().isBuddy(id)  && rlv_show_name) // friend
		{
			return av_friend_color.get();
		}
		else // everyone else
		{
			return av_default_color.get();
		}
	}
}
