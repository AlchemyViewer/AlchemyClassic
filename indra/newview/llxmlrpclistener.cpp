/**
 * @file   llxmlrpclistener.cpp
 * @author Nat Goodspeed
 * @date   2009-03-18
 * @brief  Implementation for llxmlrpclistener.
 * 
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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


// Precompiled header
#include "llviewerprecompiledheaders.h"
// associated header
#include "llxmlrpclistener.h"
// external library headers
#include <utility>
#include <xmlrpc-epi/xmlrpc.h>
#include <curl/curl.h>

// other Linden headers
#include "llerror.h"
#include "stringize.h"
#include "llxmlrpctransaction.h"
#include "llsecapi.h"

template <typename STATUS>
class StatusMapperBase
{
    typedef std::map<STATUS, std::string> MapType;

public:
    StatusMapperBase(std::string desc):
        mDesc(std::move(desc))
    {}

    std::string lookup(STATUS status) const
    {
        typename MapType::const_iterator found = mMap.find(status);
        if (found != mMap.end())
        {
            return found->second;
        }
        return STRINGIZE("<unknown " << mDesc << " " << status << ">");
    }

protected:
    std::string mDesc;
    MapType mMap;
};

class StatusMapper: public StatusMapperBase<LLXMLRPCTransaction::EStatus>
{
public:
    StatusMapper(): StatusMapperBase<LLXMLRPCTransaction::EStatus>("Status")
    {
		mMap[LLXMLRPCTransaction::StatusNotStarted]  = "NotStarted";
		mMap[LLXMLRPCTransaction::StatusStarted]     = "Started";
		mMap[LLXMLRPCTransaction::StatusDownloading] = "Downloading";
		mMap[LLXMLRPCTransaction::StatusComplete]    = "Complete";
		mMap[LLXMLRPCTransaction::StatusCURLError]   = "CURLError";
		mMap[LLXMLRPCTransaction::StatusXMLRPCError] = "XMLRPCError";
		mMap[LLXMLRPCTransaction::StatusOtherError]  = "OtherError";
    }
};

static const StatusMapper sStatusMapper;

class CURLcodeMapper: public StatusMapperBase<CURLcode>
{
public:
    CURLcodeMapper(): StatusMapperBase<CURLcode>("CURLcode")
    {
        // from curl.h
// skip the "CURLE_" prefix for each of these strings
#define def(sym) (mMap[CURLE_##sym] = #sym)
        def(OK);
        def(UNSUPPORTED_PROTOCOL);    /* 1 */
        def(FAILED_INIT);             /* 2 */
        def(URL_MALFORMAT);           /* 3 */
        def(URL_MALFORMAT_USER);      /* 4 - NOT USED */
        def(COULDNT_RESOLVE_PROXY);   /* 5 */
        def(COULDNT_RESOLVE_HOST);    /* 6 */
        def(COULDNT_CONNECT);         /* 7 */
        def(FTP_WEIRD_SERVER_REPLY);  /* 8 */
        def(FTP_ACCESS_DENIED);       /* 9 a service was denied by the FTP server
                                          due to lack of access - when login fails
                                          this is not returned. */
        def(FTP_USER_PASSWORD_INCORRECT); /* 10 - NOT USED */
        def(FTP_WEIRD_PASS_REPLY);    /* 11 */
        def(FTP_WEIRD_USER_REPLY);    /* 12 */
        def(FTP_WEIRD_PASV_REPLY);    /* 13 */
        def(FTP_WEIRD_227_FORMAT);    /* 14 */
        def(FTP_CANT_GET_HOST);       /* 15 */
        def(FTP_CANT_RECONNECT);      /* 16 */
        def(FTP_COULDNT_SET_BINARY);  /* 17 */
        def(PARTIAL_FILE);            /* 18 */
        def(FTP_COULDNT_RETR_FILE);   /* 19 */
        def(FTP_WRITE_ERROR);         /* 20 */
        def(FTP_QUOTE_ERROR);         /* 21 */
        def(HTTP_RETURNED_ERROR);     /* 22 */
        def(WRITE_ERROR);             /* 23 */
        def(MALFORMAT_USER);          /* 24 - NOT USED */
        def(UPLOAD_FAILED);           /* 25 - failed upload "command" */
        def(READ_ERROR);              /* 26 - could open/read from file */
        def(OUT_OF_MEMORY);           /* 27 */
        /* Note: OUT_OF_MEMORY may sometimes indicate a conversion error
                 instead of a memory allocation error if CURL_DOES_CONVERSIONS
                 is defined
        */
        def(OPERATION_TIMEOUTED);     /* 28 - the timeout time was reached */
        def(FTP_COULDNT_SET_ASCII);   /* 29 - TYPE A failed */
        def(FTP_PORT_FAILED);         /* 30 - FTP PORT operation failed */
        def(FTP_COULDNT_USE_REST);    /* 31 - the REST command failed */
        def(FTP_COULDNT_GET_SIZE);    /* 32 - the SIZE command failed */
        def(HTTP_RANGE_ERROR);        /* 33 - RANGE "command" didn't work */
        def(HTTP_POST_ERROR);         /* 34 */
        def(SSL_CONNECT_ERROR);       /* 35 - wrong when connecting with SSL */
        def(BAD_DOWNLOAD_RESUME);     /* 36 - couldn't resume download */
        def(FILE_COULDNT_READ_FILE);  /* 37 */
        def(LDAP_CANNOT_BIND);        /* 38 */
        def(LDAP_SEARCH_FAILED);      /* 39 */
        def(LIBRARY_NOT_FOUND);       /* 40 */
        def(FUNCTION_NOT_FOUND);      /* 41 */
        def(ABORTED_BY_CALLBACK);     /* 42 */
        def(BAD_FUNCTION_ARGUMENT);   /* 43 */
        def(BAD_CALLING_ORDER);       /* 44 - NOT USED */
        def(INTERFACE_FAILED);        /* 45 - CURLOPT_INTERFACE failed */
        def(BAD_PASSWORD_ENTERED);    /* 46 - NOT USED */
        def(TOO_MANY_REDIRECTS );     /* 47 - catch endless re-direct loops */
        def(UNKNOWN_TELNET_OPTION);   /* 48 - User specified an unknown option */
        def(TELNET_OPTION_SYNTAX );   /* 49 - Malformed telnet option */
        def(OBSOLETE);                /* 50 - NOT USED */
        def(SSL_PEER_CERTIFICATE);    /* 51 - peer's certificate wasn't ok */
        def(GOT_NOTHING);             /* 52 - when this is a specific error */
        def(SSL_ENGINE_NOTFOUND);     /* 53 - SSL crypto engine not found */
        def(SSL_ENGINE_SETFAILED);    /* 54 - can not set SSL crypto engine as
                                          default */
        def(SEND_ERROR);              /* 55 - failed sending network data */
        def(RECV_ERROR);              /* 56 - failure in receiving network data */
        def(SHARE_IN_USE);            /* 57 - share is in use */
        def(SSL_CERTPROBLEM);         /* 58 - problem with the local certificate */
        def(SSL_CIPHER);              /* 59 - couldn't use specified cipher */
        def(SSL_CACERT);              /* 60 - problem with the CA cert (path?) */
        def(BAD_CONTENT_ENCODING);    /* 61 - Unrecognized transfer encoding */
        def(LDAP_INVALID_URL);        /* 62 - Invalid LDAP URL */
        def(FILESIZE_EXCEEDED);       /* 63 - Maximum file size exceeded */
        def(FTP_SSL_FAILED);          /* 64 - Requested FTP SSL level failed */
        def(SEND_FAIL_REWIND);        /* 65 - Sending the data requires a rewind
                                          that failed */
        def(SSL_ENGINE_INITFAILED);   /* 66 - failed to initialise ENGINE */
        def(LOGIN_DENIED);            /* 67 - user); password or similar was not
                                          accepted and we failed to login */
        def(TFTP_NOTFOUND);           /* 68 - file not found on server */
        def(TFTP_PERM);               /* 69 - permission problem on server */
        def(TFTP_DISKFULL);           /* 70 - out of disk space on server */
        def(TFTP_ILLEGAL);            /* 71 - Illegal TFTP operation */
        def(TFTP_UNKNOWNID);          /* 72 - Unknown transfer ID */
        def(TFTP_EXISTS);             /* 73 - File already exists */
        def(TFTP_NOSUCHUSER);         /* 74 - No such user */
        def(CONV_FAILED);             /* 75 - conversion failed */
        def(CONV_REQD);               /* 76 - caller must register conversion
                                          callbacks using curl_easy_setopt options
                                          CURLOPT_CONV_FROM_NETWORK_FUNCTION);
                                          CURLOPT_CONV_TO_NETWORK_FUNCTION); and
                                          CURLOPT_CONV_FROM_UTF8_FUNCTION */
        def(SSL_CACERT_BADFILE);      /* 77 - could not load CACERT file); missing
                                          or wrong format */
        def(REMOTE_FILE_NOT_FOUND);   /* 78 - remote file not found */
        def(SSH);                     /* 79 - error from the SSH layer); somewhat
                                          generic so the error message will be of
                                          interest when this has happened */

        def(SSL_SHUTDOWN_FAILED);     /* 80 - Failed to shut down the SSL
                                          connection */
