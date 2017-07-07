// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file LLFloaterIMNearbyChat.cpp
 * @brief LLFloaterIMNearbyChat class implementation
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

#include "llappviewer.h"
#include "llfloaterreg.h"
#include "lltrans.h"
#include "llfloaterimcontainer.h"
#include "llfloatersidepanelcontainer.h"
#include "lllogchat.h"
#include "llfloaterimnearbychathandler.h"
#include "llchannelmanager.h"
#include "llchathistory.h"
#include "llavatarnamecache.h"

#include "llfirstuse.h"
#include "llfloaterimnearbychat.h"
#include "llagent.h" // gAgent
#include "llchatentry.h"
#include "llchatutilities.h"
#include "llgesturemgr.h"
#include "llkeyboard.h"
#include "llviewerwindow.h"
#include "llviewerchat.h"
#include "lltranslate.h"
#include "llautoreplace.h"

#include "alchatcommand.h"

using namespace LLChatUtilities;

const S32 EXPANDED_HEIGHT = 266;
const S32 COLLAPSED_HEIGHT = 60;
const S32 EXPANDED_MIN_HEIGHT = 150;

LLFloaterIMNearbyChat::LLFloaterIMNearbyChat(const LLSD& llsd)
:	LLFloaterIMSessionTab(LLSD(LLUUID::null)),
	mSpeakerMgr(NULL),
	mExpandedHeight(COLLAPSED_HEIGHT + EXPANDED_HEIGHT)
{
    mIsP2PChat = false;
	mIsNearbyChat = true;
	mSpeakerMgr = LLLocalSpeakerMgr::getInstance();
}

//static
LLFloaterIMNearbyChat* LLFloaterIMNearbyChat::buildFloater(const LLSD& key)
{
    LLFloaterReg::getInstance("im_container");
    return new LLFloaterIMNearbyChat(key);
}

//virtual
BOOL LLFloaterIMNearbyChat::postBuild()
{
    setIsSingleInstance(TRUE);
    BOOL result = LLFloaterIMSessionTab::postBuild();

	mInputEditor->setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2, _3, _4, _5));
	mInputEditor->setCommitCallback(boost::bind(&LLFloaterIMNearbyChat::onChatBoxCommit, this));
	mInputEditor->setKeystrokeCallback(boost::bind(&LLFloaterIMNearbyChat::onChatBoxKeystroke, this));
	mInputEditor->setFocusLostCallback(boost::bind(&LLFloaterIMNearbyChat::onChatBoxFocusLost, this));
	mInputEditor->setFocusReceivedCallback(boost::bind(&LLFloaterIMNearbyChat::onChatBoxFocusReceived, this));
	changeChannelLabel(gSavedSettings.getS32("AlchemyNearbyChatChannel"));

	// Title must be defined BEFORE call to addConversationListItem() because
	// it is used to show the item's name in the conversations list
	setTitle(LLTrans::getString("NearbyChatTitle"));

	// obsolete, but may be needed for backward compatibility?
	gSavedSettings.declareS32("nearbychat_showicons_and_names", 2, "NearByChat header settings", LLControlVariable::PERSIST_NONDFT);

	if (gSavedPerAccountSettings.getBOOL("LogShowHistory"))
	{
		loadHistory();
	}

	return result;
}

// virtual
void LLFloaterIMNearbyChat::closeHostedFloater()
{
	// If detached from conversations window close anyway
	if (!getHost())
	{
		setVisible(FALSE);
	}

	// Should check how many conversations are ongoing. Select next to "Nearby Chat" in case there are some other besides.
	// Close conversations window in case "Nearby Chat" is attached and the only conversation
	LLFloaterIMContainer* floater_container = LLFloaterIMContainer::getInstance();
	if (floater_container->getConversationListItemSize() == 1)
	{
		if (getHost())
		{
			floater_container->closeFloater();
		}
	}
	else
	{
		if (!getHost())
		{
			floater_container->selectNextConversationByID(LLUUID());
		}
	}
}

// virtual
void LLFloaterIMNearbyChat::refresh()
{
	displaySpeakingIndicator();
	updateCallBtnState(LLVoiceClient::getInstance()->getUserPTTState());

	// *HACK: Update transparency type depending on whether our children have focus.
	// This is needed because this floater is chrome and thus cannot accept focus, so
	// the transparency type setting code from LLFloater::setFocus() isn't reached.
	if (getTransparencyType() != TT_DEFAULT)
	{
		setTransparencyType(hasFocus() ? TT_ACTIVE : TT_INACTIVE);
	}
}

