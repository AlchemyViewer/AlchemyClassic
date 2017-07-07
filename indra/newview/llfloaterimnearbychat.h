/** 
 * @file llfloaterimnearbychat.h
 * @brief LLFloaterIMNearbyChat class definition
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

#ifndef LL_LLFLOATERIMNEARBYCHAT_H
#define LL_LLFLOATERIMNEARBYCHAT_H

#include "llfloaterimsessiontab.h"
#include "llgesturemgr.h"
#include "llchat.h"
#include "llspeakers.h"

class LLResizeBar;

class LLFloaterIMNearbyChat : public LLFloaterIMSessionTab
{
public:
	// constructor for inline chat-bars (e.g. hosted in chat history window)
	LLFloaterIMNearbyChat(const LLSD& key = LLSD(LLUUID()));
	~LLFloaterIMNearbyChat() {}

	static LLFloaterIMNearbyChat* buildFloater(const LLSD& key);

	BOOL postBuild() override;
	void onOpen(const LLSD& key) override;
	void onClose(bool app_quitting) override;
	void setVisible(BOOL visible) override;
	void setVisibleAndFrontmost(BOOL take_focus=TRUE, const LLSD& key = LLSD()) override;
	void closeHostedFloater() override;

	void loadHistory();
    void reloadMessages(bool clean_messages = false);
	void removeScreenChat();

	void show();
	bool isMessagePanelVisible();
	bool isChatVisible();

	/** @param archive true - to save a message to the chat history log */
	void	addMessage			(const LLChat& message,bool archive = true, const LLSD &args = LLSD());

	LLChatEntry* getChatBox() const { return mInputEditor; }

	std::string getCurrentChat() const;
	S32 getMessageArchiveLength() const {return mMessageArchive.size();}

	BOOL handleKeyHere( KEY key, MASK mask ) override;

	static void startChat(const char* line);
	static void stopChat();

	static bool isWordsName(const std::string& name);

	void showHistory();
	void changeChannelLabel(S32 channel);

protected:
	void onChatBoxKeystroke();
	void onChatBoxFocusLost();
	void onChatBoxFocusReceived();

	void sendChat( EChatType type );
	void onChatBoxCommit();
	void onChatFontChange(LLFontGL* fontp);

	void onTearOffClicked() override;
	void onClickCloseBtn(bool app_qutting = false) override;

	void displaySpeakingIndicator();

	// Which non-zero channel did we last chat on?
	static S32 sLastSpecialChatChannel;

	LLLocalSpeakerMgr*		mSpeakerMgr;

	S32 mExpandedHeight;

private:
	void refresh() override;

	std::vector<LLChat> mMessageArchive;
};

#endif // LL_LLFLOATERIMNEARBYCHAT_H
