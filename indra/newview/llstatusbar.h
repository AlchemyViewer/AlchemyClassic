/** 
 * @file llstatusbar.h
 * @brief LLStatusBar class definition
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

#ifndef LL_LLSTATUSBAR_H
#define LL_LLSTATUSBAR_H

#include "llpanel.h"

// "Constants" loaded from settings.xml at start time
extern S32 STATUS_BAR_HEIGHT;

class LLButton;
class LLLineEditor;
class LLMessageSystem;
class LLTextBox;
class LLTextEditor;
class LLUICtrl;
class LLUUID;
class LLFrameTimer;
class LLStatGraph;
class ALPanelQuickSettingsPulldown;
class LLPanelAOPulldown;
class LLPanelVolumePulldown;
class LLPanelAvatarComplexityPulldown;
class LLPanelNearByMedia;
class LLIconCtrl;

class LLStatusBar
:	public LLPanel
{
public:
	LLStatusBar(const LLRect& rect );
	/*virtual*/ ~LLStatusBar();
	
	/*virtual*/ void draw() override;

	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	/*virtual*/ BOOL postBuild() override;

	// MANIPULATORS
	void		setBalance(S32 balance);
	void		debitBalance(S32 debit);
	void		creditBalance(S32 credit);

	// Request the latest currency balance from the server
	static void sendMoneyBalanceRequest();

	void		setHealth(S32 percent);
	void		setAvComplexity(S32 complexity, F32 muted_pct);

	void		setLandCredit(S32 credit);
	void		setLandCommitted(S32 committed);

	void		refresh() override;
	void		setVisibleForMouselook(bool visible);
	// some elements should hide in mouselook

	// ACCESSORS
	S32			getBalance() const;
	S32			getHealth() const;

	BOOL isUserTiered() const;
	S32 getSquareMetersCredit() const;
	S32 getSquareMetersCommitted() const;
	S32 getSquareMetersLeft() const;

	LLPanelNearByMedia* getNearbyMediaPanel() const { return mPanelNearByMedia; }

private:
	void onClickBuyCurrency() const;
	void onVolumeChanged(const LLSD& newvalue);

	void onMouseEnterQuickSettings();
	void onMouseEnterAO();
	void onMouseEnterVolume();
	void onMouseEnterAvatarComplexity();
	void onMouseEnterNearbyMedia();

	static void onClickAOBtn(void* data);
	static void onClickVolume(void* data);
	static void onClickMediaToggle(void* data);
	static void onClickBalance(void* data);

	void onAOStateChanged();

	LLTextBox	*mTextTime;
	LLTextBox	*mTextFPS;

	LLStatGraph *mSGBandwidth;
	LLStatGraph *mSGPacketLoss;

	LLView		*mPanelPopupHolder;
	LLButton	*mBtnQuickSettings;
	LLButton	*mBtnAO;
	LLButton	*mBtnVolume;
	LLTextBox	*mBoxBalance;
	LLButton	*mBtnBuyL;
	LLIconCtrl	*mAvComplexity;
	LLUICtrl	*mPanelFlycam;
	LLButton	*mMediaToggle;
	LLFrameTimer	mClockUpdateTimer;
	LLFrameTimer	mFPSUpdateTimer;

	S32				mBalance;
	S32				mHealth;
	S32				mSquareMetersCredit;
	S32				mSquareMetersCommitted;
	LLFrameTimer*	mBalanceTimer;
	LLFrameTimer*	mHealthTimer;
	ALPanelQuickSettingsPulldown* mPanelQuickSettingsPulldown;
	LLPanelAOPulldown* mPanelAOPulldown;
	LLPanelVolumePulldown* mPanelVolumePulldown;
	LLPanelAvatarComplexityPulldown* mPanelAvatarComplexityPulldown;
	LLPanelNearByMedia*	mPanelNearByMedia;

	LLPointer<LLUIImage> mImgAvComplex;
	LLPointer<LLUIImage> mImgAvComplexWarn;
	LLPointer<LLUIImage> mImgAvComplexHeavy;
};

// *HACK: Status bar owns your cached money balance. JC
BOOL can_afford_transaction(S32 cost);

extern LLStatusBar *gStatusBar;

#endif
