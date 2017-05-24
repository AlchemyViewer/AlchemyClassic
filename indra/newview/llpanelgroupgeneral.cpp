// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llpanelgroupgeneral.cpp
 * @brief General information about a group.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#include "llpanelgroupgeneral.h"

#include "llagent.h"
#include "lleconomy.h"
#include "llsdparam.h"
#include "roles_constants.h"

// UI elements
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldbstrings.h"
#include "llgroupactions.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotificationsutil.h"
#include "llspinctrl.h"
#include "llslurl.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lltrans.h"
#include "llmutelist.h"

static LLPanelInjector<LLPanelGroupGeneral> t_panel_group_general("panel_group_general");

// consts
const S32 MATURE_CONTENT = 1;
const S32 NON_MATURE_CONTENT = 2;
const S32 DECLINE_TO_STATE = 0;


LLPanelGroupGeneral::LLPanelGroupGeneral()
:	LLPanelGroupTab(),
	mChanged(FALSE),
	mFirstUse(TRUE),
	mGroupNameEditor(nullptr),
	mFounderName(nullptr),
	mInsignia(nullptr),
	mEditCharter(nullptr),
	mCtrlShowInGroupList(nullptr),
	mCtrlOpenEnrollment(nullptr),
	mCtrlEnrollmentFee(nullptr),
	mSpinEnrollmentFee(nullptr),
	mCtrlReceiveNotices(nullptr),
	mCtrlListGroup(nullptr),
	mActiveTitleLabel(nullptr),
	mComboActiveTitle(nullptr),
	mComboMature(nullptr),
	mCtrlReceiveGroupChat(nullptr)
{

}

LLPanelGroupGeneral::~LLPanelGroupGeneral()
{
}

BOOL LLPanelGroupGeneral::postBuild()
{
	bool recurse = true;

	mEditCharter = getChild<LLTextEditor>("charter", recurse);
	mEditCharter->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny, this));
	mEditCharter->setFocusReceivedCallback(boost::bind(onFocusEdit, _1, this));
	mEditCharter->setFocusChangedCallback(boost::bind(onFocusEdit, _1, this));

	// Options
	mCtrlShowInGroupList = getChild<LLCheckBoxCtrl>("show_in_group_list", recurse);
	mCtrlShowInGroupList->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny, this));

	mComboMature = getChild<LLComboBox>("group_mature_check", recurse);
	mComboMature->setCurrentByIndex(0);
	mComboMature->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny, this));
	if (gAgent.isTeen())
	{
		// Teens don't get to set mature flag. JC
		mComboMature->setVisible(FALSE);
		mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
	}

	mCtrlOpenEnrollment = getChild<LLCheckBoxCtrl>("open_enrollement", recurse);
	mCtrlOpenEnrollment->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny, this));

	mCtrlEnrollmentFee = getChild<LLCheckBoxCtrl>("check_enrollment_fee", recurse);
	mCtrlEnrollmentFee->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitEnrollment, this));

	mSpinEnrollmentFee = getChild<LLSpinCtrl>("spin_enrollment_fee", recurse);
	mSpinEnrollmentFee->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny, this));
	mSpinEnrollmentFee->setPrecision(0);
	mSpinEnrollmentFee->resetDirty();

	BOOL accept_notices = FALSE;
	BOOL list_in_profile = FALSE;
	LLGroupData data;
	if(gAgent.getGroupData(mGroupID,data))
	{
		accept_notices = data.mAcceptNotices;
		list_in_profile = data.mListInProfile;
	}
	mCtrlReceiveNotices = getChild<LLCheckBoxCtrl>("receive_notices", recurse);
	mCtrlReceiveNotices->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitUserOnly, this));
	mCtrlReceiveNotices->set(accept_notices);
	mCtrlReceiveNotices->setEnabled(data.mID.notNull());

	mCtrlReceiveGroupChat = getChild<LLCheckBoxCtrl>("receive_chat", recurse);
	if(mCtrlReceiveGroupChat)
	{
		mCtrlReceiveGroupChat->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitUserOnly, this));
		mCtrlReceiveGroupChat->setEnabled(data.mID.notNull());
		if(data.mID.notNull())
		{
			mCtrlReceiveGroupChat->set(!LLMuteList::instance().isGroupMuted(data.mID));
		}
	}
	
	mCtrlListGroup = getChild<LLCheckBoxCtrl>("list_groups_in_profile", recurse);
	mCtrlListGroup->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitUserOnly, this));
	mCtrlListGroup->set(list_in_profile);
	mCtrlListGroup->setEnabled(data.mID.notNull());
	mCtrlListGroup->resetDirty();

	mActiveTitleLabel = getChild<LLTextBox>("active_title_label", recurse);
	
	mComboActiveTitle = getChild<LLComboBox>("active_title", recurse);
	mComboActiveTitle->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny, this));

	mIncompleteMemberDataStr = getString("incomplete_member_data_str");

	// If the group_id is null, then we are creating a new group
	if (mGroupID.isNull())
	{
		mEditCharter->setEnabled(TRUE);

		mCtrlShowInGroupList->setEnabled(TRUE);
		mComboMature->setEnabled(TRUE);
		mCtrlOpenEnrollment->setEnabled(TRUE);
		mCtrlEnrollmentFee->setEnabled(TRUE);
		mSpinEnrollmentFee->setEnabled(TRUE);

	}

	return LLPanelGroupTab::postBuild();
}

