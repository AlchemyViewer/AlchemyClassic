// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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

apr_pool_t *gAPRPoolp = nullptr; // Global APR memory pool
LLVolatileAPRPool *LLAPRFile::sAPRFilePoolp = nullptr ; //global volatile APR memory pool.

const S32 FULL_VOLATILE_APR_POOL = 1024 ; //number of references to LLVolatileAPRPool

bool gAPRInitialized = false;

void ll_init_apr()
{
	// Initialize APR and create the global pool
	apr_initialize();
	
	if (!gAPRPoolp)
	{
		apr_pool_create(&gAPRPoolp, NULL);
	}

	if(!LLAPRFile::sAPRFilePoolp)
	{
		LLAPRFile::sAPRFilePoolp = new LLVolatileAPRPool(FALSE) ;
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
		gAPRPoolp = nullptr;
	}
	if (LLAPRFile::sAPRFilePoolp)
	{
		delete LLAPRFile::sAPRFilePoolp ;
		LLAPRFile::sAPRFilePoolp = nullptr ;
	}
	apr_terminate();
}

//
//
//LLAPRPool
//
LLAPRPool::LLAPRPool(apr_pool_t *parent, apr_size_t size, BOOL releasePoolFlag) 	
:   mPool(nullptr)
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
		mPool = nullptr ;
	}
}

//virtual
apr_pool_t* LLAPRPool::getAPRPool() 
{	
	return mPool ; 
}

LLVolatileAPRPool::LLVolatileAPRPool(BOOL is_local, apr_pool_t *parent, apr_size_t size, BOOL releasePoolFlag) 
				  : LLAPRPool(parent, size, releasePoolFlag),
				  mNumActiveRef(0),
				  mNumTotalRef(0),
				  mMutexp(nullptr)
{
	//create mutex
	if(!is_local) //not a local apr_pool, that is: shared by multiple threads.
	{
		mMutexp = new LLMutex();
	}
}

LLVolatileAPRPool::~LLVolatileAPRPool()
{
	delete_and_clear(mMutexp);
}

//
//define this virtual function to avoid any mistakenly calling LLAPRPool::getAPRPool().
//
//virtual 
apr_pool_t* LLVolatileAPRPool::getAPRPool() 
{
	return LLVolatileAPRPool::getVolatileAPRPool() ;
}

apr_pool_t* LLVolatileAPRPool::getVolatileAPRPool() 
{	
	LLMutexLock lock(mMutexp);

	mNumTotalRef++ ;
	mNumActiveRef++ ;

	if(!mPool)
	{
		createAPRPool() ;
	}
	
	return mPool ;
}

void LLVolatileAPRPool::clearVolatileAPRPool() 
{
	LLMutexLock lock(mMutexp) ;

	if(mNumActiveRef > 0)
	{
		mNumActiveRef--;
		if(mNumActiveRef < 1)
		{
			if(isFull()) 
			{
				mNumTotalRef = 0 ;

				//destroy the apr_pool.
				releaseAPRPool() ;
			}
			else 
			{
				//This does not actually free the memory, 
				//it just allows the pool to re-use this memory for the next allocation. 
				apr_pool_clear(mPool) ;
			}
		}
	}
	else
	{
		llassert_always(mNumActiveRef > 0) ;
	}

	llassert(mNumTotalRef <= (FULL_VOLATILE_APR_POOL << 2)) ;
}

