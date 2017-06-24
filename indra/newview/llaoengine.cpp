// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file aoengine.cpp
 * @brief The core Animation Overrider engine
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"


#include "roles_constants.h"

#include "llaoengine.h"
#include "llaoset.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llanimationstates.h"
#include "llassetstorage.h"
#include "llinventorydefines.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llnotificationsutil.h"
#include "llvfs.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llvoavatarself.h"

#include <boost/graph/graph_concepts.hpp>

const F32 INVENTORY_POLLING_INTERVAL = 5.0f;

LLAOEngine::LLAOEngine()
: mEnabled(false)
, mInMouselook(false)
, mUnderWater(false)
, mAOFolder(LLUUID::null)
, mLastMotion(ANIM_AGENT_STAND)
, mLastOverriddenMotion(ANIM_AGENT_STAND)
, mCurrentSet(nullptr)
, mDefaultSet(nullptr)
, mImportSet(nullptr)
, mImportCategory(LLUUID::null)
, mImportRetryCount(0)
{
	gSavedPerAccountSettings.getControl("UseAO")->getCommitSignal()->connect(boost::bind(&LLAOEngine::onToggleAOControl, this));

	mRegionChangeConnection = gAgent.addRegionChangedCallback(boost::bind(&LLAOEngine::onRegionChange, this));
}

LLAOEngine::~LLAOEngine()
{
	clear(false);

	if (mRegionChangeConnection.connected())
	{
		mRegionChangeConnection.disconnect();
	}
}

void LLAOEngine::init()
{
	enable(mEnabled);
}

// static
void LLAOEngine::onLoginComplete()
{
	LLAOEngine::instance().init();
}

void LLAOEngine::onToggleAOControl()
{
	enable(gSavedPerAccountSettings.getBool("UseAO"));
}

void LLAOEngine::clear(bool aFromTimer)
{
	mOldSets.insert(mOldSets.end(), mSets.begin(), mSets.end());
	mSets.clear();

	mCurrentSet = nullptr;

	if (!aFromTimer)
	{
		std::for_each(mOldSets.begin(), mOldSets.end(), DeletePointer());
		mOldSets.clear();

		std::for_each(mOldImportSets.begin(), mOldImportSets.end(), DeletePointer());
		mOldImportSets.clear();
	}
}

void LLAOEngine::stopAllStandVariants()
{
	LL_DEBUGS("AOEngine") << "stopping all STAND variants." << LL_ENDL;
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_1, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_2, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_3, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_4, ANIM_REQUEST_STOP);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_1);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_2);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_3);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_4);
}

void LLAOEngine::stopAllSitVariants()
{
	LL_DEBUGS("AOEngine") << "stopping all SIT variants." << LL_ENDL;
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_FEMALE, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_FEMALE);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GENERIC);

	// scripted seats that use ground_sit as animation need special treatment
	const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
	if (agentRoot && agentRoot->getID() != gAgentID)
	{
		LL_DEBUGS("AOEngine") << "Not stopping ground sit animations while sitting on a prim." << LL_ENDL;
		return;
	}

	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND_CONSTRAINED, ANIM_REQUEST_STOP);

	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND_CONSTRAINED);
}

void LLAOEngine::setLastMotion(const LLUUID& motion)
{
	if (motion != ANIM_AGENT_TYPE)
	{
		mLastMotion = motion;
	}
}

void LLAOEngine::setLastOverriddenMotion(const LLUUID& motion)
{
	if (motion != ANIM_AGENT_TYPE)
	{
		mLastOverriddenMotion = motion;
	}
}

bool LLAOEngine::foreignAnimations(const LLUUID& seat)
{
	for (LLVOAvatar::AnimSourceIterator sourceIterator = gAgentAvatarp->mAnimationSources.begin();
		sourceIterator != gAgentAvatarp->mAnimationSources.end(); ++sourceIterator)
	{
		if (sourceIterator->first != gAgentID)
		{
			if (seat.isNull() || sourceIterator->first == seat)
			{
				return true;
			}
		}
	}
	return false;
}

const LLUUID& LLAOEngine::mapSwimming(const LLUUID& motion) const
{
	S32 name;

	if (motion == ANIM_AGENT_HOVER)
	{
		name = LLAOSet::Floating;
	}
	else if (motion == ANIM_AGENT_FLY)
	{
		name = LLAOSet::SwimmingForward;
	}
	else if (motion == ANIM_AGENT_HOVER_UP)
	{
		name = LLAOSet::SwimmingUp;
	}
	else if (motion == ANIM_AGENT_HOVER_DOWN)
	{
		name = LLAOSet::SwimmingDown;
	}
	else
	{
		return LLUUID::null;
	}

	LLAOSet::AOState* state = mCurrentSet->getState(name);
	return mCurrentSet->getAnimationForState(state);
}

void LLAOEngine::checkBelowWater(const bool under)
{
	if (mUnderWater == under) return;

	// only restart underwater/above water motion if the overridden motion is the one currently playing
	if (mLastMotion != mLastOverriddenMotion) return;

	gAgent.sendAnimationRequest(override(mLastOverriddenMotion, false), ANIM_REQUEST_STOP);
	mUnderWater = under;
	gAgent.sendAnimationRequest(override(mLastOverriddenMotion, true), ANIM_REQUEST_START);
}

void LLAOEngine::enable(const bool enable)
{
	LL_DEBUGS("AOEngine") << "using " << mLastMotion << " enable " << enable << LL_ENDL;
	mEnabled = enable;

	if (!mCurrentSet)
	{
		LL_DEBUGS("AOEngine") << "enable(" << enable << ") without animation set loaded." << LL_ENDL;
		return;
	}

	LLAOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastMotion);
	if (mEnabled)
	{
		if (state && !state->mAnimations.empty())
		{
			LL_DEBUGS("AOEngine") << "Enabling animation state " << state->mName << LL_ENDL;

			// do not stop underlying ground sit when re-enabling the AO
			if (mLastOverriddenMotion != ANIM_AGENT_SIT_GROUND_CONSTRAINED)
			{
				gAgent.sendAnimationRequest(mLastOverriddenMotion, ANIM_REQUEST_STOP);
			}

			LLUUID animation = override(mLastMotion, true);
			if (animation.isNull()) return;

			if (mLastMotion == ANIM_AGENT_STAND)
			{
				stopAllStandVariants();
			}
			else if (mLastMotion == ANIM_AGENT_WALK)
			{
				LL_DEBUGS("AOEngine") << "Last motion was a WALK, stopping all variants." << LL_ENDL;
				gAgent.sendAnimationRequest(ANIM_AGENT_WALK_NEW, ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK, ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK_NEW, ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_WALK_NEW);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK_NEW);
			}
			else if (mLastMotion == ANIM_AGENT_RUN)
			{
				LL_DEBUGS("AOEngine") << "Last motion was a RUN, stopping all variants." << LL_ENDL;
				gAgent.sendAnimationRequest(ANIM_AGENT_RUN_NEW, ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_RUN_NEW, ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_RUN_NEW);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_RUN_NEW);
			}
			else if (mLastMotion == ANIM_AGENT_SIT)
			{
				stopAllSitVariants();
				gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_START);
			}
			else
			{
				LL_WARNS("AOEngine") << "Unhandled last motion id " << mLastMotion << LL_ENDL;
			}

			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
			mAnimationChangedSignal(state->mAnimations[state->mCurrentAnimation].mInventoryUUID);
		}
	}
	else
	{
		mAnimationChangedSignal(LLUUID::null);

		gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
		// stop all overriders, catch leftovers
		for (U32 index = 0; index < LLAOSet::AOSTATES_MAX; ++index)
		{
			state = mCurrentSet->getState(index);
			if (state)
			{
				LLUUID animation = state->mCurrentAnimationID;
				if (animation.notNull())
				{
					LL_DEBUGS("AOEngine") << "Stopping leftover animation from state " << state->mName << LL_ENDL;
					gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
					gAgentAvatarp->LLCharacter::stopMotion(animation);
					state->mCurrentAnimationID.setNull();
				}
			}
			else
			{
				LL_DEBUGS("AOEngine") << "state "<< index <<" returned NULL." << LL_ENDL;
			}
		}

		if (!foreignAnimations(LLUUID::null))
		{
			gAgent.sendAnimationRequest(mLastMotion, ANIM_REQUEST_START);
		}

		mCurrentSet->stopTimer();
	}
}

