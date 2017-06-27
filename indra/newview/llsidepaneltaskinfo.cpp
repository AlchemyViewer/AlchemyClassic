// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llsidepaneltaskinfo.cpp
 * @brief LLSidepanelTaskInfo class implementation
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

#include "llsidepaneltaskinfo.h"

#include "llfocusmgr.h"
#include "llpermissions.h"
#include "lltrans.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llnamebox.h"
#include "llspinctrl.h"
#include "lltextbase.h"
#include "lltextbox.h"

#include "llagent.h"
#include "llinventorymodel.h"
#include "llfloatergroups.h"
#include "llfloaterreg.h"
#include "llselectmgr.h"
#include "llstatusbar.h"		// for getBalance()
#include "llviewerinventory.h"
#include "llnotificationsutil.h"

#include "roles_constants.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewernetwork.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"

static const std::array<std::string, 18> sNoItemNames{{
	"Object Name",
	"Object Description",
	"button set group",
	"checkbox share with group",
	"button deed",
	"checkbox allow everyone move",
	"checkbox allow everyone copy",
	"checkbox for sale",
	"sale type",
	"Edit Cost",
	"checkbox next owner can modify",
	"checkbox next owner can copy",
	"checkbox next owner can transfer",
	"checkbox next owner can export",
	"clickaction",
	"search_check",
	"perm_modify",
	"Group Name",
}};

///----------------------------------------------------------------------------
/// Class llsidepaneltaskinfo
///----------------------------------------------------------------------------

LLSidepanelTaskInfo* LLSidepanelTaskInfo::sActivePanel = nullptr;

static LLPanelInjector<LLSidepanelTaskInfo> t_task_info("sidepanel_task_info");

// Default constructor
LLSidepanelTaskInfo::LLSidepanelTaskInfo(const LLPanel::Params& p)
:	LLSidepanelInventorySubpanel(p)
,	mObjectRenderStatsText(nullptr)
{
	setMouseOpaque(FALSE);
	LLSelectMgr::instance().mUpdateSignal.connect(boost::bind(&LLSidepanelTaskInfo::refreshAll, this));
}


LLSidepanelTaskInfo::~LLSidepanelTaskInfo()
{
	if (sActivePanel == this)
		sActivePanel = nullptr;
}

// virtual
BOOL LLSidepanelTaskInfo::postBuild()
{
	LLSidepanelInventorySubpanel::postBuild();

	mObjectNameLabel = getChild<LLTextBox>("Name:");
	mObjectNameEditor = getChild<LLLineEditor>("Object Name");
	mObjectNameEditor->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitName, this, _2));
	mObjectNameEditor->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);

	mObjectDescriptionLabel = getChild<LLTextBox>("Description:");
	mObjectDescriptionEditor = getChild<LLLineEditor>("Object Description");
	mObjectDescriptionEditor->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitDesc, this, _2));
	mObjectDescriptionEditor->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);

	mCreatorNameLabel = getChild<LLTextBox>("CreatorNameLabel");
	mCreatorNameEditor = getChild<LLTextBox>("Creator Name");

	mOwnerNameLabel = getChild<LLTextBox>("Owner:");
	mOwnerNameEditor = getChild<LLTextBox>("Owner Name");

	mGroupNameLabel = getChild<LLTextBox>("Group_label");
	mGroupSetButton = getChild<LLButton>("button set group");
	mGroupSetButton->setClickedCallback(boost::bind(&LLSidepanelTaskInfo::onClickGroup, this));
	mGroupNameBox = getChild<LLNameBox>("Group Name Proxy");

	mDeedBtn = getChild<LLButton>("button deed");
	mDeedBtn->setClickedCallback(boost::bind(&LLSidepanelTaskInfo::onClickDeedToGroup, this));

	mClickActionLabel = getChild<LLTextBox>("label click action");
	mClickActionCombo = getChild<LLComboBox>("clickaction");
	mClickActionCombo->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitClickAction, this, _2));

	mPermModifyLabel = getChild<LLTextBox>("perm_modify");

	mAllowEveryoneCopyCheck = getChild<LLCheckBoxCtrl>("checkbox allow everyone copy");
	mAllowEveryoneCopyCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitEveryoneCopy, this, _2));
	mAllowEveryoneMoveCheck = getChild<LLCheckBoxCtrl>("checkbox allow everyone move");
	mAllowEveryoneMoveCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitEveryoneMove, this, _2));

	mShareWithGroupCheck = getChild<LLCheckBoxCtrl>("checkbox share with group");
	mShareWithGroupCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitGroupShare, this, _2));

	mNextOwnerPermLabel = getChild<LLTextBox>("NextOwnerPermsLabel");
	mNextOwnerCanModifyCheck = getChild<LLCheckBoxCtrl>("checkbox next owner can modify");
	mNextOwnerCanModifyCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitNextOwnerModify, this, _2));
	mNextOwnerCanCopyCheck = getChild<LLCheckBoxCtrl>("checkbox next owner can copy");
	mNextOwnerCanCopyCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitNextOwnerCopy, this, _2));
	mNextOwnerCanTransferCheck = getChild<LLCheckBoxCtrl>("checkbox next owner can transfer");
	mNextOwnerCanTransferCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitNextOwnerTransfer, this, _2));
	mNextOwnerCanExportCheck = getChild<LLCheckBoxCtrl>("checkbox next owner can export");
	mNextOwnerCanExportCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitNextOwnerExport, this, _2));

	mObjectRenderStatsText = getChild<LLTextBox>("object_stats");

	mForSaleCheck = getChild<LLCheckBoxCtrl>("checkbox for sale");
	mForSaleCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::setAllSaleInfo, this));
	mSaleTypeCombo = getChild<LLComboBox>("sale type");
	mSaleTypeCombo->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::setAllSaleInfo, this));
	mSaleCostSpinner = getChild<LLSpinCtrl>("Edit Cost");
	mSaleCostSpinner->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::setAllSaleInfo, this));

	mSearchCheck = getChild<LLCheckBoxCtrl>("search_check");
	mSearchCheck->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onCommitIncludeInSearch, this, _2));

	mPathfindingAttributesText = getChild<LLTextBox>("pathfinding_attributes_value");

	mAdvPermB = getChild<LLTextBox>("B:");
	mAdvPermO = getChild<LLTextBox>("O:");
	mAdvPermG = getChild<LLTextBox>("G:");
	mAdvPermE = getChild<LLTextBox>("E:");
	mAdvPermN = getChild<LLTextBox>("N:");
	mAdvPermF = getChild<LLTextBox>("F:");

	mOpenBtn = getChild<LLButton>("open_btn");
	mOpenBtn->setClickedCallback(boost::bind(&LLSidepanelTaskInfo::onOpenButtonClicked, this));

	mPayBtn = getChild<LLButton>("pay_btn");
	mPayBtn->setClickedCallback(boost::bind(&LLSidepanelTaskInfo::onPayButtonClicked, this));

	mBuyBtn = getChild<LLButton>("buy_btn");
	mBuyBtn->setClickedCallback(boost::bind(&handle_buy));

	mDetailsBtn = getChild<LLButton>("details_btn");
	mDetailsBtn->setClickedCallback(boost::bind(&LLSidepanelTaskInfo::onDetailsButtonClicked, this));

	
	if (LLFloater* floater = dynamic_cast<LLFloater*>(getParent()))
		getChild<LLUICtrl>("back_btn")->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::closeParentFloater, this));
	//else if (dynamic_cast<LLSideTrayPanelContainer*>(getParent()))
	//	getChild<LLUICtrl>("back_btn")->setCommitCallback(boost::bind(&LLSidepanelTaskInfo::onBackBtnClick, this));
	else
		getChild<LLUICtrl>("back_btn")->setEnabled(FALSE);

	return TRUE;
}

