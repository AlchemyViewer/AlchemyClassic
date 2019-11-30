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
#include "llexception.h"

#include <chrono>

using namespace std::chrono_literals;

#ifdef LL_WINDOWS
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void set_thread_name(const char* threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = (DWORD)-1;
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
	static std::thread::id s_thread_id = LLThread::currentID();
	if (LLThread::currentID() != s_thread_id)
	{
		LL_WARNS() << "Illegal execution from thread id " << LLThread::currentID()
			<< " outside main thread " << s_thread_id << LL_ENDL;
	}
}

//
// Handed to the APR thread creation function
//
void LLThread::threadRun()
{
#ifdef LL_WINDOWS
    set_thread_name(mName.c_str());
#endif

	// for now, hard code all LLThreads to report to single master thread recorder, which is known to be running on main thread
	mRecorder = std::make_unique<LLTrace::ThreadRecorder>(*LLTrace::get_master_thread_recorder());


    // Run the user supplied function
    do 
    {
        try
        {
            run();
        }
        catch (const LLContinueError &e)
        {
            LL_WARNS("THREAD") << "ContinueException on thread '" << mName <<
                "' reentering run(). Error what is: '" << e.what() << "'" << LL_ENDL;
            //output possible call stacks to log file.
            LLError::LLCallStacks::print();

            LOG_UNHANDLED_EXCEPTION("LLThread");
            continue;
        }
        break;

    } while (true);

    //LL_INFOS() << "LLThread::staticRun() Exiting: " << mName << LL_ENDL;

	mRecorder.reset(nullptr);

    // We're done with the run function, this thread is done executing now.
    //NB: we are using this flag to sync across threads...we really need memory barriers here
    // Todo: add LLMutex per thread instead of flag?
    // We are using "while (mStatus != STOPPED) {ms_sleep();}" everywhere.
    mStatus = STOPPED;
}

LLThread::LLThread(std::string name, apr_pool_t *poolp) :
    mPaused(false), 
    mNativeHandle(),
    mName(std::move(name)),
    mRunCondition(std::make_unique<LLCondition>()),
    mDataLock(std::make_unique<LLMutex>()),
    mStatus(STOPPED)
{
    mLocalAPRFilePoolp = nullptr;
}


LLThread::~LLThread()
{
    LLThread::shutdown();

    if (isCrashed())
    {
        LL_WARNS("THREAD") << "Destroying crashed thread named '" << mName << "'" << LL_ENDL;
    }

    if(mLocalAPRFilePoolp)
    {
        delete mLocalAPRFilePoolp ;
		mLocalAPRFilePoolp = nullptr;
    }
}

void LLThread::shutdown()
{
    if (isCrashed())
    {
        LL_WARNS("THREAD") << "Shutting down crashed thread named '" << mName << "'" << LL_ENDL;
    }
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
            // Sleep for a tenth of a second
            std::this_thread::sleep_for(100ms);
			yield();
		}

		if (count >= MAX_WAIT)
		{
			LL_WARNS() << "Failed to stop thread: \"" << mName << "\" with status: " << mStatus << " and id: " << mThread.get_id() << LL_ENDL;
		}
	}

    if (!isStopped())
    {
		LL_WARNS("THREAD") << "Forced to terminated thread: " << mName << LL_ENDL;
        // This thread just wouldn't stop, even though we gave it time
        //LL_WARNS() << "LLThread::~LLThread() exiting thread before clean exit!" << LL_ENDL;
        // Put a stake in its heart. (A very hostile method to force a thread to quit)
#if	LL_WINDOWS
		TerminateThread(mNativeHandle, 0);
#else
		pthread_cancel(mNativeHandle);
#endif
		mStatus = CRASHED;
	}

	mRecorder.reset(nullptr);
}


void LLThread::start()
{
    llassert(isStopped());
    
    // Set thread state to running
    mStatus = RUNNING;

	try
	{
		mThread = std::thread(std::bind(&LLThread::threadRun, this));
		mNativeHandle = mThread.native_handle();
		mThread.detach();
	}
	catch (const std::system_error& err)
	{
		mStatus = CRASHED;
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
        mPaused = true;        // Does not need to be atomic since this is only set/unset from the main thread
    }   
}

void LLThread::unpause()
{
    if (mPaused)
    {
        mPaused = false;
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
std::thread::id LLThread::currentID()
{
	return std::this_thread::get_id();
}

// static
void LLThread::yield()
{
    std::this_thread::yield();
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

LLThreadSafeRefCount::LLThreadSafeRefCount(const LLThreadSafeRefCount& src) :
	mRef(0)
{
}

LLThreadSafeRefCount::~LLThreadSafeRefCount()
{ 
    if (mRef != 0)
    {
		LL_ERRS() << "deleting referenced object mRef = " << mRef << LL_ENDL;
    }
}
