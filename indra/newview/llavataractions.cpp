// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llavataractions.cpp
 * @brief Friend-related actions (add, remove, offer teleport, etc)
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

#include "llavataractions.h"

#include "llavatarnamecache.h"	// IDEVO
#include "llsd.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "roles_constants.h"    // for GP_MEMBER_INVITE

#include "llagent.h"
#include "llappviewer.h"		// for gLastVersionChannel
#include "llcallingcard.h"		// for LLAvatarTracker
#include "llconversationlog.h"
#include "llfloateravatarpicker.h"	// for LLFloaterAvatarPicker
#include "llfloaterconversationpreview.h"
#include "llfloatergroupinvite.h"
#include "llfloatergroups.h"
#include "llfloaterreg.h"
#include "llfloaterregioninfo.h"
#include "llfloaterpay.h"
#include "llfloatersidepanelcontainer.h"
#include "llfloaterwebcontent.h"
#include "llfloaterworldmap.h"
#include "llfolderview.h"
#include "llgiveinventory.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"	// for gInventory.findCategoryUUIDForType
#include "llinventorypanel.h"
#include "llfloaterimcontainer.h"
#include "llimview.h"			// for gIMMgr
#include "llmutelist.h"
#include "llpaneloutfitedit.h"
#include "llpanelprofile.h"
#include "llrecentpeople.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewermenu.h"		// for handle_zoom_to_object
#include "llviewermessage.h"	// for handle_lure
#include "llviewerregion.h"
#include "llslurl.h"			// IDEVO
#include "llsidepanelinventory.h"
#include "llavatarname.h"
#include "llagentui.h"
#include "llweb.h"

// <alchemy> - Includes
#include "llclipboard.h"
#include "llworld.h"
#include "llfloaterlegacyprofile.h"
// </alchemy>

// Flags for kick message
const U32 KICK_FLAGS_DEFAULT	= 0x0;
const U32 KICK_FLAGS_FREEZE		= 1 << 0;
const U32 KICK_FLAGS_UNFREEZE	= 1 << 1;


// static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id, const std::string& name)
{
	if(id == gAgentID)
	{
		LLNotificationsUtil::add("AddSelfFriend");
		return;
	}

	LLSD args;
	args["NAME"] = LLSLURL("agent", id, "completename").getSLURLString();
	LLSD payload;
	payload["id"] = id;
	payload["name"] = name;
    
    	LLNotificationsUtil::add("AddFriendWithMessage", args, payload, &callbackAddFriendWithMessage);

	// add friend to recent people list
	LLRecentPeople::instance().add(id);
}

static void on_avatar_name_friendship(const LLUUID& id, const LLAvatarName& av_name)
{
	LLAvatarActions::requestFriendshipDialog(id, av_name.getCompleteName());
}

// static
void LLAvatarActions::requestFriendshipDialog(const LLUUID& id)
{
	if(id.isNull())
	{
		return;
	}

	LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_friendship, _1, _2));
}

// static
void LLAvatarActions::removeFriendDialog(const LLUUID& id)
{
	if (id.isNull())
		return;

	uuid_vec_t ids;
	ids.push_back(id);
	removeFriendsDialog(ids);
}

// static
void LLAvatarActions::removeFriendsDialog(const uuid_vec_t& ids)
{
	if(ids.size() == 0)
		return;

	LLSD args;
	std::string msgType;
	if(ids.size() == 1)
	{
		LLUUID agent_id = ids[0];
		LLAvatarName av_name;
		if(LLAvatarNameCache::get(agent_id, &av_name))
		{
			args["NAME"] = av_name.getCompleteName();
		}

		msgType = "RemoveFromFriends";
	}
	else
	{
		msgType = "RemoveMultipleFromFriends";
	}

	LLSD payload;
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		payload["ids"].append(*it);
	}

	LLNotificationsUtil::add(msgType,
		args,
		payload,
		&handleRemove);
}

// static
void LLAvatarActions::offerTeleport(const LLUUID& invitee)
{
	if (invitee.isNull())
		return;

	std::vector<LLUUID> ids;
	ids.push_back(invitee);
	offerTeleport(ids);
}

// static
void LLAvatarActions::offerTeleport(const uuid_vec_t& ids) 
{
	if (ids.size() == 0)
		return;

	handle_lure(ids);
}

static void on_avatar_name_cache_start_im(const LLUUID& agent_id,
										  const LLAvatarName& av_name)
{
	std::string name = av_name.getDisplayName();
	LLUUID session_id = gIMMgr->addSession(name, IM_NOTHING_SPECIAL, agent_id);
	if (session_id != LLUUID::null)
	{
		LLFloaterIMContainer::getInstance()->showConversation(session_id);
	}
	make_ui_sound("UISndStartIM");
}

// static
void LLAvatarActions::startIM(const LLUUID& id)
{
	if (id.isNull() || gAgent.getID() == id)
		return;

	LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_cache_start_im, _1, _2));
}

// static
void LLAvatarActions::endIM(const LLUUID& id)
{
	if (id.isNull())
		return;
	
	LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, id);
	if (session_id != LLUUID::null)
	{
		gIMMgr->leaveSession(session_id);
	}
}

static void on_avatar_name_cache_start_call(const LLUUID& agent_id,
											const LLAvatarName& av_name)
{
	std::string name = av_name.getDisplayName();
	LLUUID session_id = gIMMgr->addSession(name, IM_NOTHING_SPECIAL, agent_id, true);
	if (session_id != LLUUID::null)
	{
		gIMMgr->startCall(session_id);
	}
	make_ui_sound("UISndStartIM");
}

// static
void LLAvatarActions::startCall(const LLUUID& id)
{
	if (id.isNull())
	{
		return;
	}
	LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_cache_start_call, _1, _2));
}

// static
void LLAvatarActions::startAdhocCall(const uuid_vec_t& ids, const LLUUID& floater_id)
{
	if (ids.size() == 0)
	{
		return;
	}

	// convert vector into std::vector for addSession
	std::vector<LLUUID> id_array;
	id_array.reserve(ids.size());
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		id_array.push_back(*it);
	}

	// create the new ad hoc voice session
	const std::string title = LLTrans::getString("conference-title");
	LLUUID session_id = gIMMgr->addSession(title, IM_SESSION_CONFERENCE_START,
										   ids[0], id_array, true, floater_id);
	if (session_id == LLUUID::null)
	{
		return;
	}

	gIMMgr->autoStartCallOnStartup(session_id);

	make_ui_sound("UISndStartIM");
}

/* AD *TODO: Is this function needed any more?
	I fixed it a bit(added check for canCall), but it appears that it is not used
	anywhere. Maybe it should be removed?
// static
bool LLAvatarActions::isCalling(const LLUUID &id)
{
	if (id.isNull() || !canCall())
	{
		return false;
	}

	LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, id);
	return (LLIMModel::getInstance()->findIMSession(session_id) != NULL);
}*/

