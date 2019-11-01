/** 
 * @file llmutex.h
 * @brief Base classes for mutex and condition handling.
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

#ifndef LL_LLMUTEX_H
#define LL_LLMUTEX_H

#include "stdtypes.h"

#include <shared_mutex>
#include <mutex>
#include <condition_variable>
#include <thread>

//============================================================================

#define MUTEX_DEBUG (LL_DEBUG || LL_RELEASE_WITH_DEBUG_INFO)

#if MUTEX_DEBUG
#include <map>
#endif

class LL_COMMON_API LLMutex
{
public:
	LLMutex() :
	 mCount(0),
	 mLockingThread()
	{
	}

	void lock();		// blocks
	bool trylock();		// non-blocking, returns true if lock held.
	void unlock();		// undefined behavior when called on mutex not being held
	bool isLocked(); 	// non-blocking, but does do a lock/unlock so not free
	bool isSelfLocked(); //return true if locked in a same thread
	std::thread::id lockingThread() const; //get ID of locking thread
	
protected:
#if LL_WINDOWS // This is an SRWLock in win32
	std::shared_mutex			mMutex;
#else
	std::mutex			mMutex;
#endif
	mutable U32			mCount;
	mutable std::thread::id			mLockingThread;
	
#if MUTEX_DEBUG
	std::map<std::thread::id, BOOL> mIsLocked;
#endif
};

// Actually a condition/mutex pair (since each condition needs to be associated with a mutex).
class LL_COMMON_API LLCondition : public LLMutex
{
public:
	void wait()
	{
		std::unique_lock<decltype(mMutex)> lock(mMutex);
		mCond.wait(lock);
	}

	void signal()
	{
		mCond.notify_one();
	}

	void broadcast()
	{
		mCond.notify_all();
	}
	
protected:
	std::condition_variable_any mCond;
};

class LLMutexLock
{
public:
	LLMutexLock(LLMutex* mutex)
	{
		mMutex = mutex;
		
		if(mMutex)
			mMutex->lock();
	}
	~LLMutexLock()
	{
		if(mMutex)
			mMutex->unlock();
	}
private:
	LLMutex* mMutex;
};

//============================================================================

// Scoped locking class similar in function to LLMutexLock but uses
// the trylock() method to conditionally acquire lock without
// blocking.  Caller resolves the resulting condition by calling
// the isLocked() method and either punts or continues as indicated.
//
// Mostly of interest to callers needing to avoid stalls who can
// guarantee another attempt at a later time.

class LLMutexTrylock
{
public:
	LLMutexTrylock(LLMutex* mutex)
		: mMutex(mutex),
		mLocked(false)
	{
		if (mMutex)
			mLocked = mMutex->trylock();
	}

	LLMutexTrylock(LLMutex* mutex, U32 aTries, U32 delay_ms = 10U)
		: mMutex(mutex),
		mLocked(false)
	{
		if (!mMutex)
			return;

		for (U32 i = 0; i < aTries; ++i)
		{
			mLocked = mMutex->trylock();
			if (mLocked)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
		}
	}

	~LLMutexTrylock()
	{
		if (mMutex && mLocked)
			mMutex->unlock();
	}

	bool isLocked() const
	{
		return mLocked;
	}
	
private:
	LLMutex*	mMutex;
	bool		mLocked;
};

#endif // LL_LLMUTEX_H
