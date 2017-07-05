// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file LLSidepanelInventory.cpp
 * @brief Side Bar "Inventory" panel
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
#include "llsidepanelinventory.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llavataractions.h"
#include "llbutton.h"
#include "llfirstuse.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llfloatertaskinfo.h"
#include "llfoldertype.h"
#include "llfolderview.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventoryobserver.h"
#include "llinventorypanel.h"
#include "lllayoutstack.h"
#include "lloutfitobserver.h"
#include "llpanelmaininventory.h"
#include "llpanelmarketplaceinbox.h"
#include "llselectmgr.h"
#include "lltabcontainer.h"
#include "lltrans.h"
#include "llviewermedia.h"

static LLPanelInjector<LLSidepanelInventory> t_inventory("sidepanel_inventory");

//
// Constants
//

// No longer want the inbox panel to auto-expand since it creates issues with the "new" tag time stamp
#define AUTO_EXPAND_INBOX	0

static const char * const INBOX_BUTTON_NAME = "inbox_btn";
static const char * const INBOX_LAYOUT_PANEL_NAME = "inbox_layout_panel";
static const char * const INVENTORY_LAYOUT_STACK_NAME = "inventory_layout_stack";
static const char * const MARKETPLACE_INBOX_PANEL = "marketplace_inbox";

//
// Helpers
//
class LLInboxAddedObserver : public LLInventoryCategoryAddedObserver
{
public:
	LLInboxAddedObserver(LLSidepanelInventory * sidepanelInventory)
		: LLInventoryCategoryAddedObserver()
		, mSidepanelInventory(sidepanelInventory)
	{
	}
	
	void done() override
	{
		for (auto added_category : mAddedCategories)
		{	
			LLFolderType::EType added_category_type = added_category->getPreferredType();
			
			switch (added_category_type)
			{
				case LLFolderType::FT_INBOX:
					mSidepanelInventory->enableInbox(true);
					mSidepanelInventory->observeInboxModifications(added_category->getUUID());
					break;
				default:
					break;
			}
		}
	}
	
private:
	LLSidepanelInventory * mSidepanelInventory;
};

//
// Implementation
//

LLSidepanelInventory::LLSidepanelInventory()
	: LLPanel()
	, mInventoryPanel(nullptr)
	, mPanelMainInventory(nullptr)
	, mInboxEnabled(false)
	, mCategoriesObserver(nullptr)
	, mInboxAddedObserver(nullptr)
{
}

LLSidepanelInventory::~LLSidepanelInventory()
{
	LLLayoutPanel* inbox_layout_panel = getChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);

	// Save the InventoryMainPanelHeight in settings per account
	gSavedPerAccountSettings.setS32("InventoryInboxHeight", inbox_layout_panel->getTargetDim());

	if (mCategoriesObserver && gInventory.containsObserver(mCategoriesObserver))
	{
		gInventory.removeObserver(mCategoriesObserver);
	}
	delete mCategoriesObserver;
	
	if (mInboxAddedObserver && gInventory.containsObserver(mInboxAddedObserver))
	{
		gInventory.removeObserver(mInboxAddedObserver);
	}
	delete mInboxAddedObserver;
}

void handleInventoryDisplayInboxChanged()
{
	LLSidepanelInventory* sidepanel_inventory = LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
	if (sidepanel_inventory)
	{
		sidepanel_inventory->enableInbox(gSavedSettings.getBOOL("InventoryDisplayInbox"));
	}
}

BOOL LLSidepanelInventory::postBuild()
{
	// UI elements from inventory panel
	{
		mInventoryPanel = getChild<LLPanel>("sidepanel_inventory_panel");

		mPanelMainInventory = mInventoryPanel->getChild<LLPanelMainInventory>("panel_main_inventory");
		mPanelMainInventory->setSelectCallback(boost::bind(&LLSidepanelInventory::onSelectionChange, this, _1, _2));
		LLTabContainer* tabs = mPanelMainInventory->getChild<LLTabContainer>("inventory filter tabs");
		tabs->setCommitCallback(boost::bind(&LLSidepanelInventory::updateVerbs, this));

		LLOutfitObserver::instance().addCOFChangedCallback(boost::bind(&LLSidepanelInventory::updateVerbs, this));
	}
	
	// Received items inbox setup
	{
		LLLayoutStack* inv_stack = getChild<LLLayoutStack>(INVENTORY_LAYOUT_STACK_NAME);

		// Set up button states and callbacks
		LLButton * inbox_button = getChild<LLButton>(INBOX_BUTTON_NAME);

		inbox_button->setCommitCallback(boost::bind(&LLSidepanelInventory::onToggleInboxBtn, this));

		// Get the previous inbox state from "InventoryInboxToggleState" setting.
		bool is_inbox_collapsed = !inbox_button->getToggleState();

		// Restore the collapsed inbox panel state
		LLLayoutPanel* inbox_panel = getChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);
		inv_stack->collapsePanel(inbox_panel, is_inbox_collapsed);
		if (!is_inbox_collapsed)
		{
			inbox_panel->setTargetDim(gSavedPerAccountSettings.getS32("InventoryInboxHeight"));
		}

		// Trigger callback for after login so we can setup to track inbox changes after initial inventory load
		LLAppViewer::instance()->setOnLoginCompletedCallback(boost::bind(&LLSidepanelInventory::updateInbox, this));
	}

	gSavedSettings.getControl("InventoryDisplayInbox")->getCommitSignal()->connect(boost::bind(&handleInventoryDisplayInboxChanged));

	// Update the verbs buttons state.
	updateVerbs();

	return TRUE;
}

