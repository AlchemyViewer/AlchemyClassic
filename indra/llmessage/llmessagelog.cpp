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

#include "llmessagelog.h"
#include <boost/circular_buffer.hpp>

static boost::circular_buffer<LogPayload> sRingBuffer = boost::circular_buffer<LogPayload>(2048);

LLMessageLogEntry::LLMessageLogEntry(EType type, LLHost from_host, LLHost to_host, U8* data, S32 data_size)
:	mType(type),
	mFromHost(from_host),
	mToHost(to_host),
	mDataSize(data_size),
	mData(NULL)
{
	if(data)
	{
		mData = new U8[data_size];
		memcpy(mData, data, data_size);
	}
}

LLMessageLogEntry::LLMessageLogEntry()
:	mType(NONE),
	mFromHost(LLHost()),
	mToHost(LLHost()),
	mDataSize(0),
	mData(NULL)
{
}

LLMessageLogEntry::LLMessageLogEntry(EType type, const std::string& url, const LLChannelDescriptors& channels,
                                     const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id,
                                     EHTTPMethod method, U32 status_code)
    : mType(type),
      mURL(url),
      mHeaders(headers),
      mRequestID(request_id),
      mMethod(method),
      mStatusCode(status_code),
      mDataSize(0),
      mData(NULL)
{
	if(buffer.get())
	{
		S32 channel = type == HTTP_REQUEST ? channels.out() : channels.in();
		mDataSize = buffer->countAfter(channel, NULL);
		if (mDataSize > 0)
		{
			mData = new U8[mDataSize + 1];
			buffer->readAfter(channel, NULL, mData, mDataSize);

			//make sure this is null terminated, since it's going to be used stringified
			mData[mDataSize] = '\0';
			++mDataSize;
		}
	}
}

LLMessageLogEntry::LLMessageLogEntry(const LLMessageLogEntry& entry)
    : mDataSize(entry.mDataSize),
      mType(entry.mType),
      mToHost(entry.mToHost),
      mFromHost(entry.mFromHost),
      mURL(entry.mURL),
      mHeaders(entry.mHeaders),
      mRequestID(entry.mRequestID),
      mMethod(entry.mMethod),
      mStatusCode(entry.mStatusCode)
{
	mData = new U8[mDataSize];
	memcpy(mData, entry.mData, mDataSize);
}

LLMessageLogEntry::~LLMessageLogEntry()
{
	delete[] mData;
	mData = NULL;
}

LogCallback LLMessageLog::sCallback = NULL;

void LLMessageLog::setCallback(LogCallback callback)
{	
	if (callback != NULL)
	{
		for (auto& m : sRingBuffer)
		{
			callback(m);
		}
	}
	sCallback = callback;
}

void LLMessageLog::log(LLHost from_host, LLHost to_host, U8* data, S32 data_size)
{
	if(!data_size || data == NULL) return;

	LogPayload payload = LogPayload(new LLMessageLogEntry(LLMessageLogEntry::TEMPLATE, from_host, to_host, data, data_size));

	if(sCallback) sCallback(payload);

	sRingBuffer.push_back(std::move(payload));
}

void LLMessageLog::logHTTPRequest(const std::string& url, EHTTPMethod method, const LLChannelDescriptors& channels,
                                  const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id)
{
	LogPayload payload = LogPayload(new LLMessageLogEntry(LLMessageLogEntry::HTTP_REQUEST, url, channels, buffer,
	                                         headers, request_id, method));
	if (sCallback) sCallback(payload);

	sRingBuffer.push_back(std::move(payload));
}

void LLMessageLog::logHTTPResponse(U32 status_code, const LLChannelDescriptors& channels,
                                   const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id)
{
	LogPayload payload = LogPayload(new LLMessageLogEntry(LLMessageLogEntry::HTTP_RESPONSE, "", channels, buffer,
	                                         headers, request_id, HTTP_INVALID, status_code));
	if (sCallback) sCallback(payload);

	sRingBuffer.push_back(std::move(payload));
}