/*virtual*/ 
void LLSidepanelTaskInfo::onVisibilityChange ( BOOL visible )
{
	if (visible)
	{
		sActivePanel = this;
		mObject = getFirstSelectedObject();
	}
	else
	{
		sActivePanel = nullptr;
		// drop selection reference
		mObjectSelection = nullptr;
	}
}


void LLSidepanelTaskInfo::disableAll()
{
	mObjectNameLabel->setEnabled(FALSE);
	mObjectNameEditor->setValue(LLStringUtil::null);
	mObjectNameEditor->setEnabled(FALSE);

	mObjectDescriptionLabel->setEnabled(FALSE);
	mObjectDescriptionEditor->setValue(LLStringUtil::null);
	mObjectDescriptionEditor->setEnabled(FALSE);

	mCreatorNameLabel->setEnabled(FALSE);
	mCreatorNameEditor->setValue(LLStringUtil::null);
	mCreatorNameEditor->setEnabled(FALSE);

	mOwnerNameLabel->setEnabled(FALSE);
	mOwnerNameEditor->setValue(LLStringUtil::null);
	mOwnerNameEditor->setEnabled(FALSE);

	mGroupNameLabel->setEnabled(FALSE);
	mGroupNameBox->setValue(LLStringUtil::null);
	mGroupNameBox->setEnabled(FALSE);
	mGroupSetButton->setEnabled(FALSE);

	mDeedBtn->setEnabled(FALSE);

	mClickActionLabel->setEnabled(FALSE);
	mClickActionCombo->setEnabled(FALSE);
	mClickActionCombo->clear();

	mPermModifyLabel->setEnabled(FALSE);
	mPermModifyLabel->setValue(LLStringUtil::null);

	mAllowEveryoneCopyCheck->setValue(FALSE);
	mAllowEveryoneCopyCheck->setEnabled(FALSE);
	mAllowEveryoneMoveCheck->setValue(FALSE);
	mAllowEveryoneMoveCheck->setEnabled(FALSE);

	mShareWithGroupCheck->setValue(FALSE);
	mShareWithGroupCheck->setEnabled(FALSE);

	//Next owner can:
	mNextOwnerPermLabel->setEnabled(FALSE);
	mNextOwnerCanModifyCheck->setValue(FALSE);
	mNextOwnerCanModifyCheck->setEnabled(FALSE);
	mNextOwnerCanCopyCheck->setValue(FALSE);
	mNextOwnerCanCopyCheck->setEnabled(FALSE);
	mNextOwnerCanTransferCheck->setValue(FALSE);
	mNextOwnerCanTransferCheck->setEnabled(FALSE);
	mNextOwnerCanExportCheck->setValue(FALSE);
	mNextOwnerCanExportCheck->setEnabled(FALSE);
	mNextOwnerCanExportCheck->setVisible(!LLGridManager::getInstance()->isInSecondlife());

	// Render Info
	mObjectRenderStatsText->setValue(LLStringUtil::null);

	//checkbox for sale
	mForSaleCheck->setValue(FALSE);
	mForSaleCheck->setEnabled(FALSE);

	mSaleTypeCombo->setValue(LLSaleInfo::FS_COPY);
	mSaleTypeCombo->setEnabled(FALSE);

	mSaleCostSpinner->setValue(LLStringUtil::null);
	mSaleCostSpinner->setEnabled(FALSE);
	
	//checkbox include in search
	mSearchCheck->setValue(FALSE);
	mSearchCheck->setEnabled(FALSE);

	mPathfindingAttributesText->setEnabled(FALSE);
	mPathfindingAttributesText->setValue(LLStringUtil::null);

	mAdvPermB->setVisible(FALSE);
	mAdvPermO->setVisible(FALSE);
	mAdvPermG->setVisible(FALSE);
	mAdvPermE->setVisible(FALSE);
	mAdvPermN->setVisible(FALSE);
	mAdvPermF->setVisible(FALSE);
	
	mOpenBtn->setEnabled(FALSE);
	mPayBtn->setEnabled(FALSE);
	mBuyBtn->setEnabled(FALSE);
}