void LLPanelGroupGeneral::setupCtrls(LLPanel* panel_group)
{
	mInsignia = getChild<LLTextureCtrl>("insignia");
	mInsignia->setCommitCallback(boost::bind(&LLPanelGroupGeneral::onCommitAny, this));
	mFounderName = getChild<LLTextBox>("founder_name");


	mGroupNameEditor = panel_group->getChild<LLLineEditor>("group_name_editor");
	mGroupNameEditor->setPrevalidate( LLTextValidate::validateASCIINoLeadingSpace );
}

// static
void LLPanelGroupGeneral::onFocusEdit(LLFocusableElement* ctrl, void* data)
{
	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
	self->updateChanged();
	self->notifyObservers();
}

void LLPanelGroupGeneral::onCommitAny()
{
	updateChanged();
	notifyObservers();
}

void LLPanelGroupGeneral::onCommitUserOnly()
{
	mChanged = TRUE;
	notifyObservers();
	gAgent.fireEvent(new LLOldEvents::LLEvent(&gAgent, "new group"), ""); // *HACK: for ALCH-278
}

void LLPanelGroupGeneral::onCommitEnrollment()
{
	onCommitAny();
	
	// Make sure both enrollment related widgets are there.
	if (!mCtrlEnrollmentFee || !mSpinEnrollmentFee)
	{
		return;
	}

	// Make sure the agent can change enrollment info.
	if (!gAgent.hasPowerInGroup(mGroupID,GP_MEMBER_OPTIONS)
		|| !mAllowEdit)
	{
		return;
	}

	if (mCtrlEnrollmentFee->get())
	{
		mSpinEnrollmentFee->setEnabled(TRUE);
	}
	else
	{
		mSpinEnrollmentFee->setEnabled(FALSE);
		mSpinEnrollmentFee->set(0);
	}
}

// static
void LLPanelGroupGeneral::onClickInfo(void *userdata)
{
	LLPanelGroupGeneral *self = (LLPanelGroupGeneral *)userdata;

	if ( !self ) return;

	LL_DEBUGS() << "open group info: " << self->mGroupID << LL_ENDL;

	LLGroupActions::show(self->mGroupID);

}

bool LLPanelGroupGeneral::needsApply(std::string& mesg)
{ 
	updateChanged();
	mesg = getString("group_info_unchanged");
	return mChanged || mGroupID.isNull();
}