BOOL LLVolatileAPRPool::isFull()
{
	return mNumTotalRef > FULL_VOLATILE_APR_POOL ;
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

//---------------------------------------------------------------------
//
// LLAPRFile functions
//
LLAPRFile::LLAPRFile()
	: mFile(nullptr),
	  mCurrentFilePoolp(nullptr)
{
}

LLAPRFile::LLAPRFile(const std::string& filename, apr_int32_t flags, LLVolatileAPRPool* pool)
	: mFile(nullptr),
	  mCurrentFilePoolp(nullptr)
{
	open(filename, flags, pool);
}

LLAPRFile::~LLAPRFile()
{
	close() ;
}

apr_status_t LLAPRFile::close() 
{
	apr_status_t ret = APR_SUCCESS ;
	if(mFile)
	{
		ret = apr_file_close(mFile);
		mFile = nullptr ;
	}

	if(mCurrentFilePoolp)
	{
		mCurrentFilePoolp->clearVolatileAPRPool() ;
		mCurrentFilePoolp = nullptr ;
	}

	return ret ;
}

apr_status_t LLAPRFile::open(const std::string& filename, apr_int32_t flags, LLVolatileAPRPool* pool, apr_off_t* sizep)
{
	apr_status_t s ;

	//check if already open some file
	llassert_always(!mFile) ;
	llassert_always(!mCurrentFilePoolp) ;
	
	apr_pool_t* apr_pool = pool ? pool->getVolatileAPRPool() : NULL ;
	s = apr_file_open(&mFile, filename.c_str(), flags, APR_OS_DEFAULT, getAPRFilePool(apr_pool));

	if (s != APR_SUCCESS || !mFile)
	{
		mFile = nullptr ;
		
		if (sizep)
		{
			*sizep = 0;
		}
	}
	else if (sizep)
	{
		apr_off_t file_size = 0;
		apr_off_t offset = 0;
		if (apr_file_seek(mFile, APR_END, &offset) == APR_SUCCESS)
		{
			llassert_always(offset <= 0x7fffffff);
			file_size = (apr_off_t)offset;
			offset = 0;
			apr_file_seek(mFile, APR_SET, &offset);
		}
		*sizep = file_size;
	}

	if(!mCurrentFilePoolp)
	{
		mCurrentFilePoolp = pool ;

		if(!mFile)
		{
			close() ;
		}
	}

	return s ;
}

//use gAPRPoolp.
apr_status_t LLAPRFile::open(const std::string& filename, apr_int32_t flags, BOOL use_global_pool)
{
	apr_status_t s;

	//check if already open some file
	llassert_always(!mFile) ;
	llassert_always(!mCurrentFilePoolp) ;
	llassert_always(use_global_pool) ; //be aware of using gAPRPoolp.
	
	s = apr_file_open(&mFile, filename.c_str(), flags, APR_OS_DEFAULT, gAPRPoolp);
	if (s != APR_SUCCESS || !mFile)
	{
		mFile = nullptr ;
		close() ;
		return s;
	}

	return s;
}

apr_pool_t* LLAPRFile::getAPRFilePool(apr_pool_t* pool)
{	
	if(!pool)
	{
		mCurrentFilePoolp = sAPRFilePoolp ;
		return mCurrentFilePoolp->getVolatileAPRPool() ;
	}

	return pool ;
}

// File I/O
apr_size_t LLAPRFile::read(void *buf, apr_size_t nbytes)
{
	if(!mFile) 
	{
		LL_WARNS() << "apr mFile is removed by somebody else. Can not read." << LL_ENDL ;
		return 0;
	}
	
	apr_size_t sz = nbytes;
	apr_status_t s = apr_file_read(mFile, buf, &sz);
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return 0;
	}
	else
	{
		//llassert_always(sz <= 0x7fffffff);
		return (apr_size_t)sz;
	}
}

apr_size_t LLAPRFile::write(const void *buf, apr_size_t nbytes)
{
	if(!mFile) 
	{
		LL_WARNS() << "apr mFile is removed by somebody else. Can not write." << LL_ENDL ;
		return 0;
	}
	
	apr_size_t sz = nbytes;
	apr_status_t s = apr_file_write(mFile, buf, &sz);
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return 0;
	}
	else
	{
		//llassert_always(sz <= 0x7fffffff);
		return (apr_size_t)sz;
	}
}

apr_off_t LLAPRFile::seek(apr_seek_where_t where, apr_off_t offset)
{
	return LLAPRFile::seek(mFile, where, offset) ;
}

//
//*******************************************************************************************************************************
//static components of LLAPRFile
//

