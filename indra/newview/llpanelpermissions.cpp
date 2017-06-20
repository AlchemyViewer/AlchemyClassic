// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llpanelpermissions.cpp
 * @brief LLPanelPermissions class implementation
 * This class represents the panel in the build view for
 * viewing/editing object names, owners, permissions, etc.
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

#include "llviewerprecompiledheaders.h"

#include "llpanelpermissions.h"

// library includes
#include "lluuid.h"
#include "llpermissions.h"
#include "llcategory.h"
#include "llclickaction.h"
#include "llfocusmgr.h"
#include "llnotificationsutil.h"
#include "llstring.h"

// project includes
#include "llviewerwindow.h"
#include "llresmgr.h"
#include "lltextbox.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llviewerobject.h"
#include "llselectmgr.h"
#include "llagent.h"
#include "llstatusbar.h"		// for getBalance()
#include "lllineeditor.h"
#include "llcombobox.h"
#include "lluiconstants.h"
#include "lldbstrings.h"
#include "llfloatergroups.h"
#include "llfloaterreg.h"
#include "llavataractions.h"
#include "llavatariconctrl.h"
#include "llnamebox.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llspinctrl.h"
#include "roles_constants.h"
#include "llgroupactions.h"
#include "llgroupiconctrl.h"
#include "lltrans.h"
#include "llinventorymodel.h"
#include "llmanip.h"

U8 string_value_to_click_action(const std::string& p_value);
std::string click_action_to_string_value(U8 action);

U8 string_value_to_click_action(const std::string& p_value)
{
	if(p_value == "Touch")
	{
		return CLICK_ACTION_TOUCH;
	}
	if(p_value == "Sit")
	{
		return CLICK_ACTION_SIT;
	}
	if(p_value == "Buy")
	{
		return CLICK_ACTION_BUY;
	}
	if(p_value == "Pay")
	{
		return CLICK_ACTION_PAY;
	}
	if(p_value == "Open")
	{
		return CLICK_ACTION_OPEN;
	}
	if(p_value == "Zoom")
	{
		return CLICK_ACTION_ZOOM;
	}
	LL_WARNS() << "Unknown click action" << LL_ENDL;
	return CLICK_ACTION_TOUCH;
}

std::string click_action_to_string_value(U8 action)
{
	switch (action) 
	{
		case CLICK_ACTION_TOUCH:
		default:	
			return "Touch";
			break;
		case CLICK_ACTION_SIT:
			return "Sit";
			break;
		case CLICK_ACTION_BUY:
			return "Buy";
			break;
		case CLICK_ACTION_PAY:
			return "Pay";
			break;
		case CLICK_ACTION_OPEN:
			return "Open";
			break;
		case CLICK_ACTION_ZOOM:
			return "Zoom";
			break;
	}
}

///----------------------------------------------------------------------------
/// Class llpanelpermissions
///----------------------------------------------------------------------------

// Default constructor
LLPanelPermissions::LLPanelPermissions() :
	LLPanel()
{
	setMouseOpaque(FALSE);
}

BOOL LLPanelPermissions::postBuild()
{
	mEditorObjectName = getChild<LLLineEditor>("Object Name");
	mEditorObjectName->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitName, this, _2));
	mEditorObjectName->setPrevalidate(LLTextValidate::validateASCIIPrintableNoPipe);

	mEditorObjectDesc = getChild<LLLineEditor>("Object Description");
	mEditorObjectDesc->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitDesc, this, _2));
	mEditorObjectDesc->setPrevalidate(LLTextValidate::validateASCIIPrintableNoPipe);

	mBtnPosToDesc = getChild<LLButton>("pos_to_desc");
	mBtnPosToDesc->setClickedCallback(boost::bind(&LLPanelPermissions::onClickPosToDesc, this));

	mBtnDescToPos = getChild<LLButton>("desc_to_pos"); 
	mBtnDescToPos->setClickedCallback(boost::bind(&LLPanelPermissions::onClickDescToPos, this));

	mBtnSetGroup = getChild<LLButton>("button set group");
	mBtnSetGroup->setCommitCallback(boost::bind(&LLPanelPermissions::onClickGroup, this));

	mCheckShareWithGroup = getChild<LLCheckBoxCtrl>("checkbox share with group");
	mCheckShareWithGroup->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitGroupShare, this, _2));

	mBtnDeedGroup = getChild<LLButton>("button deed");
	mBtnDeedGroup->setClickedCallback(boost::bind(&LLPanelPermissions::onClickDeedToGroup, this));

	mCheckAllowEveryoneMove = getChild<LLCheckBoxCtrl>("checkbox allow everyone move");
	mCheckAllowEveryoneMove->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitEveryoneMove, this, _2));

	mCheckAllowEveryoneCopy = getChild<LLCheckBoxCtrl>("checkbox allow everyone copy");
	mCheckAllowEveryoneCopy->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitEveryoneCopy, this, _2));

	mCheckAllowExport = getChild<LLCheckBoxCtrl>("checkbox allow export");
	mCheckAllowExport->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitExport, this, _2));
	
	mCheckForSale = getChild<LLCheckBoxCtrl>("checkbox for sale");
	mCheckForSale->setCommitCallback(boost::bind(&LLPanelPermissions::setAllSaleInfo, this));

	mComboSaleType = getChild<LLComboBox>("sale type");
	mComboSaleType->setCommitCallback(boost::bind(&LLPanelPermissions::setAllSaleInfo, this));

	mSpinnerEditCost = getChild<LLSpinCtrl>("Edit Cost");
	mSpinnerEditCost->setCommitCallback(boost::bind(&LLPanelPermissions::setAllSaleInfo, this));
	
	mCheckNextOwnerModify = getChild<LLCheckBoxCtrl>("checkbox next owner can modify");
	mCheckNextOwnerModify->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitNextOwnerModify, this, _2));

	mCheckNextOwnerCopy = getChild<LLCheckBoxCtrl>("checkbox next owner can copy");
	mCheckNextOwnerCopy->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitNextOwnerCopy, this, _2));

	mCheckNextOwnerTransfer = getChild<LLCheckBoxCtrl>("checkbox next owner can transfer");
	mCheckNextOwnerTransfer->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitNextOwnerTransfer, this, _2));

	mComboClickAction = getChild<LLComboBox>("clickaction");
	mComboClickAction->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitClickAction, this, _2));

	mCheckShowInSearch = getChild<LLCheckBoxCtrl>("search_check");
	mCheckShowInSearch->setCommitCallback(boost::bind(&LLPanelPermissions::onCommitIncludeInSearch, this, _2));
	
	mLabelAdvPermB = getChild<LLTextBox>("B:");
	mLabelAdvPermO = getChild<LLTextBox>("O:");
	mLabelAdvPermG = getChild<LLTextBox>("G:");
	mLabelAdvPermE = getChild<LLTextBox>("E:");
	mLabelAdvPermN = getChild<LLTextBox>("N:");
	mLabelAdvPermF = getChild<LLTextBox>("F:");
	mLabelClickAction = getChild<LLTextBox>("label click action");
	mLabelCreator = getChild<LLTextBox>("Creator:");
	mLabelCreatorName = getChild<LLTextBox>("Creator Name");
	mLabelGroup = getChild<LLTextBox>("Group:");
	mLabelGroupName = getChild<LLNameBox>("Group Name Proxy");
	mLabelLastOwner = getChild<LLTextBox>("Last Owner:");
	mLabelLastOwnerName = getChild<LLTextBox>("Last Owner Name");
	mLabelNextOwnerCan = getChild<LLTextBox>("Next owner can:");
	mLabelObjectDesc = getChild<LLTextBox>("Description:");
	mLabelObjectName = getChild<LLTextBox>("Name:");
	mLabelOwner = getChild<LLTextBox>("Owner:");
	mLabelOwnerName = getChild<LLTextBox>("Owner Name");
	mLabelPermModify = getChild<LLTextBox>("perm_modify");
	mLabelPathFindingAttribs = getChild<LLTextBox>("pathfinding_attributes_value");

	return TRUE;
}


