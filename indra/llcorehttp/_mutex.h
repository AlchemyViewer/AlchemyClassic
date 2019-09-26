/** 
 * @file _mutex.hpp
 * @brief mutex type abstraction
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

#ifndef LLCOREINT_MUTEX_H_
#define LLCOREINT_MUTEX_H_

#include <condition_variable>
#include <mutex>
#include <shared_mutex>

namespace LLCoreInt
{
// MUTEX TYPES

// unique mutex type
#if LL_WINDOWS
typedef std::shared_mutex HttpMutex;
#else
typedef std::mutex HttpMutex;
#endif

// CONDITION VARIABLES

// standard condition variable
typedef std::condition_variable_any HttpConditionVariable;

// LOCKS AND FENCES

// scoped unique lock
typedef std::unique_lock<HttpMutex> HttpScopedLock;

}

#endif	// LLCOREINT_MUTEX_H