//static
apr_status_t LLAPRFile::close(apr_file_t* file_handle, LLVolatileAPRPool* pool) 
{
	apr_status_t ret = APR_SUCCESS ;
	if(file_handle)
	{
		ret = apr_file_close(file_handle);
		file_handle = nullptr ;
	}

	if(pool)
	{
		pool->clearVolatileAPRPool() ;
	}

	return ret ;
}

//static
apr_file_t* LLAPRFile::open(const std::string& filename, LLVolatileAPRPool* pool, apr_int32_t flags)
{
	apr_status_t s;
	apr_file_t* file_handle ;

	pool = pool ? pool : LLAPRFile::sAPRFilePoolp ;

	s = apr_file_open(&file_handle, filename.c_str(), flags, APR_OS_DEFAULT, pool->getVolatileAPRPool());
	if (s != APR_SUCCESS || !file_handle)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to open filename: " << filename << LL_ENDL;
		file_handle = nullptr ;
		close(file_handle, pool) ;
		return nullptr;
	}

	return file_handle ;
}

//static
apr_off_t LLAPRFile::seek(apr_file_t* file_handle, apr_seek_where_t where, apr_off_t offset)
{
	if(!file_handle)
	{
		return -1 ;
	}

	apr_status_t s;
	apr_off_t apr_offset;
	if (offset >= 0)
	{
		apr_offset = (apr_off_t)offset;
		s = apr_file_seek(file_handle, where, &apr_offset);
	}
	else
	{
		apr_offset = 0;
		s = apr_file_seek(file_handle, APR_END, &apr_offset);
	}
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		return -1;
	}
	else
	{
		llassert_always(apr_offset <= 0x7fffffff);
		return (apr_off_t)apr_offset;
	}
}

//static
apr_size_t LLAPRFile::readEx(const std::string& filename, void *buf, apr_off_t offset, apr_size_t nbytes, LLVolatileAPRPool* pool)
{
	//*****************************************
	apr_file_t* file_handle = open(filename, pool, APR_READ|APR_BINARY); 
	//*****************************************	
	if (!file_handle)
	{
		return 0;
	}

	llassert(offset >= 0);

	if (offset > 0)
		offset = LLAPRFile::seek(file_handle, APR_SET, offset);
	
	apr_size_t bytes_read;
	if (offset < 0)
	{
		bytes_read = 0;
	}
	else
	{
		bytes_read = nbytes ;		
		apr_status_t s = apr_file_read(file_handle, buf, &bytes_read);
		if (s != APR_SUCCESS)
		{
			LL_WARNS("APR") << " Attempting to read filename: " << filename << LL_ENDL;
			ll_apr_warn_status(s);
			bytes_read = 0;
		}
		else
		{
			//llassert_always(bytes_read <= 0x7fffffff);		
		}
	}
	
	//*****************************************
	close(file_handle, pool) ; 
	//*****************************************
	return (apr_size_t)bytes_read;
}

//static
apr_size_t LLAPRFile::writeEx(const std::string& filename, void *buf, apr_off_t offset, apr_size_t nbytes, LLVolatileAPRPool* pool)
{
	apr_int32_t flags = APR_CREATE|APR_WRITE|APR_BINARY;
	if (offset < 0)
	{
		flags |= APR_APPEND;
		offset = 0;
	}
	
	//*****************************************
	apr_file_t* file_handle = open(filename, pool, flags);
	//*****************************************
	if (!file_handle)
	{
		return 0;
	}

	if (offset > 0)
	{
		offset = LLAPRFile::seek(file_handle, APR_SET, offset);
	}
	
	apr_size_t bytes_written;
	if (offset < 0)
	{
		bytes_written = 0;
	}
	else
	{
		bytes_written = nbytes ;		
		apr_status_t s = apr_file_write(file_handle, buf, &bytes_written);
		if (s != APR_SUCCESS)
		{
			LL_WARNS("APR") << " Attempting to write filename: " << filename << LL_ENDL;
			ll_apr_warn_status(s);
			bytes_written = 0;
		}
		else
		{
			//llassert_always(bytes_written <= 0x7fffffff);
		}
	}

	//*****************************************
	LLAPRFile::close(file_handle, pool);
	//*****************************************

	return (apr_size_t)bytes_written;
}

