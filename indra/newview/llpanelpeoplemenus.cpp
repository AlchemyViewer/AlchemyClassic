// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llpanelpeoplemenus.h
 * @brief Menus used by the side tray "People" panel
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

// libs
#include "llmenugl.h"

#include "llpanelpeoplemenus.h"

// newview
#include "alavatarcolormgr.h"
#include "llagent.h"
#include "llagentdata.h"			// for gAgentID
#include "llavataractions.h"
#include "llcallingcard.h"			// for LLAvatarTracker
#include "lllogchat.h"
#include "llviewermenu.h"			// for gMenuHolder
#include "llconversationmodel.h"
#include "llviewerobjectlist.h"

namespace LLPanelPeopleMenus
{

PeopleContextMenu gPeopleContextMenu;
NearbyPeopleContextMenu gNearbyPeopleContextMenu;
SuggestedFriendsContextMenu gSuggestedFriendsContextMenu;

//== PeopleContextMenu ===============================================================

LLContextMenu* PeopleContextMenu::createMenu()
{
	// set up the callbacks for all of the avatar menu items
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	LLContextMenu* menu;

	if ( mUUIDs.size() == 1 )
	{
		// Set up for one person selected menu

		const LLUUID& id = mUUIDs.front();
		registrar.add("Avatar.Profile",			boost::bind(&LLAvatarActions::showProfile,				id));
		registrar.add("Avatar.AddFriend",		boost::bind(&LLAvatarActions::requestFriendshipDialog,	id));
		registrar.add("Avatar.RemoveFriend",	boost::bind(&LLAvatarActions::removeFriendDialog, 		id));
		registrar.add("Avatar.IM",				boost::bind(&LLAvatarActions::startIM,					id));
		registrar.add("Avatar.Call",			boost::bind(&LLAvatarActions::startCall,				id));
		registrar.add("Avatar.OfferTeleport",	boost::bind(static_cast<void(*)(const LLUUID&)>
			(&LLAvatarActions::offerTeleport),			id));
		registrar.add("Avatar.ZoomIn",			boost::bind(&handle_zoom_to_object,						id));
		registrar.add("Avatar.ShowOnMap",		boost::bind(&LLAvatarActions::showOnMap,				id));
		registrar.add("Avatar.Share",			boost::bind(&LLAvatarActions::share,					id));
		registrar.add("Avatar.Pay",				boost::bind(&LLAvatarActions::pay,						id));
		registrar.add("Avatar.BlockUnblock",	boost::bind(&LLAvatarActions::toggleBlock,				id));
		registrar.add("Avatar.InviteToGroup",	boost::bind(&LLAvatarActions::inviteToGroup,			id));
		registrar.add("Avatar.TeleportRequest",	boost::bind(&LLAvatarActions::teleportRequest,		id));
		registrar.add("Avatar.Calllog",			boost::bind(&LLAvatarActions::viewChatHistory,			id));
		registrar.add("Avatar.Freeze",			boost::bind(static_cast<void(*)(const LLUUID&)>
			(&LLAvatarActions::parcelFreeze),					id));
		registrar.add("Avatar.Eject",			boost::bind(static_cast<void(*)(const LLUUID&)>
			(&LLAvatarActions::parcelEject),					id));
		registrar.add("Avatar.EstateTPHome",	boost::bind(static_cast<void(*)(const LLUUID&)>
			(&LLAvatarActions::estateTeleportHome), id));
		registrar.add("Avatar.EstateKick",		boost::bind(static_cast<void(*)(const LLUUID&)>
			(&LLAvatarActions::estateKick), id));
		registrar.add("Avatar.EstateBan", boost::bind(static_cast<void(*)(const LLUUID&)>
			(&LLAvatarActions::estateBan), id));
		registrar.add("Avatar.GodFreeze", boost::bind(&LLAvatarActions::godFreeze, id));
		registrar.add("Avatar.GodUnfreeze", boost::bind(&LLAvatarActions::godUnfreeze, id));
		registrar.add("Avatar.GodKick", boost::bind(&LLAvatarActions::godKick, id));
		registrar.add("Avatar.CopyName",		boost::bind(static_cast<void(*)(const LLUUID&, 
			LLAvatarActions::ECopyDataType)>(&LLAvatarActions::copyData), id, LLAvatarActions::E_DATA_NAME));
		registrar.add("Avatar.CopySLURL",		boost::bind(static_cast<void(*)(const LLUUID&,
			LLAvatarActions::ECopyDataType)>(&LLAvatarActions::copyData), id, LLAvatarActions::E_DATA_SLURL));
		registrar.add("Avatar.CopyKey",			boost::bind(static_cast<void(*)(const LLUUID&,
			LLAvatarActions::ECopyDataType)>(&LLAvatarActions::copyData), id, LLAvatarActions::E_DATA_UUID));
		registrar.add("Avatar.TeleportTo",		boost::bind(&LLAvatarActions::teleportTo, id));
		registrar.add("Avatar.Colorize",		boost::bind(&PeopleContextMenu::colorize, this, _2));

		enable_registrar.add("Avatar.EnableItem", boost::bind(&PeopleContextMenu::enableContextMenuItem, this, _2));
		enable_registrar.add("Avatar.CheckItem",  boost::bind(&PeopleContextMenu::checkContextMenuItem,	this, _2));
		enable_registrar.add("Avatar.EnableFreezeEject", boost::bind(static_cast<bool(*)(const LLUUID&)>
			(&LLAvatarActions::canFreezeEject), id));
		enable_registrar.add("Avatar.EnableEstateManage", boost::bind(static_cast<bool(*)(const LLUUID&)>
			(&LLAvatarActions::canManageAvatarsEstate), id));

		// create the context menu from the XUI
		menu = createFromFile("menu_people_nearby.xml");
		buildContextMenu(*menu, 0x0);
	}
	else
	{
		// Set up for multi-selected People

		// registrar.add("Avatar.AddFriend",	boost::bind(&LLAvatarActions::requestFriendshipDialog,	mUUIDs)); // *TODO: unimplemented
		registrar.add("Avatar.IM",				boost::bind(&PeopleContextMenu::startConference,		this));
		registrar.add("Avatar.Call",			boost::bind(&LLAvatarActions::startAdhocCall,			mUUIDs, LLUUID::null));
		registrar.add("Avatar.OfferTeleport",	boost::bind(static_cast<void(*)(const uuid_vec_t&)>
			(&LLAvatarActions::offerTeleport), mUUIDs));
		registrar.add("Avatar.RemoveFriend",	boost::bind(&LLAvatarActions::removeFriendsDialog,		mUUIDs));
		// registrar.add("Avatar.Share",		boost::bind(&LLAvatarActions::startIM,					mUUIDs)); // *TODO: unimplemented
		// registrar.add("Avatar.Pay",			boost::bind(&LLAvatarActions::pay,						mUUIDs)); // *TODO: unimplemented
		registrar.add("Avatar.Freeze", boost::bind(static_cast<void(*)(const uuid_vec_t&)>
			(&LLAvatarActions::parcelFreeze), mUUIDs));
		registrar.add("Avatar.Eject", boost::bind(static_cast<void(*)(const uuid_vec_t&)>
			(&LLAvatarActions::parcelEject), mUUIDs));
		registrar.add("Avatar.EstateTPHome", boost::bind(static_cast<void(*)(const uuid_vec_t&)>
			(&LLAvatarActions::estateTeleportHome), mUUIDs));
		registrar.add("Avatar.EstateKick", boost::bind(static_cast<void(*)(const uuid_vec_t&)>
			(&LLAvatarActions::estateKick), mUUIDs));
		registrar.add("Avatar.EstateBan", boost::bind(static_cast<void(*)(const uuid_vec_t&)>
			(&LLAvatarActions::estateBan), mUUIDs));
		registrar.add("Avatar.CopyName",		boost::bind(static_cast<void(*)(const uuid_vec_t&, LLAvatarActions::ECopyDataType)>
			(&LLAvatarActions::copyData), mUUIDs, LLAvatarActions::E_DATA_NAME));
		registrar.add("Avatar.CopySLURL",		boost::bind(static_cast<void(*)(const uuid_vec_t&, LLAvatarActions::ECopyDataType)>
			(&LLAvatarActions::copyData), mUUIDs, LLAvatarActions::E_DATA_SLURL));
		registrar.add("Avatar.CopyKey",			boost::bind(static_cast<void(*)(const uuid_vec_t&, LLAvatarActions::ECopyDataType)>
			(&LLAvatarActions::copyData), mUUIDs, LLAvatarActions::E_DATA_UUID));
		registrar.add("Avatar.Colorize", boost::bind(&PeopleContextMenu::colorize, this, _2));
		
		enable_registrar.add("Avatar.EnableItem",	boost::bind(&PeopleContextMenu::enableContextMenuItem, this, _2));
		enable_registrar.add("Avatar.EnableFreezeEject", boost::bind(static_cast<bool(*)(const uuid_vec_t&)>
			(&LLAvatarActions::canFreezeEject), mUUIDs));
		enable_registrar.add("Avatar.EnableEstateManage", boost::bind(static_cast<bool(*)(const uuid_vec_t&)>
			(&LLAvatarActions::canManageAvatarsEstate), mUUIDs));

		// create the context menu from the XUI
		menu = createFromFile("menu_people_nearby_multiselect.xml");
		buildContextMenu(*menu, ITEM_IN_MULTI_SELECTION);
	}

    return menu;
}

void PeopleContextMenu::buildContextMenu(class LLMenuGL& menu, U32 flags)
{
    menuentry_vec_t items;
    menuentry_vec_t disabled_items;
	
	if (flags & ITEM_IN_MULTI_SELECTION)
	{
		items.push_back(std::string("add_friends"));
		items.push_back(std::string("remove_friends"));
		items.push_back(std::string("im"));
		items.push_back(std::string("call"));
		items.push_back(std::string("share"));
		items.push_back(std::string("pay"));
		items.push_back(std::string("offer_teleport"));
		items.push_back(std::string("separator_utilities"));
		items.push_back(std::string("tools_menu"));
		items.push_back(std::string("copy_name"));
		items.push_back(std::string("copy_slurl"));
		items.push_back(std::string("copy_uuid"));
	}
	else 
	{
		items.push_back(std::string("view_profile"));
		items.push_back(std::string("im"));
		items.push_back(std::string("offer_teleport"));
		items.push_back(std::string("request_teleport"));
		items.push_back(std::string("voice_call"));
		items.push_back(std::string("chat_history"));
		items.push_back(std::string("separator_chat_history"));
		items.push_back(std::string("add_friend"));
		items.push_back(std::string("remove_friend"));
		items.push_back(std::string("invite_to_group"));
		items.push_back(std::string("separator_invite_to_group"));
		items.push_back(std::string("map"));
		items.push_back(std::string("share"));
		items.push_back(std::string("pay"));
		items.push_back(std::string("block_unblock"));
		items.push_back(std::string("separator_utilities"));
		items.push_back(std::string("tools_menu"));
		items.push_back(std::string("copy_name"));
		items.push_back(std::string("copy_slurl"));
		items.push_back(std::string("copy_uuid"));
	}

    hide_context_entries(menu, items, disabled_items);
}

bool PeopleContextMenu::enableContextMenuItem(const LLSD& userdata)
{
	if(gAgent.getID() == mUUIDs.front())
	{
		return false;
	}
	std::string item = userdata.asString();

	// Note: can_block and can_delete is used only for one person selected menu
	// so we don't need to go over all uuids.

	if (item == std::string("can_block"))
	{
		const LLUUID& id = mUUIDs.front();
		return LLAvatarActions::canBlock(id);
	}
	else if (item == std::string("can_add"))
	{
		// We can add friends if:
		// - there are selected people
		// - and there are no friends among selection yet.

		//EXT-7389 - disable for more than 1
		if(mUUIDs.size() > 1)
		{
			return false;
		}

		bool result = (mUUIDs.size() > 0);

		uuid_vec_t::const_iterator
			id = mUUIDs.begin(),
			uuids_end = mUUIDs.end();

		for (;id != uuids_end; ++id)
		{
			if ( LLAvatarActions::isFriend(*id) )
			{
				result = false;
				break;
			}
		}

		return result;
	}
	else if (item == std::string("can_delete"))
	{
		// We can remove friends if:
		// - there are selected people
		// - and there are only friends among selection.

		bool result = (mUUIDs.size() > 0);

		uuid_vec_t::const_iterator
			id = mUUIDs.begin(),
			uuids_end = mUUIDs.end();

		for (;id != uuids_end; ++id)
		{
			if ( !LLAvatarActions::isFriend(*id) )
			{
				result = false;
				break;
			}
		}

		return result;
	}
	else if (item == std::string("can_call"))
	{
		return LLAvatarActions::canCall();
	}
	else if (item == std::string("can_zoom_in"))
	{
		const LLUUID& id = mUUIDs.front();
		return gObjectList.findObject(id);
	}
	else if (item == std::string("can_teleport_to"))
	{
		const LLUUID& id = mUUIDs.front();
		return LLAvatarActions::canTeleportTo(id);
	}
	else if (item == std::string("can_show_on_map"))
	{
		const LLUUID& id = mUUIDs.front();

		return (LLAvatarTracker::instance().isBuddyOnline(id) && LLAvatarActions::isAgentMappable(id))
					|| gAgent.isGodlike();
	}
	else if(item == std::string("can_offer_teleport"))
	{
		return LLAvatarActions::canOfferTeleport(mUUIDs);
	}
	else if (item == std::string("can_callog"))
	{
		return LLLogChat::isTranscriptExist(mUUIDs.front());
	}
	else if (item == std::string("can_im") || item == std::string("can_invite") ||
	         item == std::string("can_share") || item == std::string("can_pay"))
	{
		return true;
	}
	return false;
}

bool PeopleContextMenu::checkContextMenuItem(const LLSD& userdata)
{
	std::string item = userdata.asString();
	const LLUUID& id = mUUIDs.front();

	if (item == std::string("is_blocked"))
	{
		return LLAvatarActions::isBlocked(id);
	}

	return false;
}

void PeopleContextMenu::startConference()
{
	uuid_vec_t uuids;
	for (uuid_vec_t::const_iterator it = mUUIDs.begin(); it != mUUIDs.end(); ++it)
	{
		if(*it != gAgentID)
		{
			uuids.push_back(*it);
		}
	}
	LLAvatarActions::startConference(uuids);
}

void PeopleContextMenu::colorize(const LLSD& userdata)
{
	const std::string& param = userdata.asString();
	U32 color = 99;
	if (param == "color1")
	{
		color = ALAvatarColorMgr::E_FIRST_COLOR;
	}
	else if (param == "color2")
	{
		color = ALAvatarColorMgr::E_SECOND_COLOR;
	}
	else if (param == "color3")
	{
		color = ALAvatarColorMgr::E_THIRD_COLOR;
	}
	else if (param == "color4")
	{
		color = ALAvatarColorMgr::E_FOURTH_COLOR;
	}

	for (const LLUUID& id : mUUIDs)
	{
		if (id != gAgentID)
		{
			if (color != 99)
			{
				ALAvatarColorMgr::instance().addOrUpdateCustomColor(id, (ALAvatarColorMgr::e_custom_colors)color);
			}
			else
			{
				ALAvatarColorMgr::instance().clearCustomColor(id);
			}
		}
	}
}

//== NearbyPeopleContextMenu ===============================================================

void NearbyPeopleContextMenu::buildContextMenu(class LLMenuGL& menu, U32 flags)
{
    menuentry_vec_t items;
    menuentry_vec_t disabled_items;
	bool parcel_manage = LLAvatarActions::canFreezeEject(mUUIDs);
	bool estate_manage = LLAvatarActions::canManageAvatarsEstate(mUUIDs);
	if (flags & ITEM_IN_MULTI_SELECTION)
	{
		items.push_back(std::string("add_friends"));
		items.push_back(std::string("remove_friends"));
		items.push_back(std::string("im"));
		items.push_back(std::string("call"));
		items.push_back(std::string("share"));
		items.push_back(std::string("pay"));
		items.push_back(std::string("offer_teleport"));
		items.push_back(std::string("separator_utilities"));
		if (parcel_manage || estate_manage)
		{
			items.push_back("manage_menu");
			items.push_back(std::string("freeze"));
			items.push_back(std::string("eject"));
			if (estate_manage)
			{
				items.push_back(std::string("estate_tphome"));
				items.push_back(std::string("estate_kick"));
				items.push_back(std::string("estate_ban"));
			}
		}
		items.push_back(std::string("tools_menu"));
		items.push_back(std::string("copy_name"));
		items.push_back(std::string("copy_slurl"));
		items.push_back(std::string("copy_uuid"));
		items.push_back(std::string("tag_color"));
	}
	else 
	{
		items.push_back(std::string("view_profile"));
		items.push_back(std::string("im"));
		items.push_back(std::string("offer_teleport"));
		items.push_back(std::string("request_teleport"));
		items.push_back(std::string("voice_call"));
		items.push_back(std::string("chat_history"));
		items.push_back(std::string("separator_chat_history"));
		items.push_back(std::string("add_friend"));
		items.push_back(std::string("remove_friend"));
		items.push_back(std::string("invite_to_group"));
		items.push_back(std::string("separator_invite_to_group"));
		items.push_back(std::string("teleport_to"));
		items.push_back(std::string("zoom_in"));
		items.push_back(std::string("map"));
		items.push_back(std::string("share"));
		items.push_back(std::string("pay"));
		items.push_back(std::string("block_unblock"));
		items.push_back(std::string("separator_utilities"));
		if (parcel_manage || estate_manage)
		{
			items.push_back("manage_menu");
			items.push_back(std::string("freeze"));
			items.push_back(std::string("eject"));
			if (estate_manage)
			{
				items.push_back(std::string("estate_tphome"));
				items.push_back(std::string("estate_kick"));
				items.push_back(std::string("estate_ban"));
			}
		}
		items.push_back(std::string("tools_menu"));
		items.push_back(std::string("copy_name"));
		items.push_back(std::string("copy_slurl"));
		items.push_back(std::string("copy_uuid"));
		items.push_back(std::string("tag_color"));
	}

    hide_context_entries(menu, items, disabled_items);
}

//== SuggestedFriendsContextMenu ===============================================================

LLContextMenu* SuggestedFriendsContextMenu::createMenu()
{
	// set up the callbacks for all of the avatar menu items
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	LLContextMenu* menu;

	// Set up for one person selected menu
	const LLUUID& id = mUUIDs.front();
	registrar.add("Avatar.Profile",			boost::bind(&LLAvatarActions::showProfile,				id));
	registrar.add("Avatar.AddFriend",		boost::bind(&LLAvatarActions::requestFriendshipDialog,	id));

	// create the context menu from the XUI
	menu = createFromFile("menu_people_nearby.xml");
	buildContextMenu(*menu, 0x0);

	return menu;
}

void SuggestedFriendsContextMenu::buildContextMenu(class LLMenuGL& menu, U32 flags)
{ 
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;

	items.push_back(std::string("view_profile"));
	items.push_back(std::string("add_friend"));

	hide_context_entries(menu, items, disabled_items);
}

} // namespace LLPanelPeopleMenus
