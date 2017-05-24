// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llthread.cpp
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

#include "linden_common.h"
#include "llapr.h"

#include "llthread.h"
#include "llmutex.h"

#include "lltimer.h"
#include "lltrace.h"
#include "lltracethreadrecorder.h"

#include <chrono>


#ifdef LL_WINDOWS
const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void set_thread_name( DWORD dwThreadID, const char* threadName)
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		::RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR*)&info );
	}
	__except(EXCEPTION_CONTINUE_EXECUTION)
	{
	}
}
#endif


//----------------------------------------------------------------------------
// Usage:
// void run_func(LLThread* thread)
// {
// }
// LLThread* thread = new LLThread();
// thread->run(run_func);
// ...
// thread->setQuitting();
// while(!timeout)
// {
//   if (thread->isStopped())
//   {
//     delete thread;
//     break;
//   }
// }
// 
//----------------------------------------------------------------------------

LL_COMMON_API void assert_main_thread()
{
	static boost::thread::id s_thread_id = LLThread::currentID();
	if (LLThread::currentID() != s_thread_id)
	{
		LL_WARNS() << "Illegal execution from thread id " << LLThread::currentID()
			<< " outside main thread " << s_thread_id << LL_ENDL;
	}
}

void LLThread::runWrapper()
{
#ifdef LL_WINDOWS
	set_thread_name(-1, mName.c_str());
#endif

	// for now, hard code all LLThreads to report to single master thread recorder, which is known to be running on main thread
	mRecorder = std::make_unique<LLTrace::ThreadRecorder>(*LLTrace::get_master_thread_recorder());

	// Run the user supplied function
	run();

	//LL_INFOS() << "LLThread::staticRun() Exiting: " << threadp->mName << LL_ENDL;
	
	mRecorder.reset(nullptr);
	
	// We're done with the run function, this thread is done executing now.
	//NB: we are using this flag to sync across threads...we really need memory barriers here
	mStatus = STOPPED;
}

LLThread::LLThread(const std::string& name, apr_pool_t *poolp) :
	mPaused(FALSE),
	mName(name),
	mRunCondition(std::make_unique<LLCondition>()),
	mDataLock(std::make_unique<LLMutex>()),
	mStatus(STOPPED)
{
	// Thread creation probably CAN be paranoid about APR being initialized, if necessary
	if (poolp)
	{
		mIsLocalPool = FALSE;
		mAPRPoolp = poolp;
	}
	else
	{
		mIsLocalPool = TRUE;
		apr_pool_create(&mAPRPoolp, NULL); // Create a subpool for this thread
	}
	mLocalAPRFilePoolp = nullptr;
}


LLThread::~LLThread()
{
	shutdown();

	if (mLocalAPRFilePoolp)
	{
		delete mLocalAPRFilePoolp;
		mLocalAPRFilePoolp = nullptr;
	}
}

void LLThread::shutdown()
{
	// Warning!  If you somehow call the thread destructor from itself,
	// the thread will die in an unclean fashion!
	if (!isStopped())
	{
		// The thread isn't already stopped
		// First, set the flag that indicates that we're ready to die
		setQuitting();

		//LL_INFOS() << "LLThread::~LLThread() Killing thread " << mName << " Status: " << mStatus << LL_ENDL;
		// Now wait a bit for the thread to exit
		// It's unclear whether I should even bother doing this - this destructor
		// should never get called unless we're already stopped, really...
		U32 count = 0;
		constexpr U32 MAX_WAIT = 600;
		for (; count < MAX_WAIT; count++)
		{
			if (isStopped())
			{
				break;
			}
			yield();
		}

		if (count >= MAX_WAIT)
		{
			LL_WARNS() << "Failed to stop thread: \"" << mName << "\" with status: " << mStatus << " and id: " << mThread.get_id() << LL_ENDL;
		}
	}

	if (mThread.joinable())
	{
		try
		{
			bool joined = false;
			constexpr U32 MAX_WAIT = 100;
			for(U32 count = 0; count < MAX_WAIT; count++)
			{
				// Try to join for a tenth of a second
				if (mThread.try_join_for(boost::chrono::milliseconds(100)))
				{
					LL_INFOS() << "Successfully joined thread: \"" << mName << "\"" << LL_ENDL;
					joined = true;
					break;
				}
				yield();
			}

			if (!joined)
			{
				// This thread just wouldn't join, even though we gave it time
				LL_WARNS() << "Forcefully terminating thread: \"" << mName << "\" with id: " << mThread.get_id() << " before clean exit!" << LL_ENDL;
				// Put a stake in its heart.
				boost::thread::native_handle_type thread(mThread.native_handle());
#if LL_WINDOWS
				TerminateThread(thread, 0);
#else
				pthread_cancel(thread);
#endif
			}
		}
		catch (const boost::thread_interrupted&)
		{
			LL_WARNS() << "Failed to join thread: \"" << mName << "\" with id: " << mThread.get_id() << " and interrupted exception" << LL_ENDL;
		}
	}

	mRecorder.reset();

	if (mIsLocalPool && mAPRPoolp)
	{
		apr_pool_destroy(mAPRPoolp);
		mAPRPoolp = nullptr;
	}
}


