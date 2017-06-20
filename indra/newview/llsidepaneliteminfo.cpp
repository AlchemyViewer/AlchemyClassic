// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llsidepaneliteminfo.cpp
 * @brief A floater which shows an inventory item's properties.
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
#include "llsidepaneliteminfo.h"

#include "roles_constants.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "llgroupactions.h"
#include "llinventorydefines.h"
#include "llinventorymodel.h"
#include "llinventoryobserver.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "llslurl.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llexperiencecache.h"
#include "lltrans.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"

static const std::array<std::string, 21> perm_and_sale_items{{
	"perms_inv",
	"OwnerLabel",
	"perm_modify",
	"CheckOwnerModify",
	"CheckOwnerCopy",
	"CheckOwnerTransfer",
	"CheckOwnerExport",
	"GroupLabel",
	"CheckShareWithGroup",
	"AnyoneLabel",
	"CheckEveryoneCopy",
	"NextOwnerLabel",
	"CheckNextOwnerModify",
	"CheckNextOwnerCopy",
	"CheckNextOwnerTransfer",
	"CheckNextOwnerExport",
	"CheckPurchase",
	"SaleLabel",
	"ComboBoxSaleType",
	"Edit Cost",
	"TextPrice"
}};

static const std::array<std::string, 16> no_item_names{{
	"LabelItemName",
	"LabelItemDesc",
	"LabelCreatorName",
	"LabelOwnerName",
	"CheckOwnerModify",
	"CheckOwnerCopy",
	"CheckOwnerTransfer",
	"CheckOwnerExport",
	"CheckShareWithGroup",
	"CheckEveryoneCopy",
	"CheckNextOwnerModify",
	"CheckNextOwnerCopy",
	"CheckNextOwnerTransfer",
	"CheckNextOwnerExport",
	"CheckPurchase",
	"Edit Cost"
}};

static const std::array<std::string, 5> debug_items{{
	"BaseMaskDebug",
	"OwnerMaskDebug",
	"GroupMaskDebug",
	"EveryoneMaskDebug",
	"NextMaskDebug"
}};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLItemPropertiesObserver
//
// Helper class to watch for changes to the item.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLItemPropertiesObserver : public LLInventoryObserver
{
public:
	LLItemPropertiesObserver(LLSidepanelItemInfo* floater)
		: mFloater(floater)
	{
		gInventory.addObserver(this);
	}
	virtual ~LLItemPropertiesObserver()
	{
		gInventory.removeObserver(this);
	}

	void changed(U32 mask) override;
private:
	LLSidepanelItemInfo* mFloater;
};