LLPanelPermissions::~LLPanelPermissions()
{
	// base class will take care of everything
}


void LLPanelPermissions::disableAll()
{
	mLabelPermModify->setEnabled(FALSE);
	mLabelPermModify->setValue(LLStringUtil::null);

	mLabelPathFindingAttribs->setEnabled(FALSE);
	mLabelPathFindingAttribs->setValue(LLStringUtil::null);

	mLabelCreator->setEnabled(FALSE);
	mLabelCreatorName->setValue(LLStringUtil::null);
	mLabelCreatorName->setEnabled(FALSE);

	mLabelOwner->setEnabled(FALSE);
	mLabelOwnerName->setValue(LLStringUtil::null);
	mLabelOwnerName->setEnabled(FALSE);

	mLabelLastOwner->setEnabled(FALSE);
	mLabelLastOwnerName->setValue(LLStringUtil::null);
	mLabelLastOwnerName->setEnabled(FALSE);

	mLabelGroup->setEnabled(FALSE);
	mLabelGroupName->setValue(LLStringUtil::null);
	mLabelGroupName->setEnabled(FALSE);
	mBtnSetGroup->setEnabled(FALSE);

	mEditorObjectName->setValue(LLStringUtil::null);
	mEditorObjectName->setEnabled(FALSE);
	mLabelObjectName->setEnabled(FALSE);
	mLabelObjectDesc->setEnabled(FALSE);
	mEditorObjectDesc->setValue(LLStringUtil::null);
	mEditorObjectDesc->setEnabled(FALSE);
	mBtnPosToDesc->setEnabled(FALSE); // <alchemy/>
	mBtnDescToPos->setEnabled(FALSE); // <alchemy/>

	mCheckShareWithGroup->setValue(FALSE);
	mCheckShareWithGroup->setEnabled(FALSE);
	mBtnDeedGroup->setEnabled(FALSE);

	mCheckAllowEveryoneMove->setValue(FALSE);
	mCheckAllowEveryoneMove->setEnabled(FALSE);
	mCheckAllowEveryoneCopy->setValue(FALSE);
	mCheckAllowEveryoneCopy->setEnabled(FALSE);

	mCheckAllowExport->setVisible(!LLGridManager::getInstance()->isInSecondlife());
	mCheckAllowExport->setValue(FALSE);
	mCheckAllowExport->setEnabled(FALSE);
	
	//Next owner can:
	mLabelNextOwnerCan->setEnabled(FALSE);
	mCheckNextOwnerModify->setValue(FALSE);
	mCheckNextOwnerModify->setEnabled(FALSE);
	mCheckNextOwnerCopy->setValue(FALSE);
	mCheckNextOwnerCopy->setEnabled(FALSE);
	mCheckNextOwnerTransfer->setValue(FALSE);
	mCheckNextOwnerTransfer->setEnabled(FALSE);

	//checkbox for sale
	mCheckForSale->setValue(FALSE);
	mCheckForSale->setEnabled(FALSE);

	//checkbox include in search
	mCheckShowInSearch->setValue(FALSE);
	mCheckShowInSearch->setEnabled(FALSE);
		
	mComboSaleType->setValue(LLSaleInfo::FS_COPY);
	mComboSaleType->setEnabled(FALSE);
		
	mSpinnerEditCost->setValue(LLStringUtil::null);
	mSpinnerEditCost->setEnabled(FALSE);
		
	mLabelClickAction->setEnabled(FALSE);

	mComboClickAction->setEnabled(FALSE);
	mComboClickAction->clear();

	mLabelAdvPermB->setVisible(FALSE);
	mLabelAdvPermO->setVisible(FALSE);
	mLabelAdvPermG->setVisible(FALSE);
	mLabelAdvPermE->setVisible(FALSE);
	mLabelAdvPermN->setVisible(FALSE);
	mLabelAdvPermF->setVisible(FALSE);
}

