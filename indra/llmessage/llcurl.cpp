/**
 * @file llcurl.cpp
 * @author Zero / Donovan
 * @date 2006-10-15
 * @brief Implementation of wrapper around libcurl.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010-2013, Linden Research, Inc.
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

#if LL_WINDOWS
#define SAFE_SSL 1
#elif LL_DARWIN
#define SAFE_SSL 1
#else
#define SAFE_SSL 1
#endif

#include "linden_common.h"

#include "llcurl.h"

#include <algorithm>
#include <iomanip>
#include <curl/curl.h>
#if SAFE_SSL
#include <openssl/crypto.h>
#endif

#include "llbufferstream.h"
#include "llproxy.h"
#include "llsdserialize.h"
#include "llstl.h"
#include "llthread.h"
#include "lltimer.h"

//////////////////////////////////////////////////////////////////////////////
/*
	The trick to getting curl to do keep-alives is to reuse the
	same easy handle for the requests.  It appears that curl
	keeps a pool of connections alive for each easy handle, but
	doesn't share them between easy handles.  Therefore it is
	important to keep a pool of easy handles and reuse them,
	rather than create and destroy them with each request.  This
	code does this.

	Furthermore, it would behoove us to keep track of which
	hosts an easy handle was used for and pick an easy handle
	that matches the next request.  This code does not current
	do this.
 */

//////////////////////////////////////////////////////////////////////////////

static const U32 EASY_HANDLE_POOL_SIZE		= 5;
static const S32 MULTI_PERFORM_CALL_REPEAT	= 5;
static const S32 CURL_REQUEST_TIMEOUT = 120; // seconds per operation
static const S32 CURL_CONNECT_TIMEOUT = 30; //seconds to wait for a connection
static const S32 MAX_ACTIVE_REQUEST_COUNT = 100;

// DEBUG //
S32 gCurlEasyCount = 0;
S32 gCurlMultiCount = 0;

//////////////////////////////////////////////////////////////////////////////

//static
std::vector<LLMutex*> LLCurl::sSSLMutex;
std::string LLCurl::sCAPath;
std::string LLCurl::sCAFile;
LLCurlThread* LLCurl::sCurlThread = NULL ;
LLMutex* LLCurl::sHandleMutexp = NULL ;
S32      LLCurl::sTotalHandles = 0 ;
bool     LLCurl::sNotQuitting = true;
F32      LLCurl::sCurlRequestTimeOut = 120.f; //seonds
S32      LLCurl::sMaxHandles = 256; //max number of handles, (multi handles and easy handles combined).
CURL*	 LLCurl::sCurlTemplateStandardHandle = NULL;

void check_curl_code(CURLcode code)
{
	if (code != CURLE_OK)
	{
		// linux appears to throw a curl error once per session for a bad initialization
		// at a pretty random time (when enabling cookies).
		LL_INFOS() << "curl error detected: " << curl_easy_strerror(code) << LL_ENDL;
	}
}

void check_curl_multi_code(CURLMcode code) 
{
	if (code != CURLM_OK)
	{
		// linux appears to throw a curl error once per session for a bad initialization
		// at a pretty random time (when enabling cookies).
		LL_INFOS() << "curl multi error detected: " << curl_multi_strerror(code) << LL_ENDL;
	}
}

//static
void LLCurl::setCAPath(const std::string& path)
{
	sCAPath = path;
}

//static
void LLCurl::setCAFile(const std::string& file)
{
	sCAFile = file;
}

//static
std::string LLCurl::getVersionString()
{
	return std::string(curl_version());
}

//////////////////////////////////////////////////////////////////////////////

LLCurl::Responder::Responder()
{
}

LLCurl::Responder::~Responder()
{
	LL_CHECK_MEMORY
}

// virtual
void LLCurl::Responder::errorWithContent(
	U32 status,
	const std::string& reason,
	const LLSD&)
{
	error(status, reason);
}

// virtual
void LLCurl::Responder::error(U32 status, const std::string& reason)
{
	LL_INFOS() << mURL << " [" << status << "]: " << reason << LL_ENDL;
}

// virtual
void LLCurl::Responder::result(const LLSD& content)
{
}

void LLCurl::Responder::setURL(const std::string& url)
{
	mURL = url;
}

// virtual
void LLCurl::Responder::completedRaw(
	U32 status,
	const std::string& reason,
	const LLChannelDescriptors& channels,
	const LLIOPipe::buffer_ptr_t& buffer)
{
	LLSD content;
	LLBufferStream istr(channels, buffer.get());
	const bool emit_errors = false;
	if (LLSDParser::PARSE_FAILURE == LLSDSerialize::fromXML(content, istr, emit_errors))
	{
		LL_INFOS() << "Failed to deserialize LLSD. " << mURL << " [" << status << "]: " << reason << LL_ENDL;
		content["reason"] = reason;
	}

	completed(status, reason, content);
}

// virtual
void LLCurl::Responder::completed(U32 status, const std::string& reason, const LLSD& content)
{
	if (isGoodStatus(status))
	{
		result(content);
	}
	else
	{
		errorWithContent(status, reason, content);
	}
}

//virtual
void LLCurl::Responder::completedHeader(U32 status, const std::string& reason, const LLSD& content)
{

}

//////////////////////////////////////////////////////////////////////////////

std::set<CURL*> LLCurl::Easy::sFreeHandles;
std::set<CURL*> LLCurl::Easy::sActiveHandles;
LLMutex* LLCurl::Easy::sHandleMutexp = NULL ;

//static
CURL* LLCurl::Easy::allocEasyHandle()
{
	llassert(LLCurl::getCurlThread()) ;

	CURL* ret = NULL;

	LLMutexLock lock(sHandleMutexp) ;

	if (sFreeHandles.empty())
	{
		ret = LLCurl::newEasyHandle();
	}
	else
	{
		ret = *(sFreeHandles.begin());
		sFreeHandles.erase(ret);
		curl_easy_reset(ret);
	}

	if (ret)
	{
		sActiveHandles.insert(ret);
	}

	return ret;
}

