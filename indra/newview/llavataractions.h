/** 
 * @file llavataractions.h
 * @brief Friend-related actions (add, remove, offer teleport, etc)
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

#ifndef LL_LLAVATARACTIONS_H
#define LL_LLAVATARACTIONS_H

#include "llsd.h"
#include "lluuid.h"

class LLAvatarName;
class LLInventoryPanel;
class LLFloater;
class LLView;

/**
 * Friend-related actions (add, remove, offer teleport, etc)
 */
class LLAvatarActions
{
public:
	/**
	 * Show a dialog explaining what friendship entails, then request friendship.
	 */
	static void requestFriendshipDialog(const LLUUID& id, const std::string& name);

	/**
	 * Show a dialog explaining what friendship entails, then request friendship.
	 */
	static void requestFriendshipDialog(const LLUUID& id);

	/**
	 * Show a friend removal dialog.
	 */
	static void removeFriendDialog(const LLUUID& id);
	static void removeFriendsDialog(const uuid_vec_t& ids);
	
	/**
	 * Show teleport offer dialog.
	 */
	static void offerTeleport(const LLUUID& invitee);
	static void offerTeleport(const uuid_vec_t& ids);

	/**
	 * Start instant messaging session.
	 */
	static void startIM(const LLUUID& id);

	/**
	 * End instant messaging session.
	 */
	static void endIM(const LLUUID& id);

	/**
	 * Start an avatar-to-avatar voice call with another user
	 */
	static void startCall(const LLUUID& id);

	/**
	 * Start an ad-hoc conference voice call with multiple users in a specific IM floater.
	 */
	static void startAdhocCall(const uuid_vec_t& ids, const LLUUID& floater_id = LLUUID::null);

	/**
	 * Start conference chat with the given avatars in a specific IM floater.
	 */
	static void startConference(const uuid_vec_t& ids, const LLUUID& floater_id = LLUUID::null);

	/**
	 * Show avatar profile.
	 */
	static void showProfile(const LLUUID& id);
	static void showWebProfile(const LLUUID& id);
	static void hideProfile(const LLUUID& id);
	static bool profileVisible(const LLUUID& id);
	static LLFloater* getProfileFloater(const LLUUID& id);

	/**
	 * Show avatar on world map.
	 */
	static void showOnMap(const LLUUID& id);

	/**
	 * Give money to the avatar.
	 */
	static void pay(const LLUUID& id);

	/**
	 * Request teleport from other avatar
	 */
	static void teleportRequest(const LLUUID& id);
	static void teleport_request_callback(const LLSD& notification, const LLSD& response);

	/**
	 * Share items with the avatar.
	 */
	static void share(const LLUUID& id);

	/**
	 * Share items with the picked avatars.
	 */
	static void shareWithAvatars(LLView * panel);

	/**
	 * Block/unblock the avatar.
	 */
	static void toggleBlock(const LLUUID& id);

	/**
	 * Block/unblock the avatar voice.
	 */
	static void toggleMuteVoice(const LLUUID& id);

	/**
	 * Return true if avatar with "id" is a friend
	 */
	static bool isFriend(const LLUUID& id);

	/**
	 * @return true if the avatar is blocked
	 */
	static bool isBlocked(const LLUUID& id);

	/**
	 * @return true if the avatar voice is blocked
	 */
	static bool isVoiceMuted(const LLUUID& id);

	/**
	 * @return true if you can block the avatar
	 */
	static bool canBlock(const LLUUID& id);

	/**
	 * Return true if the avatar is in a P2P voice call with a given user
	 */
	/* AD *TODO: Is this function needed any more?
		I fixed it a bit(added check for canCall), but it appears that it is not used
		anywhere. Maybe it should be removed?
	static bool isCalling(const LLUUID &id);*/

	/**
	 * @return true if call to the resident can be made
	 */

	static bool canCall();
	/**
	 * Invite avatar to a group.
	 */	
	static void inviteToGroup(const LLUUID& id);
	
	/**
	 * Kick avatar off grid
	 */	
	static void godKick(const LLUUID& id);

	/**
	 * Freeze avatar
	 */	
	static void godFreeze(const LLUUID& id);

	/**
	 * Unfreeze avatar
	 */	
	static void godUnfreeze(const LLUUID& id);

