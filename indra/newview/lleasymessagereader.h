/**
 * @file lleasymessagereader.h
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
 **/

#ifndef EASY_MESSAGE_READER_H
#define EASY_MESSAGE_READER_H

#include "llmessagelog.h"
#include "linden_common.h"
#include "message.h"
#include "lltemplatemessagereader.h"
#include "llmessagetemplate.h"

class LLViewerRegion;

class LLEasyMessageReader
{
public:
	LLEasyMessageReader();
	~LLEasyMessageReader();

	LLMessageTemplate* decodeTemplateMessage(U8* data, S32 data_len, LLHost from_host);
	LLMessageTemplate* decodeTemplateMessage(U8* data, S32 data_len, LLHost from_host, U32& sequence_id);

	S32 getNumberOfBlocks(const char *blockname);

	std::string var2Str(const char* block_name, S32 block_num, LLMessageVariable* variable, BOOL &returned_hex, BOOL summary_mode=FALSE);

private:
	LLTemplateMessageReader mTemplateMessageReader;
	U8	mRecvBuffer[MAX_BUFFER_SIZE];
};

class LLEasyMessageLogEntry
{
public:
	LLEasyMessageLogEntry(LogPayload entry, LLEasyMessageReader* message_reader = nullptr);
	LLEasyMessageLogEntry(LLEasyMessageReader* message_reader = nullptr);
	~LLEasyMessageLogEntry();

	LogPayload operator()() { return mEntry; };

	std::string getFull(BOOL beautify = FALSE, BOOL show_header = FALSE);
	std::string getName();
	std::string getResponseFull(BOOL beautify = FALSE, BOOL show_header = FALSE);
	BOOL isOutgoing();

	void setResponseMessage(LogPayload entry);

	LLUUID mID;
	U32 mSequenceID;
	//depending on how the server is configured, two cap handlers
	//may have the exact same URI, meaning there may be multiple possible
	//cap names for each message. Ditto for possible region hosts.
	std::set<std::string> mNames;
	std::set<LLHost> mRegionHosts;
	std::string mSummary;
	U32 mFlags;

private:
	LogPayload mEntry;

	LLEasyMessageLogEntry* mResponseMsg;
	LLEasyMessageReader* mEasyMessageReader;
};

#endif
