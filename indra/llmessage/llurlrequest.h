/** 
 * @file llurlrequest.h
 * @author Phoenix
 * @date 2005-04-21
 * @brief Declaration of url based requests on pipes.
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

#ifndef LL_LLURLREQUEST_H
#define LL_LLURLREQUEST_H

/** 
 * This file holds the declaration of useful classes for dealing with
 * url based client requests. 
 */

#include <string>
#include "lliopipe.h"
#include "llchainio.h"
#include "llerror.h"
#include <openssl/x509_vfy.h>
#include "llcurl.h"


/**
 * External constants
 */
extern const std::string CONTEXT_DEST_URI_SD_LABEL;
extern const std::string CONTEXT_TRANSFERED_BYTES;

class LLURLRequestDetail;

class LLURLRequestComplete;

/** 
 * @class LLURLRequest
 * @brief Class to handle url based requests.
 * @see LLIOPipe
 *
 * Currently, this class is implemented on top of curl. From the
 * vantage of a programmer using this class, you do not care so much,
 * but it's useful to know since in order to accomplish 'non-blocking'
 * behavior, we have to use a more expensive curl interface which can
 * still block if the server enters a half-accepted state. It would be
 * worth the time and effort to eventually port this to a raw client
 * socket.
 */
class LLURLRequest : public LLIOPipe
{
	LOG_CLASS(LLURLRequest);
public:
	typedef int (* SSLCertVerifyCallback)(X509_STORE_CTX *ctx, void *param);

	/** 
	 * @brief Constructor.
	 *
	 * @param action One of the EHTTPMethod enumerations.
	 */
	LLURLRequest(EHTTPMethod action, bool follow_redirects = true);

	/** 
	 * @brief Constructor.
	 *
	 * @param action One of the EHTTPMethod enumerations.
	 * @param url The url of the request. It should already be encoded.
	 */
	LLURLRequest(EHTTPMethod action, const std::string& url, bool follow_redirects = true);

	/** 
	 * @brief Destructor.
	 */
	virtual ~LLURLRequest();

	/* @name Instance methods
	 */
	//@{
	/** 
	 * @brief Set the url for the request
	 *
	 * This method assumes the url is encoded appropriately for the
	 * request. 
	 * The url must be set somehow before the first call to process(),
	 * or the url will not be set correctly.
	 * 
	 */
	void setURL(const std::string& url);
	const std::string& getURL() const;
	/** 
	 * @brief Add a header to the http post.
	 *
	 * This provides a raw interface if you know what kind of request you
	 * will be making during construction of this instance. All
	 * required headers will be automatically constructed, so this is
	 * usually useful for encoding parameters.
	 */
	void addHeader(const std::string& header, const std::string& value = "");
	void addHeaderRaw(const char* header);

	/**
	 * @brief Check remote server certificate signed by a known root CA.
	 *
	 * Set whether request will check that remote server
	 * certificates are signed by a known root CA when using HTTPS.
	 */
	void setSSLVerifyCallback(SSLCertVerifyCallback callback, void * param);

	
	/**
	 * @brief Return at most size bytes of body.
	 *
	 * If the body had more bytes than this limit, they will not be
	 * returned and the connection closed.  In this case, STATUS_STOP
	 * will be passed to responseStatus();
	 */
	void setBodyLimit(U32 size);

	/** 
	 * @brief Set a completion callback for this URLRequest.
	 *
	 * The callback is added to this URLRequet's pump when either the
	 * entire buffer is known or an error like timeout or connection
	 * refused has happened. In the case of a complete transfer, this
	 * object builds a response chain such that the callback and the
	 * next process consumer get to read the output.
	 *
	 * This setup is a little fragile since the url request consumer
	 * might not just read the data - it may do a channel change,
	 * which invalidates the input to the callback, but it works well
	 * in practice.
	 */
	void setCallback(LLURLRequestComplete* callback);
	//@}

	/* @name LLIOPipe virtual implementations
	 */

    /**
     * @ brief Turn off (or on) the CURLOPT_PROXY header.
     */
    void useProxy(bool use_proxy);

    /**
     * @ brief Set the CURLOPT_PROXY header to the given value.
     */
	void useProxy(const std::string& proxy);

	/**
	 * @brief Turn on cookie handling for this request with CURLOPT_COOKIEFILE.
	 */
	void allowCookies();