void LLPanelPermissions::refresh()
{
	std::string deedText;
	if (gWarningSettings.getBOOL("DeedObject"))
	{
		deedText = getString("text deed continued");
	}
	else
	{
		deedText = getString("text deed");
	}
	mBtnDeedGroup->setLabelSelected(deedText);
	mBtnDeedGroup->setLabelUnselected(deedText);

	BOOL root_selected = TRUE;
	LLSelectNode* nodep = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode();
	S32 object_count = LLSelectMgr::getInstance()->getSelection()->getRootObjectCount();
	if(!nodep || 0 == object_count)
	{
		nodep = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
		object_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
		root_selected = FALSE;
	}

	//BOOL attachment_selected = LLSelectMgr::getInstance()->getSelection()->isAttachment();
	//attachment_selected = false;
	LLViewerObject* objectp = nullptr;
	if(nodep) objectp = nodep->getObject();
	if(!nodep || !objectp)// || attachment_selected)
	{
		// ...nothing selected
		disableAll();
		return;
	}

	// figure out a few variables
	const BOOL is_one_object = (object_count == 1);
	
	// BUG: fails if a root and non-root are both single-selected.
	BOOL is_perm_modify = (LLSelectMgr::getInstance()->getSelection()->getFirstRootNode() 
						   && LLSelectMgr::getInstance()->selectGetRootsModify())
		|| LLSelectMgr::getInstance()->selectGetModify();
	BOOL is_nonpermanent_enforced = (LLSelectMgr::getInstance()->getSelection()->getFirstRootNode() 
						   && LLSelectMgr::getInstance()->selectGetRootsNonPermanentEnforced())
		|| LLSelectMgr::getInstance()->selectGetNonPermanentEnforced();
	const LLFocusableElement* keyboard_focus_view = gFocusMgr.getKeyboardFocus();

	S32 string_index = 0;
	std::string MODIFY_INFO_STRINGS[] =
		{
			getString("text modify info 1"),
			getString("text modify info 2"),
			getString("text modify info 3"),
			getString("text modify info 4"),
			getString("text modify info 5"),
			getString("text modify info 6")
		};
	if (!is_perm_modify)
	{
		string_index += 2;
	}
	else if (!is_nonpermanent_enforced)
	{
		string_index += 4;
	}
	if (!is_one_object)
	{
		++string_index;
	}
	mLabelPermModify->setEnabled(TRUE);
	mLabelPermModify->setValue(MODIFY_INFO_STRINGS[string_index]);

	std::string pfAttrName;

	if ((LLSelectMgr::getInstance()->getSelection()->getFirstRootNode() 
		&& LLSelectMgr::getInstance()->selectGetRootsNonPathfinding())
		|| LLSelectMgr::getInstance()->selectGetNonPathfinding())
	{
		pfAttrName = "Pathfinding_Object_Attr_None";
	}
	else if ((LLSelectMgr::getInstance()->getSelection()->getFirstRootNode() 
		&& LLSelectMgr::getInstance()->selectGetRootsPermanent())
		|| LLSelectMgr::getInstance()->selectGetPermanent())
	{
		pfAttrName = "Pathfinding_Object_Attr_Permanent";
	}
	else if ((LLSelectMgr::getInstance()->getSelection()->getFirstRootNode() 
		&& LLSelectMgr::getInstance()->selectGetRootsCharacter())
		|| LLSelectMgr::getInstance()->selectGetCharacter())
	{
		pfAttrName = "Pathfinding_Object_Attr_Character";
	}
	else
	{
		pfAttrName = "Pathfinding_Object_Attr_MultiSelect";
	}

	mLabelPathFindingAttribs->setEnabled(TRUE);
	mLabelPathFindingAttribs->setValue(LLTrans::getString(pfAttrName));

	// Update creator text field
	mLabelCreator->setEnabled(TRUE);
	std::string creator_name;
	LLSelectMgr::getInstance()->selectGetCreator(mCreatorID, creator_name);

	mLabelCreatorName->setValue(creator_name);
	mLabelCreatorName->setEnabled(TRUE);

	// Update owner text field
	mLabelOwner->setEnabled(TRUE);
	std::string owner_name;
	const BOOL owners_identical = LLSelectMgr::getInstance()->selectGetOwner(mOwnerID, owner_name);

	mLabelOwnerName->setValue(owner_name);
	mLabelOwnerName->setEnabled(TRUE);

	// Display last owner if public
	mLabelLastOwner->setEnabled(TRUE);
	std::string last_owner_name;
	LLSelectMgr::getInstance()->selectGetLastOwner(mLastOwnerID, last_owner_name);

	mLabelLastOwnerName->setValue(last_owner_name);
	mLabelLastOwnerName->setEnabled(TRUE);

	// update group text field
	mLabelGroup->setEnabled(TRUE);
	LLUUID group_id;
	BOOL groups_identical = LLSelectMgr::getInstance()->selectGetGroup(group_id);
	if (groups_identical)
	{
		if (mLabelGroupName)
		{
			mLabelGroupName->setNameID(group_id,TRUE);
			mLabelGroupName->setEnabled(TRUE);
		}
	}
	else
	{
		if (mLabelGroupName)
		{
			mLabelGroupName->setNameID(LLUUID::null, TRUE);
			mLabelGroupName->refresh(LLUUID::null, std::string(), true);
			mLabelGroupName->setEnabled(FALSE);
		}
	}
	
	mBtnSetGroup->setEnabled(root_selected && owners_identical && (mOwnerID == gAgent.getID()) && is_nonpermanent_enforced);

	mLabelObjectName->setEnabled(TRUE);
	mLabelObjectDesc->setEnabled(TRUE);

	// <alchemy>
	//if (is_one_object)
	//{
	const std::string multi_select_string = LLTrans::getString("BuildMultiSelect");
	if (keyboard_focus_view != mEditorObjectName)
	{
		mEditorObjectName->setValue(is_one_object ? nodep->mName : multi_select_string);
	}

	if (keyboard_focus_view != mEditorObjectDesc)
	{
		mEditorObjectDesc->setText(is_one_object ? nodep->mDescription : multi_select_string);
	}

	//}
	//else
	//{
	//	mEditorObjectName->setValue(LLStringUtil::null);
	//	mEditorObjectDesc->setText(LLStringUtil::null);
	//}
	// </alchemy>

	// figure out the contents of the name, description, & category
	BOOL edit_name_desc = FALSE;
	if (/*is_one_object &&*/ objectp->permModify() && !objectp->isPermanentEnforced()) // <alchemy/>
	{
		edit_name_desc = TRUE;
	}
	if (edit_name_desc)
	{
		mEditorObjectName->setEnabled(TRUE);
		mEditorObjectDesc->setEnabled(TRUE);
		// <alchemy>
		const BOOL is_attached = LLSelectMgr::getInstance()->getSelection()->isAttachment();
		mBtnPosToDesc->setEnabled(root_selected && is_one_object && !is_attached);
		mBtnDescToPos->setEnabled(root_selected && is_one_object && !is_attached);
		// </alchemy>
	}
	else
	{
		mEditorObjectName->setEnabled(FALSE);
		mEditorObjectDesc->setEnabled(FALSE);
		mBtnPosToDesc->setEnabled(FALSE); // <alchemy/>
		mBtnDescToPos->setEnabled(FALSE); // <alchemy/>
	}

	S32 total_sale_price = 0;
	S32 individual_sale_price = 0;
	BOOL is_for_sale_mixed = FALSE;
	BOOL is_sale_price_mixed = FALSE;
	U32 num_for_sale = FALSE;
    LLSelectMgr::getInstance()->selectGetAggregateSaleInfo(num_for_sale,
														   is_for_sale_mixed,
														   is_sale_price_mixed,
														   total_sale_price,
														   individual_sale_price);

	const BOOL self_owned = (gAgent.getID() == mOwnerID);
	const BOOL group_owned = LLSelectMgr::getInstance()->selectIsGroupOwned() ;
	const BOOL public_owned = (mOwnerID.isNull() && !LLSelectMgr::getInstance()->selectIsGroupOwned());
	const BOOL can_transfer = LLSelectMgr::getInstance()->selectGetRootsTransfer();
	const BOOL can_copy = LLSelectMgr::getInstance()->selectGetRootsCopy();

	if (!owners_identical)
	{
		mSpinnerEditCost->setValue(LLStringUtil::null);
		mSpinnerEditCost->setEnabled(FALSE);
	}
	// You own these objects.
	else if (self_owned || (group_owned && gAgent.hasPowerInGroup(group_id,GP_OBJECT_SET_SALE)))
	{
		if (!mSpinnerEditCost->hasFocus())
		{
			// If the sale price is mixed then set the cost to MIXED, otherwise
			// set to the actual cost.
			if ((num_for_sale > 0) && is_for_sale_mixed)
			{
				mSpinnerEditCost->setTentative(TRUE);
			}
			else if ((num_for_sale > 0) && is_sale_price_mixed)
			{
				mSpinnerEditCost->setTentative(TRUE);
			}
			else 
			{
				mSpinnerEditCost->setValue(individual_sale_price);
			}
		}
		// The edit fields are only enabled if you can sell this object
		// and the sale price is not mixed.
		BOOL enable_edit = (num_for_sale && can_transfer) ? !is_for_sale_mixed : FALSE;
		mSpinnerEditCost->setEnabled(enable_edit);
	}
	// Someone, not you, owns these objects.
	else if (!public_owned)
	{
		mSpinnerEditCost->setEnabled(FALSE);
		
		// Don't show a price if none of the items are for sale.
		if (num_for_sale)
		{
			mSpinnerEditCost->setValue(llformat("%d", total_sale_price));
		}
		else
		{
			mSpinnerEditCost->setValue(LLStringUtil::null);
		}
	}
	// This is a public object.
	else
	{
		mSpinnerEditCost->setValue(LLStringUtil::null);
		mSpinnerEditCost->setEnabled(FALSE);
	}

	// Enable and disable the permissions checkboxes
	// based on who owns the object.
	// TODO: Creator permissions

	U32 base_mask_on 			= 0;
	U32 base_mask_off		 	= 0;
	U32 owner_mask_off			= 0;
	U32 owner_mask_on 			= 0;
	U32 group_mask_on 			= 0;
	U32 group_mask_off 			= 0;
	U32 everyone_mask_on 		= 0;
	U32 everyone_mask_off 		= 0;
	U32 next_owner_mask_on 		= 0;
	U32 next_owner_mask_off		= 0;

	BOOL valid_base_perms 		= LLSelectMgr::getInstance()->selectGetPerm(PERM_BASE,
																			&base_mask_on,
																			&base_mask_off);
	//BOOL valid_owner_perms =//
	LLSelectMgr::getInstance()->selectGetPerm(PERM_OWNER,
											  &owner_mask_on,
											  &owner_mask_off);
	BOOL valid_group_perms 		= LLSelectMgr::getInstance()->selectGetPerm(PERM_GROUP,
																			&group_mask_on,
																			&group_mask_off);
	
	BOOL valid_everyone_perms 	= LLSelectMgr::getInstance()->selectGetPerm(PERM_EVERYONE,
																			&everyone_mask_on,
																			&everyone_mask_off);
	
	BOOL valid_next_perms 		= LLSelectMgr::getInstance()->selectGetPerm(PERM_NEXT_OWNER,
																			&next_owner_mask_on,
																			&next_owner_mask_off);


	if (gSavedSettings.getBOOL("DebugPermissions") )
	{
		if (valid_base_perms)
		{
			mLabelAdvPermB->setValue("B: " + mask_to_string(base_mask_on));
			mLabelAdvPermB->setVisible(TRUE);
			mLabelAdvPermO->setValue("O: " + mask_to_string(owner_mask_on));
			mLabelAdvPermO->setVisible(TRUE);
			mLabelAdvPermG->setValue("G: " + mask_to_string(group_mask_on));
			mLabelAdvPermG->setVisible(TRUE);
			mLabelAdvPermE->setValue("E: " + mask_to_string(everyone_mask_on));
			mLabelAdvPermE->setVisible(TRUE);
			mLabelAdvPermN->setValue("N: " + mask_to_string(next_owner_mask_on));
			mLabelAdvPermN->setVisible(TRUE);
		}
		else if(!root_selected)
		{
			if(object_count == 1)
			{
				LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
				if (node && node->mValid)
				{
					mLabelAdvPermB->setValue("B: " + mask_to_string( node->mPermissions->getMaskBase()));
					mLabelAdvPermB->setVisible(TRUE);
					mLabelAdvPermO->setValue("O: " + mask_to_string(node->mPermissions->getMaskOwner()));
					mLabelAdvPermO->setVisible(TRUE);
					mLabelAdvPermG->setValue("G: " + mask_to_string(node->mPermissions->getMaskGroup()));
					mLabelAdvPermG->setVisible(TRUE);
					mLabelAdvPermE->setValue("E: " + mask_to_string(node->mPermissions->getMaskEveryone()));
					mLabelAdvPermE->setVisible(TRUE);
					mLabelAdvPermN->setValue("N: " + mask_to_string(node->mPermissions->getMaskNextOwner()));
					mLabelAdvPermN->setVisible(TRUE);
				}
			}
		}
		else
		{
			mLabelAdvPermB->setVisible(FALSE);
			mLabelAdvPermO->setVisible(FALSE);
			mLabelAdvPermG->setVisible(FALSE);
			mLabelAdvPermE->setVisible(FALSE);
			mLabelAdvPermN->setVisible(FALSE);
		}

		U32 flag_mask = 0x0;
		if (objectp->permMove()) 		flag_mask |= PERM_MOVE;
		if (objectp->permModify()) 		flag_mask |= PERM_MODIFY;
		if (objectp->permCopy()) 		flag_mask |= PERM_COPY;
		if (objectp->permTransfer()) 	flag_mask |= PERM_TRANSFER;
		//if (objectp->permExport())		flag_mask |= PERM_EXPORT;

		mLabelAdvPermF->setValue("F:" + mask_to_string(flag_mask));
		mLabelAdvPermF->setVisible(TRUE);
	}
	else
	{
		mLabelAdvPermB->setVisible(FALSE);
		mLabelAdvPermO->setVisible(FALSE);
		mLabelAdvPermG->setVisible(FALSE);
		mLabelAdvPermE->setVisible(FALSE);
		mLabelAdvPermN->setVisible(FALSE);
		mLabelAdvPermF->setVisible(FALSE);
	}

	BOOL has_change_perm_ability = FALSE;
	BOOL has_change_sale_ability = FALSE;

	if (valid_base_perms && is_nonpermanent_enforced &&
		(self_owned || (group_owned && gAgent.hasPowerInGroup(group_id, GP_OBJECT_MANIPULATE))))
	{
		has_change_perm_ability = TRUE;
	}
	if (valid_base_perms && is_nonpermanent_enforced &&
	   (self_owned || (group_owned && gAgent.hasPowerInGroup(group_id, GP_OBJECT_SET_SALE))))
	{
		has_change_sale_ability = TRUE;
	}

	if (!has_change_perm_ability && !has_change_sale_ability && !root_selected)
	{
		// ...must select root to choose permissions
		mLabelPermModify->setValue(getString("text modify warning"));
	}

	if (has_change_perm_ability)
	{
		mCheckShareWithGroup->setEnabled(TRUE);
		mCheckAllowEveryoneMove->setEnabled(owner_mask_on & PERM_MOVE);
		mCheckAllowEveryoneCopy->setEnabled(owner_mask_on & PERM_COPY && owner_mask_on & PERM_TRANSFER);
	}
	else
	{
		mCheckShareWithGroup->setEnabled(FALSE);
		mCheckAllowEveryoneMove->setEnabled(FALSE);
		mCheckAllowEveryoneCopy->setEnabled(FALSE);
	}
	
	mCheckAllowExport->setEnabled(gAgentID == mCreatorID);

	if (has_change_sale_ability && (owner_mask_on & PERM_TRANSFER))
	{
		mCheckForSale->setEnabled(can_transfer || num_for_sale);
		// Set the checkbox to tentative if the prices of each object selected
		// are not the same.
		mCheckForSale->setTentative(is_for_sale_mixed);
		mComboSaleType->setEnabled(num_for_sale && can_transfer && !is_sale_price_mixed);

		mLabelNextOwnerCan->setEnabled(TRUE);
		mCheckNextOwnerModify->setEnabled(base_mask_on & PERM_MODIFY);
		mCheckNextOwnerCopy->setEnabled(base_mask_on & PERM_COPY);
		mCheckNextOwnerTransfer->setEnabled(next_owner_mask_on & PERM_COPY);
	}
	else 
	{
		mCheckForSale->setEnabled(FALSE);
		mComboSaleType->setEnabled(FALSE);

		mLabelNextOwnerCan->setEnabled(FALSE);
		mCheckNextOwnerModify->setEnabled(FALSE);
		mCheckNextOwnerCopy->setEnabled(FALSE);
		mCheckNextOwnerTransfer->setEnabled(FALSE);
	}

	if (valid_group_perms)
	{
		if ((group_mask_on & PERM_COPY) && (group_mask_on & PERM_MODIFY) && (group_mask_on & PERM_MOVE))
		{
			mCheckShareWithGroup->setValue(TRUE);
			mCheckShareWithGroup->setTentative(FALSE);
			mBtnDeedGroup->setEnabled(gAgent.hasPowerInGroup(group_id, GP_OBJECT_DEED) && (owner_mask_on & PERM_TRANSFER) && !group_owned && can_transfer);
		}
		else if ((group_mask_off & PERM_COPY) && (group_mask_off & PERM_MODIFY) && (group_mask_off & PERM_MOVE))
		{
			mCheckShareWithGroup->setValue(FALSE);
			mCheckShareWithGroup->setTentative(FALSE);
			mBtnDeedGroup->setEnabled(FALSE);
		}
		else
		{
			mCheckShareWithGroup->setValue(TRUE);
			mCheckShareWithGroup->setTentative(TRUE);
			mBtnDeedGroup->setEnabled(gAgent.hasPowerInGroup(group_id, GP_OBJECT_DEED) && (group_mask_on & PERM_MOVE) && (owner_mask_on & PERM_TRANSFER) && !group_owned && can_transfer);
		}
	}			

	if (valid_everyone_perms)
	{
		// Move
		if (everyone_mask_on & PERM_MOVE)
		{
			mCheckAllowEveryoneMove->setValue(TRUE);
			mCheckAllowEveryoneMove->setTentative(FALSE);
		}
		else if (everyone_mask_off & PERM_MOVE)
		{
			mCheckAllowEveryoneMove->setValue(FALSE);
			mCheckAllowEveryoneMove->setTentative(FALSE);
		}
		else
		{
			mCheckAllowEveryoneMove->setValue(TRUE);
			mCheckAllowEveryoneMove->setTentative(TRUE);
		}

		// Copy == everyone can't copy
		if (everyone_mask_on & PERM_COPY)
		{
			mCheckAllowEveryoneCopy->setValue(TRUE);
			mCheckAllowEveryoneCopy->setTentative(!can_copy || !can_transfer);
		}
		else if (everyone_mask_off & PERM_COPY)
		{
			mCheckAllowEveryoneCopy->setValue(FALSE);
			mCheckAllowEveryoneCopy->setTentative(FALSE);
		}
		else
		{
			mCheckAllowEveryoneCopy->setValue(TRUE);
			mCheckAllowEveryoneCopy->setTentative(TRUE);
		}
		
		// Export
		if (everyone_mask_on & PERM_EXPORT)
		{
			mCheckAllowExport->setValue(TRUE);
			mCheckAllowExport->setTentative(FALSE);
		}
		else if (everyone_mask_off & PERM_COPY)
		{
			mCheckAllowExport->setValue(FALSE);
			mCheckAllowExport->setTentative(FALSE);
		}
		else
		{
			mCheckAllowExport->setValue(TRUE);
			mCheckAllowExport->setTentative(TRUE);
		}
	}

	if (valid_next_perms)
	{
		// Modify == next owner canot modify
		if (next_owner_mask_on & PERM_MODIFY)
		{
			mCheckNextOwnerModify->setValue(TRUE);
			mCheckNextOwnerModify->setTentative(FALSE);
		}
		else if (next_owner_mask_off & PERM_MODIFY)
		{
			mCheckNextOwnerModify->setValue(FALSE);
			mCheckNextOwnerModify->setTentative(FALSE);
		}
		else
		{
			mCheckNextOwnerModify->setValue(TRUE);
			mCheckNextOwnerModify->setTentative(TRUE);
		}

		// Copy == next owner cannot copy
		if (next_owner_mask_on & PERM_COPY)
		{			
			mCheckNextOwnerCopy->setValue(TRUE);
			mCheckNextOwnerCopy->setTentative(!can_copy);
		}
		else if (next_owner_mask_off & PERM_COPY)
		{
			mCheckNextOwnerCopy->setValue(FALSE);
			mCheckNextOwnerCopy->setTentative(FALSE);
		}
		else
		{
			mCheckNextOwnerCopy->setValue(TRUE);
			mCheckNextOwnerCopy->setTentative(TRUE);
		}

		// Transfer == next owner cannot transfer
		if (next_owner_mask_on & PERM_TRANSFER)
		{
			mCheckNextOwnerTransfer->setValue(TRUE);
			mCheckNextOwnerTransfer->setTentative(!can_transfer);
		}
		else if (next_owner_mask_off & PERM_TRANSFER)
		{
			mCheckNextOwnerTransfer->setValue(FALSE);
			mCheckNextOwnerTransfer->setTentative(FALSE);
		}
		else
		{
			mCheckNextOwnerTransfer->setValue(TRUE);
			mCheckNextOwnerTransfer->setTentative(TRUE);
		}
	}

	// reflect sale information
	LLSaleInfo sale_info;
	BOOL valid_sale_info = LLSelectMgr::getInstance()->selectGetSaleInfo(sale_info);
	LLSaleInfo::EForSale sale_type = sale_info.getSaleType();

	if (valid_sale_info)
	{
		mComboSaleType->setValue(					sale_type == LLSaleInfo::FS_NOT ? LLSaleInfo::FS_COPY : sale_type);
		mComboSaleType->setTentative(				FALSE); // unfortunately this doesn't do anything at the moment.
	}
	else
	{
		// default option is sell copy, determined to be safest
		mComboSaleType->setValue(					LLSaleInfo::FS_COPY);
		mComboSaleType->setTentative(				TRUE); // unfortunately this doesn't do anything at the moment.
	}

	mCheckForSale->setValue((num_for_sale != 0));

	// HACK: There are some old objects in world that are set for sale,
	// but are no-transfer.  We need to let users turn for-sale off, but only
	// if for-sale is set.
	bool cannot_actually_sell = !can_transfer || (!can_copy && sale_type == LLSaleInfo::FS_COPY);
	if (cannot_actually_sell)
	{
		if (num_for_sale && has_change_sale_ability)
		{
			mCheckForSale->setEnabled(true);
		}
	}
	
	// Check search status of objects
	const BOOL all_volume = LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME );
	bool include_in_search;
	const BOOL all_include_in_search = LLSelectMgr::getInstance()->selectionGetIncludeInSearch(&include_in_search);
	mCheckShowInSearch->setEnabled(has_change_sale_ability && all_volume);
	mCheckShowInSearch->setValue(include_in_search);
	mCheckShowInSearch->setTentative(!all_include_in_search);

	// Click action (touch, sit, buy)
	U8 click_action = 0;
	if (LLSelectMgr::getInstance()->selectionGetClickAction(&click_action))
	{
		const std::string combo_value = click_action_to_string_value(click_action);
		mComboClickAction->setValue(LLSD(combo_value));
	}

	if(LLSelectMgr::getInstance()->getSelection()->isAttachment())
	{
		mCheckForSale->setEnabled(FALSE);
		mSpinnerEditCost->setEnabled(FALSE);
		mComboSaleType->setEnabled(FALSE);
	}

	mLabelClickAction->setEnabled(is_perm_modify && is_nonpermanent_enforced  && all_volume);
	mComboClickAction->setEnabled(is_perm_modify && is_nonpermanent_enforced && all_volume);
}

