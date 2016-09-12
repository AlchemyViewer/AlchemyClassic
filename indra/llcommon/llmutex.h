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

#define AL_BOOST_MUTEX 1

#if AL_BOOST_MUTEX
#include "fix_macros.h"
#define BOOST_SYSTEM_NO_DEPRECATED
#include <boost/thread/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition_variable.hpp>
typedef boost::recursive_mutex LLMutexImpl;
typedef boost::condition_variable_any LLConditionImpl;
#elif AL_STD_MUTEX
#include <mutex>
#include <condition_variable>
typedef std::recursive_mutex LLMutexImpl;
typedef std::condition_variable_any LLConditionImpl;
#else
#error Mutex definition required.
#endif

class LL_COMMON_API LLMutex : public LLMutexImpl
{
public:
	LLMutex() : LLMutexImpl(), mLockingThread() {}
	~LLMutex() {}

	void lock();	// blocks

	void unlock();

	// Returns true if lock was obtained successfully.
	bool try_lock();

	// Returns true if locked not by this thread
	bool isLocked();

	// Returns true if locked by this thread.
	bool isSelfLocked() const;

private:
	mutable boost::thread::id mLockingThread;
};

// Actually a condition/mutex pair (since each condition needs to be associated with a mutex).
class LL_COMMON_API LLCondition : public LLConditionImpl, public LLMutex
{
public:
	LLCondition() : LLConditionImpl(), LLMutex() {}
	~LLCondition() {}

	void wait() { LLConditionImpl::wait(*this); }
	void signal()		{ LLConditionImpl::notify_one(); }
	void broadcast()	{ LLConditionImpl::notify_all(); }
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
// the try_lock() method to conditionally acquire lock without
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
			mLocked = mMutex->try_lock();
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
#endif // LL_LLTHREAD_H
