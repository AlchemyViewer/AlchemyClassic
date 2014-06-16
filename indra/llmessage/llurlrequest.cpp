/** 
 * @file llurlrequest.cpp
 * @author Phoenix
 * @date 2005-04-28
 * @brief Implementation of the URLRequest class and related classes.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
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
#include "llurlrequest.h"

#include <algorithm>
#include <openssl/x509_vfy.h>
#include <openssl/ssl.h>
#include "llcurl.h"
#include "llfasttimer.h"
#include "llioutil.h"
#include "llproxy.h"
#include "llpumpio.h"
#include "llsd.h"
#include "llstring.h"
#include "apr_env.h"
#include "llapr.h"

/**
 * String constants
 */
const std::string CONTEXT_DEST_URI_SD_LABEL("dest_uri");
const std::string CONTEXT_TRANSFERED_BYTES("transfered_bytes");

// These are defined in llhttpnode.h/llhttpnode.cpp
extern const std::string CONTEXT_REQUEST;
extern const std::string CONTEXT_RESPONSE;

static size_t headerCallback(void* data, size_t size, size_t nmemb, void* user);

/**
 * class LLURLRequestDetail
 */
class LLURLRequestDetail
{
public:
	LLURLRequestDetail();
	~LLURLRequestDetail();
	std::string mURL;
	LLCurlEasyRequest* mCurlRequest;
	LLIOPipe::buffer_ptr_t mResponseBuffer;
	LLChannelDescriptors mChannels;
	U8* mLastRead;
	U32 mBodyLimit;
	S32 mByteAccumulator;
	bool mIsBodyLimitSet;
	LLURLRequest::SSLCertVerifyCallback mSSLVerifyCallback;
};

LLURLRequestDetail::LLURLRequestDetail() :
	mCurlRequest(NULL),
	mLastRead(NULL),
	mBodyLimit(0),
	mByteAccumulator(0),
	mIsBodyLimitSet(false),
    mSSLVerifyCallback(NULL)
{
	mCurlRequest = new LLCurlEasyRequest();
	
	if(!mCurlRequest->isValid()) //failed.
	{
		delete mCurlRequest ;
		mCurlRequest = NULL ;
	}
}

LLURLRequestDetail::~LLURLRequestDetail()
{
	delete mCurlRequest;
	mLastRead = NULL;
}

void LLURLRequest::setSSLVerifyCallback(SSLCertVerifyCallback callback, void *param)
{
	mDetail->mSSLVerifyCallback = callback;
	mDetail->mCurlRequest->setSSLCtxCallback(LLURLRequest::_sslCtxCallback, (void *)this);
	mDetail->mCurlRequest->setopt(CURLOPT_SSL_VERIFYPEER, true);
	mDetail->mCurlRequest->setopt(CURLOPT_SSL_VERIFYHOST, 2);	
}


// _sslCtxFunction
// Callback function called when an SSL Context is created via CURL
// used to configure the context for custom cert validation

CURLcode LLURLRequest::_sslCtxCallback(CURL * curl, void *sslctx, void *param)
{	
	LLURLRequest *req = (LLURLRequest *)param;
	if(req == NULL || req->mDetail->mSSLVerifyCallback == NULL)
	{
		SSL_CTX_set_cert_verify_callback((SSL_CTX *)sslctx, NULL, NULL);
		return CURLE_OK;
	}
	SSL_CTX * ctx = (SSL_CTX *) sslctx;
	// disable any default verification for server certs
	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	// set the verification callback.
	SSL_CTX_set_cert_verify_callback(ctx, req->mDetail->mSSLVerifyCallback, (void *)req);
	// the calls are void
	return CURLE_OK;
	
}

/**
 * class LLURLRequest
 */


LLURLRequest::LLURLRequest(EHTTPMethod action, bool follow_redirects /* = true */) :
	mAction(action),
	mFollowRedirects(follow_redirects)
{
	initialize();
}

LLURLRequest::LLURLRequest(
	EHTTPMethod action,
	const std::string& url,
	bool follow_redirects /* = true */) :
	mAction(action),
	mFollowRedirects(follow_redirects)
{
	initialize();
	setURL(url);
}