//static
bool LLAvatarActions::canCall()
{
	return LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking();
}

// static
void LLAvatarActions::startConference(const uuid_vec_t& ids, const LLUUID& floater_id)
{
	// *HACK: Copy into dynamic array
	std::vector<LLUUID> id_array;

	id_array.reserve(ids.size());
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		id_array.push_back(*it);
	}
	const std::string title = LLTrans::getString("conference-title");
	LLUUID session_id = gIMMgr->addSession(title, IM_SESSION_CONFERENCE_START, ids[0], id_array, false, floater_id);

	if (session_id == LLUUID::null)
	{
		return;
	}
	
	LLFloaterIMContainer::getInstance()->showConversation(session_id);
	
	make_ui_sound("UISndStartIM");
}

static const char* get_profile_floater_name(const LLUUID& avatar_id)
{
	// Use different floater XML for our profile to be able to save its rect.
	return avatar_id == gAgentID ? "my_profile" : "profile";
}

static void on_avatar_name_show_profile(const LLUUID& agent_id, const LLAvatarName& av_name)
{
	std::string url = getProfileURL(av_name.getAccountName());

	// PROFILES: open in webkit window
	LLFloaterWebContent::Params p;
	p.url(url).id(agent_id.asString());
	LLFloaterReg::showInstance(get_profile_floater_name(agent_id), p);
}

// static
void LLAvatarActions::showProfile(const LLUUID& id)
{
	if (id.notNull())
	{
		if (gSavedSettings.getBool("AlchemyUseWannabeFacebook"))
		{
			LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_show_profile, _1, _2));
		}
		else
		{
			LLFloaterReg::showTypedInstance<LLFloaterLegacyProfile>("legacy_profile", LLSD().with("avatar_id", id), TAKE_FOCUS_YES);
		}
	}
}

// static
void LLAvatarActions::showWebProfile(const LLUUID& id)
{
	if (id.notNull())
		LLAvatarNameCache::get(id, boost::bind(&on_avatar_name_show_profile, _1, _2));
}

//static 
bool LLAvatarActions::profileVisible(const LLUUID& id)
{
	LLFloater* floaterp = getProfileFloater(id);
	return floaterp && floaterp->isShown();
}

//static
LLFloater* LLAvatarActions::getProfileFloater(const LLUUID& id)
{
	if (gSavedSettings.getBool("AlchemyUseWannabeFacebook"))
	{
		return LLFloaterReg::findTypedInstance<LLFloaterWebContent>(get_profile_floater_name(id), LLSD().with("id", id));
	}
	else
	{
		return LLFloaterReg::findTypedInstance<LLFloaterLegacyProfile>("legacy_profile", LLSD().with("avatar_id", id));
	}
}

//static 
void LLAvatarActions::hideProfile(const LLUUID& id)
{
	LLFloater* floaterp = getProfileFloater(id);
	if (floaterp)
	{
		floaterp->closeFloater();
	}
}

// static
void LLAvatarActions::showOnMap(const LLUUID& id)
{
	LLAvatarName av_name;
	if (!LLAvatarNameCache::get(id, &av_name))
	{
		LLAvatarNameCache::get(id, boost::bind(&LLAvatarActions::showOnMap, id));
		return;
	}

	LLFloaterWorldMap::getInstance()->trackAvatar(id, av_name.getDisplayName());
	LLFloaterReg::showInstance("world_map", "center");
}

// static
void LLAvatarActions::pay(const LLUUID& id)
{
	LLNotification::Params params("DoNotDisturbModePay");
	params.functor.function(boost::bind(&LLAvatarActions::handlePay, _1, _2, id));

	if (gAgent.isDoNotDisturb())
	{
		// warn users of being in do not disturb mode during a transaction
		LLNotifications::instance().add(params);
	}
	else
	{
		LLNotifications::instance().forceResponse(params, 1);
	}
}

void LLAvatarActions::teleport_request_callback(const LLSD& notification, const LLSD& response)
{
	S32 option;
	if (response.isInteger()) 
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotificationsUtil::getSelectedOption(notification, response);
	}

	if (0 == option)
	{
		LLMessageSystem* msg = gMessageSystem;

		msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

		msg->nextBlockFast(_PREHASH_MessageBlock);
		msg->addBOOLFast(_PREHASH_FromGroup, FALSE);
		msg->addUUIDFast(_PREHASH_ToAgentID, notification["substitutions"]["uuid"] );
		msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
		msg->addU8Fast(_PREHASH_Dialog, IM_TELEPORT_REQUEST);
		msg->addUUIDFast(_PREHASH_ID, LLUUID::null);
		msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP); // no timestamp necessary

		std::string name;
		LLAgentUI::buildFullname(name);

		msg->addStringFast(_PREHASH_FromAgentName, name);
		msg->addStringFast(_PREHASH_Message, response["message"]);
		msg->addU32Fast(_PREHASH_ParentEstateID, 0);
		msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
		msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());

		gMessageSystem->addBinaryDataFast(
				_PREHASH_BinaryBucket,
				EMPTY_BINARY_BUCKET,
				EMPTY_BINARY_BUCKET_SIZE);

		gAgent.sendReliableMessage();
	}
}

// static
void LLAvatarActions::teleportRequest(const LLUUID& id)
{
	LLSD notification;
	notification["uuid"] = id;
	LLAvatarName av_name;
	if (!LLAvatarNameCache::get(id, &av_name))
	{
		// unlikely ... they just picked this name from somewhere...
		LLAvatarNameCache::get(id, boost::bind(&LLAvatarActions::teleportRequest, id));
		return; // reinvoke this when the name resolves
	}
	notification["NAME"] = av_name.getCompleteName();

	LLSD payload;

	LLNotificationsUtil::add("TeleportRequestPrompt", notification, payload, teleport_request_callback);
}

// static
void LLAvatarActions::godKick(const LLUUID& id)
{
	LLSD args;
	args["AVATAR_NAME"] = LLSLURL("agent", id, "about").getSLURLString();

	LLSD payload;
	payload["type"] = static_cast<LLSD::Integer>(KICK_FLAGS_DEFAULT);
	payload["avatar_id"] = id;
	LLNotifications::instance().add("KickUser", args, payload, handleGodKick);
}

