/** 
 * @file lltexturectrl.h
 * @author Richard Nelson, James Cook
 * @brief LLTextureCtrl class header file including related functions
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

#ifndef LL_LLTEXTURECTRL_H
#define LL_LLTEXTURECTRL_H

#include "llcoord.h"
#include "llfiltereditor.h"
#include "llfloater.h"
#include "llfolderview.h"
#include "lllocalbitmaps.h"
#include "llstring.h"
#include "lluictrl.h"
#include "llpermissionsflags.h"
#include "llradiogroup.h"
#include "lltextbox.h" // for params
#include "llviewerinventory.h"
#include "llviewborder.h" // for params
#include "llviewerobject.h"
#include "llviewertexture.h"
#include "llwindow.h"

class LLButton;
class LLFloaterTexturePicker;
class LLInventoryItem;
class LLViewerFetchedTexture;

// used for setting drag & drop callbacks.
typedef std::function<BOOL (LLUICtrl*, LLInventoryItem*)> drag_n_drop_callback;
typedef std::function<void (LLInventoryItem*)> texture_selected_callback;


//////////////////////////////////////////////////////////////////////////////////////////
// LLTextureCtrl


class LLTextureCtrl
: public LLUICtrl
{
public:
	typedef enum e_texture_pick_op
	{
		TEXTURE_CHANGE,
		TEXTURE_SELECT,
		TEXTURE_CANCEL
	} ETexturePickOp;

public:
	struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		Optional<LLUUID>		image_id;
		Optional<LLUUID>		default_image_id;
		Optional<std::string>	default_image_name;
		Optional<bool>			allow_no_texture;
		Optional<bool>			can_apply_immediately;
		Optional<bool>			no_commit_on_selection; // alternative mode: commit occurs and the widget gets dirty
														// only on DnD or when OK is pressed in the picker
		Optional<S32>			label_width;
		Optional<LLUIColor>		border_color;
		Optional<LLUIImage*>	fallback_image;
		
		Optional<LLTextBox::Params>	multiselect_text,
									caption_text;

		Optional<LLViewBorder::Params> border;

		Params()
		:	image_id("image"),
			default_image_id("default_image_id"),
			default_image_name("default_image_name"),
			allow_no_texture("allow_no_texture"),
			can_apply_immediately("can_apply_immediately"),
			no_commit_on_selection("no_commit_on_selection", false),
		    label_width("label_width", -1),
			border_color("border_color"),
			fallback_image("fallback_image"),
			multiselect_text("multiselect_text"),
			caption_text("caption_text"),
			border("border")
		{}
	};
protected:
	LLTextureCtrl(const Params&);
	friend class LLUICtrlFactory;
public:
	virtual ~LLTextureCtrl();

	// LLView interface

	BOOL	handleMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL	handleDragAndDrop(S32 x, S32 y, MASK mask,
						BOOL drop, EDragAndDropType cargo_type, void *cargo_data,
						EAcceptance *accept,
						std::string& tooltip_msg) override;
	BOOL	handleHover(S32 x, S32 y, MASK mask) override;
	BOOL	handleUnicodeCharHere(llwchar uni_char) override;

	void	draw() override;
	void	setVisible( BOOL visible ) override;
	void	setEnabled( BOOL enabled ) override;

	void			setValid(BOOL valid);

	// LLUICtrl interface
	void	clear() override;

	// Takes a UUID, wraps get/setImageAssetID
	void	setValue(const LLSD& value) override;
	LLSD	getValue() const override;

	// LLTextureCtrl interface
	void			showPicker(BOOL take_focus);
	bool			isPickerShown() { return !mFloaterHandle.isDead(); }
	void			setLabel(const std::string& label);
	void			setLabelWidth(S32 label_width) {mLabelWidth =label_width;}	
	const std::string&	getLabel() const							{ return mLabel; }

	void			setAllowNoTexture( BOOL b )					{ mAllowNoTexture = b; }
	bool			getAllowNoTexture() const					{ return mAllowNoTexture; }

	const LLUUID&	getImageItemID() { return mImageItemID; }

	virtual void	setImageAssetName(const std::string& name);
	
	void			setImageAssetID(const LLUUID &image_asset_id);
	const LLUUID&	getImageAssetID() const						{ return mImageAssetID; }

	void			setDefaultImageAssetID( const LLUUID& id )	{ mDefaultImageAssetID = id; }
	const LLUUID&	getDefaultImageAssetID() const { return mDefaultImageAssetID; }

	const std::string&	getDefaultImageName() const					{ return mDefaultImageName; }

	void			setBlankImageAssetID( const LLUUID& id )	{ mBlankImageAssetID = id; }
	const LLUUID&	getBlankImageAssetID() const { return mBlankImageAssetID; }

	// <alchemy>
	void			setTransparentImageAssetID( const LLUUID& id )	{ mTransparentImageAssetID = id; }
	const LLUUID&	getTransparentImageAssetID() const { return mTransparentImageAssetID; }
	// </alchemy>

	void			setCaption(const std::string& caption);
	void			setCanApplyImmediately(BOOL b);

	void			setCanApply(bool can_preview, bool can_apply);

	void			setImmediateFilterPermMask(PermissionMask mask)
					{ mImmediateFilterPermMask = mask; }
	void			setDnDFilterPermMask(PermissionMask mask)
						{ mDnDFilterPermMask = mask; }
	void			setNonImmediateFilterPermMask(PermissionMask mask)
					{ mNonImmediateFilterPermMask = mask; }
	PermissionMask	getImmediateFilterPermMask() { return mImmediateFilterPermMask; }
	PermissionMask	getNonImmediateFilterPermMask() { return mNonImmediateFilterPermMask; }

	void			closeDependentFloater();

	void			onFloaterClose();
	void			onFloaterCommit(ETexturePickOp op, LLUUID id);

	// This call is returned when a drag is detected. Your callback
	// should return TRUE if the drag is acceptable.
	void setDragCallback(drag_n_drop_callback cb)	{ mDragCallback = cb; }

	// This callback is called when the drop happens. Return TRUE if
	// the drop happened - resulting in an on commit callback, but not
	// necessariliy any other change.
	void setDropCallback(drag_n_drop_callback cb)	{ mDropCallback = cb; }
	
	void setOnCancelCallback(commit_callback_t cb)	{ mOnCancelCallback = cb; }
	void setOnCloseCallback(commit_callback_t cb)	{ mOnCloseCallback = cb; }
	void setOnSelectCallback(commit_callback_t cb)	{ mOnSelectCallback = cb; }

	/*
	 * callback for changing texture selection in inventory list of texture floater
	 */
	void setOnTextureSelectedCallback(texture_selected_callback cb);

	void setShowLoadingPlaceholder(BOOL showLoadingPlaceholder);

	LLViewerFetchedTexture* getTexture() { return mTexturep; }