void LLPanelGroupGeneral::activate()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (mGroupID.notNull()
		&& (!gdatap || mFirstUse))
	{
		LLGroupMgr::getInstance()->sendGroupTitlesRequest(mGroupID);
		LLGroupMgr::getInstance()->sendGroupPropertiesRequest(mGroupID);

		mFirstUse = FALSE;
	}
	mChanged = FALSE;
	
	update(GC_ALL);
}

void LLPanelGroupGeneral::draw()
{
	LLPanelGroupTab::draw();
}

bool LLPanelGroupGeneral::apply(std::string& mesg)
{
	if (!mGroupID.isNull() && mAllowEdit && mComboActiveTitle && mComboActiveTitle->isDirty())
	{
		LLGroupMgr::getInstance()->sendGroupTitleUpdate(mGroupID,mComboActiveTitle->getCurrentID());
		update(GC_TITLES);
		mComboActiveTitle->resetDirty();
	}

	BOOL has_power_in_group = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);

	if (has_power_in_group || mGroupID.isNull())
	{
		LL_INFOS() << "LLPanelGroupGeneral::apply" << LL_ENDL;

		// Check to make sure mature has been set
		if(mComboMature &&
		   mComboMature->getCurrentIndex() == DECLINE_TO_STATE)
		{
			LLNotificationsUtil::add("SetGroupMature", LLSD(), LLSD(), 
											boost::bind(&LLPanelGroupGeneral::confirmMatureApply, this, _1, _2));
			return false;
		}

		if (mGroupID.isNull())
		{
			// Validate the group name length.
			S32 group_name_len = mGroupNameEditor->getText().size();
			if ( group_name_len < DB_GROUP_NAME_MIN_LEN 
				|| group_name_len > DB_GROUP_NAME_STR_LEN)
			{
				std::ostringstream temp_error;
				temp_error << "A group name must be between " << DB_GROUP_NAME_MIN_LEN
					<< " and " << DB_GROUP_NAME_STR_LEN << " characters.";
				mesg = temp_error.str();
				return false;
			}

			LLNotificationsUtil::add("CreateGroupCost",
									 LLSD().with("COST", std::to_string(LLGlobalEconomy::getInstance()->getPriceGroupCreate())),
									 LLSD(), boost::bind(&LLPanelGroupGeneral::createGroupCallback, this, _1, _2));

			return false;
		}

		LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
		if (!gdatap)
		{
			mesg = LLTrans::getString("NoGroupDataFound");
			mesg.append(mGroupID.asString());
			return false;
		}
		bool can_change_ident = false;
		bool can_change_member_opts = false;
		can_change_ident = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);
		can_change_member_opts = gAgent.hasPowerInGroup(mGroupID,GP_MEMBER_OPTIONS);

		if (can_change_ident)
		{
			if (mEditCharter) gdatap->mCharter = mEditCharter->getText();
			if (mInsignia) gdatap->mInsigniaID = mInsignia->getImageAssetID();
			if (mComboMature)
			{
				if (!gAgent.isTeen())
				{
					gdatap->mMaturePublish = 
						mComboMature->getCurrentIndex() == MATURE_CONTENT;
				}
				else
				{
					gdatap->mMaturePublish = FALSE;
				}
			}
			if (mCtrlShowInGroupList) gdatap->mShowInList = mCtrlShowInGroupList->get();
		}

		if (can_change_member_opts)
		{
			if (mCtrlOpenEnrollment) gdatap->mOpenEnrollment = mCtrlOpenEnrollment->get();
			if (mCtrlEnrollmentFee && mSpinEnrollmentFee)
			{
				gdatap->mMembershipFee = (mCtrlEnrollmentFee->get()) ? 
					(S32) mSpinEnrollmentFee->get() : 0;
				// Set to the used value, and reset initial value used for isdirty check
				mSpinEnrollmentFee->set( (F32)gdatap->mMembershipFee );
			}
		}

		if (can_change_ident || can_change_member_opts)
		{
			LLGroupMgr::getInstance()->sendUpdateGroupInfo(mGroupID);
		}
	}

	BOOL receive_notices = false;
	BOOL list_in_profile = false;
	if (mCtrlReceiveNotices)
		receive_notices = mCtrlReceiveNotices->get();
	if (mCtrlListGroup) 
		list_in_profile = mCtrlListGroup->get();

	gAgent.setUserGroupFlags(mGroupID, receive_notices, list_in_profile);

	if(mCtrlReceiveGroupChat)
	{
		if(mCtrlReceiveGroupChat->get())
		{
			LLMuteList::instance().removeGroup(mGroupID);
		}
		else
		{
			LLMuteList::instance().addGroup(mGroupID);
		}
	}

	resetDirty();

	mChanged = FALSE;

	return true;
}

