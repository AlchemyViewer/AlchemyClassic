/**
 * @file aoengine.h
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

#ifndef LL_AOENGINE_H
#define LL_AOENGINE_H

#include <boost/signals2.hpp>

#include "llaoset.h"

#include "llassettype.h"
#include "lleventtimer.h"

#include "llextendedstatus.h"
#include "llsingleton.h"

class LLAOTimerCollection : public LLEventTimer
{
public:
	LLAOTimerCollection();
	~LLAOTimerCollection() {}

	BOOL tick() override;

	void setInventoryTimer(const bool enable);
	void setSettingsTimer(const bool enable);
	void setReloadTimer(const bool enable);
	void setImportTimer(const bool enable);

protected:
	void updateTimers();

	bool mInventoryTimer;
	bool mSettingsTimer;
	bool mReloadTimer;
	bool mImportTimer;
};

// ----------------------------------------------------

class LLAOSitCancelTimer : public LLEventTimer
{
public:
	LLAOSitCancelTimer();
	~LLAOSitCancelTimer() {}

	void oneShot();
	void stop();

	BOOL tick() override;

protected:
	S32 mTickCount;
};

// ----------------------------------------------------

class LLInventoryItem;
class LLVFS;

class LLAOEngine : public LLSingleton<LLAOEngine>
{
	LLSINGLETON(LLAOEngine);
	~LLAOEngine();

public:
	typedef enum e_cycle_mode
	{
		CycleAny,
		CycleNext,
		CyclePrevious
	} eCycleMode;

	void enable(const bool enable);
	const LLUUID override(const LLUUID& motion, const bool start);
	void tick();
	void update();
	void reload(const bool reload);
	void reloadStateAnimations(LLAOSet::AOState* state);
	void clear(const bool clear);

	const LLUUID& getAOFolder() const;

	LLUUID addSet(const std::string& name, bool reload = true);
	bool removeSet(LLAOSet* set);

	bool addAnimation(const LLAOSet* set, LLAOSet::AOState* state,
					  const LLInventoryItem* item, bool reload = true);
	bool removeAnimation(const LLAOSet* set, LLAOSet::AOState* state, S32 index);
	void checkSitCancel();
	void checkBelowWater(const bool under);

	bool importNotecard(const LLInventoryItem* item);
	void processImport(const bool process);

	bool swapWithPrevious(LLAOSet::AOState* state, S32 index);
	bool swapWithNext(LLAOSet::AOState* state, S32 index);

	void cycleTimeout(const LLAOSet* set);
	void cycle(eCycleMode cycleMode);

	void inMouselook(const bool in_mouselook);
	void selectSet(LLAOSet* set);
	LLAOSet* selectSetByName(const std::string& name);
	LLAOSet* getSetByName(const std::string& name) const;

	// callback from LLAppViewer
	static void onLoginComplete();

	const std::vector<LLAOSet*> getSetList() const;
	const std::string getCurrentSetName() const;
	const LLAOSet* getDefaultSet() const;
	bool renameSet(LLAOSet* set, const std::string& name);

	void setDefaultSet(LLAOSet* set);
	void setOverrideSits(LLAOSet* set, const bool override_sits);
	void setSmart(LLAOSet* set, const bool smart);
	void setDisableStands(LLAOSet* set, const bool disable);
	void setCycle(LLAOSet::AOState* set, const bool cycle);
	void setRandomize(LLAOSet::AOState* state, const bool randomize);
	void setCycleTime(LLAOSet::AOState* state, const F32 time);

	void saveSettings();

	typedef boost::signals2::signal<void ()> updated_signal_t;
	boost::signals2::connection setReloadCallback(const updated_signal_t::slot_type& cb)
	{
		return mUpdatedSignal.connect(cb);
	};

	typedef boost::signals2::signal<void (const LLUUID&)> animation_changed_signal_t;
	boost::signals2::connection setAnimationChangedCallback(const animation_changed_signal_t::slot_type& cb)
	{
		return mAnimationChangedSignal.connect(cb);
	};
	
	typedef boost::signals2::signal<void (const std::string&)> set_changed_signal_t;
	boost::signals2::connection setSetChangedCallback(const set_changed_signal_t::slot_type& cb)
	{
		return mSetChangedSignal.connect(cb);
	}

protected:
	void init();

	void setLastMotion(const LLUUID& motion);
	void setLastOverriddenMotion(const LLUUID& motion);
	void setStateCycleTimer(const LLAOSet::AOState* state);

	void stopAllStandVariants();
	void stopAllSitVariants();

	bool foreignAnimations(const LLUUID& seat);
	const LLUUID& mapSwimming(const LLUUID& motion) const;

	void updateSortOrder(LLAOSet::AOState* state);
	void saveSet(const LLAOSet* set);
	void saveState(const LLAOSet::AOState* state);

	bool createAnimationLink(const LLAOSet* set, LLAOSet::AOState* state, const LLInventoryItem* item);
	bool findForeignItems(const LLUUID& uuid) const;
	void purgeFolder(const LLUUID& uuid) const;

	void onRegionChange();

	void onToggleAOControl();
	static void onNotecardLoadComplete(LLVFS* vfs, const LLUUID& assetUUID, LLAssetType::EType type,
												void* userdata, S32 status, LLExtStat extStatus);
	void parseNotecard(std::unique_ptr<char[]>&& buffer);

	updated_signal_t mUpdatedSignal;
	animation_changed_signal_t mAnimationChangedSignal;
	set_changed_signal_t mSetChangedSignal;

	LLAOTimerCollection mTimerCollection;
	LLAOSitCancelTimer mSitCancelTimer;

	bool mEnabled;
	bool mInMouselook;
	bool mUnderWater;

	LLUUID mAOFolder;
	LLUUID mLastMotion;
	LLUUID mLastOverriddenMotion;

	std::vector<LLAOSet*> mSets;
	std::vector<LLAOSet*> mOldSets;
	LLAOSet* mCurrentSet;
	LLAOSet* mDefaultSet;
	
	LLAOSet* mImportSet;
	std::vector<LLAOSet*> mOldImportSets;
	LLUUID mImportCategory;
	S32 mImportRetryCount;

	boost::signals2::connection mRegionChangeConnection;
};

#endif // LL_AOENGINE_H
