/**
 * @file lleasymessagereader.cpp
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
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
 * $/LicenseInfo$
 */

#ifndef LL_LLMESSAGELOG_H
#define LL_LLMESSAGELOG_H

#include "linden_common.h"
#include "llhttpconstants.h"
#include "llhost.h"
#include "lliopipe.h"
#include <queue>
#include <string.h>

class LLMessageSystem;

class LLMessageLogEntry;
typedef std::shared_ptr<LLMessageLogEntry> LogPayload;

class LLMessageLogEntry
{
public:
	enum EType
	{
		NONE,
		TEMPLATE,
		HTTP_REQUEST,
		HTTP_RESPONSE,
		LOG_TYPE_NUM
	};
	LLMessageLogEntry();
	LLMessageLogEntry(EType type, LLHost from_host, LLHost to_host, U8* data, S32 data_size);
	LLMessageLogEntry(EType type, const std::string& url, const LLChannelDescriptors& channels,
	                  const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id,
	                  EHTTPMethod method = HTTP_INVALID, U32 status_code = 0);
	LLMessageLogEntry(const LLMessageLogEntry& entry);
	~LLMessageLogEntry();
	EType mType;
	LLHost mFromHost;
	LLHost mToHost;
	S32 mDataSize;
	U8* mData;

	//http-related things
	std::string mURL;
	U32 mStatusCode;
	EHTTPMethod mMethod;
	LLSD mHeaders;
	U64 mRequestID;
};

typedef void(*LogCallback) (LogPayload&);

class LLMessageLog
{
public:
	static void setCallback(LogCallback callback);
	static void log(LLHost from_host, LLHost to_host, U8* data, S32 data_size);
	static void logHTTPRequest(const std::string& url, EHTTPMethod method, const LLChannelDescriptors& channels,
	                           const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id);
	static void logHTTPResponse(U32 status_code, const LLChannelDescriptors& channels,
	                            const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id);

	static bool haveLogger(){return sCallback != nullptr;}

private:
	static LogCallback sCallback;
};
#endif