LLURLRequest::~LLURLRequest()
{
	delete mDetail;
	mDetail = NULL ;
}

void LLURLRequest::setURL(const std::string& url)
{
	mDetail->mURL = url;
	if (url.empty())
	{
		LL_WARNS() << "empty URL specified" << LL_ENDL;
	}
}

const std::string& LLURLRequest::getURL() const
{
	return mDetail->mURL;
}

void LLURLRequest::addHeader(const std::string& header, const std::string& value /* = "" */)
{
	mDetail->mCurlRequest->slist_append(header, value);
}

void LLURLRequest::addHeaderRaw(const char* header)
{
	mDetail->mCurlRequest->slist_append(header);
}

void LLURLRequest::setBodyLimit(U32 size)
{
	mDetail->mBodyLimit = size;
	mDetail->mIsBodyLimitSet = true;
}

void LLURLRequest::setCallback(LLURLRequestComplete* callback)
{
	mCompletionCallback = callback;
	mDetail->mCurlRequest->setHeaderCallback(&headerCallback, (void*)callback);
}

// Added to mitigate the effect of libcurl looking
// for the ALL_PROXY and http_proxy env variables
// and deciding to insert a Pragma: no-cache
// header! The only usage of this method at the
// time of this writing is in llhttpclient.cpp
// in the request() method, where this method
// is called with use_proxy = FALSE
void LLURLRequest::useProxy(bool use_proxy)
{
    static char *env_proxy;

    if (use_proxy && (env_proxy == NULL))
    {
        apr_status_t status;
        LLAPRPool pool;
		status = apr_env_get(&env_proxy, "ALL_PROXY", pool.getAPRPool());
        if (status != APR_SUCCESS)
        {
			status = apr_env_get(&env_proxy, "http_proxy", pool.getAPRPool());
        }
        if (status != APR_SUCCESS)
        {
           use_proxy = FALSE;
        }
    }


    LL_DEBUGS() << "use_proxy = " << (use_proxy?'Y':'N') << ", env_proxy = " << (env_proxy ? env_proxy : "(null)") << LL_ENDL;

    if (env_proxy && use_proxy)
    {
		mDetail->mCurlRequest->setoptString(CURLOPT_PROXY, env_proxy);
    }
    else
    {
        mDetail->mCurlRequest->setoptString(CURLOPT_PROXY, "");
    }
}

void LLURLRequest::useProxy(const std::string &proxy)
{
    mDetail->mCurlRequest->setoptString(CURLOPT_PROXY, proxy);
}

void LLURLRequest::allowCookies()
{
	mDetail->mCurlRequest->setoptString(CURLOPT_COOKIEFILE, "");
}

//virtual 
bool LLURLRequest::isValid() 
{
	return mDetail->mCurlRequest && mDetail->mCurlRequest->isValid(); 
}

// virtual
LLIOPipe::EStatus LLURLRequest::handleError(
	LLIOPipe::EStatus status,
	LLPumpIO* pump)
{
	if(!isValid())
	{
		return STATUS_EXPIRED ;
	}

	if(mCompletionCallback && pump)
	{
		LLURLRequestComplete* complete = NULL;
		complete = (LLURLRequestComplete*)mCompletionCallback.get();
		complete->httpStatus(
			HTTP_INTERNAL_ERROR,
			LLIOPipe::lookupStatusString(status));
		complete->responseStatus(status);
		pump->respond(complete);
		mCompletionCallback = NULL;
	}
	return status;
}

static LLTrace::BlockTimerStatHandle FTM_PROCESS_URL_REQUEST("URL Request");
static LLTrace::BlockTimerStatHandle FTM_PROCESS_URL_REQUEST_GET_RESULT("Get Result");
static LLTrace::BlockTimerStatHandle FTM_URL_PERFORM("Perform");
static LLTrace::BlockTimerStatHandle FTM_PROCESS_URL_PUMP_RESPOND("Pump Respond");
static LLTrace::BlockTimerStatHandle FTM_URL_ADJUST_TIMEOUT("Adjust Timeout");