void LLThread::start()
{
	llassert(isStopped());
	
	// Set thread state to running
	mStatus = RUNNING;

	try
	{
		mThread = boost::thread(std::bind(&LLThread::runWrapper, this));
	}
	catch (const boost::thread_resource_error& err)
	{
		mStatus = STOPPED;
		LL_WARNS() << "Failed to start thread: \"" << mName << "\" due to error: " << err.what() << LL_ENDL;
	}
}

//============================================================================
// Called from MAIN THREAD.

// Request that the thread pause/resume.
// The thread will pause when (and if) it calls checkPause()
void LLThread::pause()
{
	if (!mPaused)
	{
		// this will cause the thread to stop execution as soon as checkPause() is called
		mPaused = 1;		// Does not need to be atomic since this is only set/unset from the main thread
	}	
}

void LLThread::unpause()
{
	if (mPaused)
	{
		mPaused = 0;
	}

	wake(); // wake up the thread if necessary
}

// virtual predicate function -- returns true if the thread should wake up, false if it should sleep.
bool LLThread::runCondition(void)
{
	// by default, always run.  Handling of pause/unpause is done regardless of this function's result.
	return true;
}

//============================================================================
// Called from run() (CHILD THREAD).
// Stop thread execution if requested until unpaused.
void LLThread::checkPause()
{
	mDataLock->lock();

	// This is in a while loop because the pthread API allows for spurious wakeups.
	while(shouldSleep())
	{
		mDataLock->unlock();
		mRunCondition->wait(); // unlocks mRunCondition
		mDataLock->lock();
		// mRunCondition is locked when the thread wakes up
	}
	
 	mDataLock->unlock();
}

//============================================================================

void LLThread::setQuitting()
{
	mDataLock->lock();
	if (mStatus == RUNNING)
	{
		mStatus = QUITTING;
	}
	mDataLock->unlock();
	wake();
}

// static
boost::thread::id LLThread::currentID()
{
	return boost::this_thread::get_id();
}

// static
void LLThread::yield()
{
	boost::this_thread::yield();
}

void LLThread::wake()
{
	mDataLock->lock();
	if(!shouldSleep())
	{
		mRunCondition->signal();
	}
	mDataLock->unlock();
}

void LLThread::wakeLocked()
{
	if(!shouldSleep())
	{
		mRunCondition->signal();
	}
}

//============================================================================

//----------------------------------------------------------------------------

//static
LLMutex* LLThreadSafeRefCount::sMutex = nullptr;

//static
void LLThreadSafeRefCount::initThreadSafeRefCount()
{
	if (!sMutex)
	{
		sMutex = new LLMutex();
	}
}

//static
void LLThreadSafeRefCount::cleanupThreadSafeRefCount()
{
	delete sMutex;
	sMutex = nullptr;
}
	

//----------------------------------------------------------------------------

LLThreadSafeRefCount::LLThreadSafeRefCount() :
	mRef(0)
{
}

LLThreadSafeRefCount::LLThreadSafeRefCount(const LLThreadSafeRefCount& src)
{
	mRef = 0;
}

LLThreadSafeRefCount::~LLThreadSafeRefCount()
{ 
	if (mRef != 0)
	{
		LL_ERRS() << "deleting non-zero reference" << LL_ENDL;
	}
}

//============================================================================

LLResponder::~LLResponder()
{
}

//============================================================================
