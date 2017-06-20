/** 
 * @file llinventorybridge.h
 * @brief Implementation of the Inventory-Folder-View-Bridge classes.
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

#ifndef LL_LLINVENTORYBRIDGE_H
#define LL_LLINVENTORYBRIDGE_H

#include "llcallingcard.h"
#include "llfloaterproperties.h"
#include "llfolderviewmodel.h"
#include "llinventorymodel.h"
#include "llinventoryobserver.h"
#include "llinventorypanel.h"
#include "llviewercontrol.h"
#include "llviewerwearable.h"
#include "lltooldraganddrop.h"
#include "lllandmarklist.h"
#include "llfolderviewitem.h"

class LLInventoryFilter;
class LLInventoryPanel;
class LLInventoryModel;
class LLMenuGL;
class LLCallingCardObserver;
class LLViewerJointAttachment;
class LLFolderView;

typedef std::vector<std::string> menuentry_vec_t;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInvFVBridge
//
// Short for Inventory-Folder-View-Bridge. This is an
// implementation class to be able to view inventory items.
//
// You'll want to call LLInvItemFVELister::createBridge() to actually create
// an instance of this class. This helps encapsulate the
// functionality a bit. (except for folders, you can create those
// manually...)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInvFVBridge : public LLFolderViewModelItemInventory
{
public:
	// This method is a convenience function which creates the correct
	// type of bridge based on some basic information
	static LLInvFVBridge* createBridge(LLAssetType::EType asset_type,
									   LLAssetType::EType actual_asset_type,
									   LLInventoryType::EType inv_type,
									   LLInventoryPanel* inventory,
									   LLFolderViewModelInventory* view_model,
									   LLFolderView* root,
									   const LLUUID& uuid,
									   U32 flags = 0x00);
	virtual ~LLInvFVBridge() {}

	bool canShare() const;
	bool canListOnMarketplace() const;
	bool canListOnMarketplaceNow() const;

	//--------------------------------------------------------------------
	// LLInvFVBridge functionality
	//--------------------------------------------------------------------
	const LLUUID& getUUID() const override { return mUUID; }
	virtual void clearDisplayName() { mDisplayName.clear(); }
	virtual void restoreItem() {}
	virtual void restoreToWorld() {}

	//--------------------------------------------------------------------
	// Inherited LLFolderViewModelItemInventory functions
	//--------------------------------------------------------------------
	const std::string& getName() const override;
	const std::string& getDisplayName() const override;
	const std::string& getSearchableName() const override { return mSearchableName; }

	PermissionMask getPermissionMask() const override;
	LLFolderType::EType getPreferredType() const override;
	time_t getCreationDate() const override;
	void setCreationDate(time_t creation_date_utc) override;
	LLFontGL::StyleFlags getLabelStyle() const override { return LLFontGL::NORMAL; }
	std::string getLabelSuffix() const override { return LLStringUtil::null; }
	void openItem() override {}
	void closeItem() override {}
	void showProperties() override;
	BOOL isItemRenameable() const override { return TRUE; }
	//virtual BOOL renameItem(const std::string& new_name) {}
	BOOL isItemRemovable() const override;
	BOOL isItemMovable() const override;
	BOOL isItemInTrash() const override;
	virtual BOOL isLink() const;
	virtual BOOL isLibraryItem() const;
	//virtual BOOL removeItem() = 0;
	void removeBatch(std::vector<LLFolderViewModelItem*>& batch) override;
	void move(LLFolderViewModelItem* new_parent_bridge) override {}
	BOOL isItemCopyable() const override { return FALSE; }
	BOOL copyToClipboard() const override;
	BOOL cutToClipboard() override;
	bool isCutToClipboard() override;
	BOOL isClipboardPasteable() const override;
	virtual BOOL isClipboardPasteableAsLink() const;
	void pasteFromClipboard() override {}
	void pasteLinkFromClipboard() override {}
	void getClipboardEntries(bool show_asset_id, menuentry_vec_t &items, 
							 menuentry_vec_t &disabled_items, U32 flags);
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	LLToolDragAndDrop::ESource getDragSource() const override;
	BOOL startDrag(EDragAndDropType* type, LLUUID* id) const override;

	BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data,
							std::string& tooltip_msg) override { return FALSE; }

	LLInventoryType::EType getInventoryType() const override { return mInvType; }
	LLWearableType::EType getWearableType() const override { return LLWearableType::WT_NONE; }
        EInventorySortGroup getSortGroup()  const override { return SG_ITEM; }
	LLInventoryObject* getInventoryObject() const override;


	//--------------------------------------------------------------------
	// Convenience functions for adding various common menu options.
	//--------------------------------------------------------------------
protected:
	virtual void addTrashContextMenuOptions(menuentry_vec_t &items,
											menuentry_vec_t &disabled_items);
	virtual void addDeleteContextMenuOptions(menuentry_vec_t &items,
											 menuentry_vec_t &disabled_items);
	virtual void addOpenRightClickMenuOption(menuentry_vec_t &items);
	virtual void addMarketplaceContextMenuOptions(U32 flags,
											 menuentry_vec_t &items,
											 menuentry_vec_t &disabled_items);
	virtual void addLinkReplaceMenuOption(menuentry_vec_t& items,
										  menuentry_vec_t& disabled_items);

protected:
	LLInvFVBridge(LLInventoryPanel* inventory, LLFolderView* root, const LLUUID& uuid);

	LLInventoryModel* getInventoryModel() const;
	LLInventoryFilter* getInventoryFilter() const;
	
	BOOL isLinkedObjectInTrash() const; // Is this obj or its baseobj in the trash?
	BOOL isLinkedObjectMissing() const; // Is this a linked obj whose baseobj is not in inventory?

	BOOL isAgentInventory() const; // false if lost or in the inventory library
	BOOL isCOFFolder() const;       // true if COF or descendant of
	BOOL isInboxFolder() const;     // true if COF or descendant of   marketplace inbox

	BOOL isMarketplaceListingsFolder() const;     // true if descendant of Marketplace listings folder

	virtual BOOL isItemPermissive() const;
	static void changeItemParent(LLInventoryModel* model,
								 LLViewerInventoryItem* item,
								 const LLUUID& new_parent,
								 BOOL restamp);
	static void changeCategoryParent(LLInventoryModel* model,
									 LLViewerInventoryCategory* item,
									 const LLUUID& new_parent,
									 BOOL restamp);
	void removeBatchNoCheck(std::vector<LLFolderViewModelItem*>& batch);
    
    BOOL callback_cutToClipboard(const LLSD& notification, const LLSD& response);
    BOOL perform_cutToClipboard();

	LLHandle<LLInventoryPanel> mInventoryPanel;
	LLFolderView* mRoot;
	const LLUUID mUUID;	// item id
	LLInventoryType::EType mInvType;
	bool						mIsLink;
	LLTimer						mTimeSinceRequestStart;
	mutable std::string			mDisplayName;
	mutable std::string			mSearchableName;

	void purgeItem(LLInventoryModel *model, const LLUUID &uuid);
	void removeObject(LLInventoryModel *model, const LLUUID &uuid);
	virtual void buildDisplayName() const {}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryFolderViewModelBuilder
//
// This class intended to build Folder View Model via LLInvFVBridge::createBridge.
// It can be overridden with another way of creation necessary Inventory Folder View Models.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryFolderViewModelBuilder
{
public:
 	LLInventoryFolderViewModelBuilder() {}
 	virtual ~LLInventoryFolderViewModelBuilder() {}
	virtual LLInvFVBridge* createBridge(LLAssetType::EType asset_type,
										LLAssetType::EType actual_asset_type,
										LLInventoryType::EType inv_type,
										LLInventoryPanel* inventory,
										LLFolderViewModelInventory* view_model,
										LLFolderView* root,
										const LLUUID& uuid,
										U32 flags = 0x00) const;
};

class LLItemBridge : public LLInvFVBridge
{
public:
	LLItemBridge(LLInventoryPanel* inventory, 
				 LLFolderView* root,
				 const LLUUID& uuid) :
		LLInvFVBridge(inventory, root, uuid) {}

	typedef std::function<void(std::string& slurl)> slurl_callback_t;

	void performAction(LLInventoryModel* model, std::string action) override;
	void selectItem() override;
	void restoreItem() override;
	void restoreToWorld() override;
	virtual void gotoItem();
	LLUIImagePtr getIcon() const override;
	std::string getLabelSuffix() const override;
	LLFontGL::StyleFlags getLabelStyle() const override;
	PermissionMask getPermissionMask() const override;
	time_t getCreationDate() const override;
	BOOL isItemRenameable() const override;
	BOOL renameItem(const std::string& new_name) override;
	BOOL removeItem() override;
	BOOL isItemCopyable() const override;
	bool hasChildren() const override { return FALSE; }
	BOOL isUpToDate() const override { return TRUE; }
	LLUIImagePtr getIconOverlay() const override;

	LLViewerInventoryItem* getItem() const;

protected:
	BOOL confirmRemoveItem(const LLSD& notification, const LLSD& response);
	BOOL isItemPermissive() const override;
	void buildDisplayName() const override;
	void doActionOnCurSelectedLandmark(LLLandmarkList::loaded_callback_t cb);

private:
	void doShowOnMap(LLLandmark* landmark);
};

class LLFolderBridge : public LLInvFVBridge
{
public:
	LLFolderBridge(LLInventoryPanel* inventory, 
				   LLFolderView* root,
				   const LLUUID& uuid) 
	:	LLInvFVBridge(inventory, root, uuid),
		mCallingCards(FALSE),
		mWearables(FALSE),
		mIsLoading(false)
	{}
		
	BOOL dragItemIntoFolder(LLInventoryItem* inv_item, BOOL drop, std::string& tooltip_msg, BOOL user_confirm = TRUE);
	BOOL dragCategoryIntoFolder(LLInventoryCategory* inv_category, BOOL drop, std::string& tooltip_msg, BOOL is_link = FALSE, BOOL user_confirm = TRUE);
    void callback_dropItemIntoFolder(const LLSD& notification, const LLSD& response, LLInventoryItem* inv_item);
    void callback_dropCategoryIntoFolder(const LLSD& notification, const LLSD& response, LLInventoryCategory* inv_category);

	void buildDisplayName() const override;

	void performAction(LLInventoryModel* model, std::string action) override;
	void openItem() override;
	void closeItem() override;
	BOOL isItemRenameable() const override;
	void selectItem() override;
	void restoreItem() override;

	LLFolderType::EType getPreferredType() const override;
	LLUIImagePtr getIcon() const override;
	LLUIImagePtr getIconOpen() const override;
	LLUIImagePtr getIconOverlay() const override;
	static LLUIImagePtr getIcon(LLFolderType::EType preferred_type);
	std::string getLabelSuffix() const override;
	LLFontGL::StyleFlags getLabelStyle() const override;

	BOOL renameItem(const std::string& new_name) override;

	BOOL removeItem() override;
	BOOL removeSystemFolder();
	bool removeItemResponse(const LLSD& notification, const LLSD& response);
    void updateHierarchyCreationDate(time_t date);

	void pasteFromClipboard() override;
	void pasteLinkFromClipboard() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	bool hasChildren() const override;
	BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data,
							std::string& tooltip_msg) override;

	BOOL isItemRemovable() const override;
	BOOL isItemMovable() const override;
	BOOL isUpToDate() const override;
	BOOL isItemCopyable() const override;
	BOOL isClipboardPasteable() const override;
	BOOL isClipboardPasteableAsLink() const override;
	
	EInventorySortGroup getSortGroup()  const override;
	void update() override;

	static void createWearable(LLFolderBridge* bridge, LLWearableType::EType type);

	LLViewerInventoryCategory* getCategory() const;
	LLHandle<LLFolderBridge> getHandle() { mHandle.bind(this); return mHandle; }

	bool isLoading() { return mIsLoading; }

protected:
	void buildContextMenuOptions(U32 flags, menuentry_vec_t& items,   menuentry_vec_t& disabled_items);
	void buildContextMenuFolderOptions(U32 flags, menuentry_vec_t& items,   menuentry_vec_t& disabled_items);

	//--------------------------------------------------------------------
	// Menu callbacks
	//--------------------------------------------------------------------
	static void pasteClipboard(void* user_data);
	static void createNewShirt(void* user_data);
	static void createNewPants(void* user_data);
	static void createNewShoes(void* user_data);
	static void createNewSocks(void* user_data);
	static void createNewJacket(void* user_data);
	static void createNewSkirt(void* user_data);
	static void createNewGloves(void* user_data);
	static void createNewUndershirt(void* user_data);
	static void createNewUnderpants(void* user_data);
	static void createNewShape(void* user_data);
	static void createNewSkin(void* user_data);
	static void createNewHair(void* user_data);
	static void createNewEyes(void* user_data);

	BOOL checkFolderForContentsOfType(LLInventoryModel* model, LLInventoryCollectFunctor& typeToCheck);

	void modifyOutfit(BOOL append);
	void copyOutfitToClipboard();
	void determineFolderType();

	void dropToFavorites(LLInventoryItem* inv_item);
	void dropToOutfit(LLInventoryItem* inv_item, BOOL move_is_into_current_outfit);

	//--------------------------------------------------------------------
	// Messy hacks for handling folder options
	//--------------------------------------------------------------------
public:
	static LLHandle<LLFolderBridge> sSelf;
	static void staticFolderOptionsMenu();

protected:
    void callback_pasteFromClipboard(const LLSD& notification, const LLSD& response);
    void perform_pasteFromClipboard();
    void gatherMessage(std::string& message, S32 depth, LLError::ELevel log_level);
    LLUIImagePtr getFolderIcon(BOOL is_open) const;

	bool							mCallingCards;
	bool							mWearables;
	bool							mIsLoading;
	LLTimer							mTimeSinceRequestStart;
    std::string                     mMessage;
	LLRootHandle<LLFolderBridge> mHandle;
};

class LLTextureBridge : public LLItemBridge
{
public:
	LLTextureBridge(LLInventoryPanel* inventory, 
					LLFolderView* root,
					const LLUUID& uuid, 
					LLInventoryType::EType type) :
		LLItemBridge(inventory, root, uuid)
	{
		mInvType = type;
	}

	LLUIImagePtr getIcon() const override;
	void openItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	void performAction(LLInventoryModel* model, std::string action) override;
	bool canSaveTexture(void);
};

class LLSoundBridge : public LLItemBridge
{
public:
 	LLSoundBridge(LLInventoryPanel* inventory, 
				  LLFolderView* root,
				  const LLUUID& uuid) :
		LLItemBridge(inventory, root, uuid) {}

	void openItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	void performAction(LLInventoryModel* model, std::string action) override;
	static void openSoundPreview(void*);
};

class LLLandmarkBridge : public LLItemBridge
{
public:
 	LLLandmarkBridge(LLInventoryPanel* inventory, 
					 LLFolderView* root,
					 const LLUUID& uuid, 
					 U32 flags = 0x00);
	void performAction(LLInventoryModel* model, std::string action) override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	LLUIImagePtr getIcon() const override;
	void openItem() override;
protected:
	BOOL mVisited;
};

class LLCallingCardBridge : public LLItemBridge
{
public:
	LLCallingCardBridge(LLInventoryPanel* inventory, 
						LLFolderView* folder,
						const LLUUID& uuid );
	~LLCallingCardBridge();
	std::string getLabelSuffix() const override;
	//virtual const std::string& getDisplayName() const;
	LLUIImagePtr getIcon() const override;
	void performAction(LLInventoryModel* model, std::string action) override;
	void openItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data,
							std::string& tooltip_msg) override;
	void refreshFolderViewItem();
	void checkSearchBySuffixChanges();
protected:
	LLCallingCardObserver* mObserver;
};

class LLNotecardBridge : public LLItemBridge
{
public:
	LLNotecardBridge(LLInventoryPanel* inventory, 
					 LLFolderView* root,
					 const LLUUID& uuid) :
		LLItemBridge(inventory, root, uuid) {}

	void openItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
};

class LLGestureBridge : public LLItemBridge
{
public:
	LLGestureBridge(LLInventoryPanel* inventory, 
					LLFolderView* root,
					const LLUUID& uuid) :
		LLItemBridge(inventory, root, uuid) {}
	// Only suffix for gesture items, not task items, because only
	// gestures in your inventory can be active.
	LLFontGL::StyleFlags getLabelStyle() const override;
	std::string getLabelSuffix() const override;
	void performAction(LLInventoryModel* model, std::string action) override;
	void openItem() override;
	BOOL removeItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	static void playGesture(const LLUUID& item_id);
};

class LLAnimationBridge : public LLItemBridge
{
public:
	LLAnimationBridge(LLInventoryPanel* inventory, 
					  LLFolderView* root, 
					  const LLUUID& uuid) :
		LLItemBridge(inventory, root, uuid) {}

	void performAction(LLInventoryModel* model, std::string action) override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	void openItem() override;
};

class LLObjectBridge : public LLItemBridge
{
public:
	LLObjectBridge(LLInventoryPanel* inventory, 
				   LLFolderView* root, 
				   const LLUUID& uuid, 
				   LLInventoryType::EType type, 
				   U32 flags);
	LLUIImagePtr	getIcon() const override;
	void			performAction(LLInventoryModel* model, std::string action) override;
	void			openItem() override;
	BOOL isItemWearable() const override { return TRUE; }
	std::string getLabelSuffix() const override;
	void			buildContextMenu(LLMenuGL& menu, U32 flags) override;
	BOOL renameItem(const std::string& new_name) override;
	LLInventoryObject* getObject() const;
protected:
	static LLUUID sContextMenuItemID;  // Only valid while the context menu is open.
	U32 mAttachPt;
	BOOL mIsMultiObject;
};

class LLLSLTextBridge : public LLItemBridge
{
public:
	LLLSLTextBridge(LLInventoryPanel* inventory, 
					LLFolderView* root, 
					const LLUUID& uuid ) :
		LLItemBridge(inventory, root, uuid) {}

	void openItem() override;
};

class LLWearableBridge : public LLItemBridge
{
public:
	LLWearableBridge(LLInventoryPanel* inventory, 
					 LLFolderView* root, 
					 const LLUUID& uuid, 
					 LLAssetType::EType asset_type, 
					 LLInventoryType::EType inv_type, 
					 LLWearableType::EType wearable_type);
	LLUIImagePtr getIcon() const override;
	void	performAction(LLInventoryModel* model, std::string action) override;
	void	openItem() override;
	BOOL isItemWearable() const override { return TRUE; }
	void	buildContextMenu(LLMenuGL& menu, U32 flags) override;
	std::string getLabelSuffix() const override;
	BOOL renameItem(const std::string& new_name) override;
	LLWearableType::EType getWearableType() const override { return mWearableType; }

	static void		onWearOnAvatar( void* userdata );	// Access to wearOnAvatar() from menu
	static BOOL		canWearOnAvatar( void* userdata );
	static void		onWearOnAvatarArrived( LLViewerWearable* wearable, void* userdata );
	void			wearOnAvatar();

	static void		onWearAddOnAvatarArrived( LLViewerWearable* wearable, void* userdata );
	void			wearAddOnAvatar();

	static BOOL		canEditOnAvatar( void* userdata );	// Access to editOnAvatar() from menu
	static void		onEditOnAvatar( void* userdata );
	void			editOnAvatar();

	static BOOL		canRemoveFromAvatar( void* userdata );
	static void 	removeAllClothesFromAvatar();
	void			removeFromAvatar();
protected:
	LLAssetType::EType mAssetType;
	LLWearableType::EType  mWearableType;
};

class LLLinkItemBridge : public LLItemBridge
{
public:
	LLLinkItemBridge(LLInventoryPanel* inventory, 
					 LLFolderView* root,
					 const LLUUID& uuid) :
		LLItemBridge(inventory, root, uuid) {}
	virtual const std::string& getPrefix() { return sPrefix; }
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
protected:
	static std::string sPrefix;
};

class LLLinkFolderBridge : public LLItemBridge
{
public:
	LLLinkFolderBridge(LLInventoryPanel* inventory, 
					   LLFolderView* root,
					   const LLUUID& uuid) :
		LLItemBridge(inventory, root, uuid) {}
	virtual const std::string& getPrefix() { return sPrefix; }
	LLUIImagePtr getIcon() const override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	void performAction(LLInventoryModel* model, std::string action) override;
	void gotoItem() override;
protected:
	const LLUUID &getFolderID() const;
	static std::string sPrefix;
};


class LLMeshBridge : public LLItemBridge
{
	friend class LLInvFVBridge;
public:
	LLUIImagePtr getIcon() const override;
	void openItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;

protected:
	LLMeshBridge(LLInventoryPanel* inventory, 
		     LLFolderView* root,
		     const LLUUID& uuid) :
                       LLItemBridge(inventory, root, uuid) {}
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInvFVBridgeAction
//
// This is an implementation class to be able to 
// perform action to view inventory items.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInvFVBridgeAction
{
public:
	// This method is a convenience function which creates the correct
	// type of bridge action based on some basic information.
	static LLInvFVBridgeAction* createAction(LLAssetType::EType asset_type,
											 const LLUUID& uuid,
											 LLInventoryModel* model);
	static void doAction(LLAssetType::EType asset_type,
						 const LLUUID& uuid, LLInventoryModel* model);
	static void doAction(const LLUUID& uuid, LLInventoryModel* model);

	virtual void doIt() {};
	virtual ~LLInvFVBridgeAction() {} // need this because of warning on OSX
protected:
	LLInvFVBridgeAction(const LLUUID& id, LLInventoryModel* model) :
		mUUID(id), mModel(model) {}
	LLViewerInventoryItem* getItem() const;
protected:
	const LLUUID& mUUID; // item id
	LLInventoryModel* mModel;
};

class LLMeshBridgeAction: public LLInvFVBridgeAction
{
	friend class LLInvFVBridgeAction;
public:
	void	doIt() override;
	virtual ~LLMeshBridgeAction(){}
protected:
	LLMeshBridgeAction(const LLUUID& id,LLInventoryModel* model):LLInvFVBridgeAction(id,model){}

};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Recent Inventory Panel related classes
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Overridden version of the Inventory-Folder-View-Bridge for Folders
class LLRecentItemsFolderBridge : public LLFolderBridge
{
	friend class LLInvFVBridgeAction;
public:
	// Creates context menu for Folders related to Recent Inventory Panel.
	// Uses base logic and than removes from visible items "New..." menu items.
	LLRecentItemsFolderBridge(LLInventoryType::EType type,
							  LLInventoryPanel* inventory,
							  LLFolderView* root,
							  const LLUUID& uuid) :
		LLFolderBridge(inventory, root, uuid)
	{
		mInvType = type;
	}
	/*virtual*/ void buildContextMenu(LLMenuGL& menu, U32 flags) override;
};

