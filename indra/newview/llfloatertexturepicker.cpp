/**
* @file llfloatertexturepicker.cpp
* @author Richard Nelson, James Cook
* @brief LLTextureCtrl class implementation including related functions
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

#include "llagent.h"
#include "lldraghandle.h"
#include "llfiltereditor.h"
#include "llfloaterreg.h"
#include "llfloatertexturepicker.h"
#include "llinventoryfunctions.h"
#include "llinventorypanel.h"
#include "llscrolllistctrl.h"
#include "lltoolmgr.h"
#include "lltoolpipette.h"
#include "lltrans.h"
#include "llviewercontrol.h"

static const F32 CONTEXT_CONE_IN_ALPHA = 0.0f;
static const F32 CONTEXT_CONE_OUT_ALPHA = 1.f;
static const F32 CONTEXT_FADE_TIME = 0.08f;

static const S32 LOCAL_TRACKING_ID_COLUMN = 1;

LLFloaterTexturePicker::LLFloaterTexturePicker(
    LLView* owner,
    LLUUID image_asset_id,
    LLUUID default_image_asset_id,
    LLUUID transparent_image_asset_id,
    LLUUID blank_image_asset_id,
    BOOL tentative,
    BOOL allow_no_texture,
    const std::string& label,
    PermissionMask immediate_filter_perm_mask,
    PermissionMask dnd_filter_perm_mask,
    PermissionMask non_immediate_filter_perm_mask,
    BOOL can_apply_immediately,
    LLUIImagePtr fallback_image)
    : LLFloater(LLSD()),
    mOwner(owner),
    mImageAssetID(image_asset_id),
    mFallbackImage(fallback_image),
    mDefaultImageAssetID(default_image_asset_id),
    mTransparentImageAssetID(transparent_image_asset_id),
    mBlankImageAssetID(blank_image_asset_id),
    mTentative(tentative),
    mAllowNoTexture(allow_no_texture),
    mOriginalImageAssetID(image_asset_id),
    mLabel(label),
    mTentativeLabel(nullptr),
    mResolutionLabel(nullptr),
    mActive(TRUE),
    mFilterEdit(nullptr),
    mImmediateFilterPermMask(immediate_filter_perm_mask),
    mDnDFilterPermMask(dnd_filter_perm_mask),
    mNonImmediateFilterPermMask(non_immediate_filter_perm_mask),
    mContextConeOpacity(0.f),
    mSelectedItemPinned(FALSE),
    mCanApply(true),
    mCanPreview(true),
    mPreviewSettingChanged(false),
    mOnFloaterCloseCallback(nullptr),
    mOnFloaterCommitCallback(nullptr),
    mSetImageAssetIDCallback(nullptr),
    mOnUpdateImageStatsCallback(nullptr)
{
    buildFromFile("floater_texture_ctrl.xml");
    mCanApplyImmediately = can_apply_immediately;
    setCanMinimize(FALSE);
}

LLFloaterTexturePicker::~LLFloaterTexturePicker()
{
}

void LLFloaterTexturePicker::setImageID(const LLUUID& image_id, bool set_selection /*=true*/)
{
    if (mImageAssetID != image_id && mActive)
    {
        mNoCopyTextureSelected = FALSE;
        mViewModel->setDirty(); // *TODO: shouldn't we be using setValue() here?
        mImageAssetID = image_id;
        LLUUID item_id = findItemID(mImageAssetID, FALSE);
        if (item_id.isNull())
        {
            mInventoryPanel->getRootFolder()->clearSelection();
        }
        else
        {
            LLInventoryItem* itemp = gInventory.getItem(image_id);
            if (itemp && !itemp->getPermissions().allowCopyBy(gAgent.getID()))
            {
                // no copy texture
                getChild<LLUICtrl>("apply_immediate_check")->setValue(FALSE);
                mNoCopyTextureSelected = TRUE;
            }
        }

        if (set_selection)
        {
            mInventoryPanel->setSelection(item_id, TAKE_FOCUS_NO);
        }
    }
}

void LLFloaterTexturePicker::setActive(BOOL active)
{
    if (!active && getChild<LLUICtrl>("Pipette")->getValue().asBoolean())
    {
        stopUsingPipette();
    }
    mActive = active;
}