#undef  def
    }
};

static const CURLcodeMapper sCURLcodeMapper;

LLXMLRPCListener::LLXMLRPCListener(const std::string& pumpname):
    mBoundListener(LLEventPumps::instance().
                   obtain(pumpname).
                   listen("LLXMLRPCListener", boost::bind(&LLXMLRPCListener::process, this, _1)))
{
}

/**
 * Capture an outstanding LLXMLRPCTransaction and poll it periodically until
 * done.
 *
 * The sequence is:
 * # Instantiate Poller, which instantiates, populates and initiates an
 *   LLXMLRPCTransaction. Poller self-registers on the LLEventPump named
 *   "mainloop".
 * # "mainloop" is conventionally pumped once per frame. On each such call,
 *   Poller checks its LLXMLRPCTransaction for completion.
 * # When the LLXMLRPCTransaction completes, Poller collects results (if any)
 *   and sends notification.
 * # The tricky part: Poller frees itself (and thus its LLXMLRPCTransaction)
 *   when done. The only external reference to it is the connection to the
 *   "mainloop" LLEventPump.
 */
class Poller
{
public:
    /// Validate the passed request for required fields, then use it to
    /// populate an XMLRPC_REQUEST and an associated LLXMLRPCTransaction. Send
    /// the request.
    Poller(const LLSD& command):
        mReqID(command),
        mUri(command["uri"]),
        mMethod(command["method"]),
        mReplyPump(command["reply"])
    {
        // LL_ERRS if any of these are missing
        const char* required[] = { "uri", "method", "reply" };
        // optional: "options" (array of string)
        // Validate the request
        std::set<std::string> missing;
        for (const char** ri = boost::begin(required); ri != boost::end(required); ++ri)
        {
            // If the command does not contain this required entry, add it to 'missing'.
            if (! command.has(*ri))
            {
                missing.insert(*ri);
            }
        }
        if (! missing.empty())
        {
            LL_ERRS("LLXMLRPCListener") << mMethod << " request missing params: ";
            const char* separator = "";
            for (const auto& mi : missing)
            {
                LL_CONT << separator << mi;
                separator = ", ";
            }
            LL_CONT << LL_ENDL;
        }

        // Build the XMLRPC request.
        XMLRPC_REQUEST request = XMLRPC_RequestNew();
        XMLRPC_RequestSetMethodName(request, mMethod.c_str());
        XMLRPC_RequestSetRequestType(request, xmlrpc_request_call);
        XMLRPC_VALUE xparams = XMLRPC_CreateVector(nullptr, xmlrpc_vector_struct);
        LLSD params(command["params"]);
        if (params.isMap())
        {
            for (LLSD::map_const_iterator pi(params.beginMap()), pend(params.endMap());
                 pi != pend; ++pi)
            {
                std::string name(pi->first);
                LLSD param(pi->second);
                if (param.isString())
                {
                    XMLRPC_VectorAppendString(xparams, name.c_str(), param.asString().c_str(), 0);
                }
                else if (param.isInteger() || param.isBoolean())
                {
                    XMLRPC_VectorAppendInt(xparams, name.c_str(), param.asInteger());
                }
                else if (param.isReal())
                {
                    XMLRPC_VectorAppendDouble(xparams, name.c_str(), param.asReal());
                }
                else
                {
                    LL_ERRS("LLXMLRPCListener") << mMethod << " request param "
                                                << name << " has unknown type: " << param << LL_ENDL;
                }
            }
        }
        LLSD options(command["options"]);
        if (options.isArray())
        {
            XMLRPC_VALUE xoptions = XMLRPC_CreateVector("options", xmlrpc_vector_array);
            for (LLSD::array_const_iterator oi(options.beginArray()), oend(options.endArray());
                 oi != oend; ++oi)
            {
                XMLRPC_VectorAppendString(xoptions, NULL, oi->asString().c_str(), 0);
            }
            XMLRPC_AddValueToVector(xparams, xoptions);
        }
        XMLRPC_RequestSetData(request, xparams);

        mTransaction.reset(new LLXMLRPCTransaction(mUri, request, true, command.has("http_params")? LLSD(command["http_params"]) : LLSD()));
		mPreviousStatus = mTransaction->status(nullptr);

        // Free the XMLRPC_REQUEST object and the attached data values.
        XMLRPC_RequestFree(request, 1);

        // Now ensure that we get regular callbacks to poll for completion.
        mBoundListener =
            LLEventPumps::instance().
            obtain("mainloop").
            listen(LLEventPump::ANONYMOUS, boost::bind(&Poller::poll, this, _1));

        LL_INFOS("LLXMLRPCListener") << mMethod << " request sent to " << mUri << LL_ENDL;
    }