	/*virtual*/ bool isValid() ;

public:
	/** 
	 * @brief Give this pipe a chance to handle a generated error
	 */
	virtual EStatus handleError(EStatus status, LLPumpIO* pump);

	
protected:
	/** 
	 * @brief Process the data in buffer
	 */
	virtual EStatus process_impl(
		const LLChannelDescriptors& channels,
		buffer_ptr_t& buffer,
		bool& eos,
		LLSD& context,
		LLPumpIO* pump);
	//@}

protected:
	enum EState
	{
		STATE_INITIALIZED,
		STATE_WAITING_FOR_RESPONSE,
		STATE_PROCESSING_RESPONSE,
		STATE_HAVE_RESPONSE,
	};
	EState mState;
	EHTTPMethod mAction;
	bool mFollowRedirects;
	LLURLRequestDetail* mDetail;
	LLIOPipe::ptr_t mCompletionCallback;
	S32 mRequestTransferedBytes;
	S32 mResponseTransferedBytes;

	static CURLcode _sslCtxCallback(CURL * curl, void *sslctx, void *param);
	
private:
	/** 
	 * @brief Initialize the object. Called during construction.
	 */
	void initialize();

	/** 
	 * @brief Handle action specific url request configuration.
	 *
	 * @return Returns true if this is configured.
	 */
	bool configure();

	/** 
	 * @brief Download callback method.
	 */
 	static size_t downCallback(
		char* data,
		size_t size,
		size_t nmemb,
		void* user);

	/** 
	 * @brief Upload callback method.
	 */
 	static size_t upCallback(
		char* data,
		size_t size,
		size_t nmemb,
		void* user);

	/** 
	 * @brief Declaration of unimplemented method to prevent copy
	 * construction.
	 */
	LLURLRequest(const LLURLRequest&);
};


/** 
 * @class LLContextURLExtractor
 * @brief This class unpacks the url out of a agent usher service so
 * it can be packed into a LLURLRequest object.
 * @see LLIOPipe
 *
 * This class assumes that the context is a map that contains an entry
 * named CONTEXT_DEST_URI_SD_LABEL.
 */
class LLContextURLExtractor : public LLIOPipe
{
public:
	LLContextURLExtractor(LLURLRequest* req) : mRequest(req) {}
	~LLContextURLExtractor() {}

protected:
	/* @name LLIOPipe virtual implementations
	 */
	//@{
	/** 
	 * @brief Process the data in buffer
	 */
	virtual EStatus process_impl(
		const LLChannelDescriptors& channels,
		buffer_ptr_t& buffer,
		bool& eos,
		LLSD& context,
		LLPumpIO* pump);
	//@}

protected:
	LLURLRequest* mRequest;
};


/** 
 * @class LLURLRequestComplete
 * @brief Class which can optionally be used with an LLURLRequest to
 * get notification when the url request is complete.
 */
class LLURLRequestComplete : public LLIOPipe
{
public:
	
	// Called once for each header received, except status lines
	virtual void header(const std::string& header, const std::string& value);

	// May be called more than once, particularly for redirects and proxy madness.
	// Ex. a 200 for a connection to https through a proxy, followed by the "real" status
	//     a 3xx for a redirect followed by a "real" status, or more redirects.
	virtual void httpStatus(S32 status, const std::string& reason) { }

	virtual void complete(
		const LLChannelDescriptors& channels,
		const buffer_ptr_t& buffer);

	/** 
	 * @brief This method is called when we got a valid response.
	 *
	 * It is up to class implementers to do something useful here.
	 */
	virtual void response(
		const LLChannelDescriptors& channels,
		const buffer_ptr_t& buffer);

	/** 
	 * @brief This method is called if there was no response.
	 *
	 * It is up to class implementers to do something useful here.
	 */
	virtual void noResponse();

	/** 
	 * @brief This method will be called by the LLURLRequest object.
	 *
	 * If this is set to STATUS_OK or STATUS_STOP, then the transfer
	 * is asssumed to have worked. This will lead to calling response()
	 * on the next call to process(). Otherwise, this object will call
	 * noResponse() on the next call to process.
	 * @param status The status of the URLRequest.
	 */
	void responseStatus(EStatus status);

	// constructor & destructor.
	LLURLRequestComplete();
	virtual ~LLURLRequestComplete();

protected:
	/* @name LLIOPipe virtual implementations
	 */
	//@{
	/** 
	 * @brief Process the data in buffer
	 */
	virtual EStatus process_impl(
		const LLChannelDescriptors& channels,
		buffer_ptr_t& buffer,
		bool& eos,
		LLSD& context,
		LLPumpIO* pump);
	//@}

	// value to note if we actually got the response. This value
	// depends on correct usage from the LLURLRequest instance.
	EStatus mRequestStatus;
};

#endif // LL_LLURLREQUEST_H
