/** 
 * @file llpanelgroup.h
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#ifndef LL_LLPANELGROUP_H
#define LL_LLPANELGROUP_H

#include "llgroupmgr.h"
#include "llpanel.h"
#include "lltimer.h"
#include "llvoiceclient.h"
#include <unordered_map>

class LLOfferInfo;

const F32 UPDATE_MEMBERS_SECONDS_PER_FRAME = 0.005f; // 5ms // <alchemy/>

// Forward declares
class LLPanelGroupTab;
class LLTabContainer;
class LLAgent;
class LLAccordionCtrl;


class LLPanelGroup : public LLPanel,
					 public LLGroupMgrObserver,
					 public LLVoiceClientStatusObserver
{
	friend class LLGroupActions;

public:
	LLPanelGroup();
	virtual ~LLPanelGroup();
	BOOL postBuild() override;

	void setGroupID(const LLUUID& group_id);
	void draw() override;
	void onOpen(const LLSD& key) override;

	// Group manager observer trigger.
	void changed(LLGroupChange gc) override;

	// Implements LLVoiceClientStatusObserver::onChange() to enable the call
	// button when voice is available
	void onChange(EStatusType status, const std::string &channelURI, bool proximal) override;

	void showNotice(const std::string& subject,
					const std::string& message,
					const bool& has_inventory,
					const std::string& inventory_name,
					LLOfferInfo* inventory_offer);

	void notifyObservers();

	bool apply();
	void refreshData();
	void callGroup();
	void chatGroup();

	void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE) override;

	static void refreshCreatedGroup(const LLUUID& group_id);

	static void showNotice(const std::string& subject,
						   const std::string& message,
						   const LLUUID& group_id,
						   const bool& has_inventory,
						   const std::string& inventory_name,
						   LLOfferInfo* inventory_offer);


protected:
	virtual void update(LLGroupChange gc);

	void onBtnCreate();
	void onBackBtnClick();
	void onBtnJoin() const;
	void onBtnCancel();
	void onBtnApply();

	void reposButton(LLButton* button);
	void reposButtons();
	
	bool apply(LLPanelGroupTab* tab);

	void refreshNotices();

	LLTimer mRefreshTimer;

	BOOL mSkipRefresh;

	std::string mDefaultNeedsApplyMesg;
	std::string mWantApplyMesg;

	std::vector<LLPanelGroupTab* > mTabs;

	LLAccordionCtrl*	mAccordianGroup;
	LLButton*			mButtonApply;
	LLButton*			mButtonCall;
	LLButton*			mButtonChat;
	LLButton*			mButtonCreate;
	LLButton*			mButtonJoin;
	LLButton*			mButtonRefresh;
	LLUICtrl*			mJoinText;
	
private:
	void copyData(const LLSD& userdata);
	void closeParentFloater();
};

using panel_multimap_t = std::unordered_multimap< LLUUID, LLPanelGroup* >;
static panel_multimap_t sGroupPanelInstances;

class LLPanelGroupTab : public LLPanel
{
public:
	LLPanelGroupTab();
	virtual ~LLPanelGroupTab();

	// Triggered when the tab becomes active.
	virtual void activate() { }
	
	// Triggered when the tab becomes inactive.
	virtual void deactivate() { }

	// Asks if something needs to be applied.
	// If returning true, this function should modify the message to the user.
	virtual bool needsApply(std::string& mesg) { return false; }

	// Asks if there is currently a modal dialog being shown.
	virtual BOOL hasModal() { return mHasModal; }

	// Request to apply current data.
	// If returning fail, this function should modify the message to the user.
	virtual bool apply(std::string& mesg) { return true; }

	// Request a cancel of changes
	virtual void cancel() { }

	// Triggered when group information changes in the group manager.
	virtual void update(LLGroupChange gc) { }

	// This just connects the help button callback.
	BOOL postBuild() override;

	virtual BOOL isVisibleByAgent(LLAgent* agentp);

	virtual void setGroupID(const LLUUID& id) {mGroupID = id;};

	void notifyObservers() {};

	const LLUUID& getGroupID() const { return mGroupID;}

	virtual void setupCtrls	(LLPanel* parent) {};

protected:
	LLUUID	mGroupID;
	BOOL mAllowEdit;
	BOOL mHasModal;
};

#endif // LL_LLPANELGROUP_H