void LLPanelPermissions::onClickClaim()
{
	// try to claim ownership
	LLSelectMgr::getInstance()->sendOwner(gAgent.getID(), gAgent.getGroupID());
}

void LLPanelPermissions::onClickRelease()
{
	// try to release ownership
	LLSelectMgr::getInstance()->sendOwner(LLUUID::null, LLUUID::null);
}

void LLPanelPermissions::onClickGroup()
{
	LLUUID owner_id;
	std::string name;
	BOOL owners_identical = LLSelectMgr::getInstance()->selectGetOwner(owner_id, name);
	LLFloater* parent_floater = gFloaterView->getParentFloater(this);

	if(owners_identical && (owner_id == gAgent.getID()))
	{
		LLFloaterGroupPicker* fg = 	LLFloaterReg::showTypedInstance<LLFloaterGroupPicker>("group_picker", LLSD(gAgent.getID()));
		if (fg)
		{
			fg->setSelectGroupCallback( boost::bind(&LLPanelPermissions::cbGroupID, this, _1) );

			if (parent_floater)
			{
				LLRect new_rect = gFloaterView->findNeighboringPosition(parent_floater, fg);
				fg->setOrigin(new_rect.mLeft, new_rect.mBottom);
				parent_floater->addDependentFloater(fg);
			}
		}
	}
}

