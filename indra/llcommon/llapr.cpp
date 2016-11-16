/** 
 * @file llapr.cpp
 * @author Phoenix
 * @date 2004-11-28
 * @brief Helper functions for using the apache portable runtime library.
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
#include "llmutex.h"
#include "llstl.h"

apr_pool_t *gAPRPoolp = NULL; // Global APR memory pool

bool gAPRInitialized = false;

void ll_init_apr()
{
	// Initialize APR and create the global pool
	apr_initialize();
	
	if (!gAPRPoolp)
	{
		apr_pool_create(&gAPRPoolp, NULL);
	}

	gAPRInitialized = true;
}


bool ll_apr_is_initialized()
{
	return gAPRInitialized;
}

void ll_cleanup_apr()
{
	gAPRInitialized = false;

	LL_INFOS("APR") << "Cleaning up APR" << LL_ENDL;

	if (gAPRPoolp)
	{
		apr_pool_destroy(gAPRPoolp);
		gAPRPoolp = NULL;
	}
	apr_terminate();
}

//
//
//LLAPRPool
//
LLAPRPool::LLAPRPool(apr_pool_t *parent, apr_size_t size, BOOL releasePoolFlag) 	
:   mPool(NULL)
,   mParent(parent)
,	mMaxSize(size)
,	mReleasePoolFlag(releasePoolFlag)
{	
	createAPRPool() ;
}

LLAPRPool::~LLAPRPool() 
{
	releaseAPRPool() ;
}

void LLAPRPool::createAPRPool()
{
	if(mPool)
	{
		return ;
	}

	mStatus = apr_pool_create(&mPool, mParent);
	ll_apr_warn_status(mStatus) ;

	if(mMaxSize > 0) //size is the number of blocks (which is usually 4K), NOT bytes.
	{
		apr_allocator_t *allocator = apr_pool_allocator_get(mPool); 
		if (allocator) 
		{ 
			apr_allocator_max_free_set(allocator, mMaxSize) ;
		}
	}
}

void LLAPRPool::releaseAPRPool()
{
	if(!mPool)
	{
		return ;
	}

	if(!mParent || mReleasePoolFlag)
	{
		apr_pool_destroy(mPool) ;
		mPool = NULL ;
	}
}

//virtual
apr_pool_t* LLAPRPool::getAPRPool() 
{	
	return mPool ; 
}

//---------------------------------------------------------------------

bool ll_apr_warn_status(apr_status_t status)
{
	if(APR_SUCCESS == status) return false;
#if !LL_LINUX
	char buf[MAX_STRING];	/* Flawfinder: ignore */
	apr_strerror(status, buf, sizeof(buf));
	LL_WARNS("APR") << "APR: " << buf << LL_ENDL;
#endif
	return true;
}

bool ll_apr_warn_status(apr_status_t status, apr_dso_handle_t *handle)
{
    bool result = ll_apr_warn_status(status);
    // Despite observed truncation of actual Mac dylib load errors, increasing
    // this buffer to more than MAX_STRING doesn't help: it appears that APR
    // stores the output in a fixed 255-character internal buffer. (*sigh*)
    char buf[MAX_STRING];           /* Flawfinder: ignore */
    apr_dso_error(handle, buf, sizeof(buf));
    LL_WARNS("APR") << "APR: " << buf << LL_ENDL;
    return result;
}

void ll_apr_assert_status(apr_status_t status)
{
	llassert(! ll_apr_warn_status(status));
}

void ll_apr_assert_status(apr_status_t status, apr_dso_handle_t *handle)
{
    llassert(! ll_apr_warn_status(status, handle));
}

//
//end of static components of LLAPRFile
//*******************************************************************************************************************************
//
