// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llfloaterdestinations.h
 * @author Leyla Farazha
 * @brief floater for the destinations guide
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

/**
 * Floater that appears when buying an object, giving a preview
 * of its contents and their permissions.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterdestinations.h"
#include "llagent.h"
#include "llmediactrl.h"
#include "llviewerregion.h"
#include "llweb.h"

std::string LLFloaterDestinations::sCurrentURL = LLStringUtil::null;

LLFloaterDestinations::LLFloaterDestinations(const LLSD& key)
:	LLFloater(key)
{
}

BOOL LLFloaterDestinations::postBuild()
{
	enableResizeCtrls(true, true, false);
	return TRUE;
}

void LLFloaterDestinations::onOpen(const LLSD& key)
{
	LLViewerRegion *regionp = gAgent.getRegion();
	if (!regionp) return;
	const std::string dest_url = regionp->getDestinationGuideURL();
	if (dest_url == sCurrentURL) return;
	sCurrentURL = dest_url;
	getChild<LLMediaCtrl>("destination_guide_contents")->navigateTo(LLWeb::expandURLSubstitutions(dest_url, LLSD()),
																	HTTP_CONTENT_TEXT_HTML);
}