void LLAOEngine::setStateCycleTimer(const LLAOSet::AOState* state)
{
	F32 timeout = state->mCycleTime;
	LL_DEBUGS("AOEngine") << "Setting cycle timeout for state " << state->mName << " of " << timeout << LL_ENDL;
	if (timeout > 0.0f)
	{
		mCurrentSet->startTimer(timeout);
	}
}

const LLUUID LLAOEngine::override(const LLUUID& pMotion, const bool start)
{
	LL_DEBUGS("AOEngine") << "override(" << pMotion << "," << start << ")" << LL_ENDL;

	LLUUID animation = LLUUID::null;

	LLUUID motion = pMotion;

	if (!mEnabled)
	{
		if (start && mCurrentSet)
		{
			const LLAOSet::AOState* state = mCurrentSet->getStateByRemapID(motion);
			if (state)
			{
				setLastMotion(motion);
				LL_DEBUGS("AOEngine") << "(disabled AO) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
				if (!state->mAnimations.empty())
				{
					setLastOverriddenMotion(motion);
					LL_DEBUGS("AOEngine") << "(disabled AO) setting last overridden motion id to " <<  gAnimLibrary.animationName(mLastOverriddenMotion) << LL_ENDL;
				}
			}
		}
		return animation;
	}

	if (mSets.empty())
	{
		LL_DEBUGS("AOEngine") << "No sets loaded. Skipping overrider." << LL_ENDL;
		return animation;
	}

	if (!mCurrentSet)
	{
		LL_DEBUGS("AOEngine") << "No current AO set chosen. Skipping overrider." << LL_ENDL;
		return animation;
	}

	// we don't distinguish between these two
	if (motion == ANIM_AGENT_SIT_GROUND)
	{
		motion = ANIM_AGENT_SIT_GROUND_CONSTRAINED;
	}

	LLAOSet::AOState* state = mCurrentSet->getStateByRemapID(motion);
	if (!state)
	{
		LL_DEBUGS("AOEngine") << "No current AO state for motion " << motion << " (" << gAnimLibrary.animationName(motion) << ")." << LL_ENDL;
		return animation;
	}

	mAnimationChangedSignal(LLUUID::null);

	mCurrentSet->stopTimer();
	if (start)
	{
		setLastMotion(motion);
		LL_DEBUGS("AOEngine") << "(enabled AO) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;

		// Disable start stands in Mouselook
		if (mCurrentSet->getMouselookDisable() &&
			motion == ANIM_AGENT_STAND &&
			mInMouselook)
		{
			LL_DEBUGS("AOEngine") << "(enabled AO, mouselook stand stopped) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
			return animation;
		}

		// Do not start override sits if not selected
		if (!mCurrentSet->getSitOverride() && motion == ANIM_AGENT_SIT)
		{
			LL_DEBUGS("AOEngine") << "(enabled AO, sit override stopped) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
			return animation;
		}

		// scripted seats that use ground_sit as animation need special treatment
		if (motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED)
		{
			const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
			if (agentRoot && agentRoot->getID() != gAgentID)
			{
				LL_DEBUGS("AOEngine") << "Ground sit animation playing but sitting on a prim - disabling overrider." << LL_ENDL;
				return animation;
			}
		}

		if (!state->mAnimations.empty())
		{
			setLastOverriddenMotion(motion);
			LL_DEBUGS("AOEngine") << "(enabled AO) setting last overridden motion id to " <<  gAnimLibrary.animationName(mLastOverriddenMotion) << LL_ENDL;
		}

		// do not remember typing as set-wide motion
		if (motion != ANIM_AGENT_TYPE)
		{
			mCurrentSet->setMotion(motion);
		}

		mUnderWater = gAgentAvatarp->mBelowWater;
		if (mUnderWater)
		{
			animation = mapSwimming(motion);
		}

		if (animation.isNull())
		{
			animation = mCurrentSet->getAnimationForState(state);
		}

		if (state->mCurrentAnimationID.notNull())
		{
			LL_DEBUGS("AOEngine")	<< "Previous animation for state "
						<< gAnimLibrary.animationName(motion)
						<< " was not stopped, but we were asked to start a new one. Killing old animation." << LL_ENDL;
			gAgent.sendAnimationRequest(state->mCurrentAnimationID, ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(state->mCurrentAnimationID);
		}

		state->mCurrentAnimationID = animation;
		LL_DEBUGS("AOEngine")	<< "overriding " <<  gAnimLibrary.animationName(motion)
					<< " with " << animation
					<< " in state " << state->mName
					<< " of set " << mCurrentSet->getName()
					<< " (" << mCurrentSet << ")" << LL_ENDL;

		if (animation.notNull() && state->mCurrentAnimation < state->mAnimations.size())
		{
			mAnimationChangedSignal(state->mAnimations[state->mCurrentAnimation].mInventoryUUID);
		}

		setStateCycleTimer(state);

		if (motion == ANIM_AGENT_SIT)
		{
			// Use ANIM_AGENT_SIT_GENERIC, so we don't create an overrider loop with ANIM_AGENT_SIT
			// while still having a base sitting pose to cover up cycle points
			gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_START);
			if (mCurrentSet->getSmart())
			{
				mSitCancelTimer.oneShot();
			}
		}
		// special treatment for "transient animations" because the viewer needs the Linden animation to know the agent's state
		else if (motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED ||
				motion == ANIM_AGENT_PRE_JUMP ||
				motion == ANIM_AGENT_STANDUP ||
				motion == ANIM_AGENT_LAND ||
				motion == ANIM_AGENT_MEDIUM_LAND)
		{
			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
			return LLUUID::null;
		}
	}
	else
	{
		animation = state->mCurrentAnimationID;
		state->mCurrentAnimationID.setNull();

		// for typing animaiton, just return the stored animation, reset the state timer, and don't memorize anything else
		if (motion == ANIM_AGENT_TYPE)
		{
			LLAOSet::AOState* previousState = mCurrentSet->getStateByRemapID(mLastMotion);
			if (previousState)
			{
				setStateCycleTimer(previousState);
			}
			return animation;
		}

		if (motion != mCurrentSet->getMotion())
		{
			LL_WARNS("AOEngine") << "trying to stop-override motion " <<  gAnimLibrary.animationName(motion)
					<< " but the current set has motion " <<  gAnimLibrary.animationName(mCurrentSet->getMotion()) << LL_ENDL;
			return animation;
		}

		mCurrentSet->setMotion(LLUUID::null);

		// again, special treatment for "transient" animations to make sure our own animation gets stopped properly
		if (motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED ||
			motion == ANIM_AGENT_PRE_JUMP ||
			motion == ANIM_AGENT_STANDUP ||
			motion == ANIM_AGENT_LAND ||
			motion == ANIM_AGENT_MEDIUM_LAND)
		{
			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			setStateCycleTimer(state);
			return LLUUID::null;
		}

		// stop the underlying Linden Lab motion, in case it's still running.
		// frequently happens with sits, so we keep it only for those currently.
		if (mLastMotion == ANIM_AGENT_SIT)
		{
			stopAllSitVariants();
		}

		LL_DEBUGS("AOEngine") << "stopping cycle timer for motion " <<  gAnimLibrary.animationName(motion) <<
					" using animation " << animation <<
					" in state " << state->mName << LL_ENDL;
	}

	return animation;
}