void LLSidepanelTaskInfo::refresh()
{
	{	
		const std::string& deedText = getString(gWarningSettings.getBOOL("DeedObject")
												? "text deed continued"
												: "text deed");
		mDeedBtn->setLabelSelected(deedText);
		mDeedBtn->setLabelUnselected(deedText);
	}

	BOOL root_selected = TRUE;
	LLSelectNode* nodep = mObjectSelection->getFirstRootNode();
	S32 object_count = mObjectSelection->getRootObjectCount();
	if (!nodep || (object_count == 0))
	{
		nodep = mObjectSelection->getFirstNode();
		object_count = mObjectSelection->getObjectCount();
		root_selected = FALSE;
	}

	LLViewerObject* objectp = nullptr;
	if (nodep)
	{
		objectp = nodep->getObject();
	}

	// ...nothing selected
	if (!nodep || !objectp)
	{
		disableAll();
		return;
	}

	// figure out a few variables
	const BOOL is_one_object = (object_count == 1);
	
	// BUG: fails if a root and non-root are both single-selected.
	const BOOL is_perm_modify = (mObjectSelection->getFirstRootNode() && LLSelectMgr::getInstance()->selectGetRootsModify()) ||
		LLSelectMgr::getInstance()->selectGetModify();
	const BOOL is_nonpermanent_enforced = (mObjectSelection->getFirstRootNode() && LLSelectMgr::getInstance()->selectGetRootsNonPermanentEnforced()) ||
		LLSelectMgr::getInstance()->selectGetNonPermanentEnforced();

	S32 total_bytes = 0;
	S32 visible_bytes = 0;
	S32 vertex_count = 0;
	F32 streaming_cost = mObjectSelection->getSelectedObjectStreamingCost(&total_bytes, &visible_bytes);
	S32 triangle_count = mObjectSelection->getSelectedObjectTriangleCount(&vertex_count);
	
	/* Objects: [COUNT]
	   Vertices: [vCOUNT]
	   Triangles: [tCOUNT]
	   Streaming Cost: [COST]
	   [BYTES] */
	LLStringUtil::format_map_t args;
	args["COUNT"]	= std::to_string(object_count);
	args["vCOUNT"]	= std::to_string(vertex_count);
	args["tCOUNT"]	= std::to_string(triangle_count);
	args["COST"]	= llformat("%.3f", streaming_cost);
	args["BYTES"]	= total_bytes != 0 ? llformat("%d / %d KB", visible_bytes/1024, total_bytes/1024) : LLStringUtil::null;
	LLUIString fmt = getString("stats_fmt");
	fmt.setArgs(args);
	mObjectRenderStatsText->setText(fmt.getString());
	
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
	mPermModifyLabel->setEnabled(TRUE);
	mPermModifyLabel->setValue(MODIFY_INFO_STRINGS[string_index]);

	std::string pfAttrName;

	if ((mObjectSelection->getFirstRootNode() 
		&& LLSelectMgr::getInstance()->selectGetRootsNonPathfinding())
		|| LLSelectMgr::getInstance()->selectGetNonPathfinding())
	{
		pfAttrName = "Pathfinding_Object_Attr_None";
	}
	else if ((mObjectSelection->getFirstRootNode() 
		&& LLSelectMgr::getInstance()->selectGetRootsPermanent())
		|| LLSelectMgr::getInstance()->selectGetPermanent())
	{
		pfAttrName = "Pathfinding_Object_Attr_Permanent";
	}
	else if ((mObjectSelection->getFirstRootNode() 
		&& LLSelectMgr::getInstance()->selectGetRootsCharacter())
		|| LLSelectMgr::getInstance()->selectGetCharacter())
	{
		pfAttrName = "Pathfinding_Object_Attr_Character";
	}
	else
	{
		pfAttrName = "Pathfinding_Object_Attr_MultiSelect";
	}

	mPathfindingAttributesText->setEnabled(TRUE);
	mPathfindingAttributesText->setValue(LLTrans::getString(pfAttrName));

	// Update creator text field
	mCreatorNameLabel->setEnabled(TRUE);

	std::string creator_name;
	LLUUID creator_id;
	LLSelectMgr::getInstance()->selectGetCreator(creator_id, creator_name);

	if(creator_id != mCreatorID )
	{
		mCreatorNameEditor->setValue(creator_name);
		mCreatorID = creator_id;
	}
	if(mCreatorNameEditor->getValue().asString() == LLStringUtil::null)
	{
		mCreatorNameEditor->setValue(creator_name);
	}
	mCreatorNameEditor->setEnabled(TRUE);

	// Update owner text field
	mOwnerNameLabel->setEnabled(TRUE);

	std::string owner_name;
	LLUUID owner_id;
	const BOOL owners_identical = LLSelectMgr::getInstance()->selectGetOwner(owner_id, owner_name);
	if (owner_id.isNull())
	{
		if (LLSelectMgr::getInstance()->selectIsGroupOwned())
		{
			// Group owned already displayed by selectGetOwner
		}
		else
		{
			// Display last owner if public
			std::string last_owner_name;
			LLSelectMgr::getInstance()->selectGetLastOwner(mLastOwnerID, last_owner_name);

			// It should never happen that the last owner is null and the owner
			// is null, but it seems to be a bug in the simulator right now. JC
			if (!mLastOwnerID.isNull() && !last_owner_name.empty())
			{
				owner_name.append(", last ");
				owner_name.append(last_owner_name);
			}
		}
	}

	if(owner_id.isNull() || (owner_id != mOwnerID))
	{
		mOwnerNameEditor->setValue(owner_name);
		mOwnerID = owner_id;
	}
	if(mOwnerNameEditor->getValue().asString() == LLStringUtil::null)
	{
		mOwnerNameEditor->setValue(owner_name);
	}

	mOwnerNameEditor->setEnabled(TRUE);

	// update group text field
	mGroupNameLabel->setEnabled(TRUE);
	mGroupNameBox->setNameID(LLUUID::null, TRUE);
	LLUUID group_id;
	BOOL groups_identical = LLSelectMgr::getInstance()->selectGetGroup(group_id);
	if (groups_identical)
	{
		if (mGroupNameBox)
		{
			mGroupNameBox->setNameID(group_id,TRUE);
			mGroupNameBox->setEnabled(TRUE);
		}
	}
	else
	{
		if (mGroupNameBox)
		{
			mGroupNameBox->setNameID(LLUUID::null, TRUE);
			mGroupNameBox->refresh(LLUUID::null, std::string(), true);
			mGroupNameBox->setEnabled(FALSE);
		}
	}
	
	mGroupSetButton->setEnabled(owners_identical && (mOwnerID == gAgent.getID()) && is_nonpermanent_enforced);

	mObjectNameLabel->setEnabled(TRUE);
	mObjectDescriptionLabel->setEnabled(TRUE);

	// figure out the contents of the name, description, & category
	BOOL edit_name_desc = (is_one_object && objectp->permModify() && !objectp->isPermanentEnforced());
	mObjectNameEditor->setEnabled(edit_name_desc);
	mObjectDescriptionEditor->setEnabled(edit_name_desc);

	if (is_one_object)
	{
		if (!mObjectNameEditor->hasFocus())
		{
			mObjectNameEditor->setValue(nodep->mName);
		}

		if (!mObjectDescriptionEditor->hasFocus())
		{
			mObjectDescriptionEditor->setText(nodep->mDescription);
		}
	}
	else
	{
		mObjectNameEditor->setValue(LLStringUtil::null);
		mObjectDescriptionEditor->setText(LLStringUtil::null);
	}


	S32 total_sale_price = 0;
	S32 individual_sale_price = 0;
	BOOL is_for_sale_mixed = FALSE;
	BOOL is_sale_price_mixed = FALSE;
	U32 num_for_sale = 0;
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
		mSaleCostSpinner->setValue(LLStringUtil::null);
		mSaleCostSpinner->setEnabled(FALSE);
	}
	// You own these objects.
	else if (self_owned || (group_owned && gAgent.hasPowerInGroup(group_id,GP_OBJECT_SET_SALE)))
	{
		// If there are multiple items for sale then set text to PRICE PER UNIT.
		if (num_for_sale > 1)
		{
			std::string label_text = is_sale_price_mixed? "Cost Mixed" :"Cost Per Unit";
			mSaleCostSpinner->setLabel(getString(label_text));
		}
		else
		{
			mSaleCostSpinner->setLabel(getString("Cost Default"));
		}
		
		if (!mSaleCostSpinner->hasFocus())
		{
			// If the sale price is mixed then set the cost to MIXED, otherwise
			// set to the actual cost.
			if ((num_for_sale > 0) && is_for_sale_mixed)
			{
				mSaleCostSpinner->setTentative(TRUE);
			}
			else if ((num_for_sale > 0) && is_sale_price_mixed)
			{
				mSaleCostSpinner->setTentative(TRUE);
			}
			else 
			{
				mSaleCostSpinner->setValue(individual_sale_price);
			}
		}
		// The edit fields are only enabled if you can sell this object
		// and the sale price is not mixed.
		BOOL enable_edit = (num_for_sale && can_transfer) ? !is_for_sale_mixed : FALSE;
		mSaleCostSpinner->setEnabled(enable_edit);
	}
	// Someone, not you, owns these objects.
	else if (!public_owned)
	{
		mSaleCostSpinner->setEnabled(FALSE);
		
		// Don't show a price if none of the items are for sale.
		mSaleCostSpinner->setValue(num_for_sale
												  ? llformat("%d",total_sale_price)
												  : LLStringUtil::null);

		// If multiple items are for sale, set text to TOTAL PRICE.
		mSaleCostSpinner->setLabel(getString(num_for_sale > 1
													   ? "Cost Total"
													   : "Cost Default"));
	}
	// This is a public object.
	else
	{
		mSaleCostSpinner->setLabel(getString("Cost Default"));
		mSaleCostSpinner->setValue(LLStringUtil::null);
		mSaleCostSpinner->setEnabled(FALSE);
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
			mAdvPermB->setValue("B: " + mask_to_string(base_mask_on));
			mAdvPermB->setVisible(TRUE);

			mAdvPermO->setValue("O: " + mask_to_string(owner_mask_on));
			mAdvPermO->setVisible(TRUE);

			mAdvPermG->setValue("G: " + mask_to_string(group_mask_on));
			mAdvPermG->setVisible(TRUE);

			mAdvPermE->setValue("E: " + mask_to_string(everyone_mask_on));
			mAdvPermE->setVisible(TRUE);

			mAdvPermN->setValue("N: " + mask_to_string(next_owner_mask_on));
			mAdvPermN->setVisible(TRUE);
		}

		U32 flag_mask = 0x0;
		if (objectp->permMove()) 		flag_mask |= PERM_MOVE;
		if (objectp->permModify()) 		flag_mask |= PERM_MODIFY;
		if (objectp->permCopy()) 		flag_mask |= PERM_COPY;
		if (objectp->permTransfer()) 	flag_mask |= PERM_TRANSFER;

		mAdvPermF->setValue("F:" + mask_to_string(flag_mask));
		mAdvPermF->setVisible(TRUE);
	}
	else
	{
		mAdvPermB->setVisible(FALSE);
		mAdvPermO->setVisible(FALSE);
		mAdvPermG->setVisible(FALSE);
		mAdvPermE->setVisible(FALSE);
		mAdvPermN->setVisible(FALSE);
		mAdvPermF->setVisible(FALSE);
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
		mPermModifyLabel->setValue(getString("text modify warning"));
	}

	if (has_change_perm_ability)
	{
		mShareWithGroupCheck->setEnabled(TRUE);
		mAllowEveryoneMoveCheck->setEnabled(owner_mask_on & PERM_MOVE);
		mAllowEveryoneCopyCheck->setEnabled(owner_mask_on & PERM_COPY && owner_mask_on & PERM_TRANSFER);
	}
	else
	{
		mShareWithGroupCheck->setEnabled(FALSE);
		mAllowEveryoneMoveCheck->setEnabled(FALSE);
		mAllowEveryoneCopyCheck->setEnabled(FALSE);
	}

	if (has_change_sale_ability && (owner_mask_on & PERM_TRANSFER))
	{
		mForSaleCheck->setEnabled(can_transfer || num_for_sale);
		// Set the checkbox to tentative if the prices of each object selected
		// are not the same.
		mForSaleCheck->setTentative(is_for_sale_mixed);
		mSaleTypeCombo->setEnabled(num_for_sale && can_transfer && !is_sale_price_mixed);

		mNextOwnerPermLabel->setEnabled(TRUE);
		mNextOwnerCanModifyCheck->setEnabled(base_mask_on & PERM_MODIFY);
		mNextOwnerCanCopyCheck->setEnabled(base_mask_on & PERM_COPY);
		mNextOwnerCanTransferCheck->setEnabled(next_owner_mask_on & PERM_COPY);
		mNextOwnerCanExportCheck->setEnabled(next_owner_mask_on & PERM_EXPORT);
	}
	else 
	{
		mForSaleCheck->setEnabled(FALSE);
		mSaleTypeCombo->setEnabled(FALSE);

		mNextOwnerPermLabel->setEnabled(FALSE);
		mNextOwnerCanModifyCheck->setEnabled(FALSE);
		mNextOwnerCanCopyCheck->setEnabled(FALSE);
		mNextOwnerCanTransferCheck->setEnabled(FALSE);
		mNextOwnerCanExportCheck->setEnabled(FALSE);
	}

	if (valid_group_perms)
	{
		if ((group_mask_on & PERM_COPY) && (group_mask_on & PERM_MODIFY) && (group_mask_on & PERM_MOVE))
		{
			mShareWithGroupCheck->setValue(TRUE);
			mShareWithGroupCheck->setTentative(FALSE);
			mDeedBtn->setEnabled(gAgent.hasPowerInGroup(group_id, GP_OBJECT_DEED) && (owner_mask_on & PERM_TRANSFER) && !group_owned && can_transfer);
		}
		else if ((group_mask_off & PERM_COPY) && (group_mask_off & PERM_MODIFY) && (group_mask_off & PERM_MOVE))
		{
			mShareWithGroupCheck->setValue(FALSE);
			mShareWithGroupCheck->setTentative(FALSE);
			mDeedBtn->setEnabled(FALSE);
		}
		else
		{
			mShareWithGroupCheck->setValue(TRUE);
			mShareWithGroupCheck->setTentative(TRUE);
			mDeedBtn->setEnabled(gAgent.hasPowerInGroup(group_id, GP_OBJECT_DEED) && (group_mask_on & PERM_MOVE) && (owner_mask_on & PERM_TRANSFER) && !group_owned && can_transfer);
		}
	}			

	if (valid_everyone_perms)
	{
		// Move
		if (everyone_mask_on & PERM_MOVE)
		{
			mAllowEveryoneMoveCheck->setValue(TRUE);
			mAllowEveryoneMoveCheck->setTentative(FALSE);
		}
		else if (everyone_mask_off & PERM_MOVE)
		{
			mAllowEveryoneMoveCheck->setValue(FALSE);
			mAllowEveryoneMoveCheck->setTentative(FALSE);
		}
		else
		{
			mAllowEveryoneMoveCheck->setValue(TRUE);
			mAllowEveryoneMoveCheck->setTentative(TRUE);
		}

		// Copy == everyone can't copy
		if (everyone_mask_on & PERM_COPY)
		{
			mAllowEveryoneCopyCheck->setValue(TRUE);
			mAllowEveryoneCopyCheck->setTentative(!can_copy || !can_transfer);
		}
		else if (everyone_mask_off & PERM_COPY)
		{
			mAllowEveryoneCopyCheck->setValue(FALSE);
			mAllowEveryoneCopyCheck->setTentative(FALSE);
		}
		else
		{
			mAllowEveryoneCopyCheck->setValue(TRUE);
			mAllowEveryoneCopyCheck->setTentative(TRUE);
		}
	}

	if (valid_next_perms)
	{
		// Modify == next owner canot modify
		if (next_owner_mask_on & PERM_MODIFY)
		{
			mNextOwnerCanModifyCheck->setValue(TRUE);
			mNextOwnerCanModifyCheck->setTentative(FALSE);
		}
		else if (next_owner_mask_off & PERM_MODIFY)
		{
			mNextOwnerCanModifyCheck->setValue(FALSE);
			mNextOwnerCanModifyCheck->setTentative(FALSE);
		}
		else
		{
			mNextOwnerCanModifyCheck->setValue(TRUE);
			mNextOwnerCanModifyCheck->setTentative(TRUE);
		}

		// Copy == next owner cannot copy
		if (next_owner_mask_on & PERM_COPY)
		{			
			mNextOwnerCanCopyCheck->setValue(TRUE);
			mNextOwnerCanCopyCheck->setTentative(!can_copy);
		}
		else if (next_owner_mask_off & PERM_COPY)
		{
			mNextOwnerCanCopyCheck->setValue(FALSE);
			mNextOwnerCanCopyCheck->setTentative(FALSE);
		}
		else
		{
			mNextOwnerCanCopyCheck->setValue(TRUE);
			mNextOwnerCanCopyCheck->setTentative(TRUE);
		}

		// Transfer == next owner cannot transfer
		if (next_owner_mask_on & PERM_TRANSFER)
		{
			mNextOwnerCanTransferCheck->setValue(TRUE);
			mNextOwnerCanTransferCheck->setTentative(!can_transfer);
		}
		else if (next_owner_mask_off & PERM_TRANSFER)
		{
			mNextOwnerCanTransferCheck->setValue(FALSE);
			mNextOwnerCanTransferCheck->setTentative(FALSE);
		}
		else
		{
			mNextOwnerCanTransferCheck->setValue(TRUE);
			mNextOwnerCanTransferCheck->setTentative(TRUE);
		}

		// Export == next owner cannot export
		if (next_owner_mask_on & PERM_EXPORT)
		{
			mNextOwnerCanExportCheck->setValue(TRUE);
			mNextOwnerCanExportCheck->setTentative(	FALSE);
		}
		else if (next_owner_mask_off & PERM_EXPORT)
		{
			mNextOwnerCanExportCheck->setValue(FALSE);
			mNextOwnerCanExportCheck->setTentative(FALSE);
		}
		else
		{
			mNextOwnerCanExportCheck->setValue(TRUE);
			mNextOwnerCanExportCheck->setTentative(TRUE);
		}
	}

	// reflect sale information
	LLSaleInfo sale_info;
	BOOL valid_sale_info = LLSelectMgr::getInstance()->selectGetSaleInfo(sale_info);
	LLSaleInfo::EForSale sale_type = sale_info.getSaleType();

	if (valid_sale_info)
	{
		mSaleTypeCombo->setValue(sale_type == LLSaleInfo::FS_NOT ? LLSaleInfo::FS_COPY : sale_type);
		mSaleTypeCombo->setTentative(FALSE); // unfortunately this doesn't do anything at the moment.
	}
	else
	{
		// default option is sell copy, determined to be safest
		mSaleTypeCombo->setValue(LLSaleInfo::FS_COPY);
		mSaleTypeCombo->setTentative(TRUE); // unfortunately this doesn't do anything at the moment.
	}

	mForSaleCheck->setValue((num_for_sale != 0));

	// HACK: There are some old objects in world that are set for sale,
	// but are no-transfer.  We need to let users turn for-sale off, but only
	// if for-sale is set.
	bool cannot_actually_sell = !can_transfer || (!can_copy && sale_type == LLSaleInfo::FS_COPY);
	if (cannot_actually_sell && num_for_sale && has_change_sale_ability)
	{
		mForSaleCheck->setEnabled(TRUE);
	}
	
	// Check search status of objects
	const BOOL all_volume = LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME );
	bool include_in_search;
	const BOOL all_include_in_search = LLSelectMgr::getInstance()->selectionGetIncludeInSearch(&include_in_search);
	mSearchCheck->setEnabled(has_change_sale_ability && all_volume);
	mSearchCheck->setValue(include_in_search);
	mSearchCheck->setTentative(!all_include_in_search);

	// Click action (touch, sit, buy)
	U8 click_action = 0;
	if (LLSelectMgr::getInstance()->selectionGetClickAction(&click_action))
	{
		mClickActionCombo->setCurrentByIndex((S32)click_action);
	}
	mClickActionLabel->setEnabled(is_perm_modify && is_nonpermanent_enforced && all_volume);
	mClickActionCombo->setEnabled(is_perm_modify && is_nonpermanent_enforced && all_volume);

	if (!getIsEditing())
	{
		mObjectNameEditor->setEnabled(FALSE);
		mObjectDescriptionEditor->setEnabled(FALSE);
		mGroupSetButton->setEnabled(FALSE);
		mShareWithGroupCheck->setEnabled(FALSE);
		mDeedBtn->setEnabled(FALSE);
		mAllowEveryoneMoveCheck->setEnabled(FALSE);
		mAllowEveryoneCopyCheck->setEnabled(FALSE);
		mForSaleCheck->setEnabled(FALSE);
		mSaleTypeCombo->setEnabled(FALSE);
		mSaleCostSpinner->setEnabled(FALSE);
		mNextOwnerCanModifyCheck->setEnabled(FALSE);
		mNextOwnerCanCopyCheck->setEnabled(FALSE);
		mNextOwnerCanTransferCheck->setEnabled(FALSE);
		mNextOwnerCanExportCheck->setEnabled(FALSE);
		mClickActionCombo->setEnabled(FALSE);
		mSearchCheck->setEnabled(FALSE);
		mPermModifyLabel->setEnabled(FALSE);
		mGroupNameBox->setEnabled(FALSE);
	}
	updateVerbs();
}

