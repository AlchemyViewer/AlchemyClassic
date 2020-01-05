/* 
 * @file llinventorypanel.cpp
 * @brief Implementation of the inventory panel and associated stuff.
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
#include "llinventorypanel.h"

#include <utility> // for std::pair<>

#include "llagent.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llavataractions.h"
#include "llclipboard.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llfolderview.h"
#include "llfolderviewitem.h"
#include "llfloaterimcontainer.h"
#include "llimview.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llnotificationsutil.h"
#include "llpreview.h"
#include "llsidepanelinventory.h"
#include "lltrans.h"
#include "llviewerattachmenu.h"
#include "llviewerfoldertype.h"
#include "llvoavatarself.h"
// [RLVa:KB] - Checked: 2013-05-08 (RLVa-1.4.9)
#include "rlvactions.h"
#include "rlvcommon.h"
// [/RLVa:KB]

static LLDefaultChildRegistry::Register<LLInventoryPanel> r("inventory_panel");

const std::string LLInventoryPanel::DEFAULT_SORT_ORDER = std::string("InventorySortOrder");
const std::string LLInventoryPanel::RECENTITEMS_SORT_ORDER = std::string("RecentItemsSortOrder");
const std::string LLInventoryPanel::INHERIT_SORT_ORDER = std::string("");
static const LLInventoryFolderViewModelBuilder INVENTORY_BRIDGE_BUILDER;

// statics 
bool LLInventoryPanel::sColorSetInitialized = false;
LLUIColor LLInventoryPanel::sDefaultColor;
LLUIColor LLInventoryPanel::sDefaultHighlightColor;
LLUIColor LLInventoryPanel::sLibraryColor;
LLUIColor LLInventoryPanel::sLinkColor;

const LLColor4U DEFAULT_WHITE(255, 255, 255);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryPanelObserver
//
// Bridge to support knowing when the inventory has changed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryPanelObserver : public LLInventoryObserver
{
public:
	LLInventoryPanelObserver(LLInventoryPanel* ip) : mIP(ip) {}
	virtual ~LLInventoryPanelObserver() = default;

	void changed(U32 mask) override
	{
		mIP->modelChanged(mask);
	}
protected:
	LLInventoryPanel* mIP;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInvPanelComplObserver
//
// Calls specified callback when all specified items become complete.
//
// Usage:
// observer = new LLInvPanelComplObserver(boost::bind(onComplete));
// inventory->addObserver(observer);
// observer->reset(); // (optional)
// observer->watchItem(incomplete_item1_id);
// observer->watchItem(incomplete_item2_id);
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInvPanelComplObserver : public LLInventoryCompletionObserver
{
public:
	typedef std::function<void()> callback_t;

	LLInvPanelComplObserver(callback_t cb)
	:	mCallback(std::move(cb))
	{
	}

	void reset();

private:
	/*virtual*/ void done() override;

	/// Called when all the items are complete.
	callback_t	mCallback;
};

void LLInvPanelComplObserver::reset()
{
	mIncomplete.clear();
	mComplete.clear();
}