// virtual
LLIOPipe::EStatus LLURLRequest::process_impl(
	const LLChannelDescriptors& channels,
	buffer_ptr_t& buffer,
	bool& eos,
	LLSD& context,
	LLPumpIO* pump)
{
	LL_RECORD_BLOCK_TIME(FTM_PROCESS_URL_REQUEST);
	PUMP_DEBUG;
	//LL_INFOS() << "LLURLRequest::process_impl()" << LL_ENDL;
	if (!buffer) return STATUS_ERROR;
	
	// we're still waiting or prcessing, check how many
	// bytes we have accumulated.
	const S32 MIN_ACCUMULATION = 100000;
	if(pump && (mDetail->mByteAccumulator > MIN_ACCUMULATION))
	{
		LL_RECORD_BLOCK_TIME(FTM_URL_ADJUST_TIMEOUT);
		 // This is a pretty sloppy calculation, but this
		 // tries to make the gross assumption that if data
		 // is coming in at 56kb/s, then this transfer will
		 // probably succeed. So, if we're accumlated
		 // 100,000 bytes (MIN_ACCUMULATION) then let's
		 // give this client another 2s to complete.
		 const F32 TIMEOUT_ADJUSTMENT = 2.0f;
		 mDetail->mByteAccumulator = 0;
		 pump->adjustTimeoutSeconds(TIMEOUT_ADJUSTMENT);
		 LL_DEBUGS() << "LLURLRequest adjustTimeoutSeconds for request: " << mDetail->mURL << LL_ENDL;
		 if (mState == STATE_INITIALIZED)
		 {
			  LL_INFOS() << "LLURLRequest adjustTimeoutSeconds called during upload" << LL_ENDL;
		 }
	}

	switch(mState)
	{
	case STATE_INITIALIZED:
	{
		PUMP_DEBUG;
		// We only need to wait for input if we are uploading
		// something.
		if(((HTTP_PUT == mAction) || (HTTP_POST == mAction)) && !eos)
		{
			// we're waiting to get all of the information
			return STATUS_BREAK;
		}

		// *FIX: bit of a hack, but it should work. The configure and
		// callback method expect this information to be ready.
		mDetail->mResponseBuffer = buffer;
		mDetail->mChannels = channels;
		if(!configure())
		{
			return STATUS_ERROR;
		}
		mState = STATE_WAITING_FOR_RESPONSE;

		// *FIX: Maybe we should just go to the next state now...
		return STATUS_BREAK;
	}
	case STATE_WAITING_FOR_RESPONSE:
	case STATE_PROCESSING_RESPONSE:
	{
		PUMP_DEBUG;
		LLIOPipe::EStatus status = STATUS_BREAK;
		{
			LL_RECORD_BLOCK_TIME(FTM_URL_PERFORM);
			if(!mDetail->mCurlRequest->wait())
			{
				return status ;
			}
		}

		bool keep_looping = true;
		while(keep_looping)
		{
			CURLcode result;

			bool newmsg = false;
			{
				LL_RECORD_BLOCK_TIME(FTM_PROCESS_URL_REQUEST_GET_RESULT);
				newmsg = mDetail->mCurlRequest->getResult(&result);
			}
		
			if(!newmsg)
			{
				// keep processing
				break;
			}
		

			mState = STATE_HAVE_RESPONSE;
			context[CONTEXT_REQUEST][CONTEXT_TRANSFERED_BYTES] = mRequestTransferedBytes;
			context[CONTEXT_RESPONSE][CONTEXT_TRANSFERED_BYTES] = mResponseTransferedBytes;
			LL_DEBUGS() << this << "Setting context to " << context << LL_ENDL;
			switch(result)
			{
				case CURLE_OK:
				case CURLE_WRITE_ERROR:
					// NB: The error indication means that we stopped the
					// writing due the body limit being reached
					if(mCompletionCallback && pump)
					{
						LLURLRequestComplete* complete = NULL;
						complete = (LLURLRequestComplete*)
							mCompletionCallback.get();
						complete->responseStatus(
								result == CURLE_OK
									? STATUS_OK : STATUS_STOP);
						LLPumpIO::links_t chain;
						LLPumpIO::LLLinkInfo link;
						link.mPipe = mCompletionCallback;
						link.mChannels = LLBufferArray::makeChannelConsumer(
							channels);
						chain.push_back(link);
						{
							LL_RECORD_BLOCK_TIME(FTM_PROCESS_URL_PUMP_RESPOND);
							pump->respond(chain, buffer, context);
						}
						mCompletionCallback = NULL;
					}
					break;
				case CURLE_FAILED_INIT:
				case CURLE_COULDNT_CONNECT:
					status = STATUS_NO_CONNECTION;
					keep_looping = false;
					break;
				default:			// CURLE_URL_MALFORMAT
					LL_WARNS() << "URLRequest Error: " << result
							<< ", "
							<< LLCurl::strerror(result)
							<< ", "
							<< (mDetail->mURL.empty() ? "<EMPTY URL>" : mDetail->mURL)
							<< LL_ENDL;
					status = STATUS_ERROR;
					keep_looping = false;
					break;
			}
		}
		return status;
	}
	case STATE_HAVE_RESPONSE:
		PUMP_DEBUG;
		// we already stuffed everything into channel in in the curl
		// callback, so we are done.
		eos = true;
		context[CONTEXT_REQUEST][CONTEXT_TRANSFERED_BYTES] = mRequestTransferedBytes;
		context[CONTEXT_RESPONSE][CONTEXT_TRANSFERED_BYTES] = mResponseTransferedBytes;
		LL_DEBUGS() << this << "Setting context to " << context << LL_ENDL;
		return STATUS_DONE;

	default:
		PUMP_DEBUG;
		context[CONTEXT_REQUEST][CONTEXT_TRANSFERED_BYTES] = mRequestTransferedBytes;
		context[CONTEXT_RESPONSE][CONTEXT_TRANSFERED_BYTES] = mResponseTransferedBytes;
		LL_DEBUGS() << this << "Setting context to " << context << LL_ENDL;
		return STATUS_ERROR;
	}
}

