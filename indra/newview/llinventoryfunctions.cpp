/** 
 * @file llinventoryfunctions.cpp
 * @brief Implementation of the inventory view and associated stuff.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include <utility> // for std::pair<>

#include "llinventoryfunctions.h"

// library includes
#include "llagent.h"
#include "llagentwearables.h"
#include "llcallingcard.h"
#include "llfloaterreg.h"
#include "llinventorydefines.h"
#include "llsdserialize.h"
#include "llfiltereditor.h"
#include "llspinctrl.h"
#include "llui.h"
#include "message.h"

// newview includes
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llclipboard.h"
#include "lldonotdisturbnotificationstorage.h"
#include "llfloaterinventory.h"
#include "llfloatersidepanelcontainer.h"
#include "llfocusmgr.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "lliconctrl.h"
#include "llimview.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "lllineeditor.h"
#include "llmarketplacenotifications.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llpanelmaininventory.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "llresmgr.h"
#include "llscrollbar.h"
#include "llscrollcontainer.h"
#include "llselectmgr.h"
#include "llsidepanelinventory.h"
#include "lltabcontainer.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llviewermessage.h"
#include "llviewerfoldertype.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llwearablelist.h"

#include <boost/foreach.hpp>

BOOL LLInventoryState::sWearNewClothing = FALSE;
LLUUID LLInventoryState::sWearNewClothingTransactionID;

// Generates a string containing the path to the item specified by
// item_id.
void append_path(const LLUUID& id, std::string& path)
{
	std::string temp;
	const LLInventoryObject* obj = gInventory.getObject(id);
	LLUUID parent_id;
	if(obj) parent_id = obj->getParentUUID();
	std::string forward_slash("/");
	while(obj)
	{
		obj = gInventory.getCategory(parent_id);
		if(obj)
		{
			temp.assign(forward_slash + obj->getName() + temp);
			parent_id = obj->getParentUUID();
		}
	}
	path.append(temp);
}

void rename_category(LLInventoryModel* model, const LLUUID& cat_id, const std::string& new_name)
{
	LLViewerInventoryCategory* cat;

	if (!model ||
		!get_is_category_renameable(model, cat_id) ||
		(cat = model->getCategory(cat_id)) == NULL ||
		cat->getName() == new_name)
	{
		return;
	}

	LLPointer<LLViewerInventoryCategory> new_cat = new LLViewerInventoryCategory(cat);
	new_cat->rename(new_name);
	new_cat->updateServer(FALSE);
	model->updateCategory(new_cat);

	model->notifyObservers();
}

void copy_inventory_category(LLInventoryModel* model,
							 LLViewerInventoryCategory* cat,
							 const LLUUID& parent_id,
							 const LLUUID& root_copy_id)
{
	// Create the initial folder
	LLUUID new_cat_uuid = gInventory.createNewCategory(parent_id, LLFolderType::FT_NONE, cat->getName());
	model->notifyObservers();
	
	// We need to exclude the initial root of the copy to avoid recursively copying the copy, etc...
	LLUUID root_id = (root_copy_id.isNull() ? new_cat_uuid : root_copy_id);

	// Get the content of the folder
	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(cat->getUUID(),cat_array,item_array);

	// Copy all the items
	LLInventoryModel::item_array_t item_array_copy = *item_array;
	for (LLInventoryModel::item_array_t::iterator iter = item_array_copy.begin(); iter != item_array_copy.end(); iter++)
	{
		LLInventoryItem* item = *iter;
		copy_inventory_item(
							gAgent.getID(),
							item->getPermissions().getOwner(),
							item->getUUID(),
							new_cat_uuid,
							std::string(),
							LLPointer<LLInventoryCallback>(NULL));
	}
	
	// Copy all the folders
	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;
	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLViewerInventoryCategory* category = *iter;
		if (category->getUUID() != root_id)
		{
			copy_inventory_category(model, category, new_cat_uuid, root_id);
		}
	}
}

class LLInventoryCollectAllItems : public LLInventoryCollectFunctor
{
public:
	virtual bool operator()(LLInventoryCategory* cat, LLInventoryItem* item)
	{
		return true;
	}
};

BOOL get_is_parent_to_worn_item(const LLUUID& id)
{
	const LLViewerInventoryCategory* cat = gInventory.getCategory(id);
	if (!cat)
	{
		return FALSE;
	}

	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLInventoryCollectAllItems collect_all;
	gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), cats, items, LLInventoryModel::EXCLUDE_TRASH, collect_all);

	for (LLInventoryModel::item_array_t::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		const LLViewerInventoryItem * const item = *it;

		llassert(item->getIsLinkType());

		LLUUID linked_id = item->getLinkedUUID();
		const LLViewerInventoryItem * const linked_item = gInventory.getItem(linked_id);

		if (linked_item)
		{
			LLUUID parent_id = linked_item->getParentUUID();

			while (!parent_id.isNull())
			{
				LLInventoryCategory * parent_cat = gInventory.getCategory(parent_id);

				if (cat == parent_cat)
				{
					return TRUE;
				}

				parent_id = parent_cat->getParentUUID();
			}
		}
	}

	return FALSE;
}

BOOL get_is_item_worn(const LLUUID& id)
{
	const LLViewerInventoryItem* item = gInventory.getItem(id);
	if (!item)
		return FALSE;

	// Consider the item as worn if it has links in COF.
	if (LLAppearanceMgr::instance().isLinkInCOF(id))
	{
		return TRUE;
	}

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		{
			if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(item->getLinkedUUID()))
				return TRUE;
			break;
		}
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if(gAgentWearables.isWearingItem(item->getLinkedUUID()))
				return TRUE;
			break;
		case LLAssetType::AT_GESTURE:
			if (LLGestureMgr::instance().isGestureActive(item->getLinkedUUID()))
				return TRUE;
			break;
		default:
			break;
	}
	return FALSE;
}

BOOL get_can_item_be_worn(const LLUUID& id)
{
	const LLViewerInventoryItem* item = gInventory.getItem(id);
	if (!item)
		return FALSE;

	if (LLAppearanceMgr::isLinkInCOF(item->getLinkedUUID()))
	{
		// an item having links in COF (i.e. a worn item)
		return FALSE;
	}

	if (gInventory.isObjectDescendentOf(id, LLAppearanceMgr::instance().getCOF()))
	{
		// a non-link object in COF (should not normally happen)
		return FALSE;
	}
	
	const LLUUID trash_id = gInventory.findCategoryUUIDForType(
			LLFolderType::FT_TRASH);

	// item can't be worn if base obj in trash, see EXT-7015
	if (gInventory.isObjectDescendentOf(item->getLinkedUUID(),
			trash_id))
	{
		return false;
	}

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		{
			if (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(item->getLinkedUUID()))
			{
				// Already being worn
				return FALSE;
			}
			else
			{
				// Not being worn yet.
				return TRUE;
			}
			break;
		}
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if(gAgentWearables.isWearingItem(item->getLinkedUUID()))
			{
				// Already being worn
				return FALSE;
			}
			else
			{
				// Not being worn yet.
				return TRUE;
			}
			break;
		default:
			break;
	}
	return FALSE;
}

BOOL get_is_item_removable(const LLInventoryModel* model, const LLUUID& id)
{
	if (!model)
	{
		return FALSE;
	}

	// Can't delete an item that's in the library.
	if (!model->isObjectDescendentOf(id, gInventory.getRootFolderID()))
	{
		return FALSE;
	}

	// Disable delete from COF folder; have users explicitly choose "detach/take off",
	// unless the item is not worn but in the COF (i.e. is bugged).
	if (LLAppearanceMgr::instance().getIsProtectedCOFItem(id))
	{
		if (get_is_item_worn(id))
		{
			return FALSE;
		}
	}

	const LLInventoryObject *obj = model->getItem(id);
	if (obj && obj->getIsLinkType())
	{
		return TRUE;
	}
	if (get_is_item_worn(id))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL get_is_category_removable(const LLInventoryModel* model, const LLUUID& id)
{
	// NOTE: This function doesn't check the folder's children.
	// See LLFolderBridge::isItemRemovable for a function that does
	// consider the children.

	if (!model)
	{
		return FALSE;
	}

	if (!model->isObjectDescendentOf(id, gInventory.getRootFolderID()))
	{
		return FALSE;
	}

	if (!isAgentAvatarValid()) return FALSE;

	const LLInventoryCategory* category = model->getCategory(id);
	if (!category)
	{
		return FALSE;
	}

	const LLFolderType::EType folder_type = category->getPreferredType();
	
	if (LLFolderType::lookupIsProtectedType(folder_type))
	{
		return FALSE;
	}

	// Can't delete the outfit that is currently being worn.
	if (folder_type == LLFolderType::FT_OUTFIT)
	{
		const LLViewerInventoryItem *base_outfit_link = LLAppearanceMgr::instance().getBaseOutfitLink();
		if (base_outfit_link && (category == base_outfit_link->getLinkedCategory()))
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL get_is_category_renameable(const LLInventoryModel* model, const LLUUID& id)
{
	if (!model)
	{
		return FALSE;
	}

	LLViewerInventoryCategory* cat = model->getCategory(id);

	if (cat && !LLFolderType::lookupIsProtectedType(cat->getPreferredType()) &&
		cat->getOwnerID() == gAgent.getID())
	{
		return TRUE;
	}
	return FALSE;
}

void show_task_item_profile(const LLUUID& item_uuid, const LLUUID& object_id)
{
	LLFloaterSidePanelContainer::showPanel("inventory", LLSD().with("id", item_uuid).with("object", object_id));
}

void show_item_profile(const LLUUID& item_uuid)
{
	LLUUID linked_uuid = gInventory.getLinkedItemID(item_uuid);
	LLFloaterSidePanelContainer::showPanel("inventory", LLSD().with("id", linked_uuid));
}

void show_item_original(const LLUUID& item_uuid)
{
	LLFloater* floater_inventory = LLFloaterReg::getInstance("inventory");
	if (!floater_inventory)
	{
		llwarns << "Could not find My Inventory floater" << llendl;
		return;
	}

	//sidetray inventory panel
	LLSidepanelInventory *sidepanel_inventory =	LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");

	LLInventoryPanel* active_panel = LLInventoryPanel::getActiveInventoryPanel();
	if (!active_panel) 
	{
		//this may happen when there is no floatera and other panel is active in inventory tab

		if	(sidepanel_inventory)
		{
			sidepanel_inventory->showInventoryPanel();
		}
	}
	
	active_panel = LLInventoryPanel::getActiveInventoryPanel();
	if (!active_panel) 
	{
		return;
	}
	active_panel->setSelection(gInventory.getLinkedItemID(item_uuid), TAKE_FOCUS_NO);
	
	// Reset filters
	reset_inventory_filter();
}


void reset_inventory_filter()
{
	//inventory floater
	bool floater_inventory_visible = false;

	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("inventory");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin(); iter != inst_list.end(); ++iter)
	{
		LLFloaterInventory* floater_inventory = dynamic_cast<LLFloaterInventory*>(*iter);
		if (floater_inventory)
		{
			LLPanelMainInventory* main_inventory = floater_inventory->getMainInventoryPanel();

			main_inventory->onFilterEdit("");

			if(floater_inventory->getVisible())
			{
				floater_inventory_visible = true;
			}
		}
	}

	if(!floater_inventory_visible)
	{
		LLSidepanelInventory *sidepanel_inventory =	LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
		if (sidepanel_inventory)
		{
			LLPanelMainInventory* main_inventory = sidepanel_inventory->getMainInventoryPanel();
			if (main_inventory)
			{
				main_inventory->onFilterEdit("");
			}
		}
	}
}

void open_outbox()
{
	LLFloaterReg::showInstance("outbox");
}

LLUUID create_folder_in_outbox_for_item(LLInventoryItem* item, const LLUUID& destFolderId, S32 operation_id)
{
	llassert(item);
	llassert(destFolderId.notNull());

	LLUUID created_folder_id = gInventory.createNewCategory(destFolderId, LLFolderType::FT_NONE, item->getName());
	gInventory.notifyObservers();

	LLNotificationsUtil::add("OutboxFolderCreated");

	return created_folder_id;
}

void move_to_outbox_cb_action(const LLSD& payload)
{
	LLViewerInventoryItem * viitem = gInventory.getItem(payload["item_id"].asUUID());
	LLUUID dest_folder_id = payload["dest_folder_id"].asUUID();

	if (viitem)
	{	
		// when moving item directly into outbox create folder with that name
		if (dest_folder_id == gInventory.findCategoryUUIDForType(LLFolderType::FT_OUTBOX, false))
		{
			S32 operation_id = payload["operation_id"].asInteger();
			dest_folder_id = create_folder_in_outbox_for_item(viitem, dest_folder_id, operation_id);
		}

		LLUUID parent = viitem->getParentUUID();

		gInventory.changeItemParent(
			viitem,
			dest_folder_id,
			false);

		LLUUID top_level_folder = payload["top_level_folder"].asUUID();

		if (top_level_folder != LLUUID::null)
		{
			LLViewerInventoryCategory* category;

			while (parent.notNull())
			{
				LLInventoryModel::cat_array_t* cat_array;
				LLInventoryModel::item_array_t* item_array;
				gInventory.getDirectDescendentsOf(parent,cat_array,item_array);

				LLUUID next_parent;

				category = gInventory.getCategory(parent);

				if (!category) break;

				next_parent = category->getParentUUID();

				if (cat_array->empty() && item_array->empty())
				{
					gInventory.removeCategory(parent);
				}

				if (parent == top_level_folder)
				{
					break;
				}

				parent = next_parent;
			}
		}

		open_outbox();
	}
}

void copy_item_to_outbox(LLInventoryItem* inv_item, LLUUID dest_folder, const LLUUID& top_level_folder, S32 operation_id)
{
	// Collapse links directly to items/folders
	LLViewerInventoryItem * viewer_inv_item = (LLViewerInventoryItem *) inv_item;
	LLViewerInventoryCategory * linked_category = viewer_inv_item->getLinkedCategory();
	if (linked_category != NULL)
	{
		copy_folder_to_outbox(linked_category, dest_folder, top_level_folder, operation_id);
	}
	else
	{
		LLViewerInventoryItem * linked_item = viewer_inv_item->getLinkedItem();
		if (linked_item != NULL)
		{
			inv_item = (LLInventoryItem *) linked_item;
		}
		
		// Check for copy permissions
		if (inv_item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
		{
			// when moving item directly into outbox create folder with that name
			if (dest_folder == gInventory.findCategoryUUIDForType(LLFolderType::FT_OUTBOX, false))
			{
				dest_folder = create_folder_in_outbox_for_item(inv_item, dest_folder, operation_id);
			}
			
			copy_inventory_item(gAgent.getID(),
								inv_item->getPermissions().getOwner(),
								inv_item->getUUID(),
								dest_folder,
								inv_item->getName(),
								LLPointer<LLInventoryCallback>(NULL));

			open_outbox();
		}
		else
		{
			LLSD payload;
			payload["item_id"] = inv_item->getUUID();
			payload["dest_folder_id"] = dest_folder;
			payload["top_level_folder"] = top_level_folder;
			payload["operation_id"] = operation_id;
			
			LLMarketplaceInventoryNotifications::addNoCopyNotification(payload, move_to_outbox_cb_action);
		}
	}
}

void move_item_within_outbox(LLInventoryItem* inv_item, LLUUID dest_folder, S32 operation_id)
{
	// when moving item directly into outbox create folder with that name
	if (dest_folder == gInventory.findCategoryUUIDForType(LLFolderType::FT_OUTBOX, false))
	{
		dest_folder = create_folder_in_outbox_for_item(inv_item, dest_folder, operation_id);
	}
	
	LLViewerInventoryItem * viewer_inv_item = (LLViewerInventoryItem *) inv_item;

	gInventory.changeItemParent(
					   viewer_inv_item,
					   dest_folder,
					   false);
}

void copy_folder_to_outbox(LLInventoryCategory* inv_cat, const LLUUID& dest_folder, const LLUUID& top_level_folder, S32 operation_id)
{
	LLUUID new_folder_id = gInventory.createNewCategory(dest_folder, LLFolderType::FT_NONE, inv_cat->getName());
	gInventory.notifyObservers();

	LLInventoryModel::cat_array_t* cat_array;
	LLInventoryModel::item_array_t* item_array;
	gInventory.getDirectDescendentsOf(inv_cat->getUUID(),cat_array,item_array);

	// copy the vector because otherwise the iterator won't be happy if we delete from it
	LLInventoryModel::item_array_t item_array_copy = *item_array;

	for (LLInventoryModel::item_array_t::iterator iter = item_array_copy.begin(); iter != item_array_copy.end(); iter++)
	{
		LLInventoryItem* item = *iter;
		copy_item_to_outbox(item, new_folder_id, top_level_folder, operation_id);
	}

	LLInventoryModel::cat_array_t cat_array_copy = *cat_array;

	for (LLInventoryModel::cat_array_t::iterator iter = cat_array_copy.begin(); iter != cat_array_copy.end(); iter++)
	{
		LLViewerInventoryCategory* category = *iter;
		copy_folder_to_outbox(category, new_folder_id, top_level_folder, operation_id);
	}

	open_outbox();
}

///----------------------------------------------------------------------------
/// LLInventoryCollectFunctor implementations
///----------------------------------------------------------------------------

// static
bool LLInventoryCollectFunctor::itemTransferCommonlyAllowed(const LLInventoryItem* item)
{
	if (!item)
		return false;

	switch(item->getType())
	{
		case LLAssetType::AT_OBJECT:
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if (!get_is_item_worn(item->getUUID()))
				return true;
			break;
		default:
			return true;
			break;
	}
	return false;
}

bool LLIsType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return TRUE;
	}
	if(item)
	{
		if(item->getType() == mType) return TRUE;
	}
	return FALSE;
}

bool LLIsNotType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return FALSE;
	}
	if(item)
	{
		if(item->getType() == mType) return FALSE;
		else return TRUE;
	}
	return TRUE;
}

bool LLIsOfAssetType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) return TRUE;
	}
	if(item)
	{
		if(item->getActualType() == mType) return TRUE;
	}
	return FALSE;
}

bool LLIsTypeWithPermissions::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(mType == LLAssetType::AT_CATEGORY)
	{
		if(cat) 
		{
			return TRUE;
		}
	}
	if(item)
	{
		if(item->getType() == mType)
		{
			LLPermissions perm = item->getPermissions();
			if ((perm.getMaskBase() & mPerm) == mPerm)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

bool LLBuddyCollector::operator()(LLInventoryCategory* cat,
								  LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
		   && (!item->getCreatorUUID().isNull())
		   && (item->getCreatorUUID() != gAgent.getID()))
		{
			return true;
		}
	}
	return false;
}


bool LLUniqueBuddyCollector::operator()(LLInventoryCategory* cat,
										LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
 		   && (item->getCreatorUUID().notNull())
 		   && (item->getCreatorUUID() != gAgent.getID()))
		{
			mSeen.insert(item->getCreatorUUID());
			return true;
		}
	}
	return false;
}


bool LLParticularBuddyCollector::operator()(LLInventoryCategory* cat,
											LLInventoryItem* item)
{
	if(item)
	{
		if((LLAssetType::AT_CALLINGCARD == item->getType())
		   && (item->getCreatorUUID() == mBuddyID))
		{
			return TRUE;
		}
	}
	return FALSE;
}


bool LLNameCategoryCollector::operator()(
	LLInventoryCategory* cat, LLInventoryItem* item)
{
	if(cat)
	{
		if (!LLStringUtil::compareInsensitive(mName, cat->getName()))
		{
			return true;
		}
	}
	return false;
}

bool LLFindCOFValidItems::operator()(LLInventoryCategory* cat,
									 LLInventoryItem* item)
{
	// Valid COF items are:
	// - links to wearables (body parts or clothing)
	// - links to attachments
	// - links to gestures
	// - links to ensemble folders
	LLViewerInventoryItem *linked_item = ((LLViewerInventoryItem*)item)->getLinkedItem();
	if (linked_item)
	{
		LLAssetType::EType type = linked_item->getType();
		return (type == LLAssetType::AT_CLOTHING ||
				type == LLAssetType::AT_BODYPART ||
				type == LLAssetType::AT_GESTURE ||
				type == LLAssetType::AT_OBJECT);
	}
	else
	{
		LLViewerInventoryCategory *linked_category = ((LLViewerInventoryItem*)item)->getLinkedCategory();
		// BAP remove AT_NONE support after ensembles are fully working?
		return (linked_category &&
				((linked_category->getPreferredType() == LLFolderType::FT_NONE) ||
				 (LLFolderType::lookupIsEnsembleType(linked_category->getPreferredType()))));
	}
}

bool LLFindWearables::operator()(LLInventoryCategory* cat,
								 LLInventoryItem* item)
{
	if(item)
	{
		if((item->getType() == LLAssetType::AT_CLOTHING)
		   || (item->getType() == LLAssetType::AT_BODYPART))
		{
			return TRUE;
		}
	}
	return FALSE;
}

LLFindWearablesEx::LLFindWearablesEx(bool is_worn, bool include_body_parts)
:	mIsWorn(is_worn)
,	mIncludeBodyParts(include_body_parts)
{}

bool LLFindWearablesEx::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
	if (!vitem) return false;

	// Skip non-wearables.
	if (!vitem->isWearableType() && vitem->getType() != LLAssetType::AT_OBJECT)
	{
		return false;
	}

	// Skip body parts if requested.
	if (!mIncludeBodyParts && vitem->getType() == LLAssetType::AT_BODYPART)
	{
		return false;
	}

	// Skip broken links.
	if (vitem->getIsBrokenLink())
	{
		return false;
	}

	return (bool) get_is_item_worn(item->getUUID()) == mIsWorn;
}

bool LLFindWearablesOfType::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if (!item) return false;
	if (item->getType() != LLAssetType::AT_CLOTHING &&
		item->getType() != LLAssetType::AT_BODYPART)
	{
		return false;
	}

	LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
	if (!vitem || vitem->getWearableType() != mWearableType) return false;

	return true;
}

void LLFindWearablesOfType::setType(LLWearableType::EType type)
{
	mWearableType = type;
}

bool LLFindNonRemovableObjects::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	if (item)
	{
		return !get_is_item_removable(&gInventory, item->getUUID());
	}
	if (cat)
	{
		return !get_is_category_removable(&gInventory, cat->getUUID());
	}

	llwarns << "Not a category and not an item?" << llendl;
	return false;
}

///----------------------------------------------------------------------------
/// LLAssetIDMatches 
///----------------------------------------------------------------------------
bool LLAssetIDMatches::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	return (item && item->getAssetUUID() == mAssetID);
}

///----------------------------------------------------------------------------
/// LLLinkedItemIDMatches 
///----------------------------------------------------------------------------
bool LLLinkedItemIDMatches::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	return (item && 
			(item->getIsLinkType()) &&
			(item->getLinkedUUID() == mBaseItemID)); // A linked item's assetID will be the compared-to item's itemID.
}

void LLSaveFolderState::setApply(BOOL apply)
{
	mApply = apply; 
	// before generating new list of open folders, clear the old one
	if(!apply) 
	{
		clearOpenFolders(); 
	}
}

void LLSaveFolderState::doFolder(LLFolderViewFolder* folder)
{
	LLInvFVBridge* bridge = (LLInvFVBridge*)folder->getViewModelItem();
	if(!bridge) return;
	
	if(mApply)
	{
		// we're applying the open state
		LLUUID id(bridge->getUUID());
		if(mOpenFolders.find(id) != mOpenFolders.end())
		{
			if (!folder->isOpen())
			{
				folder->setOpen(TRUE);
			}
		}
		else
		{
			// keep selected filter in its current state, this is less jarring to user
			if (!folder->isSelected() && folder->isOpen())
			{
				folder->setOpen(FALSE);
			}
		}
	}
	else
	{
		// we're recording state at this point
		if(folder->isOpen())
		{
			mOpenFolders.insert(bridge->getUUID());
		}
	}
}

void LLOpenFilteredFolders::doItem(LLFolderViewItem *item)
{
	if (item->passedFilter())
	{
		item->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
	}
}

void LLOpenFilteredFolders::doFolder(LLFolderViewFolder* folder)
{
	if (folder->LLFolderViewItem::passedFilter() && folder->getParentFolder())
	{
		folder->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
	}
	// if this folder didn't pass the filter, and none of its descendants did
	else if (!folder->getViewModelItem()->passedFilter() && !folder->getViewModelItem()->descendantsPassedFilter())
	{
		folder->setOpenArrangeRecursively(FALSE, LLFolderViewFolder::RECURSE_NO);
	}
}

void LLSelectFirstFilteredItem::doItem(LLFolderViewItem *item)
{
	if (item->passedFilter() && !mItemSelected)
	{
		item->getRoot()->setSelection(item, FALSE, FALSE);
		if (item->getParentFolder())
		{
			item->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
		}
		mItemSelected = TRUE;
	}
}

void LLSelectFirstFilteredItem::doFolder(LLFolderViewFolder* folder)
{
	// Skip if folder or item already found, if not filtered or if no parent (root folder is not selectable)
	if (!mFolderSelected && !mItemSelected && folder->LLFolderViewItem::passedFilter() && folder->getParentFolder())
	{
		folder->getRoot()->setSelection(folder, FALSE, FALSE);
		folder->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
		mFolderSelected = TRUE;
	}
}

void LLOpenFoldersWithSelection::doItem(LLFolderViewItem *item)
{
	if (item->getParentFolder() && item->isSelected())
	{
		item->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
	}
}

void LLOpenFoldersWithSelection::doFolder(LLFolderViewFolder* folder)
{
	if (folder->getParentFolder() && folder->isSelected())
	{
		folder->getParentFolder()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
	}
}

void LLInventoryAction::doToSelected(LLInventoryModel* model, LLFolderView* root, const std::string& action)
{
	if ("rename" == action)
	{
		root->startRenamingSelectedItem();
		return;
	}
	if ("delete" == action)
	{
		LLSD args;
		args["QUESTION"] = LLTrans::getString(root->getSelectedCount() > 1 ? "DeleteItems" :  "DeleteItem");
		LLNotificationsUtil::add("DeleteItems", args, LLSD(), boost::bind(&LLInventoryAction::onItemsRemovalConfirmation, _1, _2, root));
		return;
	}
	if (("copy" == action) || ("cut" == action))
	{	
		// Clear the clipboard before we start adding things on it
		LLClipboard::instance().reset();
	}

	static const std::string change_folder_string = "change_folder_type_";
	if (action.length() > change_folder_string.length() && 
		(action.compare(0,change_folder_string.length(),"change_folder_type_") == 0))
	{
		LLFolderType::EType new_folder_type = LLViewerFolderType::lookupTypeFromXUIName(action.substr(change_folder_string.length()));
		LLFolderViewModelItemInventory* inventory_item = static_cast<LLFolderViewModelItemInventory*>(root->getViewModelItem());
		LLViewerInventoryCategory *cat = model->getCategory(inventory_item->getUUID());
		if (!cat) return;
		cat->changeType(new_folder_type);
		return;
	}


	std::set<LLFolderViewItem*> selected_items = root->getSelectionList();

	LLMultiPreview* multi_previewp = NULL;
	LLMultiProperties* multi_propertiesp = NULL;

	if (("task_open" == action  || "open" == action) && selected_items.size() > 1)
	{
		multi_previewp = new LLMultiPreview();
		gFloaterView->addChild(multi_previewp);

		LLFloater::setFloaterHost(multi_previewp);

	}
	else if (("task_properties" == action || "properties" == action) && selected_items.size() > 1)
	{
		multi_propertiesp = new LLMultiProperties();
		gFloaterView->addChild(multi_propertiesp);

		LLFloater::setFloaterHost(multi_propertiesp);
	}

	std::set<LLFolderViewItem*>::iterator set_iter;

	for (set_iter = selected_items.begin(); set_iter != selected_items.end(); ++set_iter)
	{
		LLFolderViewItem* folder_item = *set_iter;
		if(!folder_item) continue;
		LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getViewModelItem();
		if(!bridge) continue;
		bridge->performAction(model, action);
	}

	LLFloater::setFloaterHost(NULL);
	if (multi_previewp)
	{
		multi_previewp->openFloater(LLSD());
	}
	else if (multi_propertiesp)
	{
		multi_propertiesp->openFloater(LLSD());
	}
}

void LLInventoryAction::removeItemFromDND(LLFolderView* root)
{
    if(gAgent.isDoNotDisturb())
    {
        //Get selected items
        LLFolderView::selected_items_t selectedItems = root->getSelectedItems();
        LLFolderViewModelItemInventory * viewModel = NULL;

        //If user is in DND and deletes item, make sure the notification is not displayed by removing the notification
        //from DND history and .xml file. Once this is done, upon exit of DND mode the item deleted will not show a notification.
        for(LLFolderView::selected_items_t::iterator it = selectedItems.begin(); it != selectedItems.end(); ++it)
        {
            viewModel = dynamic_cast<LLFolderViewModelItemInventory *>((*it)->getViewModelItem());

            if(viewModel && viewModel->getUUID().notNull())
            {
                //Will remove the item offer notification
                LLDoNotDisturbNotificationStorage::instance().removeNotification(LLDoNotDisturbNotificationStorage::offerName, viewModel->getUUID());
            }
        }
    }
}

void LLInventoryAction::onItemsRemovalConfirmation( const LLSD& notification, const LLSD& response, LLFolderView* root )
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
        //Need to remove item from DND before item is removed from root folder view
        //because once removed from root folder view the item is no longer a selected item
        removeItemFromDND(root);
		root->removeSelectedItems();
	}
}