void LLPanelGroupGeneral::cancel()
{
	mChanged = FALSE;

	//cancel out all of the click changes to, although since we are
	//shifting tabs or closing the floater, this need not be done...yet
	notifyObservers();
}

// invoked from callbackConfirmMature
bool LLPanelGroupGeneral::confirmMatureApply(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	// 0 == Yes
	// 1 == No
	// 2 == Cancel
	switch(option)
	{
	case 0:
		mComboMature->setCurrentByIndex(MATURE_CONTENT);
		break;
	case 1:
		mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
		break;
	default:
		return false;
	}

	// If we got here it means they set a valid value
	std::string mesg = "";
	bool ret = apply(mesg);
	if ( !mesg.empty() )
	{
		LLSD args;
		args["MESSAGE"] = mesg;
		LLNotificationsUtil::add("GenericAlert", args);
	}

	return ret;
}

// static
bool LLPanelGroupGeneral::createGroupCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch(option)
	{
	case 0:
		{
			// Yay!  We are making a new group!
			U32 enrollment_fee = (mCtrlEnrollmentFee->get() ? 
									(U32) mSpinEnrollmentFee->get() : 0);
		
			LLGroupMgr::getInstance()->sendCreateGroupRequest(mGroupNameEditor->getText(),
												mEditCharter->getText(),
												mCtrlShowInGroupList->get(),
												mInsignia->getImageAssetID(),
												enrollment_fee,
												mCtrlOpenEnrollment->get(),
												false,
												mComboMature->getCurrentIndex() == MATURE_CONTENT);

		}
		break;
	case 1:
	default:
		break;
	}
	return false;
}

