// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llgroupactions.cpp
 * @brief Group-related actions (join, leave, new, delete, etc)
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

#include "llgroupactions.h"

#include "message.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llclipboard.h"
#include "llfloaterreg.h"
#include "llfloatergroupprofile.h"
#include "llfloatersidepanelcontainer.h"
#include "llgroupmgr.h"
#include "llfloaterimcontainer.h"
#include "llimview.h" // for gIMMgr
#include "llnotificationsutil.h"
#include "llpanelgroup.h"
#include "llslurl.h"
#include "llstatusbar.h"	// can_afford_transaction()
#include "groupchatlistener.h"

//
// Globals
//
static GroupChatListener sGroupChatListener;

class LLGroupHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLGroupHandler() : LLCommandHandler("group", UNTRUSTED_THROTTLE) { }
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web) override
	{
		if (!LLUI::sSettingGroups["config"]->getBOOL("EnableGroupInfo"))
		{
			LLNotificationsUtil::add("NoGroupInfo", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
			return true;
		}

		if (tokens.size() < 1)
		{
			return false;
		}

		if (tokens[0].asString() == "create")
		{
			LLGroupActions::createGroup();
			return true;
		}

		if (tokens.size() < 2)
		{
			return false;
		}

		if (tokens[0].asString() == "list")
		{
			if (tokens[1].asString() == "show")
			{
				LLSD params;
				params["people_panel_tab_name"] = "groups_panel";
				LLFloaterSidePanelContainer::showPanel("people", "panel_people", params);
				return true;
			}
            return false;
		}

		LLUUID group_id;
		if (!group_id.set(tokens[0], FALSE))
		{
			return false;
		}

		if (tokens[1].asString() == "about")
		{
			if (group_id.isNull())
				return true;

			LLGroupActions::show(group_id);

			return true;
		}
		if (tokens[1].asString() == "inspect")
		{
			if (group_id.isNull())
				return true;
			LLGroupActions::inspect(group_id);
			return true;
		}
		return false;
	}
};
LLGroupHandler gGroupHandler;

// This object represents a pending request for specified group member information
// which is needed to check whether avatar can leave group
class LLFetchGroupMemberData : public LLGroupMgrObserver
{
public:
	LLFetchGroupMemberData(const LLUUID& group_id) : 
		LLGroupMgrObserver(group_id),
		mGroupId(group_id),
		mRequestProcessed(false) 
	{
		LL_INFOS() << "Sending new group member request for group_id: "<< group_id << LL_ENDL;
		LLGroupMgr* mgr = LLGroupMgr::getInstance();
		// register ourselves as an observer
		mgr->addObserver(this);
		// send a request
		mgr->sendGroupPropertiesRequest(group_id);
		mgr->sendCapGroupMembersRequest(group_id);
	}

	~LLFetchGroupMemberData()
	{
		if (!mRequestProcessed)
		{
			// Request is pending
			LL_WARNS() << "Destroying pending group member request for group_id: "
				<< mGroupId << LL_ENDL;
		}
		// Remove ourselves as an observer
		LLGroupMgr::getInstance()->removeObserver(this);
	}

	void changed(LLGroupChange gc) override
	{
		if (gc == GC_PROPERTIES && !mRequestProcessed)
		{
			LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupId);
			if (!gdatap)
			{
				LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData() was NULL" << LL_ENDL;
			} 
			else if (!gdatap->isMemberDataComplete())
			{
				LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData()->isMemberDataComplete() was FALSE" << LL_ENDL;
				processGroupData();
				mRequestProcessed = true;
			}
		}
	}

	LLUUID getGroupId() const { return mGroupId; }
	virtual void processGroupData() = 0;
protected:
	LLUUID mGroupId;
private:
	bool mRequestProcessed;
};

class LLFetchLeaveGroupData: public LLFetchGroupMemberData
{
public:
	 LLFetchLeaveGroupData(const LLUUID& group_id)
		 : LLFetchGroupMemberData(group_id)
	 {}
	 void processGroupData() override
	 {
		 LLGroupActions::processLeaveGroupDataResponse(mGroupId);
	 }
};

