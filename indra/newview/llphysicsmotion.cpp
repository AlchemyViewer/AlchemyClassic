/** 
 * @file llphysicsmotion.cpp
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "llviewerprecompiledheaders.h"
#include "linden_common.h"

#include "llphysicsmotion.h"
#include "llcharacter.h"
#include "llviewercontrol.h"
#include "llviewervisualparam.h"
#include "llvoavatarself.h"
#include <utility>

#define MIN_REQUIRED_PIXEL_AREA_AVATAR_PHYSICS_MOTION 0.f
// we use TIME_ITERATION_STEP_MAX in division operation, make sure this is a simple
// value and devision result won't end with repeated/recurring tail like 1.333(3)
#define TIME_ITERATION_STEP_MAX 0.05f // minimal step size will end up as 0.025

inline F64 llsgn(const F64 a)
{
        if (a >= D_APPROXIMATELY_ZERO)
                return 1;
        return -1;
}

inline F32 llsgn(const F32 a)
{
	if (a >= F_APPROXIMATELY_ZERO)
		return 1.f;
	return -1.f;
}

LLPhysicsMotion::default_controller_map_t initDefaultController()
{
        LLPhysicsMotion::default_controller_map_t controller;
        controller["Mass"] = 0.2f;
        controller["Gravity"] = 0.0f;
        controller["Damping"] = .05f;
        controller["Drag"] = 0.15f;
        controller["MaxEffect"] = 0.1f;
        controller["Spring"] = 0.1f;
        controller["Gain"] = 10.0f;
        return controller;
}

LLPhysicsMotion::default_controller_map_t LLPhysicsMotion::sDefaultController = initDefaultController();

LLPhysicsMotion::LLPhysicsMotion(std::string param_driver_name,
	std::string joint_name,
	LLCharacter* character,
	const LLVector3& motion_direction_vec,
	controller_map_t controllers) :
	mParamDriverName(std::move(param_driver_name)),
	mMotionDirectionVec(motion_direction_vec),
	mJointName(std::move(joint_name)),
	mPosition_local(0),
	mVelocityJoint_local(0),
	mPositionLastUpdate_local(0),
	mParamDriver(nullptr),
	mParamControllers(std::move(controllers)),
	mCharacter(character),
	mLastTime(0)
{
	mJointState = new LLJointState;

	for (auto& i : mParamCache)
	{
		i = nullptr;
	}
}

BOOL LLPhysicsMotion::initialize()
{
        if (!mJointState->setJoint(mCharacter->getJoint(mJointName.c_str())))
                return FALSE;
        mJointState->setUsage(LLJointState::ROT);

        mParamDriver = (LLViewerVisualParam*)mCharacter->getVisualParam(mParamDriverName.c_str());
        if (mParamDriver == nullptr)
        {
                LL_INFOS() << "Failure reading in  [ " << mParamDriverName << " ]" << LL_ENDL;
                return FALSE;
        }

        return TRUE;
}

LLPhysicsMotionController::LLPhysicsMotionController(const LLUUID &id) : 
        LLMotion(id),
        mCharacter(nullptr)
{
        mName = "breast_motion";
}

BOOL LLPhysicsMotionController::onActivate() 
{ 
        return TRUE; 
}

void LLPhysicsMotionController::onDeactivate() 
{
}

LLMotion::LLMotionInitStatus LLPhysicsMotionController::onInitialize(LLCharacter* character)
{
	if (!character)
		return STATUS_FAILURE;

	mCharacter = character;

	mMotions.clear();

	// Breast Cleavage
	{
		LLPhysicsMotion::controller_map_t controller;
		controller["Mass"] = "Breast_Physics_Mass";
		controller["Gravity"] = "Breast_Physics_Gravity";
		controller["Drag"] = "Breast_Physics_Drag";
		controller["Damping"] = "Breast_Physics_InOut_Damping";
		controller["MaxEffect"] = "Breast_Physics_InOut_Max_Effect";
		controller["Spring"] = "Breast_Physics_InOut_Spring";
		controller["Gain"] = "Breast_Physics_InOut_Gain";
		auto motion = std::make_unique<LLPhysicsMotion>("Breast_Physics_InOut_Controller",
			"mChest",
			character,
			LLVector3(-1, 0, 0),
			controller);
		if (!motion->initialize())
		{
			llassert_always(FALSE);
			return STATUS_FAILURE;
		}
		addMotion(std::move(motion));
	}

	// Breast Bounce
	{
		LLPhysicsMotion::controller_map_t controller;
		controller["Mass"] = "Breast_Physics_Mass";
		controller["Gravity"] = "Breast_Physics_Gravity";
		controller["Drag"] = "Breast_Physics_Drag";
		controller["Damping"] = "Breast_Physics_UpDown_Damping";
		controller["MaxEffect"] = "Breast_Physics_UpDown_Max_Effect";
		controller["Spring"] = "Breast_Physics_UpDown_Spring";
		controller["Gain"] = "Breast_Physics_UpDown_Gain";
		auto motion = std::make_unique<LLPhysicsMotion>("Breast_Physics_UpDown_Controller",
			"mChest",
			character,
			LLVector3(0, 0, 1),
			controller);
		if (!motion->initialize())
		{
			llassert_always(FALSE);
			return STATUS_FAILURE;
		}
		addMotion(std::move(motion));
	}

	// Breast Sway
	{
		LLPhysicsMotion::controller_map_t controller;
		controller["Mass"] = "Breast_Physics_Mass";
		controller["Gravity"] = "Breast_Physics_Gravity";
		controller["Drag"] = "Breast_Physics_Drag";
		controller["Damping"] = "Breast_Physics_LeftRight_Damping";
		controller["MaxEffect"] = "Breast_Physics_LeftRight_Max_Effect";
		controller["Spring"] = "Breast_Physics_LeftRight_Spring";
		controller["Gain"] = "Breast_Physics_LeftRight_Gain";
		auto motion = std::make_unique<LLPhysicsMotion>("Breast_Physics_LeftRight_Controller",
			"mChest",
			character,
			LLVector3(0, -1, 0),
			controller);
		if (!motion->initialize())
		{
			llassert_always(FALSE);
			return STATUS_FAILURE;
		}
		addMotion(std::move(motion));
	}
	// Butt Bounce
	{
		LLPhysicsMotion::controller_map_t controller;
		controller["Mass"] = "Butt_Physics_Mass";
		controller["Gravity"] = "Butt_Physics_Gravity";
		controller["Drag"] = "Butt_Physics_Drag";
		controller["Damping"] = "Butt_Physics_UpDown_Damping";
		controller["MaxEffect"] = "Butt_Physics_UpDown_Max_Effect";
		controller["Spring"] = "Butt_Physics_UpDown_Spring";
		controller["Gain"] = "Butt_Physics_UpDown_Gain";
		auto motion = std::make_unique<LLPhysicsMotion>("Butt_Physics_UpDown_Controller",
			"mPelvis",
			character,
			LLVector3(0, 0, -1),
			controller);
		if (!motion->initialize())
		{
			llassert_always(FALSE);
			return STATUS_FAILURE;
		}
		addMotion(std::move(motion));
	}

	// Butt LeftRight
	{
		LLPhysicsMotion::controller_map_t controller;
		controller["Mass"] = "Butt_Physics_Mass";
		controller["Gravity"] = "Butt_Physics_Gravity";
		controller["Drag"] = "Butt_Physics_Drag";
		controller["Damping"] = "Butt_Physics_LeftRight_Damping";
		controller["MaxEffect"] = "Butt_Physics_LeftRight_Max_Effect";
		controller["Spring"] = "Butt_Physics_LeftRight_Spring";
		controller["Gain"] = "Butt_Physics_LeftRight_Gain";
		auto motion = std::make_unique<LLPhysicsMotion>("Butt_Physics_LeftRight_Controller",
			"mPelvis",
			character,
			LLVector3(0, -1, 0),
			controller);
		if (!motion->initialize())
		{
			llassert_always(FALSE);
			return STATUS_FAILURE;
		}
		addMotion(std::move(motion));
	}

	// Belly Bounce
	{
		LLPhysicsMotion::controller_map_t controller;
		controller["Mass"] = "Belly_Physics_Mass";
		controller["Gravity"] = "Belly_Physics_Gravity";
		controller["Drag"] = "Belly_Physics_Drag";
		controller["Damping"] = "Belly_Physics_UpDown_Damping";
		controller["MaxEffect"] = "Belly_Physics_UpDown_Max_Effect";
		controller["Spring"] = "Belly_Physics_UpDown_Spring";
		controller["Gain"] = "Belly_Physics_UpDown_Gain";
		auto motion = std::make_unique<LLPhysicsMotion>("Belly_Physics_UpDown_Controller",
			"mPelvis",
			character,
			LLVector3(0, 0, -1),
			controller);
		if (!motion->initialize())
		{
			llassert_always(FALSE);
			return STATUS_FAILURE;
		}
		addMotion(std::move(motion));
	}

	return STATUS_SUCCESS;
}

void LLPhysicsMotionController::addMotion(std::unique_ptr<LLPhysicsMotion> motion)
{
        addJointState(motion->getJointState());
        mMotions.push_back(std::move(motion));
}

F32 LLPhysicsMotionController::getMinPixelArea() 
{
        return MIN_REQUIRED_PIXEL_AREA_AVATAR_PHYSICS_MOTION;
}

// Local space means "parameter space".
F32 LLPhysicsMotion::toLocal(const LLVector3 &world)
{
        LLJoint *joint = mJointState->getJoint();
        const LLQuaternion rotation_world = joint->getWorldRotation();
        
        LLVector3 dir_world = mMotionDirectionVec * rotation_world;
        dir_world.normalize();
        return world * dir_world;
}

F32 LLPhysicsMotion::calculateVelocity_local(const F32 time_delta)
{
	const F32 world_to_model_scale = 100.0f;
        LLJoint *joint = mJointState->getJoint();
        const LLVector3 position_world = joint->getWorldPosition();
        const LLVector3 last_position_world = mPosition_world;
	const LLVector3 positionchange_world = (position_world-last_position_world) * world_to_model_scale;
        const F32 velocity_local = toLocal(positionchange_world) / time_delta;
        return velocity_local;
}

F32 LLPhysicsMotion::calculateAcceleration_local(const F32 velocity_local, const F32 time_delta)
{
//        const F32 smoothing = getParamValue("Smoothing");
        static const F32 smoothing = 3.0f; // Removed smoothing param since it's probably not necessary
        const F32 acceleration_local = (velocity_local - mVelocityJoint_local) / time_delta;
        
        const F32 smoothed_acceleration_local = 
                acceleration_local * 1.0f/smoothing + 
                mAccelerationJoint_local * (smoothing-1.0f)/smoothing;
        
        return smoothed_acceleration_local;
}

BOOL LLPhysicsMotionController::onUpdate(F32 time, U8* joint_mask)
{
        // Skip if disabled globally.
		static LLCachedControl<bool> avatar_physics(gSavedSettings, "AvatarPhysics");
		if (!avatar_physics)
        {
                return TRUE;
        }
        
        BOOL update_visuals = FALSE;
		for(const auto& motion : mMotions)
        {
                update_visuals |= motion->onUpdate(time);
        }
                
        if (update_visuals)
                mCharacter->updateVisualParams();
        
        return TRUE;
}

// Return TRUE if character has to update visual params.
BOOL LLPhysicsMotion::onUpdate(F32 time)
{
        // static FILE *mFileWrite = fopen("c:\\temp\\avatar_data.txt","w");
        
        if (!mParamDriver)
                return FALSE;

        if (!mLastTime || mLastTime >= time)
        {
                mLastTime = time;
                return FALSE;
        }

        ////////////////////////////////////////////////////////////////////////////////
        // Get all parameters and settings
        //

        const F32 time_delta = time - mLastTime;
	
	// If less than 1FPS, we don't want to be spending time updating physics at all.
        if (time_delta > 1.0f)
        {
                mLastTime = time;
                return FALSE;
        }

        // Higher LOD is better.  This controls the granularity
        // and frequency of updates for the motions.
        const F32 lod_factor = LLVOAvatar::sPhysicsLODFactor;
        if (lod_factor == 0.f)
        {
                return TRUE;
        }

        LLJoint *joint = mJointState->getJoint();

		const F32 behavior_mass = getParamValue(MASS);
		const F32 behavior_gravity = getParamValue(GRAVITY);
		const F32 behavior_spring = getParamValue(SPRING);
		const F32 behavior_gain = getParamValue(GAIN);
		const F32 behavior_damping = getParamValue(DAMPING);
		const F32 behavior_drag = getParamValue(DRAG);
		F32 behavior_maxeffect = getParamValue(MAX_EFFECT);
		
		const BOOL physics_test = FALSE; // Enable this to simulate bouncing on all parts.
        
        if (physics_test)
                behavior_maxeffect = 1.0f;

	// Normalize the param position to be from [0,1].
	// We have to use normalized values because there may be more than one driven param,
	// and each of these driven params may have its own range.
	// This means we'll do all our calculations in normalized [0,1] local coordinates.
	const F32 position_user_local = (mParamDriver->getWeight() - mParamDriver->getMinWeight()) / (mParamDriver->getMaxWeight() - mParamDriver->getMinWeight());
       	
	//
	// End parameters and settings
	////////////////////////////////////////////////////////////////////////////////
	
	
	////////////////////////////////////////////////////////////////////////////////
	// Calculate velocity and acceleration in parameter space.
	//
        
    const F32 joint_local_factor = 30.0f;
    const F32 velocity_joint_local = calculateVelocity_local(time_delta * joint_local_factor);
    const F32 acceleration_joint_local = calculateAcceleration_local(velocity_joint_local, time_delta * joint_local_factor);
	
	//
	// End velocity and acceleration
	////////////////////////////////////////////////////////////////////////////////
	
	BOOL update_visuals = FALSE;
	
	// Break up the physics into a bunch of iterations so that differing framerates will show
	// roughly the same behavior.
	// Explanation/example: Lets assume we have a bouncing object. Said abjects bounces at a
	// trajectory that has points A>B>C. Object bounces from A to B with specific speed.
	// It needs time T to move from A to B.
	// As long as our frame's time significantly smaller then T our motion will be split into
	// multiple parts. with each part speed will decrease. Object will reach B position (roughly)
	// and bounce/fall back to A.
	// But if frame's time (F_T) is larger then T, object will move with same speed for whole F_T
	// and will jump over point B up to C ending up with increased amplitude. To avoid that we
	// split F_T into smaller portions so that when frame's time is too long object can virtually
	// bounce at right (relatively) position.
	// Note: this doesn't look to be optimal, since it provides only "roughly same" behavior, but
	// irregularity at higher fps looks to be insignificant so it works good enough for low fps.
	U32 steps = (U32)(time_delta / TIME_ITERATION_STEP_MAX) + 1;
	F32 time_iteration_step = time_delta / (F32)steps; //minimal step size ends up as 0.025
	for (U32 i = 0; i < steps; i++)
	{
		// mPositon_local should be in normalized 0,1 range already.  Just making sure...
		const F32 position_current_local = llclamp(mPosition_local,
							   0.0f,
							   1.0f);
		// If the effect is turned off then don't process unless we need one more update
		// to set the position to the default (i.e. user) position.
		if ((behavior_maxeffect == 0.f) && (position_current_local == position_user_local))
		{
			return update_visuals;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Calculate the total force 
		//

		// Spring force is a restoring force towards the original user-set breast position.
		// F = kx
		const F32 spring_length = position_current_local - position_user_local;
		const F32 force_spring = -spring_length * behavior_spring;

		// Acceleration is the force that comes from the change in velocity of the torso.
		// F = ma
		const F32 force_accel = behavior_gain * (acceleration_joint_local * behavior_mass);

		// Gravity always points downward in world space.
		// F = mg
		const LLVector3 gravity_world(0.f, 0.f, 1.f);
		const F32 force_gravity = (toLocal(gravity_world) * behavior_gravity * behavior_mass);
                
		// Damping is a restoring force that opposes the current velocity.
		// F = -kv
		const F32 force_damping = -behavior_damping * mVelocity_local;
                
		// Drag is a force imparted by velocity (intuitively it is similar to wind resistance)
		// F = .5kv^2
		const F32 force_drag = 0.5f*behavior_drag*velocity_joint_local*velocity_joint_local*llsgn(velocity_joint_local);

		const F32 force_net = (force_accel + 
				       force_gravity +
				       force_spring + 
				       force_damping + 
				       force_drag);

		//
		// End total force
		////////////////////////////////////////////////////////////////////////////////

        
		////////////////////////////////////////////////////////////////////////////////
		// Calculate new params
		//

		// Calculate the new acceleration based on the net force.
		// a = F/m
		const F32 acceleration_new_local = force_net / behavior_mass;
		static const F32 max_velocity = 100.0f; // magic number, used to be customizable.
		F32 velocity_new_local = mVelocity_local + acceleration_new_local*time_iteration_step;
		velocity_new_local = llclamp(velocity_new_local, 
					     -max_velocity, max_velocity);
        
		// Temporary debugging setting to cause all avatars to move, for profiling purposes.
		if (physics_test)
		{
			velocity_new_local = sin(time*4.0f);
		}
		// Calculate the new parameters, or remain unchanged if max speed is 0.
		F32 position_new_local = position_current_local + velocity_new_local*time_iteration_step;
		if (behavior_maxeffect == 0.f)
			position_new_local = position_user_local;

		// Zero out the velocity if the param is being pushed beyond its limits.
		if ((position_new_local < 0.f && velocity_new_local < 0.f) ||
		    (position_new_local > 1.f && velocity_new_local > 0.f))
		{
			velocity_new_local = 0.f;
		}
	
		// Check for NaN values.  A NaN value is detected if the variables doesn't equal itself.  
		// If NaN, then reset everything.
		if ((mPosition_local != mPosition_local) ||
		    (mVelocity_local != mVelocity_local) ||
		    (position_new_local != position_new_local))
		{
			position_new_local = 0.f;
			mVelocity_local = 0.f;
			mVelocityJoint_local = 0.f;
			mAccelerationJoint_local = 0.f;
			mPosition_local = 0.f;
			mPosition_world.setZero();
		}

		const F32 position_new_local_clamped = llclamp(position_new_local,
							       0.0f,
							       1.0f);

		LLDriverParam *driver_param = dynamic_cast<LLDriverParam *>(mParamDriver);
		llassert_always(driver_param);
		if (driver_param)
		{
			// If this is one of our "hidden" driver params, then make sure it's
			// the default value.
			if ((driver_param->getGroup() != VISUAL_PARAM_GROUP_TWEAKABLE) &&
			    (driver_param->getGroup() != VISUAL_PARAM_GROUP_TWEAKABLE_NO_TRANSMIT))
			{
				mCharacter->setVisualParamWeight(driver_param, 0.f, FALSE);
			}
			S32 num_driven = driver_param->getDrivenParamsCount();
			for (S32 i = 0; i < num_driven; ++i)
			{
				const LLViewerVisualParam *driven_param = driver_param->getDrivenParam(i);
				setParamValue(driven_param,position_new_local_clamped, behavior_maxeffect);
			}
		}
        
		//
		// End calculate new params
		////////////////////////////////////////////////////////////////////////////////

		////////////////////////////////////////////////////////////////////////////////
		// Conditionally update the visual params
		//
        
		// Updating the visual params (i.e. what the user sees) is fairly expensive.
		// So only update if the params have changed enough, and also take into account
		// the graphics LOD settings.
        
		// For non-self, if the avatar is small enough visually, then don't update.
		const F32 area_for_max_settings = 0.0f;
		const F32 area_for_min_settings = 1400.0f;
		const F32 area_for_this_setting = area_for_max_settings + (area_for_min_settings-area_for_max_settings)*(1.0f-lod_factor);
	        const F32 pixel_area = sqrtf(mCharacter->getPixelArea());
        
		const BOOL is_self = (dynamic_cast<LLVOAvatarSelf *>(mCharacter) != nullptr);
		if ((pixel_area > area_for_this_setting) || is_self)
		{
			const F32 position_diff_local = llabs(mPositionLastUpdate_local-position_new_local_clamped);
			const F32 min_delta = (1.0001f-lod_factor)*0.4f;
			if (llabs(position_diff_local) > min_delta)
			{
				update_visuals = TRUE;
				mPositionLastUpdate_local = position_new_local;
			}
		}

		//
		// End update visual params
		////////////////////////////////////////////////////////////////////////////////

		mVelocity_local = velocity_new_local;
		mAccelerationJoint_local = acceleration_joint_local;
		mPosition_local = position_new_local;
	}
	mLastTime = time;
	mPosition_world = joint->getWorldPosition();
	mVelocityJoint_local = velocity_joint_local;


        /*
          // Write out debugging info into a spreadsheet.
          if (mFileWrite != NULL && is_self)
          {
          fprintf(mFileWrite,"%f\t%f\t%f \t\t%f \t\t%f\t%f\t%f\t \t\t%f\t%f\t%f\t%f\t%f \t\t%f\t%f\t%f\n",
          position_new_local,
          velocity_new_local,
          acceleration_new_local,

          time_delta,

          mPosition_world[0],
          mPosition_world[1],
          mPosition_world[2],

          force_net,
          force_spring,
          force_accel,
          force_damping,
          force_drag,

          spring_length,
          velocity_joint_local,
          acceleration_joint_local
          );
          }
        */

        return update_visuals;
}

