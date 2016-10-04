/** 
 * @file roles_constants.h
 * @brief General Roles Constants
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

#ifndef LL_ROLES_CONSTANTS_H
#define LL_ROLES_CONSTANTS_H

// This value includes the everyone group.
constexpr S32 MAX_ROLES = 10;

enum class LLRoleMemberChangeType
{
	RMC_ADD,
	RMC_REMOVE,
	RMC_NONE
};

enum class LLRoleChangeType
{
	RC_UPDATE_NONE,
	RC_UPDATE_DATA,
	RC_UPDATE_POWERS,
	RC_UPDATE_ALL,
	RC_CREATE,
	RC_DELETE
};

//
// Powers
//

// KNOWN HOLES: use these for any single bit powers you need
// bit 0x1 << 46
// bit 0x1 << 52 and above

// These powers were removed to make group roles simpler
// bit 0x1 << 41 (GP_ACCOUNTING_VIEW)
// bit 0x1 << 46 (GP_PROPOSAL_VIEW)

constexpr U64 GP_NO_POWERS = 0x0;
constexpr U64 GP_ALL_POWERS = 0xFFFFffffFFFFffffLL;

// Membership
constexpr U64 GP_MEMBER_INVITE				= 0x1LL << 1;	// Invite member
constexpr U64 GP_MEMBER_EJECT				= 0x1LL << 2;	// Eject member from group
constexpr U64 GP_MEMBER_OPTIONS				= 0x1LL << 3;	// Toggle "Open enrollment" and change "Signup Fee"
constexpr U64 GP_MEMBER_VISIBLE_IN_DIR		= 0x1LL << 47;

// Roles
constexpr U64 GP_ROLE_CREATE				= 0x1LL << 4;	// Create new roles
constexpr U64 GP_ROLE_DELETE				= 0x1LL << 5;	// Delete roles
constexpr U64 GP_ROLE_PROPERTIES			= 0x1LL << 6;	// Change Role Names, Titles, and Descriptions (Of roles the user is in, only, or any role in group?)
constexpr U64 GP_ROLE_ASSIGN_MEMBER_LIMITED	= 0x1LL << 7;	// Assign Member to a Role that the assigner is in
constexpr U64 GP_ROLE_ASSIGN_MEMBER			= 0x1LL << 8;	// Assign Member to Role
constexpr U64 GP_ROLE_REMOVE_MEMBER			= 0x1LL << 9;	// Remove Member from Role
constexpr U64 GP_ROLE_CHANGE_ACTIONS		= 0x1LL << 10;	// Change actions a role can perform

// Group Identity
constexpr U64 GP_GROUP_CHANGE_IDENTITY		= 0x1LL << 11;	// Charter, insignia, 'Show In Group List', 'Publish on the web', 'Mature', all 'Show Member In Group Profile' checkboxes

// Parcel Management
constexpr U64 GP_LAND_DEED					= 0x1LL << 12;	// Deed Land and Buy Land for Group
constexpr U64 GP_LAND_RELEASE				= 0x1LL << 13;	// Release Land (to Gov. Linden)
constexpr U64 GP_LAND_SET_SALE_INFO			= 0x1LL << 14;	// Set for sale info (Toggle "For Sale", Set Price, Set Target, Toggle "Sell objects with the land")
constexpr U64 GP_LAND_DIVIDE_JOIN			= 0x1LL << 15;	// Divide and Join Parcels

// Parcel Identity
constexpr U64 GP_LAND_FIND_PLACES			= 0x1LL << 17;	// Toggle "Show in Find Places" and Set Category.
constexpr U64 GP_LAND_CHANGE_IDENTITY		= 0x1LL << 18;	// Change Parcel Identity: Parcel Name, Parcel Description, Snapshot, 'Publish on the web', and 'Mature' checkbox
constexpr U64 GP_LAND_SET_LANDING_POINT		= 0x1LL << 19;	// Set Landing Point

// Parcel Settings
constexpr U64 GP_LAND_CHANGE_MEDIA			= 0x1LL << 20;	// Change Media Settings
constexpr U64 GP_LAND_EDIT					= 0x1LL << 21;	// Toggle Edit Land
constexpr U64 GP_LAND_OPTIONS				= 0x1LL << 22;	// Toggle Set Home Point, Fly, Outside Scripts, Create/Edit Objects, Landmark, and Damage checkboxes

// Parcel Powers
constexpr U64 GP_LAND_ALLOW_EDIT_LAND		= 0x1LL << 23;	// Bypass Edit Land Restriction
constexpr U64 GP_LAND_ALLOW_FLY				= 0x1LL << 24;	// Bypass Fly Restriction
constexpr U64 GP_LAND_ALLOW_CREATE			= 0x1LL << 25;	// Bypass Create/Edit Objects Restriction
constexpr U64 GP_LAND_ALLOW_LANDMARK		= 0x1LL << 26;	// Bypass Landmark Restriction
constexpr U64 GP_LAND_ALLOW_SET_HOME		= 0x1LL << 28;	// Bypass Set Home Point Restriction
constexpr U64 GP_LAND_ALLOW_HOLD_EVENT		= 0x1LL << 41;	// Allowed to hold events on group-owned land

// Parcel Access
constexpr U64 GP_LAND_MANAGE_ALLOWED		= 0x1LL << 29;	// Manage Allowed List
constexpr U64 GP_LAND_MANAGE_BANNED			= 0x1LL << 30;	// Manage Banned List
constexpr U64 GP_LAND_MANAGE_PASSES			= 0x1LL << 31;	// Change Sell Pass Settings
constexpr U64 GP_LAND_ADMIN					= 0x1LL << 32;	// Eject and Freeze Users on the land

// Parcel Content
constexpr U64 GP_LAND_RETURN_GROUP_SET		= 0x1LL << 33;	// Return objects on parcel that are set to group
constexpr U64 GP_LAND_RETURN_NON_GROUP		= 0x1LL << 34;	// Return objects on parcel that are not set to group
constexpr U64 GP_LAND_RETURN_GROUP_OWNED	= 0x1LL << 48;	// Return objects on parcel that are owned by the group

// Select a power-bit based on an object's relationship to a parcel.
constexpr U64 GP_LAND_RETURN		= GP_LAND_RETURN_GROUP_OWNED 
								| GP_LAND_RETURN_GROUP_SET	
								| GP_LAND_RETURN_NON_GROUP;

constexpr U64 GP_LAND_GARDENING				= 0x1LL << 35;	// Parcel Gardening - plant and move linden trees

// Object Management
constexpr U64 GP_OBJECT_DEED				= 0x1LL << 36;	// Deed Object
constexpr U64 GP_OBJECT_MANIPULATE			= 0x1LL << 38;	// Manipulate Group Owned Objects (Move, Copy, Mod)
constexpr U64 GP_OBJECT_SET_SALE			= 0x1LL << 39;	// Set Group Owned Object for Sale

// Accounting
constexpr U64 GP_ACCOUNTING_ACCOUNTABLE		= 0x1LL << 40;	// Pay Group Liabilities and Receive Group Dividends

// Notices
constexpr U64 GP_NOTICES_SEND				= 0x1LL << 42;	// Send Notices
constexpr U64 GP_NOTICES_RECEIVE			= 0x1LL << 43;	// Receive Notices and View Notice History

// Proposals
// TODO: _DEPRECATED suffix as part of vote removal - DEV-24856:
constexpr U64 GP_PROPOSAL_START				= 0x1LL << 44;	// Start Proposal
// TODO: _DEPRECATED suffix as part of vote removal - DEV-24856:
constexpr U64 GP_PROPOSAL_VOTE				= 0x1LL << 45;	// Vote on Proposal

// Group chat moderation related
constexpr U64 GP_SESSION_JOIN				= 0x1LL << 16;	//can join session
constexpr U64 GP_SESSION_VOICE				= 0x1LL << 27;	//can hear/talk
constexpr U64 GP_SESSION_MODERATOR			= 0x1LL << 37;	//can mute people's session

constexpr U64 GP_EXPERIENCE_ADMIN			= 0x1LL << 49;	// has admin rights to any experiences owned by this group
constexpr U64 GP_EXPERIENCE_CREATOR 		= 0x1LL << 50;	// can sign scripts for experiences owned by this group

// Group Banning
constexpr U64 GP_GROUP_BAN_ACCESS			= 0x1LL << 51;	// Allows access to ban / un-ban agents from a group.

constexpr U64 GP_DEFAULT_MEMBER = GP_ACCOUNTING_ACCOUNTABLE
                                | GP_LAND_ALLOW_SET_HOME
								| GP_NOTICES_RECEIVE
								| GP_SESSION_JOIN
								| GP_SESSION_VOICE
								;

constexpr U64 GP_DEFAULT_OFFICER = GP_DEFAULT_MEMBER // Superset of GP_DEFAULT_MEMBER
								| GP_GROUP_CHANGE_IDENTITY
								| GP_LAND_ADMIN
								| GP_LAND_ALLOW_EDIT_LAND
								| GP_LAND_ALLOW_FLY
								| GP_LAND_ALLOW_CREATE
								| GP_LAND_ALLOW_LANDMARK
								| GP_LAND_CHANGE_IDENTITY
								| GP_LAND_CHANGE_MEDIA
								| GP_LAND_DEED
								| GP_LAND_DIVIDE_JOIN
								| GP_LAND_EDIT
								| GP_LAND_FIND_PLACES
								| GP_LAND_GARDENING
								| GP_LAND_MANAGE_ALLOWED
								| GP_LAND_MANAGE_BANNED
								| GP_LAND_MANAGE_PASSES
								| GP_LAND_OPTIONS
								| GP_LAND_RELEASE
								| GP_LAND_RETURN_GROUP_OWNED
								| GP_LAND_RETURN_GROUP_SET
								| GP_LAND_RETURN_NON_GROUP
								| GP_LAND_SET_LANDING_POINT
								| GP_LAND_SET_SALE_INFO
								| GP_MEMBER_EJECT
								| GP_MEMBER_INVITE	
								| GP_MEMBER_OPTIONS
								| GP_MEMBER_VISIBLE_IN_DIR
								| GP_NOTICES_SEND
								| GP_OBJECT_DEED
								| GP_OBJECT_MANIPULATE
								| GP_OBJECT_SET_SALE
								| GP_ROLE_ASSIGN_MEMBER_LIMITED
								| GP_ROLE_PROPERTIES
								| GP_SESSION_MODERATOR
								;
#endif