// static
void LLAvatarActions::godFreeze(const LLUUID& id)
{
	LLSD args;
	args["AVATAR_NAME"] = LLSLURL("agent", id, "about").getSLURLString();

	LLSD payload;
	payload["type"] = static_cast<LLSD::Integer>(KICK_FLAGS_FREEZE);
	payload["avatar_id"] = id;
	LLNotifications::instance().add("FreezeUser", args, payload, handleGodKick);
}
// static
void LLAvatarActions::godUnfreeze(const LLUUID& id)
{
	LLSD args;
	args["AVATAR_NAME"] = LLSLURL("agent", id, "about").getSLURLString();

	LLSD payload;
	payload["type"] = static_cast<LLSD::Integer>(KICK_FLAGS_UNFREEZE);
	payload["avatar_id"] = id;
	LLNotifications::instance().add("UnFreezeUser", args, payload, handleGodKick);
}

//static 
void LLAvatarActions::csr(const LLUUID& id, std::string name)
{
	if (name.empty()) return;
	
	std::string url = "http://csr.lindenlab.com/agent/";
	
	// slow and stupid, but it's late
	S32 len = name.length();
	for (S32 i = 0; i < len; i++)
	{
		if (name[i] == ' ')
		{
			url += "%20";
		}
		else
		{
			url += name[i];
		}
	}
	
	LLWeb::loadURL(url);
}

//static
void LLAvatarActions::zoomIn(const LLUUID& id)
{
	handle_zoom_to_object(id);
}

//static 
void LLAvatarActions::share(const LLUUID& id)
{
	LLSD key;
	LLFloaterSidePanelContainer::showPanel("inventory", key);
	LLFloaterReg::showInstance("im_container");

	LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL,id);

	if (!gIMMgr->hasSession(session_id))
	{
		startIM(id);
	}

	if (gIMMgr->hasSession(session_id))
	{
		// we should always get here, but check to verify anyways
		LLIMModel::getInstance()->addMessage(session_id, SYSTEM_FROM, LLUUID::null, LLTrans::getString("share_alert"), false);

		LLFloaterIMSessionTab* session_floater = LLFloaterIMSessionTab::findConversation(session_id);
		if (session_floater && session_floater->isMinimized())
		{
			session_floater->setMinimized(false);
		}
		LLFloaterIMContainer *im_container = LLFloaterReg::getTypedInstance<LLFloaterIMContainer>("im_container");
		im_container->selectConversationPair(session_id, true);
	}
}

namespace action_give_inventory
{
	/**
	 * Returns a pointer to 'Add More' inventory panel of Edit Outfit SP.
	 */
	static LLInventoryPanel* get_outfit_editor_inventory_panel()
	{
		LLPanelOutfitEdit* panel_outfit_edit = dynamic_cast<LLPanelOutfitEdit*>(LLFloaterSidePanelContainer::getPanel("appearance", "panel_outfit_edit"));
		if (nullptr == panel_outfit_edit) return nullptr;

		LLInventoryPanel* inventory_panel = panel_outfit_edit->findChild<LLInventoryPanel>("folder_view");
		return inventory_panel;
	}

	/**
	 * @return active inventory panel, or NULL if there's no such panel
	 */
	static LLInventoryPanel* get_active_inventory_panel()
	{
		LLInventoryPanel* active_panel = LLInventoryPanel::getActiveInventoryPanel(FALSE);
		LLFloater* floater_appearance = LLFloaterReg::findInstance("appearance");
		if (!active_panel || (floater_appearance && floater_appearance->hasFocus()))
		{
			active_panel = get_outfit_editor_inventory_panel();
		}

		return active_panel;
	}

	/**
	 * Checks My Inventory visibility.
	 */

	static bool is_give_inventory_acceptable()
	{
		// check selection in the panel
		const std::set<LLUUID> inventory_selected_uuids = LLAvatarActions::getInventorySelectedUUIDs();
		if (inventory_selected_uuids.empty()) return false; // nothing selected

		bool acceptable = false;
		std::set<LLUUID>::const_iterator it = inventory_selected_uuids.begin();
		const std::set<LLUUID>::const_iterator it_end = inventory_selected_uuids.end();
		for (; it != it_end; ++it)
		{
			LLViewerInventoryCategory* inv_cat = gInventory.getCategory(*it);
			// any category can be offered.
			if (inv_cat)
			{
				acceptable = true;
				continue;
			}

			LLViewerInventoryItem* inv_item = gInventory.getItem(*it);
			// check if inventory item can be given
			if (LLGiveInventory::isInventoryGiveAcceptable(inv_item))
			{
				acceptable = true;
				continue;
			}

			// there are neither item nor category in inventory
			acceptable = false;
			break;
		}
		return acceptable;
	}

	static void build_items_string(const std::set<LLUUID>& inventory_selected_uuids , std::string& items_string)
	{
		llassert(inventory_selected_uuids.size() > 0);

		const std::string& separator = LLTrans::getString("words_separator");
		for (std::set<LLUUID>::const_iterator it = inventory_selected_uuids.begin(); ; )
		{
			LLViewerInventoryCategory* inv_cat = gInventory.getCategory(*it);
			if (nullptr != inv_cat)
			{
				items_string = inv_cat->getName();
				break;
			}
			LLViewerInventoryItem* inv_item = gInventory.getItem(*it);
			if (nullptr != inv_item)
			{
				items_string.append(inv_item->getName());
			}
			if(++it == inventory_selected_uuids.end())
			{
				break;
			}
			items_string.append(separator);
		}
	}

	struct LLShareInfo : public LLSingleton<LLShareInfo>
	{
		LLSINGLETON_EMPTY_CTOR(LLShareInfo);
	public:
		std::vector<LLAvatarName> mAvatarNames;
		uuid_vec_t mAvatarUuids;
	};

