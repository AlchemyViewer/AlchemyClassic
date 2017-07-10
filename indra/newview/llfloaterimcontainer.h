/** 
 * @file llfloaterimcontainer.h
 * @brief Multifloater containing active IM sessions in separate tab container tabs
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#ifndef LL_LLFLOATERIMCONTAINER_H
#define LL_LLFLOATERIMCONTAINER_H

// LLUI
#include "lltrans.h"
#include "llfloater.h"
#include "llmultifloater.h"
// newview
#include "llimview.h"
#include "llevents.h"
#include "llgroupmgr.h"
#include "llconversationmodel.h"
#include "llconversationview.h"

class LLButton;
class LLLayoutPanel;
class LLLayoutStack;
class LLTabContainer;
class LLFloaterIMContainer;
class LLSpeaker;
class LLSpeakerMgr;

class LLFloaterIMContainer
	: public LLMultiFloater
	, public LLIMSessionObserver
{
public:
	LLFloaterIMContainer(const LLSD& seed, const Params& params = getDefaultParams());
	virtual ~LLFloaterIMContainer();
	
	BOOL postBuild() override;
	void onOpen(const LLSD& key) override;
	void draw() override;
	void setMinimized(BOOL b) override;
	void setVisible(BOOL visible) override;
	void setVisibleAndFrontmost(BOOL take_focus=TRUE, const LLSD& key = LLSD()) override;
	void updateResizeLimits() override;
	void handleReshape(const LLRect& rect, bool by_user) override;

	void onCloseFloater(LLUUID& id);

	void addFloater(LLFloater* floaterp, 
		BOOL select_added_floater, 
		LLTabContainer::eInsertionPoint insertion_point = LLTabContainer::END) override;
	void returnFloaterToHost();
    void showConversation(const LLUUID& session_id);
    void selectConversation(const LLUUID& session_id);
	void selectNextConversationByID(const LLUUID& session_id);
    BOOL selectConversationPair(const LLUUID& session_id, bool select_widget, bool focus_floater = true);
    void clearAllFlashStates();
	bool selectAdjacentConversation(bool focus_selected);
    bool selectNextorPreviousConversation(bool select_next, bool focus_selected = true);
    void expandConversation();

	void tabClose() override;
	void showStub(bool visible);

	static LLFloaterIMContainer* findInstance();
	static LLFloaterIMContainer* getInstance();

	static void onCurrentChannelChanged(const LLUUID& session_id);

	void collapseMessagesPane(bool collapse);
	bool isMessagesPaneCollapsed() const;
	bool isConversationsPaneCollapsed() const;
	
	// Callbacks
	static void idle(void* user_data);

	// LLIMSessionObserver observe triggers
	void sessionAdded(const LLUUID& session_id, const std::string& name, const LLUUID& other_participant_id, BOOL has_offline_msg) override;
    void sessionActivated(const LLUUID& session_id, const std::string& name, const LLUUID& other_participant_id) override;
	void sessionVoiceOrIMStarted(const LLUUID& session_id) override;
	void sessionRemoved(const LLUUID& session_id) override;
	void sessionIDUpdated(const LLUUID& old_session_id, const LLUUID& new_session_id) override;

	LLConversationViewModel& getRootViewModel() { return mConversationViewModel; }
    LLUUID getSelectedSession() const { return mSelectedSession; }
    void setSelectedSession(LLUUID sessionID) { mSelectedSession = sessionID; }
	LLConversationItem* getSessionModel(const LLUUID& session_id) const { return get_ptr_in_map(mConversationsItems,session_id); }
	LLConversationSort& getSortOrder() { return mConversationViewModel.getSorter(); }

	// Handling of lists of participants is public so to be common with llfloatersessiontab
	// *TODO : Find a better place for this.
    bool checkContextMenuItem(const std::string& item, uuid_vec_t& selectedIDS);
    bool enableContextMenuItem(const std::string& item, uuid_vec_t& selectedIDS);
    void doToParticipants(const std::string& item, uuid_vec_t& selectedIDS);

	void assignResizeLimits();
	BOOL handleKeyHere(KEY key, MASK mask ) override;
	void closeFloater(bool app_quitting = false) override;
    void closeAllConversations();
    void closeSelectedConversations(const uuid_vec_t& ids);
	BOOL isFrontmost() override;


private:
	typedef std::map<LLUUID,LLFloater*> avatarID_panel_map_t;
	avatarID_panel_map_t mSessions;
	boost::signals2::connection mNewMessageConnection;

	void computeResizeLimits(S32& new_min_width, S32& new_min_height) override {}

	void onNewMessageReceived(const LLSD& data);

	void onExpandCollapseButtonClicked();
	void onStubCollapseButtonClicked();
	void processParticipantsStyleUpdate();
	void onSpeakButtonPressed();
	void onSpeakButtonReleased();
	void onClickCloseBtn(bool app_quitting = false) override;
	void closeHostedFloater() override;

	void collapseConversationsPane(bool collapse, bool save_is_allowed=true);

	void reshapeFloaterAndSetResizeLimits(bool collapse, S32 delta_width);

	void onAddButtonClicked();
	void onAvatarPicked(const uuid_vec_t& ids);

	BOOL isActionChecked(const LLSD& userdata);
	void onCustomAction (const LLSD& userdata);
	void setSortOrderSessions(const LLConversationFilter::ESortOrderType order);
	void setSortOrderParticipants(const LLConversationFilter::ESortOrderType order);
	void setSortOrder(const LLConversationSort& order);

    void getSelectedUUIDs(uuid_vec_t& selected_uuids, bool participant_uuids = true) const;
    const LLConversationItem * getCurSelectedViewModelItem() const;
    void getParticipantUUIDs(uuid_vec_t& selected_uuids);
    void doToSelected(const LLSD& userdata);
	bool checkContextMenuItem(const LLSD& userdata);
	bool enableContextMenuItem(const LLSD& userdata);
	bool visibleContextMenuItem(const LLSD& userdata);
    void doToSelectedConversation(const std::string& command, uuid_vec_t& selectedIDS);
    void doToSelectedGroup(const LLSD& userdata);

	static void confirmMuteAllCallback(const LLSD& notification, const LLSD& response);
	bool enableModerateContextMenuItem(const std::string& userdata);
	LLSpeaker * getSpeakerOfSelectedParticipant(LLSpeakerMgr * speaker_managerp) const;
	LLSpeakerMgr * getSpeakerMgrForSelectedParticipant();
	bool isGroupModerator();
	bool haveAbilityToBan();
	bool canBanSelectedMember(const LLUUID& participant_uuid);
	bool isMuted(const LLUUID& avatar_id);
	void moderateVoice(const std::string& command, const LLUUID& userID);
	void moderateVoiceAllParticipants(bool unmute);
	void moderateVoiceParticipant(const LLUUID& avatar_id, bool unmute);
	void toggleAllowTextChat(const LLUUID& participant_uuid);
	void toggleMute(const LLUUID& participant_id, U32 flags);
	void banSelectedMember(const LLUUID& participant_uuid);
	bool isParticipantListExpanded() const;

	LLButton* mExpandCollapseBtn;
	LLButton* mStubCollapseBtn;
    LLButton* mSpeakBtn;
	LLPanel* mStubPanel;
	LLTextBox* mStubTextBox;
	LLLayoutPanel* mMessagesPane;
	LLLayoutPanel* mConversationsPane;
	LLLayoutStack* mConversationsStack;
	
	bool mInitialized;
	bool mIsFirstLaunch;

	bool mIsFirstOpen;

	LLUUID mSelectedSession;
	std::string mGeneralTitle;

	// Conversation list implementation
public:
	bool removeConversationListItem(const LLUUID& uuid);
	LLConversationItem* addConversationListItem(const LLUUID& uuid, bool isWidgetSelected = false);
	void setTimeNow(const LLUUID& session_id, const LLUUID& participant_id);
	void setNearbyDistances();
	void reSelectConversation();
	void updateSpeakBtnState();
	void updateTypingState(const LLUUID& session_id, bool typing);
	static bool isConversationLoggingAllowed();
	void flashConversationItemWidget(const LLUUID& session_id, bool is_flashes);
	void highlightConversationItemWidget(const LLUUID& session_id, bool is_highlighted);
	bool isScrolledOutOfSight(LLConversationViewSession* conversation_item_widget) const;
	boost::signals2::connection mMicroChangedSignal;
	S32 getConversationListItemSize() const { return mConversationsWidgets.size(); }
	typedef std::list<LLFloater*> floater_list_t;
	void getDetachedConversationFloaters(floater_list_t& floaters);

private:
	LLConversationViewSession* createConversationItemWidget(LLConversationItem* item);
	LLConversationViewParticipant* createConversationViewParticipant(LLConversationItem* item);
	bool onConversationModelEvent(const LLSD& event);

	// Conversation list data
	LLPanel* mConversationsListPanel;	// This is the main widget we add conversation widget to
	conversations_items_map mConversationsItems;
	conversations_widgets_map mConversationsWidgets;
	LLConversationViewModel mConversationViewModel;
	LLFolderView* mConversationsRoot;
	LLEventStream mConversationsEventStream; 
};

#endif // LL_LLFLOATERIMCONTAINER_H
