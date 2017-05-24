/** 
 * @file llheadrotmotion.h
 * @brief Implementation of LLHeadRotMotion class.
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

#ifndef LL_LLHEADROTMOTION_H
#define LL_LLHEADROTMOTION_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include "llmotion.h"
#include "llframetimer.h"

#define MIN_REQUIRED_PIXEL_AREA_HEAD_ROT 500.f;
#define MIN_REQUIRED_PIXEL_AREA_EYE 25000.f;

//-----------------------------------------------------------------------------
// class LLHeadRotMotion
//-----------------------------------------------------------------------------
class LLHeadRotMotion :
	public LLMotion
{
public:
	// Constructor
	LLHeadRotMotion(const LLUUID &id);

	// Destructor
	virtual ~LLHeadRotMotion();

public:
	//-------------------------------------------------------------------------
	// functions to support MotionController and MotionRegistry
	//-------------------------------------------------------------------------

	// static constructor
	// all subclasses must implement such a function and register it
	static LLMotion *create(const LLUUID &id) { return new LLHeadRotMotion(id); }

public:
	//-------------------------------------------------------------------------
	// animation callbacks to be implemented by subclasses
	//-------------------------------------------------------------------------

	// motions must specify whether or not they loop
	BOOL getLoop() override { return TRUE; }

	// motions must report their total duration
	F32 getDuration() override { return 0.0; }

	// motions must report their "ease in" duration
	F32 getEaseInDuration() override { return 1.f; }

	// motions must report their "ease out" duration.
	F32 getEaseOutDuration() override { return 1.f; }

	// called to determine when a motion should be activated/deactivated based on avatar pixel coverage
	F32 getMinPixelArea() override
	{ return MIN_REQUIRED_PIXEL_AREA_HEAD_ROT; }

	// motions must report their priority
	LLJoint::JointPriority getPriority() override { return LLJoint::MEDIUM_PRIORITY; }

	LLMotionBlendType getBlendType() override { return NORMAL_BLEND; }

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

public:
	//-------------------------------------------------------------------------
	// joint states to be animated
	//-------------------------------------------------------------------------
	LLCharacter			*mCharacter;

	LLJoint				*mTorsoJoint;
	LLJoint				*mHeadJoint;
	LLJoint				*mRootJoint;
	LLJoint				*mPelvisJoint;

	LLPointer<LLJointState> mTorsoState;
	LLPointer<LLJointState> mNeckState;
	LLPointer<LLJointState> mHeadState;

	LLQuaternion		mLastHeadRot;
};

//-----------------------------------------------------------------------------
// class LLEyeMotion
//-----------------------------------------------------------------------------
class LLEyeMotion :
	public LLMotion
{
public:
	// Constructor
	LLEyeMotion(const LLUUID &id);

	// Destructor
	virtual ~LLEyeMotion();

public:
	//-------------------------------------------------------------------------
	// functions to support MotionController and MotionRegistry
	//-------------------------------------------------------------------------

	// static constructor
	// all subclasses must implement such a function and register it
	static LLMotion *create( const LLUUID &id) { return new LLEyeMotion(id); }

public:
	//-------------------------------------------------------------------------
	// animation callbacks to be implemented by subclasses
	//-------------------------------------------------------------------------

	// motions must specify whether or not they loop
	BOOL getLoop() override { return TRUE; }

	// motions must report their total duration
	F32 getDuration() override { return 0.0; }

	// motions must report their "ease in" duration
	F32 getEaseInDuration() override { return 0.5f; }

	// motions must report their "ease out" duration.
	F32 getEaseOutDuration() override { return 0.5f; }

	// called to determine when a motion should be activated/deactivated based on avatar pixel coverage
	F32 getMinPixelArea() override
	{ return MIN_REQUIRED_PIXEL_AREA_EYE; }

	// motions must report their priority
	LLJoint::JointPriority getPriority() override { return LLJoint::MEDIUM_PRIORITY; }

	LLMotionBlendType getBlendType() override { return NORMAL_BLEND; }

	// run-time (post constructor) initialization,
	// called after parameters have been set
	// must return true to indicate success and be available for activation
	LLMotionInitStatus onInitialize(LLCharacter *character) override;

	// called when a motion is activated
	// must return TRUE to indicate success, or else
	// it will be deactivated
	BOOL onActivate() override;

    void adjustEyeTarget(LLVector3* targetPos, LLJointState& left_eye_state, LLJointState& right_eye_state);

	// called per time step
	// must return TRUE while it is active, and
	// must return FALSE when the motion is completed.
	BOOL onUpdate(F32 time, U8* joint_mask) override;

	// called when a motion is deactivated
	void onDeactivate() override;

public:
	//-------------------------------------------------------------------------
	// joint states to be animated
	//-------------------------------------------------------------------------
	LLCharacter			*mCharacter;

	LLJoint				*mHeadJoint;
	LLPointer<LLJointState> mLeftEyeState;
	LLPointer<LLJointState> mRightEyeState;
	LLPointer<LLJointState> mAltLeftEyeState;
	LLPointer<LLJointState> mAltRightEyeState;

	LLFrameTimer		mEyeJitterTimer;
	F32					mEyeJitterTime;
	F32					mEyeJitterYaw;
	F32					mEyeJitterPitch;
	F32					mEyeLookAwayTime;
	F32					mEyeLookAwayYaw;
	F32					mEyeLookAwayPitch;

	// eye blinking
	LLFrameTimer		mEyeBlinkTimer;
	F32					mEyeBlinkTime;
	BOOL				mEyesClosed;
};

#endif // LL_LLHEADROTMOTION_H