// virtual
void LLPanelGroupGeneral::update(LLGroupChange gc)
{
	if (mGroupID.isNull()) return;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	LLGroupData agent_gdatap;
	bool is_member = false;
	if (gAgent.getGroupData(mGroupID,agent_gdatap)) is_member = true;

	if (mComboActiveTitle)
	{
		mComboActiveTitle->setVisible(is_member);
		mComboActiveTitle->setEnabled(mAllowEdit);
		
		if ( mActiveTitleLabel) mActiveTitleLabel->setVisible(is_member);

		if (is_member)
		{
			LLUUID current_title_role;

			mComboActiveTitle->clear();
			mComboActiveTitle->removeall();
			bool has_selected_title = false;

			if (1 == gdatap->mTitles.size())
			{
				// Only the everyone title.  Don't bother letting them try changing this.
				mComboActiveTitle->setEnabled(FALSE);
			}
			else
			{
				mComboActiveTitle->setEnabled(TRUE);
			}

			std::vector<LLGroupTitle>::const_iterator citer = gdatap->mTitles.begin();
			std::vector<LLGroupTitle>::const_iterator end = gdatap->mTitles.end();
			
			for ( ; citer != end; ++citer)
			{
				mComboActiveTitle->add(citer->mTitle,citer->mRoleID, (citer->mSelected ? ADD_TOP : ADD_BOTTOM));
				if (citer->mSelected)
				{
					mComboActiveTitle->setCurrentByID(citer->mRoleID);
					has_selected_title = true;
				}
			}
			
			if (!has_selected_title)
			{
				mComboActiveTitle->setCurrentByID(LLUUID::null);
			}
		}

	}

	// After role member data was changed in Roles->Members
	// need to update role titles. See STORM-918.
	if (gc == GC_ROLE_MEMBER_DATA)
		LLGroupMgr::getInstance()->sendGroupTitlesRequest(mGroupID);

	// If this was just a titles update, we are done.
	if (gc == GC_TITLES) return;

	bool can_change_ident = false;
	bool can_change_member_opts = false;
	can_change_ident = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);
	can_change_member_opts = gAgent.hasPowerInGroup(mGroupID,GP_MEMBER_OPTIONS);

	if (mCtrlShowInGroupList) 
	{
		mCtrlShowInGroupList->set(gdatap->mShowInList);
		mCtrlShowInGroupList->setEnabled(mAllowEdit && can_change_ident);
	}
	if (mComboMature)
	{
		if(gdatap->mMaturePublish)
		{
			mComboMature->setCurrentByIndex(MATURE_CONTENT);
		}
		else
		{
			mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
		}
		mComboMature->setEnabled(mAllowEdit && can_change_ident);
		mComboMature->setVisible( !gAgent.isTeen() );
	}
	if (mCtrlOpenEnrollment) 
	{
		mCtrlOpenEnrollment->set(gdatap->mOpenEnrollment);
		mCtrlOpenEnrollment->setEnabled(mAllowEdit && can_change_member_opts);
	}
	if (mCtrlEnrollmentFee) 
	{	
		mCtrlEnrollmentFee->set(gdatap->mMembershipFee > 0);
		mCtrlEnrollmentFee->setEnabled(mAllowEdit && can_change_member_opts);
	}

	if (mSpinEnrollmentFee)
	{
		S32 fee = gdatap->mMembershipFee;
		mSpinEnrollmentFee->set((F32)fee);
		mSpinEnrollmentFee->setEnabled( mAllowEdit &&
						(fee > 0) &&
						can_change_member_opts);
	}
	if (mCtrlReceiveNotices)
	{
		mCtrlReceiveNotices->setVisible(is_member);
		if (is_member)
		{
			mCtrlReceiveNotices->setEnabled(mAllowEdit);
		}
	}
	if (mCtrlReceiveGroupChat)
	{
		mCtrlReceiveGroupChat->setVisible(is_member);
		if (is_member)
		{
			mCtrlReceiveGroupChat->setEnabled(mAllowEdit);
		}
	}

	if (mInsignia) mInsignia->setEnabled(mAllowEdit && can_change_ident);
	if (mEditCharter) mEditCharter->setEnabled(mAllowEdit && can_change_ident);
	
	if (mGroupNameEditor) mGroupNameEditor->setVisible(FALSE);
	if (mFounderName) mFounderName->setText(LLSLURL("agent", gdatap->mFounderID, "inspect").getSLURLString());
	if (mInsignia)
	{
		if (gdatap->mInsigniaID.notNull())
		{
			mInsignia->setImageAssetID(gdatap->mInsigniaID);
		}
		else
		{
			mInsignia->setImageAssetName(mInsignia->getDefaultImageName());
		}
	}

	if (mEditCharter)
	{
		mEditCharter->setText(gdatap->mCharter);
	}
	
	resetDirty();
}

void LLPanelGroupGeneral::updateChanged()
{
	// List all the controls we want to check for changes...
	const std::array<LLUICtrl*, 14> check_list{{
		mGroupNameEditor,
		mFounderName,
		mInsignia,
		mEditCharter,
		mCtrlShowInGroupList,
		mComboMature,
		mCtrlOpenEnrollment,
		mCtrlEnrollmentFee,
		mSpinEnrollmentFee,
		mCtrlReceiveNotices,
		mCtrlListGroup,
		mActiveTitleLabel,
		mComboActiveTitle,
        mCtrlReceiveGroupChat
	}};

	mChanged = FALSE;

	for(LLUICtrl* control : check_list)
	{
		if( control != nullptr && control->isDirty() )
		{
			mChanged = TRUE;
			break;
		}
	}
}