void LLSidepanelInventory::updateInbox()
{
	//
	// Track inbox folder changes
	//
	const LLUUID inbox_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_INBOX, true);
	
	// Set up observer to listen for creation of inbox if it doesn't exist
	if (inbox_id.isNull())
	{
		observeInboxCreation();
	}
	// Set up observer for inbox changes, if we have an inbox already
	else 
	{
        // Consolidate Received items
        // We shouldn't have to do that but with a client/server system relying on a "well known folder" convention,
        // things can get messy and conventions broken. This call puts everything back together in its right place.
        gInventory.consolidateForType(inbox_id, LLFolderType::FT_INBOX);
        
		// Enable the display of the inbox if it exists
		enableInbox(gSavedSettings.getBOOL("InventoryDisplayInbox"));

		observeInboxModifications(inbox_id);
	}
}

void LLSidepanelInventory::observeInboxCreation()
{
	//
	// Set up observer to track inbox folder creation
	//
	
	if (mInboxAddedObserver == nullptr)
	{
		mInboxAddedObserver = new LLInboxAddedObserver(this);
		
		gInventory.addObserver(mInboxAddedObserver);
	}
}

void LLSidepanelInventory::observeInboxModifications(const LLUUID& inboxID)
{
	//
	// Silently do nothing if we already have an inbox inventory panel set up
	// (this can happen multiple times on the initial session that creates the inbox)
	//

	if (mInventoryPanelInbox.get() != nullptr)
	{
		return;
	}

	//
	// Track inbox folder changes
	//

	if (inboxID.isNull())
	{
		LL_WARNS() << "Attempting to track modifications to non-existent inbox" << LL_ENDL;
		return;
	}

	if (mCategoriesObserver == nullptr)
	{
		mCategoriesObserver = new LLInventoryCategoriesObserver();
		gInventory.addObserver(mCategoriesObserver);
	}

	mCategoriesObserver->addCategory(inboxID, boost::bind(&LLSidepanelInventory::onInboxChanged, this, inboxID));

	//
	// Trigger a load for the entire contents of the Inbox
	//

	LLInventoryModelBackgroundFetch::instance().start(inboxID);

	//
	// Set up the inbox inventory view
	//

	LLPanelMarketplaceInbox * inbox = getChild<LLPanelMarketplaceInbox>(MARKETPLACE_INBOX_PANEL);
    LLInventoryPanel* inventory_panel = inbox->setupInventoryPanel();
	mInventoryPanelInbox = inventory_panel->getInventoryPanelHandle();
}

void LLSidepanelInventory::enableInbox(bool enabled)
{
	mInboxEnabled = enabled;
	
	LLLayoutPanel * inbox_layout_panel = getChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);
	inbox_layout_panel->setVisible(enabled);
}

void LLSidepanelInventory::openInbox()
{
	if (mInboxEnabled)
	{
		getChild<LLButton>(INBOX_BUTTON_NAME)->setToggleState(true);
		onToggleInboxBtn();
	}
}

void LLSidepanelInventory::onInboxChanged(const LLUUID& inbox_id)
{
	// Trigger a load of the entire inbox so we always know the contents and their creation dates for sorting
	LLInventoryModelBackgroundFetch::instance().start(inbox_id);

#if AUTO_EXPAND_INBOX
	// Expand the inbox since we have fresh items
	if (mInboxEnabled)
	{
		getChild<LLButton>(INBOX_BUTTON_NAME)->setToggleState(true);
		onToggleInboxBtn();
	}
#endif
}

