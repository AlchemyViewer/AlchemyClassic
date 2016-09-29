/** 
 * @file llfloaterabout.cpp
 * @author James Cook
 * @brief The about box from Help->About
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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
#include <iostream>
#include <fstream>

#include "llfloaterabout.h"

// Viewer includes
#include "llagent.h"
#include "llagentui.h"
#include "llappviewer.h"
#include "llnotificationsutil.h"
#include "llslurl.h"
#include "llvoiceclient.h"
#include "llupdaterservice.h"
#include "llviewertexteditor.h"
#include "llviewercontrol.h"
#include "llviewerstats.h"
#include "llviewerregion.h"
#include "llversioninfo.h"
#include "llweb.h"

// Linden library includes
#include "llaudioengine.h"
#include "llbutton.h"
#include "llglheaders.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llfontfreetype.h"
#include "llimagej2c.h"
#include "llsys.h"
#include "lltrans.h"
#include "lluri.h"
#include "v3dmath.h"
#include "llwindow.h"
#include "stringize.h"
#include "llsdutil_math.h"
#include "lleventapi.h"
#include "llcorehttputil.h"

#ifndef LL_LINUX
#include <cef/llceflib.h>
#endif

#if LL_WINDOWS
#include "lldxhardware.h"
#endif

extern LLMemoryInfo gSysMemory;
extern U32 gPacketsIn;

///----------------------------------------------------------------------------
/// Class LLFloaterAbout
///----------------------------------------------------------------------------
class LLFloaterAbout 
	: public LLFloater
{
	friend class LLServerReleaseNotesURLFetcher;
	friend class LLFloaterReg;
private:
	LLFloaterAbout(const LLSD& key);
	virtual ~LLFloaterAbout();

public:
	/*virtual*/ BOOL postBuild();

	/// Obtain the data used to fill out the contents string. This is
	/// separated so that we can programmatically access the same info.
	static LLSD getInfo(const std::string& server_release_notes_url = "");
	void onClickCopyToClipboard();
	void onClickUpdateCheck();

	// checks state of updater service and starts a check outside of schedule.
	// subscribes callback for closest state update
	static void setUpdateListener();

private:
	void setSupportText(const std::string& server_release_notes_url);

	// notifications for user requested checks
	static void showCheckUpdateNotification(S32 state);

	// callback method for manual checks
	static bool callbackCheckUpdate(LLSD const & event);

	// listener name for update checks
	static const std::string sCheckUpdateListenerName;
	
    static void startFetchServerReleaseNotes();
    static void handleServerReleaseNotes(LLSD results);
};

// Default constructor
LLFloaterAbout::LLFloaterAbout(const LLSD& key) 
:	LLFloater(key)
{
	
}

// Destroys the object
LLFloaterAbout::~LLFloaterAbout()
{
}

BOOL LLFloaterAbout::postBuild()
{
	center();
	LLViewerTextEditor *support_widget = 
		getChild<LLViewerTextEditor>("support_editor", true);

	LLViewerTextEditor *contrib_names_widget = 
		getChild<LLViewerTextEditor>("contrib_names", true);

	LLViewerTextEditor *licenses_widget = 
		getChild<LLViewerTextEditor>("licenses_editor", true);

	getChild<LLUICtrl>("copy_btn")->setCommitCallback(
		boost::bind(&LLFloaterAbout::onClickCopyToClipboard, this));

	getChild<LLUICtrl>("update_btn")->setCommitCallback(
		boost::bind(&LLFloaterAbout::onClickUpdateCheck, this));

	static const LLUIColor about_color = LLUIColorTable::instance().getColor("TextFgReadOnlyColor");

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		// start fetching server release notes URL
		setSupportText(LLTrans::getString("RetrievingData"));
        startFetchServerReleaseNotes();
	}
	else // not logged in
	{
		LL_DEBUGS("ViewerInfo") << "cannot display region info when not connected" << LL_ENDL;
		setSupportText(LLTrans::getString("NotConnected"));
	}

	support_widget->blockUndo();

	// Fix views
	support_widget->setEnabled(FALSE);
	support_widget->startOfDoc();

	// Get the names of contributors, extracted from .../doc/contributions.txt by viewer_manifest.py at build time
	std::string contributors_path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,"contributors.txt");
	llifstream contrib_file;
	std::string contributors;
	contrib_file.open(contributors_path.c_str());		/* Flawfinder: ignore */
	if (contrib_file.is_open())
	{
		std::getline(contrib_file, contributors); // all names are on a single line
		contrib_file.close();
	}
	else
	{
		LL_WARNS("AboutInit") << "Could not read contributors file at " << contributors_path << LL_ENDL;
	}
	contrib_names_widget->setText(contributors);
	contrib_names_widget->setEnabled(FALSE);
	contrib_names_widget->startOfDoc();

    // Get the Versions and Copyrights, created at build time
	std::string licenses_path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,"packages-info.txt");
	llifstream licenses_file;
	licenses_file.open(licenses_path.c_str());		/* Flawfinder: ignore */
	if (licenses_file.is_open())
	{
		std::string license_line;
		licenses_widget->clear();
		while ( std::getline(licenses_file, license_line) )
		{
			licenses_widget->appendText(license_line+"\n", FALSE,
										LLStyle::Params() .color(about_color));
		}
		licenses_file.close();
	}
	else
	{
		// this case will use the (out of date) hard coded value from the XUI
		LL_INFOS("AboutInit") << "Could not read licenses file at " << licenses_path << LL_ENDL;
	}
	licenses_widget->setEnabled(FALSE);
	licenses_widget->startOfDoc();

	return TRUE;
}