LLFetchLeaveGroupData* gFetchLeaveGroupData = nullptr;

// static
void LLGroupActions::search()
{
	LLFloaterReg::showInstance("search", LLSD().with("category", "groups"));
}

// static
void LLGroupActions::startCall(const LLUUID& group_id)
{
	// create a new group voice session
	LLGroupData gdata;

	if (!gAgent.getGroupData(group_id, gdata))
	{
		LL_WARNS() << "Error getting group data" << LL_ENDL;
		return;
	}

	LLUUID session_id = gIMMgr->addSession(gdata.mName, IM_SESSION_GROUP_START, group_id, true);
	if (session_id == LLUUID::null)
	{
		LL_WARNS() << "Error adding session" << LL_ENDL;
		return;
	}

	// start the call
	gIMMgr->autoStartCallOnStartup(session_id);

	make_ui_sound("UISndStartIM");
}

// static
void LLGroupActions::join(const LLUUID& group_id)
{
	if (!gAgent.canJoinGroups())
	{
		LLNotificationsUtil::add("JoinedTooManyGroups");
		return;
	}

	LLGroupMgrGroupData* gdatap = 
		LLGroupMgr::getInstance()->getGroupData(group_id);

	if (gdatap)
	{
		S32 cost = gdatap->mMembershipFee;
		LLSD args;
		args["COST"] = llformat("%d", cost);
		args["NAME"] = gdatap->mName;
		LLSD payload;
		payload["group_id"] = group_id;

		if (can_afford_transaction(cost))
		{
			if(cost > 0)
				LLNotificationsUtil::add("JoinGroupCanAfford", args, payload, onJoinGroup);
			else
				LLNotificationsUtil::add("JoinGroupNoCost", args, payload, onJoinGroup);
				
		}
		else
		{
			LLNotificationsUtil::add("JoinGroupCannotAfford", args, payload);
		}
	}
	else
	{
		LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData(" << group_id 
			<< ") was NULL" << LL_ENDL;
	}
}

// static
bool LLGroupActions::onJoinGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (option == 1)
	{
		// user clicked cancel
		return false;
	}

	LLGroupMgr::getInstance()->
		sendGroupMemberJoin(notification["payload"]["group_id"].asUUID());
	return false;
}

// static
void LLGroupActions::leave(const LLUUID& group_id)
{
	if (group_id.isNull())
	{
		return;
	}

	LLGroupData group_data;
	if (gAgent.getGroupData(group_id, group_data))
	{
		LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
		if (!gdatap || !gdatap->isMemberDataComplete())
		{
			if (gFetchLeaveGroupData != nullptr)
			{
				delete gFetchLeaveGroupData;
				gFetchLeaveGroupData = nullptr;
			}
			gFetchLeaveGroupData = new LLFetchLeaveGroupData(group_id);
		}
		else
		{
			processLeaveGroupDataResponse(group_id);
		}
	}
}

//static
void LLGroupActions::processLeaveGroupDataResponse(const LLUUID group_id)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
	LLUUID agent_id = gAgent.getID();
	LLGroupMgrGroupData::member_list_t::iterator mit = gdatap->mMembers.find(agent_id);
	//get the member data for the group
	if ( mit != gdatap->mMembers.end() )
	{
		LLGroupMemberData* member_data = (*mit).second;

		if ( member_data && member_data->isOwner() && gdatap->mMemberCount == 1)
		{
			LLNotificationsUtil::add("OwnerCannotLeaveGroup");
			return;
		}
	}
	LLSD args;
	args["GROUP"] = gdatap->mName;
	LLSD payload;
	payload["group_id"] = group_id;
	LLNotificationsUtil::add("GroupLeaveConfirmMember", args, payload, onLeaveGroup);
}

// static
void LLGroupActions::activate(const LLUUID& group_id)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ActivateGroup);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_GroupID, group_id);
	gAgent.sendReliableMessage();
}

#if 0 // this isn't used!!!! <alchemy>
static bool isGroupUIVisible(const LLUUID& group_id)
{
	auto* floaterp = LLFloaterReg::findInstance("group_profile", LLSD(group_id));
	return LLFloater::isVisible(floaterp);
}
#endif