void LLFloaterIMNearbyChat::reloadMessages(bool clean_messages/* = false*/)
{
	if (clean_messages)
	{
		mMessageArchive.clear();
		loadHistory();
	}

	mChatHistory->clear();

	LLSD do_not_log;
	do_not_log["do_not_log"] = true;
	for(std::vector<LLChat>::iterator it = mMessageArchive.begin();it!=mMessageArchive.end();++it)
	{
		// Update the messages without re-writing them to a log file.
		addMessage(*it,false, do_not_log);
	}
	mInputEditor->setFont(LLViewerChat::getChatFont());
}

void LLFloaterIMNearbyChat::loadHistory()
{
	LLSD do_not_log;
	do_not_log["do_not_log"] = true;

	std::list<LLSD> history;
	LLLogChat::loadChatHistory("chat", history);

	std::list<LLSD>::const_iterator it = history.begin();
	while (it != history.end())
	{
		const LLSD& msg = *it;

		std::string from = msg[LL_IM_FROM];
		LLUUID from_id;
		if (msg[LL_IM_FROM_ID].isDefined())
		{
			from_id = msg[LL_IM_FROM_ID].asUUID();
		}
		else
 		{
			std::string legacy_name = gCacheName->buildLegacyName(from);
			from_id = LLAvatarNameCache::findIdByName(legacy_name);
 		}

		LLChat chat;
		chat.mFromName = from;
		chat.mFromID = from_id;
		chat.mText = msg[LL_IM_TEXT].asString();
		chat.mTimeStr = msg[LL_IM_TIME].asString();
		chat.mChatStyle = CHAT_STYLE_HISTORY;

		chat.mSourceType = CHAT_SOURCE_AGENT;
		if (from_id.isNull() && SYSTEM_FROM == from)
		{
			chat.mSourceType = CHAT_SOURCE_SYSTEM;

		}
		else if (from_id.isNull())
		{
			chat.mSourceType = isWordsName(from) ? CHAT_SOURCE_UNKNOWN : CHAT_SOURCE_OBJECT;
		}

		addMessage(chat, true, do_not_log);

		it++;
	}
}

void LLFloaterIMNearbyChat::removeScreenChat()
{
	LLNotificationsUI::LLScreenChannelBase* chat_channel = LLNotificationsUI::LLChannelManager::getInstance()->findChannelByID(LLUUID(gSavedSettings.getString("NearByChatChannelUUID")));
	if(chat_channel)
	{
		chat_channel->removeToastsFromChannel();
	}
}


void LLFloaterIMNearbyChat::setVisible(BOOL visible)
{
	LLFloaterIMSessionTab::setVisible(visible);

	if(visible && isMessagePaneExpanded()) // <alchemy/>
	{
		removeScreenChat();
	}
}


void LLFloaterIMNearbyChat::setVisibleAndFrontmost(BOOL take_focus, const LLSD& key)
{
	LLFloaterIMSessionTab::setVisibleAndFrontmost(take_focus, key);

	if(!isTornOff() && matchesKey(key)) // <alchemy/>
	{
		LLFloaterIMContainer::getInstance()->selectConversationPair(mSessionID, true, take_focus);
	}
}

// virtual
void LLFloaterIMNearbyChat::onTearOffClicked()
{
	LLFloaterIMSessionTab::onTearOffClicked();

	// see CHUI-170: Save torn-off state of the nearby chat between sessions
	bool in_the_multifloater = !!getHost();
	gSavedPerAccountSettings.setBOOL("NearbyChatIsNotTornOff", in_the_multifloater);
}


// virtual
void LLFloaterIMNearbyChat::onOpen(const LLSD& key)
{
	LLFloaterIMSessionTab::onOpen(key);
	if(!isMessagePaneExpanded())
	{
		restoreFloater();
		onCollapseToLine(this);
	}
	showTranslationCheckbox(LLTranslate::isTranslationConfigured());
}

// virtual
void LLFloaterIMNearbyChat::onClose(bool app_quitting)
{
	// Override LLFloaterIMSessionTab::onClose() so that Nearby Chat is not removed from the conversation floater
	LLFloaterIMSessionTab::restoreFloater();
	if (app_quitting)
	{
		// We are starting and closing floater in "expanded" state
		// Update expanded (restored) rect and position for use in next session
		forceReshape();
		storeRectControl();
	}
}

// virtual
void LLFloaterIMNearbyChat::onClickCloseBtn(bool)

{
	if (!isTornOff())
	{
		return;
	}
	closeHostedFloater();
}

void LLFloaterIMNearbyChat::onChatFontChange(LLFontGL* fontp)
{
	// Update things with the new font whohoo
	if (mInputEditor)
	{
		mInputEditor->setFont(fontp);
	}
}