void LLItemPropertiesObserver::changed(U32 mask)
{
	const std::set<LLUUID>& mChangedItemIDs = gInventory.getChangedIDs();

	const LLUUID& item_id = mFloater->getItemID();

	for (std::set<LLUUID>::const_iterator it = mChangedItemIDs.begin(); it != mChangedItemIDs.end(); ++it)
	{
		// set dirty for 'item profile panel' only if changed item is the item for which 'item profile panel' is shown (STORM-288)
		if (*it == item_id)
		{
			// if there's a change we're interested in.
			if((mask & (LLInventoryObserver::LABEL | LLInventoryObserver::INTERNAL | LLInventoryObserver::REMOVE)) != 0)
			{
				mFloater->dirty();
			}
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLObjectInventoryObserver
//
// Helper class to watch for changes in an object inventory.
// Used to update item properties in LLSidepanelItemInfo.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLObjectInventoryObserver : public LLVOInventoryListener
{
public:
	LLObjectInventoryObserver(LLSidepanelItemInfo* floater, LLViewerObject* object)
		: mFloater(floater)
	{
		registerVOInventoryListener(object, nullptr);
	}
	virtual ~LLObjectInventoryObserver()
	{
		removeVOInventoryListener();
	}
	/*virtual*/ void inventoryChanged(LLViewerObject* object,
									  LLInventoryObject::object_list_t* inventory,
									  S32 serial_num,
									  void* user_data) override;
private:
	LLSidepanelItemInfo* mFloater;
};

/*virtual*/
void LLObjectInventoryObserver::inventoryChanged(LLViewerObject* object,
												 LLInventoryObject::object_list_t* inventory,
												 S32 serial_num,
												 void* user_data)
{
	mFloater->dirty();
}

///----------------------------------------------------------------------------
/// Class LLSidepanelItemInfo
///----------------------------------------------------------------------------

static LLPanelInjector<LLSidepanelItemInfo> t_item_info("sidepanel_item_info");

// Default constructor
LLSidepanelItemInfo::LLSidepanelItemInfo(const LLPanel::Params& p)
	: LLSidepanelInventorySubpanel(p)
	, mItemID(LLUUID::null)
	, mObjectInventoryObserver(nullptr)
{
	mPropertiesObserver = new LLItemPropertiesObserver(this);
}

// Destroys the object
LLSidepanelItemInfo::~LLSidepanelItemInfo()
{
	delete mPropertiesObserver;
	mPropertiesObserver = nullptr;

	stopObjectInventoryObserver();
}

// virtual
BOOL LLSidepanelItemInfo::postBuild()
{
	LLSidepanelInventorySubpanel::postBuild();

	getChild<LLLineEditor>("LabelItemName")->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);
	getChild<LLUICtrl>("LabelItemName")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitName,this));
	getChild<LLLineEditor>("LabelItemDesc")->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);
	getChild<LLUICtrl>("LabelItemDesc")->setCommitCallback(boost::bind(&LLSidepanelItemInfo:: onCommitDescription, this));
	// Creator information
	getChild<LLUICtrl>("BtnCreator")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onClickCreator,this));
	// owner information
	getChild<LLUICtrl>("BtnOwner")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onClickOwner,this));
	// acquired date
	// owner permissions
	// Permissions debug text
	// group permissions
	getChild<LLUICtrl>("CheckShareWithGroup")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this));
	// everyone permissions
	getChild<LLUICtrl>("CheckEveryoneCopy")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this));
	// next owner permissions
	getChild<LLUICtrl>("CheckNextOwnerModify")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckNextOwnerCopy")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckNextOwnerTransfer")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckNextOwnerExport")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitPermissions, this));
	// Mark for sale or not, and sale info
	getChild<LLUICtrl>("CheckPurchase")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitSaleInfo, this));
	// Change sale type, and sale info
	getChild<LLUICtrl>("ComboBoxSaleType")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitSaleInfo, this));
	// "Price" label for edit
	getChild<LLUICtrl>("Edit Cost")->setCommitCallback(boost::bind(&LLSidepanelItemInfo::onCommitSaleInfo, this));
	refresh();
	return TRUE;
}

void LLSidepanelItemInfo::setObjectID(const LLUUID& object_id)
{
	mObjectID = object_id;

	// Start monitoring changes in the object inventory to update
	// selected inventory item properties in Item Profile panel. See STORM-148.
	startObjectInventoryObserver();
}

void LLSidepanelItemInfo::setItemID(const LLUUID& item_id)
{
	mItemID = item_id;
}

const LLUUID& LLSidepanelItemInfo::getObjectID() const
{
	return mObjectID;
}

const LLUUID& LLSidepanelItemInfo::getItemID() const
{
	return mItemID;
}

void LLSidepanelItemInfo::reset()
{
	LLSidepanelInventorySubpanel::reset();

	mObjectID = LLUUID::null;
	mItemID = LLUUID::null;

	stopObjectInventoryObserver();
}

void LLSidepanelItemInfo::refresh()
{
	LLViewerInventoryItem* itemp = findItem();
	if(itemp)
	{
		refreshFromItem(itemp);
		updateVerbs();
		return;
	}
	if (getIsEditing())
	{
		setIsEditing(FALSE);
	}

	if (!getIsEditing())
	{
		for(const std::string item : no_item_names)
		{
			getChildView(item)->setEnabled(false);
		}

		for(const std::string item : debug_items)
		{
			getChildView(item)->setVisible(false);
		}
	}

	if (!itemp)
	{
		getChildView("BtnCreator")->setEnabled(false);
		getChildView("BtnOwner")->setEnabled(false);
	}

	updateVerbs();
}

