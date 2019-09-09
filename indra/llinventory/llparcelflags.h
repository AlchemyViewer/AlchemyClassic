/** 
 * @file llparcelflags.h
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

#ifndef LL_LLPARCEL_FLAGS_H
#define LL_LLPARCEL_FLAGS_H

//---------------------------------------------------------------------------
// Parcel Flags (PF) constants
//---------------------------------------------------------------------------
enum ParcelFlags : U32
{
	PF_ALLOW_FLY = 1 << 0, // Can start flying
	PF_ALLOW_OTHER_SCRIPTS = 1U << 1U,// Scripts by others can run.
	PF_FOR_SALE = 1U << 2U,// Can buy this land
	PF_FOR_SALE_OBJECTS = 1U << 7U,// Can buy all objects on this land
	PF_ALLOW_LANDMARK = 1U << 3U,
	PF_ALLOW_TERRAFORM = 1U << 4U,
	PF_ALLOW_DAMAGE = 1U << 5U,
	PF_CREATE_OBJECTS = 1U << 6U,
	// 7 is moved above
	PF_USE_ACCESS_GROUP = 1U << 8U,
	PF_USE_ACCESS_LIST = 1U << 9U,
	PF_USE_BAN_LIST = 1U << 10U,
	PF_USE_PASS_LIST = 1U << 11U,
	PF_SHOW_DIRECTORY = 1U << 12U,
	PF_ALLOW_DEED_TO_GROUP = 1U << 13U,
	PF_CONTRIBUTE_WITH_DEED = 1U << 14U,
	PF_SOUND_LOCAL = 1U << 15U,	// Hear sounds in this parcel only
	PF_SELL_PARCEL_OBJECTS = 1U << 16U,	// Objects on land are included as part of the land when the land is sold
	PF_ALLOW_PUBLISH = 1U << 17U,	// Allow publishing of parcel information on the web
	PF_MATURE_PUBLISH = 1U << 18U,	// The information on this parcel is mature
	PF_URL_WEB_PAGE = 1U << 19U,	// The "media URL" is an HTML page
	PF_URL_RAW_HTML = 1U << 20U,	// The "media URL" is a raw HTML string like <H1>Foo</H1>
	PF_RESTRICT_PUSHOBJECT = 1U << 21U,	// Restrict push object to either on agent or on scripts owned by parcel owner
	PF_DENY_ANONYMOUS = 1U << 22U,	// Deny all non identified/transacted accounts
	//  PF_DENY_IDENTIFIED			= 1U << 23U,	// Deny identified accounts
	//  PF_DENY_TRANSACTED			= 1U << 24U,	// Deny identified accounts
	PF_ALLOW_GROUP_SCRIPTS = 1U << 25U,	// Allow scripts owned by group
	PF_CREATE_GROUP_OBJECTS = 1U << 26U,	// Allow object creation by group members or objects
	PF_ALLOW_ALL_OBJECT_ENTRY = 1U << 27U,	// Allow all objects to enter a parcel
	PF_ALLOW_GROUP_OBJECT_ENTRY = 1U << 28U,	// Only allow group (and owner) objects to enter the parcel
	PF_ALLOW_VOICE_CHAT = 1U << 29U,	// Allow residents to use voice chat on this parcel
	PF_USE_ESTATE_VOICE_CHAN = 1U << 30U,
	PF_DENY_AGEUNVERIFIED = 1U << 31U,  // Prevent residents who aren't age-verified 
	// NOTE: At one point we have used all of the bits.
	// We have deprecated two of them in 1.19.0 which *could* be reused,
	// but only after we are certain there are no simstates using those bits.
	// PF_RESERVED			= 1U << 31,
	// If any of these are true the parcel is restricting access in some maner.
	PF_USE_RESTRICTED_ACCESS = PF_USE_ACCESS_GROUP | PF_USE_ACCESS_LIST | PF_USE_BAN_LIST
							| PF_USE_PASS_LIST | PF_DENY_ANONYMOUS | PF_DENY_AGEUNVERIFIED,
	PF_NONE = 0x00000000,
	PF_ALL = 0xFFFFFFFF,
	PF_DEFAULT = PF_ALLOW_FLY | PF_ALLOW_OTHER_SCRIPTS | PF_ALLOW_GROUP_SCRIPTS
			  | PF_ALLOW_LANDMARK | PF_CREATE_OBJECTS
			  | PF_CREATE_GROUP_OBJECTS | PF_USE_BAN_LIST | PF_ALLOW_ALL_OBJECT_ENTRY
			  | PF_ALLOW_GROUP_OBJECT_ENTRY | PF_ALLOW_VOICE_CHAT | PF_USE_ESTATE_VOICE_CHAN
};

// Access list flags
const U32 AL_ACCESS				= (1 << 0);
const U32 AL_BAN					= (1 << 1);
const U32 AL_ALLOW_EXPERIENCE	= (1 << 3);
const U32 AL_BLOCK_EXPERIENCE	= (1 << 4);
//const U32 AL_RENTER  = (1 << 2);

// Block access return values. BA_ALLOWED is the only success case
// since some code in the simulator relies on that assumption. All
// other BA_ values should be reasons why you are not allowed.
const S32 BA_ALLOWED = 0;
const S32 BA_NOT_IN_GROUP = 1;
const S32 BA_NOT_ON_LIST = 2;
const S32 BA_BANNED = 3;
const S32 BA_NO_ACCESS_LEVEL = 4;
const S32 BA_NOT_AGE_VERIFIED = 5;

// ParcelRelease flags
const U32 PR_NONE		= 0x0;
const U32 PR_GOD_FORCE	= (1 << 0);

enum EObjectCategory
{
	OC_INVALID = -1,
	OC_NONE = 0,
	OC_TOTAL = 0,	// yes zero, like OC_NONE
	OC_OWNER,
	OC_GROUP,
	OC_OTHER,
	OC_SELECTED,
	OC_TEMP,
	OC_COUNT
};

const S32 PARCEL_DETAILS_NAME = 0;
const S32 PARCEL_DETAILS_DESC = 1;
const S32 PARCEL_DETAILS_OWNER = 2;
const S32 PARCEL_DETAILS_GROUP = 3;
const S32 PARCEL_DETAILS_AREA = 4;
const S32 PARCEL_DETAILS_ID = 5;
const S32 PARCEL_DETAILS_SEE_AVATARS = 6;

#endif