	/**
	 * Open csr page for avatar
	 */	
	static void csr(const LLUUID& id, std::string name);
	
	/**
	 * Zooms in to the avatar
	 */
	static void zoomIn(const LLUUID& id);

	/**
	 * Checks whether we can offer a teleport to the avatar, only offline friends
	 * cannot be offered a teleport.
	 *
	 * @return false if avatar is a friend and not visibly online
	 */
	static bool canOfferTeleport(const LLUUID& id);

	/**
	 * @return false if any one of the specified avatars a friend and not visibly online
	 */
	static bool canOfferTeleport(const uuid_vec_t& ids);

	/**
	 * Checks whether all items selected in the given inventory panel can be shared
	 *
	 * @param inv_panel Inventory panel to get selection from. If NULL, the active inventory panel is used.
	 *
	 * @return false if the selected items cannot be shared or the active inventory panel cannot be obtained
	 */
	static bool canShareSelectedItems(LLInventoryPanel* inv_panel = nullptr);

	/**
	 * Checks whether agent is mappable
	 */
	static bool isAgentMappable(const LLUUID& agent_id);
	
	/**
	 * Builds a string of residents' display names separated by "words_separator" string.
	 *
	 * @param avatar_names - a vector of given avatar names from which resulting string is built
	 * @param residents_string - the resulting string
	 */
	static void buildResidentsString(std::vector<LLAvatarName> avatar_names, std::string& residents_string, bool complete_name = false);

	/**
	 * Builds a string of residents' display names separated by "words_separator" string.
	 *
	 * @param avatar_uuids - a vector of given avatar uuids from which resulting string is built
	 * @param residents_string - the resulting string
	 */
	static void buildResidentsString(const uuid_vec_t& avatar_uuids, std::string& residents_string);

	/**
	 * Opens the chat history for avatar
	 */
	static void viewChatHistory(const LLUUID& id);

	static std::set<LLUUID> getInventorySelectedUUIDs();

	/**
     * Copy the selected avatar's name, slurl, or UUID to clipboard
	 */
	enum ECopyDataType{
		E_DATA_NAME = 0,
		E_DATA_SLURL,
		E_DATA_UUID
	};
	static void copyData(const LLUUID& id, ECopyDataType type);
	static void copyData(const uuid_vec_t& ids, ECopyDataType type);

	static bool canTeleportTo(const LLUUID& avatar_id);
	static void teleportTo(const LLUUID& avatar_id);
	
	static bool canFreezeEject(const LLUUID& avatar_id);
	static bool canFreezeEject(const uuid_vec_t& ids);
	static void parcelFreeze(const LLUUID& avatar_id);
	static void parcelFreeze(const uuid_vec_t& ids);
	static void parcelEject(const LLUUID& avatar_id);
	static void parcelEject(const uuid_vec_t& ids);

	static bool canManageAvatarsEstate(const LLUUID& avatar_id);
	static bool canManageAvatarsEstate(const uuid_vec_t& ids);
	static void estateTeleportHome(const LLUUID& avatar_id);
	static void estateTeleportHome(const uuid_vec_t& ids);
	static void estateKick(const LLUUID& avatar_id);
	static void estateKick(const uuid_vec_t& ids);
	static void estateBan(const LLUUID& avatar_id);
	static void estateBan(const uuid_vec_t& ids);

private:
	static bool callbackAddFriendWithMessage(const LLSD& notification, const LLSD& response);
	static bool handleRemove(const LLSD& notification, const LLSD& response);
	static bool handlePay(const LLSD& notification, const LLSD& response, LLUUID avatar_id);
	static bool handleGodKick(const LLSD& notification, const LLSD& response);
	static bool handleParcelFreeze(const LLSD& notification, const LLSD& response);
	static bool handleParcelEject(const LLSD& notification, const LLSD& response);
	static bool handleEstateTeleportHome(const LLSD& notification, const LLSD& response);
	static bool handleEstateKick(const LLSD& notification, const LLSD& response);
	static bool handleEstateBan(const LLSD& notification, const LLSD& response);
	static void callback_invite_to_group(LLUUID group_id, LLUUID id);

	// Just request friendship, no dialog.
	static void requestFriendship(const LLUUID& target_id, const std::string& target_name, const std::string& message);
};

#endif // LL_LLAVATARACTIONS_H
