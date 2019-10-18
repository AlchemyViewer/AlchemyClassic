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
#include "lljointstate.h"
#include "llmotion.h"
#include "llframetimer.h"

#include "llviewervisualparam.h"

#define PHYSICS_MOTION_FADEIN_TIME 1.0f
#define PHYSICS_MOTION_FADEOUT_TIME 1.0f

//-----------------------------------------------------------------------------
// class LLPhysicsMotion
//-----------------------------------------------------------------------------
/*
   At a high level, this works by setting temporary parameters that are not stored
   in the avatar's list of params, and are not conveyed to other users.  We accomplish
   this by creating some new temporary driven params inside avatar_lad that are then driven
   by the actual params that the user sees and sets.  For example, in the old system,
   the user sets a param called breast bouyancy, which controls the Z value of the breasts.
   In our new system, the user still sets the breast bouyancy, but that param is redefined
   as a driver param so that affects a new temporary driven param that the bounce is applied
   to.
*/


class LLPhysicsMotion
{
public:
	typedef std::map<std::string, std::string> controller_map_t;
	typedef std::map<std::string, F32> default_controller_map_t;

	typedef enum
	{
		SMOOTHING = 0,
		MASS,
		GRAVITY,
		SPRING,
		GAIN,
		DAMPING,
		DRAG,
		MAX_EFFECT,
		NUM_PARAMS
	} eParamName;

	/*
	  param_driver_name: The param that controls the params that are being affected by the physics.
	  joint_name: The joint that the body part is attached to.  The joint is
	  used to determine the orientation (rotation) of the body part.

	  character: The avatar that this physics affects.

	  motion_direction_vec: The direction (in world coordinates) that determines the
	  motion.  For example, (0,0,1) is up-down, and means that up-down motion is what
	  determines how this joint moves.

	  controllers: The various settings (e.g. spring force, mass) that determine how
	  the body part behaves.
	*/
	LLPhysicsMotion(std::string param_driver_name,
		std::string joint_name,
		LLCharacter* character,
		const LLVector3& motion_direction_vec,
		controller_map_t controllers);

	BOOL initialize();

	~LLPhysicsMotion() = default;

	BOOL onUpdate(F32 time);

	LLPointer<LLJointState> getJointState()
	{
		return mJointState;
	}
protected:

	F32 getParamValue(eParamName param);
	void setParamValue(const LLViewerVisualParam* param,
		const F32 new_value_local,
		F32 behavior_maxeffect);

	F32 toLocal(const LLVector3& world);
	F32 calculateVelocity_local(const F32 time_delta);
	F32 calculateAcceleration_local(F32 velocity_local, const F32 time_delta);
private:
	const std::string mParamDriverName;
	const std::string mParamControllerName;
	const LLVector3 mMotionDirectionVec;
	const std::string mJointName;

	F32 mPosition_local;
	F32 mVelocityJoint_local; // How fast the joint is moving
	F32 mAccelerationJoint_local; // Acceleration on the joint

	F32 mVelocity_local; // How fast the param is moving
	F32 mPositionLastUpdate_local;
	LLVector3 mPosition_world;

	LLViewerVisualParam* mParamDriver;
	const controller_map_t mParamControllers;

	LLPointer<LLJointState> mJointState;
	LLCharacter* mCharacter;

	F32 mLastTime;

	LLVisualParam* mParamCache[NUM_PARAMS];

	static default_controller_map_t sDefaultController;
};

//-----------------------------------------------------------------------------
// class LLPhysicsMotionController
//-----------------------------------------------------------------------------
class LLPhysicsMotionController final :
	public LLMotion
{
public:
	// Constructor
	LLPhysicsMotionController(const LLUUID &id);

	// Destructor
	virtual ~LLPhysicsMotionController() = default;

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
	void addMotion(std::unique_ptr<LLPhysicsMotion> motion);
private:
	LLCharacter*		mCharacter;

	typedef std::vector<std::unique_ptr<LLPhysicsMotion> > motion_vec_t;
	motion_vec_t mMotions;
};

#endif // LL_LLPHYSICSMOTION_H