// Bridge builder to create Inventory-Folder-View-Bridge for Recent Inventory Panel
class LLRecentInventoryBridgeBuilder : public LLInventoryFolderViewModelBuilder
{
public:
	LLRecentInventoryBridgeBuilder() {}
	// Overrides FolderBridge for Recent Inventory Panel.
	// It use base functionality for bridges other than FolderBridge.
	LLInvFVBridge* createBridge(LLAssetType::EType asset_type,
		LLAssetType::EType actual_asset_type,
		LLInventoryType::EType inv_type,
		LLInventoryPanel* inventory,
		LLFolderViewModelInventory* view_model,
		LLFolderView* root,
		const LLUUID& uuid,
		U32 flags = 0x00) const override;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Marketplace Inventory Panel related classes
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMarketplaceFolderBridge : public LLFolderBridge
{
public:
    // Overloads some display related methods specific to folders in a marketplace floater context
	LLMarketplaceFolderBridge(LLInventoryPanel* inventory,
							  LLFolderView* root,
                              const LLUUID& uuid);

	LLUIImagePtr getIcon() const override;
	LLUIImagePtr getIconOpen() const override;
	std::string getLabelSuffix() const override;
	LLFontGL::StyleFlags getLabelStyle() const override;
    
private:
    LLUIImagePtr getMarketplaceFolderIcon(BOOL is_open) const;
    // Those members are mutable because they are cached variablse to speed up display, not a state variables
    mutable S32 m_depth;
    mutable S32 m_stockCountCache;
};

class LLWornInventoryFolderBridge : public LLFolderBridge
{
	friend class LLInvFVBridgeAction;
public:
	LLWornInventoryFolderBridge(LLInventoryType::EType type,
								LLInventoryPanel* inventory,
								LLFolderView* root,
								const LLUUID& uuid) :
	LLFolderBridge(inventory, root, uuid)
	{
		mInvType = type;
	}
	/*virtual*/ void buildContextMenu(LLMenuGL& menu, U32 flags) override;
};

class LLWornInventoryBridgeBuilder : public LLInventoryFolderViewModelBuilder
{
public:
	LLWornInventoryBridgeBuilder() {}
	LLInvFVBridge* createBridge(LLAssetType::EType asset_type,
										LLAssetType::EType actual_asset_type,
										LLInventoryType::EType inv_type,
										LLInventoryPanel* inventory,
										LLFolderViewModelInventory* view_model,
										LLFolderView* root,
										const LLUUID& uuid,
										U32 flags = 0x00) const override;
};

void rez_attachment(LLViewerInventoryItem* item, 
					LLViewerJointAttachment* attachment,
					bool replace = false);

// Move items from an in-world object's "Contents" folder to a specified
// folder in agent inventory.
BOOL move_inv_category_world_to_agent(const LLUUID& object_id, 
									  const LLUUID& category_id,
									  BOOL drop,
									  void (*callback)(S32, void*) = nullptr,
									  void* user_data = nullptr,
									  LLInventoryFilter* filter = nullptr);

// Utility function to hide all entries except those in the list
// Can be called multiple times on the same menu (e.g. if multiple items
// are selected).  If "append" is false, then only common enabled items
// are set as enabled.
void hide_context_entries(LLMenuGL& menu, 
						  const menuentry_vec_t &entries_to_show, 
						  const menuentry_vec_t &disabled_entries);

// Helper functions to classify actions.
bool isAddAction(const std::string& action);
bool isRemoveAction(const std::string& action);
bool isMarketplaceCopyAction(const std::string& action);
bool isMarketplaceSendAction(const std::string& action);

class LLFolderViewGroupedItemBridge: public LLFolderViewGroupedItemModel
{
public:
    LLFolderViewGroupedItemBridge();
	void groupFilterContextMenu(folder_view_item_deque& selected_items, LLMenuGL& menu) override;
};

#endif // LL_LLINVENTORYBRIDGE_H