void LLInvPanelComplObserver::done()
{
	mCallback();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryPanel
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LLInventoryPanel::LLInventoryPanel(const LLInventoryPanel::Params& p) :	
	LLPanel(p),
	mInventory(p.inventory),
	mInventoryObserver(nullptr),
	mCompletionObserver(nullptr),
	mAcceptsDragAndDrop(p.accepts_drag_and_drop),
	mAllowMultiSelect(p.allow_multi_select),
	mShowItemLinkOverlays(p.show_item_link_overlays),
	mShowEmptyMessage(p.show_empty_message),
	mScroller(nullptr),
	mInventoryViewModel(p.name),
	mGroupedItemBridge(new LLFolderViewGroupedItemBridge),
	mInvFVBridgeBuilder(nullptr),
	mSortOrderSetting(p.sort_order_setting),
	mViewsInitialized(false)
{
	mInvFVBridgeBuilder = &INVENTORY_BRIDGE_BUILDER;

	if (!sColorSetInitialized)
	{
		sDefaultColor = LLUIColorTable::instance().getColor("MenuItemEnabledColor", DEFAULT_WHITE);
		sDefaultHighlightColor = LLUIColorTable::instance().getColor("MenuItemHighlightFgColor", DEFAULT_WHITE);
		sLibraryColor = LLUIColorTable::instance().getColor("InventoryItemLibraryColor", DEFAULT_WHITE);
		sLinkColor = LLUIColorTable::instance().getColor("InventoryItemLinkColor", DEFAULT_WHITE);
		sColorSetInitialized = true;
	}
	
	// context menu callbacks
	mCommitCallbackRegistrar.add("Inventory.DoToSelected", boost::bind(&LLInventoryPanel::doToSelected, this, _2));
	mCommitCallbackRegistrar.add("Inventory.EmptyTrash", boost::bind(&LLInventoryModel::emptyFolderType, &gInventory, "ConfirmEmptyTrash", LLFolderType::FT_TRASH));
	mCommitCallbackRegistrar.add("Inventory.EmptyLostAndFound", boost::bind(&LLInventoryModel::emptyFolderType, &gInventory, "ConfirmEmptyLostAndFound", LLFolderType::FT_LOST_AND_FOUND));
	mCommitCallbackRegistrar.add("Inventory.DoCreate", boost::bind(&LLInventoryPanel::doCreate, this, _2));
	mCommitCallbackRegistrar.add("Inventory.AttachObject", boost::bind(&LLInventoryPanel::attachObject, this, _2));
	mCommitCallbackRegistrar.add("Inventory.BeginIMSession", boost::bind(&LLInventoryPanel::beginIMSession, this));
	mCommitCallbackRegistrar.add("Inventory.Share",  boost::bind(&LLAvatarActions::shareWithAvatars, this));
	mCommitCallbackRegistrar.add("Inventory.FileUploadLocation", boost::bind(&LLInventoryPanel::fileUploadLocation, this, _2));
}

LLFolderView * LLInventoryPanel::createFolderRoot(LLUUID root_id )
{
    LLFolderView::Params p(mParams.folder_view);
    p.name = getName();
    p.title = getLabel();
    p.rect = LLRect(0, 0, getRect().getWidth(), 0);
    p.parent_panel = this;
    p.tool_tip = p.name;
    p.listener = mInvFVBridgeBuilder->createBridge(	LLAssetType::AT_CATEGORY,
																	LLAssetType::AT_CATEGORY,
																	LLInventoryType::IT_CATEGORY,
																	this,
																	&mInventoryViewModel,
																	nullptr,
																	root_id);
    p.view_model = &mInventoryViewModel;
	p.grouped_item_model = mGroupedItemBridge;
    p.use_label_suffix = mParams.use_label_suffix;
    p.allow_multiselect = mAllowMultiSelect;
    p.show_empty_message = mShowEmptyMessage;
    p.show_item_link_overlays = mShowItemLinkOverlays;
    p.root = nullptr;
    p.allow_drop = mParams.allow_drop_on_root;
    p.options_menu = "menu_inventory.xml";

    return LLUICtrlFactory::create<LLFolderView>(p);
}

void LLInventoryPanel::clearFolderRoot()
{
	gIdleCallbacks.deleteFunction(idle, this);
    gIdleCallbacks.deleteFunction(onIdle, this);
    
    if (mInventoryObserver)
    {
        mInventory->removeObserver(mInventoryObserver);
        delete mInventoryObserver;
        mInventoryObserver = nullptr;
    }
    if (mCompletionObserver)
    {
        mInventory->removeObserver(mCompletionObserver);
        delete mCompletionObserver;
        mCompletionObserver = nullptr;
    }
    
    if (mScroller)
    {
        removeChild(mScroller);
        delete mScroller;
        mScroller = nullptr;
    }
}

void LLInventoryPanel::initFromParams(const LLInventoryPanel::Params& params)
{
	// save off copy of params
	mParams = params;
	// Clear up the root view
	// Note: This needs to be done *before* we build the new folder view 
	LLUUID root_id = getRootFolderID();
	if (mFolderRoot.get())
	{
		removeItemID(root_id);
		mFolderRoot.get()->destroyView();
	}

	mCommitCallbackRegistrar.pushScope(); // registered as a widget; need to push callback scope ourselves
	{
		// Determine the root folder in case specified, and
		// build the views starting with that folder.
        LLFolderView* folder_view = createFolderRoot(root_id);
		mFolderRoot = folder_view->getHandle();
	
		addItemID(root_id, mFolderRoot.get());
	}
	mCommitCallbackRegistrar.popScope();
	mFolderRoot.get()->setCallbackRegistrar(&mCommitCallbackRegistrar);
	mFolderRoot.get()->setEnableRegistrar(&mEnableCallbackRegistrar);
	
	// Scroller
		LLRect scroller_view_rect = getRect();
		scroller_view_rect.translate(-scroller_view_rect.mLeft, -scroller_view_rect.mBottom);
	LLScrollContainer::Params scroller_params(mParams.scroll());
		scroller_params.rect(scroller_view_rect);
		mScroller = LLUICtrlFactory::create<LLFolderViewScrollContainer>(scroller_params, this);
		mScroller->addChild(mFolderRoot.get());
		mFolderRoot.get()->setScrollContainer(mScroller);
		mFolderRoot.get()->setFollows(FOLLOWS_ALL);
		mFolderRoot.get()->addChild(mFolderRoot.get()->mStatusTextBox);

	// Set up the callbacks from the inventory we're viewing, and then build everything.
	mInventoryObserver = new LLInventoryPanelObserver(this);
	mInventory->addObserver(mInventoryObserver);

	mCompletionObserver = new LLInvPanelComplObserver(boost::bind(&LLInventoryPanel::onItemsCompletion, this));
	mInventory->addObserver(mCompletionObserver);

	// Build view of inventory if we need default full hierarchy and inventory ready,
	// otherwise wait for idle callback.
	if (mInventory->isInventoryUsable() && !mViewsInitialized)
	{
		initializeViews();
	}
	
	gIdleCallbacks.addFunction(onIdle, (void*)this);

	if (mSortOrderSetting != INHERIT_SORT_ORDER)
	{
		setSortOrder(gSavedSettings.getU32(mSortOrderSetting));
	}
	else
	{
		setSortOrder(gSavedSettings.getU32(DEFAULT_SORT_ORDER));
	}

	// hide inbox
	if (!gSavedSettings.getBOOL("InventoryOutboxMakeVisible"))
	{
		//getFilter().setFilterCategoryTypes(getFilter().getFilterCategoryTypes() & ~(1ULL << LLFolderType::FT_INBOX));
		getFilter().setFilterCategoryTypes(getFilter().getFilterCategoryTypes() & ~(1ULL << LLFolderType::FT_OUTBOX));
	}
    // hide marketplace listing box, unless we are a marketplace panel
	if (!gSavedSettings.getBOOL("InventoryOutboxMakeVisible") && !mParams.use_marketplace_folders)
	{
		getFilter().setFilterCategoryTypes(getFilter().getFilterCategoryTypes() & ~(1ULL << LLFolderType::FT_MARKETPLACE_LISTINGS));
    }
    
	// set the filter for the empty folder if the debug setting is on
	if (gSavedSettings.getBOOL("DebugHideEmptySystemFolders"))
	{
		getFilter().setFilterEmptySystemFolders();
	}
	
	// keep track of the clipboard state so that we avoid filtering too much
	mClipboardState = LLClipboard::instance().getGeneration();
	
	// Initialize base class params.
	LLPanel::initFromParams(mParams);
}

LLInventoryPanel::~LLInventoryPanel()
{
	U32 sort_order = getFolderViewModel()->getSorter().getSortOrder();
    if (mSortOrderSetting != INHERIT_SORT_ORDER)
    {
        gSavedSettings.setU32(mSortOrderSetting, sort_order);
    }
    
    clearFolderRoot();
}

void LLInventoryPanel::draw()
{
	// Select the desired item (in case it wasn't loaded when the selection was requested)
	updateSelection();
	
	LLPanel::draw();
}

const LLInventoryFilter& LLInventoryPanel::getFilter() const
{
	return getFolderViewModel()->getFilter();
}

LLInventoryFilter& LLInventoryPanel::getFilter()
{
	return getFolderViewModel()->getFilter();
}

void LLInventoryPanel::setFilterTypes(U64 types, LLInventoryFilter::EFilterType filter_type)
{
	if (filter_type == LLInventoryFilter::FILTERTYPE_OBJECT)
	{
		getFilter().setFilterObjectTypes(types);
	}
	if (filter_type == LLInventoryFilter::FILTERTYPE_CATEGORY)
		getFilter().setFilterCategoryTypes(types);
}

void LLInventoryPanel::setFilterWorn()
{
    getFilter().setFilterWorn();
}

U32 LLInventoryPanel::getFilterObjectTypes() const 
{ 
	return getFilter().getFilterObjectTypes();
}

U32 LLInventoryPanel::getFilterPermMask() const 
{ 
	return getFilter().getFilterPermissions();
}


void LLInventoryPanel::setFilterPermMask(PermissionMask filter_perm_mask)
{
	getFilter().setFilterPermissions(filter_perm_mask);
}

void LLInventoryPanel::setFilterWearableTypes(U64 types)
{
	getFilter().setFilterWearableTypes(types);
}

void LLInventoryPanel::setFilterSubString(const std::string& string)
{
	getFilter().setFilterSubString(string);
}

const std::string& LLInventoryPanel::getFilterSubString() 
{ 
	return getFilter().getFilterSubString();
}


void LLInventoryPanel::setSortOrder(U32 order)
{
    LLInventorySort sorter(order);
	if (order != getFolderViewModel()->getSorter().getSortOrder())
	{
		getFolderViewModel()->setSorter(sorter);
		mFolderRoot.get()->arrangeAll();
		// try to keep selection onscreen, even if it wasn't to start with
		mFolderRoot.get()->scrollToShowSelection();
	}
}

U32 LLInventoryPanel::getSortOrder() const 
{ 
	return getFolderViewModel()->getSorter().getSortOrder();
}

void LLInventoryPanel::setSinceLogoff(BOOL sl)
{
	getFilter().setDateRangeLastLogoff(sl);
}

void LLInventoryPanel::setHoursAgo(U32 hours)
{
	getFilter().setHoursAgo(hours);
}

void LLInventoryPanel::setDateSearchDirection(U32 direction)
{
	getFilter().setDateSearchDirection(direction);
}

void LLInventoryPanel::setFilterLinks(LLInventoryFilter::EFilterLink filter_links)
{
	getFilter().setFilterLinks(filter_links);
}

void LLInventoryPanel::setSearchType(LLInventoryFilter::ESearchType type)
{
	getFilter().setSearchType(type);
}

LLInventoryFilter::ESearchType LLInventoryPanel::getSearchType()
{
	return getFilter().getSearchType();
}

void LLInventoryPanel::setShowFolderState(LLInventoryFilter::EFolderShow show)
{
	getFilter().setShowFolderState(show);
}

LLInventoryFilter::EFolderShow LLInventoryPanel::getShowFolderState()
{
	return getFilter().getShowFolderState();
}

// Called when something changed in the global model (new item, item coming through the wire, rename, move, etc...) (CHUI-849)
static LLTrace::BlockTimerStatHandle FTM_REFRESH("Inventory Refresh");
void LLInventoryPanel::modelChanged(U32 mask)
{
	LL_RECORD_BLOCK_TIME(FTM_REFRESH);

	if (!mViewsInitialized) return;
	
	const LLInventoryModel* model = getModel();
	if (!model) return;

	const LLInventoryModel::changed_items_t& changed_items = model->getChangedIDs();
	if (changed_items.empty()) return;

	for (const auto& item_id : changed_items)
    {
        const LLInventoryObject* model_item = model->getObject(item_id);
		LLFolderViewItem* view_item = getItemByID(item_id);
		LLFolderViewModelItemInventory* viewmodel_item = 
			static_cast<LLFolderViewModelItemInventory*>(view_item ? view_item->getViewModelItem() : NULL);

		// LLFolderViewFolder is derived from LLFolderViewItem so dynamic_cast from item
		// to folder is the fast way to get a folder without searching through folders tree.
		LLFolderViewFolder* view_folder = nullptr;

		// Check requires as this item might have already been deleted
		// as a child of its deleted parent.
		if (model_item && view_item)
		{
			view_folder = dynamic_cast<LLFolderViewFolder*>(view_item);
		}

		//////////////////////////////
		// LABEL Operation
		// Empty out the display name for relabel.
		if (mask & LLInventoryObserver::LABEL)
		{
			if (view_item)
			{
				// Request refresh on this item (also flags for filtering)
				LLInvFVBridge* bridge = (LLInvFVBridge*)view_item->getViewModelItem();
				if(bridge)
				{	// Clear the display name first, so it gets properly re-built during refresh()
					bridge->clearDisplayName();

					view_item->refresh();
				}
				LLFolderViewFolder* parent = view_item->getParentFolder();
				if(parent)
				{
					parent->getViewModelItem()->dirtyDescendantsFilter();
				}
			}
		}

		//////////////////////////////
		// REBUILD Operation
		// Destroy and regenerate the UI.
		if (mask & LLInventoryObserver::REBUILD)
		{
			if (model_item && view_item && viewmodel_item)
			{
				const LLUUID& idp = viewmodel_item->getUUID();
				view_item->destroyView();
				removeItemID(idp);
			}
			view_item = buildNewViews(item_id);
			viewmodel_item = 
				static_cast<LLFolderViewModelItemInventory*>(view_item ? view_item->getViewModelItem() : NULL);
			view_folder = dynamic_cast<LLFolderViewFolder *>(view_item);
		}

		//////////////////////////////
		// INTERNAL Operation
		// This could be anything.  For now, just refresh the item.
		if (mask & LLInventoryObserver::INTERNAL)
		{
			if (view_item)
			{
				view_item->refresh();
			}
		}

		//////////////////////////////
		// SORT Operation
		// Sort the folder.
		if (mask & LLInventoryObserver::SORT)
		{
			if (view_folder)
			{
				view_folder->getViewModelItem()->requestSort();
			}
		}	

		// We don't typically care which of these masks the item is actually flagged with, since the masks
		// may not be accurate (e.g. in the main inventory panel, I move an item from My Inventory into
		// Landmarks; this is a STRUCTURE change for that panel but is an ADD change for the Landmarks
		// panel).  What's relevant is that the item and UI are probably out of sync and thus need to be
		// resynchronized.
		if (mask & (LLInventoryObserver::STRUCTURE |
					LLInventoryObserver::ADD |
					LLInventoryObserver::REMOVE))
		{
			//////////////////////////////
			// ADD Operation
			// Item exists in memory but a UI element hasn't been created for it.
			if (model_item && !view_item)
			{
				// Add the UI element for this item.
				buildNewViews(item_id);
				// Select any newly created object that has the auto rename at top of folder root set.
				if(mFolderRoot.get()->getRoot()->needsAutoRename())
				{
					setSelection(item_id, FALSE);
				}
				updateFolderLabel(model_item->getParentUUID());
			}

			//////////////////////////////
			// STRUCTURE Operation
			// This item already exists in both memory and UI.  It was probably reparented.
			else if (model_item && view_item)
			{
				LLFolderViewFolder* old_parent = view_item->getParentFolder();
				// Don't process the item if it is the root
				if (old_parent)
				{
					LLFolderViewModelItemInventory* viewmodel_folder = static_cast<LLFolderViewModelItemInventory*>(old_parent->getViewModelItem());
					LLFolderViewFolder* new_parent =   (LLFolderViewFolder*)getItemByID(model_item->getParentUUID());
					// Item has been moved.
					if (old_parent != new_parent)
					{
						if (new_parent != nullptr)
						{
							// Item is to be moved and we found its new parent in the panel's directory, so move the item's UI.
							view_item->addToFolder(new_parent);
							addItemID(viewmodel_item->getUUID(), view_item);
							if (mInventory)
							{
								const LLUUID trash_id = mInventory->findCategoryUUIDForType(LLFolderType::FT_TRASH);
								if (trash_id != model_item->getParentUUID() && (mask & LLInventoryObserver::INTERNAL) && new_parent->isOpen())
								{
									setSelection(item_id, FALSE);
								}
							}
							updateFolderLabel(model_item->getParentUUID());
						}
						else 
						{
							// Remove the item ID before destroying the view because the view-model-item gets
							// destroyed when the view is destroyed
                            removeItemID(viewmodel_item->getUUID());

							// Item is to be moved outside the panel's directory (e.g. moved to trash for a panel that 
							// doesn't include trash).  Just remove the item's UI.
							view_item->destroyView();
						}
						if(viewmodel_folder)
						{
							updateFolderLabel(viewmodel_folder->getUUID());
						}
						old_parent->getViewModelItem()->dirtyDescendantsFilter();
					}
				}
			}
			
			//////////////////////////////
			// REMOVE Operation
			// This item has been removed from memory, but its associated UI element still exists.
			else if (!model_item && view_item && viewmodel_item)
			{
				// Remove the item's UI.
				LLFolderViewFolder* parent = view_item->getParentFolder();
				removeItemID(viewmodel_item->getUUID());
				view_item->destroyView();
				if(parent)
				{
					parent->getViewModelItem()->dirtyDescendantsFilter();
					LLFolderViewModelItemInventory* viewmodel_folder = static_cast<LLFolderViewModelItemInventory*>(parent->getViewModelItem());
					if(viewmodel_folder)
					{
						updateFolderLabel(viewmodel_folder->getUUID());
					}
				}
			}
		}
	}
}

LLUUID LLInventoryPanel::getRootFolderID()
{
    LLUUID root_id;
	if (mFolderRoot.get() && mFolderRoot.get()->getViewModelItem())
	{
		root_id = static_cast<LLFolderViewModelItemInventory*>(mFolderRoot.get()->getViewModelItem())->getUUID();
	}
	else
	{
		if (mParams.start_folder.id.isChosen())
		{
			root_id = mParams.start_folder.id;
		}
		else
		{
			const LLFolderType::EType preferred_type = mParams.start_folder.type.isChosen() 
				? mParams.start_folder.type
				: LLViewerFolderType::lookupTypeFromNewCategoryName(mParams.start_folder.name);

			if ("LIBRARY" == mParams.start_folder.name())
			{
				root_id = gInventory.getLibraryRootFolderID();
			}
			else if (preferred_type != LLFolderType::FT_NONE)
			{
                LLStringExplicit label(mParams.start_folder.name());
                setLabel(label);
                
				root_id = gInventory.findCategoryUUIDForType(preferred_type, false);
				if (root_id.isNull())
				{
					LL_WARNS() << "Could not find folder of type " << preferred_type << LL_ENDL;
					root_id.generateNewID();
				}
			}
		}
	}
    return root_id;
}

// static
void LLInventoryPanel::onIdle(void *userdata)
{
	if (!gInventory.isInventoryUsable())
		return;

	LLInventoryPanel *self = (LLInventoryPanel*)userdata;
	// Inventory just initialized, do complete build
	if (!self->mViewsInitialized)
	{
		self->initializeViews();
	}
	if (self->mViewsInitialized)
	{
		gIdleCallbacks.deleteFunction(onIdle, (void*)self);
	}
}

struct DirtyFilterFunctor : public LLFolderViewFunctor
{
	/*virtual*/ void doFolder(LLFolderViewFolder* folder) override
	{
		folder->getViewModelItem()->dirtyFilter();
	}
	/*virtual*/ void doItem(LLFolderViewItem* item) override
	{
		item->getViewModelItem()->dirtyFilter();
	}
};

void LLInventoryPanel::idle(void* user_data)
{
	LLInventoryPanel* panel = (LLInventoryPanel*)user_data;
	// Nudge the filter if the clipboard state changed
	if (panel->mClipboardState != LLClipboard::instance().getGeneration())
	{
		panel->mClipboardState = LLClipboard::instance().getGeneration();
		const LLUUID trash_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
		LLFolderViewFolder* trash_folder = panel->getFolderByID(trash_id);
		if (trash_folder)
		{
            DirtyFilterFunctor dirtyFilterFunctor;
			trash_folder->applyFunctorToChildren(dirtyFilterFunctor);
		}

	}

    // Take into account the fact that the root folder might be invalidated
    if (panel->mFolderRoot.get())
    {
        panel->mFolderRoot.get()->update();
        // while dragging, update selection rendering to reflect single/multi drag status
        if (LLToolDragAndDrop::getInstance()->hasMouseCapture())
        {
            EAcceptance last_accept = LLToolDragAndDrop::getInstance()->getLastAccept();
            if (last_accept == ACCEPT_YES_SINGLE || last_accept == ACCEPT_YES_COPY_SINGLE)
            {
                panel->mFolderRoot.get()->setShowSingleSelection(TRUE);
            }
            else
            {
                panel->mFolderRoot.get()->setShowSingleSelection(FALSE);
            }
        }
        else
        {
            panel->mFolderRoot.get()->setShowSingleSelection(FALSE);
        }
    }
    else
    {
        LL_WARNS() << "Inventory : Deleted folder root detected on panel" << LL_ENDL;
        panel->clearFolderRoot();
    }
}


void LLInventoryPanel::initializeViews()
{
	if (!gInventory.isInventoryUsable()) return;

	LLUUID root_id = getRootFolderID();
	if (root_id.notNull())
	{
		buildNewViews(getRootFolderID());
	}
	else
	{
		// Default case: always add "My Inventory" first, "Library" second
		buildNewViews(gInventory.getRootFolderID());		// My Inventory
		buildNewViews(gInventory.getLibraryRootFolderID());	// Library
	}

	gIdleCallbacks.addFunction(idle, this);

	mViewsInitialized = true;
	
	openStartFolderOrMyInventory();
	
	// Special case for new user login
	if (gAgent.isFirstLogin())
	{
		// Auto open the user's library
		LLFolderViewFolder* lib_folder =   getFolderByID(gInventory.getLibraryRootFolderID());
		if (lib_folder)
		{
			lib_folder->setOpen(TRUE);
		}
		
		// Auto close the user's my inventory folder
		LLFolderViewFolder* my_inv_folder =   getFolderByID(gInventory.getRootFolderID());
		if (my_inv_folder)
		{
			my_inv_folder->setOpenArrangeRecursively(FALSE, LLFolderViewFolder::RECURSE_DOWN);
		}
	}
}


LLFolderViewFolder * LLInventoryPanel::createFolderViewFolder(LLInvFVBridge * bridge, bool allow_drop)
{
	LLFolderViewFolder::Params params(mParams.folder);

	params.name = bridge->getDisplayName();
	params.root = mFolderRoot.get();
	params.listener = bridge;
	params.tool_tip = params.name;
    params.allow_drop = allow_drop;

	params.font_color = (bridge->isLibraryItem() ? sLibraryColor : (bridge->isLink() ? sLinkColor : sDefaultColor));
	params.font_highlight_color = (bridge->isLibraryItem() ? sLibraryColor : (bridge->isLink() ? sLinkColor : sDefaultHighlightColor));
	
	return LLUICtrlFactory::create<LLFolderViewFolder>(params);
}

LLFolderViewItem * LLInventoryPanel::createFolderViewItem(LLInvFVBridge * bridge)
{
	LLFolderViewItem::Params params(mParams.item);
	
	params.name = bridge->getDisplayName();
	params.creation_date = bridge->getCreationDate();
	params.root = mFolderRoot.get();
	params.listener = bridge;
	params.rect = LLRect (0, 0, 0, 0);
	params.tool_tip = params.name;

	params.font_color = (bridge->isLibraryItem() ? sLibraryColor : (bridge->isLink() ? sLinkColor : sDefaultColor));
	params.font_highlight_color = (bridge->isLibraryItem() ? sLibraryColor : (bridge->isLink() ? sLinkColor : sDefaultHighlightColor));
	
	return LLUICtrlFactory::create<LLFolderViewItem>(params);
}

LLFolderViewItem* LLInventoryPanel::buildNewViews(const LLUUID& id)
{
 	LLInventoryObject const* objectp = gInventory.getObject(id);
	
	if (!objectp)
    {
        return nullptr;
    }

	LLFolderViewItem* folder_view_item = getItemByID(id);

    const LLUUID &parent_id = objectp->getParentUUID();
	LLFolderViewFolder* parent_folder = (LLFolderViewFolder*)getItemByID(parent_id);
  		
    // Force the creation of an extra root level folder item if required by the inventory panel (default is "false")
    bool allow_drop = true;
    bool create_root = false;
    if (mParams.show_root_folder)
    {
        LLUUID root_id = getRootFolderID();
        if (root_id == id)
        {
            // We insert an extra level that's seen by the UI but has no influence on the model
            parent_folder = dynamic_cast<LLFolderViewFolder*>(folder_view_item);
            folder_view_item = nullptr;
            allow_drop = mParams.allow_drop_on_root;
            create_root = true;
        }
    }

 	if (!folder_view_item && parent_folder)
  		{
			if (objectp->getType() <= LLAssetType::AT_NONE)
			{
				LL_WARNS() << "LLInventoryPanel::buildNewViews called with invalid objectp->mType : "
					<< ((S32)objectp->getType()) << " name " << objectp->getName() << " UUID " << objectp->getUUID()
					<< LL_ENDL;
  				return nullptr;
			}
			
			if (objectp->getType() >= LLAssetType::AT_COUNT)
  			{
				// Example: Happens when we add assets of new, not yet supported type to library
				LL_DEBUGS() << "LLInventoryPanel::buildNewViews called with unknown objectp->mType : "
				<< ((S32) objectp->getType()) << " name " << objectp->getName() << " UUID " << objectp->getUUID()
				<< LL_ENDL;

				LLInventoryItem* item = (LLInventoryItem*)objectp;
				if (item)
				{
					LLInvFVBridge* new_listener = mInvFVBridgeBuilder->createBridge(LLAssetType::AT_UNKNOWN,
						LLAssetType::AT_UNKNOWN,
						LLInventoryType::IT_UNKNOWN,
						this,
						&mInventoryViewModel,
						mFolderRoot.get(),
						item->getUUID(),
						item->getFlags());

					if (new_listener)
					{
						folder_view_item = createFolderViewItem(new_listener);
					}
				}
  			}
  		
  			if ((objectp->getType() == LLAssetType::AT_CATEGORY) &&
  				(objectp->getActualType() != LLAssetType::AT_LINK_FOLDER))
  			{
  				LLInvFVBridge* new_listener = mInvFVBridgeBuilder->createBridge(LLAssetType::AT_CATEGORY,
                                            (mParams.use_marketplace_folders ? LLAssetType::AT_MARKETPLACE_FOLDER : LLAssetType::AT_CATEGORY),
  																				LLInventoryType::IT_CATEGORY,
  																				this,
                                                                                &mInventoryViewModel,
  																				mFolderRoot.get(),
  																				objectp->getUUID());
  				if (new_listener)
  				{
                    folder_view_item = createFolderViewFolder(new_listener,allow_drop);
  				}
  			}
  			else
  			{
  				// Build new view for item.
  				LLInventoryItem* item = (LLInventoryItem*)objectp;
  				LLInvFVBridge* new_listener = mInvFVBridgeBuilder->createBridge(item->getType(),
  																				item->getActualType(),
  																				item->getInventoryType(),
  																				this,
																			&mInventoryViewModel,
  																				mFolderRoot.get(),
  																				item->getUUID(),
  																				item->getFlags());
 
  				if (new_listener)
  				{
				folder_view_item = createFolderViewItem(new_listener);
  				}
  			}
 
  	    if (folder_view_item)
        {
            llassert(parent_folder != NULL);
            folder_view_item->addToFolder(parent_folder);
			addItemID(id, folder_view_item);
            // In the case of the root folder been shown, open that folder by default once the widget is created
            if (create_root)
            {
                folder_view_item->setOpen(TRUE);
            }
        }
	}

	// If this is a folder, add the children of the folder and recursively add any 
	// child folders.
	if (folder_view_item && objectp->getType() == LLAssetType::AT_CATEGORY)
	{
		LLViewerInventoryCategory::cat_array_t* categories;
		LLViewerInventoryItem::item_array_t* items;
		mInventory->lockDirectDescendentArrays(id, categories, items);
		
		if(categories)
		{
			for (LLViewerInventoryCategory::cat_array_t::const_iterator cat_iter = categories->begin();
				 cat_iter != categories->end();
				 ++cat_iter)
			{
				const LLViewerInventoryCategory* cat = (*cat_iter);
				buildNewViews(cat->getUUID());
			}
		}
		
		if(items)
		{
			for (LLViewerInventoryItem::item_array_t::const_iterator item_iter = items->begin();
				 item_iter != items->end();
				 ++item_iter)
			{
				const LLViewerInventoryItem* item = (*item_iter);
				buildNewViews(item->getUUID());
			}
		}
		mInventory->unlockDirectDescendentArrays(id);
	}
	
	return folder_view_item;
}

// bit of a hack to make sure the inventory is open.
void LLInventoryPanel::openStartFolderOrMyInventory()
{
	// Find My Inventory folder and open it up by name
	for (LLView *child = mFolderRoot.get()->getFirstChild(); child; child = mFolderRoot.get()->findNextSibling(child))
	{
		LLFolderViewFolder *fchild = dynamic_cast<LLFolderViewFolder*>(child);
		if (fchild
			&& fchild->getViewModelItem()
			&& fchild->getViewModelItem()->getName() == "My Inventory")
		{
			fchild->setOpen(TRUE);
			break;
		}
	}
}

void LLInventoryPanel::onItemsCompletion()
{
	if (mFolderRoot.get()) mFolderRoot.get()->updateMenu();
}

void LLInventoryPanel::openSelected()
{
	LLFolderViewItem* folder_item = mFolderRoot.get()->getCurSelectedItem();
	if(!folder_item) return;
	LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getViewModelItem();
	if(!bridge) return;
	bridge->openItem();
}

void LLInventoryPanel::unSelectAll()	
{ 
	mFolderRoot.get()->setSelection(nullptr, FALSE, FALSE);
}


BOOL LLInventoryPanel::handleHover(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLView::handleHover(x, y, mask);
	if(handled)
	{
		ECursorType cursor = getWindow()->getCursor();
		if (LLInventoryModelBackgroundFetch::instance().folderFetchActive() && cursor == UI_CURSOR_ARROW)
		{
			// replace arrow cursor with arrow and hourglass cursor
			getWindow()->setCursor(UI_CURSOR_WORKING);
		}
	}
	else
	{
		getWindow()->setCursor(UI_CURSOR_ARROW);
	}
	return TRUE;
}

BOOL LLInventoryPanel::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg)
{
	BOOL handled = FALSE;

	if (mAcceptsDragAndDrop)
	{
		handled = LLPanel::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);

		// If folder view is empty the (x, y) point won't be in its rect
		// so the handler must be called explicitly.
		// but only if was not handled before. See EXT-6746.
		if (!handled && mParams.allow_drop_on_root && !mFolderRoot.get()->hasVisibleChildren())
		{
			handled = mFolderRoot.get()->handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
		}

		if (handled)
		{
			mFolderRoot.get()->setDragAndDropThisFrame();
		}
	}

	return handled;
}