LLSD LLFloaterAbout::getInfo(const std::string& server_release_notes_url)
{
	// The point of having one method build an LLSD info block and the other
	// construct the user-visible About string is to ensure that the same info
	// is available to a getInfo() caller as to the user opening
	// LLFloaterAbout.
	LLSD info;
	LLSD version;
	version.append(LLVersionInfo::getMajor());
	version.append(LLVersionInfo::getMinor());
	version.append(LLVersionInfo::getPatch());
	version.append(LLVersionInfo::getBuild());
	info["VIEWER_VERSION"] = version;
	info["VIEWER_VERSION_STR"] = LLVersionInfo::getVersion();
	info["BUILD_DATE"] = __DATE__;
	info["BUILD_TIME"] = __TIME__;
	info["CHANNEL"] = LLVersionInfo::getChannel();

	// return a URL to the release notes for this viewer, such as:
	// http://wiki.secondlife.com/wiki/Release_Notes/Second Life Beta Viewer/2.1.0.123456
	std::string url = LLTrans::getString("RELEASE_NOTES_BASE_URL");
	if (!LLStringUtil::endsWith(url, "/"))
		url += "/";
	url += LLURI::escape(LLVersionInfo::getChannel() + " " + LLVersionInfo::getVersion());
	info["VIEWER_RELEASE_NOTES_URL"] = url;

#if LL_MSVC
	info["COMPILER"] = "MSVC";
	info["COMPILER_VERSION"] = _MSC_FULL_VER; // <alchemy/>
#elif LL_GNUC
	info["COMPILER"] = "GCC";
	info["COMPILER_VERSION"] = GCC_VERSION;
#elif LL_CLANG
	info["COMPILER"] = "Clang";
	info["COMPILER_VERSION"] = __VERSION__;
#elif LL_INTELC
	info["COMPILER"] = "ICC";
	info["COMPILER_VERSION"] = __ICC;
#endif

	// <alchemy>
#if defined(_WIN64) || defined(__amd64__) || defined(__x86_64__)
	info["BUILD_ARCH"] = "x64";
#else
	info["BUILD_ARCH"] = "x86";
#endif
	// </alchemy>

	// Position
	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
		LLVector3d pos = gAgent.getPositionGlobal();
		info["POSITION"] = ll_sd_from_vector3d(pos);
		info["POSITION_LOCAL"] = ll_sd_from_vector3(gAgent.getPosAgentFromGlobal(pos));
		info["REGION"] = gAgent.getRegion()->getName();
		info["HOSTNAME"] = gAgent.getRegion()->getHost().getHostName();
		info["HOSTIP"] = gAgent.getRegion()->getHost().getString();
		info["SERVER_VERSION"] = gLastVersionChannel;
		LLSLURL slurl;
		LLAgentUI::buildSLURL(slurl);
		info["SLURL"] = slurl.getSLURLString();
	}

	// CPU
	info["CPU"] = gSysCPU.getCPUString();
	info["MEMORY_MB"] = LLSD::Integer(gSysMemory.getPhysicalMemoryKB().valueInUnits<LLUnits::Megabytes>());
	// Moved hack adjustment to Windows memory size into llsys.cpp
	info["OS_VERSION"] = LLAppViewer::instance()->getOSInfo().getOSString();
	info["GRAPHICS_CARD_VENDOR"] = (const char*) (glGetString(GL_VENDOR));
	info["GRAPHICS_CARD"] = (const char*) (glGetString(GL_RENDERER));

#if LL_WINDOWS
	LLSD driver_info = gDXHardware.getDisplayInfo();
	if (driver_info.has("DriverVersion"))
	{
		info["GRAPHICS_DRIVER_VERSION"] = driver_info["DriverVersion"];
	}