void LLFloaterTexturePicker::setCanApplyImmediately(BOOL b)
{
    mCanApplyImmediately = b;
    if (!mCanApplyImmediately)
    {
        getChild<LLUICtrl>("apply_immediate_check")->setValue(FALSE);
    }
    updateFilterPermMask();
}

void LLFloaterTexturePicker::stopUsingPipette()
{
    if (LLToolMgr::getInstance()->getCurrentTool() == LLToolPipette::getInstance())
    {
        LLToolMgr::getInstance()->clearTransientTool();
    }
}

void LLFloaterTexturePicker::updateImageStats()
{
    if (mTexturep.notNull())
    {
        //RN: have we received header data for this image?
        if (mTexturep->getFullWidth() > 0 && mTexturep->getFullHeight() > 0)
        {
            std::string formatted_dims = llformat("%d x %d", mTexturep->getFullWidth(), mTexturep->getFullHeight());
            mResolutionLabel->setTextArg("[DIMENSIONS]", formatted_dims);
            if (mOnUpdateImageStatsCallback != nullptr)
            {
                mOnUpdateImageStatsCallback(mTexturep);
            }
        }
        else
        {
            mResolutionLabel->setTextArg("[DIMENSIONS]", std::string("[? x ?]"));
        }
    }
    else
    {
        mResolutionLabel->setTextArg("[DIMENSIONS]", std::string(""));
    }
}

// virtual
BOOL LLFloaterTexturePicker::handleDragAndDrop(
    S32 x, S32 y, MASK mask,
    BOOL drop,
    EDragAndDropType cargo_type, void *cargo_data,
    EAcceptance *accept,
    std::string& tooltip_msg)
{
    BOOL handled = FALSE;

    bool is_mesh = cargo_type == DAD_MESH;

    if ((cargo_type == DAD_TEXTURE) || is_mesh)
    {
        LLInventoryItem *item = (LLInventoryItem *)cargo_data;

        BOOL copy = item->getPermissions().allowCopyBy(gAgent.getID());
        BOOL mod = item->getPermissions().allowModifyBy(gAgent.getID());
        BOOL xfer = item->getPermissions().allowOperationBy(PERM_TRANSFER,
            gAgent.getID());

        PermissionMask item_perm_mask = 0;
        if (copy) item_perm_mask |= PERM_COPY;
        if (mod)  item_perm_mask |= PERM_MODIFY;
        if (xfer) item_perm_mask |= PERM_TRANSFER;

        //PermissionMask filter_perm_mask = getFilterPermMask();  Commented out due to no-copy texture loss.
        PermissionMask filter_perm_mask = mDnDFilterPermMask;
        if ((item_perm_mask & filter_perm_mask) == filter_perm_mask)
        {
            if (drop)
            {
                setImageID(item->getAssetUUID());
                commitIfImmediateSet();
            }

            *accept = ACCEPT_YES_SINGLE;
        }
        else
        {
            *accept = ACCEPT_NO;
        }
    }
    else
    {
        *accept = ACCEPT_NO;
    }

    handled = TRUE;
    LL_DEBUGS("UserInput") << "dragAndDrop handled by LLFloaterTexturePicker " << getName() << LL_ENDL;

    return handled;
}

BOOL LLFloaterTexturePicker::handleKeyHere(KEY key, MASK mask)
{
    LLFolderView* root_folder = mInventoryPanel->getRootFolder();

    if (root_folder && mFilterEdit)
    {
        if (mFilterEdit->hasFocus()
            && (key == KEY_RETURN || key == KEY_DOWN)
            && mask == MASK_NONE)
        {
            if (!root_folder->getCurSelectedItem())
            {
                LLFolderViewItem* itemp = mInventoryPanel->getItemByID(gInventory.getRootFolderID());
                if (itemp)
                {
                    root_folder->setSelection(itemp, FALSE, FALSE);
                }
            }
            root_folder->scrollToShowSelection();

            // move focus to inventory proper
            mInventoryPanel->setFocus(TRUE);

            // treat this as a user selection of the first filtered result
            commitIfImmediateSet();

            return TRUE;
        }

        if (mInventoryPanel->hasFocus() && key == KEY_UP)
        {
            mFilterEdit->focusFirstItem(TRUE);
        }
    }

    return LLFloater::handleKeyHere(key, mask);
}