void LLSidepanelItemInfo::refreshFromItem(LLViewerInventoryItem* item)
{
	////////////////////////
	// PERMISSIONS LOOKUP //
	////////////////////////

	llassert(item);
	if (!item) return;

	// do not enable the UI for incomplete items.
	BOOL is_complete = item->isFinished();
	const BOOL cannot_restrict_permissions = LLInventoryType::cannotRestrictPermissions(item->getInventoryType());
	const BOOL is_calling_card = (item->getInventoryType() == LLInventoryType::IT_CALLINGCARD);
	const LLPermissions& perm = item->getPermissions();
	const BOOL can_agent_manipulate = gAgent.allowOperation(PERM_OWNER, perm, 
								GP_OBJECT_MANIPULATE);
	const BOOL can_agent_sell = gAgent.allowOperation(PERM_OWNER, perm, 
							  GP_OBJECT_SET_SALE) &&
		!cannot_restrict_permissions;
	const BOOL is_link = item->getIsLinkType();
	
	const LLUUID trash_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
	bool not_in_trash = (item->getUUID() != trash_id) && !gInventory.isObjectDescendentOf(item->getUUID(), trash_id);

	// You need permission to modify the object to modify an inventory
	// item in it.
	LLViewerObject* object = nullptr;
	if(!mObjectID.isNull()) object = gObjectList.findObject(mObjectID);
	BOOL is_obj_modify = TRUE;
	if(object)
	{
		is_obj_modify = object->permOwnerModify();
	}

    if(item->getInventoryType() == LLInventoryType::IT_LSL)
    {
        getChildView("LabelItemExperienceTitle")->setVisible(TRUE);
        LLTextBox* tb = getChild<LLTextBox>("LabelItemExperience");
        tb->setText(getString("loading_experience"));
        tb->setVisible(TRUE);
        std::string url = std::string();
        if(object && object->getRegion())
        {
            url = object->getRegion()->getCapability("GetMetadata");
        }
        LLExperienceCache::instance().fetchAssociatedExperience(item->getParentUUID(), item->getUUID(), url,
                boost::bind(&LLSidepanelItemInfo::setAssociatedExperience, getDerivedHandle<LLSidepanelItemInfo>(), _1));
    }
    
	//////////////////////
	// ITEM NAME & DESC //
	//////////////////////
	BOOL is_modifiable = gAgent.allowOperation(PERM_MODIFY, perm,
											   GP_OBJECT_MANIPULATE)
		&& is_obj_modify && is_complete && not_in_trash;

	getChildView("LabelItemNameTitle")->setEnabled(TRUE);
	getChildView("LabelItemName")->setEnabled(is_modifiable && !is_calling_card); // for now, don't allow rename of calling cards
	getChild<LLUICtrl>("LabelItemName")->setValue(item->getName());
	getChildView("LabelItemDescTitle")->setEnabled(TRUE);
	getChildView("LabelItemDesc")->setEnabled(is_modifiable);
	getChildView("IconLocked")->setVisible(!is_modifiable);
	getChild<LLUICtrl>("LabelItemDesc")->setValue(item->getDescription());

	//////////////////
	// CREATOR NAME //
	//////////////////
	if(!gCacheName) return;
	if(!gAgent.getRegion()) return;

	if (item->getCreatorUUID().notNull())
	{
		LLUUID creator_id = item->getCreatorUUID();
		std::string name =
			LLSLURL("agent", creator_id, "completename").getSLURLString();
		getChildView("BtnCreator")->setEnabled(TRUE);
		getChildView("LabelCreatorTitle")->setEnabled(TRUE);
		getChildView("LabelCreatorName")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelCreatorName")->setValue(name);
	}
	else
	{
		getChildView("BtnCreator")->setEnabled(FALSE);
		getChildView("LabelCreatorTitle")->setEnabled(FALSE);
		getChildView("LabelCreatorName")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelCreatorName")->setValue(getString("unknown_multiple"));
	}

	////////////////
	// OWNER NAME //
	////////////////
	if(perm.isOwned())
	{
		std::string name;
		if (perm.isGroupOwned())
		{
			gCacheName->getGroupName(perm.getGroup(), name);
		}
		else
		{
			LLUUID owner_id = perm.getOwner();
			name = LLSLURL("agent", owner_id, "completename").getSLURLString();
		}
		getChildView("BtnOwner")->setEnabled(TRUE);
		getChildView("LabelOwnerTitle")->setEnabled(TRUE);
		getChildView("LabelOwnerName")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelOwnerName")->setValue(name);
	}
	else
	{
		getChildView("BtnOwner")->setEnabled(FALSE);
		getChildView("LabelOwnerTitle")->setEnabled(FALSE);
		getChildView("LabelOwnerName")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelOwnerName")->setValue(getString("public"));
	}
	
	////////////
	// ORIGIN //
	////////////

	getChild<LLUICtrl>("origin")->setValue(getString(object ? "origin_inworld" : "origin_inventory"));

	//////////////////
	// ACQUIRE DATE //
	//////////////////
	
	time_t time_utc = item->getCreationDate();
	if (0 == time_utc)
	{
		getChild<LLUICtrl>("LabelAcquiredDate")->setValue(getString("unknown"));
	}
	else
	{
		std::string timeStr = getString("acquiredDate");
		LLSD substitution;
		substitution["datetime"] = (S32) time_utc;
		LLStringUtil::format (timeStr, substitution);
		getChild<LLUICtrl>("LabelAcquiredDate")->setValue(timeStr);
	}
	
	//////////////////////////////////////
	// PERMISSIONS AND SALE ITEM HIDING //
	//////////////////////////////////////
	
	// Hide permissions checkboxes and labels and for sale info if in the trash
	// or ui elements don't apply to these objects and return from function
	if (!not_in_trash || cannot_restrict_permissions)
	{
		for(const std::string item : perm_and_sale_items)
		{
			getChildView(item)->setVisible(false);
		}
		
		for(const std::string item : debug_items)
		{
			getChildView(item)->setVisible(false);
		}
		return;
	}
	else // Make sure perms and sale ui elements are visible
	{
		for(const std::string item : perm_and_sale_items)
		{
			getChildView(item)->setVisible(true);
		}
	}

	///////////////////////
	// OWNER PERMISSIONS //
	///////////////////////
	if(can_agent_manipulate)
	{
		getChild<LLUICtrl>("OwnerLabel")->setValue(getString("you_can"));
	}
	else
	{
		getChild<LLUICtrl>("OwnerLabel")->setValue(getString("owner_can"));
	}

	U32 base_mask		= perm.getMaskBase();
	U32 owner_mask		= perm.getMaskOwner();
	U32 group_mask		= perm.getMaskGroup();
	U32 everyone_mask	= perm.getMaskEveryone();
	U32 next_owner_mask	= perm.getMaskNextOwner();

	getChildView("OwnerLabel")->setEnabled(TRUE);
	getChildView("CheckOwnerModify")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerModify")->setValue(LLSD((BOOL)(owner_mask & PERM_MODIFY)));
	getChildView("CheckOwnerCopy")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerCopy")->setValue(LLSD((BOOL)(owner_mask & PERM_COPY)));
	getChildView("CheckOwnerTransfer")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerTransfer")->setValue(LLSD((BOOL)(owner_mask & PERM_TRANSFER)));
	getChildView("CheckOwnerExport")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerExport")->setValue(LLSD((BOOL)(owner_mask & PERM_EXPORT)));
	getChildView("CheckOwnerExport")->setVisible(!LLGridManager::getInstance()->isInSecondlife());

	///////////////////////
	// DEBUG PERMISSIONS //
	///////////////////////

	if( gSavedSettings.getBOOL("DebugPermissions") )
	{
		BOOL slam_perm 			= FALSE;
		BOOL overwrite_group	= FALSE;
		BOOL overwrite_everyone	= FALSE;

		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = item->getFlags();
			slam_perm 			= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
			overwrite_everyone	= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
			overwrite_group		= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
		}
		
		std::string perm_string;

		perm_string = "B: ";
		perm_string += mask_to_string(base_mask);
		getChild<LLUICtrl>("BaseMaskDebug")->setValue(perm_string);
		getChildView("BaseMaskDebug")->setVisible(TRUE);
		
		perm_string = "O: ";
		perm_string += mask_to_string(owner_mask);
		getChild<LLUICtrl>("OwnerMaskDebug")->setValue(perm_string);
		getChildView("OwnerMaskDebug")->setVisible(TRUE);
		
		perm_string = "G";
		perm_string += overwrite_group ? "*: " : ": ";
		perm_string += mask_to_string(group_mask);
		getChild<LLUICtrl>("GroupMaskDebug")->setValue(perm_string);
		getChildView("GroupMaskDebug")->setVisible(TRUE);
		
		perm_string = "E";
		perm_string += overwrite_everyone ? "*: " : ": ";
		perm_string += mask_to_string(everyone_mask);
		getChild<LLUICtrl>("EveryoneMaskDebug")->setValue(perm_string);
		getChildView("EveryoneMaskDebug")->setVisible(TRUE);
		
		perm_string = "N";
		perm_string += slam_perm ? "*: " : ": ";
		perm_string += mask_to_string(next_owner_mask);
		getChild<LLUICtrl>("NextMaskDebug")->setValue(perm_string);
		getChildView("NextMaskDebug")->setVisible(TRUE);
	}
	else
	{
		getChildView("BaseMaskDebug")->setVisible(FALSE);
		getChildView("OwnerMaskDebug")->setVisible(FALSE);
		getChildView("GroupMaskDebug")->setVisible(FALSE);
		getChildView("EveryoneMaskDebug")->setVisible(FALSE);
		getChildView("NextMaskDebug")->setVisible(FALSE);
	}

	/////////////
	// SHARING //
	/////////////

	// Check for ability to change values.
	if (is_link || cannot_restrict_permissions)
	{
		getChildView("CheckShareWithGroup")->setEnabled(FALSE);
		getChildView("CheckEveryoneCopy")->setEnabled(FALSE);
	}
	else if (is_obj_modify && can_agent_manipulate)
	{
		getChildView("CheckShareWithGroup")->setEnabled(TRUE);
		getChildView("CheckEveryoneCopy")->setEnabled((owner_mask & PERM_COPY) && (owner_mask & PERM_TRANSFER));
	}
	else
	{
		getChildView("CheckShareWithGroup")->setEnabled(FALSE);
		getChildView("CheckEveryoneCopy")->setEnabled(FALSE);
	}

	// Set values.
	BOOL is_group_copy = (group_mask & PERM_COPY) ? TRUE : FALSE;
	BOOL is_group_modify = (group_mask & PERM_MODIFY) ? TRUE : FALSE;
	BOOL is_group_move = (group_mask & PERM_MOVE) ? TRUE : FALSE;

	if (is_group_copy && is_group_modify && is_group_move)
	{
		getChild<LLUICtrl>("CheckShareWithGroup")->setValue(LLSD(TRUE));

		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(FALSE);
		}
	}
	else if (!is_group_copy && !is_group_modify && !is_group_move)
	{
		getChild<LLUICtrl>("CheckShareWithGroup")->setValue(LLSD(FALSE));
		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(FALSE);
		}
	}
	else
	{
		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(TRUE);
			ctl->set(TRUE);
		}
	}
	
	getChild<LLUICtrl>("CheckEveryoneCopy")->setValue(LLSD((BOOL)(everyone_mask & PERM_COPY)));

	///////////////
	// SALE INFO //
	///////////////

	const LLSaleInfo& sale_info = item->getSaleInfo();
	BOOL is_for_sale = sale_info.isForSale();
	LLComboBox* combo_sale_type = getChild<LLComboBox>("ComboBoxSaleType");
	LLUICtrl* edit_cost = getChild<LLUICtrl>("Edit Cost");

	// Check for ability to change values.
	if (is_obj_modify && can_agent_sell 
		&& gAgent.allowOperation(PERM_TRANSFER, perm, GP_OBJECT_MANIPULATE))
	{
		getChildView("SaleLabel")->setEnabled(is_complete);
		getChildView("CheckPurchase")->setEnabled(is_complete);

		getChildView("NextOwnerLabel")->setEnabled(TRUE);
		getChildView("CheckNextOwnerModify")->setEnabled((base_mask & PERM_MODIFY) && !cannot_restrict_permissions);
		getChildView("CheckNextOwnerCopy")->setEnabled((base_mask & PERM_COPY) && !cannot_restrict_permissions);
		getChildView("CheckNextOwnerTransfer")->setEnabled((next_owner_mask & PERM_COPY) && !cannot_restrict_permissions);
		getChildView("CheckNextOwnerExport")->setEnabled((base_mask & PERM_EXPORT) && !cannot_restrict_permissions);

		getChildView("TextPrice")->setEnabled(is_complete && is_for_sale);
		combo_sale_type->setEnabled(is_complete && is_for_sale);
		edit_cost->setEnabled(is_complete && is_for_sale);
	}
	else
	{
		getChildView("SaleLabel")->setEnabled(FALSE);
		getChildView("CheckPurchase")->setEnabled(FALSE);

		getChildView("NextOwnerLabel")->setEnabled(FALSE);
		getChildView("CheckNextOwnerModify")->setEnabled(FALSE);
		getChildView("CheckNextOwnerCopy")->setEnabled(FALSE);
		getChildView("CheckNextOwnerTransfer")->setEnabled(FALSE);
		getChildView("CheckNextOwnerExport")->setEnabled(FALSE);

		getChildView("TextPrice")->setEnabled(FALSE);
		combo_sale_type->setEnabled(FALSE);
		edit_cost->setEnabled(FALSE);
	}

	// Set values.
	getChild<LLUICtrl>("CheckPurchase")->setValue(is_for_sale);
	getChild<LLUICtrl>("CheckNextOwnerModify")->setValue(LLSD(BOOL(next_owner_mask & PERM_MODIFY)));
	getChild<LLUICtrl>("CheckNextOwnerCopy")->setValue(LLSD(BOOL(next_owner_mask & PERM_COPY)));
	getChild<LLUICtrl>("CheckNextOwnerTransfer")->setValue(LLSD(BOOL(next_owner_mask & PERM_TRANSFER)));
	getChild<LLUICtrl>("CheckNextOwnerExport")->setValue(LLSD(BOOL(next_owner_mask & PERM_EXPORT)));

	if (is_for_sale)
	{
		S32 numerical_price = sale_info.getSalePrice();
		edit_cost->setValue(llformat("%d",numerical_price));
		combo_sale_type->setValue(sale_info.getSaleType());
	}
	else
	{
		edit_cost->setValue("0");
		combo_sale_type->setValue(LLSaleInfo::FS_COPY);
	}
}