//static
void LLCurl::Easy::releaseEasyHandle(CURL* handle)
{
	static const S32 MAX_NUM_FREE_HANDLES = 32 ;

	if (!handle)
	{
		return ; //handle allocation failed.
		//LL_ERRS() << "handle cannot be NULL!" << LL_ENDL;
	}

	LLMutexLock lock(sHandleMutexp) ;
	if (sActiveHandles.find(handle) != sActiveHandles.end())
	{
		LL_CHECK_MEMORY
		sActiveHandles.erase(handle);
		LL_CHECK_MEMORY
		if(sFreeHandles.size() < MAX_NUM_FREE_HANDLES)
		{
			sFreeHandles.insert(handle);
			LL_CHECK_MEMORY
		}
		else
		{
			LLCurl::deleteEasyHandle(handle) ;
			LL_CHECK_MEMORY
		}
	}
	else
	{
		LL_ERRS() << "Invalid handle." << LL_ENDL;
	}
}

LLCurl::Easy::Easy()
	: mHeaders(NULL),
	  mCurlEasyHandle(NULL)
{
	mErrorBuffer[0] = 0;
}

LLCurl::Easy* LLCurl::Easy::getEasy()
{
	Easy* easy = new Easy();
	easy->mCurlEasyHandle = allocEasyHandle(); 
	
	if (!easy->mCurlEasyHandle)
	{
		// this can happen if we have too many open files (fails in c-ares/ares_init.c)
		LL_WARNS() << "allocEasyHandle() returned NULL! Easy handles: " << gCurlEasyCount << " Multi handles: " << gCurlMultiCount << LL_ENDL;
		delete easy;
		return NULL;
	}
	
	// Enable a brief cache period for now.  This was zero for the longest time
	// which caused some routers grief and generated unneeded traffic.  For the
	// threaded resolver, we're using system resolution libraries and non-zero values
	// are preferred.  The c-ares resolver is another matter and it might not
	// track server changes as well.
	CURLcode result = curl_easy_setopt(easy->mCurlEasyHandle, CURLOPT_DNS_CACHE_TIMEOUT, 15);
	check_curl_code(result);
	result = curl_easy_setopt(easy->mCurlEasyHandle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	check_curl_code(result);
	
	// Disable SSL/TLS session caching. Some servers refuse to talk to us when session ids are enabled.
	// id.secondlife.com is such a server, when greeted with a SSL HELLO and a session id, it immediatly returns a RST packet and closes
	// the connections.
	// Fixes: VWR-28039, VWR-28629
	result = curl_easy_setopt(easy->mCurlEasyHandle, CURLOPT_SSL_SESSIONID_CACHE, 0);
	check_curl_code(result);

	++gCurlEasyCount;
	return easy;
}

LLCurl::Easy::~Easy()
{
	releaseEasyHandle(mCurlEasyHandle);
	--gCurlEasyCount;
	curl_slist_free_all(mHeaders);
	LL_CHECK_MEMORY
	for_each(mStrings.begin(), mStrings.end(), DeletePointerArray());
	LL_CHECK_MEMORY
	if (mResponder && LLCurl::sNotQuitting) //aborted
	{	
		std::string reason("Request timeout, aborted.") ;
		mResponder->completedRaw(408, //HTTP_REQUEST_TIME_OUT, timeout, abort
			reason, mChannels, mOutput);		
		LL_CHECK_MEMORY
	}
	mResponder = NULL;
}

void LLCurl::Easy::resetState()
{
 	curl_easy_reset(mCurlEasyHandle);

	if (mHeaders)
	{
		curl_slist_free_all(mHeaders);
		mHeaders = NULL;
	}

	mRequest.str("");
	mRequest.clear();

	mOutput.reset();
	
	mInput.str("");
	mInput.clear();
	
	mErrorBuffer[0] = 0;
	
	mHeaderOutput.str("");
	mHeaderOutput.clear();
}

void LLCurl::Easy::setErrorBuffer()
{
	setopt(CURLOPT_ERRORBUFFER, &mErrorBuffer);
}

const char* LLCurl::Easy::getErrorBuffer()
{
	return mErrorBuffer;
}

void LLCurl::Easy::setCA()
{
	if (!sCAPath.empty())
	{
		setoptString(CURLOPT_CAPATH, sCAPath);
	}
	if (!sCAFile.empty())
	{
		setoptString(CURLOPT_CAINFO, sCAFile);
	}
}

void LLCurl::Easy::setHeaders()
{
	setopt(CURLOPT_HTTPHEADER, mHeaders);
}

void LLCurl::Easy::getTransferInfo(LLCurl::TransferInfo* info)
{
	check_curl_code(curl_easy_getinfo(mCurlEasyHandle, CURLINFO_SIZE_DOWNLOAD, &info->mSizeDownload));
	check_curl_code(curl_easy_getinfo(mCurlEasyHandle, CURLINFO_TOTAL_TIME, &info->mTotalTime));
	check_curl_code(curl_easy_getinfo(mCurlEasyHandle, CURLINFO_SPEED_DOWNLOAD, &info->mSpeedDownload));
}

U32 LLCurl::Easy::report(CURLcode code)
{
	U32 responseCode = 0;	
	std::string responseReason;
	
	if (code == CURLE_OK)
	{
		check_curl_code(curl_easy_getinfo(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, &responseCode));
		//*TODO: get reason from first line of mHeaderOutput
	}
	else
	{
		responseCode = 499;
		responseReason = strerror(code) + " : " + mErrorBuffer;
		setopt(CURLOPT_FRESH_CONNECT, TRUE);
	}

	if (mResponder)
	{	
		mResponder->completedRaw(responseCode, responseReason, mChannels, mOutput);
		mResponder = NULL;
	}
	
	resetState();
	return responseCode;
}

// Note: these all assume the caller tracks the value (i.e. keeps it persistant)
void LLCurl::Easy::setopt(CURLoption option, S32 value)
{
	CURLcode result = curl_easy_setopt(mCurlEasyHandle, option, value);
	check_curl_code(result);
}

void LLCurl::Easy::setopt(CURLoption option, void* value)
{
	CURLcode result = curl_easy_setopt(mCurlEasyHandle, option, value);
	check_curl_code(result);
}

void LLCurl::Easy::setopt(CURLoption option, char* value)
{
	CURLcode result = curl_easy_setopt(mCurlEasyHandle, option, value);
	check_curl_code(result);
}

// Note: this copies the string so that the caller does not have to keep it around
void LLCurl::Easy::setoptString(CURLoption option, const std::string& value)
{
	char* tstring = new char[value.length()+1];
	strcpy(tstring, value.c_str());
	mStrings.push_back(tstring);
	CURLcode result = curl_easy_setopt(mCurlEasyHandle, option, tstring);
	check_curl_code(result);
}

void LLCurl::Easy::slist_append(const char* str)
{
	mHeaders = curl_slist_append(mHeaders, str);
}

size_t curlReadCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
	LLCurl::Easy* easy = (LLCurl::Easy*)user_data;
	
	S32 n = size * nmemb;
	S32 startpos = (S32)easy->getInput().tellg();
	easy->getInput().seekg(0, std::ios::end);
	S32 endpos = (S32)easy->getInput().tellg();
	easy->getInput().seekg(startpos, std::ios::beg);
	S32 maxn = endpos - startpos;
	n = llmin(n, maxn);
	easy->getInput().read((char*)data, n);

	return n;
}

