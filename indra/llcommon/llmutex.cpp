// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llmutex.cpp
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
#include "llmutex.h"
#include "llthread.h"

void LLMutex::lock()	// blocks
{
	
	LLMutexImpl::lock();
	
#if MUTEX_DEBUG
	// Have to have the lock before we can access the debug info
	boost::thread::id id = LLThread::currentID();
	if (mIsLocked[id] != FALSE)
		LL_ERRS() << "Already locked in Thread: " << id << LL_ENDL;
	mIsLocked[id] = TRUE;
#endif

	mLockingThread = LLThread::currentID();
}

void LLMutex::unlock()
{
#if MUTEX_DEBUG
	// Access the debug info while we have the lock
	boost::thread::id id = LLThread::currentID();
	if (mIsLocked[id] != TRUE)
		LL_ERRS() << "Not locked in Thread: " << id << LL_ENDL;	
	mIsLocked[id] = FALSE;
#endif

	mLockingThread = boost::thread::id();
	LLMutexImpl::unlock();
}

// Returns true if lock was obtained successfully.
bool LLMutex::try_lock()
{
	if (!LLMutexImpl::try_lock())
	{
		return false;
	}
	
#if MUTEX_DEBUG
	// Have to have the lock before we can access the debug info
	boost::thread::id id = LLThread::currentID();
	if (mIsLocked[id] != FALSE)
		LL_ERRS() << "Already locked in Thread: " << id << LL_ENDL;
	mIsLocked[id] = TRUE;
#endif

	mLockingThread = LLThread::currentID();
	return true;
}

// Returns true if locked not by this thread
bool LLMutex::isLocked()
{
	if (isSelfLocked())
		return false;
	if (LLMutexImpl::try_lock())
	{
		LLMutexImpl::unlock();
		return false;
	}
	return true;
}

// Returns true if locked by this thread.
bool LLMutex::isSelfLocked() const
{
	return mLockingThread == LLThread::currentID();
}

//============================================================================