#endif

	info["OPENGL_VERSION"] = (const char*) (glGetString(GL_VERSION));
	info["LIBCURL_VERSION"] = LLCore::LLHttp::getCURLVersion();
	info["J2C_VERSION"] = LLImageJ2C::getEngineInfo();
	info["FONT_VERSION"] = LLFontFreetype::getVersionString();
	bool want_fullname = true;
	info["AUDIO_DRIVER_VERSION"] = gAudiop ? LLSD(gAudiop->getDriverName(want_fullname)) : LLSD();
	if (LLVoiceClient::getInstance()->voiceEnabled())
	{
		LLVoiceVersionInfo version = LLVoiceClient::getInstance()->getVersion();
		std::ostringstream version_string;
		version_string << version.serverType << " " << version.serverVersion << std::endl;
		info["VOICE_VERSION"] = version_string.str();
	}
	else
	{
		info["VOICE_VERSION"] = LLTrans::getString("NotConnected");
	}

	info["LLCEFLIB_VERSION"] = LLCEFLIB_VERSION;

	S32 packets_in = LLViewerStats::instance().getRecording().getSum(LLStatViewer::PACKETS_IN);
	if (packets_in > 0)
	{
		info["PACKETS_LOST"] = LLViewerStats::instance().getRecording().getSum(LLStatViewer::PACKETS_LOST);
		info["PACKETS_IN"] = packets_in;
		info["PACKETS_PCT"] = 100.f*info["PACKETS_LOST"].asReal() / info["PACKETS_IN"].asReal();
	}

	if (LLStringUtil::startsWith(server_release_notes_url, "http")) // it's an URL
	{
		info["SERVER_RELEASE_NOTES_URL"] = "[" + LLWeb::escapeURL(server_release_notes_url) + " " + LLTrans::getString("ReleaseNotes") + "]";
	}
	else
	{
		info["SERVER_RELEASE_NOTES_URL"] = server_release_notes_url;
	}

	return info;
}

/*static*/
void LLFloaterAbout::startFetchServerReleaseNotes()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region) return;

    // We cannot display the URL returned by the ServerReleaseNotes capability
    // because opening it in an external browser will trigger a warning about untrusted
    // SSL certificate.
    // So we query the URL ourselves, expecting to find
    // an URL suitable for external browsers in the "Location:" HTTP header.
    std::string cap_url = region->getCapability("ServerReleaseNotes");

    LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet(cap_url,
        &LLFloaterAbout::handleServerReleaseNotes, &LLFloaterAbout::handleServerReleaseNotes);

}

/*static*/
void LLFloaterAbout::handleServerReleaseNotes(LLSD results)
{
     LLFloaterAbout* floater_about = LLFloaterReg::findTypedInstance<LLFloaterAbout>("sl_about");
     if (floater_about)
     {
        LLSD http_headers;
        if (results.has(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS))
        {
            LLSD http_results = results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
            http_headers = http_results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
        }
        else
        {
            http_headers = results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
        }
        
        std::string location = http_headers[HTTP_IN_HEADER_LOCATION].asString();
        if (location.empty())
        {
            location = LLTrans::getString("ErrorFetchingServerReleaseNotesURL");
        }
		floater_about->setSupportText(location);
    }
}

class LLFloaterAboutListener: public LLEventAPI
{
public:
	LLFloaterAboutListener():
		LLEventAPI("LLFloaterAbout",
                   "LLFloaterAbout listener to retrieve About box info")
	{
		add("getInfo",
            "Request an LLSD::Map containing information used to populate About box",
            &LLFloaterAboutListener::getInfo,
            LLSD().with("reply", LLSD()));
	}

private:
	void getInfo(const LLSD& request) const
	{
		LLReqID reqid(request);
		LLSD reply(LLFloaterAbout::getInfo());
		reqid.stamp(reply);
		LLEventPumps::instance().obtain(request["reply"]).post(reply);
	}
};

static LLFloaterAboutListener floaterAboutListener;

void LLFloaterAbout::onClickCopyToClipboard()
{
	LLViewerTextEditor *support_widget = 
		getChild<LLViewerTextEditor>("support_editor", true);
	support_widget->selectAll();
	support_widget->copy();
	support_widget->deselect();
}

void LLFloaterAbout::onClickUpdateCheck()
{
	setUpdateListener();
}