void LLFloaterIMNearbyChat::show()
{
		openFloater(getKey());
}

bool LLFloaterIMNearbyChat::isMessagePanelVisible()
{
	bool isVisible = false;
	LLFloaterIMContainer* im_box = LLFloaterIMContainer::getInstance();
	// Is the IM floater container ever null?
	llassert(im_box != NULL);
	if (im_box)
	{
		isVisible = !isTornOff() ?
			im_box->isShown() && im_box->getSelectedSession().isNull() && !im_box->isMessagesPaneCollapsed() :
			isShown() && isMessagePaneExpanded();
	}

	return isVisible;
}

bool LLFloaterIMNearbyChat::isChatVisible()
{
	bool isVisible = false;
	LLFloaterIMContainer* im_box = LLFloaterIMContainer::getInstance();
	// Is the IM floater container ever null?
	llassert(im_box != NULL);
	if (im_box)
	{
		isVisible = !isTornOff() ?
			im_box->isShown() && im_box->getSelectedSession().isNull() :
			isShown();
	}

	return isVisible;
}

void LLFloaterIMNearbyChat::showHistory()
{
	openFloater();
	LLFloaterIMContainer::getInstance()->selectConversation(LLUUID::null);

	if(!isMessagePaneExpanded())
	{
		restoreFloater();
		setFocus(true);
	}
	else
	{
		LLFloaterIMContainer::getInstance()->setFocus(TRUE);
	}
	setResizeLimits(getMinWidth(), EXPANDED_MIN_HEIGHT);
}

std::string LLFloaterIMNearbyChat::getCurrentChat() const
{
	return mInputEditor ? mInputEditor->getText() : LLStringUtil::null;
}

// virtual
BOOL LLFloaterIMNearbyChat::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;

	if( KEY_RETURN == key && mask == MASK_CONTROL)
	{
		if(gSavedSettings.getBool("AlchemyEnableKeyboardShout"))
		{
			// shout
			sendChat(CHAT_TYPE_SHOUT);
		}
		else // We disabled shouting
		{
			// send chat as normal
			sendChat(CHAT_TYPE_NORMAL);
		}
		handled = TRUE;
	}

	else if (KEY_RETURN == key && mask == MASK_SHIFT)
	{
		if(gSavedSettings.getBool("AlchemyEnableKeyboardWhisper"))
		{
			// whisper
			sendChat(CHAT_TYPE_WHISPER);
		}
		else // We disabled whispering
		{
			// send chat as normal
			sendChat(CHAT_TYPE_NORMAL);
		}
		handled = TRUE;
	}


	if((mask == MASK_ALT) && isTornOff())
	{
		LLFloaterIMContainer* floater_container = LLFloaterIMContainer::getInstance();
		if ((KEY_UP == key) || (KEY_LEFT == key))
		{
			floater_container->selectNextorPreviousConversation(false);
			handled = TRUE;
		}
		if ((KEY_DOWN == key ) || (KEY_RIGHT == key))
		{
			floater_container->selectNextorPreviousConversation(true);
			handled = TRUE;
		}
	}

	return handled;
}