//static
bool LLAPRFile::remove(const std::string& filename, LLVolatileAPRPool* pool)
{
	apr_status_t s;

	pool = pool ? pool : LLAPRFile::sAPRFilePoolp ;
	s = apr_file_remove(filename.c_str(), pool->getVolatileAPRPool());
	pool->clearVolatileAPRPool() ;

	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to remove filename: " << filename << LL_ENDL;
		return false;
	}
	return true;
}

//static
bool LLAPRFile::rename(const std::string& filename, const std::string& newname, LLVolatileAPRPool* pool)
{
	apr_status_t s;

	pool = pool ? pool : LLAPRFile::sAPRFilePoolp ;
	s = apr_file_rename(filename.c_str(), newname.c_str(), pool->getVolatileAPRPool());
	pool->clearVolatileAPRPool() ;
	
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to rename filename: " << filename << LL_ENDL;
		return false;
	}
	return true;
}

//static
bool LLAPRFile::isExist(const std::string& filename, LLVolatileAPRPool* pool, apr_int32_t flags)
{
	apr_file_t* apr_file;
	apr_status_t s;

	pool = pool ? pool : LLAPRFile::sAPRFilePoolp ;
	s = apr_file_open(&apr_file, filename.c_str(), flags, APR_OS_DEFAULT, pool->getVolatileAPRPool());	

	if (s != APR_SUCCESS || !apr_file)
	{
		pool->clearVolatileAPRPool() ;
		return false;
	}
	else
	{
		apr_file_close(apr_file) ;
		pool->clearVolatileAPRPool() ;
		return true;
	}
}

//static
apr_off_t LLAPRFile::size(const std::string& filename, LLVolatileAPRPool* pool)
{
	apr_file_t* apr_file;
	apr_finfo_t info;
	apr_status_t s;
	
	pool = pool ? pool : LLAPRFile::sAPRFilePoolp ;
	s = apr_file_open(&apr_file, filename.c_str(), APR_READ, APR_OS_DEFAULT, pool->getVolatileAPRPool());
	
	if (s != APR_SUCCESS || !apr_file)
	{		
		pool->clearVolatileAPRPool() ;
		
		return 0;
	}
	else
	{
		s = apr_file_info_get(&info, APR_FINFO_SIZE, apr_file);		

		apr_file_close(apr_file) ;
		pool->clearVolatileAPRPool() ;
		
		if (s == APR_SUCCESS)
		{
			return (apr_off_t)info.size;
		}
		else
		{
			return 0;
		}
	}
}

//static
bool LLAPRFile::makeDir(const std::string& dirname, LLVolatileAPRPool* pool)
{
	apr_status_t s;

	pool = pool ? pool : LLAPRFile::sAPRFilePoolp ;
	s = apr_dir_make(dirname.c_str(), APR_FPROT_OS_DEFAULT, pool->getVolatileAPRPool());
	pool->clearVolatileAPRPool() ;
		
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to make directory: " << dirname << LL_ENDL;
		return false;
	}
	return true;
}

//static
bool LLAPRFile::removeDir(const std::string& dirname, LLVolatileAPRPool* pool)
{
	apr_status_t s;

	pool = pool ? pool : LLAPRFile::sAPRFilePoolp ;
	s = apr_file_remove(dirname.c_str(), pool->getVolatileAPRPool());
	pool->clearVolatileAPRPool() ;
	
	if (s != APR_SUCCESS)
	{
		ll_apr_warn_status(s);
		LL_WARNS("APR") << " Attempting to remove directory: " << dirname << LL_ENDL;
		return false;
	}
	return true;
}
//
//end of static components of LLAPRFile
//*******************************************************************************************************************************
//
