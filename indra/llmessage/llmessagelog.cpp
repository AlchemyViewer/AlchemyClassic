// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
#include "llbuffer.h"
#include <boost/circular_buffer.hpp>

static boost::circular_buffer<LogPayload> sRingBuffer = boost::circular_buffer<LogPayload>(2048);

LLMessageLogEntry::LLMessageLogEntry()
:	mType(NONE)
,	mFromHost(LLHost())
,	mToHost(LLHost())
,	mDataSize(0)
,	mData(nullptr)
,	mStatusCode(0)
,	mMethod()
,	mRequestID(0)
{
}

LLMessageLogEntry::LLMessageLogEntry(EType type, LLHost from_host, LLHost to_host, U8* data, S32 data_size)
:	mType(type)
,	mFromHost(from_host)
,	mToHost(to_host)
,	mDataSize(data_size)
,	mData(nullptr)
,	mStatusCode(0)
,	mMethod()
,	mRequestID(0)
{
	if(data)
	{
		mData = new U8[data_size];
		memcpy(mData, data, data_size);
	}
}

LLMessageLogEntry::LLMessageLogEntry(EType type, const std::string& url, const LLChannelDescriptors& channels,
                                     const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id,
                                     EHTTPMethod method, U32 status_code)
    : mType(type),
      mDataSize(0),
      mData(nullptr),
      mURL(url),
      mStatusCode(status_code),
      mMethod(method),
      mHeaders(headers),
      mRequestID(request_id)
{
	if(buffer.get())
	{
		S32 channel = type == HTTP_REQUEST ? channels.out() : channels.in();
		mDataSize = buffer->countAfter(channel, nullptr);
		if (mDataSize > 0)
		{
			mData = new U8[mDataSize + 1];
			buffer->readAfter(channel, nullptr, mData, mDataSize);

			//make sure this is null terminated, since it's going to be used stringified
			mData[mDataSize] = '\0';
			++mDataSize;
		}
	}
}

LLMessageLogEntry::LLMessageLogEntry(const LLMessageLogEntry& entry)
    : mType(entry.mType),
      mFromHost(entry.mFromHost),
      mToHost(entry.mToHost),
      mDataSize(entry.mDataSize),
      mURL(entry.mURL),
      mStatusCode(entry.mStatusCode),
      mMethod(entry.mMethod),
      mHeaders(entry.mHeaders),
      mRequestID(entry.mRequestID)
{
	mData = new U8[mDataSize];
	memcpy(mData, entry.mData, mDataSize);
}

LLMessageLogEntry::~LLMessageLogEntry()
{
	delete[] mData;
	mData = nullptr;
}

LogCallback LLMessageLog::sCallback = nullptr;

void LLMessageLog::setCallback(LogCallback callback)
{	
	if (callback != nullptr)
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
	if(!data_size || data == nullptr) return;

	LogPayload payload = std::make_shared<LLMessageLogEntry>(LLMessageLogEntry::TEMPLATE, from_host, to_host, data, data_size);

	if(sCallback) sCallback(payload);

	sRingBuffer.push_back(std::move(payload));
}

void LLMessageLog::logHTTPRequest(const std::string& url, EHTTPMethod method, const LLChannelDescriptors& channels,
                                  const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id)
{
	LogPayload payload = std::make_shared<LLMessageLogEntry>(LLMessageLogEntry::HTTP_REQUEST, url, channels, buffer,
	                                                         headers, request_id, method);
	if (sCallback) sCallback(payload);

	sRingBuffer.push_back(std::move(payload));
}

void LLMessageLog::logHTTPResponse(U32 status_code, const LLChannelDescriptors& channels,
                                   const LLIOPipe::buffer_ptr_t& buffer, const LLSD& headers, U64 request_id)
{
	LogPayload payload = std::make_shared<LLMessageLogEntry>(LLMessageLogEntry::HTTP_RESPONSE, "", channels, buffer,
	                                                         headers, request_id, HTTP_INVALID, status_code);
	if (sCallback) sCallback(payload);

	sRingBuffer.push_back(std::move(payload));
}
