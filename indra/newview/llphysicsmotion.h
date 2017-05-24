/** 
 * @file llphysicsmotion.h
 * @brief Implementation of LLPhysicsMotion class.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

#ifndef LL_LLPHYSICSMOTIONCONTROLLER_H
#define LL_LLPHYSICSMOTIONCONTROLLER_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include "llmotion.h"
#include "llframetimer.h"

#define PHYSICS_MOTION_FADEIN_TIME 1.0f
#define PHYSICS_MOTION_FADEOUT_TIME 1.0f

class LLPhysicsMotion;

//-----------------------------------------------------------------------------
// class LLPhysicsMotion
//-----------------------------------------------------------------------------
class LLPhysicsMotionController :
	public LLMotion
{
public:
	// Constructor
	LLPhysicsMotionController(const LLUUID &id);

	// Destructor
	virtual ~LLPhysicsMotionController();

public:
	//-------------------------------------------------------------------------
	// functions to support MotionController and MotionRegistry
	//-------------------------------------------------------------------------

	// static constructor
	// all subclasses must implement such a function and register it
	static LLMotion *create(const LLUUID &id) { return new LLPhysicsMotionController(id); }

public:
	//-------------------------------------------------------------------------
	// animation callbacks to be implemented by subclasses
	//-------------------------------------------------------------------------

	// motions must specify whether or not they loop
	BOOL getLoop() override { return TRUE; }

	// motions must report their total duration
	F32 getDuration() override { return 0.0; }

	// motions must report their "ease in" duration
	F32 getEaseInDuration() override { return PHYSICS_MOTION_FADEIN_TIME; }

	// motions must report their "ease out" duration.
	F32 getEaseOutDuration() override { return PHYSICS_MOTION_FADEOUT_TIME; }

	// called to determine when a motion should be activated/deactivated based on avatar pixel coverage
	F32 getMinPixelArea() override;

	// motions must report their priority
	LLJoint::JointPriority getPriority() override { return LLJoint::MEDIUM_PRIORITY; }

	LLMotionBlendType getBlendType() override { return ADDITIVE_BLEND; }

	// run-time (post constructor) initialization,
	// called after parameters have been set
	// must return true to indicate success and be available for activation
	LLMotionInitStatus onInitialize(LLCharacter *character) override;

	// called when a motion is activated
	// must return TRUE to indicate success, or else
	// it will be deactivated
	BOOL onActivate() override;

	// called per time step
	// must return TRUE while it is active, and
	// must return FALSE when the motion is completed.
	BOOL onUpdate(F32 time, U8* joint_mask) override;

	// called when a motion is deactivated
	void onDeactivate() override;

	LLCharacter* getCharacter() { return mCharacter; }

protected:
	void addMotion(LLPhysicsMotion *motion);
private:
	LLCharacter*		mCharacter;

	typedef std::vector<LLPhysicsMotion *> motion_vec_t;
	motion_vec_t mMotions;
};

#endif // LL_LLPHYSICSMOTION_H

