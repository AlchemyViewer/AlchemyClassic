/**
 * @file llchatutilities.h
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

#ifndef LL_CHATUTILITIES_H
#define LL_CHATUTILITIES_H

#include "llchat.h"
#include <unordered_map>

namespace LLChatUtilities
{
	static std::unordered_map<std::string, EChatType> sChatTypeTriggers;
	
	// Send a chat (after stripping /20foo channel chats).
	// "Animate" means the nodding animation for regular text.
	void		sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate);
	void		sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate);
	
	// If input of the form "/20foo" or "/20 foo", returns "foo" and channel 20.
	// Otherwise returns input and channel 0.
	LLWString stripChannelNumber(const LLWString &mesg, S32* channel);
	EChatType processChatTypeTriggers(EChatType type, std::string &str);
	void applyMUPose(std::string& text);
}

#endif // LL_CHATUTILITIES_H