void LLSidepanelItemInfo::setAssociatedExperience( LLHandle<LLSidepanelItemInfo> hInfo, const LLSD& experience )
{
    LLSidepanelItemInfo* info = hInfo.get();
    if(info)
    {
        LLUUID id;
        if(experience.has(LLExperienceCache::EXPERIENCE_ID))
        {
            id=experience[LLExperienceCache::EXPERIENCE_ID].asUUID();
        }
        if(id.notNull())
        {
            info->getChild<LLTextBox>("LabelItemExperience")->setText(LLSLURL("experience", id, "profile").getSLURLString());    
        }
        else
        {
            info->getChild<LLTextBox>("LabelItemExperience")->setText(LLTrans::getString("ExperienceNameNull"));
        }
    }
}


void LLSidepanelItemInfo::startObjectInventoryObserver()
{
	if (!mObjectInventoryObserver)
	{
		stopObjectInventoryObserver();

		// Previous object observer should be removed before starting to observe a new object.
		llassert(mObjectInventoryObserver == NULL);
	}

	if (mObjectID.isNull())
	{
		LL_WARNS() << "Empty object id passed to inventory observer" << LL_ENDL;
		return;
	}

	LLViewerObject* object = gObjectList.findObject(mObjectID);

	mObjectInventoryObserver = new LLObjectInventoryObserver(this, object);
}

