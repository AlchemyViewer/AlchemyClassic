/** 
 * @file _thread.h
 * @brief thread type abstraction
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
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

#ifndef LLCOREINT_THREAD_H_
#define LLCOREINT_THREAD_H_

#include "linden_common.h"

#include <boost/chrono.hpp>
#include <boost/thread.hpp>

#include "llwin32headerslean.h"

#include "_refcounted.h"

namespace LLCoreInt
{

class HttpThread : public RefCounted
{
private:
	HttpThread() = delete;								// Not defined
	HttpThread& operator=(const HttpThread &) = delete;	// Not defined

	void at_exit()
		{
			// the thread function has exited so we need to release our reference
			// to ourself so that we will be automagically cleaned up.
			release();
		}

	void run()
		{ // THREAD CONTEXT

			// Take out additional reference for the at_exit handler
			addRef();
			boost::this_thread::at_thread_exit(boost::bind(&HttpThread::at_exit, this));

			// run the thread function
			mThreadFunc(this);

		} // THREAD CONTEXT

protected:
	virtual ~HttpThread()
		{
			if(joinable())
			{
				bool joined = false;
				S32 counter = 0;
				const S32 MAX_WAIT = 600;
				while (counter < MAX_WAIT)
				{
					// Try to join for a tenth of a second
					if (timedJoin(100))
					{
						joined = true;
						break;
					}
					yield();
					counter++;
				}

				if (!joined)
				{
					// Failed to join, expect problems ahead so do a hard termination.
					cancel();

					LL_WARNS() << "Destroying HttpThread with running thread.  Expect problems."
									   << LL_ENDL;
				}
			}
			delete mThread;
		}

public:
	/// Constructs a thread object for concurrent execution but does
	/// not start running.  Caller receives on refcount on the thread
	/// instance.  If the thread is started, another will be taken
	/// out for the exit handler.
	explicit HttpThread(std::function<void (HttpThread *)> threadFunc)
		: RefCounted(true), // implicit reference
		  mThreadFunc(threadFunc)
		{
			// this creates a boost thread that will call HttpThread::run on this instance
			// and pass it the threadfunc callable...
			std::function<void()> f = std::bind(&HttpThread::run, this);

			mThread = new boost::thread(f);
		}

	inline void join()
		{
			mThread->join();
		}

	inline bool timedJoin(S32 millis)
		{
			return mThread->try_join_for(boost::chrono::milliseconds(millis));
		}

	inline bool joinable() const
		{
			return mThread->joinable();
		}

	inline void yield()
		{
			boost::this_thread::yield();
		}

	// A very hostile method to force a thread to quit
	inline void cancel()
		{
			boost::thread::native_handle_type thread(mThread->native_handle());
#if		LL_WINDOWS
			TerminateThread(thread, 0);
#else
			pthread_cancel(thread);
#endif
		}
	
private:
	std::function<void(HttpThread *)> mThreadFunc;
	boost::thread * mThread;
}; // end class HttpThread

} // end namespace LLCoreInt

#endif // LLCOREINT_THREAD_H_


