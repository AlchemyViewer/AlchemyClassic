/** 
 * @file alscriptdefinitions.h
 * @brief LSL runtime definitions
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

#pragma once

typedef enum e_lscript_runtime_permissions
{
	SCRIPT_PERMISSION_DEBIT,
	SCRIPT_PERMISSION_TAKE_CONTROLS,
	SCRIPT_PERMISSION_REMAP_CONTROLS,
	SCRIPT_PERMISSION_TRIGGER_ANIMATION,
	SCRIPT_PERMISSION_ATTACH,
	SCRIPT_PERMISSION_RELEASE_OWNERSHIP,
	SCRIPT_PERMISSION_CHANGE_LINKS,
	SCRIPT_PERMISSION_CHANGE_JOINTS,
	SCRIPT_PERMISSION_CHANGE_PERMISSIONS,
	SCRIPT_PERMISSION_TRACK_CAMERA,
	SCRIPT_PERMISSION_CONTROL_CAMERA,
	SCRIPT_PERMISSION_TELEPORT,
	SCRIPT_PERMISSION_EXPERIENCE,
	SCRIPT_PERMISSION_SILENT_ESTATE_MANAGEMENT,
	SCRIPT_PERMISSION_OVERRIDE_ANIMATIONS,
	SCRIPT_PERMISSION_RETURN_OBJECTS,
	SCRIPT_PERMISSION_EOF
} LSCRIPTRunTimePermissions;

const U32 LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_EOF] =
{
	(0x1 << 1),	//	SCRIPT_PERMISSION_DEBIT,
	(0x1 << 2),	//	SCRIPT_PERMISSION_TAKE_CONTROLS,
	(0x1 << 3),	//	SCRIPT_PERMISSION_REMAP_CONTROLS,
	(0x1 << 4),	//	SCRIPT_PERMISSION_TRIGGER_ANIMATION,
	(0x1 << 5),	//	SCRIPT_PERMISSION_ATTACH,
	(0x1 << 6),	//	SCRIPT_PERMISSION_RELEASE_OWNERSHIP,
	(0x1 << 7),	//	SCRIPT_PERMISSION_CHANGE_LINKS,
	(0x1 << 8),	//	SCRIPT_PERMISSION_CHANGE_JOINTS,
	(0x1 << 9),	//	SCRIPT_PERMISSION_CHANGE_PERMISSIONS
	(0x1 << 10),//	SCRIPT_PERMISSION_TRACK_CAMERA
	(0x1 << 11),//	SCRIPT_PERMISSION_CONTROL_CAMERA
	(0x1 << 12),//	SCRIPT_PERMISSION_TELEPORT
	(0x1 << 13),//	SCRIPT_PERMISSION_EXPERIENCE,
	(0x1 << 14),//	SCRIPT_PERMISSION_SILENT_ESTATE_MANAGEMENT,
	(0x1 << 15),//	SCRIPT_PERMISSION_OVERRIDE_ANIMATIONS,
	(0x1 << 16),//	SCRIPT_PERMISSION_RETURN_OBJECTS,
};

