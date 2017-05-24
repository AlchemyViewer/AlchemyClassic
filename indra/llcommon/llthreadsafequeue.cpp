// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llthread.cpp
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
#include "llapr.h"
#include "llthreadsafequeue.h"
#include "llexception.h"



// LLThreadSafeQueueImplementation
//-----------------------------------------------------------------------------


LLThreadSafeQueueImplementation::LLThreadSafeQueueImplementation(apr_pool_t * pool, unsigned int capacity):
	mOwnsPool(pool == nullptr),
	mPool(pool),
	mQueue(nullptr)
{
	if(mOwnsPool) {
		apr_status_t status = apr_pool_create(&mPool, nullptr);
		if(status != APR_SUCCESS) LLTHROW(LLThreadSafeQueueError("failed to allocate pool"));
	} else {
		; // No op.
	}
	
	apr_status_t status = apr_queue_create(&mQueue, capacity, mPool);
	if(status != APR_SUCCESS) LLTHROW(LLThreadSafeQueueError("failed to allocate queue"));
}


LLThreadSafeQueueImplementation::~LLThreadSafeQueueImplementation()
{
	if(mQueue != nullptr) {
		if(apr_queue_size(mQueue) != 0) LL_WARNS() << 
			"terminating queue which still contains " << apr_queue_size(mQueue) <<
			" elements;" << "memory will be leaked" << LL_ENDL;
		apr_queue_term(mQueue);
	}
	if(mOwnsPool && (mPool != nullptr)) apr_pool_destroy(mPool);
}


void LLThreadSafeQueueImplementation::pushFront(void * element)
{
	apr_status_t status = apr_queue_push(mQueue, element);
	
	if(status == APR_EINTR) {
		LLTHROW(LLThreadSafeQueueInterrupt());
	} else if(status != APR_SUCCESS) {
		LLTHROW(LLThreadSafeQueueError("push failed"));
	} else {
		; // Success.
	}
}


bool LLThreadSafeQueueImplementation::tryPushFront(void * element){
	return apr_queue_trypush(mQueue, element) == APR_SUCCESS;
}


void * LLThreadSafeQueueImplementation::popBack(void)
{
	void * element = nullptr;
	apr_status_t status = apr_queue_pop(mQueue, &element);

	if(status == APR_EINTR) {
		LLTHROW(LLThreadSafeQueueInterrupt());
	}
	else if (status != APR_SUCCESS) {
		LLTHROW(LLThreadSafeQueueError("pop failed"));
	}
	return element;
}


bool LLThreadSafeQueueImplementation::tryPopBack(void *& element)
{
	return apr_queue_trypop(mQueue, &element) == APR_SUCCESS;
}


size_t LLThreadSafeQueueImplementation::size()
{
	return apr_queue_size(mQueue);
}
