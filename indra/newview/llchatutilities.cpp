// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file llchatutilities.cpp
 * @brief Helper functions for chat input
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2015, Alchemy Developer Group
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llchatutilities.h"

#include "message.h"

#include "llanimationstates.h"
#include "llcommandhandler.h"
#include "llmediactrl.h"
#include "llviewercontrol.h"
#include "llviewerstats.h"


// legacy callback glue
extern void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel);

const std::map<std::string, EChatType> sChatTypeTriggers = {
	{LLStringExplicit("/whisper"), CHAT_TYPE_WHISPER},
	{LLStringExplicit("/shout"), CHAT_TYPE_SHOUT}
};

static S32 sLastSpecialChatChannel = 0;

//
// Functions
//

void LLChatUtilities::sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate)
{
	sendChatFromViewer(utf8str_to_wstring(utf8text), type, animate);
}

void LLChatUtilities::sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate)
{
	// Look for "/20 foo" channel chats.
	S32 channel = gSavedSettings.getS32("AlchemyNearbyChatChannel");
	LLWString out_text = stripChannelNumber(wtext, &channel);
	std::string utf8_out_text = wstring_to_utf8str(out_text);
	std::string utf8_text = wstring_to_utf8str(wtext);
	
	utf8_text = utf8str_trim(utf8_text);
	if (!utf8_text.empty())
	{
		utf8_text = utf8str_truncate(utf8_text, MAX_STRING - 1);
	}
	
	// Don't animate for chats people can't hear (chat to scripts)
	if (animate && (channel == 0))
	{
		if (type == CHAT_TYPE_WHISPER)
		{
			LL_DEBUGS() << "You whisper " << utf8_text << LL_ENDL;
			gAgent.sendAnimationRequest(ANIM_AGENT_WHISPER, ANIM_REQUEST_START);
		}
		else if (type == CHAT_TYPE_NORMAL)
		{
			LL_DEBUGS() << "You say " << utf8_text << LL_ENDL;
			gAgent.sendAnimationRequest(ANIM_AGENT_TALK, ANIM_REQUEST_START);
		}
		else if (type == CHAT_TYPE_SHOUT)
		{
			LL_DEBUGS() << "You shout " << utf8_text << LL_ENDL;
			gAgent.sendAnimationRequest(ANIM_AGENT_SHOUT, ANIM_REQUEST_START);
		}
		else
		{
			LL_INFOS() << "send_chat_from_viewer() - invalid volume" << LL_ENDL;
			return;
		}
	}
	else
	{
		if (type != CHAT_TYPE_START && type != CHAT_TYPE_STOP)
		{
			LL_DEBUGS() << "Channel chat: " << utf8_text << LL_ENDL;
		}
	}
	
	send_chat_from_viewer(utf8_out_text, type, channel);
}

EChatType LLChatUtilities::processChatTypeTriggers(EChatType type, std::string &str)
{
	U32 length = str.length();
	
	for (const auto& ti : sChatTypeTriggers)
	{
		if (length >= ti.first.length())
		{
			std::string trigger = str.substr(0, ti.first.length());
			
			if (!LLStringUtil::compareInsensitive(trigger, ti.first))
			{
				size_t trigger_length = ti.first.length();
				
				// It's to remove space after trigger name
				if (length > trigger_length && str[trigger_length] == ' ')
					trigger_length++;
				
				str = str.substr(trigger_length, length);
				
				if (type == CHAT_TYPE_NORMAL)
					return ti.second;
				else
					break;
			}
		}
	}
	
	return type;
}

LLWString LLChatUtilities::stripChannelNumber(const LLWString &mesg, S32* channel)
{
	if (mesg[0] == '/'
		&& mesg[1] == '/')
	{
		// This is a "repeat channel send"
		*channel = sLastSpecialChatChannel;
		return mesg.substr(2, mesg.length() - 2);
	}
	else if (mesg[0] == '/'
			 && mesg[1]
			 && (LLStringOps::isDigit(mesg[1])
				 || (mesg[1] == '-' && mesg[2] && LLStringOps::isDigit(mesg[2]))))
	{
		// This a special "/20" speak on a channel
		S32 pos = 0;

		// Copy the channel number into a string
		LLWString channel_string;
		llwchar c;
		do
		{
			c = mesg[pos+1];
			channel_string.push_back(c);
			pos++;
		}
		while(c && pos < 64 && (LLStringOps::isDigit(c) || (pos==1 && c =='-')));
		
		// Move the pointer forward to the first non-whitespace char
		// Check isspace before looping, so we can handle "/33foo"
		// as well as "/33 foo"
		while(c && iswspace(c))
		{
			c = mesg[pos+1];
			pos++;
		}
		
		sLastSpecialChatChannel = strtol(wstring_to_utf8str(channel_string).c_str(), nullptr, 10);
		*channel = sLastSpecialChatChannel;
		return mesg.substr(pos, mesg.length() - pos);
	}
	else
	{
		// This is normal chat.
		*channel = 0;
		return mesg;
	}
}

void LLChatUtilities::applyMUPose(std::string& text)
{
	static LLCachedControl<bool> useMUPose(gSavedSettings, "AlchemyChatMUPose", false);
	if (!useMUPose)
		return;

	if (text.at(0) == ':'
		&& text.length() > 3)
	{
		if (text.find(":'") == 0)
		{
			text.replace(0, 1, "/me");
		}
		// Account for emotes and smilies
		else if (!isdigit(text.at(1))
				 && !ispunct(text.at(1))
				 && !isspace(text.at(1)))
		{
			text.replace(0, 1, "/me ");
		}
	}
}

void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel)
{
	LLMessageSystem* msg = gMessageSystem;
	if (channel >= 0)
	{
		
		msg->newMessageFast(_PREHASH_ChatFromViewer);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ChatData);
		msg->addStringFast(_PREHASH_Message, utf8_out_text);
		msg->addU8Fast(_PREHASH_Type, type);
		msg->addS32("Channel", channel);
	}
	else
	{
		msg->newMessageFast(_PREHASH_ScriptDialogReply);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_Data);
		msg->addUUIDFast(_PREHASH_ObjectID, gAgent.getID());
		msg->addS32("ChatChannel", channel);
		msg->addS32Fast(_PREHASH_ButtonIndex, 0);
		msg->addStringFast(_PREHASH_ButtonLabel, utf8_out_text);
	}
	gAgent.sendReliableMessage();
	
	add(LLStatViewer::CHAT_COUNT, 1);
}

//
// LLChatCommandHandler
//

class LLChatCommandHandler : public LLCommandHandler
{
public:
	// not allowed from outside the app
	LLChatCommandHandler() : LLCommandHandler("chat", UNTRUSTED_BLOCK) { }
	
	// Your code here
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web) override
	{
		bool retval = false;
		// Need at least 2 tokens to have a valid message.
		if (tokens.size() < 2)
		{
			retval = false;
		}
		else
		{
			S32 channel = tokens[0].asInteger();
			// VWR-19499 Restrict function to chat channels greater than 0.
			if ((channel > 0) && (channel < CHAT_CHANNEL_DEBUG))
			{
				retval = true;
				// Send unescaped message, see EXT-6353.
				std::string unescaped_mesg (LLURI::unescape(tokens[1].asString()));
				send_chat_from_viewer(unescaped_mesg, CHAT_TYPE_NORMAL, channel);
			}
			else
			{
				retval = false;
				// Tell us this is an unsupported SLurl.
			}
		}
		return retval;
	}
};

// Creating the object registers with the dispatcher.
LLChatCommandHandler gChatHandler;