void LLSidepanelTaskInfo::onClickGroup()
{
	LLUUID owner_id;
	std::string name;
	BOOL owners_identical = LLSelectMgr::getInstance()->selectGetOwner(owner_id, name);
	LLFloater* parent_floater = gFloaterView->getParentFloater(this);

	if (owners_identical && (owner_id == gAgent.getID()))
	{
		LLFloaterGroupPicker* fg = LLFloaterReg::showTypedInstance<LLFloaterGroupPicker>("group_picker", LLSD(gAgent.getID()));
		if (fg)
		{
			fg->setSelectGroupCallback( boost::bind(&LLSidepanelTaskInfo::cbGroupID, this, _1) );
			if (parent_floater)
			{
				LLRect new_rect = gFloaterView->findNeighboringPosition(parent_floater, fg);
				fg->setOrigin(new_rect.mLeft, new_rect.mBottom);
				parent_floater->addDependentFloater(fg);
			}
		}
	}
}

void LLSidepanelTaskInfo::cbGroupID(LLUUID group_id)
{
	if (mGroupNameBox)
	{
		mGroupNameBox->setNameID(group_id, TRUE);
	}
	LLSelectMgr::getInstance()->sendGroup(group_id);
}

static bool callback_deed_to_group(const LLSD& notification, const LLSD& response)
{
	const S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		LLUUID group_id;
		const BOOL groups_identical = LLSelectMgr::getInstance()->selectGetGroup(group_id);
		if (group_id.notNull() && groups_identical && (gAgent.hasPowerInGroup(group_id, GP_OBJECT_DEED)))
		{
			LLSelectMgr::getInstance()->sendOwner(LLUUID::null, group_id, FALSE);
		}
	}
	return FALSE;
}

