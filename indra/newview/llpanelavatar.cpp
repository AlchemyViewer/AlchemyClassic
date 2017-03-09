// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llpanelavatar.cpp
 * @brief LLPanelAvatar and related class implementations
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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
#include "llpanelavatar.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llcallingcard.h"

#include "lltooldraganddrop.h"
#include "llscrollcontainer.h"
#include "llavatariconctrl.h"
#include "lltextbox.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLDropTarget
//
// This handy class is a simple way to drop something on another
// view. It handles drop events, always setting itself to the size of
// its parent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/* Moved to lldroptarget.cpp */

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LLPanelProfileTab::LLPanelProfileTab()
: LLPanel()
, mAvatarId(LLUUID::null)
{
}

LLPanelProfileTab::~LLPanelProfileTab()
{
	if(getAvatarId().notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(),this);
	}
}

void LLPanelProfileTab::setAvatarId(const LLUUID& id)
{
	if(id.notNull())
	{
		if(getAvatarId().notNull())
		{
			LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarId,this);
		}
		mAvatarId = id;
		LLAvatarPropertiesProcessor::getInstance()->addObserver(getAvatarId(),this);
	}
}

void LLPanelProfileTab::onOpen(const LLSD& key)
{
	// Don't reset panel if we are opening it for same avatar.
	if(getAvatarId() != key.asUUID())
	{
		resetControls();
		resetData();

		scrollToTop();
	}

	// Update data even if we are viewing same avatar profile as some data might been changed.
	setAvatarId(key.asUUID());
	updateData();
	updateButtons();
}

void LLPanelProfileTab::scrollToTop() const
{
	LLScrollContainer* scrollContainer = findChild<LLScrollContainer>("profile_scroll");
	if (scrollContainer)
		scrollContainer->goToTop();
}

void LLPanelProfileTab::onMapButtonClick()
{
	LLAvatarActions::showOnMap(getAvatarId());
}

void LLPanelProfileTab::updateButtons()
{
	bool is_buddy_online = LLAvatarTracker::instance().isBuddyOnline(getAvatarId());
	
	if(LLAvatarActions::isFriend(getAvatarId()))
	{
		getChildView("teleport")->setEnabled(is_buddy_online);
	}
	else
	{
		getChildView("teleport")->setEnabled(true);
	}

	bool enable_map_btn = (is_buddy_online &&
			       LLAvatarActions::isAgentMappable(getAvatarId()))
		|| gAgent.isGodlike();
	getChildView("show_on_map_btn")->setEnabled(enable_map_btn);
}
