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
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llweb.h"

LLFloaterDestinations::LLFloaterDestinations(const LLSD& key)
:	LLFloater(key)
{
}

LLFloaterDestinations::~LLFloaterDestinations()
{
	LLMediaCtrl* avatar_picker = findChild<LLMediaCtrl>("avatar_picker_contents");
	if (avatar_picker)
	{
		LL_INFOS() << "Unloading destinations media" << LL_ENDL;
		avatar_picker->navigateStop();
		avatar_picker->clearCache();          //images are reloading each time already
		avatar_picker->unloadMediaSource();
	}
}

BOOL LLFloaterDestinations::postBuild()
{
	enableResizeCtrls(true, true, false);
	LLMediaCtrl* destinations = findChild<LLMediaCtrl>("destination_guide_contents");
	if (destinations)
	{
		destinations->setErrorPageURL(gSavedSettings.getString("GenericErrorPageURL"));
		LLViewerRegion *regionp = gAgent.getRegion();
		if (regionp)
		{
			std::string dest_url = regionp->getDestinationGuideURL();
			dest_url = LLWeb::expandURLSubstitutions(dest_url, LLSD());
			destinations->navigateTo(dest_url, HTTP_CONTENT_TEXT_HTML);

			std::string authority = LLViewerMedia::sOpenIDURL.mAuthority;
			std::string::size_type hostStart = authority.find('@');
			if (hostStart == std::string::npos)
			{   // no username/password
				hostStart = 0;
			}
			else
			{   // Hostname starts after the @. 
				// Hostname starts after the @. 
				// (If the hostname part is empty, this may put host_start at the end of the string.  In that case, it will end up passing through an empty hostname, which is correct.)
				++hostStart;
			}
			std::string::size_type hostEnd = authority.rfind(':');
			if ((hostEnd == std::string::npos) || (hostEnd < hostStart))
			{   // no port
				hostEnd = authority.size();
			}

			std::string cookie_host = authority.substr(hostStart, hostEnd - hostStart);
			std::string cookie_name = "";
			std::string cookie_value = "";
			std::string cookie_path = "";
			bool httponly = true;
			bool secure = true;
			if (LLViewerMedia::parseRawCookie(LLViewerMedia::sOpenIDCookie, cookie_name, cookie_value, cookie_path, httponly, secure) &&
				destinations->getMediaPlugin())
			{
				// MAINT-5711 - inexplicably, the CEF setCookie function will no longer set the cookie if the 
				// url and domain are not the same. This used to be my.sl.com and id.sl.com respectively and worked.
				// For now, we use the URL for the OpenID POST request since it will have the same authority
				// as the domain field.
				// (Feels like there must be a less dirty way to construct a URL from component LLURL parts)
				// MAINT-6392 - Rider: Do not change, however, the original URI requested, since it is used further
				// down.
				std::string cefUrl(std::string(LLViewerMedia::sOpenIDURL.mURI) + "://" + std::string(LLViewerMedia::sOpenIDURL.mAuthority));

				destinations->getMediaPlugin()->setCookie(cefUrl, cookie_name, cookie_value, cookie_host, cookie_path, httponly, secure);
			}

		}
	}
	return TRUE;
}