void LLPanelPermissions::cbGroupID(LLUUID group_id)
{
	if(mLabelGroupName)
	{
		mLabelGroupName->setNameID(group_id, TRUE);
	}
	LLSelectMgr::getInstance()->sendGroup(group_id);
}

bool callback_deed_to_group(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == option)
	{
		LLUUID group_id;
		BOOL groups_identical = LLSelectMgr::getInstance()->selectGetGroup(group_id);
		if(group_id.notNull() && groups_identical && (gAgent.hasPowerInGroup(group_id, GP_OBJECT_DEED)))
		{
			LLSelectMgr::getInstance()->sendOwner(LLUUID::null, group_id, FALSE);
		}
	}
	return false;
}

void LLPanelPermissions::onClickDeedToGroup()
{
	LLNotificationsUtil::add("DeedObjectToGroup", LLSD(), LLSD(), callback_deed_to_group);
}

// <alchemy>
void LLPanelPermissions::onClickDescToPos()
{
	if (LLSelectNode* nodep = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode())
	{
		if (LLViewerObject* objectp = nodep->getObject())
		{
			std::string desc = nodep->mDescription;
			if (desc.empty()) return;

			LLVector3 target(0.f);
			S32 count = sscanf(desc.c_str(), "<%f, %f, %f>", &target.mV[VX], &target.mV[VY], &target.mV[VZ]);
			if (count != 3 || target.isNull()) return;

			target.mV[VX] = llclamp(target.mV[VX], 0.f, 256.f);
			target.mV[VY] = llclamp(target.mV[VY], 0.f, 256.f);
			target.mV[VZ] = llclamp(target.mV[VZ], 0.f, 4096.f);

			objectp->setPositionEdit(target);
			LLManip::rebuild(objectp);
			LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_POSITION);
			LLSelectMgr::getInstance()->updateSelectionCenter();
		}
	}
}