void LLFloaterAbout::setSupportText(const std::string& server_release_notes_url)
{
#if LL_WINDOWS
	getWindow()->incBusyCount();
	getWindow()->setCursor(UI_CURSOR_ARROW);
#endif
	LLSD info(getInfo(server_release_notes_url));
#if LL_WINDOWS
	getWindow()->decBusyCount();
	getWindow()->setCursor(UI_CURSOR_ARROW);
#endif

	// Render the LLSD from getInfo() as a format_map_t
	LLStringUtil::format_map_t args;

	// allow the "Release Notes" URL label to be localized
	args["ReleaseNotes"] = LLTrans::getString("ReleaseNotes");

	for (LLSD::map_const_iterator ii(info.beginMap()), iend(info.endMap());
	ii != iend; ++ii)
	{
		if (!ii->second.isArray())
		{
			// Scalar value
			if (ii->second.isUndefined())
			{
				args[ii->first] = LLTrans::getString("none_text");
			}
			else
			{
				// don't forget to render value asString()
				args[ii->first] = ii->second.asString();
			}
		}
		else
		{
			// array value: build KEY_0, KEY_1 etc. entries
			for (LLSD::Integer n(0), size(ii->second.size()); n < size; ++n)
			{
				args[STRINGIZE(ii->first << '_' << n)] = ii->second[n].asString();
			}
		}
	}

	// Now build the various pieces
	std::ostringstream support;
	support << LLTrans::getString("AboutHeader", args);
	if (info.has("REGION"))
	{
		support << "\n\n" << LLTrans::getString("AboutPosition", args);
	}
	support << "\n\n" << LLTrans::getString("AboutSystem", args);
	support << "\n";
	if (info.has("GRAPHICS_DRIVER_VERSION"))
	{
		support << "\n" << LLTrans::getString("AboutDriver", args);
	}
	support << "\n" << LLTrans::getString("AboutLibs", args);
	if (info.has("COMPILER"))
	{
		support << "\n" << LLTrans::getString("AboutCompiler", args);
	}
	if (info.has("PACKETS_IN"))
	{
		support << '\n' << LLTrans::getString("AboutTraffic", args);
	}

	LLViewerTextEditor *support_widget = getChild<LLViewerTextEditor>("support_editor", true);
	static LLUIColor about_color = LLUIColorTable::instance().getColor("TextFgReadOnlyColor");
	support_widget->setText(support.str(),
							   LLStyle::Params().color(about_color));
}

///----------------------------------------------------------------------------
/// Floater About Update-check related functions
///----------------------------------------------------------------------------

const std::string LLFloaterAbout::sCheckUpdateListenerName = "LLUpdateNotificationListener";

void LLFloaterAbout::showCheckUpdateNotification(S32 state)
{
	switch (state)
	{
	case LLUpdaterService::UP_TO_DATE:
		LLNotificationsUtil::add("UpdateViewerUpToDate");
		break;
	case LLUpdaterService::DOWNLOADING:
	case LLUpdaterService::INSTALLING:
		LLNotificationsUtil::add("UpdateDownloadInProgress");
		break;
	case LLUpdaterService::TERMINAL:
		// download complete, user triggered check after download pop-up appeared
		LLNotificationsUtil::add("UpdateDownloadComplete");
		break;
	default:
		LLNotificationsUtil::add("UpdateCheckError");
		break;
	}
}

bool LLFloaterAbout::callbackCheckUpdate(LLSD const & event)
{
	if (!event.has("payload"))
	{
		return false;
	}

	LLSD payload = event["payload"];
	if (payload.has("type") && payload["type"].asInteger() == LLUpdaterService::STATE_CHANGE)
	{
		LLEventPumps::instance().obtain("mainlooprepeater").stopListening(sCheckUpdateListenerName);
		showCheckUpdateNotification(payload["state"].asInteger());
	}
	return false;
}

void LLFloaterAbout::setUpdateListener()
{
	LLUpdaterService update_service;
	S32 service_state = update_service.getState();
	// Note: Do not set state listener before forceCheck() since it set's new state
	if (update_service.forceCheck() || service_state == LLUpdaterService::CHECKING_FOR_UPDATE)
	{
		LLEventPump& mainloop(LLEventPumps::instance().obtain("mainlooprepeater"));
		if (mainloop.getListener(sCheckUpdateListenerName) == LLBoundListener()) // dummy listener
		{
			mainloop.listen(sCheckUpdateListenerName, boost::bind(&callbackCheckUpdate, _1));
		}
	}
	else
	{
		showCheckUpdateNotification(service_state);
	}
}

///----------------------------------------------------------------------------
/// LLFloaterAboutUtil
///----------------------------------------------------------------------------
void LLFloaterAboutUtil::registerFloater()
{
	LLFloaterReg::add("sl_about", "floater_about.xml",
		&LLFloaterReg::build<LLFloaterAbout>);

}

void LLFloaterAboutUtil::checkUpdatesAndNotify()
{
	LLFloaterAbout::setUpdateListener();
}

