/** 
 * @file llthread.h
 * @brief Base classes for thread, mutex and condition handling.
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010-2013, Linden Research, Inc.
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

#ifndef LL_LLTHREAD_H
#define LL_LLTHREAD_H

#include "llapp.h"
#include "llapr.h"
#include "llmutex.h"
#include "llrefcount.h"

#include <boost/thread.hpp>
#include <boost/intrusive_ptr.hpp>

LL_COMMON_API void assert_main_thread();

namespace LLTrace
{
	class ThreadRecorder;
}

class LL_COMMON_API LLThread
{
private:
	friend class LLMutex;

public:
	typedef enum e_thread_status
	{
		STOPPED = 0,	// The thread is not running.  Not started, or has exited its run function
		RUNNING = 1,	// The thread is currently running
		QUITTING= 2 	// Someone wants this thread to quit
	} EThreadStatus;

	LLThread(const std::string& name, apr_pool_t *poolp = nullptr);
	virtual ~LLThread(); // Warning!  You almost NEVER want to destroy a thread unless it's in the STOPPED state.
	virtual void shutdown(); // stops the thread
	
	bool isQuitting() const { return (QUITTING == mStatus); }
	bool isStopped() const { return (STOPPED == mStatus); }
	
	static boost::thread::id currentID(); // Return ID of current thread
	static void yield(); // Static because it can be called by the main thread, which doesn't have an LLThread data structure.
	
public:
	// PAUSE / RESUME functionality. See source code for important usage notes.
	// Called from MAIN THREAD.
	void pause();
	void unpause();
	bool isPaused() { return isStopped() || mPaused == TRUE; }
	
	// Cause the thread to wake up and check its condition
	void wake();

	// Same as above, but to be used when the condition is already locked.
	void wakeLocked();

	// Called from run() (CHILD THREAD). Pause the thread if requested until unpaused.
	void checkPause();

	// this kicks off the apr thread
	void start(void);

	apr_pool_t *getAPRPool() { return mAPRPoolp; }
	LLVolatileAPRPool* getLocalAPRFilePool() { return mLocalAPRFilePoolp ; }

private:
	BOOL				mPaused;
	
	// static function passed to APR thread creation routine
	void runWrapper();

protected:
	std::string						mName;
	std::unique_ptr<LLCondition>	mRunCondition;
	std::unique_ptr<LLMutex>		mDataLock;

	apr_pool_t			*mAPRPoolp;
	boost::thread	mThread;
	BOOL				mIsLocalPool;
	EThreadStatus	mStatus;
	std::unique_ptr<LLTrace::ThreadRecorder> mRecorder;

	//a local apr_pool for APRFile operations in this thread. If it exists, LLAPRFile::sAPRFilePoolp should not be used.
	//Note: this pool is used by APRFile ONLY, do NOT use it for any other purposes.
	//      otherwise it will cause severe memory leaking!!! --bao
	LLVolatileAPRPool  *mLocalAPRFilePoolp ; 

	void setQuitting();
	
	// virtual function overridden by subclass -- this will be called when the thread runs
	virtual void run(void) = 0; 
	
	// virtual predicate function -- returns true if the thread should wake up, false if it should sleep.
	virtual bool runCondition(void);

	// Lock/Unlock Run Condition -- use around modification of any variable used in runCondition()
	inline void lockData();
	inline void unlockData();
	
	// This is the predicate that decides whether the thread should sleep.  
	// It should only be called with mDataLock locked, since the virtual runCondition() function may need to access
	// data structures that are thread-unsafe.
	bool shouldSleep(void) { return (mStatus == RUNNING) && (isPaused() || (!runCondition())); }

	// To avoid spurious signals (and the associated context switches) when the condition may or may not have changed, you can do the following:
	// mDataLock->lock();
	// if(!shouldSleep())
	//     mRunCondition->signal();
	// mDataLock->unlock();
};


void LLThread::lockData()
{
	mDataLock->lock();
}

void LLThread::unlockData()
{
	mDataLock->unlock();
}


//============================================================================

// Simple responder for self destructing callbacks
// Pure virtual class
class LL_COMMON_API LLResponder : public LLThreadSafeRefCount
{
protected:
	virtual ~LLResponder();
public:
	virtual void completed(bool success) = 0;
};

//============================================================================

extern LL_COMMON_API void assert_main_thread();

#endif // LL_LLTHREAD_H