void LLInventoryPanel::onFocusLost()
{
	// inventory no longer handles cut/copy/paste/delete
	if (LLEditMenuHandler::gEditMenuHandler == mFolderRoot.get())
	{
		LLEditMenuHandler::gEditMenuHandler = nullptr;
	}

	LLPanel::onFocusLost();
}

void LLInventoryPanel::onFocusReceived()
{
	// inventory now handles cut/copy/paste/delete
	LLEditMenuHandler::gEditMenuHandler = mFolderRoot.get();

	LLPanel::onFocusReceived();
}

bool LLInventoryPanel::addBadge(LLBadge * badge)
{
	bool badge_added = false;

	if (acceptsBadge())
	{
		badge_added = badge->addToView(mFolderRoot.get());
	}

	return badge_added;
}

void LLInventoryPanel::openAllFolders()
{
	mFolderRoot.get()->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_DOWN);
	mFolderRoot.get()->arrangeAll();
}

void LLInventoryPanel::setSelection(const LLUUID& obj_id, BOOL take_keyboard_focus)
{
	// Don't select objects in COF (e.g. to prevent refocus when items are worn).
	const LLInventoryObject *obj = gInventory.getObject(obj_id);
	if (obj && obj->getParentUUID() == LLAppearanceMgr::instance().getCOF())
	{
		return;
	}
	setSelectionByID(obj_id, take_keyboard_focus);
}