F32 LLPhysicsMotion::getParamValue(eParamName param)
{
	static std::string controller_key[] =
	{
		"Smoothing",
		"Mass",
		"Gravity",
		"Spring",
		"Gain",
		"Damping",
		"Drag",
		"MaxEffect"
	};

	if (!mParamCache[param])
	{
		const controller_map_t::const_iterator& entry = mParamControllers.find(controller_key[param]);
		if (entry == mParamControllers.end())
		{
			return sDefaultController[controller_key[param]];
		}
		const std::string& param_name = (*entry).second.c_str();
		mParamCache[param] = mCharacter->getVisualParam(param_name.c_str());
	}

	if (mParamCache[param])
	{
		return mParamCache[param]->getWeight();
	}
	else
	{
		return sDefaultController[controller_key[param]];
	}
}

// Range of new_value_local is assumed to be [0 , 1] normalized.
void LLPhysicsMotion::setParamValue(const LLViewerVisualParam *param,
                                    F32 new_value_normalized,
				    F32 behavior_maxeffect)
{
        const F32 value_min_local = param->getMinWeight();
        const F32 value_max_local = param->getMaxWeight();
        const F32 min_val = 0.5f-behavior_maxeffect/2.0f;
        const F32 max_val = 0.5f+behavior_maxeffect/2.0f;

	// Scale from [0,1] to [min_val,max_val]
	const F32 new_value_rescaled = min_val + (max_val-min_val) * new_value_normalized;
	
	// Scale from [0,1] to [value_min_local,value_max_local]
        const F32 new_value_local = value_min_local + (value_max_local-value_min_local) * new_value_rescaled;

        mCharacter->setVisualParamWeight(param, new_value_local, FALSE);
}