void LLFloaterTexturePicker::onClose(bool app_quitting)
{
    if (mOwner && mOnFloaterCloseCallback != nullptr)
    {
        mOnFloaterCloseCallback();
    }
    stopUsingPipette();
}

// virtual
BOOL LLFloaterTexturePicker::postBuild()
{
    LLFloater::postBuild();

    if (!mLabel.empty())
    {
        std::string pick = getString("pick title");

        setTitle(pick + mLabel);
    }
    mTentativeLabel = getChild<LLTextBox>("Multiple");

    mResolutionLabel = getChild<LLTextBox>("unknown");


    childSetAction("Default", LLFloaterTexturePicker::onBtnSetToDefault, this);
    childSetAction("None", LLFloaterTexturePicker::onBtnNone, this);
    childSetAction("Blank", LLFloaterTexturePicker::onBtnBlank, this);
    childSetAction("Transparent", LLFloaterTexturePicker::onBtnTransparent, this); // <alchemy/>


    childSetCommitCallback("show_folders_check", onShowFolders, this);
    getChildView("show_folders_check")->setVisible(FALSE);

    mFilterEdit = getChild<LLFilterEditor>("inventory search editor");
    mFilterEdit->setCommitCallback(boost::bind(&LLFloaterTexturePicker::onFilterEdit, this, _2));

    mInventoryPanel = getChild<LLInventoryPanel>("inventory panel");

    if (mInventoryPanel)
    {
        U32 filter_types = 0x0;
        filter_types |= 0x1 << LLInventoryType::IT_TEXTURE;
        filter_types |= 0x1 << LLInventoryType::IT_SNAPSHOT;

        mInventoryPanel->setFilterTypes(filter_types);
        //mInventoryPanel->setFilterPermMask(getFilterPermMask());  //Commented out due to no-copy texture loss.
        mInventoryPanel->setFilterPermMask(mImmediateFilterPermMask);
        mInventoryPanel->setSelectCallback(boost::bind(&LLFloaterTexturePicker::onSelectionChange, this, _1, _2));
        mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);

        // Disable auto selecting first filtered item because it takes away
        // selection from the item set by LLTextureCtrl owning this floater.
        mInventoryPanel->getRootFolder()->setAutoSelectOverride(TRUE);

        // Commented out to scroll to currently selected texture. See EXT-5403.
        // // store this filter as the default one
        // mInventoryPanel->getRootFolder()->getFilter().markDefault();

        // Commented out to stop opening all folders with textures
        // mInventoryPanel->openDefaultFolderForType(LLFolderType::FT_TEXTURE);

        // don't put keyboard focus on selected item, because the selection callback
        // will assume that this was user input
        if (!mImageAssetID.isNull())
        {
            mInventoryPanel->setSelection(findItemID(mImageAssetID, FALSE), TAKE_FOCUS_NO);
        }
    }

    mModeSelector = getChild<LLRadioGroup>("mode_selection");
    mModeSelector->setCommitCallback(boost::bind(&LLFloaterTexturePicker::onModeSelect, this));
    mModeSelector->setSelectedIndex(0, 0);

    childSetAction("l_add_btn", LLFloaterTexturePicker::onBtnAdd, this);
    childSetAction("l_rem_btn", LLFloaterTexturePicker::onBtnRemove, this);
    childSetAction("l_upl_btn", LLFloaterTexturePicker::onBtnUpload, this);

    mLocalScrollCtrl = getChild<LLScrollListCtrl>("l_name_list");
    mLocalScrollCtrl->setCommitCallback(boost::bind(&LLFloaterTexturePicker::onLocalScrollCommit, this));
    LLLocalBitmapMgr::feedScrollList(mLocalScrollCtrl);

    getChild<LLLineEditor>("uuid_editor")->setCommitCallback(boost::bind(&onApplyUUID, this));
    getChild<LLButton>("apply_uuid_btn")->setClickedCallback(boost::bind(&onApplyUUID, this));

    mNoCopyTextureSelected = FALSE;

    getChild<LLUICtrl>("apply_immediate_check")->setValue(gSavedSettings.getBOOL("TextureLivePreview"));
    childSetCommitCallback("apply_immediate_check", onApplyImmediateCheck, this);

    if (!mCanApplyImmediately)
    {
        getChildView("show_folders_check")->setEnabled(FALSE);
    }

    getChild<LLUICtrl>("Pipette")->setCommitCallback(boost::bind(&LLFloaterTexturePicker::onBtnPipette, this));
    childSetAction("Cancel", LLFloaterTexturePicker::onBtnCancel, this);
    childSetAction("Select", LLFloaterTexturePicker::onBtnSelect, this);

    // update permission filter once UI is fully initialized
    updateFilterPermMask();
    mSavedFolderState.setApply(FALSE);

    LLToolPipette::getInstance()->setToolSelectCallback(boost::bind(&LLFloaterTexturePicker::onTextureSelect, this, _1));

    return TRUE;
}

