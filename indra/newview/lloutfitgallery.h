/** 
 * @file lloutfitgallery.h
 * @author Pavlo Kryvych
 * @brief Visual gallery of agent's outfits for My Appearance side panel
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2015, Linden Research, Inc.
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

#ifndef LL_LLOUTFITGALLERYCTRL_H
#define LL_LLOUTFITGALLERYCTRL_H

#include "llextendedstatus.h"
#include "lliconctrl.h"
#include "lllayoutstack.h"
#include "lloutfitslist.h"
#include "llpanelappearancetab.h"
#include "lltexturectrl.h"
#include "llviewertexture.h"

#include <vector>

class LLVFS;
class LLOutfitGallery;
class LLOutfitGalleryItem;
class LLOutfitListGearMenuBase;
class LLOutfitGalleryGearMenu;
class LLOutfitGalleryContextMenu;

class LLUpdateGalleryOnPhotoLinked : public LLInventoryCallback
{
public:
    LLUpdateGalleryOnPhotoLinked(){}
    virtual ~LLUpdateGalleryOnPhotoLinked(){}
    /* virtual */ void fire(const LLUUID& inv_item_id) override;
private:
};

class LLOutfitGallery : public LLOutfitListBase
{
public:
    friend class LLOutfitGalleryGearMenu;
    friend class LLOutfitGalleryContextMenu;
    friend class LLUpdateGalleryOnPhotoLinked;

    struct Params
        : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Optional<S32>   row_panel_height;
        Optional<S32>   row_panel_width_factor;
        Optional<S32>   gallery_width_factor;
        Optional<S32>   vertical_gap;
        Optional<S32>   horizontal_gap;
        Optional<S32>   item_width;
        Optional<S32>   item_height;
        Optional<S32>   item_horizontal_gap;
        Optional<S32>   items_in_row;

        Params();
    };

    static const LLOutfitGallery::Params& getDefaultParams();

    LLOutfitGallery(const LLOutfitGallery::Params& params = getDefaultParams());
    virtual ~LLOutfitGallery();

    /*virtual*/ BOOL postBuild() override;
    /*virtual*/ void onOpen(const LLSD& info) override;
    /*virtual*/ void draw() override;	
    
    void onSelectPhoto(LLUUID selected_outfit_id);
    void onTakeSnapshot(LLUUID selected_outfit_id);

    void wearSelectedOutfit();


    /*virtual*/ void setFilterSubString(const std::string& string) override;

    /*virtual*/ void getCurrentCategories(uuid_vec_t& vcur) override;
    /*virtual*/ void updateAddedCategory(LLUUID cat_id) override;
    /*virtual*/ void updateRemovedCategory(LLUUID cat_id) override;
    /*virtual*/ void updateChangedCategoryName(LLViewerInventoryCategory *cat, std::string name) override;

    /*virtual*/ bool hasItemSelected() override;
    /*virtual*/ bool canWearSelected() override;

    /*virtual*/ bool getHasExpandableFolders() override { return FALSE; }

    void updateMessageVisibility();
    bool hasDefaultImage(const LLUUID& outfit_cat_id);

    void refreshTextures(const LLUUID& category_id);
    void refreshOutfit(const LLUUID& category_id);

    void onTexturePickerCommit(LLTextureCtrl::ETexturePickOp op, LLUUID id);
    void onTexturePickerUpdateImageStats(LLPointer<LLViewerTexture> texture);
    void onBeforeOutfitSnapshotSave();
    void onAfterOutfitSnapshotSave();

    /*virtual*/ BOOL handleDoubleClick(S32 x, S32 y, MASK mask) override;

protected:
    /*virtual*/ void onHighlightBaseOutfit(LLUUID base_id, LLUUID prev_id) override;
    /*virtual*/ void onSetSelectedOutfitByUUID(const LLUUID& outfit_uuid) override;
    /*virtual*/ void onOutfitRightClick(LLUICtrl* ctrl, S32 x, S32 y, const LLUUID& cat_id) override;
    /*virtual*/ void onChangeOutfitSelection(LLWearableItemsList* list, const LLUUID& category_id) override;

    /*virtual*/ void onCollapseAllFolders() override {}
    /*virtual*/ void onExpandAllFolders() override {}
    /*virtual*/ LLOutfitListGearMenuBase* createGearMenu() override;

    void applyFilter(LLOutfitGalleryItem* item, const std::string& filter_substring);