void LLURLRequest::initialize()
{
	mState = STATE_INITIALIZED;
	mDetail = new LLURLRequestDetail;

	if(!isValid())
	{
		return ;
	}

	mDetail->mCurlRequest->setopt(CURLOPT_NOSIGNAL, 1);
	mDetail->mCurlRequest->setWriteCallback(&downCallback, (void*)this);
	mDetail->mCurlRequest->setReadCallback(&upCallback, (void*)this);
	mRequestTransferedBytes = 0;
	mResponseTransferedBytes = 0;
}

static LLTrace::BlockTimerStatHandle FTM_URL_REQUEST_CONFIGURE("URL Configure");
bool LLURLRequest::configure()
{
	LL_RECORD_BLOCK_TIME(FTM_URL_REQUEST_CONFIGURE);
	
	bool rv = false;
	S32 bytes = mDetail->mResponseBuffer->countAfter(
   		mDetail->mChannels.in(),
		NULL);
	switch(mAction)
	{
	case HTTP_HEAD:
		mDetail->mCurlRequest->setopt(CURLOPT_HEADER, 1);
		mDetail->mCurlRequest->setopt(CURLOPT_NOBODY, 1);
		if (mFollowRedirects)
		{
			mDetail->mCurlRequest->setopt(CURLOPT_FOLLOWLOCATION, 1);
		}
		rv = true;
		break;
	case HTTP_GET:
		mDetail->mCurlRequest->setopt(CURLOPT_HTTPGET, 1);
		if (mFollowRedirects)
		{
			mDetail->mCurlRequest->setopt(CURLOPT_FOLLOWLOCATION, 1);
		}

		// Set Accept-Encoding to allow response compression
		mDetail->mCurlRequest->setoptString(CURLOPT_ENCODING, "");
		rv = true;
		break;

	case HTTP_PUT:
		// Disable the expect http 1.1 extension. POST and PUT default
		// to turning this on, and I am not too sure what it means.
		addHeader(HTTP_OUT_HEADER_EXPECT);

		mDetail->mCurlRequest->setopt(CURLOPT_UPLOAD, 1);
		mDetail->mCurlRequest->setopt(CURLOPT_INFILESIZE, bytes);
		rv = true;
		break;

	case HTTP_PATCH:
		// Disable the expect http 1.1 extension. POST and PUT default
		// to turning this on, and I am not too sure what it means.
		addHeader(HTTP_OUT_HEADER_EXPECT);

		mDetail->mCurlRequest->setopt(CURLOPT_UPLOAD, 1);
		mDetail->mCurlRequest->setopt(CURLOPT_INFILESIZE, bytes);
		mDetail->mCurlRequest->setoptString(CURLOPT_CUSTOMREQUEST, "PATCH");
		rv = true;
		break;

	case HTTP_POST:
		// Disable the expect http 1.1 extension. POST and PUT default
		// to turning this on, and I am not too sure what it means.
		addHeader(HTTP_OUT_HEADER_EXPECT);

		// Disable the content type http header.
		// *FIX: what should it be?
		addHeader(HTTP_OUT_HEADER_CONTENT_TYPE);

		// Set the handle for an http post
		mDetail->mCurlRequest->setPost(NULL, bytes);

		// Set Accept-Encoding to allow response compression
		mDetail->mCurlRequest->setoptString(CURLOPT_ENCODING, "");
		rv = true;
		break;

	case HTTP_DELETE:
		// Set the handle for an http delete
		mDetail->mCurlRequest->setoptString(CURLOPT_CUSTOMREQUEST, "DELETE");
		rv = true;
		break;

	case HTTP_COPY:
		// Set the handle for an http copy
		mDetail->mCurlRequest->setoptString(CURLOPT_CUSTOMREQUEST, "COPY");
		rv = true;
		break;

	case HTTP_MOVE:
		// Set the handle for an http move
		mDetail->mCurlRequest->setoptString(CURLOPT_CUSTOMREQUEST, "MOVE");
		// *NOTE: should we check for the Destination header?
		rv = true;
		break;

	default:
		LL_WARNS() << "Unhandled URLRequest action: " << mAction << LL_ENDL;
		break;
	}
	if(rv)
	{
		mDetail->mCurlRequest->sendRequest(mDetail->mURL);
	}
	return rv;
}

