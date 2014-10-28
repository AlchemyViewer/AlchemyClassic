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
#define AL_CXX_ATOMICS 1
#define AL_BOOST_ATOMICS 1

#if AL_CXX_ATOMICS && (__cplusplus >= 201103L || _MSC_VER >= 1800)
#if AL_BOOST_ATOMICS
#include <boost/atomic.hpp>
template<typename Type>
using LLAtomic32 = boost::atomic<Type>;
#elif AL_STD_ATOMICS
#include <atomic>
template<typename Type>
using LLAtomic32 = std::atomic<Type>;
#endif
#else
#include <apr_atomic.h>
template <typename Type> class LLAtomic32
{
public:
	LLAtomic32<Type>() {};
	LLAtomic32<Type>(Type x) {apr_atomic_set32(&mData, apr_uint32_t(x)); };
	~LLAtomic32<Type>() {};

	operator const Type() { apr_uint32_t data = apr_atomic_read32(&mData); return Type(data); }
	
	Type	load() const { apr_uint32_t data = apr_atomic_read32(const_cast< volatile apr_uint32_t* >(&mData)); return Type(data); }

	Type operator =(const Type& x) { apr_atomic_set32(&mData, apr_uint32_t(x)); return Type(mData); }
	void operator -=(Type x) { apr_atomic_sub32(&mData, apr_uint32_t(x)); }
	void operator +=(Type x) { apr_atomic_add32(&mData, apr_uint32_t(x)); }
	Type operator ++(int) { return apr_atomic_inc32(&mData); } // Type++
	Type operator --(int) { return apr_atomic_dec32(&mData); } // approximately --Type (0 if final is 0, non-zero otherwise)

	Type operator ++() { return apr_atomic_inc32(&mData); } // Type++
	Type operator --() { return apr_atomic_dec32(&mData); } // approximately --Type (0 if final is 0, non-zero otherwise)
	
private:
	volatile apr_uint32_t mData;
};
#endif // AL_ATOMICS_CXX

typedef LLAtomic32<U32> LLAtomicU32;
typedef LLAtomic32<S32> LLAtomicS32;