	static void give_inventory_cb(const LLSD& notification, const LLSD& response)
	{
		S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
		// if Cancel pressed
		if (option == 1)
		{
			return;
		}

		const std::set<LLUUID> inventory_selected_uuids = LLAvatarActions::getInventorySelectedUUIDs();
		if (inventory_selected_uuids.empty())
		{
			return;
		}

		S32 count = LLShareInfo::instance().mAvatarNames.size();
		bool shared = count && !inventory_selected_uuids.empty();

		// iterate through avatars
		for(S32 i = 0; i < count; ++i)
		{
			const LLUUID& avatar_uuid = LLShareInfo::instance().mAvatarUuids[i];

			// We souldn't open IM session, just calculate session ID for logging purpose. See EXT-6710
			const LLUUID session_id = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, avatar_uuid);

			std::set<LLUUID>::const_iterator it = inventory_selected_uuids.begin();
			const std::set<LLUUID>::const_iterator it_end = inventory_selected_uuids.end();

			const std::string& separator = LLTrans::getString("words_separator");
			std::string noncopy_item_names;
			LLSD noncopy_items = LLSD::emptyArray();
			// iterate through selected inventory objects
			for (; it != it_end; ++it)
			{
				LLViewerInventoryCategory* inv_cat = gInventory.getCategory(*it);
				if (inv_cat)
				{
					if (!LLGiveInventory::doGiveInventoryCategory(avatar_uuid, inv_cat, session_id, "ItemsShared"))
					{
						shared = false;
					}
					break;
				}
				LLViewerInventoryItem* inv_item = gInventory.getItem(*it);
				if (!inv_item->getPermissions().allowCopyBy(gAgentID))
				{
					if (!noncopy_item_names.empty())
					{
						noncopy_item_names.append(separator);
					}
					noncopy_item_names.append(inv_item->getName());
					noncopy_items.append(*it);
				}
				else
				{
					if (!LLGiveInventory::doGiveInventoryItem(avatar_uuid, inv_item, session_id))
					{
						shared = false;
					}
				}
			}
			if (noncopy_items.beginArray() != noncopy_items.endArray())
			{
				LLSD substitutions;
				substitutions["ITEMS"] = noncopy_item_names;
				LLSD payload;
				payload["agent_id"] = avatar_uuid;
				payload["items"] = noncopy_items;
				payload["success_notification"] = "ItemsShared";
				LLNotificationsUtil::add("CannotCopyWarning", substitutions, payload,
					&LLGiveInventory::handleCopyProtectedItem);
				shared = false;
				break;
			}
		}
		if (shared)
		{
			LLFloaterReg::hideInstance("avatar_picker");
			LLNotificationsUtil::add("ItemsShared");
		}
	}

	/**
	 * Performs "give inventory" operations for provided avatars.
	 *
	 * Sends one requests to give all selected inventory items for each passed avatar.
	 * Avatars are represent by two vectors: names and UUIDs which must be sychronized with each other.
	 *
	 * @param avatar_names - avatar names request to be sent.
	 * @param avatar_uuids - avatar names request to be sent.
	 */
	static void give_inventory(const uuid_vec_t& avatar_uuids, const std::vector<LLAvatarName>& avatar_names)
	{
		llassert(avatar_names.size() == avatar_uuids.size());

		const std::set<LLUUID> inventory_selected_uuids = LLAvatarActions::getInventorySelectedUUIDs();
		if (inventory_selected_uuids.empty())
		{
			return;
		}

		std::string residents;
		LLAvatarActions::buildResidentsString(avatar_names, residents, true);

		std::string items;
		build_items_string(inventory_selected_uuids, items);

		int folders_count = 0;
		std::set<LLUUID>::const_iterator it = inventory_selected_uuids.begin();

		//traverse through selected inventory items and count folders among them
		for ( ; it != inventory_selected_uuids.end() && folders_count <=1 ; ++it)
		{
			LLViewerInventoryCategory* inv_cat = gInventory.getCategory(*it);
			if (nullptr != inv_cat)
			{
				folders_count++;
			}
		}

		// EXP-1599
		// In case of sharing multiple folders, make the confirmation
		// dialog contain a warning that only one folder can be shared at a time.
		std::string notification = (folders_count > 1) ? "ShareFolderConfirmation" : "ShareItemsConfirmation";
		LLSD substitutions;
		substitutions["RESIDENTS"] = residents;
		substitutions["ITEMS"] = items;
		LLShareInfo::instance().mAvatarNames = avatar_names;
		LLShareInfo::instance().mAvatarUuids = avatar_uuids;
		LLNotificationsUtil::add(notification, substitutions, LLSD(), &give_inventory_cb);
	}
}

// static
void LLAvatarActions::buildResidentsString(std::vector<LLAvatarName> avatar_names, std::string& residents_string, bool complete_name)
{
	llassert(avatar_names.size() > 0);
	
	std::sort(avatar_names.begin(), avatar_names.end());
	const std::string& separator = LLTrans::getString("words_separator");
	for (std::vector<LLAvatarName>::const_iterator it = avatar_names.begin(); ; )
	{
		if(complete_name)
		{
			residents_string.append((*it).getCompleteName());
		}
		else
		{
			residents_string.append((*it).getDisplayName());
		}

		if	(++it == avatar_names.end())
		{
			break;
		}
		residents_string.append(separator);
	}
}

// static
void LLAvatarActions::buildResidentsString(const uuid_vec_t& avatar_uuids, std::string& residents_string)
{
	std::vector<LLAvatarName> avatar_names;
	uuid_vec_t::const_iterator it = avatar_uuids.begin();
	for (; it != avatar_uuids.end(); ++it)
	{
		LLAvatarName av_name;
		if (LLAvatarNameCache::get(*it, &av_name))
		{
			avatar_names.push_back(av_name);
		}
	}
	
	// We should check whether the vector is not empty to pass the assertion
	// that avatar_names.size() > 0 in LLAvatarActions::buildResidentsString.
	if (!avatar_names.empty())
	{
		LLAvatarActions::buildResidentsString(avatar_names, residents_string);
	}
}

//static
std::set<LLUUID> LLAvatarActions::getInventorySelectedUUIDs()
{
	std::set<LLFolderViewItem*> inventory_selected;

	LLInventoryPanel* active_panel = action_give_inventory::get_active_inventory_panel();
	if (active_panel)
	{
		inventory_selected= active_panel->getRootFolder()->getSelectionList();
	}

	if (inventory_selected.empty())
	{
		LLSidepanelInventory *sidepanel_inventory = LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
		if (sidepanel_inventory)
		{
			inventory_selected= sidepanel_inventory->getInboxSelectionList();
		}
	}

	std::set<LLUUID> inventory_selected_uuids;
	for (std::set<LLFolderViewItem*>::iterator it = inventory_selected.begin(), end_it = inventory_selected.end();
		it != end_it;
		++it)
	{
		inventory_selected_uuids.insert(static_cast<LLFolderViewModelItemInventory*>((*it)->getViewModelItem())->getUUID());
	}
	return inventory_selected_uuids;
}

//static
void LLAvatarActions::shareWithAvatars(LLView * panel)
{
	using namespace action_give_inventory;

    LLFloater* root_floater = gFloaterView->getParentFloater(panel);
	LLFloaterAvatarPicker* picker =
		LLFloaterAvatarPicker::show(boost::bind(give_inventory, _1, _2), TRUE, FALSE, FALSE, root_floater->getName());
	if (!picker)
	{
		return;
	}

	picker->setOkBtnEnableCb(boost::bind(is_give_inventory_acceptable));
	picker->openFriendsTab();
    
    if (root_floater)
    {
        root_floater->addDependentFloater(picker);
    }
	LLNotificationsUtil::add("ShareNotification");
}