private:
    void loadPhotos();
    void uploadPhoto(LLUUID outfit_id);
    LLUUID getPhotoAssetId(const LLUUID& outfit_id);
    LLUUID getDefaultPhoto();
    void linkPhotoToOutfit(LLUUID outfit_id, LLUUID photo_id);
    bool checkRemovePhoto(LLUUID outfit_id);
    void addToGallery(LLOutfitGalleryItem* item);
    void removeFromGalleryLast(LLOutfitGalleryItem* item);
    void removeFromGalleryMiddle(LLOutfitGalleryItem* item);
    LLPanel* addLastRow();
    void removeLastRow();
    void moveRowUp(int row);
    void moveRowDown(int row);
    void moveRow(int row, int pos);
    LLPanel* addToRow(LLPanel* row_stack, LLOutfitGalleryItem* item, int pos, int hgap);
    void removeFromLastRow(LLOutfitGalleryItem* item);
    void reArrangeRows(S32 row_diff = 0);
    void updateRowsIfNeeded();
    void updateGalleryWidth();

    LLOutfitGalleryItem* buildGalleryItem(std::string name);

    void onTextureSelectionChanged(LLInventoryItem* itemp);

    void buildGalleryPanel(int row_count);
    void reshapeGalleryPanel(int row_count);
    LLPanel* buildItemPanel(int left);
    LLPanel* buildRowPanel(int left, int bottom);
    void moveRowPanel(LLPanel* stack, int left, int bottom);
    std::vector<LLPanel*> mRowPanels;
    std::vector<LLPanel*> mItemPanels;
    std::vector<LLPanel*> mUnusedRowPanels;
    std::vector<LLPanel*> mUnusedItemPanels;
    std::vector<LLOutfitGalleryItem*> mItems;
    std::vector<LLOutfitGalleryItem*> mHiddenItems;
    LLScrollContainer* mScrollPanel;
    LLPanel* mGalleryPanel;
    LLPanel* mLastRowPanel;
    LLUUID mOutfitLinkPending;
    LLUUID mOutfitRenamePending;
    LLTextBox* mMessageTextBox;
    bool mGalleryCreated;
    int mRowCount;
    int mItemsAddedCount;
    LLPointer<LLViewerTexture> mTextureSelected;
    /* Params */
    int mRowPanelHeight;
    int mVerticalGap;
    int mHorizontalGap;
    int mItemWidth;
    int mItemHeight;
    int mItemHorizontalGap;
    int mItemsInRow;
    int mRowPanelWidth;
    int mGalleryWidth;
    int mRowPanWidthFactor;
    int mGalleryWidthFactor;
    
    LLListContextMenu* mOutfitGalleryMenu;

    LLHandle<LLFloater> mFloaterHandle;

    typedef std::map<LLUUID, LLOutfitGalleryItem*>      outfit_map_t;
    typedef outfit_map_t::value_type                    outfit_map_value_t;
    outfit_map_t                                        mOutfitMap;
    typedef std::map<LLOutfitGalleryItem*, int>         item_num_map_t;
    typedef item_num_map_t::value_type                  item_numb_map_value_t;
    item_num_map_t                                      mItemIndexMap;


    LLInventoryCategoriesObserver* 	mTexturesObserver;
    LLInventoryCategoriesObserver* 	mOutfitsObserver;
};
class LLOutfitGalleryContextMenu : public LLOutfitContextMenu
{
public:
    
    friend class LLOutfitGallery;
    LLOutfitGalleryContextMenu(LLOutfitListBase* outfit_list)
    : LLOutfitContextMenu(outfit_list),
    mOutfitList(outfit_list){}
protected:
    /* virtual */ LLContextMenu* createMenu() override;
    bool onEnable(LLSD::String param);
    bool onVisible(LLSD::String param);
    void onUploadPhoto(const LLUUID& outfit_cat_id);
    void onSelectPhoto(const LLUUID& outfit_cat_id);
    void onRemovePhoto(const LLUUID& outfit_cat_id);
    void onTakeSnapshot(const LLUUID& outfit_cat_id);
    void onCreate(const LLSD& data);
    void onRemoveOutfit(const LLUUID& outfit_cat_id);
    void onOutfitsRemovalConfirmation(const LLSD& notification, const LLSD& response, const LLUUID& outfit_cat_id);
private:
    LLOutfitListBase*	mOutfitList;
};


class LLOutfitGalleryGearMenu : public LLOutfitListGearMenuBase
{
public:
    friend class LLOutfitGallery;
    LLOutfitGalleryGearMenu(LLOutfitListBase* olist);

protected:
    /*virtual*/ void onUpdateItemsVisibility() override;
private:
    /*virtual*/ void onUploadFoto() override;
    /*virtual*/ void onSelectPhoto() override;
    /*virtual*/ void onTakeSnapshot() override;
    /*virtual*/ void onRemovePhoto() override;
    /*virtual*/ void onChangeSortOrder() override;

    bool hasDefaultImage();
};

class LLOutfitGalleryItem : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {};

    LLOutfitGalleryItem(const Params& p);
    virtual ~LLOutfitGalleryItem();

    /*virtual*/ BOOL postBuild() override;
    /*virtual*/ void draw() override;
    /*virtual*/ BOOL handleMouseDown(S32 x, S32 y, MASK mask) override;
    /*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask) override;

    void setDefaultImage();
    void setImageAssetId(LLUUID asset_id);
    LLUUID getImageAssetId();
    void setOutfitName(std::string name);
    void setOutfitWorn(bool value);
    void setSelected(bool value);
    
    std::string getItemName() {return mOutfitName;}
    bool isDefaultImage() {return mDefaultImage;}
    
    bool isHidden() {return mHidden;}
    void setHidden(bool hidden) {mHidden = hidden;}
    
private:
    LLPointer<LLViewerFetchedTexture> mTexturep;
    LLUUID mImageAssetId;
    LLTextBox* mOutfitNameText;
    LLTextBox* mOutfitWornText;
    LLPanel* mTextBgPanel;
	LLIconCtrl* mIconPreviewOutfit;
    bool     mSelected;
    bool     mWorn;
    bool     mDefaultImage;
    bool	 mHidden;
    std::string mOutfitName;
};

#endif  // LL_LLOUTFITGALLERYCTRL_H
