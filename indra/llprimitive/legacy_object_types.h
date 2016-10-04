/** 
 * @file legacy_object_types.h
 * @brief Byte codes for basic object and primitive types
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LEGACY_OBJECT_TYPES_H
#define LL_LEGACY_OBJECT_TYPES_H

constexpr	S8		PLAYER			= 'c';
//constexpr	S8		BASIC_SHOT		= 's';
//constexpr	S8		BIG_SHOT		= 'S';
//constexpr	S8		TREE_SHOT		= 'g';
//constexpr	S8		PHYSICAL_BALL	= 'b';

constexpr	S8		TREE		= 'T';
constexpr	S8		TREE_NEW	= 'R';
//constexpr	S8		SPARK		= 'p';
//constexpr	S8		SMOKE		= 'q';
//constexpr	S8		BOX			= 'x';
//constexpr	S8		CYLINDER	= 'y';
//constexpr	S8		CONE		= 'o';
//constexpr	S8		SPHERE		= 'h';
//constexpr S8		BIRD		= 'r';			// ascii 114
//constexpr S8		ATOR		= 'a';
//constexpr S8      ROCK		= 'k';

constexpr	S8		GRASS		= 'd';			
constexpr   S8      PART_SYS    = 'P';

//constexpr	S8		ORACLE		= 'O';
//constexpr	S8		TEXTBUBBLE	= 't';			//  Text bubble to show communication
//constexpr	S8		DEMON		= 'M';			// Maxwell's demon for scarfing legacy_object_types.h
//constexpr	S8		CUBE		= 'f';
//constexpr	S8		LSL_TEST	= 'L';
//constexpr	S8		PRISM			= '1';
//constexpr	S8		PYRAMID			= '2';
//constexpr	S8		TETRAHEDRON		= '3';
//constexpr	S8		HALF_CYLINDER	= '4';
//constexpr	S8		HALF_CONE		= '5';
//constexpr	S8		HALF_SPHERE		= '6';

constexpr	S8		PRIMITIVE_VOLUME = 'v';

// Misc constants 

//constexpr   F32		AVATAR_RADIUS			= 0.5f;
//constexpr   F32		SHOT_RADIUS				= 0.05f;
//constexpr   F32		BIG_SHOT_RADIUS			= 0.05f;
//constexpr   F32		TREE_SIZE				= 5.f;
//constexpr   F32		BALL_SIZE				= 4.f;

//constexpr	F32		SHOT_VELOCITY			= 100.f;
//constexpr	F32		GRENADE_BLAST_RADIUS	= 5.f;

#endif