// static
bool LLAvatarActions::canShareSelectedItems(LLInventoryPanel* inv_panel /* = NULL*/)
{
	using namespace action_give_inventory;

	if (!inv_panel)
	{
		LLInventoryPanel* active_panel = get_active_inventory_panel();
		if (!active_panel) return false;
		inv_panel = active_panel;
	}

	// check selection in the panel
	LLFolderView* root_folder = inv_panel->getRootFolder();
    if (!root_folder)
    {
        return false;
    }
	const std::set<LLFolderViewItem*> inventory_selected = root_folder->getSelectionList();
	if (inventory_selected.empty()) return false; // nothing selected

	const LLUUID trash_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
	bool can_share = true;
	std::set<LLFolderViewItem*>::const_iterator it = inventory_selected.begin();
	const std::set<LLFolderViewItem*>::const_iterator it_end = inventory_selected.end();
	for (; it != it_end; ++it)
	{
		LLUUID cat_id = static_cast<LLFolderViewModelItemInventory*>((*it)->getViewModelItem())->getUUID();
		LLViewerInventoryCategory* inv_cat = gInventory.getCategory(cat_id);
		// any category can be offered if it's not in trash.
		if (inv_cat)
		{
			if ((cat_id == trash_id) || gInventory.isObjectDescendentOf(cat_id, trash_id))
			{
				can_share = false;
				break;
			}
			continue;
		}

		// check if inventory item can be given
		LLFolderViewItem* item = *it;
		if (!item) return false;
		LLInvFVBridge* bridge = dynamic_cast<LLInvFVBridge*>(item->getViewModelItem());
		if (bridge && bridge->canShare())
		{
			continue;
		}

		// there are neither item nor category in inventory
		can_share = false;
		break;
	}

	return can_share;
}

// static
void LLAvatarActions::toggleBlock(const LLUUID& id)
{
	LLAvatarName av_name;
	LLAvatarNameCache::get(id, &av_name);

	LLMute mute(id, av_name.getUserName(), LLMute::AGENT);

	if (LLMuteList::getInstance()->isMuted(mute.mID, mute.mName))
	{
		LLMuteList::getInstance()->remove(mute);
	}
	else
	{
		LLMuteList::getInstance()->add(mute);
	}
}

// static
void LLAvatarActions::toggleMuteVoice(const LLUUID& id)
{
	LLAvatarName av_name;
	LLAvatarNameCache::get(id, &av_name);

	LLMuteList* mute_list = LLMuteList::getInstance();
	bool is_muted = mute_list->isMuted(id, LLMute::flagVoiceChat);

	LLMute mute(id, av_name.getUserName(), LLMute::AGENT);
	if (!is_muted)
	{
		mute_list->add(mute, LLMute::flagVoiceChat);
	}
	else
	{
		mute_list->remove(mute, LLMute::flagVoiceChat);
	}
}

// static
bool LLAvatarActions::canOfferTeleport(const LLUUID& id)
{
	// First use LLAvatarTracker::isBuddy()
	// If LLAvatarTracker::instance().isBuddyOnline function only is used
	// then for avatars that are online and not a friend it will return false.
	// But we should give an ability to offer a teleport for such avatars.
	if(LLAvatarTracker::instance().isBuddy(id))
	{
		return LLAvatarTracker::instance().isBuddyOnline(id);
	}

	return true;
}

// static
bool LLAvatarActions::canOfferTeleport(const uuid_vec_t& ids)
{
	// We can't send more than 250 lures in a single message, so disable this
	// button when there are too many id's selected.
	if(ids.size() > 250) return false;
	
	bool result = true;
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		if(!canOfferTeleport(*it))
		{
			result = false;
			break;
		}
	}
	return result;
}

void LLAvatarActions::inviteToGroup(const LLUUID& id)
{
	LLFloaterGroupPicker* widget = LLFloaterReg::showTypedInstance<LLFloaterGroupPicker>("group_picker", LLSD(id));
	if (widget)
	{
		widget->center();
		widget->setPowersMask(GP_MEMBER_INVITE);
		widget->removeNoneOption();
		widget->setSelectGroupCallback(boost::bind(callback_invite_to_group, _1, id));
	}
}

// static
void LLAvatarActions::viewChatHistory(const LLUUID& id)
{
	const std::vector<LLConversation>& conversations = LLConversationLog::instance().getConversations();
	std::vector<LLConversation>::const_iterator iter = conversations.begin();

	for (; iter != conversations.end(); ++iter)
	{
		if (iter->getParticipantID() == id)
		{
			LLFloaterReg::showInstance("preview_conversation", iter->getSessionID(), true);
			return;
		}
	}

	if (LLLogChat::isTranscriptExist(id))
	{
		LLAvatarName avatar_name;
		LLSD extended_id(id);

		LLAvatarNameCache::get(id, &avatar_name);
		extended_id[LL_FCP_COMPLETE_NAME] = avatar_name.getCompleteName();
		if (gSavedSettings.getBool("UseLegacyLogNames"))
		{
			const std::string& username = avatar_name.getUserName();
			extended_id[LL_FCP_ACCOUNT_NAME] = username.substr(0, username.find(" Resident"));
		}
		else
		{
			extended_id[LL_FCP_ACCOUNT_NAME] = avatar_name.getAccountName();
		}
		LLFloaterReg::showInstance("preview_conversation", extended_id, true);
	}
}

//== private methods ========================================================================================

// static
bool LLAvatarActions::handleRemove(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	const LLSD& ids = notification["payload"]["ids"];
	for (LLSD::array_const_iterator itr = ids.beginArray(); itr != ids.endArray(); ++itr)
	{
		LLUUID id = itr->asUUID();
		const LLRelationship* ip = LLAvatarTracker::instance().getBuddyInfo(id);
		if (ip)
		{
			switch (option)
			{
			case 0: // YES
				if( ip->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS))
				{
					LLAvatarTracker::instance().empower(id, FALSE);
					LLAvatarTracker::instance().notifyObservers();
				}
				LLAvatarTracker::instance().terminateBuddy(id);
				LLAvatarTracker::instance().notifyObservers();
				break;

			case 1: // NO
			default:
				LL_INFOS() << "No removal performed." << LL_ENDL;
				break;
			}
		}
	}
	return false;
}

// static
bool LLAvatarActions::handlePay(const LLSD& notification, const LLSD& response, LLUUID avatar_id)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		gAgent.setDoNotDisturb(false);
	}

	LLFloaterPayUtil::payDirectly(&give_money, avatar_id, /*is_group=*/false);
	return false;
}

// static
void LLAvatarActions::callback_invite_to_group(LLUUID group_id, LLUUID id)
{
	uuid_vec_t agent_ids;
	agent_ids.push_back(id);
	
	LLFloaterGroupInvite::showForGroup(group_id, &agent_ids);
}


// static
bool LLAvatarActions::callbackAddFriendWithMessage(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		requestFriendship(notification["payload"]["id"].asUUID(), 
		    notification["payload"]["name"].asString(),
		    response["message"].asString());
	}
	return false;
}