// virtual
void LLFloaterTexturePicker::draw()
{
    if (mOwner)
    {
        // draw cone of context pointing back to texture swatch	
        LLRect owner_rect;
        mOwner->localRectToOtherView(mOwner->getLocalRect(), &owner_rect, this);
        LLRect local_rect = getLocalRect();
        if (gFocusMgr.childHasKeyboardFocus(this) && mOwner->isInVisibleChain() && mContextConeOpacity > 0.001f)
        {
            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
            LLGLEnable(GL_CULL_FACE);
            gGL.begin(LLRender::TRIANGLE_STRIP);
            {
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
                gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
                gGL.vertex2i(owner_rect.mLeft, owner_rect.mTop);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
                gGL.vertex2i(local_rect.mRight, local_rect.mTop);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
                gGL.vertex2i(owner_rect.mRight, owner_rect.mTop);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
                gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
                gGL.vertex2i(owner_rect.mRight, owner_rect.mBottom);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
                gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
                gGL.vertex2i(owner_rect.mLeft, owner_rect.mBottom);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
                gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
                gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
                gGL.vertex2i(owner_rect.mLeft, owner_rect.mTop);
            }
            gGL.end();
        }
    }

    if (gFocusMgr.childHasMouseCapture(getDragHandle()))
    {
        mContextConeOpacity = lerp(mContextConeOpacity, gSavedSettings.getF32("PickerContextOpacity"), LLSmoothInterpolation::getInterpolant(CONTEXT_FADE_TIME));
    }
    else
    {
        mContextConeOpacity = lerp(mContextConeOpacity, 0.f, LLSmoothInterpolation::getInterpolant(CONTEXT_FADE_TIME));
    }

    updateImageStats();

    // if we're inactive, gray out "apply immediate" checkbox
    getChildView("show_folders_check")->setEnabled(mActive && mCanApplyImmediately && !mNoCopyTextureSelected);
    getChildView("Select")->setEnabled(mActive && mCanApply);
    getChildView("Pipette")->setEnabled(mActive);
    getChild<LLUICtrl>("Pipette")->setValue(LLToolMgr::getInstance()->getCurrentTool() == LLToolPipette::getInstance());

    //BOOL allow_copy = FALSE;
    if (mOwner)
    {
        mTexturep = nullptr;
        if (mImageAssetID.notNull())
        {
            mTexturep = LLViewerTextureManager::getFetchedTexture(mImageAssetID);
            mTexturep->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
        }

        if (mTentativeLabel)
        {
            mTentativeLabel->setVisible(FALSE);
        }

        getChildView("Default")->setEnabled(mImageAssetID != mDefaultImageAssetID || mTentative);
        getChildView("Transparent")->setEnabled(mImageAssetID != mTransparentImageAssetID || mTentative); // <alchemy/>
        getChildView("Blank")->setEnabled(mImageAssetID != mBlankImageAssetID || mTentative);
        getChildView("None")->setEnabled(mAllowNoTexture && (!mImageAssetID.isNull() || mTentative));

        LLFloater::draw();

        if (isMinimized())
        {
            return;
        }

        // Border
        LLRect border = getChildView("preview_widget")->getRect();
        gl_rect_2d(border, LLColor4::black, FALSE);


        // Interior
        LLRect interior = border;
        interior.stretch(-1);

        // If the floater is focused, don't apply its alpha to the texture (STORM-677).
        const F32 alpha = getTransparencyType() == TT_ACTIVE ? 1.0f : getCurrentTransparency();
        if (mTexturep)
        {
            if (mTexturep->getComponents() == 4)
            {
                gl_rect_2d_checkerboard(interior, alpha);
            }

            gl_draw_scaled_image(interior.mLeft, interior.mBottom, interior.getWidth(), interior.getHeight(), mTexturep, UI_VERTEX_COLOR % alpha);

            // Pump the priority
            mTexturep->addTextureStats((F32)(interior.getWidth() * interior.getHeight()));
        }
        else if (!mFallbackImage.isNull())
        {
            mFallbackImage->draw(interior, UI_VERTEX_COLOR % alpha);
        }
        else
        {
            gl_rect_2d(interior, LLColor4::grey % alpha, TRUE);

            // Draw X
            gl_draw_x(interior, LLColor4::black);
        }

        // Draw Tentative Label over the image
        if (mTentative && !mViewModel->isDirty())
        {
            mTentativeLabel->setVisible(TRUE);
            drawChild(mTentativeLabel);
        }

        if (mSelectedItemPinned) return;

        LLFolderView* folder_view = mInventoryPanel->getRootFolder();
        if (!folder_view) return;

        LLFolderViewFilter& filter = static_cast<LLFolderViewModelInventory*>(folder_view->getFolderViewModel())->getFilter();

        bool is_filter_active = folder_view->getViewModelItem()->getLastFilterGeneration() < filter.getCurrentGeneration() &&
            filter.isNotDefault();

        // After inventory panel filter is applied we have to update
        // constraint rect for the selected item because of folder view
        // AutoSelectOverride set to TRUE. We force PinningSelectedItem
        // flag to FALSE state and setting filter "dirty" to update
        // scroll container to show selected item (see LLFolderView::doIdle()).
        if (!is_filter_active && !mSelectedItemPinned)
        {
            folder_view->setPinningSelectedItem(mSelectedItemPinned);
            folder_view->getViewModelItem()->dirtyFilter();
            mSelectedItemPinned = TRUE;
        }
    }
}