// static
size_t LLURLRequest::downCallback(
	char* data,
	size_t size,
	size_t nmemb,
	void* user)
{
	LLURLRequest* req = (LLURLRequest*)user;
	if(STATE_WAITING_FOR_RESPONSE == req->mState)
	{
		req->mState = STATE_PROCESSING_RESPONSE;
	}
	U32 bytes = size * nmemb;
	if (req->mDetail->mIsBodyLimitSet)
	{
		if (bytes > req->mDetail->mBodyLimit)
		{
			bytes = req->mDetail->mBodyLimit;
			req->mDetail->mBodyLimit = 0;
		}
		else
		{
			req->mDetail->mBodyLimit -= bytes;
		}
	}

	req->mDetail->mResponseBuffer->append(
		req->mDetail->mChannels.out(),
		(U8*)data,
		bytes);
	req->mResponseTransferedBytes += bytes;
	req->mDetail->mByteAccumulator += bytes;
	return bytes;
}

// static
size_t LLURLRequest::upCallback(
	char* data,
	size_t size,
	size_t nmemb,
	void* user)
{
	LLURLRequest* req = (LLURLRequest*)user;
	S32 bytes = llmin(
		(S32)(size * nmemb),
		req->mDetail->mResponseBuffer->countAfter(
			req->mDetail->mChannels.in(),
			req->mDetail->mLastRead));
	req->mDetail->mLastRead =  req->mDetail->mResponseBuffer->readAfter(
		req->mDetail->mChannels.in(),
		req->mDetail->mLastRead,
		(U8*)data,
		bytes);
	req->mRequestTransferedBytes += bytes;
	return bytes;
}