// static
bool LLAvatarActions::handleGodKick(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		U32 type = static_cast<U32>(notification["payload"]["type"].asInteger());
		LLMessageSystem* msg = gMessageSystem;

		msg->newMessageFast(_PREHASH_GodKickUser);
		msg->nextBlockFast(_PREHASH_UserInfo);
		msg->addUUIDFast(_PREHASH_GodID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_GodSessionID, gAgent.getSessionID());
		msg->addUUIDFast(_PREHASH_AgentID, avatar_id);
		msg->addU32Fast(_PREHASH_KickFlags, type);
		msg->addStringFast(_PREHASH_Reason, response["message"].asString());
		gAgent.sendReliableMessage();
	}
	return false;
}

// static
void LLAvatarActions::requestFriendship(const LLUUID& target_id, const std::string& target_name, const std::string& message)
{
	const LLUUID calling_card_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	send_improved_im(target_id,
					 target_name,
					 message,
					 IM_ONLINE,
					 IM_FRIENDSHIP_OFFERED,
					 calling_card_folder_id);

	LLSD args;
	args["TO_NAME"] = target_name;

	LLSD payload;
	payload["from_id"] = target_id;
	LLNotificationsUtil::add("FriendshipOffered", args, payload);
}

//static
bool LLAvatarActions::isFriend(const LLUUID& id)
{
	return (nullptr != LLAvatarTracker::instance().getBuddyInfo(id) );
}

// static
bool LLAvatarActions::isBlocked(const LLUUID& id)
{
	LLAvatarName av_name;
	LLAvatarNameCache::get(id, &av_name);
	return LLMuteList::getInstance()->isMuted(id, av_name.getUserName());
}

// static
bool LLAvatarActions::isVoiceMuted(const LLUUID& id)
{
	return LLMuteList::getInstance()->isMuted(id, LLMute::flagVoiceChat);
}

// static
bool LLAvatarActions::canBlock(const LLUUID& id)
{
	LLAvatarName av_name;
	LLAvatarNameCache::get(id, &av_name);

	std::string full_name = av_name.getUserName();
	bool is_linden = (full_name.find("Linden") != std::string::npos);
	bool is_self = id == gAgentID;
	return !is_self && !is_linden;
}

//static
bool LLAvatarActions::isAgentMappable(const LLUUID& agent_id)
{
	const LLRelationship* buddy_info = nullptr;
	bool is_friend = LLAvatarActions::isFriend(agent_id);
	
	if (is_friend)
		buddy_info = LLAvatarTracker::instance().getBuddyInfo(agent_id);
	
	return (buddy_info &&
			buddy_info->isOnline() &&
			buddy_info->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION)
			);
}

// ------------------------------------------------------------------------------------------------
// <alchemy> Alchemy functions below this line
// ------------------------------------------------------------------------------------------------
// static
void LLAvatarActions::copyData(const LLUUID& id, ECopyDataType type)
{
	if (id.notNull())
	{
		uuid_vec_t id_vec = { id };
		copyData(id_vec, type);
	}
}

// static
void LLAvatarActions::copyData(const uuid_vec_t& ids, ECopyDataType type)
{
	if (!ids.empty())
	{
		std::string data_string;
		static LLCachedControl<std::string> seperator(gSavedSettings, "AlchemyCopySeperator", ", ");
		for (const LLUUID& id : ids)
		{
			if (id.isNull())
				continue;

			if (!data_string.empty())
				data_string.append(seperator);

			switch (type)
			{
			case E_DATA_NAME:
			{
				LLAvatarName av_name;
				LLAvatarNameCache::get(id, &av_name);
				data_string.append(av_name.getUserName());
				break;
			}
			case E_DATA_SLURL:
				data_string.append(LLSLURL("agent", id, "about").getSLURLString());
				break;
			case E_DATA_UUID:
				data_string.append(id.asString());
				break;
			default:
				break;
			}
		}

		if (!data_string.empty())
		{
			LLWString wdata_str = utf8str_to_wstring(data_string);
			LLClipboard::instance().copyToClipboard(wdata_str, 0, wdata_str.length());
		}
	}
}

// static
bool LLAvatarActions::canTeleportTo(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return false;

	LLWorld::pos_map_t positions;
	LLWorld::getInstance()->getAvatars(&positions, gAgent.getPositionGlobal());
	auto iter = positions.find(avatar_id);
	if (iter != positions.cend())
	{
		const auto& id = iter->first;
		const auto& pos = iter->second;
		if (id != gAgentID && !pos.isNull())
		{
			return true;
		}
	}
	return false;
}

// static
void LLAvatarActions::teleportTo(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return;

	LLWorld::pos_map_t positions;
	LLWorld::getInstance()->getAvatars(&positions, gAgent.getPositionGlobal());
	auto iter = positions.find(avatar_id);
	if (iter != positions.cend())
	{
		const auto& id = iter->first;
		const auto& pos = iter->second;
		if (id != gAgentID && !pos.isNull())
		{
			gAgent.teleportViaLocation(pos);
		}
	}
}

// static 
bool LLAvatarActions::canFreezeEject(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return false;

	uuid_vec_t ids = { avatar_id };
	return canFreezeEject(ids);
}

// static 
bool LLAvatarActions::canFreezeEject(const uuid_vec_t& ids)
{
	if (ids.empty())
		return false;

	if (std::find(ids.cbegin(), ids.cend(), gAgentID) != ids.cend())
		return false;

	// Gods can always freeze and eject
	if (gAgent.isGodlike())
		return true;

	LLWorld::region_gpos_map_t idRegions;
	LLWorld::getInstance()->getAvatars(&idRegions, gAgent.getPositionGlobal());

	auto ret = false;

	for (const auto& id : ids)
	{
		if (id.isNull())
			continue;
		auto it = idRegions.find(id);
		if (it != idRegions.cend())
		{
			const auto& region = it->second.first;
			const auto& pos_global = it->second.second;
			if (region)
			{
				// Estate owners / managers can freeze
				// Parcel owners can also freeze
				const auto& local_pos = region->getPosRegionFromGlobal(pos_global);
				LLParcel* parcel = LLViewerParcelMgr::getInstance()->selectParcelAt(pos_global)->getParcel();

				ret = region->isOwnedSelf(local_pos);
				if (!ret || region->isOwnedGroup(local_pos))
				{
					ret = LLViewerParcelMgr::getInstance()->isParcelOwnedByAgent(parcel, GP_LAND_ADMIN);
				}
				// We hit a false so bail out early in case of multi-select
				if (!ret)
				{
					break;
				}
				continue;
			}
		}
		ret = false;
		break;
	}
	return ret;
}