void LLPanelPermissions::onClickPosToDesc()
{
	if (LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject())
	{
		const LLVector3& vec = objectp->getPosition();
		std::string pos = llformat("<%.3f,%.3f,%.3f>", vec.mV[VX], vec.mV[VY], vec.mV[VZ]);
		mEditorObjectDesc->setText(pos);
		LLSelectMgr::getInstance()->selectionSetObjectDescription(pos);
	}
}
// </alchemy>

///----------------------------------------------------------------------------
/// Permissions checkboxes
///----------------------------------------------------------------------------

void LLPanelPermissions::onCommitPerm(BOOL new_state, U8 field, U32 perm)
{
	LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
	if(!object) return;

	LLSelectMgr::getInstance()->selectionSetObjectPermissions(field, new_state, perm);
}

void LLPanelPermissions::onCommitGroupShare(const LLSD& user_data)
{
	auto new_state = user_data.asBoolean();
	onCommitPerm(new_state, PERM_GROUP, PERM_MODIFY | PERM_MOVE | PERM_COPY);
}

void LLPanelPermissions::onCommitEveryoneMove(const LLSD& user_data)
{
	auto new_state = user_data.asBoolean();
	onCommitPerm(new_state, PERM_EVERYONE, PERM_MOVE);
}

void LLPanelPermissions::onCommitEveryoneCopy(const LLSD& user_data)
{
	auto new_state = user_data.asBoolean();
	onCommitPerm(new_state, PERM_EVERYONE, PERM_COPY);
}

