/** 
 * @file llpanelpermissions.h
 * @brief LLPanelPermissions class header file
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

#ifndef LL_LLPANELPERMISSIONS_H
#define LL_LLPANELPERMISSIONS_H

#include "llpanel.h"
#include "lluuid.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class llpanelpermissions
//
// Panel for permissions of an object.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLNameBox;
class LLSpinCtrl;
class LLTextBox;
class LLViewerInventoryItem;

class LLPanelPermissions : public LLPanel
{
public:
	LLPanelPermissions();
	virtual ~LLPanelPermissions();

	/*virtual*/	BOOL	postBuild() override;

	void refresh() override;							// refresh all labels as needed

protected:
	void onClickClaim();
	void onClickRelease();
	void onClickGroup();
	void cbGroupID(LLUUID group_id);
	void onClickDeedToGroup();
	void onClickDescToPos();
	void onClickPosToDesc();

	void onCommitPerm(BOOL new_state, U8 field, U32 perm);

	void onCommitGroupShare(const LLSD& user_data);

	void onCommitEveryoneMove(const LLSD& user_data);
	void onCommitEveryoneCopy(const LLSD& user_data);

	void onCommitNextOwnerModify(const LLSD& user_data);
	void onCommitNextOwnerCopy(const LLSD& user_data);
	void onCommitNextOwnerTransfer(const LLSD& user_data);
	
	void onCommitName(const LLSD& user_data);
	void onCommitDesc(const LLSD& user_data);

	static void onCommitSaleInfo(LLUICtrl* ctrl, void* data);
	void setAllSaleInfo();

	void onCommitClickAction(const LLSD& user_data);
	void onCommitIncludeInSearch(const LLSD& user_data);
	void onCommitExport(const LLSD& user_data);

	static LLViewerInventoryItem* findItem(LLUUID &object_id);

protected:
	void disableAll();
	
private:
	LLNameBox*		mLabelGroupName = nullptr;		// group name

	LLUUID			mCreatorID;
	LLUUID			mOwnerID;
	LLUUID			mLastOwnerID;

	LLButton*		mBtnSetGroup = nullptr;
	LLButton*		mBtnDeedGroup = nullptr;
	LLButton*		mBtnPosToDesc = nullptr;
	LLButton*		mBtnDescToPos = nullptr;
	LLCheckBoxCtrl* mCheckShareWithGroup = nullptr;
	LLCheckBoxCtrl* mCheckAllowEveryoneCopy = nullptr;
	LLCheckBoxCtrl* mCheckAllowEveryoneMove = nullptr;
	LLCheckBoxCtrl* mCheckAllowExport = nullptr;
	LLCheckBoxCtrl* mCheckNextOwnerModify = nullptr;
	LLCheckBoxCtrl* mCheckNextOwnerCopy = nullptr;
	LLCheckBoxCtrl* mCheckNextOwnerTransfer = nullptr;
	LLCheckBoxCtrl* mCheckForSale = nullptr;
	LLCheckBoxCtrl* mCheckShowInSearch = nullptr;
	LLComboBox*		mComboClickAction = nullptr;
	LLComboBox*		mComboSaleType = nullptr;
	LLLineEditor*	mEditorObjectName = nullptr;
	LLLineEditor*	mEditorObjectDesc = nullptr;
	LLSpinCtrl*		mSpinnerEditCost = nullptr;

	LLTextBox*		mLabelAdvPermB = nullptr;
	LLTextBox*		mLabelAdvPermO = nullptr;
	LLTextBox*		mLabelAdvPermG = nullptr;
	LLTextBox*		mLabelAdvPermE = nullptr;
	LLTextBox*		mLabelAdvPermN = nullptr;
	LLTextBox*		mLabelAdvPermF = nullptr;
	LLTextBox*		mLabelClickAction = nullptr;
	LLTextBox*		mLabelCreator = nullptr;
	LLTextBox*		mLabelCreatorName = nullptr;
	LLTextBox*		mLabelGroup = nullptr;
	LLTextBox*		mLabelLastOwner = nullptr;
	LLTextBox*		mLabelLastOwnerName = nullptr;
	LLTextBox*		mLabelNextOwnerCan = nullptr;
	LLTextBox*		mLabelObjectName = nullptr;
	LLTextBox*		mLabelObjectDesc = nullptr;
	LLTextBox*		mLabelOwner = nullptr;
	LLTextBox*		mLabelOwnerName = nullptr;
	LLTextBox*		mLabelPathFindingAttribs = nullptr;
	LLTextBox*		mLabelPermModify = nullptr;

};


#endif // LL_LLPANELPERMISSIONS_H
