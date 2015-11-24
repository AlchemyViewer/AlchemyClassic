/** 
 * @file llchatbar.cpp
 * @brief LLChatBar class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "llchatbar.h"

#include "llcombobox.h"
#include "llfocusmgr.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "message.h"

#include "alchatcommand.h"
#include "llagent.h"
#include "llgesturemgr.h"
#include "llviewergesture.h"			// for triggering gestures
#include "llviewermenu.h"		// for deleting object with DEL key
#include "llmultigesture.h"
#include "llviewercontrol.h"

//
// Globals
//
const F32 AGENT_TYPING_TIMEOUT = 5.f;	// seconds

class LLChatBarGestureObserver : public LLGestureManagerObserver
{
public:
	LLChatBarGestureObserver(LLChatBar* chat_barp) : mChatBar(chat_barp){}
	virtual ~LLChatBarGestureObserver() {}
	virtual void changed() { mChatBar->refreshGestures(); }
private:
	LLChatBar* mChatBar;
};


extern void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel);

//
// Functions
//

void applyMUPose(std::string& text)
{
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


LLChatBar::LLChatBar(const LLSD& key)
:	LLFloater(key),
	mInputEditor(NULL),
	mGestureLabelTimer(),
	mLastSpecialChatChannel(0),
	mIsBuilt(FALSE),
	mGestureCombo(NULL),
	mObserver(NULL)
{
	//setIsChrome(TRUE);
}


LLChatBar::~LLChatBar()
{
	LLGestureMgr::instance().removeObserver(mObserver);
	delete mObserver;
	mObserver = NULL;
	// LLView destructor cleans up children
}

BOOL LLChatBar::postBuild()
{
	// * NOTE: mantipov: getChild with default parameters returns dummy widget.
	// Seems this class will be completle removed
	// attempt to bind to an existing combo box named gesture
	setGestureCombo(findChild<LLComboBox>( "Gesture"));

	mInputEditor = getChild<LLLineEditor>("Chat Editor");
	mInputEditor->setKeystrokeCallback(&onInputEditorKeystroke, this);
	mInputEditor->setFocusLostCallback(boost::bind(&LLChatBar::onInputEditorFocusLost));
	mInputEditor->setFocusReceivedCallback(boost::bind(&LLChatBar::onInputEditorGainFocus));
	mInputEditor->setCommitOnFocusLost( FALSE );
	mInputEditor->setRevertOnEsc( FALSE );
	mInputEditor->setIgnoreTab(TRUE);
	mInputEditor->setPassDelete(TRUE);
	mInputEditor->setReplaceNewlinesWithSpaces(FALSE);

	mInputEditor->setMaxTextLength(DB_CHAT_MSG_STR_LEN);
	mInputEditor->setEnableLineHistory(TRUE);

	mIsBuilt = TRUE;

	return TRUE;
}

//-----------------------------------------------------------------------
// Overrides
//-----------------------------------------------------------------------

// virtual
BOOL LLChatBar::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;

	if( KEY_RETURN == key )
	{
		if (mask == MASK_CONTROL)
		{
			// shout
			sendChat(CHAT_TYPE_SHOUT);
			handled = TRUE;
		}
		else if (mask == MASK_NONE)
		{
			// say
			sendChat( CHAT_TYPE_NORMAL );
			handled = TRUE;
		}
	}
	// only do this in main chatbar
	else if ( KEY_ESCAPE == key)
	{
		stopChat();

		handled = TRUE;
	}

	return handled;
}

void LLChatBar::onFocusLost()
{
	stopChat();
}

void LLChatBar::refresh()
{
	// HACK: Leave the name of the gesture in place for a few seconds.
	const F32 SHOW_GESTURE_NAME_TIME = 2.f;
	if (mGestureLabelTimer.getStarted() && mGestureLabelTimer.getElapsedTimeF32() > SHOW_GESTURE_NAME_TIME)
	{
		LLCtrlListInterface* gestures = mGestureCombo ? mGestureCombo->getListInterface() : NULL;
		if (gestures) gestures->selectFirstItem();
		mGestureLabelTimer.stop();
	}

	if ((gAgent.getTypingTime() > AGENT_TYPING_TIMEOUT) && (gAgent.getRenderState() & AGENT_STATE_TYPING))
	{
		gAgent.stopTyping();
	}
}

void LLChatBar::refreshGestures()
{
	if (mGestureCombo)
	{
		//store current selection so we can maintain it
		std::string cur_gesture = mGestureCombo->getValue().asString();
		mGestureCombo->selectFirstItem();
		std::string label = mGestureCombo->getValue().asString();;
		// clear
		mGestureCombo->clearRows();

		// collect list of unique gestures
		std::map <std::string, BOOL> unique;
		LLGestureMgr::item_map_t::const_iterator it;
		const LLGestureMgr::item_map_t& active_gestures = LLGestureMgr::instance().getActiveGestures();
		for (it = active_gestures.begin(); it != active_gestures.end(); ++it)
		{
			LLMultiGesture* gesture = (*it).second;
			if (gesture)
			{
				if (!gesture->mTrigger.empty())
				{
					unique[gesture->mTrigger] = TRUE;
				}
			}
		}

		// add unique gestures
		std::map <std::string, BOOL>::iterator it2;
		for (it2 = unique.begin(); it2 != unique.end(); ++it2)
		{
			mGestureCombo->addSimpleElement((*it2).first);
		}
		
		mGestureCombo->sortByName();
		// Insert label after sorting, at top, with separator below it
		mGestureCombo->addSeparator(ADD_TOP);
		mGestureCombo->addSimpleElement(getString("gesture_label"), ADD_TOP);
		
		if (!cur_gesture.empty())
		{ 
			mGestureCombo->selectByValue(LLSD(cur_gesture));
		}
		else
		{
			mGestureCombo->selectFirstItem();
		}
	}
}

// Move the cursor to the correct input field.
void LLChatBar::setKeyboardFocus(BOOL focus)
{
	if (focus)
	{
		if (mInputEditor)
		{
			mInputEditor->setFocus(TRUE);
			mInputEditor->selectAll();
		}
	}
	else if (gFocusMgr.childHasKeyboardFocus(this))
	{
		if (mInputEditor)
		{
			mInputEditor->deselect();
		}
		setFocus(FALSE);
	}
}


// Ignore arrow keys in chat bar
void LLChatBar::setIgnoreArrowKeys(BOOL b)
{
	if (mInputEditor)
	{
		mInputEditor->setIgnoreArrowKeys(b);
	}
}

BOOL LLChatBar::inputEditorHasFocus()
{
	return mInputEditor && mInputEditor->hasFocus();
}

std::string LLChatBar::getCurrentChat()
{
	return mInputEditor ? mInputEditor->getText() : LLStringUtil::null;
}

void LLChatBar::setGestureCombo(LLComboBox* combo)
{
	mGestureCombo = combo;
	if (mGestureCombo)
	{
		mGestureCombo->setCommitCallback(boost::bind(&LLChatBar::onCommitGesture, this, _1));

		// now register observer since we have a place to put the results
		mObserver = new LLChatBarGestureObserver(this);
		LLGestureMgr::instance().addObserver(mObserver);

		// refresh list from current active gestures
		refreshGestures();
	}
}

//-----------------------------------------------------------------------
// Internal functions
//-----------------------------------------------------------------------


void LLChatBar::sendChat( EChatType type )
{
	if (mInputEditor)
	{
		LLWString text = mInputEditor->getConvertedText();
		if (!text.empty())
		{
			// store sent line in history, duplicates will get filtered
			if (mInputEditor) mInputEditor->updateHistory();
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
				LLGestureMgr::instance().triggerAndReviseString(utf8text, &utf8_revised_text);
			}
			else
			{
				utf8_revised_text = utf8text;
			}

			utf8_revised_text = utf8str_trim(utf8_revised_text);

			if (!utf8_revised_text.empty())
			{
				if(!ALChatCommand::parseCommand(utf8_revised_text))
				{
					// Chat with animation
					sendChatFromViewer(utf8_revised_text, type, gSavedSettings.getBOOL("PlayChatAnim"));
				}
			}
		}
	}

	getChild<LLUICtrl>("Chat Editor")->setValue(LLStringUtil::null);

	gAgent.stopTyping();

	// If the user wants to stop chatting on hitting return, lose focus
	// and go out of chat mode.
	if (gSavedSettings.getBOOL("CloseChatBarOnReturn"))
	{
		stopChat();
	}
}

//-----------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------

// static
void LLChatBar::startChat(const char* line)
{
	LLChatBar* bar = LLFloaterReg::getTypedInstance<LLChatBar>("chatbar");
	bar->setVisible(TRUE);
	bar->setFocus(TRUE);
	
	if (line)
	{
		std::string line_string(line);
		bar->mInputEditor->setText(line_string);
	}
}

// static
void LLChatBar::stopChat()
{
	LLChatBar* bar = LLFloaterReg::getTypedInstance<LLChatBar>("chatbar");
	bar->mInputEditor->setFocus(FALSE);
	bar->setVisible(FALSE);
	gAgent.stopTyping();
}

// static
void LLChatBar::onInputEditorKeystroke( LLLineEditor* caller, void* userdata )
{
	LLChatBar* self = (LLChatBar *)userdata;

	LLWString raw_text;
	if (self->mInputEditor) raw_text = self->mInputEditor->getWText();

	// Can't trim the end, because that will cause autocompletion
	// to eat trailing spaces that might be part of a gesture.
	LLWStringUtil::trimHead(raw_text);

	S32 length = raw_text.length();

	if( (length > 0)
	    && (raw_text[0] != '/')		// forward slash is used for escape (eg. emote) sequences
		    && (raw_text[0] != ':')	// colon is used in for MUD poses
	  )
	
	{
		gAgent.startTyping();
	}
	else
	{
		gAgent.stopTyping();
	}

	KEY key = gKeyboard->currentKey();

	// Ignore "special" keys, like backspace, arrows, etc.
	if (length > 1 
		&& raw_text[0] == '/'
		&& key < KEY_SPECIAL)
	{
		// we're starting a gesture, attempt to autocomplete

		std::string utf8_trigger = wstring_to_utf8str(raw_text);
		std::string utf8_out_str(utf8_trigger);

		if (LLGestureMgr::instance().matchPrefix(utf8_trigger, &utf8_out_str))
		{
			if (self->mInputEditor)
			{
				std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
				self->mInputEditor->setText(utf8_trigger + rest_of_match); // keep original capitalization for user-entered part
				S32 outlength = self->mInputEditor->getLength(); // in characters
			
				// Select to end of line, starting from the character
				// after the last one the user typed.
				self->mInputEditor->setSelection(length, outlength);
			}
		}
	}
}

// static
void LLChatBar::onInputEditorFocusLost()
{
	// stop typing animation
	gAgent.stopTyping();
}

// static
void LLChatBar::onInputEditorGainFocus()
{

}

void LLChatBar::sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate)
{
	sendChatFromViewer(utf8str_to_wstring(utf8text), type, animate);
}

void LLChatBar::sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate)
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
	if (animate && (channel == 0) && gSavedSettings.getBOOL("AlchemyAnimateShoutWhisper"))
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

void LLChatBar::onCommitGesture(LLUICtrl* ctrl)
{
	LLCtrlListInterface* gestures = mGestureCombo ? mGestureCombo->getListInterface() : NULL;
	if (gestures)
	{
		S32 index = gestures->getFirstSelectedIndex();
		if (index == 0)
		{
			return;
		}
		const std::string& trigger = gestures->getSelectedValue().asString();

		// pretend the user chatted the trigger string, to invoke
		// substitution and logging.
		std::string text(trigger);
		std::string revised_text;
		LLGestureMgr::instance().triggerAndReviseString(text, &revised_text);

		revised_text = utf8str_trim(revised_text);
		if (!revised_text.empty())
		{
			// Don't play nodding animation
			sendChatFromViewer(revised_text, CHAT_TYPE_NORMAL, FALSE);
		}
	}
	mGestureLabelTimer.start();
	if (mGestureCombo != NULL)
	{
		// free focus back to chat bar
		mGestureCombo->setFocus(FALSE);
	}
}

// If input of the form "/20foo" or "/20 foo", returns "foo" and channel 20.
// Otherwise returns input and channel 0.
LLWString LLChatBar::stripChannelNumber(const LLWString &mesg, S32* channel)
{
	if (mesg[0] == '/'
		&& mesg[1] == '/')
	{
		// This is a "repeat channel send"
		*channel = mLastSpecialChatChannel;
		return mesg.substr(2, mesg.length() - 2);
	}
	else if (mesg[0] == '/'
			 && mesg[1]
			 && (LLStringOps::isDigit(mesg[1])
				 || mesg[1] == '-' ))

	{
		// This a special "/20" speak on a channel
		S32 pos = 0;
		if(mesg[1] == '-')
			pos++;

		// Copy the channel number into a string
		LLWString channel_string;
		llwchar c;
		do
		{
			c = mesg[pos+1];
			channel_string.push_back(c);
			pos++;
		}
		while(c && pos < 64 && LLStringOps::isDigit(c));
		
		// Move the pointer forward to the first non-whitespace char
		// Check isspace before looping, so we can handle "/33foo"
		// as well as "/33 foo"
		while(c && iswspace(c))
		{
			c = mesg[pos+1];
			pos++;
		}
		
		mLastSpecialChatChannel = strtol(wstring_to_utf8str(channel_string).c_str(), NULL, 10);
		if(mesg[1] == '-')
			mLastSpecialChatChannel = -mLastSpecialChatChannel;
		*channel = mLastSpecialChatChannel;
		return mesg.substr(pos, mesg.length() - pos);
	}
	else
	{
		// This is normal chat.
		*channel = 0;
		return mesg;
	}
}

