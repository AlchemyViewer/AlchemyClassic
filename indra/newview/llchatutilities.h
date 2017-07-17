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

#include "llagent.h"
#include "llgesturemgr.h"
#include "alchatcommand.h"
#include "llviewercontrol.h"

#include <type_traits>

namespace LLChatUtilities
{
	// Send a chat (after stripping /20foo channel chats).
	// "Animate" means the nodding animation for regular text.
	void		sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate);
	void		sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate);
	
	// If input of the form "/20foo" or "/20 foo", returns "foo" and channel 20.
	// Otherwise returns input and channel 0.
	LLWString stripChannelNumber(const LLWString &mesg, S32* channel);
	EChatType processChatTypeTriggers(EChatType type, std::string &str);
	void applyMUPose(std::string& text);

	template <class T> void processChat(T* editor, EChatType type);
}

template<typename> struct Void { using type = void; };

template<typename, typename = void> struct has_getWText : std::false_type {};
template<typename T>
struct has_getWText<T, typename Void<decltype(std::declval<T*>()->getWText())>::type> : std::true_type {};

template <class T>
void LLChatUtilities::processChat(T* editor, EChatType type)
{
	static_assert(has_getWText<T>::value, "Class must implement getWText()");
	//static_assert(has_setText<T>::value, "Class must implement setText(const std::string&)");
	if (editor)
	{
		LLWString text = editor->getWText();
		LLWStringUtil::trim(text);
		LLWStringUtil::replaceChar(text, 182, '\n'); // Convert paragraph symbols back into newlines.
		if (!text.empty())
		{
			// Check if this is destined for another channel
			S32 channel = 0;
			stripChannelNumber(text, &channel);

			std::string utf8text = wstring_to_utf8str(text);
			// Try to trigger a gesture, if not chat to a script.
			std::string utf8_revised_text;
			if (0 == channel)
			{
				applyMUPose(utf8text);

				// discard returned "found" boolean
				if (!LLGestureMgr::instance().triggerAndReviseString(utf8text, &utf8_revised_text))
				{
					utf8_revised_text = utf8text;
				}
			}
			else
			{
				utf8_revised_text = utf8text;
			}

			utf8_revised_text = utf8str_trim(utf8_revised_text);

			type = processChatTypeTriggers(type, utf8_revised_text);

			if (!utf8_revised_text.empty())
			{
				if (!ALChatCommand::parseCommand(utf8_revised_text))
				{
					// Chat with animation
					sendChatFromViewer(utf8_revised_text, type, gSavedSettings.getBOOL("PlayChatAnim"));
				}
			}
		}

		editor->setText(LLStringUtil::null);
	}
	gAgent.stopTyping();
}

#endif // LL_CHATUTILITIES_H