size_t curlWriteCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
	LLCurl::Easy* easy = (LLCurl::Easy*)user_data;
	
	S32 n = size * nmemb;
	easy->getOutput()->append(easy->getChannels().in(), (const U8*)data, n);

	return n;
}

size_t curlHeaderCallback(void* data, size_t size, size_t nmemb, void* user_data)
{
	LLCurl::Easy* easy = (LLCurl::Easy*)user_data;
	
	size_t n = size * nmemb;
	easy->getHeaderOutput().write((const char*)data, n);

	return n;
}

void LLCurl::Easy::prepRequest(const std::string& url,
							   const std::vector<std::string>& headers,
							   ResponderPtr responder, S32 time_out, bool post)
{
	resetState();
	
	if (post) setoptString(CURLOPT_ENCODING, "");

	//setopt(CURLOPT_VERBOSE, 1); // useful for debugging
	setopt(CURLOPT_NOSIGNAL, 1);
	setopt(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	
	// Set the CURL options for either Socks or HTTP proxy
	LLProxy::getInstance()->applyProxySettings(this);

	mOutput.reset(new LLBufferArray);
	mOutput->setThreaded(true);
	setopt(CURLOPT_WRITEFUNCTION, (void*)&curlWriteCallback);
	setopt(CURLOPT_WRITEDATA, (void*)this);

	setopt(CURLOPT_READFUNCTION, (void*)&curlReadCallback);
	setopt(CURLOPT_READDATA, (void*)this);
	
	setopt(CURLOPT_HEADERFUNCTION, (void*)&curlHeaderCallback);
	setopt(CURLOPT_HEADERDATA, (void*)this);

	// Allow up to five redirects
	if (responder && responder->followRedir())
	{
		setopt(CURLOPT_FOLLOWLOCATION, 1);
		setopt(CURLOPT_MAXREDIRS, MAX_REDIRECTS);
	}

	setErrorBuffer();
	setCA();

	setopt(CURLOPT_SSL_VERIFYPEER, true);
	
	//don't verify host name so urls with scrubbed host names will work (improves DNS performance)
	setopt(CURLOPT_SSL_VERIFYHOST, 0);
	setopt(CURLOPT_TIMEOUT, llmax(time_out, CURL_REQUEST_TIMEOUT));
	setopt(CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT);

	setoptString(CURLOPT_URL, url);

	mResponder = responder;

	if (!post)
	{
		slist_append("Connection: keep-alive");
		slist_append("Keep-alive: 300");
		// Accept and other headers
		for (std::vector<std::string>::const_iterator iter = headers.begin();
			 iter != headers.end(); ++iter)
		{
			slist_append((*iter).c_str());
		}
	}
}

////////////////////////////////////////////////////////////////////////////
LLCurl::Multi::Multi(F32 idle_time_out)
	: mQueued(0),
	  mErrorCount(0),
	  mState(STATE_READY),
	  mDead(FALSE),
	  mValid(TRUE),
	  mMutexp(NULL),
	  mDeletionMutexp(NULL),
	  mEasyMutexp(NULL)
{
	mCurlMultiHandle = LLCurl::newMultiHandle();
	if (!mCurlMultiHandle)
	{
		LL_WARNS() << "curl_multi_init() returned NULL! Easy handles: " << gCurlEasyCount << " Multi handles: " << gCurlMultiCount << LL_ENDL;
		mCurlMultiHandle = LLCurl::newMultiHandle();
	}
	
	//llassert_always(mCurlMultiHandle);	

	if(mCurlMultiHandle)
	{
	if(LLCurl::getCurlThread()->getThreaded())
	{
		mMutexp = new LLMutex(NULL) ;
		mDeletionMutexp = new LLMutex(NULL) ;
		mEasyMutexp = new LLMutex(NULL) ;
	}
	LLCurl::getCurlThread()->addMulti(this) ;

		mIdleTimeOut = idle_time_out ;
		if(mIdleTimeOut < LLCurl::sCurlRequestTimeOut)
		{
			mIdleTimeOut = LLCurl::sCurlRequestTimeOut ;
		}

	++gCurlMultiCount;
}
}

LLCurl::Multi::~Multi()
{
	cleanup(true) ;	
	
	delete mDeletionMutexp ;
	mDeletionMutexp = NULL ;	
}

void LLCurl::Multi::cleanup(bool deleted)
{
	if(!mCurlMultiHandle)
	{
		return ; //nothing to clean.
	}
	llassert_always(deleted || !mValid) ;

	LLMutexLock lock(mDeletionMutexp);


	// Clean up active
	for(easy_active_list_t::iterator iter = mEasyActiveList.begin();
		iter != mEasyActiveList.end(); ++iter)
	{
		Easy* easy = *iter;
		LL_CHECK_MEMORY
		check_curl_multi_code(curl_multi_remove_handle(mCurlMultiHandle, easy->getCurlHandle()));
		LL_CHECK_MEMORY
		if(deleted)
		{
			easy->mResponder = NULL ; //avoid triggering mResponder.
			LL_CHECK_MEMORY
		}
		delete easy;
		LL_CHECK_MEMORY
	}
	mEasyActiveList.clear();
	mEasyActiveMap.clear();
	
	LL_CHECK_MEMORY
	
		// Clean up freed
	for_each(mEasyFreeList.begin(), mEasyFreeList.end(), DeletePointer());	
	mEasyFreeList.clear();
	
	LL_CHECK_MEMORY
		
	check_curl_multi_code(LLCurl::deleteMultiHandle(mCurlMultiHandle));
	mCurlMultiHandle = NULL ;

	LL_CHECK_MEMORY
	
	delete mMutexp ;
	mMutexp = NULL ;

	LL_CHECK_MEMORY

	delete mEasyMutexp ;
	mEasyMutexp = NULL ;

	LL_CHECK_MEMORY

	mQueued = 0 ;
	mState = STATE_COMPLETED;
	
	--gCurlMultiCount;

	return ;
}

void LLCurl::Multi::lock()
{
	if(mMutexp)
	{
		mMutexp->lock() ;
	}
}

void LLCurl::Multi::unlock()
{
	if(mMutexp)
	{
		mMutexp->unlock() ;
	}
}

void LLCurl::Multi::markDead()
{
	{
		LLMutexLock lock(mDeletionMutexp) ;
	
		if(mCurlMultiHandle != NULL)
		{
			mDead = TRUE ;
			LLCurl::getCurlThread()->setPriority(mHandle, LLQueuedThread::PRIORITY_URGENT) ; 

			return;
		}
	}
	
	//not valid, delete it.
	delete this;	
}

void LLCurl::Multi::setState(LLCurl::Multi::ePerformState state)
{
	lock() ;
	mState = state ;
	unlock() ;

	if(mState == STATE_READY)
	{
		LLCurl::getCurlThread()->setPriority(mHandle, LLQueuedThread::PRIORITY_NORMAL) ;
	}	
}

LLCurl::Multi::ePerformState LLCurl::Multi::getState()
{
	return mState;
}
	
bool LLCurl::Multi::isCompleted() 
{
	return STATE_COMPLETED == getState() ;
}

bool LLCurl::Multi::waitToComplete()
{
	if(!isValid())
	{
		return true ;
	}

	if(!mMutexp) //not threaded
	{
		doPerform() ;
		return true ;
	}

	bool completed = (STATE_COMPLETED == mState) ;
	if(!completed)
	{
		LLCurl::getCurlThread()->setPriority(mHandle, LLQueuedThread::PRIORITY_HIGH) ;
	}
	
	return completed;
}

CURLMsg* LLCurl::Multi::info_read(S32* msgs_in_queue)
{
	LLMutexLock lock(mMutexp) ;

	CURLMsg* curlmsg = curl_multi_info_read(mCurlMultiHandle, msgs_in_queue);
	return curlmsg;
}

//return true if dead
bool LLCurl::Multi::doPerform()
{
	LLMutexLock lock(mDeletionMutexp) ;
	
	bool dead = mDead ;

	if(mDead)
	{
		setState(STATE_COMPLETED);
		mQueued = 0 ;
	}
	else if(getState() != STATE_COMPLETED)
	{		
		setState(STATE_PERFORMING);

		S32 q = 0;
		for (S32 call_count = 0;
				call_count < MULTI_PERFORM_CALL_REPEAT;
				call_count++)
		{
			LLMutexLock lock(mMutexp) ;

			//WARNING: curl_multi_perform will block for many hundreds of milliseconds
			// NEVER call this from the main thread, and NEVER allow the main thread to 
			// wait on a mutex held by this thread while curl_multi_perform is executing
			CURLMcode code = curl_multi_perform(mCurlMultiHandle, &q);
			if (CURLM_CALL_MULTI_PERFORM != code || q == 0)
			{
				check_curl_multi_code(code);
			
				break;
			}
		}

		mQueued = q;	
		setState(STATE_COMPLETED) ;
		mIdleTimer.reset() ;
	}
	else if(!mValid && mIdleTimer.getElapsedTimeF32() > mIdleTimeOut) //idle for too long, remove it.
	{
		dead = true ;
	}
	else if(mValid && mIdleTimer.getElapsedTimeF32() > mIdleTimeOut - 1.f) //idle for too long, mark it invalid.
	{
		mValid = FALSE ;
	}

	return dead ;
}

S32 LLCurl::Multi::process()
{
	if(!isValid())
	{
		return 0 ;
	}

	waitToComplete() ;

	if (getState() != STATE_COMPLETED)
	{
		return 0;
	}

	CURLMsg* msg;
	int msgs_in_queue;

	S32 processed = 0;
	while ((msg = info_read(&msgs_in_queue)))
	{
		++processed;
		if (msg->msg == CURLMSG_DONE)
		{
			U32 response = 0;
			Easy* easy = NULL ;

			{
				LLMutexLock lock(mEasyMutexp) ;
				easy_active_map_t::iterator iter = mEasyActiveMap.find(msg->easy_handle);
				if (iter != mEasyActiveMap.end())
				{
					easy = iter->second;
				}
			}

			if(easy)
			{
				response = easy->report(msg->data.result);
				removeEasy(easy);
			}
			else
			{
				response = 499;
				//*TODO: change to LL_WARNS()
				LL_ERRS() << "cleaned up curl request completed!" << LL_ENDL;
			}
			if (response >= 400)
			{
				// failure of some sort, inc mErrorCount for debugging and flagging multi for destruction
				++mErrorCount;
			}
		}
	}

	setState(STATE_READY);

	return processed;
}

LLCurl::Easy* LLCurl::Multi::allocEasy()
{
	Easy* easy = 0;	

	if (mEasyFreeList.empty())
	{		
		easy = Easy::getEasy();
	}
	else
	{
		LLMutexLock lock(mEasyMutexp) ;
		easy = *(mEasyFreeList.begin());
		mEasyFreeList.erase(easy);
	}
	if (easy)
	{
		LLMutexLock lock(mEasyMutexp) ;
		mEasyActiveList.insert(easy);
		mEasyActiveMap[easy->getCurlHandle()] = easy;
	}
	return easy;
}

bool LLCurl::Multi::addEasy(Easy* easy)
{
	LLMutexLock lock(mMutexp) ;
	CURLMcode mcode = curl_multi_add_handle(mCurlMultiHandle, easy->getCurlHandle());
	check_curl_multi_code(mcode);
	//if (mcode != CURLM_OK)
	//{
	//	LL_WARNS() << "Curl Error: " << curl_multi_strerror(mcode) << LL_ENDL;
	//	return false;
	//}
	return true;
}

void LLCurl::Multi::easyFree(Easy* easy)
{
	if(mEasyMutexp)
	{
		mEasyMutexp->lock() ;
	}

	mEasyActiveList.erase(easy);
	mEasyActiveMap.erase(easy->getCurlHandle());

	if (mEasyFreeList.size() < EASY_HANDLE_POOL_SIZE)
	{		
		mEasyFreeList.insert(easy);
		
		if(mEasyMutexp)
		{
			mEasyMutexp->unlock() ;
		}

		easy->resetState();
	}
	else
	{
		if(mEasyMutexp)
		{
			mEasyMutexp->unlock() ;
		}
		delete easy;
	}
}

void LLCurl::Multi::removeEasy(Easy* easy)
{
	{
		LLMutexLock lock(mMutexp) ;
		check_curl_multi_code(curl_multi_remove_handle(mCurlMultiHandle, easy->getCurlHandle()));
	}
	easyFree(easy);
}

//------------------------------------------------------------
//LLCurlThread
LLCurlThread::CurlRequest::CurlRequest(handle_t handle, LLCurl::Multi* multi, LLCurlThread* curl_thread) :
	LLQueuedThread::QueuedRequest(handle, LLQueuedThread::PRIORITY_NORMAL, FLAG_AUTO_COMPLETE),
	mMulti(multi),
	mCurlThread(curl_thread)
{	
}

LLCurlThread::CurlRequest::~CurlRequest()
{	
	if(mMulti)
	{
		mCurlThread->deleteMulti(mMulti) ;
		mMulti = NULL ;
	}
}

bool LLCurlThread::CurlRequest::processRequest()
{
	bool completed = true ;
	if(mMulti)
	{
		completed = mCurlThread->doMultiPerform(mMulti) ;

		if(!completed)
		{
			setPriority(LLQueuedThread::PRIORITY_LOW) ;
		}
	}

	return completed ;
}

void LLCurlThread::CurlRequest::finishRequest(bool completed)
{
	if(mMulti->isDead())
	{
		mCurlThread->deleteMulti(mMulti) ;
	}
	else
	{
		mCurlThread->cleanupMulti(mMulti) ; //being idle too long, remove the request.
	}

	mMulti = NULL ;
}
	
LLCurlThread::LLCurlThread(bool threaded) :
	LLQueuedThread("curlthread", threaded)
{
}
	
//virtual 
LLCurlThread::~LLCurlThread() 
{
}

S32 LLCurlThread::update(F32 max_time_ms)
{	
	return LLQueuedThread::update(max_time_ms);
}

void LLCurlThread::addMulti(LLCurl::Multi* multi)
{
	multi->mHandle = generateHandle() ;

	CurlRequest* req = new CurlRequest(multi->mHandle, multi, this) ;

	if (!addRequest(req))
	{
		LL_WARNS() << "curl request added when the thread is quitted" << LL_ENDL;
	}
}
	
void LLCurlThread::killMulti(LLCurl::Multi* multi)
{
	if(!multi)
	{
		return ;
	}


	multi->markDead() ;
}

//private
bool LLCurlThread::doMultiPerform(LLCurl::Multi* multi) 
{
	return multi->doPerform() ;
}

//private
void LLCurlThread::deleteMulti(LLCurl::Multi* multi) 
{
	delete multi ;
}

//private
void LLCurlThread::cleanupMulti(LLCurl::Multi* multi) 
{
	multi->cleanup() ;
	if(multi->isDead()) //check if marked dead during cleaning up.
	{
		deleteMulti(multi) ;
	}
}

//------------------------------------------------------------

//static
std::string LLCurl::strerror(CURLcode errorcode)
{
	return std::string(curl_easy_strerror(errorcode));
}

////////////////////////////////////////////////////////////////////////////
// For generating a simple request for data
// using one multi and one easy per request 

LLCurlRequest::LLCurlRequest() :
	mActiveMulti(NULL),
	mActiveRequestCount(0)
{
	mProcessing = FALSE;
}

LLCurlRequest::~LLCurlRequest()
{
	//stop all Multi handle background threads
	for (curlmulti_set_t::iterator iter = mMultiSet.begin(); iter != mMultiSet.end(); ++iter)
	{
		LLCurl::getCurlThread()->killMulti(*iter) ;
	}
	mMultiSet.clear() ;
}

void LLCurlRequest::addMulti()
{
	LLCurl::Multi* multi = new LLCurl::Multi();
	if(!multi->isValid())
	{
		LLCurl::getCurlThread()->killMulti(multi) ;
		mActiveMulti = NULL ;
		mActiveRequestCount = 0 ;
		return;
	}
	
	mMultiSet.insert(multi);
	mActiveMulti = multi;
	mActiveRequestCount = 0;
}

LLCurl::Easy* LLCurlRequest::allocEasy()
{
	if (!mActiveMulti ||
		mActiveRequestCount	>= MAX_ACTIVE_REQUEST_COUNT ||
		mActiveMulti->mErrorCount > 0)
	{
		addMulti();
	}
	if(!mActiveMulti)
	{
		return NULL ;
	}

	//llassert_always(mActiveMulti);
	++mActiveRequestCount;
	LLCurl::Easy* easy = mActiveMulti->allocEasy();
	return easy;
}

bool LLCurlRequest::addEasy(LLCurl::Easy* easy)
{
	llassert_always(mActiveMulti);
	
	if (mProcessing)
	{
		LL_ERRS() << "Posting to a LLCurlRequest instance from within a responder is not allowed (causes DNS timeouts)." << LL_ENDL;
	}
	bool res = mActiveMulti->addEasy(easy);
	return res;
}

void LLCurlRequest::get(const std::string& url, LLCurl::ResponderPtr responder)
{
	getByteRange(url, headers_t(), 0, -1, responder);
}

// Note: (length==0) is interpreted as "the rest of the file", i.e. the whole file if (offset==0) or
// the remainder of the file if not.
bool LLCurlRequest::getByteRange(const std::string& url,
								 const headers_t& headers,
								 S32 offset, S32 length,
								 LLCurl::ResponderPtr responder)
{
	llassert(LLCurl::sNotQuitting);
	LLCurl::Easy* easy = allocEasy();
	if (!easy)
	{
		return false;
	}
	easy->prepRequest(url, headers, responder);
	easy->setopt(CURLOPT_HTTPGET, 1);
	if (length > 0)
	{
		std::string range = llformat("Range: bytes=%d-%d", offset,offset+length-1);
		easy->slist_append(range.c_str());
	}
	else if (offset > 0)
	{
		std::string range = llformat("Range: bytes=%d-", offset);
		easy->slist_append(range.c_str());
	}
	easy->setHeaders();
	bool res = addEasy(easy);
	return res;
}

bool LLCurlRequest::post(const std::string& url,
						 const headers_t& headers,
						 const LLSD& data,
						 LLCurl::ResponderPtr responder, S32 time_out)
{
	llassert(LLCurl::sNotQuitting);
	LLCurl::Easy* easy = allocEasy();
	if (!easy)
	{
		return false;
	}
	easy->prepRequest(url, headers, responder, time_out);

	LLSDSerialize::toXML(data, easy->getInput());
	S32 bytes = easy->getInput().str().length();
	
	easy->setopt(CURLOPT_POST, 1);
	easy->setopt(CURLOPT_POSTFIELDS, (void*)NULL);
	easy->setopt(CURLOPT_POSTFIELDSIZE, bytes);

	easy->slist_append("Content-Type: application/llsd+xml");
	easy->setHeaders();

	LL_DEBUGS() << "POSTING: " << bytes << " bytes." << LL_ENDL;
	bool res = addEasy(easy);
	return res;
}

bool LLCurlRequest::post(const std::string& url,
						 const headers_t& headers,
						 const std::string& data,
						 LLCurl::ResponderPtr responder, S32 time_out)
{
	llassert(LLCurl::sNotQuitting);
	LLCurl::Easy* easy = allocEasy();
	if (!easy)
	{
		return false;
	}
	easy->prepRequest(url, headers, responder, time_out);

	easy->getInput().write(data.data(), data.size());
	S32 bytes = easy->getInput().str().length();
	
	easy->setopt(CURLOPT_POST, 1);
	easy->setopt(CURLOPT_POSTFIELDS, (void*)NULL);
	easy->setopt(CURLOPT_POSTFIELDSIZE, bytes);

	easy->slist_append("Content-Type: application/octet-stream");
	easy->setHeaders();

	LL_DEBUGS() << "POSTING: " << bytes << " bytes." << LL_ENDL;
	bool res = addEasy(easy);
	return res;
}

// Note: call once per frame
S32 LLCurlRequest::process()
{
	S32 res = 0;

	mProcessing = TRUE;
	for (curlmulti_set_t::iterator iter = mMultiSet.begin();
		 iter != mMultiSet.end(); )
	{
		curlmulti_set_t::iterator curiter = iter++;
		LLCurl::Multi* multi = *curiter;

		if(!multi->isValid())
		{
			if(multi == mActiveMulti)
			{				
				mActiveMulti = NULL ;
				mActiveRequestCount = 0 ;
			}
			mMultiSet.erase(curiter) ;
			LLCurl::getCurlThread()->killMulti(multi) ;
			continue ;
		}

		S32 tres = multi->process();
		res += tres;
		if (multi != mActiveMulti && tres == 0 && multi->mQueued == 0)
		{
			mMultiSet.erase(curiter);
			LLCurl::getCurlThread()->killMulti(multi);
		}
	}
	mProcessing = FALSE;
	return res;
}

S32 LLCurlRequest::getQueued()
{
	S32 queued = 0;
	for (curlmulti_set_t::iterator iter = mMultiSet.begin();
		 iter != mMultiSet.end(); )
	{
		curlmulti_set_t::iterator curiter = iter++;
		LLCurl::Multi* multi = *curiter;
		
		if(!multi->isValid())
		{
			if(multi == mActiveMulti)
			{				
				mActiveMulti = NULL ;
				mActiveRequestCount = 0 ;
			}
			LLCurl::getCurlThread()->killMulti(multi);
			mMultiSet.erase(curiter) ;
			continue ;
		}

		queued += multi->mQueued;
		if (multi->getState() != LLCurl::Multi::STATE_READY)
		{
			++queued;
		}
	}
	return queued;
}

LLCurlTextureRequest::LLCurlTextureRequest(S32 concurrency) : 
	LLCurlRequest(), 
	mConcurrency(concurrency),
	mInQueue(0),
	mMutex(NULL),
	mHandleCounter(1),
	mTotalIssuedRequests(0),
	mTotalReceivedBits(0)
{
	mGlobalTimer.reset();
}

LLCurlTextureRequest::~LLCurlTextureRequest()
{
	mRequestMap.clear();

	for(req_queue_t::iterator iter = mCachedRequests.begin(); iter != mCachedRequests.end(); ++iter)
	{
		delete *iter;
	}
	mCachedRequests.clear();
}

//return 0: success
// > 0: cached handle
U32 LLCurlTextureRequest::getByteRange(const std::string& url,
								 const headers_t& headers,
								 S32 offset, S32 length, U32 pri,
								 LLCurl::ResponderPtr responder, F32 delay_time)
{
	U32 ret_val = 0;
	bool success = false;	

	if(mInQueue < mConcurrency && delay_time < 0.f)
	{
		success = LLCurlRequest::getByteRange(url, headers, offset, length, responder);		
	}

	LLMutexLock lock(&mMutex);

	if(success)
	{
		mInQueue++;
		mTotalIssuedRequests++;
	}
	else
	{
		request_t* request = new request_t(mHandleCounter, url, headers, offset, length, pri, responder);
		if(delay_time > 0.f)
		{
			request->mStartTime = mGlobalTimer.getElapsedTimeF32() + delay_time;
		}

		mCachedRequests.insert(request);
		mRequestMap[mHandleCounter] = request;
		ret_val = mHandleCounter;
		mHandleCounter++;

		if(!mHandleCounter)
		{
			mHandleCounter = 1;
		}
	}

	return ret_val;
}

void LLCurlTextureRequest::completeRequest(S32 received_bytes)
{
	LLMutexLock lock(&mMutex);

	llassert_always(mInQueue > 0);

	mInQueue--;
	mTotalReceivedBits += received_bytes * 8;
}

void LLCurlTextureRequest::nextRequests()
{
	if(mCachedRequests.empty() || mInQueue >= mConcurrency)
	{
		return;
	}

	F32 cur_time = mGlobalTimer.getElapsedTimeF32();

	req_queue_t::iterator iter;	
	{
		LLMutexLock lock(&mMutex);
		iter = mCachedRequests.begin();
	}
	while(1)
	{
		request_t* request = *iter;
		if(request->mStartTime < cur_time)
		{
			if(!LLCurlRequest::getByteRange(request->mUrl, request->mHeaders, request->mOffset, request->mLength, request->mResponder))
			{
				break;
			}

			LLMutexLock lock(&mMutex);
			++iter;
			mInQueue++;
			mTotalIssuedRequests++;
			mCachedRequests.erase(request);
			mRequestMap.erase(request->mHandle);
			delete request;

			if(iter == mCachedRequests.end() || mInQueue >= mConcurrency)
			{
				break;
			}
		}
		else
		{
			LLMutexLock lock(&mMutex);
			++iter;
			if(iter == mCachedRequests.end() || mInQueue >= mConcurrency)
			{
				break;
			}
		}
	}

	return;
}

void LLCurlTextureRequest::updatePriority(U32 handle, U32 pri)
{
	if(!handle)
	{
		return;
	}

	LLMutexLock lock(&mMutex);

	std::map<S32, request_t*>::iterator iter = mRequestMap.find(handle);
	if(iter != mRequestMap.end())
	{
		request_t* req = iter->second;
		
		if(req->mPriority != pri)
		{
			mCachedRequests.erase(req);
			req->mPriority = pri;
			mCachedRequests.insert(req);
		}
	}
}

void LLCurlTextureRequest::removeRequest(U32 handle)
{
	if(!handle)
	{
		return;
	}

	LLMutexLock lock(&mMutex);

	std::map<S32, request_t*>::iterator iter = mRequestMap.find(handle);
	if(iter != mRequestMap.end())
	{
		request_t* req = iter->second;
		mRequestMap.erase(iter);
		mCachedRequests.erase(req);
		delete req;
	}
}

bool LLCurlTextureRequest::isWaiting(U32 handle)
{
	if(!handle)
	{
		return false;
	}

	LLMutexLock lock(&mMutex);
	return mRequestMap.find(handle) != mRequestMap.end();
}

U32 LLCurlTextureRequest::getTotalReceivedBits()
{
	LLMutexLock lock(&mMutex);

	U32 bits = mTotalReceivedBits;
	mTotalReceivedBits = 0;
	return bits;
}

U32 LLCurlTextureRequest::getTotalIssuedRequests()
{
	LLMutexLock lock(&mMutex);
	return mTotalIssuedRequests;
}

S32 LLCurlTextureRequest::getNumRequests()
{
	LLMutexLock lock(&mMutex);
	return mInQueue;
}

////////////////////////////////////////////////////////////////////////////
// For generating one easy request
// associated with a single multi request

LLCurlEasyRequest::LLCurlEasyRequest()
	: mRequestSent(false),
	  mResultReturned(false)
{
	mMulti = new LLCurl::Multi();
	
	if(mMulti->isValid())
	{
	mEasy = mMulti->allocEasy();
	if (mEasy)
	{
		mEasy->setErrorBuffer();
		mEasy->setCA();
		// Set proxy settings if configured to do so.
		LLProxy::getInstance()->applyProxySettings(mEasy);
	}
}
	else
	{
		LLCurl::getCurlThread()->killMulti(mMulti) ;
		mEasy = NULL ;
		mMulti = NULL ;
	}
}

LLCurlEasyRequest::~LLCurlEasyRequest()
{
	LLCurl::getCurlThread()->killMulti(mMulti) ;
}
	
void LLCurlEasyRequest::setopt(CURLoption option, S32 value)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(option, value);
	}
}