    /// called by "mainloop" LLEventPump
    bool poll(const LLSD&)
    {
        bool done = mTransaction->process();

        CURLcode curlcode;
        LLXMLRPCTransaction::EStatus status;
        {
            // LLXMLRPCTransaction::status() is defined to accept int* rather
            // than CURLcode*. I don't feel the urge to fix the signature, but
            // we want a CURLcode rather than an int. So fetch it as a local
            // int, but then assign to a CURLcode for the remainder of this
            // method.
            int curlint;
            status = mTransaction->status(&curlint);
            curlcode = CURLcode(curlint);
        }

        LLSD data(mReqID.makeResponse());
        data["status"] = sStatusMapper.lookup(status);
        data["errorcode"] = sCURLcodeMapper.lookup(curlcode);
        data["error"] = "";
        data["transfer_rate"] = 0.0;
        LLEventPump& replyPump(LLEventPumps::instance().obtain(mReplyPump));
		if (! done)
        {
            // Not done yet, carry on.
			if (status == LLXMLRPCTransaction::StatusDownloading
				&& status != mPreviousStatus)
			{
				// If a response has been received, send the 
				// 'downloading' status if it hasn't been sent.
				replyPump.post(data);
			}

			mPreviousStatus = status;
            return false;
        }

        // Here the transaction is complete. Check status.
        data["error"] = mTransaction->statusMessage();
		data["transfer_rate"] = mTransaction->transferRate();
        LL_INFOS("LLXMLRPCListener") << mMethod << " result from " << mUri << ": status "
                                     << data["status"].asString() << ", errorcode "
                                     << data["errorcode"].asString()
                                     << " (" << data["error"].asString() << ")"
                                     << LL_ENDL;
		
        if (curlcode == CURLE_PEER_FAILED_VERIFICATION) {
            data["certificate"] = mTransaction->getErrorCertData();
        }

        // values of 'curlcode':
        // CURLE_COULDNT_RESOLVE_HOST,
        // CURLE_SSL_PEER_CERTIFICATE,
        // CURLE_SSL_CACERT,
        // CURLE_SSL_CONNECT_ERROR.
        // Given 'message', need we care?
        if (status == LLXMLRPCTransaction::StatusComplete)
        {
            // Success! Parse data.
            std::string status_string(data["status"]);
            data["responses"] = parseResponse(status_string);
            data["status"] = status_string;
        }

        // whether successful or not, send reply on requested LLEventPump
        replyPump.post(data);

        // Because mTransaction is a std::unique_ptr, deleting this object
        // frees our LLXMLRPCTransaction object.
        // Because mBoundListener is an LLTempBoundListener, deleting this
        // object disconnects it from "mainloop".
        // *** MUST BE LAST ***
        delete this;
        return false;
    }

private:
    /// Derived from LLUserAuth::parseResponse() and parseOptionInto()
    LLSD parseResponse(std::string& status_string)
    {
        // Extract every member into data["responses"] (a map of string
        // values).
        XMLRPC_REQUEST response = mTransaction->response();
        if (! response)
        {
            LL_DEBUGS("LLXMLRPCListener") << "No response" << LL_ENDL;
            return LLSD();
        }

        XMLRPC_VALUE param = XMLRPC_RequestGetData(response);
        if (! param)
        {
            LL_DEBUGS("LLXMLRPCListener") << "Response contains no data" << LL_ENDL;
            return LLSD();
        }

        // Now, parse everything
        return parseValues(status_string, "", param);
    }