private:
	BOOL allowDrop(LLInventoryItem* item);
	BOOL doDrop(LLInventoryItem* item);

private:
	drag_n_drop_callback	 	mDragCallback;
	drag_n_drop_callback	 	mDropCallback;
	commit_callback_t		 	mOnCancelCallback;
	commit_callback_t		 	mOnSelectCallback;
	commit_callback_t		 	mOnCloseCallback;
	texture_selected_callback	mOnTextureSelectedCallback;
	LLPointer<LLViewerFetchedTexture> mTexturep;
	LLUIColor				 	mBorderColor;
	LLUUID					 	mImageItemID;
	LLUUID					 	mImageAssetID;
	LLUUID					 	mDefaultImageAssetID;
	LLUUID					 	mBlankImageAssetID;
	LLUUID					 	mTransparentImageAssetID; // <alchemy/>
	LLUIImagePtr				mFallbackImage;
	std::string					mDefaultImageName;
	LLHandle<LLFloater>			mFloaterHandle;
	LLTextBox*				 	mTentativeLabel;
	LLTextBox*				 	mCaption;
	std::string				 	mLabel;
	BOOL					 	mAllowNoTexture; // If true, the user can select "none" as an option
	PermissionMask			 	mImmediateFilterPermMask;
	PermissionMask				mDnDFilterPermMask;
	PermissionMask			 	mNonImmediateFilterPermMask;
	BOOL					 	mCanApplyImmediately;
	BOOL					 	mCommitOnSelection;
	BOOL					 	mNeedsRawImageData;
	LLViewBorder*			 	mBorder;
	BOOL					 	mValid;
	BOOL					 	mShowLoadingPlaceholder;
	std::string				 	mLoadingPlaceholderString;
	S32						 	mLabelWidth;
	BOOL						mPreview;
};

//////////////////////////////////////////////////////////////////////////////////////////
// LLFloaterTexturePicker
typedef std::function<void(LLTextureCtrl::ETexturePickOp op, LLUUID id)> floater_commit_callback;
typedef std::function<void()> floater_close_callback;
typedef std::function<void(const LLUUID& asset_id)> set_image_asset_id_callback;
typedef std::function<void(LLPointer<LLViewerTexture> texture)> set_on_update_image_stats_callback;

class LLFloaterTexturePicker : public LLFloater
{
public:
	LLFloaterTexturePicker(
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
		LLUIImagePtr fallback_image_name
		);

	virtual ~LLFloaterTexturePicker();

	// LLView overrides
	/*virtual*/ BOOL	handleDragAndDrop(S32 x, S32 y, MASK mask,
		BOOL drop, EDragAndDropType cargo_type, void *cargo_data,
		EAcceptance *accept,
		std::string& tooltip_msg) override;
	/*virtual*/ void	draw() override;
	/*virtual*/ BOOL	handleKeyHere(KEY key, MASK mask) override;