void LLCurlEasyRequest::setoptString(CURLoption option, const std::string& value)
{
	if (isValid() && mEasy)
	{
		mEasy->setoptString(option, value);
	}
}

void LLCurlEasyRequest::setPost(char* postdata, S32 size)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_POST, 1);
		mEasy->setopt(CURLOPT_POSTFIELDS, postdata);
		mEasy->setopt(CURLOPT_POSTFIELDSIZE, size);
	}
}

void LLCurlEasyRequest::setHeaderCallback(curl_header_callback callback, void* userdata)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_HEADERFUNCTION, (void*)callback);
		mEasy->setopt(CURLOPT_HEADERDATA, userdata); // aka CURLOPT_WRITEHEADER
	}
}

void LLCurlEasyRequest::setWriteCallback(curl_write_callback callback, void* userdata)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_WRITEFUNCTION, (void*)callback);
		mEasy->setopt(CURLOPT_WRITEDATA, userdata);
	}
}

void LLCurlEasyRequest::setReadCallback(curl_read_callback callback, void* userdata)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_READFUNCTION, (void*)callback);
		mEasy->setopt(CURLOPT_READDATA, userdata);
	}
}

void LLCurlEasyRequest::setSSLCtxCallback(curl_ssl_ctx_callback callback, void* userdata)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_SSL_CTX_FUNCTION, (void*)callback);
		mEasy->setopt(CURLOPT_SSL_CTX_DATA, userdata);
	}
}