void LLSidepanelTaskInfo::onClickDeedToGroup() const
{
	LLNotificationsUtil::add("DeedObjectToGroup", LLSD(), LLSD(), callback_deed_to_group);
}

///----------------------------------------------------------------------------
/// Permissions checkboxes
///----------------------------------------------------------------------------

void LLSidepanelTaskInfo::onCommitPerm(BOOL enabled, U8 field, U32 perm)
{
	LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
	if(!object) return;

	LLSelectMgr::getInstance()->selectionSetObjectPermissions(field, enabled, perm);
}

void LLSidepanelTaskInfo::onCommitGroupShare(const LLSD& user_data)
{
	onCommitPerm(user_data.asBoolean(), PERM_GROUP, PERM_MODIFY | PERM_MOVE | PERM_COPY);
}

void LLSidepanelTaskInfo::onCommitEveryoneMove(const LLSD& user_data)
{
	onCommitPerm(user_data.asBoolean(), PERM_EVERYONE, PERM_MOVE);
}

void LLSidepanelTaskInfo::onCommitEveryoneCopy(const LLSD& user_data)
{
	onCommitPerm(user_data.asBoolean(), PERM_EVERYONE, PERM_COPY);
}

void LLSidepanelTaskInfo::onCommitNextOwnerModify(const LLSD& user_data)
{
	//LL_INFOS() << "LLSidepanelTaskInfo::onCommitNextOwnerModify" << LL_ENDL;
	onCommitPerm(user_data.asBoolean(), PERM_NEXT_OWNER, PERM_MODIFY);
}