void LLFloaterIMNearbyChat::onChatBoxKeystroke()
{
	LLFloaterIMContainer* im_box = LLFloaterIMContainer::findInstance();
	if (im_box)
	{
		im_box->flashConversationItemWidget(mSessionID,false);
	}

	LLFirstUse::otherAvatarChatFirst(false);

	LLWString raw_text = mInputEditor->getWText();

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

	/* Doesn't work -- can't tell the difference between a backspace
	   that killed the selection vs. backspace at the end of line.
	if (length > 1 
		&& text[0] == '/'
		&& key == KEY_BACKSPACE)
	{
		// the selection will already be deleted, but we need to trim
		// off the character before
		std::string new_text = raw_text.substr(0, length-1);
		mInputEditor->setText( new_text );
		mInputEditor->setCursorToEnd();
		length = length - 1;
	}
	*/

	KEY key = gKeyboard->currentKey();

	// Ignore "special" keys, like backspace, arrows, etc.
	if (gSavedSettings.getBOOL("ChatAutocompleteGestures")
		&& length > 1
		&& raw_text[0] == '/'
		&& key < KEY_SPECIAL)
	{
		// we're starting a gesture, attempt to autocomplete

		std::string utf8_trigger = wstring_to_utf8str(raw_text);
		std::string utf8_out_str(utf8_trigger);

		if (LLGestureMgr::instance().matchPrefix(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			if (!rest_of_match.empty())
			{
				mInputEditor->setText(utf8_trigger + rest_of_match); // keep original capitalization for user-entered part
				// Select to end of line, starting from the character
				// after the last one the user typed.
				mInputEditor->selectByCursorPosition(utf8_out_str.size()-rest_of_match.size(),utf8_out_str.size());
			}

		}
	}
}

// static
void LLFloaterIMNearbyChat::onChatBoxFocusLost()
{
	// stop typing animation
	gAgent.stopTyping();
}

void LLFloaterIMNearbyChat::onChatBoxFocusReceived()
{
	mInputEditor->setEnabled(!gDisconnected);
}

void LLFloaterIMNearbyChat::sendChat( EChatType type )
{
	LLChatUtilities::processChat(mInputEditor, type);

	// If the user wants to stop chatting on hitting return, lose focus
	// and go out of chat mode.
	if (gSavedSettings.getBOOL("CloseChatOnReturn"))
	{
		stopChat();
		if (isTornOff())
		{
			closeHostedFloater();
		}
	}
}

void LLFloaterIMNearbyChat::addMessage(const LLChat& chat,bool archive,const LLSD &args)
{
	appendMessage(chat, args);

	if(archive)
	{
		mMessageArchive.push_back(chat);
		if(mMessageArchive.size() > 200)
		{
			mMessageArchive.erase(mMessageArchive.begin());
		}
	}

	// logging
	if (!args["do_not_log"].asBoolean() && gSavedPerAccountSettings.getS32("KeepConversationLogTranscripts") > 1)
	{
		std::string from_name = chat.mFromName;

		if (chat.mSourceType == CHAT_SOURCE_AGENT)
		{
			// if the chat is coming from an agent, log the complete name
			LLAvatarName av_name;
			LLAvatarNameCache::get(chat.mFromID, &av_name);

			if (!av_name.isDisplayNameDefault())
			{
				from_name = av_name.getCompleteName();
			}
		}

		LLLogChat::saveHistory("chat", from_name, chat.mFromID, chat.mText);
	}
}


void LLFloaterIMNearbyChat::onChatBoxCommit()
{
	sendChat(CHAT_TYPE_NORMAL);

	gAgent.stopTyping();
}

void LLFloaterIMNearbyChat::displaySpeakingIndicator()
{
	LLSpeakerMgr::speaker_list_t speaker_list;
	LLUUID id;

	id.setNull();
	mSpeakerMgr->update(FALSE);
	mSpeakerMgr->getSpeakerList(&speaker_list, FALSE);

	for (LLSpeakerMgr::speaker_list_t::iterator i = speaker_list.begin(); i != speaker_list.end(); ++i)
	{
		LLPointer<LLSpeaker> s = *i;
		if (s->mSpeechVolume > 0 || s->mStatus == LLSpeaker::STATUS_SPEAKING)
		{
			id = s->mID;
			break;
		}
	}
}

void LLFloaterIMNearbyChat::changeChannelLabel(S32 channel)
{
	if (channel == 0)
		mInputEditor->setLabel(LLTrans::getString("NearbyChatTitle"));
	else
	{
		LLStringUtil::format_map_t args;
		args["CHANNEL"] = llformat("%d", channel);
		mInputEditor->setLabel(LLTrans::getString("NearbyChatTitleChannel", args));
	}
}

// static 
bool LLFloaterIMNearbyChat::isWordsName(const std::string& name)
{
	// checking to see if it's display name plus username in parentheses
	size_t open_paren = name.find(" (", 0);
	size_t close_paren = name.find(')', 0);

	if (open_paren != std::string::npos &&
		close_paren == name.length()-1)
	{
		return true;
	}
	else
	{
		//checking for a single space
		size_t pos = name.find(' ', 0);
		return std::string::npos != pos && name.rfind(' ', name.length()) == pos && 0 != pos && name.length()-1 != pos;
	}
}

// static 
void LLFloaterIMNearbyChat::startChat(const char* line)
{
	LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
	if (nearby_chat)
	{
		if(!nearby_chat->isTornOff())
		{
			LLFloaterIMContainer::getInstance()->selectConversation(LLUUID(NULL));
		}
		if(nearby_chat->isMinimized())
		{
			nearby_chat->setMinimized(false);
		}
		nearby_chat->show();
		nearby_chat->setFocus(TRUE);

		if (line)
		{
			std::string line_string(line);
			nearby_chat->mInputEditor->setText(line_string);
		}

		nearby_chat->mInputEditor->endOfDoc();
	}
}

// Exit "chat mode" and do the appropriate focus changes
// static
void LLFloaterIMNearbyChat::stopChat()
{
	LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
	if (nearby_chat)
	{
		nearby_chat->mInputEditor->setFocus(FALSE);
	    gAgent.stopTyping();
	}
}