void LLCurlEasyRequest::slist_append(const char* str)
{
	if (isValid() && mEasy)
	{
		mEasy->slist_append(str);
	}
}

void LLCurlEasyRequest::sendRequest(const std::string& url)
{
	llassert_always(!mRequestSent);
	mRequestSent = true;
	LL_DEBUGS() << url << LL_ENDL;
	if (isValid() && mEasy)
	{
		mEasy->setHeaders();
		mEasy->setoptString(CURLOPT_URL, url);
		mMulti->addEasy(mEasy);
	}
}

void LLCurlEasyRequest::requestComplete()
{
	llassert_always(mRequestSent);
	mRequestSent = false;
	if (isValid() && mEasy)
	{
		mMulti->removeEasy(mEasy);
	}
}

// Usage: Call getRestult until it returns false (no more messages)
bool LLCurlEasyRequest::getResult(CURLcode* result, LLCurl::TransferInfo* info)
{
	if(!isValid())
	{
		return false ;
	}
	if (!mMulti->isCompleted())
	{ //we're busy, try again later
		return false;
	}
	mMulti->setState(LLCurl::Multi::STATE_READY) ;

	if (!mEasy)
	{
		// Special case - we failed to initialize a curl_easy (can happen if too many open files)
		//  Act as though the request failed to connect
		if (mResultReturned)
		{
			return false;
		}
		else
		{
			*result = CURLE_FAILED_INIT;
			mResultReturned = true;
			return true;
		}
	}
	// In theory, info_read might return a message with a status other than CURLMSG_DONE
	// In practice for all messages returned, msg == CURLMSG_DONE
	// Ignore other messages just in case
	while(1)
	{
		S32 q;
		CURLMsg* curlmsg = info_read(&q, info);
		if (curlmsg)
		{
			if (curlmsg->msg == CURLMSG_DONE)
			{
				*result = curlmsg->data.result;			
				return true;
			}
			// else continue
		}
		else
		{
			return false;
		}
	}
}