static size_t headerCallback(void* data, size_t size, size_t nmemb, void* user)
{
	const char* header_line = (const char*)data;
	size_t header_len = size * nmemb;
	LLURLRequestComplete* complete = (LLURLRequestComplete*)user;

	if (!complete || !header_line)
	{
		return header_len;
	}

	// *TODO: This should be a utility in llstring.h: isascii()
	for (size_t i = 0; i < header_len; ++i)
	{
		if (header_line[i] < 0)
		{
			return header_len;
		}
	}

	std::string header(header_line, header_len);

	// Per HTTP spec the first header line must be the status line.
	if (header.substr(0,5) == "HTTP/")
	{
		std::string::iterator end = header.end();
		std::string::iterator pos1 = std::find(header.begin(), end, ' ');
		if (pos1 != end) ++pos1;
		std::string::iterator pos2 = std::find(pos1, end, ' ');
		if (pos2 != end) ++pos2;
		std::string::iterator pos3 = std::find(pos2, end, '\r');

		std::string version(header.begin(), pos1);
		std::string status(pos1, pos2);
		std::string reason(pos2, pos3);

		S32 status_code = atoi(status.c_str());
		if (status_code > 0)
		{
			complete->httpStatus(status_code, reason);
			return header_len;
		}
	}

	std::string::iterator sep = std::find(header.begin(),header.end(),':');

	if (sep != header.end())
	{
		std::string key(header.begin(), sep);
		std::string value(sep + 1, header.end());

		key = utf8str_tolower(utf8str_trim(key));
		value = utf8str_trim(value);

		complete->header(key, value);
	}
	else
	{
		LLStringUtil::trim(header);
		if (!header.empty())
		{
			LL_WARNS() << "Unable to parse header: " << header << LL_ENDL;
		}
	}

	return header_len;
}

static LLTrace::BlockTimerStatHandle FTM_PROCESS_URL_EXTRACTOR("URL Extractor");
/**
 * LLContextURLExtractor
 */
// virtual
LLIOPipe::EStatus LLContextURLExtractor::process_impl(
	const LLChannelDescriptors& channels,
	buffer_ptr_t& buffer,
	bool& eos,
	LLSD& context,
	LLPumpIO* pump)
{
	LL_RECORD_BLOCK_TIME(FTM_PROCESS_URL_EXTRACTOR);
	PUMP_DEBUG;
	// The destination host is in the context.
	if(context.isUndefined() || !mRequest)
	{
		return STATUS_PRECONDITION_NOT_MET;
	}

	// copy in to out, since this just extract the URL and does not
	// actually change the data.
	LLChangeChannel change(channels.in(), channels.out());
	std::for_each(buffer->beginSegment(), buffer->endSegment(), change);

	// find the context url
	if(context.has(CONTEXT_DEST_URI_SD_LABEL))
	{
		mRequest->setURL(context[CONTEXT_DEST_URI_SD_LABEL].asString());
		return STATUS_DONE;
	}
	return STATUS_ERROR;
}


/**
 * LLURLRequestComplete
 */
LLURLRequestComplete::LLURLRequestComplete() :
	mRequestStatus(LLIOPipe::STATUS_ERROR)
{
}

// virtual
LLURLRequestComplete::~LLURLRequestComplete()
{
}

//virtual 
void LLURLRequestComplete::header(const std::string& header, const std::string& value)
{
}

//virtual 
void LLURLRequestComplete::complete(const LLChannelDescriptors& channels,
		const buffer_ptr_t& buffer)
{
	if(STATUS_OK == mRequestStatus)
	{
		response(channels, buffer);
	}
	else
	{
		noResponse();
	}
}

//virtual 
void LLURLRequestComplete::response(const LLChannelDescriptors& channels,
		const buffer_ptr_t& buffer)
{
	LL_WARNS() << "LLURLRequestComplete::response default implementation called"
		<< LL_ENDL;
}

//virtual 
void LLURLRequestComplete::noResponse()
{
	LL_WARNS() << "LLURLRequestComplete::noResponse default implementation called"
		<< LL_ENDL;
}

void LLURLRequestComplete::responseStatus(LLIOPipe::EStatus status)
{
	mRequestStatus = status;
}

static LLTrace::BlockTimerStatHandle FTM_PROCESS_URL_COMPLETE("URL Complete");
// virtual
LLIOPipe::EStatus LLURLRequestComplete::process_impl(
	const LLChannelDescriptors& channels,
	buffer_ptr_t& buffer,
	bool& eos,
	LLSD& context,
	LLPumpIO* pump)
{
	LL_RECORD_BLOCK_TIME(FTM_PROCESS_URL_COMPLETE);
	PUMP_DEBUG;
	complete(channels, buffer);
	return STATUS_OK;
}