void LLPanelGroupGeneral::reset()
{
	mFounderName->setVisible(false);

	
	mCtrlReceiveNotices->set(false);
	
	
	mCtrlListGroup->set(true);
	
	mCtrlReceiveNotices->setEnabled(false);
	mCtrlReceiveNotices->setVisible(true);

	mCtrlListGroup->setEnabled(false);

	mGroupNameEditor->setEnabled(TRUE);
	mEditCharter->setEnabled(TRUE);

	mCtrlShowInGroupList->setEnabled(false);
	mComboMature->setEnabled(TRUE);
	
	mCtrlOpenEnrollment->setEnabled(TRUE);
	
	mCtrlEnrollmentFee->setEnabled(TRUE);
	
	mSpinEnrollmentFee->setEnabled(TRUE);
	mSpinEnrollmentFee->set((F32)0);

	mGroupNameEditor->setVisible(true);

	mComboActiveTitle->setVisible(false);

	mInsignia->setImageAssetID(LLUUID::null);
	
	mInsignia->setEnabled(true);

	mInsignia->setImageAssetName(mInsignia->getDefaultImageName());

    mCtrlReceiveGroupChat->set(false);
    mCtrlReceiveGroupChat->setEnabled(false);
    mCtrlReceiveGroupChat->setVisible(true);

    
	{
        mEditCharter->setText(LLStringUtil::null);
        mGroupNameEditor->setText(LLStringUtil::null);
	}
	
	{
		mComboMature->setEnabled(true);
		mComboMature->setVisible( !gAgent.isTeen() );
		mComboMature->selectFirstItem();
	}


	resetDirty();
}

void	LLPanelGroupGeneral::resetDirty()
{
	// List all the controls we want to check for changes...
	const std::array<LLUICtrl*, 14> check_list{{
		mGroupNameEditor,
		mFounderName,
		mInsignia,
		mEditCharter,
		mCtrlShowInGroupList,
		mComboMature,
		mCtrlOpenEnrollment,
		mCtrlEnrollmentFee,
		mSpinEnrollmentFee,
		mCtrlReceiveNotices,
		mCtrlListGroup,
		mActiveTitleLabel,
		mComboActiveTitle,
        mCtrlReceiveGroupChat
	}};

	for(LLUICtrl* control : check_list)
	{
		if( control )
			control->resetDirty() ;
	}


}

void LLPanelGroupGeneral::setGroupID(const LLUUID& id)
{
	LLPanelGroupTab::setGroupID(id);

	if(id == LLUUID::null)
	{
		reset();
		return;
	}

	BOOL accept_notices = FALSE;
	BOOL list_in_profile = FALSE;
	LLGroupData data;
	if(gAgent.getGroupData(mGroupID,data))
	{
		accept_notices = data.mAcceptNotices;
		list_in_profile = data.mListInProfile;
	}

	mCtrlReceiveNotices = getChild<LLCheckBoxCtrl>("receive_notices");
	if (mCtrlReceiveNotices)
	{
		mCtrlReceiveNotices->set(accept_notices);
		mCtrlReceiveNotices->setEnabled(data.mID.notNull());
	}
	
	mCtrlListGroup = getChild<LLCheckBoxCtrl>("list_groups_in_profile");
	if (mCtrlListGroup)
	{
		mCtrlListGroup->set(list_in_profile);
		mCtrlListGroup->setEnabled(data.mID.notNull());
	}

	mCtrlReceiveGroupChat = getChild<LLCheckBoxCtrl>("receive_chat");
	if (mCtrlReceiveGroupChat)
	{
		if(data.mID.notNull())
		{
			mCtrlReceiveGroupChat->set(!LLMuteList::instance().isGroupMuted(data.mID));
		}
		mCtrlReceiveGroupChat->setEnabled(data.mID.notNull());
	}

	mCtrlShowInGroupList->setEnabled(data.mID.notNull());

	mActiveTitleLabel = getChild<LLTextBox>("active_title_label");
	
	mComboActiveTitle = getChild<LLComboBox>("active_title");

	mFounderName->setVisible(true);

	mInsignia->setImageAssetID(LLUUID::null);

	resetDirty();

	activate();
}
