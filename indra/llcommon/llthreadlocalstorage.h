/** 
 * @file llthreadlocalstorage.h
 * @author Richard
 * @brief Class wrappers for thread local storage
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

#ifndef LL_LLTHREADLOCALSTORAGE_H
#define LL_LLTHREADLOCALSTORAGE_H

#include "llpreprocessor.h"

#include "llinstancetracker.h"

template <typename T>
class LLThreadLocalPointer
{
public:

	LLThreadLocalPointer()
	{}

	explicit LLThreadLocalPointer(T* value)
	{
		sThreadLocalPointer = value;
	}


	LLThreadLocalPointer(const LLThreadLocalPointer<T>& other)
	{
		sThreadLocalPointer = other.get();
	}

	~LLThreadLocalPointer()
	{
		if (notNull())
			delete sThreadLocalPointer;
	}

	LL_FORCE_INLINE T* get() const
	{
		return (T*) sThreadLocalPointer;
	}

	T* operator -> () const
	{
		return (T*) sThreadLocalPointer;
	}

	T& operator*() const
	{
		return *(T*) sThreadLocalPointer;
	}

	LLThreadLocalPointer<T>& operator = (T* value)
	{
		sThreadLocalPointer = value;
		return *this;
	}

	bool operator ==(const T* other) const
	{
		return sThreadLocalPointer == other;
	}

	bool isNull() const { return sThreadLocalPointer == nullptr; }

	bool notNull() const { return sThreadLocalPointer != nullptr; }
private:
	static LL_THREAD_LOCAL T* sThreadLocalPointer;
};

template<typename DERIVED_TYPE>
LL_THREAD_LOCAL DERIVED_TYPE* LLThreadLocalPointer<DERIVED_TYPE>::sThreadLocalPointer = nullptr;

template<typename DERIVED_TYPE>
class LLThreadLocalSingletonPointer
{
public:
	LL_FORCE_INLINE static DERIVED_TYPE* getInstance()
	{
		return sInstance;
	}

	static void setInstance(DERIVED_TYPE* instance)
	{
		sInstance = instance;
	}

private:
	static LL_THREAD_LOCAL DERIVED_TYPE* sInstance;
};

template<typename DERIVED_TYPE>
LL_THREAD_LOCAL DERIVED_TYPE* LLThreadLocalSingletonPointer<DERIVED_TYPE>::sInstance = nullptr;

#endif // LL_LLTHREADLOCALSTORAGE_H
