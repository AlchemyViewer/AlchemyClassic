/** 
 * @file llatmomic.h
 * @brief Base classes for atomics.
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
 * Copyright (C) 2014, Alchemy Development Group
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

#pragma once
 
#include "stdtypes.h"
#define AL_BOOST_ATOMICS 1

#if AL_BOOST_ATOMICS
#include <boost/atomic.hpp>
template<typename Type>
using LLAtomic32 = boost::atomic<Type>;
#elif AL_STD_ATOMICS
#include <atomic>
template<typename Type>
using LLAtomic32 = std::atomic<Type>;
#endif


typedef LLAtomic32<U32> LLAtomicU32;
typedef LLAtomic32<S32> LLAtomicS32;