void LLSidepanelTaskInfo::onCommitNextOwnerCopy(const LLSD& user_data)
{
	//LL_INFOS() << "LLSidepanelTaskInfo::onCommitNextOwnerCopy" << LL_ENDL;
	onCommitPerm(user_data.asBoolean(), PERM_NEXT_OWNER, PERM_COPY);
}

void LLSidepanelTaskInfo::onCommitNextOwnerTransfer(const LLSD& user_data)
{
	//LL_INFOS() << "LLSidepanelTaskInfo::onCommitNextOwnerTransfer" << LL_ENDL;
	onCommitPerm(user_data.asBoolean(), PERM_EVERYONE, PERM_TRANSFER);
}

void LLSidepanelTaskInfo::onCommitNextOwnerExport(const LLSD& user_data)
{
	onCommitPerm(user_data.asBoolean(), PERM_NEXT_OWNER, PERM_EXPORT);
}

void LLSidepanelTaskInfo::onCommitName(const LLSD& user_data)
{
	const auto& name_string = user_data.asStringRef();

	LLSelectMgr::getInstance()->selectionSetObjectName(name_string);
	LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
	if (selection->isAttachment() && (selection->getNumNodes() == 1) && !name_string.empty())
	{
		LLUUID object_id = selection->getFirstObject()->getAttachmentItemID();
		if (object_id.notNull())
		{
			LLViewerInventoryItem* item = gInventory.getItem(object_id);
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
}


void LLSidepanelTaskInfo::onCommitDesc(const LLSD& user_data)
{
	const auto& desc_string = user_data.asStringRef();

	LLSelectMgr::getInstance()->selectionSetObjectDescription(user_data.asString());
	LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
	if (selection->isAttachment() && (selection->getNumNodes() == 1))
	{
		LLUUID object_id = selection->getFirstObject()->getAttachmentItemID();
		if (object_id.notNull())
		{
			LLViewerInventoryItem* item = gInventory.getItem(object_id);
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
}

void LLSidepanelTaskInfo::setAllSaleInfo()
{
	LLSaleInfo::EForSale sale_type = LLSaleInfo::FS_NOT;

	// Set the sale type if the object(s) are for sale.
	if(mForSaleCheck->get())
	{
		sale_type = static_cast<LLSaleInfo::EForSale>(mSaleTypeCombo->getValue().asInteger());
	}

	S32 price = -1;
	
	price = (mSaleCostSpinner->getTentative()) ? DEFAULT_PRICE : mSaleCostSpinner->getValue().asInteger();

	// If somehow an invalid price, turn the sale off.
	if (price < 0)
		sale_type = LLSaleInfo::FS_NOT;

	LLSaleInfo old_sale_info;
	LLSelectMgr::getInstance()->selectGetSaleInfo(old_sale_info);

	LLSaleInfo new_sale_info(sale_type, price);
	LLSelectMgr::getInstance()->selectionSetObjectSaleInfo(new_sale_info);
	
	U8 old_click_action = 0;
	LLSelectMgr::getInstance()->selectionGetClickAction(&old_click_action);

	if (old_sale_info.isForSale()
		&& !new_sale_info.isForSale()
		&& old_click_action == CLICK_ACTION_BUY)
	{
		// If turned off for-sale, make sure click-action buy is turned
		// off as well
		LLSelectMgr::getInstance()->
			selectionSetClickAction(CLICK_ACTION_TOUCH);
	}
	else if (new_sale_info.isForSale()
		&& !old_sale_info.isForSale()
		&& old_click_action == CLICK_ACTION_TOUCH)
	{
		// If just turning on for-sale, preemptively turn on one-click buy
		// unless user have a different click action set
		LLSelectMgr::getInstance()->
			selectionSetClickAction(CLICK_ACTION_BUY);
	}
}

struct LLSelectionPayable : public LLSelectedObjectFunctor
{
	bool apply(LLViewerObject* obj) override
	{
		// can pay if you or your parent has money() event in script
		LLViewerObject* parent = (LLViewerObject*)obj->getParent();
		return (obj->flagTakesMoney() ||
				(parent && parent->flagTakesMoney()));
	}
};

static U8 string_value_to_click_action(std::string p_value)
{
	if (p_value == "Touch")
		return CLICK_ACTION_TOUCH;
	if (p_value == "Sit")
		return CLICK_ACTION_SIT;
	if (p_value == "Buy")
		return CLICK_ACTION_BUY;
	if (p_value == "Pay")
		return CLICK_ACTION_PAY;
	if (p_value == "Open")
		return CLICK_ACTION_OPEN;
	if (p_value == "Zoom")
		return CLICK_ACTION_ZOOM;
	return CLICK_ACTION_TOUCH;
}

void LLSidepanelTaskInfo::onCommitClickAction(const LLSD& user_data)
{
	std::string value = user_data.asString();
	LL_INFOS() << "CLICK ACTION SELECTED: " << value << LL_ENDL;
	U8 click_action = string_value_to_click_action(value);
	doClickAction(click_action);
}

// static
void LLSidepanelTaskInfo::doClickAction(U8 click_action)
{
	if (click_action == CLICK_ACTION_BUY)
	{
		LLSaleInfo sale_info;
		LLSelectMgr::getInstance()->selectGetSaleInfo(sale_info);
		if (!sale_info.isForSale())
		{
			LLNotificationsUtil::add("CantSetBuyObject");

			// Set click action back to its old value
			click_action = 0;
			LLSelectMgr::getInstance()->selectionGetClickAction(&click_action);
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
		else
		{
			handle_give_money_dialog();
		}
	}
	LLSelectMgr::getInstance()->selectionSetClickAction(click_action);
}

void LLSidepanelTaskInfo::onCommitIncludeInSearch(const LLSD& user_data)
{
	LLSelectMgr::getInstance()->selectionSetIncludeInSearch(user_data.asBoolean());
}

// virtual
void LLSidepanelTaskInfo::updateVerbs()
{
	LLSidepanelInventorySubpanel::updateVerbs();

	/*
	mOpenBtn->setVisible(!getIsEditing());
	mPayBtn->setVisible(!getIsEditing());
	mBuyBtn->setVisible(!getIsEditing());
	//const LLViewerObject *obj = getFirstSelectedObject();
	//mEditBtn->setEnabled(obj && obj->permModify());
	*/

	LLSafeHandle<LLObjectSelection> object_selection = LLSelectMgr::getInstance()->getSelection();
	const BOOL any_selected = (object_selection->getNumNodes() > 0);

	mOpenBtn->setVisible(true);
	mPayBtn->setVisible(true);
	mBuyBtn->setVisible(true);
	mDetailsBtn->setVisible(true);

	mOpenBtn->setEnabled(enable_object_open());
	mPayBtn->setEnabled(enable_pay_object());
	mBuyBtn->setEnabled(enable_buy_object());
	mDetailsBtn->setEnabled(any_selected);
}

void LLSidepanelTaskInfo::onOpenButtonClicked()
{
	if (enable_object_open())
	{
		handle_object_open();
	}
}

void LLSidepanelTaskInfo::onPayButtonClicked()
{
	doClickAction(CLICK_ACTION_PAY);
}

void LLSidepanelTaskInfo::onBuyButtonClicked()
{
	doClickAction(CLICK_ACTION_BUY);
}

void LLSidepanelTaskInfo::onDetailsButtonClicked()
{
	LLFloaterReg::showInstance("inspect", LLSD());
}

// virtual
void LLSidepanelTaskInfo::save()
{
	onCommitGroupShare(mShareWithGroupCheck->getValue());
	onCommitEveryoneMove(mAllowEveryoneMoveCheck->getValue());
	onCommitEveryoneCopy(mAllowEveryoneCopyCheck->getValue());
	onCommitNextOwnerModify(mNextOwnerCanModifyCheck->getValue());
	onCommitNextOwnerCopy(mNextOwnerCanCopyCheck->getValue());
	onCommitNextOwnerTransfer(mNextOwnerCanTransferCheck->getValue());
	onCommitNextOwnerExport(mNextOwnerCanExportCheck->getValue());
	onCommitName(mObjectNameEditor->getValue());
	onCommitDesc(mObjectDescriptionEditor->getValue());
	setAllSaleInfo();
	onCommitIncludeInSearch(mSearchCheck->getValue());
}

// removes keyboard focus so that all fields can be updated
// and then restored focus
void LLSidepanelTaskInfo::refreshAll()
{
	// update UI as soon as we have an object
	// but remove keyboard focus first so fields are free to update
	LLFocusableElement* focus = nullptr;
	if (hasFocus())
	{
		focus = gFocusMgr.getKeyboardFocus();
		setFocus(FALSE);
	}
	refresh();
	if (focus)
	{
		focus->setFocus(TRUE);
	}
}


void LLSidepanelTaskInfo::setObjectSelection(LLObjectSelectionHandle selection)
{
	mObjectSelection = selection;
	refreshAll();
}

LLSidepanelTaskInfo* LLSidepanelTaskInfo::getActivePanel()
{
	return sActivePanel;
}

LLViewerObject* LLSidepanelTaskInfo::getObject()
{
	if (!mObject->isDead())
		return mObject;
	return nullptr;
}

LLViewerObject* LLSidepanelTaskInfo::getFirstSelectedObject()
{
	LLSelectNode *node = mObjectSelection->getFirstRootNode();
	if (node)
	{
		return node->getObject();
	}
	return nullptr;
}

const LLUUID& LLSidepanelTaskInfo::getSelectedUUID()
{
	const LLViewerObject* obj = getFirstSelectedObject();
	if (obj)
	{
		return obj->getID();
	}
	return LLUUID::null;
}

void LLSidepanelTaskInfo::closeParentFloater()
{
	LLFloater* floater = dynamic_cast<LLFloater*>(getParent());
	if (floater) floater->closeFloater();
}