const LLUUID& LLFloaterTexturePicker::findItemID(const LLUUID& asset_id, BOOL copyable_only, BOOL ignore_library)
{
    LLViewerInventoryCategory::cat_array_t cats;
    LLViewerInventoryItem::item_array_t items;
    LLAssetIDMatches asset_id_matches(asset_id);
    gInventory.collectDescendentsIf(LLUUID::null,
        cats,
        items,
        LLInventoryModel::INCLUDE_TRASH,
        asset_id_matches);

    if (items.size())
    {
        // search for copyable version first
        for (S32 i = 0; i < items.size(); i++)
        {
            LLInventoryItem* itemp = items[i];
            LLPermissions item_permissions = itemp->getPermissions();
            if (item_permissions.allowCopyBy(gAgent.getID(), gAgent.getGroupID()))
            {
                if (!ignore_library || !gInventory.isObjectDescendentOf(itemp->getUUID(), gInventory.getLibraryRootFolderID()))
                {
                    return itemp->getUUID();
                }
            }
        }
        // otherwise just return first instance, unless copyable requested
        if (copyable_only)
        {
            return LLUUID::null;
        }
        else
        {
            if (!ignore_library || !gInventory.isObjectDescendentOf(items[0]->getUUID(), gInventory.getLibraryRootFolderID()))
            {
                return items[0]->getUUID();
            }
        }
    }

    return LLUUID::null;
}

PermissionMask LLFloaterTexturePicker::getFilterPermMask()
{
    bool apply_immediate = getChild<LLUICtrl>("apply_immediate_check")->getValue().asBoolean();
    return apply_immediate ? mImmediateFilterPermMask : mNonImmediateFilterPermMask;
}