void LLAOEngine::checkSitCancel()
{
	LLUUID seat;

	const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
	if (agentRoot)
	{
		seat = agentRoot->getID();
	}

	if (foreignAnimations(seat))
	{
		LLAOSet::AOState* aoState = mCurrentSet->getStateByRemapID(ANIM_AGENT_SIT);
		if (aoState)
		{
			LLUUID animation = aoState->mCurrentAnimationID;
			if (animation.notNull())
			{
				LL_DEBUGS("AOEngine") << "Stopping sit animation due to foreign animations running" << LL_ENDL;
				gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
				// remove cycle point cover-up
				gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(animation);
				mSitCancelTimer.stop();
				// stop cycle tiemr
				mCurrentSet->stopTimer();
			}
		}
	}
}

void LLAOEngine::cycleTimeout(const LLAOSet* set)
{
	if (!mEnabled)
	{
		return;
	}

	if (set != mCurrentSet)
	{
		LL_WARNS("AOEngine") << "cycleTimeout for set " << set->getName() << " but current set is " << mCurrentSet->getName() << LL_ENDL;
		return;
	}

	cycle(CycleAny);
}

void LLAOEngine::cycle(eCycleMode cycleMode)
{
	if (!mCurrentSet)
	{
		LL_DEBUGS("AOEngine") << "cycle without set." << LL_ENDL;
		return;
	}

	LLUUID motion = mCurrentSet->getMotion();

	// assume stand if no motion is registered, happens after login when the avatar hasn't moved at all yet
	// or if the agent has said something in local chat while sitting
	if (motion.isNull())
	{
		if (gAgentAvatarp->isSitting())
		{
			motion = ANIM_AGENT_SIT;
		}
		else
		{
			motion = ANIM_AGENT_STAND;
		}
	}

	// do not cycle if we're sitting and sit-override is off
	else if (motion == ANIM_AGENT_SIT && !mCurrentSet->getSitOverride())
	{
		return;
	}
	// do not cycle if we're standing and mouselook stand override is disabled while being in mouselook
	else if (motion == ANIM_AGENT_STAND && mCurrentSet->getMouselookDisable() && mInMouselook)
	{
		return;
	}

	LLAOSet::AOState* state = mCurrentSet->getStateByRemapID(motion);
	if (!state)
	{
		LL_DEBUGS("AOEngine") << "cycle without state." << LL_ENDL;
		return;
	}

	if (!state->mAnimations.size())
	{
		LL_DEBUGS("AOEngine") << "cycle without animations in state." << LL_ENDL;
		return;
	}

	// make sure we disable cycling only for timed cycle, so manual cycling still works, even with cycling switched off
	if (!state->mCycle && cycleMode == CycleAny)
	{
		LL_DEBUGS("AOEngine") << "cycle timeout, but state is set to not cycling." << LL_ENDL;
		return;
	}

	LLUUID oldAnimation = state->mCurrentAnimationID;
	LLUUID animation;

	if (cycleMode == CycleAny)
	{
		animation = mCurrentSet->getAnimationForState(state);
	}
	else
	{
		if (cycleMode == CyclePrevious)
		{
			if (state->mCurrentAnimation == 0)
			{
				state->mCurrentAnimation = state->mAnimations.size() - 1;
			}
			else
			{
				state->mCurrentAnimation--;
			}
		}
		else if (cycleMode == CycleNext)
		{
			state->mCurrentAnimation++;
			if (state->mCurrentAnimation == state->mAnimations.size())
			{
				state->mCurrentAnimation = 0;
			}
		}
		animation = state->mAnimations[state->mCurrentAnimation].mAssetUUID;
	}

	// don't do anything if the animation didn't change
	if (animation == oldAnimation)
	{
		return;
	}

	mAnimationChangedSignal(LLUUID::null);

	state->mCurrentAnimationID = animation;
	if (animation.notNull())
	{
		LL_DEBUGS("AOEngine") << "requesting animation start for motion " << gAnimLibrary.animationName(motion) << ": " << animation << LL_ENDL;
		gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
		mAnimationChangedSignal(state->mAnimations[state->mCurrentAnimation].mInventoryUUID);
	}
	else
	{
		LL_DEBUGS("AOEngine") << "overrider came back with NULL animation for motion " << gAnimLibrary.animationName(motion) << "." << LL_ENDL;
	}

	if (oldAnimation.notNull())
	{
		LL_DEBUGS("AOEngine") << "Cycling state " << state->mName << " - stopping animation " << oldAnimation << LL_ENDL;
		gAgent.sendAnimationRequest(oldAnimation, ANIM_REQUEST_STOP);
		gAgentAvatarp->LLCharacter::stopMotion(oldAnimation);
	}
}

void LLAOEngine::updateSortOrder(LLAOSet::AOState* state)
{
	for (U32 index = 0; index < state->mAnimations.size(); ++index)
	{
		auto& anim = state->mAnimations[index];
		U32 sortOrder = anim.mSortOrder;

		if (sortOrder != index)
		{
			std::ostringstream numStr("");
			numStr << index;

			LL_DEBUGS("AOEngine")	<< "sort order is " << sortOrder << " but index is " << index
						<< ", setting sort order description: " << numStr.str() << LL_ENDL;

			anim.mSortOrder = index;

			LLViewerInventoryItem* item = gInventory.getItem(anim.mInventoryUUID);
			if (!item)
			{
				LL_WARNS("AOEngine") << "NULL inventory item found while trying to copy " << anim.mInventoryUUID << LL_ENDL;
				continue;
			}
			LLPointer<LLViewerInventoryItem> newItem = new LLViewerInventoryItem(item);

			newItem->setDescription(numStr.str());
			newItem->setComplete(TRUE);
			newItem->updateServer(FALSE);

			gInventory.updateItem(newItem);
		}
	}
}

LLUUID LLAOEngine::addSet(const std::string& name, const bool reload)
{
	if (mAOFolder.isNull())
	{
		LL_WARNS("AOEngine") << ROOT_AO_FOLDER << " folder not there yet. Requesting recreation." << LL_ENDL;
		tick();
		return LLUUID::null;
	}

	LL_DEBUGS("AOEngine") << "adding set folder " << name << LL_ENDL;
	LLUUID newUUID = gInventory.createNewCategory(mAOFolder, LLFolderType::FT_NONE, name);

	if (reload)
	{
		mTimerCollection.setReloadTimer(true);
	}
	return newUUID;
}

