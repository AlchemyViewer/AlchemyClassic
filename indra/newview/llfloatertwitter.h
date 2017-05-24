/** 
* @file   llfloatertwitter.h
* @brief  Header file for llfloatertwitter
* @author cho@lindenlab.com
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2013, Linden Research, Inc.
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
#ifndef LL_LLFLOATERTWITTER_H
#define LL_LLFLOATERTWITTER_H

#include "llfloater.h"
#include "llviewertexture.h"

class LLIconCtrl;
class LLCheckBoxCtrl;
class LLSnapshotLivePreview;
class LLTextBox;
class LLFloaterBigPreview;

class LLTwitterPhotoPanel : public LLPanel
{
public:
	LLTwitterPhotoPanel();
	~LLTwitterPhotoPanel();

	BOOL postBuild() override;
	void draw() override;

	LLSnapshotLivePreview* getPreviewView();
	void onVisibilityChange(BOOL new_visibility) override;
	void onAddLocationToggled();
	void onAddPhotoToggled();
    void onClickBigPreview();
	void onClickNewSnapshot();
	void onSend();
	S32 notify(const LLSD& info) override;
	bool onTwitterConnectStateChange(const LLSD& data);

	void sendPhoto();
	void clearAndClose();

	void updateStatusTextLength(BOOL restore_old_status_text);
	void updateControls();
	void updateResolution(BOOL do_update);
	void checkAspectRatio(S32 index);
	LLUICtrl* getRefreshBtn();

private:
    bool isPreviewVisible();
    void attachPreview();

	LLHandle<LLView> mPreviewHandle;

	LLUICtrl * mResolutionComboBox;
	LLUICtrl * mFilterComboBox;
	LLUICtrl * mRefreshBtn;
	LLUICtrl * mWorkingLabel;
	LLUICtrl * mThumbnailPlaceholder;
	LLUICtrl * mStatusCounterLabel;
	LLUICtrl * mStatusTextBox;
	LLUICtrl * mLocationCheckbox;
	LLUICtrl * mPhotoCheckbox;
	LLUICtrl * mPostButton;
	LLUICtrl * mCancelButton;
	LLButton * mBtnPreview;

    LLFloaterBigPreview * mBigPreviewFloater;
    
	std::string mOldStatusText;
};

class LLTwitterAccountPanel : public LLPanel
{
public:
	LLTwitterAccountPanel();
	BOOL postBuild() override;
	void draw() override;

private:
	void onVisibilityChange(BOOL new_visibility) override;
	bool onTwitterConnectStateChange(const LLSD& data);
	bool onTwitterConnectInfoChange();
	void onConnect();
	void onUseAnotherAccount();
	void onDisconnect();

	void showConnectButton();
	void hideConnectButton();
	void showDisconnectedLayout();
	void showConnectedLayout();

	LLTextBox * mAccountCaptionLabel;
	LLTextBox * mAccountNameLabel;
	LLUICtrl * mPanelButtons;
	LLUICtrl * mConnectButton;
	LLUICtrl * mDisconnectButton;
};


class LLFloaterTwitter : public LLFloater
{
public:
	LLFloaterTwitter(const LLSD& key);
	BOOL postBuild() override;
	void draw() override;
	void onClose(bool app_quitting) override;
	void onCancel();

	void showPhotoPanel();

private:
	LLTwitterPhotoPanel* mTwitterPhotoPanel;
    LLTextBox* mStatusErrorText;
    LLTextBox* mStatusLoadingText;
    LLUICtrl*  mStatusLoadingIndicator;
};

#endif // LL_LLFLOATERTWITTER_H