void LLFloaterTexturePicker::commitIfImmediateSet()
{
    if (!mNoCopyTextureSelected && mOnFloaterCommitCallback != nullptr && mCanApply)
    {
        mOnFloaterCommitCallback(LLTextureCtrl::TEXTURE_CHANGE, LLUUID::null);
    }
}

void LLFloaterTexturePicker::commitCancel()
{
    if (!mNoCopyTextureSelected && mOnFloaterCommitCallback != nullptr && mCanApply)
    {
        mOnFloaterCommitCallback(LLTextureCtrl::TEXTURE_CANCEL, LLUUID::null);
    }
}

// static
void LLFloaterTexturePicker::onBtnSetToDefault(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    self->setCanApply(true, true);
    if (self->mOwner)
    {
        self->setImageID(self->getDefaultImageAssetID());
    }
    self->commitIfImmediateSet();
}

// static
void LLFloaterTexturePicker::onBtnTransparent(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    self->setCanApply(true, true);
    self->setImageID(self->getTransparentImageAssetID());
    self->commitIfImmediateSet();
}

// static
void LLFloaterTexturePicker::onBtnBlank(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    self->setCanApply(true, true);
    self->setImageID(self->getBlankImageAssetID());
    self->commitIfImmediateSet();
}


// static
void LLFloaterTexturePicker::onBtnNone(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    self->setImageID(LLUUID::null);
    self->commitCancel();
}

// static
void LLFloaterTexturePicker::onBtnCancel(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    self->setImageID(self->mOriginalImageAssetID);
    if (self->mOnFloaterCommitCallback != nullptr)
    {
        self->mOnFloaterCommitCallback(LLTextureCtrl::TEXTURE_CANCEL, LLUUID::null);
    }
    self->mViewModel->resetDirty();
    self->closeFloater();
}

// static
void LLFloaterTexturePicker::onBtnSelect(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    LLUUID local_id = LLUUID::null;
    if (self->mOwner)
    {
        if (self->mLocalScrollCtrl->getVisible() && !self->mLocalScrollCtrl->getAllSelected().empty())
        {
            LLUUID temp_id = self->mLocalScrollCtrl->getFirstSelected()->getColumn(LOCAL_TRACKING_ID_COLUMN)->getValue().asUUID();
            local_id = LLLocalBitmapMgr::getWorldID(temp_id);
        }
    }
    if (self->mOnFloaterCommitCallback != nullptr)
    {
        self->mOnFloaterCommitCallback(LLTextureCtrl::TEXTURE_SELECT, local_id);
    }
    self->closeFloater();
}

void LLFloaterTexturePicker::onBtnPipette()
{
    BOOL pipette_active = getChild<LLUICtrl>("Pipette")->getValue().asBoolean();
    pipette_active = !pipette_active;
    if (pipette_active)
    {
        LLToolMgr::getInstance()->setTransientTool(LLToolPipette::getInstance());
    }
    else
    {
        LLToolMgr::getInstance()->clearTransientTool();
    }
}

// static
void LLFloaterTexturePicker::onApplyUUID(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    LLUUID id(self->getChild<LLLineEditor>("uuid_editor")->getText());
    if (id.notNull())
    {
        self->setImageID(id);
        self->commitIfImmediateSet();
    }
}

void LLFloaterTexturePicker::onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action)
{
    if (items.size())
    {
        LLFolderViewItem* first_item = items.front();
        LLInventoryItem* itemp = gInventory.getItem(static_cast<LLFolderViewModelItemInventory*>(first_item->getViewModelItem())->getUUID());
        mNoCopyTextureSelected = FALSE;
        if (itemp)
        {
            if (mTextureSelectedCallback != nullptr)
            {
                mTextureSelectedCallback(itemp);
            }
            if (!itemp->getPermissions().allowCopyBy(gAgent.getID()))
            {
                mNoCopyTextureSelected = TRUE;
            }
            setImageID(itemp->getAssetUUID(), false);
            mViewModel->setDirty(); // *TODO: shouldn't we be using setValue() here?

            if (!mPreviewSettingChanged)
            {
                mCanPreview = gSavedSettings.getBOOL("TextureLivePreview");
            }
            else
            {
                mPreviewSettingChanged = false;
            }

            if (user_action && mCanPreview)
            {
                // only commit intentional selections, not implicit ones
                commitIfImmediateSet();
            }
        }
    }
}