void LLInventoryPanel::setSelectCallback(const std::function<void (const std::deque<LLFolderViewItem*>& items, BOOL user_action)>& cb) 
{ 
	if (mFolderRoot.get())
	{
		mFolderRoot.get()->setSelectCallback(cb);
	}
}

void LLInventoryPanel::clearSelection()
{
	mSelectThisID.setNull();
}

void LLInventoryPanel::onSelectionChange(const std::deque<LLFolderViewItem*>& items, BOOL user_action)
{
	// Schedule updating the folder view context menu when all selected items become complete (STORM-373).
	mCompletionObserver->reset();
	for (std::deque<LLFolderViewItem*>::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		LLFolderViewModelItemInventory* view_model = static_cast<LLFolderViewModelItemInventory*>((*it)->getViewModelItem());
		if (view_model)
		{
			LLUUID id = view_model->getUUID();
			LLViewerInventoryItem* inv_item = mInventory->getItem(id);

			if (inv_item && !inv_item->isFinished())
			{
				mCompletionObserver->watchItem(id);
			}
		}
	}

	LLFolderView* fv = mFolderRoot.get();
	if (fv->needsAutoRename()) // auto-selecting a new user-created asset and preparing to rename
	{
		fv->setNeedsAutoRename(FALSE);
		if (!items.empty()) // new asset is visible and selected
		{
			fv->startRenamingSelectedItem();
		}
	}

	std::set<LLFolderViewItem*> selected_items = mFolderRoot.get()->getSelectionList();
	LLFolderViewItem* prev_folder_item = getItemByID(mPreviousSelectedFolder);

	if (selected_items.size() == 1)
	{
		std::set<LLFolderViewItem*>::const_iterator iter = selected_items.begin();
		LLFolderViewItem* folder_item = (*iter);
		if(folder_item && (folder_item != prev_folder_item))
		{
			LLFolderViewModelItemInventory* fve_listener = static_cast<LLFolderViewModelItemInventory*>(folder_item->getViewModelItem());
			if (fve_listener && (fve_listener->getInventoryType() == LLInventoryType::IT_CATEGORY))
			{
				if (fve_listener->getInventoryObject() && fve_listener->getInventoryObject()->getIsLinkType())
				{
					return;
				}

				if(prev_folder_item)
				{
					LLFolderBridge* prev_bridge = (LLFolderBridge*)prev_folder_item->getViewModelItem();
					if(prev_bridge)
					{
						prev_bridge->clearDisplayName();
						prev_bridge->setShowDescendantsCount(false);
						prev_folder_item->refresh();
					}
				}

				LLFolderBridge* bridge = (LLFolderBridge*)folder_item->getViewModelItem();
				if(bridge)
				{
					bridge->clearDisplayName();
					bridge->setShowDescendantsCount(true);
					folder_item->refresh();
					mPreviousSelectedFolder = bridge->getUUID();
				}
			}
		}
	}
	else
	{
		if(prev_folder_item)
		{
			LLFolderBridge* prev_bridge = (LLFolderBridge*)prev_folder_item->getViewModelItem();
			if(prev_bridge)
			{
				prev_bridge->clearDisplayName();
				prev_bridge->setShowDescendantsCount(false);
				prev_folder_item->refresh();
			}
		}
		mPreviousSelectedFolder = LLUUID();
	}

}