// private
CURLMsg* LLCurlEasyRequest::info_read(S32* q, LLCurl::TransferInfo* info)
{
	if (mEasy)
	{
		CURLMsg* curlmsg = mMulti->info_read(q);
		if (curlmsg && curlmsg->msg == CURLMSG_DONE)
		{
			if (info)
			{
				mEasy->getTransferInfo(info);
			}
		}
		return curlmsg;
	}
	return NULL;
}

std::string LLCurlEasyRequest::getErrorString()
{
	return isValid() &&  mEasy ? std::string(mEasy->getErrorBuffer()) : std::string();
}

////////////////////////////////////////////////////////////////////////////

#if SAFE_SSL
//static
void LLCurl::ssl_locking_callback(int mode, int type, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
	{
		LLCurl::sSSLMutex[type]->lock();
	}
	else
	{
		LLCurl::sSSLMutex[type]->unlock();
	}
}

//static
unsigned long LLCurl::ssl_thread_id(void)
{
	return LLThread::currentID();
}
#endif

void LLCurl::initClass(F32 curl_reuest_timeout, S32 max_number_handles, bool multi_threaded)
{
	sCurlRequestTimeOut = curl_reuest_timeout ; //seconds
	sMaxHandles = max_number_handles ; //max number of handles, (multi handles and easy handles combined).

	// Do not change this "unless you are familiar with and mean to control 
	// internal operations of libcurl"
	// - http://curl.haxx.se/libcurl/c/curl_global_init.html
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);

	check_curl_code(code);
	