void LLFloaterTexturePicker::onModeSelect()
{
    bool mode = (mModeSelector->getSelectedIndex() == 0);

    getChild<LLButton>("Default")->setVisible(mode);
    getChild<LLButton>("Transparent")->setVisible(mode); // <alchemy/>
    getChild<LLButton>("Blank")->setVisible(mode);
    getChild<LLButton>("None")->setVisible(mode);
    getChild<LLButton>("Pipette")->setVisible(mode);
    getChild<LLFilterEditor>("inventory search editor")->setVisible(mode);
    getChild<LLInventoryPanel>("inventory panel")->setVisible(mode);
    getChild<LLLineEditor>("uuid_editor")->setVisible(mode);
    getChild<LLButton>("apply_uuid_btn")->setVisible(mode);

    /*getChild<LLCheckBox>("show_folders_check")->setVisible(mode);
    no idea under which conditions the above is even shown, needs testing. */

    getChild<LLButton>("l_add_btn")->setVisible(!mode);
    getChild<LLButton>("l_rem_btn")->setVisible(!mode);
    getChild<LLButton>("l_upl_btn")->setVisible(!mode);
    getChild<LLScrollListCtrl>("l_name_list")->setVisible(!mode);
}

// static
void LLFloaterTexturePicker::onBtnAdd(void* userdata)
{
    if (LLLocalBitmapMgr::addUnit() == true)
    {
        LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
        LLLocalBitmapMgr::feedScrollList(self->mLocalScrollCtrl);
    }
}

// static
void LLFloaterTexturePicker::onBtnRemove(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    std::vector<LLScrollListItem*> selected_items = self->mLocalScrollCtrl->getAllSelected();

    if (!selected_items.empty())
    {
        for (LLScrollListItem* list_item : selected_items)
        {
            if (list_item)
            {
                LLUUID tracking_id = list_item->getColumn(LOCAL_TRACKING_ID_COLUMN)->getValue().asUUID();
                LLLocalBitmapMgr::delUnit(tracking_id);
            }
        }

        self->getChild<LLButton>("l_rem_btn")->setEnabled(false);
        self->getChild<LLButton>("l_upl_btn")->setEnabled(false);
        LLLocalBitmapMgr::feedScrollList(self->mLocalScrollCtrl);
    }

}

// static
void LLFloaterTexturePicker::onBtnUpload(void* userdata)
{
    LLFloaterTexturePicker* self = static_cast<LLFloaterTexturePicker*>(userdata);
    std::vector<LLScrollListItem*> selected_items = self->mLocalScrollCtrl->getAllSelected();

    if (selected_items.empty())
    {
        return;
    }

    /* currently only allows uploading one by one, picks the first item from the selection list.  (not the vector!)
    in the future, it might be a good idea to check the vector size and if more than one units is selected - opt for multi-image upload. */

    LLUUID tracking_id = LLUUID(self->mLocalScrollCtrl->getSelectedItemLabel(LOCAL_TRACKING_ID_COLUMN));
    std::string filename = LLLocalBitmapMgr::getFilename(tracking_id);

    if (!filename.empty())
    {
        LLFloaterReg::showInstance("upload_image", LLSD(filename));
    }

}

void LLFloaterTexturePicker::onLocalScrollCommit()
{
    std::vector<LLScrollListItem*> selected_items = mLocalScrollCtrl->getAllSelected();
    bool has_selection = !selected_items.empty();

    getChild<LLButton>("l_rem_btn")->setEnabled(has_selection);
    getChild<LLButton>("l_upl_btn")->setEnabled(has_selection && (selected_items.size() < 2));
    /* since multiple-localbitmap upload is not implemented, upl button gets disabled if more than one is selected. */

    if (has_selection)
    {
        LLUUID tracking_id = LLUUID(mLocalScrollCtrl->getSelectedItemLabel(LOCAL_TRACKING_ID_COLUMN));
        LLUUID inworld_id = LLLocalBitmapMgr::getWorldID(tracking_id);
        if (mSetImageAssetIDCallback != nullptr)
        {
            mSetImageAssetIDCallback(inworld_id);
        }

        if (childGetValue("apply_immediate_check").asBoolean())
        {
            if (mOnFloaterCommitCallback != nullptr)
            {
                mOnFloaterCommitCallback(LLTextureCtrl::TEXTURE_CHANGE, inworld_id);
            }
        }
    }
}