bool LLAOEngine::createAnimationLink(const LLAOSet* set, LLAOSet::AOState* state, const LLInventoryItem* item)
{
	LL_DEBUGS("AOEngine") << "Asset ID " << item->getAssetUUID() << " inventory id "
		<< item->getUUID() << " category id " << state->mInventoryUUID << LL_ENDL;
	if (state->mInventoryUUID.isNull())
	{
		LL_DEBUGS("AOEngine") << "no " << state->mName << " folder yet. Creating ..." << LL_ENDL;
		gInventory.createNewCategory(set->getInventoryUUID(), LLFolderType::FT_NONE, state->mName);

		LL_DEBUGS("AOEngine") << "looking for folder to get UUID ..." << LL_ENDL;
		LLUUID newStateFolderUUID;

		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(set->getInventoryUUID(), cats, items);

		for (S32 index = 0; index < cats->size(); ++index)
		{
			if (cats->at(index)->getName().compare(state->mName) == 0)
			{
				LL_DEBUGS("AOEngine") << "UUID found!" << LL_ENDL;
				newStateFolderUUID = cats->at(index)->getUUID();
				state->mInventoryUUID = newStateFolderUUID;
				break;
			}
		}
	}

	if (state->mInventoryUUID.isNull())
	{
		LL_DEBUGS("AOEngine") << "state inventory UUID not found, failing." << LL_ENDL;
		return FALSE;
	}

	LLInventoryObject::const_object_list_t obj_array;
	obj_array.emplace_back(LLConstPointer<LLInventoryObject>(item));
	link_inventory_array(state->mInventoryUUID,
							obj_array,
							LLPointer<LLInventoryCallback>(NULL));

	return TRUE;
}

bool LLAOEngine::addAnimation(const LLAOSet* set, LLAOSet::AOState* state,
							  const LLInventoryItem* item, const bool reload)
{
	state->mAnimations.emplace_back(item->getName(), item->getAssetUUID(), item->getUUID(), state->mAnimations.size() + 1);

	createAnimationLink(set, state, item);

	if (reload)
	{
		mTimerCollection.setReloadTimer(true);
	}
	return TRUE;
}

bool LLAOEngine::findForeignItems(const LLUUID& uuid) const
{
	bool moved = false;

	LLInventoryModel::item_array_t* items;
	LLInventoryModel::cat_array_t* cats;

	gInventory.getDirectDescendentsOf(uuid, cats, items);
	for (S32 index = 0; index < cats->size(); ++index)
	{
		// recurse into subfolders
		if (findForeignItems(cats->at(index)->getUUID()))
		{
			moved = true;
		}
	}

	// count backwards in case we have to remove items
	for (S32 index = items->size() - 1; index >= 0; --index)
	{
		bool move = false;

		LLPointer<LLViewerInventoryItem> item = items->at(index);
		if (item->getIsLinkType())
		{
			if (item->getInventoryType() != LLInventoryType::IT_ANIMATION)
			{
				LL_DEBUGS("AOEngine") << item->getName() << " is a link but does not point to an animation." << LL_ENDL;
				move = true;
			}
			else
			{
				LL_DEBUGS("AOEngine") << item->getName() << " is an animation link." << LL_ENDL;
			}
		}
		else
		{
			LL_DEBUGS("AOEngine") << item->getName() << " is not a link!" << LL_ENDL;
			move = true;
		}

		if (move)
		{
			moved = true;
			LLInventoryModel* model = &gInventory;
			model->changeItemParent(item, gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND), FALSE);
			LL_DEBUGS("AOEngine") << item->getName() << " moved to lost and found!" << LL_ENDL;
		}
	}

	return moved;
}

// needs a three-step process, since purge of categories only seems to work from trash
void LLAOEngine::purgeFolder(const LLUUID& uuid) const
{
	// move everything that's not an animation link to "lost and found"
	if (findForeignItems(uuid))
	{
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
	}

	// trash it
	gInventory.removeCategory(uuid);

	// clean it
	purge_descendents_of(uuid, NULL);
	gInventory.notifyObservers();

	// purge it
	remove_inventory_object(uuid, NULL);
	gInventory.notifyObservers();
}

bool LLAOEngine::removeSet(LLAOSet* set)
{
	purgeFolder(set->getInventoryUUID());

	mTimerCollection.setReloadTimer(true);
	return true;
}

bool LLAOEngine::removeAnimation(const LLAOSet* set, LLAOSet::AOState* state, S32 index)
{
	// Protect against negative index
	if (index <= -1) return false;

	size_t numOfAnimations = state->mAnimations.size();
	if (!numOfAnimations) return false;

	LLViewerInventoryItem* item = gInventory.getItem(state->mAnimations[index].mInventoryUUID);

	// check if this item is actually an animation link
	bool move = (item->getIsLinkType() && item->getInventoryType() == LLInventoryType::IT_ANIMATION) ? false : true;

	// this item was not an animation link, move it to lost and found
	if (move)
	{
		LLInventoryModel* model = &gInventory;
		model->changeItemParent(item, gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND), false);
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
		update();
		return false;
	}

	// purge the item from inventory
	LL_DEBUGS("AOEngine") << __LINE__ << " purging: " << state->mAnimations[index].mInventoryUUID << LL_ENDL;
	remove_inventory_object(state->mAnimations[index].mInventoryUUID, NULL); // item->getUUID());
	gInventory.notifyObservers();

	state->mAnimations.erase(state->mAnimations.begin() + index);

	if (!state->mAnimations.size())
	{
		LL_DEBUGS("AOEngine") << "purging folder " << state->mName << " from inventory because it's empty." << LL_ENDL;

		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(set->getInventoryUUID(), cats, items);

		for (LLInventoryModel::cat_array_t::iterator it = cats->begin(); it != cats->end(); ++it)
		{
			LLPointer<LLInventoryCategory> cat = (*it);
			std::vector<std::string> params;
			LLStringUtil::getTokens(cat->getName(), params, ":");
			std::string stateName = params[0];

			if (state->mName.compare(stateName) == 0)
			{
				LL_DEBUGS("AOEngine") << "folder found: " << cat->getName() << " purging uuid " << cat->getUUID() << LL_ENDL;

				purgeFolder(cat->getUUID());
				state->mInventoryUUID.setNull();
				break;
			}
		}
	}
	else
	{
		updateSortOrder(state);
	}

	return true;
}

bool LLAOEngine::swapWithPrevious(LLAOSet::AOState* state, S32 index)
{
	S32 numOfAnimations = state->mAnimations.size();
	if (numOfAnimations < 2 || index == 0)
	{
		return false;
	}

	LLAOSet::AOAnimation tmpAnim = state->mAnimations[index];
	state->mAnimations.erase(state->mAnimations.begin() + index);
	state->mAnimations.insert(state->mAnimations.begin() + index - 1, tmpAnim);

	updateSortOrder(state);

	return true;
}

bool LLAOEngine::swapWithNext(LLAOSet::AOState* state, S32 index)
{
	S32 numOfAnimations = state->mAnimations.size();
	if (numOfAnimations < 2 || index == (numOfAnimations - 1))
	{
		return false;
	}

	LLAOSet::AOAnimation tmpAnim = state->mAnimations[index];
	state->mAnimations.erase(state->mAnimations.begin() + index);
	state->mAnimations.insert(state->mAnimations.begin() + index + 1, tmpAnim);

	updateSortOrder(state);

	return true;
}