    /**
     * Parse key/value pairs from a given XMLRPC_VALUE into an LLSD map.
     * @param key_pfx Used to describe a given key in log messages. At top
     * level, pass "". When parsing an options array, pass the top-level key
     * name of the array plus the index of the array entry; to this we'll
     * append the subkey of interest.
     * @param param XMLRPC_VALUE iterator. At top level, pass
     * XMLRPC_RequestGetData(XMLRPC_REQUEST).
     */
    LLSD parseValues(std::string& status_string, const std::string& key_pfx, XMLRPC_VALUE param)
    {
        LLSD responses;
        for (XMLRPC_VALUE current = XMLRPC_VectorRewind(param); current;
             current = XMLRPC_VectorNext(param))
        {
            std::string key(XMLRPC_GetValueID(current));
            LL_DEBUGS("LLXMLRPCListener") << "key: " << key_pfx << key << LL_ENDL;
            XMLRPC_VALUE_TYPE_EASY type = XMLRPC_GetValueTypeEasy(current);
            if (xmlrpc_type_string == type)
            {
                LLSD::String val(XMLRPC_GetValueString(current));
                LL_DEBUGS("LLXMLRPCListener") << "val: " << val << LL_ENDL;
                responses.insert(key, val);
            }
            else if (xmlrpc_type_int == type)
            {
                LLSD::Integer val(XMLRPC_GetValueInt(current));
                LL_DEBUGS("LLXMLRPCListener") << "val: " << val << LL_ENDL;
                responses.insert(key, val);
            }
            else if (xmlrpc_type_double == type)
            {
                LLSD::Real val(XMLRPC_GetValueDouble(current));
                LL_DEBUGS("LLXMLRPCListener") << "val: " << val << LL_ENDL;
                responses.insert(key, val);
            }
            else if (xmlrpc_type_array == type)
            {
                // We expect this to be an array of submaps. Walk the array,
                // recursively parsing each submap and collecting them.
                LLSD array;
                int i = 0;          // for descriptive purposes
                for (XMLRPC_VALUE row = XMLRPC_VectorRewind(current); row;
                     row = XMLRPC_VectorNext(current), ++i)
                {
                    // Recursive call. For the lower-level key_pfx, if 'key'
                    // is "foo", pass "foo[0]:", then "foo[1]:", etc. In the
                    // nested call, a subkey "bar" will then be logged as
                    // "foo[0]:bar", and so forth.
                    // Parse the scalar subkey/value pairs from this array
                    // entry into a temp submap. Collect such submaps in 'array'.
                    array.append(parseValues(status_string,
                                             STRINGIZE(key_pfx << key << '[' << i << "]:"),
                                             row));
                }
                // Having collected an 'array' of 'submap's, insert that whole
                // 'array' as the value of this 'key'.
                responses.insert(key, array);
            }
            else if (xmlrpc_type_struct == type)
            {
                LLSD submap = parseValues(status_string,
                                          STRINGIZE(key_pfx << key << ':'),
                                          current);
                responses.insert(key, submap);
            }
            else
            {
                // whoops - unrecognized type
                LL_WARNS("LLXMLRPCListener") << "Unhandled xmlrpc type " << type << " for key "
                                             << key_pfx << key << LL_ENDL;
                responses.insert(key, STRINGIZE("<bad XMLRPC type " << type << '>'));
                status_string = "BadType";
            }
        }
        return responses;
    }

    const LLReqID mReqID;
    const std::string mUri;
    const std::string mMethod;
    const std::string mReplyPump;
    LLTempBoundListener mBoundListener;
	std::unique_ptr<LLXMLRPCTransaction> mTransaction;
	LLXMLRPCTransaction::EStatus mPreviousStatus; // To detect state changes.
};

bool LLXMLRPCListener::process(const LLSD& command)
{
    // Allocate a new heap Poller, but do not save a pointer to it. Poller
    // will check its own status and free itself on completion of the request.
    (new Poller(command));
    // conventional event listener return
    return false;
}