// static
void LLFloaterTexturePicker::onShowFolders(LLUICtrl* ctrl, void *user_data)
{
    LLCheckBoxCtrl* check_box = static_cast<LLCheckBoxCtrl*>(ctrl);
    LLFloaterTexturePicker* picker = static_cast<LLFloaterTexturePicker*>(user_data);

    if (check_box->get())
    {
        picker->mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
    }
    else
    {
        picker->mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NO_FOLDERS);
    }
}

// static
void LLFloaterTexturePicker::onApplyImmediateCheck(LLUICtrl* ctrl, void *user_data)
{
    LLFloaterTexturePicker* picker = static_cast<LLFloaterTexturePicker*>(user_data);

    LLCheckBoxCtrl* check_box = static_cast<LLCheckBoxCtrl*>(ctrl);
    gSavedSettings.setBOOL("TextureLivePreview", check_box->get());

    picker->updateFilterPermMask();
    picker->commitIfImmediateSet();
}

void LLFloaterTexturePicker::updateFilterPermMask()
{
    //mInventoryPanel->setFilterPermMask( getFilterPermMask() );  Commented out due to no-copy texture loss.
}

void LLFloaterTexturePicker::setCanApply(bool can_preview, bool can_apply)
{
    getChildRef<LLUICtrl>("Select").setEnabled(can_apply);
    getChildRef<LLUICtrl>("preview_disabled").setVisible(!can_preview);
    getChildRef<LLUICtrl>("apply_immediate_check").setVisible(can_preview);

    mCanApply = can_apply;
    mCanPreview = can_preview ? gSavedSettings.getBOOL("TextureLivePreview") : false;
    mPreviewSettingChanged = true;
}

void LLFloaterTexturePicker::onFilterEdit(const std::string& search_string)
{
    std::string upper_case_search_string = search_string;
    LLStringUtil::toUpper(upper_case_search_string);

    if (upper_case_search_string.empty())
    {
        if (mInventoryPanel->getFilterSubString().empty())
        {
            // current filter and new filter empty, do nothing
            return;
        }

        mSavedFolderState.setApply(TRUE);
        mInventoryPanel->getRootFolder()->applyFunctorRecursively(mSavedFolderState);
        // add folder with current item to list of previously opened folders
        LLOpenFoldersWithSelection opener;
        mInventoryPanel->getRootFolder()->applyFunctorRecursively(opener);
        mInventoryPanel->getRootFolder()->scrollToShowSelection();

    }
    else if (mInventoryPanel->getFilterSubString().empty())
    {
        // first letter in search term, save existing folder open state
        if (!mInventoryPanel->getFilter().isNotDefault())
        {
            mSavedFolderState.setApply(FALSE);
            mInventoryPanel->getRootFolder()->applyFunctorRecursively(mSavedFolderState);
        }
    }

    mInventoryPanel->setFilterSubString(search_string);
}

void LLFloaterTexturePicker::setLocalTextureEnabled(BOOL enabled)
{
    mModeSelector->setIndexEnabled(1, enabled);
}

void LLFloaterTexturePicker::onTextureSelect(const LLTextureEntry& te)
{
    LLUUID inventory_item_id = findItemID(te.getID(), TRUE);
    if (inventory_item_id.notNull())
    {
        LLToolPipette::getInstance()->setResult(TRUE, "");
        setImageID(te.getID());

        mNoCopyTextureSelected = FALSE;
        LLInventoryItem* itemp = gInventory.getItem(inventory_item_id);

        if (itemp && !itemp->getPermissions().allowCopyBy(gAgent.getID()))
        {
            // no copy texture
            mNoCopyTextureSelected = TRUE;
        }

        commitIfImmediateSet();
    }
    else
    {
        LLToolPipette::getInstance()->setResult(FALSE, LLTrans::getString("InventoryNoTexture"));
    }
}
