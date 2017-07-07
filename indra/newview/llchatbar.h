/** 
 * @file llchatbar.h
 * @brief LLChatBar class definition
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

#ifndef LL_LLCHATBAR_H
#define LL_LLCHATBAR_H

#include "llframetimer.h"
#include "llchatutilities.h"
#include "llfloater.h"

class LLLineEditor;
class LLMessageSystem;
class LLUICtrl;
class LLUUID;
class LLFrameTimer;
class LLChatBarGestureObserver;
class LLComboBox;


class LLChatBar
:	public LLFloater
{
public:
	// constructor for inline chat-bars (e.g. hosted in chat history window)
	LLChatBar(const LLSD& key);
	
	BOOL		postBuild() override;
	void		onOpen(const LLSD& key) override;
	BOOL		handleKeyHere(KEY key, MASK mask) override;
	void		onFocusLost() override;

	void		refresh() override;
	void		refreshGestures();

	// Ignore arrow keys for chat bar
	void		setIgnoreArrowKeys(BOOL b);

	BOOL		inputEditorHasFocus() const;
	std::string	getCurrentChat() const;

	// since chat bar logic is reused for chat history
	// gesture combo box might not be a direct child
	void		setGestureCombo(LLComboBox* combo);

	static void	onInputEditorKeystroke(LLLineEditor* caller, void* userdata);
	static void	onInputEditorFocusLost();
	static void	onInputEditorGainFocus();

	void onCommitGesture(LLUICtrl* ctrl);

	static void startChat(const char* line);
	static void stopChat();

	static void updateChatFont();

protected:
	~LLChatBar();
	
	void sendChat(EChatType type);

	LLLineEditor*	mInputEditor;

	LLFrameTimer	mGestureLabelTimer;

	BOOL			mIsBuilt;
	LLComboBox*		mGestureCombo;

	LLChatBarGestureObserver* mObserver;
};

#endif