void LLSidepanelInventory::onToggleInboxBtn()
{
	LLButton* inboxButton = getChild<LLButton>(INBOX_BUTTON_NAME);
	LLLayoutPanel* inboxPanel = getChild<LLLayoutPanel>(INBOX_LAYOUT_PANEL_NAME);
	LLLayoutStack* inv_stack = getChild<LLLayoutStack>(INVENTORY_LAYOUT_STACK_NAME);
	
	const bool inbox_expanded = inboxButton->getToggleState();
	
	// Expand/collapse the indicated panel
	inv_stack->collapsePanel(inboxPanel, !inbox_expanded);

	if (inbox_expanded)
	{
		inboxPanel->setTargetDim(gSavedPerAccountSettings.getS32("InventoryInboxHeight"));
		if (inboxPanel->isInVisibleChain())
	{
		gSavedPerAccountSettings.setU32("LastInventoryInboxActivity", time_corrected());
	}
}
	else
	{
		gSavedPerAccountSettings.setS32("InventoryInboxHeight", inboxPanel->getTargetDim());
	}

}

void LLSidepanelInventory::onOpen(const LLSD& key)
{
	LLFirstUse::newInventory(false);
	mPanelMainInventory->setFocusFilterEditor();
#if AUTO_EXPAND_INBOX
	// Expand the inbox if we have fresh items
	LLPanelMarketplaceInbox * inbox = findChild<LLPanelMarketplaceInbox>(MARKETPLACE_INBOX_PANEL);
	if (inbox && (inbox->getFreshItemCount() > 0))
	{
		getChild<LLButton>(INBOX_BUTTON_NAME)->setToggleState(true);
		onToggleInboxBtn();
	}
#else
	if (mInboxEnabled && getChild<LLButton>(INBOX_BUTTON_NAME)->getToggleState())
	{
		gSavedPerAccountSettings.setU32("LastInventoryInboxActivity", time_corrected());
	}
#endif

	if(key.size() == 0) return;

	if (key.has("id"))
	{
		LLFloaterReg::showInstance("item_properties", key, TRUE);
	}
	if (key.has("task"))
	{
		LLFloaterTaskInfo::showTask();
	}
}

void LLSidepanelInventory::onBackButtonClicked()
{
	updateVerbs();
}

void LLSidepanelInventory::onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action)
{
	updateVerbs();
}

void LLSidepanelInventory::updateVerbs()
{
}

bool LLSidepanelInventory::canShare()
{
	LLInventoryPanel* inbox = mInventoryPanelInbox.get();

	// Avoid flicker in the Recent tab while inventory is being loaded.
	if ( (!inbox || !inbox->getRootFolder() || inbox->getRootFolder()->getSelectionList().empty())
		&& (mPanelMainInventory && !mPanelMainInventory->getActivePanel()->getRootFolder()->hasVisibleChildren()) )
	{
		return false;
	}

	return ( (mPanelMainInventory ? LLAvatarActions::canShareSelectedItems(mPanelMainInventory->getActivePanel()) : false)
			|| (inbox ? LLAvatarActions::canShareSelectedItems(inbox) : false) );
}

LLInventoryPanel *LLSidepanelInventory::getActivePanel() const
{
	if (!getVisible())
	{
		return nullptr;
	}
	if (mInventoryPanel->getVisible())
	{
		return mPanelMainInventory->getActivePanel();
	}
	return nullptr;
}

BOOL LLSidepanelInventory::isMainInventoryPanelActive() const
{
	return mInventoryPanel->getVisible();
}

void LLSidepanelInventory::clearSelections(bool clearMain, bool clearInbox)
{
	if (clearMain)
	{
		LLInventoryPanel * inv_panel = getActivePanel();
		
		if (inv_panel)
		{
			inv_panel->getRootFolder()->clearSelection();
		}
	}
	
	if (clearInbox && mInboxEnabled && mInventoryPanelInbox.get())
	{
		mInventoryPanelInbox.get()->getRootFolder()->clearSelection();
	}
	
	updateVerbs();
}

std::set<LLFolderViewItem*> LLSidepanelInventory::getInboxSelectionList() const
{
	std::set<LLFolderViewItem*> inventory_selected_uuids;
	
	if (mInboxEnabled && mInventoryPanelInbox.get() && mInventoryPanelInbox.get()->getRootFolder())
	{
		inventory_selected_uuids = mInventoryPanelInbox.get()->getRootFolder()->getSelectionList();
	}
	
	return inventory_selected_uuids;
}