void LLAOEngine::reloadStateAnimations(LLAOSet::AOState* state)
{
	LLInventoryModel::item_array_t* items;
	LLInventoryModel::cat_array_t* dummy;

	state->mAnimations.clear();

	gInventory.getDirectDescendentsOf(state->mInventoryUUID, dummy, items);
	for (S32 num = 0; num < items->size(); ++num)
	{
		LL_DEBUGS("AOEngine")	<< "Found animation link " << items->at(num)->LLInventoryItem::getName()
					<< " desc " << items->at(num)->LLInventoryItem::getDescription()
					<< " asset " << items->at(num)->getAssetUUID() << LL_ENDL;

		LLViewerInventoryItem* linkedItem = items->at(num)->getLinkedItem();
		if (!linkedItem)
		{
			LL_WARNS("AOEngine") << "linked item for link " << items->at(num)->LLInventoryItem::getName() << " not found (broken link). Skipping." << LL_ENDL;
			continue;
		}

		S32 sortOrder;
		if (!LLStringUtil::convertToS32(items->at(num)->LLInventoryItem::getDescription(), sortOrder))
		{
			sortOrder = -1;
		}

		LL_DEBUGS("AOEngine") << "current sort order is " << sortOrder << LL_ENDL;

		if (sortOrder == -1)
		{
			LL_WARNS("AOEngine") << "sort order was unknown so append to the end of the list" << LL_ENDL;
			state->mAnimations.emplace_back(linkedItem->LLInventoryItem::getName(), items->at(num)->getAssetUUID(), items->at(num)->getUUID(), sortOrder);
		}
		else
		{
			bool inserted = false;
			for (U32 index = 0; index < state->mAnimations.size(); ++index)
			{
				if (state->mAnimations[index].mSortOrder > sortOrder)
				{
					LL_DEBUGS("AOEngine") << "inserting at index " << index << LL_ENDL;
					state->mAnimations.emplace(state->mAnimations.begin() + index, 
						linkedItem->LLInventoryItem::getName(), items->at(num)->getAssetUUID(), items->at(num)->getUUID(), sortOrder);
					inserted = true;
					break;
				}
			}
			if (!inserted)
			{
				LL_DEBUGS("AOEngine") << "not inserted yet, appending to the list instead" << LL_ENDL;
				state->mAnimations.emplace_back(linkedItem->LLInventoryItem::getName(), items->at(num)->getAssetUUID(), items->at(num)->getUUID(), sortOrder);
			}
		}
		LL_DEBUGS("AOEngine") << "Animation count now: " << state->mAnimations.size() << LL_ENDL;
	}

	updateSortOrder(state);
}

void LLAOEngine::update()
{
	if (mAOFolder.isNull()) return;

	// move everything that's not an animation link to "lost and found"
	if (findForeignItems(mAOFolder))
	{
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
	}

	LLInventoryModel::cat_array_t* categories;
	LLInventoryModel::item_array_t* items;

	bool allComplete = true;
	mTimerCollection.setSettingsTimer(false);

	gInventory.getDirectDescendentsOf(mAOFolder, categories, items);
	for (S32 index = 0; index < categories->size(); ++index)
	{
		LLViewerInventoryCategory* currentCategory = categories->at(index);
		const std::string& setFolderName = currentCategory->getName();

		if (setFolderName.empty())
		{
			LL_WARNS("AOEngine") << "Folder with emtpy name in AO folder" << LL_ENDL;
			continue;
		}

		std::vector<std::string> params;
		LLStringUtil::getTokens(setFolderName, params, ":");

		LLAOSet* newSet = getSetByName(params[0]);
		if (!newSet)
		{
			LL_DEBUGS("AOEngine") << "Adding set " << setFolderName << " to AO." << LL_ENDL;
			newSet = new LLAOSet(currentCategory->getUUID());
			newSet->setName(params[0]);
			mSets.emplace_back(newSet);
		}
		else
		{
			if (newSet->getComplete())
			{
				LL_DEBUGS("AOEngine") << "Set " << params[0] << " already complete. Skipping." << LL_ENDL;
				continue;
			}
			LL_DEBUGS("AOEngine") << "Updating set " << setFolderName << " in AO." << LL_ENDL;
		}
		allComplete = FALSE;

		for (U32 num = 1; num < params.size(); ++num)
		{
			if (params[num].size() != 2)
			{
				LL_WARNS("AOEngine") << "Unknown AO set option " << params[num] << LL_ENDL;
			}
			else if (params[num] == "SO")
			{
				newSet->setSitOverride(TRUE);
			}
			else if (params[num] == "SM")
			{
				newSet->setSmart(TRUE);
			}
			else if (params[num] == "DM")
			{
				newSet->setMouselookDisable(TRUE);
			}
			else if (params[num] == "**")
			{
				mDefaultSet = newSet;
				mCurrentSet = newSet;
			}
			else
			{
				LL_WARNS("AOEngine") << "Unknown AO set option " << params[num] << LL_ENDL;
			}
		}

		if (gInventory.isCategoryComplete(currentCategory->getUUID()))
		{
			LL_DEBUGS("AOEngine") << "Set " << params[0] << " is complete, reading states ..." << LL_ENDL;

			LLInventoryModel::cat_array_t* stateCategories;
			gInventory.getDirectDescendentsOf(currentCategory->getUUID(), stateCategories, items);
			newSet->setComplete(TRUE);

			for (S32 state_index = 0; state_index < stateCategories->size(); ++state_index)
			{
				std::vector<std::string> state_params;
				LLStringUtil::getTokens(stateCategories->at(state_index)->getName(), state_params, ":");
				std::string stateName = state_params[0];

				LLAOSet::AOState* state = newSet->getStateByName(stateName);
				if (!state)
				{
					LL_WARNS("AOEngine") << "Unknown state " << stateName << ". Skipping." << LL_ENDL;
					continue;
				}
				LL_DEBUGS("AOEngine") << "Reading state " << stateName << LL_ENDL;

				state->mInventoryUUID = stateCategories->at(state_index)->getUUID();
				for (U32 num = 1; num < state_params.size(); ++num)
				{
					if (state_params[num] == "CY")
					{
						state->mCycle = TRUE;
						LL_DEBUGS("AOEngine") << "Cycle on" << LL_ENDL;
					}
					else if (state_params[num] == "RN")
					{
						state->mRandom = TRUE;
						LL_DEBUGS("AOEngine") << "Random on" << LL_ENDL;
					}
					else if (state_params[num].substr(0, 2) == "CT")
					{
						LLStringUtil::convertToS32(state_params[num].substr(2, state_params[num].size() - 2), state->mCycleTime);
						LL_DEBUGS("AOEngine") << "Cycle Time specified:" << state->mCycleTime << LL_ENDL;
					}
					else
					{
						LL_WARNS("AOEngine") << "Unknown AO set option " << state_params[num] << LL_ENDL;
					}
				}

				if (!gInventory.isCategoryComplete(state->mInventoryUUID))
				{
					LL_DEBUGS("AOEngine") << "State category " << stateName << " is incomplete, fetching descendents" << LL_ENDL;
					gInventory.fetchDescendentsOf(state->mInventoryUUID);
					allComplete = FALSE;
					newSet->setComplete(FALSE);
					continue;
				}
				reloadStateAnimations(state);
			}
		}
		else
		{
			LL_DEBUGS("AOEngine") << "Set " << params[0] << " is incomplete, fetching descendents" << LL_ENDL;
			gInventory.fetchDescendentsOf(currentCategory->getUUID());
		}
	}

	if (allComplete)
	{
		mEnabled = gSavedPerAccountSettings.getBOOL("UseAO");

		if (!mCurrentSet && !mSets.empty())
		{
			LL_DEBUGS("AOEngine") << "No default set defined, choosing the first in the list." << LL_ENDL;
			selectSet(mSets[0]);
		}

		mTimerCollection.setInventoryTimer(false);
		mTimerCollection.setSettingsTimer(true);

		LL_INFOS("AOEngine") << "sending update signal" << LL_ENDL;
		mUpdatedSignal();
		enable(mEnabled);
	}
}