#if SAFE_SSL
	S32 mutex_count = CRYPTO_num_locks();
	for (S32 i=0; i<mutex_count; i++)
	{
		sSSLMutex.push_back(new LLMutex(NULL));
	}
	CRYPTO_set_id_callback(&LLCurl::ssl_thread_id);
	CRYPTO_set_locking_callback(&LLCurl::ssl_locking_callback);
#endif

	sCurlThread = new LLCurlThread(multi_threaded) ;
	if(multi_threaded)
	{
		sHandleMutexp = new LLMutex(NULL) ;
		Easy::sHandleMutexp = new LLMutex(NULL) ;
	}
}

void LLCurl::cleanupClass()
{
	sNotQuitting = false; //set quitting

	//shut down curl thread
	while(1)
	{
		if(!sCurlThread->update(1)) //finish all tasks
		{
			break ;
		}
	}
	LL_CHECK_MEMORY
	sCurlThread->shutdown() ;
	LL_CHECK_MEMORY
	delete sCurlThread ;
	sCurlThread = NULL ;
	LL_CHECK_MEMORY

#if SAFE_SSL
	CRYPTO_set_locking_callback(NULL);
	for_each(sSSLMutex.begin(), sSSLMutex.end(), DeletePointer());
	sSSLMutex.clear();
#endif
	
	LL_CHECK_MEMORY

	for (std::set<CURL*>::iterator iter = Easy::sFreeHandles.begin(); iter != Easy::sFreeHandles.end(); ++iter)
	{
		CURL* curl = *iter;
		LLCurl::deleteEasyHandle(curl);
	}
	
	LL_CHECK_MEMORY

	Easy::sFreeHandles.clear();

	LL_CHECK_MEMORY

	delete Easy::sHandleMutexp ;
	Easy::sHandleMutexp = NULL ;

	LL_CHECK_MEMORY

	delete sHandleMutexp ;
	sHandleMutexp = NULL ;

	LL_CHECK_MEMORY

	// removed as per https://jira.secondlife.com/browse/SH-3115
	//llassert(Easy::sActiveHandles.empty());
}