// static
void LLAvatarActions::parcelFreeze(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return;

	uuid_vec_t ids = { avatar_id };
	return parcelFreeze(ids);
}

// static
void LLAvatarActions::parcelFreeze(const uuid_vec_t& ids)
{
	if (ids.empty())
		return;

	LLSD payload;
	payload["avatar_ids"] = LLSDArray();
	std::string avatars;
	for (auto it = ids.cbegin(), end_it = ids.cend(); it != end_it; ++it)
	{
		const auto id = *it;
		if (id.notNull())
		{
			payload["avatar_ids"].append(id);

			if (!avatars.empty())
				avatars += "\n";

			avatars += LLSLURL("agent", id, "about").getSLURLString();
		}
	}

	LLSD args;
	args["AVATAR_NAMES"] = avatars;

	LLNotificationsUtil::add((payload["avatar_ids"].size() == 1) ? "FreezeAvatarFullname" : "FreezeAvatarMultiple", args, payload, handleParcelFreeze);
}

// static
void LLAvatarActions::parcelEject(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return;

	uuid_vec_t ids = { avatar_id };
	return parcelEject(ids);
}

// static
void LLAvatarActions::parcelEject(const uuid_vec_t& ids)
{
	if (ids.empty())
		return;

	LLWorld::pos_map_t avatar_positions;
	LLWorld::getInstance()->getAvatars(&avatar_positions, gAgent.getPositionGlobal());

	LLSD payload;
	payload["avatar_ids"] = LLSDArray();
	std::string avatars;
	bool ban_enabled = false;
	bool ban_killed = false;
	for (auto it = ids.cbegin(), end_it = ids.cend(); it != end_it; ++it)
	{
		const auto id = *it;
		if (id.notNull())
		{
			payload["avatar_ids"].append(id);

			if (!ban_killed)
			{
				const auto& pos_it = avatar_positions.find(id);
				if (pos_it != avatar_positions.cend())
				{
					const auto& pos = pos_it->second;
					LLParcel* parcel = LLViewerParcelMgr::getInstance()->selectParcelAt(pos)->getParcel();
					if (parcel)
					{
						ban_enabled = LLViewerParcelMgr::getInstance()->isParcelOwnedByAgent(parcel, GP_LAND_MANAGE_BANNED);
						if (!ban_enabled)
						{
							ban_killed = true;
						}
					}
				}
			}

			if (!avatars.empty())
				avatars += "\n";

			avatars += LLSLURL("agent", id, "about").getSLURLString();
		}
	}
	payload["ban_enabled"] = ban_enabled;

	LLSD args;
	args["AVATAR_NAMES"] = avatars;
	std::string notification;
	bool is_single = (payload["avatar_ids"].size() == 1);
	if (ban_enabled)
	{
		notification = is_single ? "EjectAvatarFullname" : "EjectAvatarMultiple";
	}
	else
	{
		notification = is_single ? "EjectAvatarFullnameNoBan" : "EjectAvatarMultipleNoBan";
	}
	LLNotificationsUtil::add(notification, args, payload, handleParcelEject);
}

// static
bool LLAvatarActions::canManageAvatarsEstate(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return false;

	uuid_vec_t ids = { avatar_id };
	return canManageAvatarsEstate(ids);
}

// static
bool LLAvatarActions::canManageAvatarsEstate(const uuid_vec_t& ids)
{
	if (ids.empty())
		return false;

	if (std::find(ids.cbegin(), ids.cend(), gAgentID) != ids.cend())
		return false;

	// Gods can always estate manage
	if (gAgent.isGodlike())
		return true;

	LLWorld::region_gpos_map_t idRegions;
	LLWorld::getInstance()->getAvatars(&idRegions, gAgent.getPositionGlobal());

	auto ret = false;

	for (const auto& id : ids)
	{
		if (id.isNull())
			continue;
		auto it = idRegions.find(id);
		if (it != idRegions.cend())
		{
			auto region = it->second.first;
			if (region)
			{
				const auto& owner = region->getOwner();
				// Can't manage the estate owner!
				if (owner == id)
				{
					ret = false;
					break;
				}
				ret = (owner == gAgentID || region->canManageEstate());
				continue;
			}
		}
		ret = false;
		break;
	}
	return ret;
}

// static
void LLAvatarActions::estateTeleportHome(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return;

	uuid_vec_t ids = { avatar_id };
	estateTeleportHome(ids);
}

// static
void LLAvatarActions::estateTeleportHome(const uuid_vec_t& ids)
{
	if (ids.empty())
		return;

	LLSD payload;
	payload["avatar_ids"] = LLSDArray();
	std::string avatars;
	for (auto it = ids.cbegin(), end_it = ids.cend(); it != end_it; ++it)
	{
		const auto id = *it;
		if (id.notNull())
		{
			payload["avatar_ids"].append(id);

			if (!avatars.empty())
				avatars += "\n";

			avatars += LLSLURL("agent", id, "about").getSLURLString();
		}
	}

	LLSD args;
	args["AVATAR_NAMES"] = avatars;

	LLNotificationsUtil::add((payload["avatar_ids"].size() == 1) ? "EstateTeleportHomeSingle" : "EstateTeleportHomeMulti", args, payload, handleEstateTeleportHome);
}

// static
void LLAvatarActions::estateKick(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return;

	uuid_vec_t ids = { avatar_id };
	estateKick(ids);
}

// static
void LLAvatarActions::estateKick(const uuid_vec_t& ids)
{
	if (ids.empty())
		return;

	LLSD payload;
	payload["avatar_ids"] = LLSDArray();
	std::string avatars;
	for (auto it = ids.cbegin(), end_it = ids.cend(); it != end_it; ++it)
	{
		const auto id = *it;
		if (id.notNull())
		{
			payload["avatar_ids"].append(id);

			if (!avatars.empty())
				avatars += "\n";

			avatars += LLSLURL("agent", id, "about").getSLURLString();
		}
	}

	LLSD args;
	args["AVATAR_NAMES"] = avatars;

	LLNotificationsUtil::add((payload["avatar_ids"].size() == 1) ? "EstateKickSingle" : "EstateKickMultiple", args, payload, handleEstateKick);
}

// static
void LLAvatarActions::estateBan(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
		return;

	uuid_vec_t ids = { avatar_id };
	estateBan(ids);
}

