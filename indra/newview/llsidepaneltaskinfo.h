/** 
 * @file llsidepaneltaskinfo.h
 * @brief LLSidepanelTaskInfo class header file
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

#ifndef LL_LLSIDEPANELTASKINFO_H
#define LL_LLSIDEPANELTASKINFO_H

#include "llsidepanelinventorysubpanel.h"
#include "lluuid.h"
#include "llselectmgr.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLSidepanelTaskInfo
//
// Panel for permissions of an object.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLCheckBoxCtrl;
class LLComboBox;
class LLNameBox;
class LLViewerObject;
class LLTextBox;
class LLLineEditor;
class LLSpinCtrl;

class LLSidepanelTaskInfo : public LLSidepanelInventorySubpanel
{
public:
	LLSidepanelTaskInfo(const LLPanel::Params& p = getDefaultParams());
	virtual ~LLSidepanelTaskInfo();

	BOOL postBuild() override;
	void onVisibilityChange ( BOOL new_visibility ) override;

	void setObjectSelection(LLObjectSelectionHandle selection);

	const LLUUID& getSelectedUUID();
	LLViewerObject* getFirstSelectedObject();

	static LLSidepanelTaskInfo *getActivePanel();
protected:
	void refresh() override;	// refresh all labels as needed
	void save() override;
	void updateVerbs() override;

	void refreshAll(); // ignore current keyboard focus and update all fields

	// statics
	void onClickGroup();
	void cbGroupID(LLUUID group_id);
	void onClickDeedToGroup() const;

	void onCommitPerm(BOOL enabled, U8 field, U32 perm);

	void onCommitGroupShare(const LLSD& user_data);

	void onCommitEveryoneMove(const LLSD& user_data);
	void onCommitEveryoneCopy(const LLSD& user_data);

	void onCommitNextOwnerModify(const LLSD& user_data);
	void onCommitNextOwnerCopy(const LLSD& user_data);
	void onCommitNextOwnerTransfer(const LLSD& user_data);
	void onCommitNextOwnerExport(const LLSD& user_data);
	
	void onCommitName(const LLSD& user_data);
	void onCommitDesc(const LLSD& user_data);

	void setAllSaleInfo();

	void onCommitClickAction(const LLSD& user_data);
	void onCommitIncludeInSearch(const LLSD& user_data);

	void doClickAction(U8 click_action);
	void disableAll();

private:
	void closeParentFloater();

	LLUUID			mCreatorID;
	LLUUID			mOwnerID;
	LLUUID			mLastOwnerID;

protected:
	void 						onOpenButtonClicked();
	void 						onPayButtonClicked();
	void 						onBuyButtonClicked();
	void 						onDetailsButtonClicked();

	LLViewerObject*				getObject();

private:
	LLPointer<LLViewerObject>	mObject;
	LLObjectSelectionHandle		mObjectSelection;
	static LLSidepanelTaskInfo* sActivePanel;
	
	// Pointers cached here to speed up ui refresh
	LLTextBox*					mObjectNameLabel;
	LLLineEditor*				mObjectNameEditor;
	LLTextBox*					mObjectDescriptionLabel;
	LLLineEditor*				mObjectDescriptionEditor;
	LLTextBox*					mCreatorNameLabel;
	LLTextBox*					mCreatorNameEditor;
	LLTextBox*					mOwnerNameLabel;
	LLTextBox*					mOwnerNameEditor;
	LLTextBox*					mGroupNameLabel;
	LLButton*					mGroupSetButton;
	LLNameBox*					mGroupNameBox;		// group name
	LLButton*					mDeedBtn;
	LLTextBox*					mClickActionLabel;
	LLComboBox*					mClickActionCombo;
	LLTextBox*					mPermModifyLabel;
	LLCheckBoxCtrl*				mAllowEveryoneCopyCheck;
	LLCheckBoxCtrl*				mAllowEveryoneMoveCheck;
	LLCheckBoxCtrl*				mShareWithGroupCheck;
	LLTextBox*					mNextOwnerPermLabel;
	LLCheckBoxCtrl*				mNextOwnerCanModifyCheck;
	LLCheckBoxCtrl*				mNextOwnerCanCopyCheck;
	LLCheckBoxCtrl*				mNextOwnerCanTransferCheck;
	LLCheckBoxCtrl*				mNextOwnerCanExportCheck;
	LLTextBox*					mObjectRenderStatsText;
	LLCheckBoxCtrl*				mForSaleCheck;
	LLComboBox*					mSaleTypeCombo;
	LLSpinCtrl*					mSaleCostSpinner;
	LLCheckBoxCtrl*				mSearchCheck;
	LLTextBox*					mPathfindingAttributesText;
	LLTextBox*					mAdvPermB;
	LLTextBox*					mAdvPermO;
	LLTextBox*					mAdvPermG;
	LLTextBox*					mAdvPermE;
	LLTextBox*					mAdvPermN;
	LLTextBox*					mAdvPermF;

	LLButton*					mOpenBtn;
	LLButton*					mPayBtn;
	LLButton*					mBuyBtn;
	LLButton*					mDetailsBtn;
};

#endif // LL_LLSIDEPANELTASKINFO_H