void LLInventoryPanel::updateFolderLabel(const LLUUID& folder_id)
{
	if(folder_id != mPreviousSelectedFolder) return;

	LLFolderViewItem* folder_item = getItemByID(mPreviousSelectedFolder);
	if(folder_item)
	{
		LLFolderBridge* bridge = (LLFolderBridge*)folder_item->getViewModelItem();
		if(bridge)
		{
			bridge->clearDisplayName();
			bridge->setShowDescendantsCount(true);
			folder_item->refresh();
		}
	}
}

void LLInventoryPanel::doCreate(const LLSD& userdata)
{
	reset_inventory_filter();
	menu_create_inventory_item(this, LLFolderBridge::sSelf.get(), userdata);
}

bool LLInventoryPanel::beginIMSession()
{
	std::set<LLFolderViewItem*> selected_items =   mFolderRoot.get()->getSelectionList();

	std::string name;

	std::vector<LLUUID> members;
//	EInstantMessage type = IM_SESSION_CONFERENCE_START;

// [RLVa:KB] - Checked: 2013-05-08 (RLVa-1.4.9)
	bool fRlvCanStartIM = true;
// [/RLVa:KB]

	std::set<LLFolderViewItem*>::const_iterator iter;
	for (iter = selected_items.begin(); iter != selected_items.end(); iter++)
	{

		LLFolderViewItem* folder_item = (*iter);
			
		if(folder_item) 
		{
			LLFolderViewModelItemInventory* fve_listener = static_cast<LLFolderViewModelItemInventory*>(folder_item->getViewModelItem());
			if (fve_listener && (fve_listener->getInventoryType() == LLInventoryType::IT_CATEGORY))
			{

				LLFolderBridge* bridge = (LLFolderBridge*)folder_item->getViewModelItem();
				if(!bridge) return true;
				LLViewerInventoryCategory* cat = bridge->getCategory();
				if(!cat) return true;
				name = cat->getName();
				LLUniqueBuddyCollector is_buddy;
				LLInventoryModel::cat_array_t cat_array;
				LLInventoryModel::item_array_t item_array;
				gInventory.collectDescendentsIf(bridge->getUUID(),
												cat_array,
												item_array,
												LLInventoryModel::EXCLUDE_TRASH,
												is_buddy);
				S32 count = item_array.size();
				if(count > 0)
				{
					//*TODO by what to replace that?
					//LLFloaterReg::showInstance("communicate");

					// create the session
					LLAvatarTracker& at = LLAvatarTracker::instance();
					LLUUID id;
					for(S32 i = 0; i < count; ++i)
					{
						id = item_array.at(i)->getCreatorUUID();
// [RLVa:KB] - Checked: 2013-05-08 (RLVa-1.4.9)
						if ( (at.isBuddyOnline(id)) && (members.end() == std::find(members.begin(), members.end(), id)) )
						{
							fRlvCanStartIM &= RlvActions::canStartIM(id);
							members.push_back(id);
						}
// [/RLVa:KB]
//						if(at.isBuddyOnline(id))
//						{
//							members.push_back(id);
//						}
					}
				}
			}
			else
			{
				LLInvFVBridge* listenerp = (LLInvFVBridge*)folder_item->getViewModelItem();

				if (listenerp->getInventoryType() == LLInventoryType::IT_CALLINGCARD)
				{
					LLInventoryItem* inv_item = gInventory.getItem(listenerp->getUUID());

					if (inv_item)
					{
						LLAvatarTracker& at = LLAvatarTracker::instance();
						LLUUID id = inv_item->getCreatorUUID();

// [RLVa:KB] - Checked: 2013-05-08 (RLVa-1.4.9)
						if ( (at.isBuddyOnline(id)) && (members.end() == std::find(members.begin(), members.end(), id)) )
						{
							fRlvCanStartIM &= RlvActions::canStartIM(id);
							members.push_back(id);
						}
// [/RLVa:KB]
//						if(at.isBuddyOnline(id))
//						{
//							members.push_back(id);
//						}
					}
				} //if IT_CALLINGCARD
			} //if !IT_CATEGORY
		}
	} //for selected_items	

	// the session_id is randomly generated UUID which will be replaced later
	// with a server side generated number

// [RLVa:KB] - Checked: 2013-05-08 (RLVa-1.4.9)
	if (!fRlvCanStartIM)
	{
		make_ui_sound("UISndInvalidOp");
		RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTCONF);
		return true;
	}
