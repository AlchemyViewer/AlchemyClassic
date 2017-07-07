// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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

#include "alchatcommand.h"
#include "llagent.h"
#include "llgesturemgr.h"
#include "llviewermenu.h"		// for deleting object with DEL key
#include "llmultigesture.h"
#include "llviewerchat.h"
#include "llviewercontrol.h"

using namespace LLChatUtilities;

class LLChatBarGestureObserver : public LLGestureManagerObserver
{
public:
	LLChatBarGestureObserver(LLChatBar* chat_barp) : mChatBar(chat_barp){}
	virtual ~LLChatBarGestureObserver() {}
	void changed() override { mChatBar->refreshGestures(); }
private:
	LLChatBar* mChatBar;
};

//
// Functions
//

LLChatBar::LLChatBar(const LLSD& key)
:	LLFloater(key),
	mInputEditor(nullptr),
	mGestureLabelTimer(),
	mIsBuilt(FALSE),
	mGestureCombo(nullptr),
	mObserver(nullptr)
{
	mCommitCallbackRegistrar.add("Chatbar.Shout", boost::bind(&LLChatBar::sendChat, this, CHAT_TYPE_SHOUT));
	mCommitCallbackRegistrar.add("Chatbar.Whisper", boost::bind(&LLChatBar::sendChat, this, CHAT_TYPE_WHISPER));
	//setIsChrome(TRUE);
}


LLChatBar::~LLChatBar()
{
	LLGestureMgr::instance().removeObserver(mObserver);
	delete mObserver;
	mObserver = nullptr;
	// LLView destructor cleans up children
}

//-----------------------------------------------------------------------
// Overrides
//-----------------------------------------------------------------------

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
	
	mInputEditor->setFont(LLViewerChat::getChatFont());

	mIsBuilt = TRUE;

	return TRUE;
}

void LLChatBar::onOpen(const LLSD& key)
{
	mInputEditor->setFocus(TRUE);
}

// virtual
BOOL LLChatBar::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;

	if( KEY_RETURN == key )
	{
		if (mask == MASK_CONTROL && gSavedSettings.getBool("AlchemyEnableKeyboardShout"))
		{
			// shout
			sendChat(CHAT_TYPE_SHOUT);
			handled = TRUE;
		}
		else if (mask == MASK_SHIFT && gSavedSettings.getBool("AlchemyEnableKeyboardWhisper"))
		{
			sendChat(CHAT_TYPE_WHISPER);
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

	if ((gAgent.getTypingTime() > LLAgent::TYPING_TIMEOUT_SECS) && (gAgent.getRenderState() & AGENT_STATE_TYPING))
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

		// clear
		mGestureCombo->clearRows();

		// collect list of unique gestures
		std::map <std::string, BOOL> unique;
		const LLGestureMgr::item_map_t& active_gestures = LLGestureMgr::instance().getActiveGestures();
		for (auto it = active_gestures.cbegin(); it != active_gestures.cend(); ++it)
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

		for (auto it2 = unique.begin(); it2 != unique.end(); ++it2)
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


// Ignore arrow keys in chat bar
void LLChatBar::setIgnoreArrowKeys(BOOL b)
{
	if (mInputEditor)
	{
		mInputEditor->setIgnoreArrowKeys(b);
	}
}

BOOL LLChatBar::inputEditorHasFocus() const
{
	return mInputEditor && mInputEditor->hasFocus();
}

std::string LLChatBar::getCurrentChat() const
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
	LLChatUtilities::processChat(mInputEditor, type);

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
	bar->mInputEditor->setFocus(TRUE);
	
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
void LLChatBar::updateChatFont()
{
	LLChatBar* bar = LLFloaterReg::getTypedInstance<LLChatBar>("chatbar");
	if (bar)
	{
		bar->mInputEditor->setFont(LLViewerChat::getChatFont());
	}
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
	if (mGestureCombo != nullptr)
	{
		// free focus back to chat bar
		mGestureCombo->setFocus(FALSE);
	}
}