	// LLFloater overrides
	/*virtual*/ BOOL    postBuild() override;
	/*virtual*/ void	onClose(bool app_settings) override;

	// New functions
	void setImageID(const LLUUID& image_asset_id, bool set_selection = true);
	void updateImageStats();
	const LLUUID&	getAssetID() { return mImageAssetID; }
	const LLUUID&	findItemID(const LLUUID& asset_id, BOOL copyable_only, BOOL ignore_library = FALSE);
	void			setCanApplyImmediately(BOOL b);

	void			setActive(BOOL active);

	LLView*			getOwner() const { return mOwner; }
	void			setOwner(LLView* owner) { mOwner = owner; }
	void			stopUsingPipette();
	PermissionMask 	getFilterPermMask();

	void updateFilterPermMask();
	void commitIfImmediateSet();
	void commitCancel();

	void onFilterEdit(const std::string& search_string);

	void setCanApply(bool can_preview, bool can_apply);
	void setTextureSelectedCallback(const texture_selected_callback& cb) { mTextureSelectedCallback = cb; }
	void setOnFloaterCloseCallback(const floater_close_callback& cb) { mOnFloaterCloseCallback = cb; }
	void setOnFloaterCommitCallback(const floater_commit_callback& cb) { mOnFloaterCommitCallback = cb; }
	void setSetImageAssetIDCallback(const set_image_asset_id_callback& cb) { mSetImageAssetIDCallback = cb; }
	void setOnUpdateImageStatsCallback(const set_on_update_image_stats_callback& cb) { mOnUpdateImageStatsCallback = cb; }
	const LLUUID& getDefaultImageAssetID() { return mDefaultImageAssetID; }
	const LLUUID& getTransparentImageAssetID() { return mTransparentImageAssetID; }
	const LLUUID& getBlankImageAssetID() { return mBlankImageAssetID; }

	static void		onBtnSetToDefault(void* userdata);
	static void		onBtnSelect(void* userdata);
	static void		onBtnCancel(void* userdata);
	void			onBtnPipette();
	//static void		onBtnRevert( void* userdata );
	static void		onBtnTransparent(void* userdata);
	static void		onBtnBlank(void* userdata);
	static void		onBtnNone(void* userdata);
	static void		onBtnClear(void* userdata);
	static void		onApplyUUID(void* userdata);
	void			onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action);
	static void		onShowFolders(LLUICtrl* ctrl, void* userdata);
	static void		onApplyImmediateCheck(LLUICtrl* ctrl, void* userdata);
	void			onTextureSelect(const LLTextureEntry& te);

	void			onModeSelect();
	static void		onBtnAdd(void* userdata);
	static void		onBtnRemove(void* userdata);
	static void		onBtnUpload(void* userdata);
	void			onLocalScrollCommit();

	void 			setLocalTextureEnabled(BOOL enabled);

protected:
	LLPointer<LLViewerTexture> mTexturep;
	LLView*				mOwner;

	LLUUID				mImageAssetID; // Currently selected texture
	LLUIImagePtr		mFallbackImage; // What to show if currently selected texture is null.
	LLUUID				mDefaultImageAssetID;
	LLUUID				mTransparentImageAssetID;
	LLUUID				mBlankImageAssetID;
	BOOL				mTentative;
	BOOL				mAllowNoTexture;
	LLUUID				mSpecialCurrentImageAssetID;  // Used when the asset id has no corresponding texture in the user's inventory.
	LLUUID				mOriginalImageAssetID;

	std::string			mLabel;

	LLTextBox*			mTentativeLabel;
	LLTextBox*			mResolutionLabel;

	std::string			mPendingName;
	BOOL				mActive;

	LLFilterEditor*		mFilterEdit;
	LLInventoryPanel*	mInventoryPanel;
	PermissionMask		mImmediateFilterPermMask;
	PermissionMask		mDnDFilterPermMask;
	PermissionMask		mNonImmediateFilterPermMask;
	BOOL				mCanApplyImmediately;
	BOOL				mNoCopyTextureSelected;
	F32					mContextConeOpacity;
	LLSaveFolderState	mSavedFolderState;
	BOOL				mSelectedItemPinned;

	LLRadioGroup*		mModeSelector;
	LLScrollListCtrl*	mLocalScrollCtrl;

private:
	bool mCanApply;
	bool mCanPreview;
	bool mPreviewSettingChanged;

	texture_selected_callback mTextureSelectedCallback;
	floater_close_callback mOnFloaterCloseCallback;
	floater_commit_callback mOnFloaterCommitCallback;
	set_image_asset_id_callback mSetImageAssetIDCallback;
	set_on_update_image_stats_callback mOnUpdateImageStatsCallback;
};

#endif  // LL_LLTEXTURECTRL_H