// static
void LLAvatarActions::estateBan(const uuid_vec_t& ids)
{
	if (ids.empty())
		return;

	auto region = gAgent.getRegion();
	if (!region)
		return;

	LLSD payload;
	payload["avatar_ids"] = LLSDArray();
	std::string avatars;
	for (auto it = ids.cbegin(), end_it = ids.cend(); it != end_it; ++it)
	{
		const auto id = *it;
		if (id.notNull())
		{
			payload["avatar_ids"].append(id);

			if (!avatars.empty())
				avatars += "\n";

			avatars += LLSLURL("agent", id, "about").getSLURLString();
		}
	}

	LLSD args;
	args["AVATAR_NAMES"] = avatars;
	std::string owner = LLSLURL("agent", region->getOwner(), "inspect").getSLURLString();
	if (gAgent.isGodlike())
	{
		LLStringUtil::format_map_t owner_args;
		owner_args["[OWNER]"] = owner;
		args["ALL_ESTATES"] = LLTrans::getString("RegionInfoAllEstatesOwnedBy", owner_args);
	}
	else if (region->getOwner() == gAgent.getID())
	{
		args["ALL_ESTATES"] = LLTrans::getString("RegionInfoAllEstatesYouOwn");
	}
	else if (region->isEstateManager())
	{
		LLStringUtil::format_map_t owner_args;
		owner_args["[OWNER]"] = owner.c_str();
		args["ALL_ESTATES"] = LLTrans::getString("RegionInfoAllEstatesYouManage", owner_args);
	}

	bool single_user = (payload["avatar_ids"].size() == 1);
	LLNotificationsUtil::add(single_user ? "EstateBanSingle" : "EstateBanMultiple", args, payload, handleEstateKick);
}

// static
bool LLAvatarActions::handleParcelFreeze(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (0 == option || 1 == option)
	{
		U32 flags = 0x0;
		if (1 == option)
		{
			// unfreeze
			flags |= 0x1;
		}
		const auto& avatar_ids = notification["payload"]["avatar_ids"];
		for (LLSD::array_const_iterator it = avatar_ids.beginArray(), it_end = avatar_ids.endArray(); it != it_end; ++it)
		{
			const auto& id = it->asUUID();
			LLMessageSystem* msg = gMessageSystem;

			msg->newMessage("FreezeUser");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgent.getID());
			msg->addUUID("SessionID", gAgent.getSessionID());
			msg->nextBlock("Data");
			msg->addUUID("TargetID", id);
			msg->addU32("Flags", flags);
			gAgent.sendReliableMessage();
		}
	}
	return false;
}

// static
bool LLAvatarActions::handleParcelEject(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (2 == option)
	{
		return false;
	}

	bool ban_enabled = notification["payload"]["ban_enabled"].asBoolean();
	const auto& avatar_ids = notification["payload"]["avatar_ids"];
	for (LLSD::array_const_iterator it = avatar_ids.beginArray(), it_end = avatar_ids.endArray(); it != it_end; ++it)
	{
		const auto& id = it->asUUID();
		U32 flags = (1 == option && ban_enabled) ? 0x1 : 0x0;
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("EjectUser");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("TargetID", id);
		msg->addU32("Flags", flags);
		gAgent.sendReliableMessage();
	}
	return false;
}

// static
bool LLAvatarActions::handleEstateTeleportHome(const LLSD & notification, const LLSD & response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		LLWorld::region_gpos_map_t idRegions;
		LLWorld::getInstance()->getAvatars(&idRegions, gAgent.getPositionGlobal());
		const auto& avatar_ids = notification["payload"]["avatar_ids"];
		for (LLSD::array_const_iterator it = avatar_ids.beginArray(), it_end = avatar_ids.endArray(); it != it_end; ++it)
		{
			const auto& id = it->asUUID();
			LLViewerRegion* regionp = nullptr;
			auto idreg_it = idRegions.find(id);
			if (idreg_it != idRegions.cend())
			{
				regionp = idreg_it->second.first;
			}

			if (!regionp)
				continue;

			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_EstateOwnerMessage);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
			msg->nextBlockFast(_PREHASH_MethodData);
			msg->addStringFast(_PREHASH_Method, "teleporthomeuser");
			msg->addUUIDFast(_PREHASH_Invoice, LLUUID::null);
			msg->nextBlockFast(_PREHASH_ParamList);
			msg->addStringFast(_PREHASH_Parameter, gAgent.getID().asString().c_str());
			msg->nextBlockFast(_PREHASH_ParamList);
			msg->addStringFast(_PREHASH_Parameter, id.asString().c_str());
			msg->sendReliable(regionp->getHost());
		}
	}
	return false;
}

// static
bool LLAvatarActions::handleEstateKick(const LLSD & notification, const LLSD & response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		LLWorld::region_gpos_map_t idRegions;
		LLWorld::getInstance()->getAvatars(&idRegions, gAgent.getPositionGlobal());
		const auto& avatar_ids = notification["payload"]["avatar_ids"];
		for (LLSD::array_const_iterator it = avatar_ids.beginArray(), it_end = avatar_ids.endArray(); it != it_end; ++it)
		{
			const auto& id = it->asUUID();
			LLViewerRegion* regionp = nullptr;
			auto idreg_it = idRegions.find(id);
			if (idreg_it != idRegions.cend())
			{
				regionp = idreg_it->second.first;
			}

			if (!regionp)
				continue;

			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_EstateOwnerMessage);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
			msg->nextBlockFast(_PREHASH_MethodData);
			msg->addStringFast(_PREHASH_Method, "kickestate");
			msg->addUUIDFast(_PREHASH_Invoice, LLUUID::null);
			msg->nextBlockFast(_PREHASH_ParamList);
			msg->addStringFast(_PREHASH_Parameter, id.asString().c_str());
			msg->sendReliable(regionp->getHost());
		}
	}
	return false;
}

// static
bool LLAvatarActions::handleEstateBan(const LLSD & notification, const LLSD & response)
{
	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
		return false;

	S32 option = LLNotification::getSelectedOption(notification, response);

	if (0 == option || 1 == option)
	{
		const auto& avatar_ids = notification["payload"]["avatar_ids"];
		for (LLSD::array_const_iterator it = avatar_ids.beginArray(), it_end = avatar_ids.endArray(); it != it_end; ++it)
		{
			const auto& id = it->asUUID();
			if (region->getOwner() == id)
			{
				// Can't ban the owner!
				continue;
			}

			U32 flags = ESTATE_ACCESS_BANNED_AGENT_ADD | ESTATE_ACCESS_ALLOWED_AGENT_REMOVE;

			if (it + 1 != it_end)
			{
				flags |= ESTATE_ACCESS_NO_REPLY;
			}

			if (option == 1)
			{
				if (region->getOwner() == gAgent.getID() || gAgent.isGodlike())
				{
					flags |= ESTATE_ACCESS_APPLY_TO_ALL_ESTATES;
				}
				else if (region->isEstateManager())
				{
					flags |= ESTATE_ACCESS_APPLY_TO_MANAGED_ESTATES;
				}
			}

			LLFloaterRegionInfo::nextInvoice();
			LLPanelEstateInfo::sendEstateAccessDelta(flags, id);
		}
	}
	return false;
}