// [/RLVa:KB]

	if (name.empty())
	{
		name = LLTrans::getString("conference-title");
	}

// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
	if (!members.empty())
	{
		if (members.size() > 1)
			LLAvatarActions::startConference(members);
		else
			LLAvatarActions::startIM(members[0]);
	}
// [/RLVa:KB]
//	LLUUID session_id = gIMMgr->addSession(name, type, members[0], members);
//	if (session_id != LLUUID::null)
//	{
//		LLFloaterIMContainer::getInstance()->showConversation(session_id);
//	}
		
	return true;
}

void LLInventoryPanel::fileUploadLocation(const LLSD& userdata)
{
    const std::string param = userdata.asString();
    if (param == "model")
    {
        gSavedPerAccountSettings.setString("ModelUploadFolder", LLFolderBridge::sSelf.get()->getUUID().asString());
    }
    else if (param == "texture")
    {
        gSavedPerAccountSettings.setString("TextureUploadFolder", LLFolderBridge::sSelf.get()->getUUID().asString());
    }
    else if (param == "sound")
    {
        gSavedPerAccountSettings.setString("SoundUploadFolder", LLFolderBridge::sSelf.get()->getUUID().asString());
    }
    else if (param == "animation")
    {
        gSavedPerAccountSettings.setString("AnimationUploadFolder", LLFolderBridge::sSelf.get()->getUUID().asString());
    }
}

void LLInventoryPanel::purgeSelectedItems()
{
    if (!mFolderRoot.get()) return;

    const std::set<LLFolderViewItem*> inventory_selected = mFolderRoot.get()->getSelectionList();
    if (inventory_selected.empty()) return;
    LLSD args;
    S32 count = inventory_selected.size();
    for (auto it : inventory_selected)
    {
        LLUUID item_id = static_cast<LLFolderViewModelItemInventory*>(it->getViewModelItem())->getUUID();
        LLInventoryModel::cat_array_t cats;
        LLInventoryModel::item_array_t items;
        gInventory.collectDescendents(item_id, cats, items, LLInventoryModel::INCLUDE_TRASH);
        count += items.size() + cats.size();
    }
    args["COUNT"] = count;
    LLNotificationsUtil::add("PurgeSelectedItems", args, LLSD(), boost::bind(&LLInventoryPanel::callbackPurgeSelectedItems, this, _1, _2));
}