void LLAOEngine::reload(bool aFromTimer)
{
	BOOL wasEnabled = mEnabled;

	mTimerCollection.setReloadTimer(false);

	if (wasEnabled)
	{
		enable(false);
	}

	gAgent.stopCurrentAnimations();
	mLastOverriddenMotion = ANIM_AGENT_STAND;

	clear(aFromTimer);
	mAOFolder.setNull();
	mTimerCollection.setInventoryTimer(true);
	tick();

	if (wasEnabled)
	{
		enable(false);
	}
}

LLAOSet* LLAOEngine::getSetByName(const std::string& name) const
{
	LLAOSet* found = NULL;
	for (U32 index = 0; index < mSets.size(); ++index)
	{
		if (mSets[index]->getName().compare(name) == 0)
		{
			found = mSets[index];
			break;
		}
	}
	return found;
}

const std::string LLAOEngine::getCurrentSetName() const
{
	if(mCurrentSet)
	{
		return mCurrentSet->getName();
	}
	return LLStringUtil::null;
}

const LLAOSet* LLAOEngine::getDefaultSet() const
{
	return mDefaultSet;
}

void LLAOEngine::selectSet(LLAOSet* set)
{
	if (mEnabled && mCurrentSet)
	{
		LLAOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastOverriddenMotion);
		if (state)
		{
			gAgent.sendAnimationRequest(state->mCurrentAnimationID, ANIM_REQUEST_STOP);
			state->mCurrentAnimationID.setNull();
			mCurrentSet->stopTimer();
		}
	}

	mCurrentSet = set;
	mSetChangedSignal(mCurrentSet->getName());

	if (mEnabled)
	{
		LL_DEBUGS("AOEngine") << "enabling with motion " << gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
		gAgent.sendAnimationRequest(override(mLastMotion, TRUE), ANIM_REQUEST_START);
	}
}

LLAOSet* LLAOEngine::selectSetByName(const std::string& name)
{
	LLAOSet* set = getSetByName(name);
	if (set)
	{
		selectSet(set);
		return set;
	}
	LL_WARNS("AOEngine") << "Could not find AO set " << name << LL_ENDL;
	return nullptr;
}

const std::vector<LLAOSet*> LLAOEngine::getSetList() const
{
	return mSets;
}

void LLAOEngine::saveSet(const LLAOSet* set)
{
	if (!set) return;

	std::string setParams=set->getName();
	if (set->getSitOverride())
	{
		setParams += ":SO";
	}
	if (set->getSmart())
	{
		setParams += ":SM";
	}
	if (set->getMouselookDisable())
	{
		setParams += ":DM";
	}
	if (set == mDefaultSet)
	{
		setParams += ":**";
	}

	rename_category(&gInventory, set->getInventoryUUID(), setParams);

	LL_INFOS("AOEngine") << "sending update signal" << LL_ENDL;
	mUpdatedSignal();
}

bool LLAOEngine::renameSet(LLAOSet* set, const std::string& name)
{
	if (name.empty() || name.find(':') != std::string::npos)
	{
		return false;
	}
	set->setName(name);
	set->setDirty(true);

	return true;
}

void LLAOEngine::saveState(const LLAOSet::AOState* state)
{
	std::string stateParams = state->mName;
	F32 time = state->mCycleTime;
	if (time > 0.0f)
	{
		std::ostringstream timeStr;
		timeStr << ":CT" << state->mCycleTime;
		stateParams += timeStr.str();
	}
	if (state->mCycle)
	{
		stateParams += ":CY";
	}
	if (state->mRandom)
	{
		stateParams += ":RN";
	}

	rename_category(&gInventory, state->mInventoryUUID, stateParams);
}

void LLAOEngine::saveSettings()
{
	for (U32 index = 0; index < mSets.size(); ++index)
	{
		LLAOSet* set = mSets[index];
		if (set->getDirty())
		{
			saveSet(set);
			LL_INFOS("AOEngine") << "dirty set saved " << set->getName() << LL_ENDL;
			set->setDirty(FALSE);
		}

		for (S32 stateIndex = 0; stateIndex < LLAOSet::AOSTATES_MAX; ++stateIndex)
		{
			LLAOSet::AOState* state = set->getState(stateIndex);
			if (state->mDirty)
			{
				saveState(state);
				LL_INFOS("AOEngine") << "dirty state saved " << state->mName << LL_ENDL;
				state->mDirty = false;
			}
		}
	}
}

void LLAOEngine::inMouselook(const bool in_mouselook)
{
	if (mInMouselook == in_mouselook) return;

	mInMouselook = in_mouselook;

	if (!mCurrentSet || !mCurrentSet->getMouselookDisable())
	{
		return;
	}

	if (!mEnabled)
	{
		return;
	}

	if (mLastMotion != ANIM_AGENT_STAND)
	{
		return;
	}

	if (in_mouselook)
	{
		LLAOSet::AOState* state = mCurrentSet->getState(LLAOSet::Standing);
		if (!state)
		{
			return;
		}

		LLUUID animation = state->mCurrentAnimationID;
		if (animation.notNull())
		{
			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			state->mCurrentAnimationID.setNull();
			LL_DEBUGS("AOEngine") << " stopped animation " << animation << " in state " << state->mName << LL_ENDL;
		}
		gAgent.sendAnimationRequest(ANIM_AGENT_STAND, ANIM_REQUEST_START);
	}
	else
	{
		stopAllStandVariants();
		gAgent.sendAnimationRequest(override(ANIM_AGENT_STAND, TRUE), ANIM_REQUEST_START);
	}
}

void LLAOEngine::setDefaultSet(LLAOSet* set)
{
	mDefaultSet = set;
	for (U32 index = 0; index < mSets.size(); ++index)
	{
		mSets[index]->setDirty(TRUE);
	}
}

void LLAOEngine::setOverrideSits(LLAOSet* set, const bool yes)
{
	set->setSitOverride(yes);
	set->setDirty(TRUE);

	if (mCurrentSet != set)
	{
		return;
	}

	if (mLastMotion != ANIM_AGENT_SIT)
	{
		return;
	}

	if (yes)
	{
		stopAllSitVariants();
		gAgent.sendAnimationRequest(override(ANIM_AGENT_SIT, TRUE), ANIM_REQUEST_START);
	}
	else
	{
		LLAOSet::AOState* state = mCurrentSet->getState(LLAOSet::Sitting);
		if (!state)
		{
			return;
		}

		LLUUID animation = state->mCurrentAnimationID;
		if (animation.notNull())
		{
			gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			state->mCurrentAnimationID.setNull();
		}

		gAgent.sendAnimationRequest(ANIM_AGENT_SIT, ANIM_REQUEST_START);
	}
}