void LLSidepanelItemInfo::stopObjectInventoryObserver()
{
	delete mObjectInventoryObserver;
	mObjectInventoryObserver = nullptr;
}

void LLSidepanelItemInfo::onClickCreator()
{
	LLViewerInventoryItem* item = findItem();
	if(!item) return;
	if(!item->getCreatorUUID().isNull())
	{
		LLAvatarActions::showProfile(item->getCreatorUUID());
	}
}

// static
void LLSidepanelItemInfo::onClickOwner()
{
	LLViewerInventoryItem* item = findItem();
	if(!item) return;
	if(item->getPermissions().isGroupOwned())
	{
		LLGroupActions::show(item->getPermissions().getGroup());
	}
	else
	{
		LLAvatarActions::showProfile(item->getPermissions().getOwner());
	}
}

// static
void LLSidepanelItemInfo::onCommitName()
{
	//LL_INFOS() << "LLSidepanelItemInfo::onCommitName()" << LL_ENDL;
	LLViewerInventoryItem* item = findItem();
	if(!item)
	{
		return;
	}
	LLLineEditor* labelItemName = getChild<LLLineEditor>("LabelItemName");

	if(labelItemName&&
	   (item->getName() != labelItemName->getText()) && 
	   (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE)) )
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->rename(labelItemName->getText());
		onCommitChanges(new_item);
	}
}