void LLInventoryPanel::callbackPurgeSelectedItems(const LLSD& notification, const LLSD& response)
{
    if (!mFolderRoot.get()) return;

    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0)
    {
        const std::set<LLFolderViewItem*> inventory_selected = mFolderRoot.get()->getSelectionList();
        if (inventory_selected.empty()) return;

        std::set<LLFolderViewItem*>::const_iterator it = inventory_selected.begin();
        const std::set<LLFolderViewItem*>::const_iterator it_end = inventory_selected.end();
        for (; it != it_end; ++it)
        {
            LLUUID item_id = static_cast<LLFolderViewModelItemInventory*>((*it)->getViewModelItem())->getUUID();
            remove_inventory_object(item_id, NULL);
        }
    }
}

bool LLInventoryPanel::attachObject(const LLSD& userdata)
{
	// Copy selected item UUIDs to a vector.
	std::set<LLFolderViewItem*> selected_items = mFolderRoot.get()->getSelectionList();
	uuid_vec_t items;
	for (auto selected_item : selected_items)
    {
		items.push_back(static_cast<LLFolderViewModelItemInventory*>(selected_item->getViewModelItem())->getUUID());
	}

	// Attach selected items.
	LLViewerAttachMenu::attachObjects(items, userdata.asString());

	gFocusMgr.setKeyboardFocus(nullptr);

	return true;
}

BOOL LLInventoryPanel::getSinceLogoff()
{
	return getFilter().isSinceLogoff();
}

// DEBUG ONLY
// static 
void LLInventoryPanel::dumpSelectionInformation(void* user_data)
{
	LLInventoryPanel* iv = (LLInventoryPanel*)user_data;
	iv->mFolderRoot.get()->dumpSelectionInformation();
}

BOOL is_inventorysp_active()
{
	LLSidepanelInventory *sidepanel_inventory =	LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
	if (!sidepanel_inventory || !sidepanel_inventory->isInVisibleChain()) return FALSE;
	return sidepanel_inventory->isMainInventoryPanelActive();
}

// static
LLInventoryPanel* LLInventoryPanel::getActiveInventoryPanel(BOOL auto_open)
{
	S32 z_min = S32_MAX;
	LLInventoryPanel* res = nullptr;
	LLFloater* active_inv_floaterp = nullptr;

	LLFloater* floater_inventory = LLFloaterReg::getInstance("inventory");
	if (!floater_inventory)
	{
		LL_WARNS() << "Could not find My Inventory floater" << LL_ENDL;
		return FALSE;
	}

	LLSidepanelInventory *inventory_panel =	LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");

	// Iterate through the inventory floaters and return whichever is on top.
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("inventory");
	for (auto iter : inst_list)
    {
		LLFloaterSidePanelContainer* inventory_floater = dynamic_cast<LLFloaterSidePanelContainer*>(iter);
		if (inventory_floater)
        {
            inventory_panel = inventory_floater->findChild<LLSidepanelInventory>("main_panel");
            if (inventory_panel && inventory_floater->getVisible())
            {
                S32 z_order = gFloaterView->getZOrder(inventory_floater);
                if (z_order < z_min)
                {
                    res = inventory_panel->getActivePanel();
                    z_min = z_order;
                    active_inv_floaterp = inventory_floater;
                }
            }
        }
	}

	if (res)
	{
		// Make sure the floater is not minimized (STORM-438).
		if (active_inv_floaterp && active_inv_floaterp->isMinimized())
		{
			active_inv_floaterp->setMinimized(FALSE);
		}
	}	
//	else if (auto_open)
// [RLVa:KB] - Checked: 2012-05-15 (RLVa-1.4.6)
	else if ( (auto_open) && (LLFloaterReg::canShowInstance(floater_inventory->getInstanceName())) )
	{
// [/RLVa:KB]
		floater_inventory->openFloater();

		res = inventory_panel->getActivePanel();
	}

	return res;
}

//static
void LLInventoryPanel::openInventoryPanelAndSetSelection(BOOL auto_open, const LLUUID& obj_id, BOOL main_panel, BOOL take_keyboard_focus, BOOL reset_filter)
{
	LLFloaterSidePanelContainer::showPanel("inventory", LLSD());
	
	LLSidepanelInventory* sidepanel_inventory = LLFloaterSidePanelContainer::getPanel<LLSidepanelInventory>("inventory");
	bool in_inbox = (gInventory.isObjectDescendentOf(obj_id, gInventory.findCategoryUUIDForType(LLFolderType::FT_INBOX)));

	if (main_panel && !in_inbox)
	{
		sidepanel_inventory->selectAllItemsPanel();
	}
	LLInventoryPanel *active_panel = LLInventoryPanel::getActiveInventoryPanel(auto_open);

	if (active_panel)
	{
		LL_DEBUGS("Messaging") << "Highlighting" << obj_id  << LL_ENDL;

		if (reset_filter)
		{
			reset_inventory_filter();
		}

		if (in_inbox)
		{
			LLInventoryPanel * inventory_panel = nullptr;
			sidepanel_inventory->openInbox();
			inventory_panel = sidepanel_inventory->getInboxPanel();

			if (inventory_panel)
			{
				inventory_panel->setSelection(obj_id, take_keyboard_focus);
			}
		}
		else
		{
			LLFloater* floater_inventory = LLFloaterReg::getInstance("inventory");
			if (floater_inventory)
			{
				floater_inventory->setFocus(TRUE);
			}
			active_panel->setSelection(obj_id, take_keyboard_focus);
		}
	}
}

void LLInventoryPanel::addHideFolderType(LLFolderType::EType folder_type)
{
	getFilter().setFilterCategoryTypes(getFilter().getFilterCategoryTypes() & ~(1ULL << folder_type));
}

BOOL LLInventoryPanel::getIsHiddenFolderType(LLFolderType::EType folder_type) const
{
	return !(getFilter().getFilterCategoryTypes() & (1ULL << folder_type));
}

void LLInventoryPanel::addItemID( const LLUUID& id, LLFolderViewItem*   itemp )
{
	mItemMap.insert_or_assign(id, itemp);
}

void LLInventoryPanel::removeItemID(const LLUUID& id)
{
	LLInventoryModel::cat_array_t categories;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendents(id, categories, items, TRUE);

	mItemMap.erase(id);

	for (LLViewerInventoryCategory* category : categories)
    {
		mItemMap.erase(category->getUUID());
}

	for (LLViewerInventoryItem* item : items)
    {
		mItemMap.erase(item->getUUID());
	}
}

LLTrace::BlockTimerStatHandle FTM_GET_ITEM_BY_ID("Get FolderViewItem by ID");
LLFolderViewItem* LLInventoryPanel::getItemByID(const LLUUID& id)
{
	LL_RECORD_BLOCK_TIME(FTM_GET_ITEM_BY_ID);

	auto map_it = mItemMap.find(id);
	if (map_it != mItemMap.end())
	{
		return map_it->second;
	}

	return nullptr;
}

LLFolderViewFolder* LLInventoryPanel::getFolderByID(const LLUUID& id)
{
	LLFolderViewItem* item = getItemByID(id);
	return dynamic_cast<LLFolderViewFolder*>(item);
}


void LLInventoryPanel::setSelectionByID( const LLUUID& obj_id, BOOL    take_keyboard_focus )
{
	LLFolderViewItem* itemp = getItemByID(obj_id);
	if(itemp && itemp->getViewModelItem())
	{
		itemp->arrangeAndSet(TRUE, take_keyboard_focus);
		mSelectThisID.setNull();
		return;
	}
	else
	{
		// save the desired item to be selected later (if/when ready)
		mSelectThisID = obj_id;
	}
}