void LLAOEngine::setSmart(LLAOSet* set, const bool smart)
{
	set->setSmart(smart);
	set->setDirty(TRUE);
}

void LLAOEngine::setDisableStands(LLAOSet* set, const bool disable)
{
	set->setMouselookDisable(disable);
	set->setDirty(true);

	if (mCurrentSet != set)
	{
		return;
	}

	// make sure an update happens if needed
	mInMouselook = !gAgentCamera.cameraMouselook();
	inMouselook(!mInMouselook);
}

void LLAOEngine::setCycle(LLAOSet::AOState* state, const bool cycle)
{
	state->mCycle = cycle;
	state->mDirty = true;
}

void LLAOEngine::setRandomize(LLAOSet::AOState* state, const bool randomize)
{
	state->mRandom = randomize;
	state->mDirty = true;
}

void LLAOEngine::setCycleTime(LLAOSet::AOState* state, F32 time)
{
	state->mCycleTime = time;
	state->mDirty = true;
}

void LLAOEngine::tick()
{
	if (!isAgentAvatarValid()) return;

	LLUUID const& category_id = gInventory.findCategoryUUIDForNameInRoot(ROOT_AO_FOLDER, true, gInventory.getRootFolderID());

	if (category_id.notNull())
	{
		mAOFolder = category_id;
		LL_INFOS("AOEngine") << "AO basic folder structure intact." << LL_ENDL;
		update();
	}
}

bool LLAOEngine::importNotecard(const LLInventoryItem* item)
{
	if (item)
	{
		LL_INFOS("AOEngine") << "importing AO notecard: " << item->getName() << LL_ENDL;
		if (getSetByName(item->getName()))
		{
			LLNotificationsUtil::add("AOImportSetAlreadyExists", LLSD());
			return false;
		}

		if (!gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE) && !gAgent.isGodlike())
		{
			LLNotificationsUtil::add("AOImportPermissionDenied", LLSD());
			return false;
		}

		if (item->getAssetUUID().notNull())
		{
			mImportSet = new LLAOSet(item->getParentUUID());
			if (!mImportSet)
			{
				LLNotificationsUtil::add("AOImportCreateSetFailed", LLSD());
				return false;
			}
			mImportSet->setName(item->getName());

			LLUUID* newUUID = new LLUUID(item->getAssetUUID());
			const LLHost sourceSim = LLHost();

			gAssetStorage->getInvItemAsset(
				sourceSim,
				gAgent.getID(),
				gAgent.getSessionID(),
				item->getPermissions().getOwner(),
				LLUUID::null,
				item->getUUID(),
				item->getAssetUUID(),
				item->getType(),
				&onNotecardLoadComplete,
				(void*) newUUID,
				TRUE);

			return true;
		}
	}
	return false;
}

// static
void LLAOEngine::onNotecardLoadComplete(LLVFS* vfs, const LLUUID& assetUUID, LLAssetType::EType type,
											void* userdata, S32 status, LLExtStat extStatus)
{
	if (status != LL_ERR_NOERR)
	{
		// AOImportDownloadFailed
		LLNotificationsUtil::add("AOImportDownloadFailed", LLSD());
		// NULL tells the importer to cancel all operations and free the import set memory
		LLAOEngine::instance().parseNotecard(nullptr);
		return;
	}
	LL_DEBUGS("AOEngine") << "Downloading import notecard complete." << LL_ENDL;

	S32 notecardSize = vfs->getSize(assetUUID, type);
	auto buffer = std::make_unique<char[]>(notecardSize + 1);
	buffer[notecardSize] = '\0';
	S32 ret = vfs->getData(assetUUID, type, reinterpret_cast<U8*>(buffer.get()), 0, notecardSize);
	if (ret > 0)
	{
		LLAOEngine::instance().parseNotecard(std::move(buffer));
	}
	else
	{
		LLAOEngine::instance().parseNotecard(nullptr);
	}
}

void LLAOEngine::parseNotecard(std::unique_ptr<char[]>&& buffer)
{
	LL_DEBUGS("AOEngine") << "parsing import notecard" << LL_ENDL;

	bool isValid = false;

	if (!buffer)
	{
		LL_WARNS("AOEngine") << "buffer==NULL - aborting import" << LL_ENDL;
		// NOTE: cleanup is always the same, needs streamlining
		delete mImportSet;
		mImportSet = nullptr;
		mUpdatedSignal();
		return;
	}

	std::string text(buffer.get());

	std::vector<std::string> lines;
	LLStringUtil::getTokens(text, lines, "\n");

	S32 found = -1;
	for (U32 index = 0; index < lines.size(); ++index)
	{
		if (lines[index].find("Text length ") == 0)
		{
			found = index;
			break;
		}
	}

	if (found == -1)
	{
		LLNotificationsUtil::add("AOImportNoText", LLSD());
		delete mImportSet;
		mImportSet = nullptr;
		mUpdatedSignal();
		return;
	}

	LLViewerInventoryCategory* importCategory = gInventory.getCategory(mImportSet->getInventoryUUID());
	if (!importCategory)
	{
		LLNotificationsUtil::add("AOImportNoFolder", LLSD());
		delete mImportSet;
		mImportSet = 0;
		mUpdatedSignal();
		return;
	}

	std::map<std::string, LLUUID> animationMap;
	LLInventoryModel::cat_array_t* dummy;
	LLInventoryModel::item_array_t* items;

	gInventory.getDirectDescendentsOf(mImportSet->getInventoryUUID(), dummy, items);
	for (U32 index = 0; index < items->size(); ++index)
	{
		const auto& inv_item = items->at(index);
		animationMap[inv_item->getName()] = inv_item->getUUID();
		LL_DEBUGS("AOEngine")	<<	"animation " << inv_item->getName() <<
						" has inventory UUID " << animationMap[inv_item->getName()] << LL_ENDL;
	}

	// [ State ]Anim1|Anim2|Anim3
	for (U32 index = found + 1; index < lines.size(); ++index)
	{
		std::string line = lines[index];

		// cut off the trailing } of a notecard asset's text portion in the last line
		if (index == lines.size() - 1)
		{
			line = line.substr(0, line.size() - 1);
		}

		LLStringUtil::trim(line);

		if (line.empty() || line[0] == '#') continue;

		if (line.find('[') != 0)
		{
			LLSD args;
			args["LINE"] = (S32)index;
			LLNotificationsUtil::add("AOImportNoStatePrefix", args);
			continue;
		}

		if (line.find(']') == std::string::npos)
		{
			LLSD args;
			args["LINE"] = (S32)index;
			LLNotificationsUtil::add("AOImportNoValidDelimiter", args);
			continue;
		}
		U32 endTag = line.find(']');

		std::string stateName = line.substr(1, endTag - 1);
		LLStringUtil::trim(stateName);

		LLAOSet::AOState* newState = mImportSet->getStateByName(stateName);
		if (!newState)
		{
			LLSD args;
			args["NAME"] = stateName;
			LLNotificationsUtil::add("AOImportStateNameNotFound", args);
			continue;
		}

		std::string animationLine = line.substr(endTag + 1);
		std::vector<std::string> animationList;
		LLStringUtil::getTokens(animationLine, animationList, "|,");

		for (U32 animIndex = 0; animIndex < animationList.size(); ++animIndex)
		{
			LLAOSet::AOAnimation animation;
			animation.mName = animationList[animIndex];
			animation.mInventoryUUID = animationMap[animation.mName];
			if (animation.mInventoryUUID.isNull())
			{
				LLSD args;
				args["NAME"] = animation.mName;
				LLNotificationsUtil::add("AOImportAnimationNotFound", args);
				continue;
			}
			animation.mSortOrder = animIndex;
			newState->mAnimations.push_back(animation);
			isValid = true;
		}
	}

	if (!isValid)
	{
		LLNotificationsUtil::add("AOImportInvalid", LLSD());
		// NOTE: cleanup is always the same, needs streamlining
		delete mImportSet;
		mImportSet = nullptr;
		mUpdatedSignal();
		return;
	}

	mTimerCollection.setImportTimer(true);
	mImportRetryCount = 0;
	processImport(false);
}