void LLSidepanelItemInfo::onCommitDescription()
{
	//LL_INFOS() << "LLSidepanelItemInfo::onCommitDescription()" << LL_ENDL;
	LLViewerInventoryItem* item = findItem();
	if(!item) return;

	LLLineEditor* labelItemDesc = getChild<LLLineEditor>("LabelItemDesc");
	if(!labelItemDesc)
	{
		return;
	}
	if((item->getDescription() != labelItemDesc->getText()) && 
	   (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE)))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		new_item->setDescription(labelItemDesc->getText());
		onCommitChanges(new_item);
	}
}

// static
void LLSidepanelItemInfo::onCommitPermissions()
{
	//LL_INFOS() << "LLSidepanelItemInfo::onCommitPermissions()" << LL_ENDL;
	LLViewerInventoryItem* item = findItem();
	if(!item) return;

	BOOL is_group_owned;
	LLUUID owner_id;
	LLUUID group_id;
	LLPermissions perm(item->getPermissions());
	perm.getOwnership(owner_id, is_group_owned);

	if (is_group_owned && gAgent.hasPowerInGroup(owner_id, GP_OBJECT_MANIPULATE))
	{
		group_id = owner_id;
	}

	LLCheckBoxCtrl* CheckShareWithGroup = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");

	if(CheckShareWithGroup)
	{
		perm.setGroupBits(gAgent.getID(), group_id,
						CheckShareWithGroup->get(),
						PERM_MODIFY | PERM_MOVE | PERM_COPY);
	}
	LLCheckBoxCtrl* CheckEveryoneCopy = getChild<LLCheckBoxCtrl>("CheckEveryoneCopy");
	if(CheckEveryoneCopy)
	{
		perm.setEveryoneBits(gAgent.getID(), group_id,
						 CheckEveryoneCopy->get(), PERM_COPY);
	}

	LLCheckBoxCtrl* CheckNextOwnerModify = getChild<LLCheckBoxCtrl>("CheckNextOwnerModify");
	if(CheckNextOwnerModify)
	{
		perm.setNextOwnerBits(gAgent.getID(), group_id,
							CheckNextOwnerModify->get(), PERM_MODIFY);
	}
	LLCheckBoxCtrl* CheckNextOwnerCopy = getChild<LLCheckBoxCtrl>("CheckNextOwnerCopy");
	if(CheckNextOwnerCopy)
	{
		perm.setNextOwnerBits(gAgent.getID(), group_id,
							CheckNextOwnerCopy->get(), PERM_COPY);
	}
	LLCheckBoxCtrl* CheckNextOwnerTransfer = getChild<LLCheckBoxCtrl>("CheckNextOwnerTransfer");
	if(CheckNextOwnerTransfer)
	{
		perm.setNextOwnerBits(gAgent.getID(), group_id,
							CheckNextOwnerTransfer->get(), PERM_TRANSFER);
	}
	LLCheckBoxCtrl* CheckNextOwnerExport = getChild<LLCheckBoxCtrl>("CheckNextOwnerExport");
	if(CheckNextOwnerExport)
	{
		perm.setNextOwnerBits(gAgent.getID(), gAgent.getGroupID(),
							  CheckNextOwnerExport->get(), PERM_EXPORT);
	}
	if(perm != item->getPermissions()
		&& item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setPermissions(perm);
		U32 flags = new_item->getFlags();
		// If next owner permissions have changed (and this is an object)
		// then set the slam permissions flag so that they are applied on rez.
		if((perm.getMaskNextOwner()!=item->getPermissions().getMaskNextOwner())
		   && (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
		}
		// If everyone permissions have changed (and this is an object)
		// then set the overwrite everyone permissions flag so they
		// are applied on rez.
		if ((perm.getMaskEveryone()!=item->getPermissions().getMaskEveryone())
			&& (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
		}
		// If group permissions have changed (and this is an object)
		// then set the overwrite group permissions flag so they
		// are applied on rez.
		if ((perm.getMaskGroup()!=item->getPermissions().getMaskGroup())
			&& (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
		}
		new_item->setFlags(flags);
		onCommitChanges(new_item);
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

// static
void LLSidepanelItemInfo::onCommitSaleInfo()
{
	//LL_INFOS() << "LLSidepanelItemInfo::onCommitSaleInfo()" << LL_ENDL;
	updateSaleInfo();
}

// static
void LLSidepanelItemInfo::onCommitSaleType()
{
	//LL_INFOS() << "LLSidepanelItemInfo::onCommitSaleType()" << LL_ENDL;
	updateSaleInfo();
}

void LLSidepanelItemInfo::updateSaleInfo()
{
	LLViewerInventoryItem* item = findItem();
	if(!item) return;
	LLSaleInfo sale_info(item->getSaleInfo());
	if(!gAgent.allowOperation(PERM_TRANSFER, item->getPermissions(), GP_OBJECT_SET_SALE))
	{
		getChild<LLUICtrl>("CheckPurchase")->setValue(FALSE);
	}

	if(getChild<LLUICtrl>("CheckPurchase")->getValue().asBoolean())
	{
		// turn on sale info
		LLSaleInfo::EForSale sale_type = LLSaleInfo::FS_COPY;
	
		LLComboBox* combo_sale_type = getChild<LLComboBox>("ComboBoxSaleType");
		if (combo_sale_type)
		{
			sale_type = static_cast<LLSaleInfo::EForSale>(combo_sale_type->getValue().asInteger());
		}

		if (sale_type == LLSaleInfo::FS_COPY 
			&& !gAgent.allowOperation(PERM_COPY, item->getPermissions(), 
									  GP_OBJECT_SET_SALE))
		{
			sale_type = LLSaleInfo::FS_ORIGINAL;
		}

	     
		
		S32 price = -1;
		price =  getChild<LLUICtrl>("Edit Cost")->getValue().asInteger();;

		// Invalid data - turn off the sale
		if (price < 0)
		{
			sale_type = LLSaleInfo::FS_NOT;
			price = 0;
		}

		sale_info.setSaleType(sale_type);
		sale_info.setSalePrice(price);
	}
	else
	{
		sale_info.setSaleType(LLSaleInfo::FS_NOT);
	}
	if(sale_info != item->getSaleInfo()
		&& item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		// Force an update on the sale price at rez
		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = new_item->getFlags();
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_SALE;
			new_item->setFlags(flags);
		}

		new_item->setSaleInfo(sale_info);
		onCommitChanges(new_item);
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

void LLSidepanelItemInfo::onCommitChanges(LLPointer<LLViewerInventoryItem> item)
{
    if (item.isNull())
    {
        return;
    }

    if (mObjectID.isNull())
    {
        // This is in the agent's inventory.
        item->updateServer(FALSE);
        gInventory.updateItem(item);
        gInventory.notifyObservers();
    }
    else
    {
        // This is in an object's contents.
        LLViewerObject* object = gObjectList.findObject(mObjectID);
        if (object)
        {
            object->updateInventory(
                item,
                TASK_INVENTORY_ITEM_KEY,
                false);

            if (object->isSelected())
            {
                // Since object is selected (build floater is open) object will
                // receive properties update, detect serial mismatch, dirty and
                // reload inventory, meanwhile some other updates will refresh it.
                // So mark dirty early, this will prevent unnecessary changes
                // and download will be triggered by LLPanelObjectInventory - it
                // prevents flashing in content tab and some duplicated request.
                object->dirtyInventory();
            }
        }
    }
}

LLViewerInventoryItem* LLSidepanelItemInfo::findItem() const
{
	LLViewerInventoryItem* item = nullptr;
	if(mObjectID.isNull())
	{
		// it is in agent inventory
		item = gInventory.getItem(mItemID);
	}
	else
	{
		LLViewerObject* object = gObjectList.findObject(mObjectID);
		if(object)
		{
			item = static_cast<LLViewerInventoryItem*>(object->getInventoryObject(mItemID));
		}
	}
	return item;
}

// virtual
void LLSidepanelItemInfo::save()
{
	onCommitName();
	onCommitDescription();
	onCommitPermissions();
	onCommitSaleInfo();
	onCommitSaleType();
}