void LLInventoryPanel::updateSelection()
{
	if (mSelectThisID.notNull())
	{
		setSelectionByID(mSelectThisID, false);
	}
}

void LLInventoryPanel::doToSelected(const LLSD& userdata)
{
	if (("purge" == userdata.asString()))
	{
		purgeSelectedItems();
		return;
	}
	LLInventoryAction::doToSelected(mInventory, mFolderRoot.get(), userdata.asString());

	return;
}

BOOL LLInventoryPanel::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;
	switch (key)
	{
	case KEY_RETURN:
		// Open selected items if enter key hit on the inventory panel
		if (mask == MASK_NONE)
		{

// @TODO$: Rider: This code is dead with Outbox, however should something similar be 
//  done for VMM?
//  
// 			//Don't allow attaching or opening items from Merchant Outbox
// 			LLFolderViewItem* folder_item = mFolderRoot.get()->getCurSelectedItem();
// 			if(folder_item)
// 			{
// 				LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getViewModelItem();
// 				if(bridge && bridge->is() && (bridge->getInventoryType() != LLInventoryType::IT_CATEGORY))
// 				{
// 					return handled;
// 				}
// 			}

			LLInventoryAction::doToSelected(mInventory, mFolderRoot.get(), "open");
			handled = TRUE;
		}
		break;
	case KEY_DELETE:
#if LL_DARWIN
	case KEY_BACKSPACE:
#endif
		// Delete selected items if delete or backspace key hit on the inventory panel
		// Note: on Mac laptop keyboards, backspace and delete are one and the same
		if (isSelectionRemovable() && (mask == MASK_NONE))
		{
			LLInventoryAction::doToSelected(mInventory, mFolderRoot.get(), "delete");
			handled = TRUE;
		}
		break;
	}
	return handled;
}

bool LLInventoryPanel::isSelectionRemovable()
{
	bool can_delete = false;
	if (mFolderRoot.get())
	{
		std::set<LLFolderViewItem*> selection_set = mFolderRoot.get()->getSelectionList();
		if (!selection_set.empty()) 
		{
			can_delete = true;
			for (auto item : selection_set)
            {
                const LLFolderViewModelItemInventory *listener = static_cast<const LLFolderViewModelItemInventory*>(item->getViewModelItem());
				if (!listener)
				{
					can_delete = false;
				}
				else
				{
					can_delete &= listener->isItemRemovable() && !listener->isItemInTrash();
				}
			}
		}
	}
	return can_delete;
}

/************************************************************************/
/* Recent Inventory Panel related class                                 */
/************************************************************************/
class LLInventoryRecentItemsPanel;
static LLDefaultChildRegistry::Register<LLInventoryRecentItemsPanel> t_recent_inventory_panel("recent_inventory_panel");

static const LLRecentInventoryBridgeBuilder RECENT_ITEMS_BUILDER;
class LLInventoryRecentItemsPanel final : public LLInventoryPanel
{
public:
	struct Params :	public LLInitParam::Block<Params, LLInventoryPanel::Params>
	{};

	void initFromParams(const Params& p)
	{
		LLInventoryPanel::initFromParams(p);
		// turn on inbox for recent items
		getFilter().setFilterCategoryTypes(getFilter().getFilterCategoryTypes() | (1ULL << LLFolderType::FT_INBOX));
        // turn off marketplace for recent items
        getFilter().setFilterNoMarketplaceFolder();
	}

protected:
	LLInventoryRecentItemsPanel (const Params&);
	friend class LLUICtrlFactory;
};

LLInventoryRecentItemsPanel::LLInventoryRecentItemsPanel( const Params& params)
: LLInventoryPanel(params)
{
	// replace bridge builder to have necessary View bridges.
	mInvFVBridgeBuilder = &RECENT_ITEMS_BUILDER;
}

namespace LLInitParam
{
	void TypeValues<LLFolderType::EType>::declareValues()
	{
		declare(LLFolderType::lookup(LLFolderType::FT_TEXTURE)          , LLFolderType::FT_TEXTURE);
		declare(LLFolderType::lookup(LLFolderType::FT_SOUND)            , LLFolderType::FT_SOUND);
		declare(LLFolderType::lookup(LLFolderType::FT_CALLINGCARD)      , LLFolderType::FT_CALLINGCARD);
		declare(LLFolderType::lookup(LLFolderType::FT_LANDMARK)         , LLFolderType::FT_LANDMARK);
		declare(LLFolderType::lookup(LLFolderType::FT_CLOTHING)         , LLFolderType::FT_CLOTHING);
		declare(LLFolderType::lookup(LLFolderType::FT_OBJECT)           , LLFolderType::FT_OBJECT);
		declare(LLFolderType::lookup(LLFolderType::FT_NOTECARD)         , LLFolderType::FT_NOTECARD);
		declare(LLFolderType::lookup(LLFolderType::FT_ROOT_INVENTORY)   , LLFolderType::FT_ROOT_INVENTORY);
		declare(LLFolderType::lookup(LLFolderType::FT_LSL_TEXT)         , LLFolderType::FT_LSL_TEXT);
		declare(LLFolderType::lookup(LLFolderType::FT_BODYPART)         , LLFolderType::FT_BODYPART);
		declare(LLFolderType::lookup(LLFolderType::FT_TRASH)            , LLFolderType::FT_TRASH);
		declare(LLFolderType::lookup(LLFolderType::FT_SNAPSHOT_CATEGORY), LLFolderType::FT_SNAPSHOT_CATEGORY);
		declare(LLFolderType::lookup(LLFolderType::FT_LOST_AND_FOUND)   , LLFolderType::FT_LOST_AND_FOUND);
		declare(LLFolderType::lookup(LLFolderType::FT_ANIMATION)        , LLFolderType::FT_ANIMATION);
		declare(LLFolderType::lookup(LLFolderType::FT_GESTURE)          , LLFolderType::FT_GESTURE);
		declare(LLFolderType::lookup(LLFolderType::FT_FAVORITE)         , LLFolderType::FT_FAVORITE);
		declare(LLFolderType::lookup(LLFolderType::FT_ENSEMBLE_START)   , LLFolderType::FT_ENSEMBLE_START);
		declare(LLFolderType::lookup(LLFolderType::FT_ENSEMBLE_END)     , LLFolderType::FT_ENSEMBLE_END);
		declare(LLFolderType::lookup(LLFolderType::FT_CURRENT_OUTFIT)   , LLFolderType::FT_CURRENT_OUTFIT);
		declare(LLFolderType::lookup(LLFolderType::FT_OUTFIT)           , LLFolderType::FT_OUTFIT);
		declare(LLFolderType::lookup(LLFolderType::FT_MY_OUTFITS)       , LLFolderType::FT_MY_OUTFITS);
		declare(LLFolderType::lookup(LLFolderType::FT_MESH )            , LLFolderType::FT_MESH );
		declare(LLFolderType::lookup(LLFolderType::FT_INBOX)            , LLFolderType::FT_INBOX);
		declare(LLFolderType::lookup(LLFolderType::FT_OUTBOX)           , LLFolderType::FT_OUTBOX);
		declare(LLFolderType::lookup(LLFolderType::FT_BASIC_ROOT)       , LLFolderType::FT_BASIC_ROOT);
		declare(LLFolderType::lookup(LLFolderType::FT_MARKETPLACE_LISTINGS)   , LLFolderType::FT_MARKETPLACE_LISTINGS);
		declare(LLFolderType::lookup(LLFolderType::FT_MARKETPLACE_STOCK), LLFolderType::FT_MARKETPLACE_STOCK);
		declare(LLFolderType::lookup(LLFolderType::FT_MARKETPLACE_VERSION), LLFolderType::FT_MARKETPLACE_VERSION);
		declare(LLFolderType::lookup(LLFolderType::FT_SUITCASE), LLFolderType::FT_SUITCASE);
	}
}