void LLPanelPermissions::onCommitNextOwnerModify(const LLSD& user_data)
{
	//LL_INFOS() << "LLPanelPermissions::onCommitNextOwnerModify" << LL_ENDL;
	auto new_state = user_data.asBoolean();
	onCommitPerm(new_state, PERM_NEXT_OWNER, PERM_MODIFY);
}

void LLPanelPermissions::onCommitNextOwnerCopy(const LLSD& user_data)
{
	//LL_INFOS() << "LLPanelPermissions::onCommitNextOwnerCopy" << LL_ENDL;
	auto new_state = user_data.asBoolean();
	onCommitPerm(new_state, PERM_NEXT_OWNER, PERM_COPY);
}

void LLPanelPermissions::onCommitNextOwnerTransfer(const LLSD& user_data)
{
	//LL_INFOS() << "LLPanelPermissions::onCommitNextOwnerTransfer" << LL_ENDL;
	auto new_state = user_data.asBoolean();
	onCommitPerm(new_state, PERM_NEXT_OWNER, PERM_TRANSFER);
}

void LLPanelPermissions::onCommitExport(const LLSD& user_data)
{
	auto new_state = user_data.asBoolean();
	onCommitPerm(new_state, PERM_EVERYONE, PERM_EXPORT);
}

void LLPanelPermissions::onCommitName(const LLSD& user_data)
{
	const auto& name_string = user_data.asStringRef();

	LLSelectMgr::getInstance()->selectionSetObjectName(name_string);
	LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
	if (selection->isAttachment() && (selection->getNumNodes() == 1) && !name_string.empty())
	{
		LLUUID object_id = selection->getFirstObject()->getAttachmentItemID();
		LLViewerInventoryItem* item = findItem(object_id);
		if (item)
		{
			LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
			new_item->rename(name_string);
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
	}
}

void LLPanelPermissions::onCommitDesc(const LLSD& user_data)
{
	const auto& desc_string = user_data.asStringRef();
	LLSelectMgr::getInstance()->selectionSetObjectDescription(desc_string);
	LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
	if (selection->isAttachment() && (selection->getNumNodes() == 1))
	{
		LLUUID object_id = selection->getFirstObject()->getAttachmentItemID();
		LLViewerInventoryItem* item = findItem(object_id);
		if (item)
		{
			LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
			new_item->setDescription(desc_string);
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
	}
}

// static
void LLPanelPermissions::onCommitSaleInfo(LLUICtrl*, void* data)
{
	LLPanelPermissions* self = (LLPanelPermissions*)data;
	self->setAllSaleInfo();
}

void LLPanelPermissions::setAllSaleInfo()
{
	LL_INFOS() << "LLPanelPermissions::setAllSaleInfo()" << LL_ENDL;
	LLSaleInfo::EForSale sale_type = LLSaleInfo::FS_NOT;
	
	// Set the sale type if the object(s) are for sale.
	if(mCheckForSale && mCheckForSale->get())
	{
		sale_type = static_cast<LLSaleInfo::EForSale>(mComboSaleType->getValue().asInteger());
	}

	S32 price = -1;
	
	price = (mSpinnerEditCost->getTentative()) ? DEFAULT_PRICE : mSpinnerEditCost->getValue().asInteger();

	// If somehow an invalid price, turn the sale off.
	if (price < 0)
		sale_type = LLSaleInfo::FS_NOT;

	LLSaleInfo old_sale_info;
	LLSelectMgr::getInstance()->selectGetSaleInfo(old_sale_info);

	LLSaleInfo new_sale_info(sale_type, price);
	LLSelectMgr::getInstance()->selectionSetObjectSaleInfo(new_sale_info);

    struct f : public LLSelectedObjectFunctor
    {
	    bool apply(LLViewerObject* object) override
        {
            return object->getClickAction() == CLICK_ACTION_BUY
                || object->getClickAction() == CLICK_ACTION_TOUCH;
        }
    } check_actions;

    // Selection should only contain objects that are of target
    // action already or of action we are aiming to remove.
    bool default_actions = LLSelectMgr::getInstance()->getSelection()->applyToObjects(&check_actions);

    if (default_actions && old_sale_info.isForSale() != new_sale_info.isForSale())
    {
        U8 new_click_action = new_sale_info.isForSale() ? CLICK_ACTION_BUY : CLICK_ACTION_TOUCH;
        LLSelectMgr::getInstance()->selectionSetClickAction(new_click_action);
    }
}

struct LLSelectionPayable : public LLSelectedObjectFunctor
{
	bool apply(LLViewerObject* obj) override
	{
		// can pay if you or your parent has money() event in script
		LLViewerObject* parent = (LLViewerObject*)obj->getParent();
		return (obj->flagTakesMoney() 
			   || (parent && parent->flagTakesMoney()));
	}
};

void LLPanelPermissions::onCommitClickAction(const LLSD& user_data)
{
	const auto& value = user_data.asStringRef();
	U8 click_action = string_value_to_click_action(value);
	
	if (click_action == CLICK_ACTION_BUY)
	{
		LLSaleInfo sale_info;
		LLSelectMgr::getInstance()->selectGetSaleInfo(sale_info);
		if (!sale_info.isForSale())
		{
			LLNotificationsUtil::add("CantSetBuyObject");

			// Set click action back to its old value
			U8 click_action = 0;
			LLSelectMgr::getInstance()->selectionGetClickAction(&click_action);
			std::string item_value = click_action_to_string_value(click_action);
			mComboClickAction->setValue(LLSD(item_value));
			return;
		}
	}
	else if (click_action == CLICK_ACTION_PAY)
	{
		// Verify object has script with money() handler
		LLSelectionPayable payable;
		bool can_pay = LLSelectMgr::getInstance()->getSelection()->applyToObjects(&payable);
		if (!can_pay)
		{
			// Warn, but do it anyway.
			LLNotificationsUtil::add("ClickActionNotPayable");
		}
	}
	LLSelectMgr::getInstance()->selectionSetClickAction(click_action);
}

void LLPanelPermissions::onCommitIncludeInSearch(const LLSD& user_data)
{
	LLSelectMgr::getInstance()->selectionSetIncludeInSearch(user_data.asBoolean());
}


LLViewerInventoryItem* LLPanelPermissions::findItem(LLUUID &object_id)
{
	if (!object_id.isNull())
	{
		return gInventory.getItem(object_id);
	}
	return nullptr;
}
