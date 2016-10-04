/**
 * @file alpanelquicksettingspulldown.cpp
 * @brief Quick Settings popdown panel
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2013-2014, Alchemy Viewer Project.
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

#include "alpanelquicksettingspulldown.h"

#include "llframetimer.h"

constexpr F32 AUTO_CLOSE_FADE_START_TIME_SEC = 4.f;
constexpr F32 AUTO_CLOSE_TOTAL_TIME_SEC = 5.f;

///----------------------------------------------------------------------------
/// Class ALPanelQuickSettingsPulldown
///----------------------------------------------------------------------------

// Default constructor
ALPanelQuickSettingsPulldown::ALPanelQuickSettingsPulldown() : LLPanel()
{
	mHoverTimer.stop();
	buildFromFile("panel_quick_settings_pulldown.xml");
}

//virtual
void ALPanelQuickSettingsPulldown::draw()
{
	F32 alpha = mHoverTimer.getStarted()
		? clamp_rescale(mHoverTimer.getElapsedTimeF32(), AUTO_CLOSE_FADE_START_TIME_SEC, AUTO_CLOSE_TOTAL_TIME_SEC, 1.f, 0.f)
		: 1.0f;
	LLViewDrawContext context(alpha);

	if (alpha == 0.f)
	{
		setVisible(FALSE);
	}

	LLPanel::draw();
}

/*virtual*/
void ALPanelQuickSettingsPulldown::onMouseEnter(S32 x, S32 y, MASK mask)
{
	mHoverTimer.stop();
	LLPanel::onMouseEnter(x, y, mask);
}

/*virtual*/
void ALPanelQuickSettingsPulldown::onMouseLeave(S32 x, S32 y, MASK mask)
{
	mHoverTimer.start();
	LLPanel::onMouseLeave(x, y, mask);
}

/*virtual*/
void ALPanelQuickSettingsPulldown::onTopLost()
{
	setVisible(FALSE);
}

/*virtual*/
void ALPanelQuickSettingsPulldown::onVisibilityChange(BOOL new_visibility)
{
	new_visibility ? mHoverTimer.start() : mHoverTimer.stop();
}