void LLAOEngine::processImport(bool aFromTimer)
{
	if (mImportCategory.isNull())
	{
		mImportCategory = addSet(mImportSet->getName(), false);
		if (mImportCategory.isNull())
		{
			mImportRetryCount++;
			if (mImportRetryCount == 5)
			{
				// NOTE: cleanup is the same as at the end of this function. Needs streamlining.
				mTimerCollection.setImportTimer(false);
				delete mImportSet;
				mImportSet = NULL;
				mImportCategory.setNull();
				mUpdatedSignal();
				LLSD args;
				args["NAME"] = mImportSet->getName();
				LLNotificationsUtil::add("AOImportAbortCreateSet", args);
			}
			else
			{
				LLSD args;
				args["NAME"] = mImportSet->getName();
				LLNotificationsUtil::add("AOImportRetryCreateSet", args);
			}
			return;
		}
		mImportSet->setInventoryUUID(mImportCategory);
	}

	bool allComplete = true;
	for (S32 index = 0; index < LLAOSet::AOSTATES_MAX; ++index)
	{
		LLAOSet::AOState* state = mImportSet->getState(index);
		if (state->mAnimations.size())
		{
			allComplete = false;
			LL_DEBUGS("AOEngine") << "state " << state->mName << " still has animations to link." << LL_ENDL;

			for (S32 animationIndex = state->mAnimations.size() - 1; animationIndex >= 0; --animationIndex)
			{
				LL_DEBUGS("AOEngine") << "linking animation " << state->mAnimations[animationIndex].mName << LL_ENDL;
				if (createAnimationLink(mImportSet, state, gInventory.getItem(state->mAnimations[animationIndex].mInventoryUUID)))
				{
					LL_DEBUGS("AOEngine")	<< "link success, size "<< state->mAnimations.size() << ", removing animation "
								<< (*(state->mAnimations.begin() + animationIndex)).mName << " from import state" << LL_ENDL;
					state->mAnimations.erase(state->mAnimations.begin() + animationIndex);
					LL_DEBUGS("AOEngine") << "deleted, size now: " << state->mAnimations.size() << LL_ENDL;
				}
				else
				{
					LLSD args;
					args["NAME"] = state->mAnimations[animationIndex].mName;
					LLNotificationsUtil::add("AOImportLinkFailed", args);
				}
			}
		}
	}

	if (allComplete)
	{
		mTimerCollection.setImportTimer(false);
		mOldImportSets.push_back(mImportSet);
		mImportSet = nullptr;
		mImportCategory.setNull();
		reload(aFromTimer);
	}
}

const LLUUID& LLAOEngine::getAOFolder() const
{
	return mAOFolder;
}

void LLAOEngine::onRegionChange()
{
	// do nothing if the AO is off
	if (!mEnabled) return;

	// catch errors without crashing
	if (!mCurrentSet)
	{
		LL_DEBUGS("AOEngine") << "Current set was NULL" << LL_ENDL;
		return;
	}

	// sitting needs special attention
	if (mCurrentSet->getMotion() == ANIM_AGENT_SIT)
	{
		// do nothing if sit overrides was disabled
		if (!mCurrentSet->getSitOverride())
		{
			return;
		}

		// do nothing if the last overridden motion wasn't a sit.
		// happens when sit override is enabled but there were no
		// sit animations added to the set yet
		if (mLastOverriddenMotion != ANIM_AGENT_SIT)
		{
			return;
		}

		// do nothing if smart sit is enabled because we have no
		// animation running from the AO
		if (mCurrentSet->getSmart())
		{
			return;
		}
	}

	// restart current animation on region crossing
	gAgent.sendAnimationRequest(mLastMotion, ANIM_REQUEST_START);
}

// ----------------------------------------------------

LLAOSitCancelTimer::LLAOSitCancelTimer()
:	LLEventTimer(0.1f),
	mTickCount(0)
{
	mEventTimer.stop();
}

void LLAOSitCancelTimer::oneShot()
{
	mTickCount = 0;
	mEventTimer.start();
}

void LLAOSitCancelTimer::stop()
{
	mEventTimer.stop();
}

BOOL LLAOSitCancelTimer::tick()
{
	mTickCount++;
	LLAOEngine::instance().checkSitCancel();
	if (mTickCount == 10)
	{
		mEventTimer.stop();
	}
	return FALSE;
}

// ----------------------------------------------------

LLAOTimerCollection::LLAOTimerCollection()
:	LLEventTimer(INVENTORY_POLLING_INTERVAL),
	mInventoryTimer(true),
	mSettingsTimer(false),
	mReloadTimer(false),
	mImportTimer(false)
{
	updateTimers();
}

BOOL LLAOTimerCollection::tick()
{
	if (mInventoryTimer)
	{
		LL_DEBUGS("AOEngine") << "Inventory timer tick()" << LL_ENDL;
		LLAOEngine::instance().tick();
	}
	if (mSettingsTimer)
	{
		LL_DEBUGS("AOEngine") << "Settings timer tick()" << LL_ENDL;
		LLAOEngine::instance().saveSettings();
	}
	if (mReloadTimer)
	{
		LL_DEBUGS("AOEngine") << "Reload timer tick()" << LL_ENDL;
		LLAOEngine::instance().reload(true);
	}
	if (mImportTimer)
	{
		LL_DEBUGS("AOEngine") << "Import timer tick()" << LL_ENDL;
		LLAOEngine::instance().processImport(true);
	}

	// always return FALSE or the LLEventTimer will be deleted -> crash
	return FALSE;
}

void LLAOTimerCollection::setInventoryTimer(const bool enable)
{
	mInventoryTimer = enable;
	updateTimers();
}

void LLAOTimerCollection::setSettingsTimer(const bool enable)
{
	mSettingsTimer = enable;
	updateTimers();
}

void LLAOTimerCollection::setReloadTimer(const bool enable)
{
	mReloadTimer = enable;
	updateTimers();
}

void LLAOTimerCollection::setImportTimer(const bool enable)
{
	mImportTimer = enable;
	updateTimers();
}

void LLAOTimerCollection::updateTimers()
{
	if (!mInventoryTimer && !mSettingsTimer && !mReloadTimer && !mImportTimer)
	{
		LL_DEBUGS("AOEngine") << "no timer needed, stopping internal timer." << LL_ENDL;
		mEventTimer.stop();
	}
	else
	{
		LL_DEBUGS("AOEngine") << "timer needed, starting internal timer." << LL_ENDL;
		mEventTimer.start();
	}
}
