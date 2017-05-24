/** 
 * @file llpanelgrouproles.h
 * @brief Panel for roles information about a particular group.
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

#ifndef LL_LLPANELGROUPROLES_H
#define LL_LLPANELGROUPROLES_H

#include "llpanelgroup.h"

class LLFilterEditor;
class LLNameListCtrl;
class LLPanelGroupSubTab;
class LLPanelGroupMembersSubTab;
class LLPanelGroupRolesSubTab;
class LLPanelGroupActionsSubTab;
class LLScrollListCtrl;
class LLScrollListItem;
class LLTextEditor;

typedef std::map<std::string,std::string> icon_map_t;


class LLPanelGroupRoles : public LLPanelGroupTab
{
public:
	LLPanelGroupRoles();
	virtual ~LLPanelGroupRoles();

	// Allow sub tabs to ask for sibling controls.
	friend class LLPanelGroupMembersSubTab;
	friend class LLPanelGroupRolesSubTab;
	friend class LLPanelGroupActionsSubTab;

	BOOL postBuild() override;
	BOOL isVisibleByAgent(LLAgent* agentp) override;

	
	bool handleSubTabSwitch(const LLSD& data);

	// Checks if the current tab needs to be applied, and tries to switch to the requested tab.
	BOOL attemptTransition();
	
	// Switches to the requested tab (will close() if requested is NULL)
	void transitionToTab();

	// Used by attemptTransition to query the user's response to a tab that needs to apply. 
	bool handleNotifyCallback(const LLSD& notification, const LLSD& response);
	bool onModalClose(const LLSD& notification, const LLSD& response);

	// Most of these messages are just passed on to the current sub-tab.
	void activate() override;
	void deactivate() override;
	bool needsApply(std::string& mesg) override;
	BOOL hasModal() override;
	bool apply(std::string& mesg) override;
	void cancel() override;
	void update(LLGroupChange gc) override;

	void setGroupID(const LLUUID& id) override;

protected:
	LLPanelGroupTab*		mCurrentTab;
	LLPanelGroupTab*		mRequestedTab;
	LLTabContainer*	mSubTabContainer;
	BOOL					mFirstUse;

	std::string				mDefaultNeedsApplyMesg;
	std::string				mWantApplyMesg;
};


class LLPanelGroupSubTab : public LLPanelGroupTab
{
public:
	LLPanelGroupSubTab();
	virtual ~LLPanelGroupSubTab();

	BOOL postBuild() override;

	// This allows sub-tabs to collect child widgets from a higher level in the view hierarchy.
	virtual BOOL postBuildSubTab(LLView* root);

	virtual void setSearchFilter( const std::string& filter );

	void activate() override;
	void deactivate() override;

	// Helper functions
	bool matchesActionSearchFilter(std::string action);


	void setFooterEnabled(BOOL enable);

	void setGroupID(const LLUUID& id) override;
protected:
	void buildActionsList(LLScrollListCtrl* ctrl,
								 U64 allowed_by_some,
								 U64 allowed_by_all,
						  		 LLUICtrl::commit_callback_t commit_callback,
								 BOOL show_all,
								 BOOL filter,
								 BOOL is_owner_role);
	void buildActionCategory(LLScrollListCtrl* ctrl,
									U64 allowed_by_some,
									U64 allowed_by_all,
									LLRoleActionSet* action_set,
									LLUICtrl::commit_callback_t commit_callback,
									BOOL show_all,
									BOOL filter,
									BOOL is_owner_role);

protected:
	LLPanel* mFooter;

	LLFilterEditor*	mSearchEditor;

	std::string mSearchFilter;

	icon_map_t	mActionIcons;

	bool mActivated;
	
	bool mHasGroupBanPower; // Used to communicate between action sets due to the dependency between
							// GP_GROUP_BAN_ACCESS and GP_EJECT_MEMBER and GP_ROLE_REMOVE_MEMBER
	
	void setOthersVisible(BOOL b);
};


class LLPanelGroupMembersSubTab : public LLPanelGroupSubTab
{
public:
	LLPanelGroupMembersSubTab();
	virtual ~LLPanelGroupMembersSubTab();

	BOOL postBuildSubTab(LLView* root) override;

	void handleMemberSelect();

	static void onMemberDoubleClick(void*);
	void handleMemberDoubleClick();

	static void onInviteMember(void*);
	void handleInviteMember();

	static void onEjectMembers(void*);
	void handleEjectMembers();
	void sendEjectNotifications(const LLUUID& group_id, const uuid_vec_t& selected_members);
	bool handleEjectCallback(const LLSD& notification, const LLSD& response);
	void commitEjectMembers(uuid_vec_t& selected_members);

	void onRoleCheck(const LLSD& userdata);
	void handleRoleCheck(const LLUUID& role_id,
						 LLRoleMemberChangeType type);

	static void onBanMember(void* user_data);
	void handleBanMember();
	bool handleBanMemberCallback(const LLSD& notification, const LLSD& response);


	void applyMemberChanges();
	bool addOwnerCB(const LLSD& notification, const LLSD& response);

	void activate() override;
	void deactivate() override;
	void cancel() override;
	bool needsApply(std::string& mesg) override;
	bool apply(std::string& mesg) override;
	void update(LLGroupChange gc) override;
	void updateMembers();

	void draw() override;

	void setGroupID(const LLUUID& id) override;

	void addMemberToList(LLGroupMemberData* data);
	void onNameCache(const LLUUID& update_id, LLGroupMemberData* member, const LLAvatarName& av_name, const LLUUID& av_id);

protected:
	typedef std::map<LLUUID, LLRoleMemberChangeType> role_change_data_map_t;
	typedef std::map<LLUUID, role_change_data_map_t*> member_role_changes_map_t;

	bool matchesSearchFilter(const std::string& fullname);
	
	void onExportMembersToCSV();

	U64  getAgentPowersBasedOnRoleChanges(const LLUUID& agent_id);
	bool getRoleChangeType(const LLUUID& member_id,
						   const LLUUID& role_id,
						   LLRoleMemberChangeType& type);

	LLNameListCtrl*		mMembersList;
	LLScrollListCtrl*	mAssignedRolesList;
	LLScrollListCtrl*	mAllowedActionsList;
	LLButton*           mEjectBtn;
	LLButton*			mBanBtn;

	BOOL mChanged;
	BOOL mPendingMemberUpdate;
	BOOL mHasMatch;

	member_role_changes_map_t mMemberRoleChangeData;
	U32 mNumOwnerAdditions;

	LLGroupMgrGroupData::member_list_t::iterator mMemberProgress;
	typedef std::map<LLUUID, boost::signals2::connection> avatar_name_cache_connection_map_t;
	avatar_name_cache_connection_map_t mAvatarNameCacheConnections;
};


class LLPanelGroupRolesSubTab : public LLPanelGroupSubTab
{
public:
	LLPanelGroupRolesSubTab();
	virtual ~LLPanelGroupRolesSubTab();

	BOOL postBuildSubTab(LLView* root) override;

	void activate() override;
	void deactivate() override;
	bool needsApply(std::string& mesg) override;
	bool apply(std::string& mesg) override;
	void cancel() override;
	bool matchesSearchFilter(std::string rolename, std::string roletitle);
	void update(LLGroupChange gc) override;

	void handleRoleSelect();
	void buildMembersList();

	static void onActionCheck(LLUICtrl*, void*);
	bool addActionCB(const LLSD& notification, const LLSD& response, LLCheckBoxCtrl* check);
	
	static void onPropertiesKey(LLLineEditor*, void*);

	void onDescriptionKeyStroke(LLTextEditor* caller);

	static void onDescriptionCommit(LLUICtrl*, void*);

	void handleMemberVisibilityChange(const LLSD& value);

	static void onCreateRole(void*);
	void handleCreateRole();

	static void onDeleteRole(void*);
	void handleDeleteRole();

	void saveRoleChanges(bool select_saved_role);

	void setGroupID(const LLUUID& id) override;

	BOOL	mFirstOpen;

protected:
	void handleActionCheck(LLUICtrl* ctrl, bool force);
	LLSD createRoleItem(const LLUUID& role_id, std::string name, std::string title, S32 members);

	LLScrollListCtrl* mRolesList;
	LLNameListCtrl* mAssignedMembersList;
	LLScrollListCtrl* mAllowedActionsList;

	LLLineEditor* mRoleName;
	LLLineEditor* mRoleTitle;
	LLTextEditor* mRoleDescription;

	LLCheckBoxCtrl* mMemberVisibleCheck;
	LLButton*       mDeleteRoleButton;
	LLButton*       mCreateRoleButton;

	LLUUID	mSelectedRole;
	BOOL	mHasRoleChange;
	std::string mRemoveEveryoneTxt;
};


class LLPanelGroupActionsSubTab : public LLPanelGroupSubTab
{
public:
	LLPanelGroupActionsSubTab();
	virtual ~LLPanelGroupActionsSubTab();

	BOOL postBuildSubTab(LLView* root) override;


	void activate() override;
	void deactivate() override;
	bool needsApply(std::string& mesg) override;
	bool apply(std::string& mesg) override;
	void update(LLGroupChange gc) override;

	void handleActionSelect();

	void setGroupID(const LLUUID& id) override;
protected:
	LLScrollListCtrl*	mActionList;
	LLScrollListCtrl*	mActionRoles;
	LLNameListCtrl*		mActionMembers;

	LLTextEditor*	mActionDescription;
};

#endif // LL_LLPANELGROUPROLES_H
