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
	mLockingThread = LLThread::currentID();
}

void LLMutex::unlock()
{
	LLMutexImpl::unlock();
	mLockingThread = boost::thread::id();
}

// Returns true if lock was obtained successfully.
bool LLMutex::try_lock()
{
	if (!LLMutexImpl::try_lock())
		return false;
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