// static 
void LLGroupActions::inspect(const LLUUID& group_id)
{
	LLFloaterReg::showInstance("inspect_group", LLSD().with("group_id", group_id));
}

// static
void LLGroupActions::show(const LLUUID& group_id)
{
	if (group_id.isNull()) return;
	LLFloaterReg::showTypedInstance<LLFloaterGroupProfile>("group_profile", group_id, TAKE_FOCUS_YES);
}

// static
void LLGroupActions::refresh_notices()
{
	for (auto panel : sGroupPanelInstances)
	{
		panel.second->refreshNotices();
	}
}

//static 
void LLGroupActions::refresh(const LLUUID& group_id)
{
	auto panels = sGroupPanelInstances.equal_range(group_id);
	std::for_each(panels.first, panels.second, [](panel_multimap_t::value_type& p)
	{
		p.second->refreshData();
	});
}

//static 
void LLGroupActions::createGroup()
{
	auto profile = LLFloaterReg::showTypedInstance<LLFloaterGroupProfile>("group_profile", LLUUID::null, TAKE_FOCUS_YES);
	profile->createGroup();
}

//static
void LLGroupActions::closeGroup(const LLUUID& group_id)
{
	auto* floaterp = LLFloaterReg::findInstance("group_profile", LLSD(group_id));
	if (floaterp)
	{
		floaterp->closeFloater();
	}
}

// static
LLUUID LLGroupActions::startIM(const LLUUID& group_id)
{
	if (group_id.isNull()) return LLUUID::null;

	LLGroupData group_data;
	if (gAgent.getGroupData(group_id, group_data))
	{
		// Unmute the group if the user tries to start a session with it.
		LLMuteList::instance().removeGroup(group_id);
		LLUUID session_id = gIMMgr->addSession(
			group_data.mName,
			IM_SESSION_GROUP_START,
			group_id);
		if (session_id != LLUUID::null)
		{
			LLFloaterIMContainer::getInstance()->showConversation(session_id);
		}
		make_ui_sound("UISndStartIM");
		return session_id;
	}
	// this should never happen, as starting a group IM session
	// relies on you belonging to the group and hence having the group data
	make_ui_sound("UISndInvalidOp");
	return LLUUID::null;
}

// static
void LLGroupActions::endIM(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;
	
	LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
	if (session_id != LLUUID::null)
	{
		gIMMgr->leaveSession(session_id);
	}
}

// static
bool LLGroupActions::isInGroup(const LLUUID& group_id)
{
	// *TODO: Move all the LLAgent group stuff into another class, such as
	// this one.
	return gAgent.isInGroup(group_id);
}

// static
bool LLGroupActions::isAvatarMemberOfGroup(const LLUUID& group_id, const LLUUID& avatar_id)
{
	if(group_id.isNull() || avatar_id.isNull())
	{
		return false;
	}

	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->getGroupData(group_id);
	if(!group_data)
	{
		return false;
	}

	if(group_data->mMembers.end() == group_data->mMembers.find(avatar_id))
	{
		return false;
	}

	return true;
}

// static
void LLGroupActions::copyData(const LLUUID& group_id, ECopyDataType data_type)
{
	if (group_id.notNull())
	{
		std::string tmp;
		switch (data_type)
		{
		case E_DATA_NAME:
			tmp = LLGroupMgr::getInstance()->getGroupData(group_id)->mName;
			break;
		case E_DATA_SLURL:
			tmp = LLSLURL("group", group_id, "about").getSLURLString();
			break;
		case E_DATA_UUID:
			tmp = group_id.asString();
			break;
		default:
			break;
		}
		if (!tmp.empty())
		{
			LLWString wstr = utf8str_to_wstring(tmp);
			LLClipboard::instance().copyToClipboard(wstr, 0, wstr.length());
		}
	}
}

//-- Private methods ----------------------------------------------------------

// static
bool LLGroupActions::onLeaveGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLUUID group_id = notification["payload"]["group_id"].asUUID();
	if(option == 0)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_LeaveGroupRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_GroupData);
		msg->addUUIDFast(_PREHASH_GroupID, group_id);
		gAgent.sendReliableMessage();
	}
	return false;
}