//static 
CURLM* LLCurl::newMultiHandle()
{
	llassert(sNotQuitting);

	LLMutexLock lock(sHandleMutexp) ;

	if(sTotalHandles + 1 > sMaxHandles)
	{
		LL_WARNS() << "no more handles available." << LL_ENDL ;
		return NULL ; //failed
	}
	sTotalHandles++;

	CURLM* ret = curl_multi_init() ;
	if(!ret)
	{
		LL_WARNS() << "curl_multi_init failed." << LL_ENDL ;
	}

	return ret ;
}

//static 
CURLMcode  LLCurl::deleteMultiHandle(CURLM* handle)
{
	if(handle)
	{
		LLMutexLock lock(sHandleMutexp) ;		
		sTotalHandles-- ;
		return curl_multi_cleanup(handle) ;
	}
	return CURLM_OK ;
}

//static 
CURL*  LLCurl::newEasyHandle()
{
	llassert(sNotQuitting);
	LLMutexLock lock(sHandleMutexp) ;

	if(sTotalHandles + 1 > sMaxHandles)
	{
		LL_WARNS() << "no more handles available." << LL_ENDL ;
		return NULL ; //failed
	}
	sTotalHandles++;

	CURL* ret = createStandardCurlHandle();
	if(!ret)
	{
		LL_WARNS() << "failed to create curl handle." << LL_ENDL ;
	}

	return ret ;
}

//static 
void  LLCurl::deleteEasyHandle(CURL* handle)
{
	if(handle)
	{
		LLMutexLock lock(sHandleMutexp) ;
		LL_CHECK_MEMORY
		curl_easy_cleanup(handle) ;
		LL_CHECK_MEMORY
		sTotalHandles-- ;
	}
}

const unsigned int LLCurl::MAX_REDIRECTS = 5;

// Provide access to LLCurl free functions outside of llcurl.cpp without polluting the global namespace.
void LLCurlFF::check_easy_code(CURLcode code)
{
	check_curl_code(code);
}
void LLCurlFF::check_multi_code(CURLMcode code)
{
	check_curl_multi_code(code);
}


// Static
CURL* LLCurl::createStandardCurlHandle()
{
	if (sCurlTemplateStandardHandle == NULL)
	{	// Late creation of the template curl handle
		sCurlTemplateStandardHandle = curl_easy_init();
		if (sCurlTemplateStandardHandle == NULL)
		{
			LL_WARNS() << "curl error calling curl_easy_init()" << LL_ENDL;
		}
		else
		{
			CURLcode result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
			check_curl_code(result);
			result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_NOSIGNAL, 1);
			check_curl_code(result);
			result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_NOPROGRESS, 1);
			check_curl_code(result);
			result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_ENCODING, "");	
			check_curl_code(result);
			result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_AUTOREFERER, 1);
			check_curl_code(result);
			result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_FOLLOWLOCATION, 1);	
			check_curl_code(result);
			result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_SSL_VERIFYPEER, 1);
			check_curl_code(result);
			result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_SSL_VERIFYHOST, 0);
			check_curl_code(result);

			// The Linksys WRT54G V5 router has an issue with frequent
			// DNS lookups from LAN machines.  If they happen too often,
			// like for every HTTP request, the router gets annoyed after
			// about 700 or so requests and starts issuing TCP RSTs to
			// new connections.  Reuse the DNS lookups for even a few
			// seconds and no RSTs.
			result = curl_easy_setopt(sCurlTemplateStandardHandle, CURLOPT_DNS_CACHE_TIMEOUT, 15);
			check_curl_code(result);
		}
	}

	return curl_easy_duphandle(sCurlTemplateStandardHandle);
}
