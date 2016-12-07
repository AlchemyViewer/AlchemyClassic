// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file   llupdaterservice_test.cpp
 * @brief  Tests of llupdaterservice.cpp.
 * 
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
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

// Precompiled header
#include "linden_common.h"
// associated header
#include "../llupdaterservice.h"
#include "../llupdatechecker.h"
#include "../llupdatedownloader.h"
#include "../llupdateinstaller.h"

#include "../../../test/lltut.h"
//#define DEBUG_ON
#include "../../../test/debug.h"

#include "llevents.h"
#include "lldir.h"

/*****************************************************************************
*   MOCK'd
*****************************************************************************/
LLUpdateChecker::LLUpdateChecker(LLUpdateChecker::Client & client)
{}
void LLUpdateChecker::checkVersion(std::string const & urlBase, 
								   std::string const & channel,
								   std::string const & version,
								   std::string const & platform,
								   std::string const & platform_version,
								   unsigned char       uniqueid[MD5HEX_STR_SIZE],
								   bool                willing_to_test)
{}
LLUpdateDownloader::LLUpdateDownloader(Client & ) {}
void LLUpdateDownloader::download(LLURI const & , std::string const &, std::string const &, std::string const &, std::string const &, bool){}

class LLDir_Mock : public LLDir
{
	void initAppDirs(const std::string &app_name, 
		   			 const std::string& app_read_only_data_dir = "") {}
	U32 countFilesInDir(const std::string &dirname, const std::string &mask) 
	{
		return 0;
	}

	void getRandomFileInDir(const std::string &dirname, 
							const std::string &mask, 
							std::string &fname) {}
	std::string getCurPath() { return ""; }
	bool fileExists(const std::string &filename) const { return false; }
	std::string getLLPluginLauncher() { return ""; }
	std::string getLLPluginFilename(std::string base_name) { return ""; }

} gDirUtil;
LLDir* gDirUtilp = &gDirUtil;
LLDir::LLDir() {}
LLDir::~LLDir() {}
S32 LLDir::deleteFilesInDir(const std::string &dirname, 
							const std::string &mask)
{ return 0; }

void LLDir::setChatLogsDir(const std::string &path){}		
void LLDir::setPerAccountChatLogsDir(const std::string &username, const std::string &gridname){}
void LLDir::setLindenUserDir(const std::string &username, const std::string &gridname){}
void LLDir::setSkinFolder(const std::string &skin_folder, const std::string& language){}
std::string LLDir::getSkinFolder() const { return "default"; }
std::string LLDir::getLanguage() const { return "en"; }
bool LLDir::setCacheDir(const std::string &path){ return true; }
void LLDir::dumpCurrentDirectories() {}
void LLDir::updatePerAccountChatLogsDir() {}

#include "llviewernetwork.h"
LLGridManager::LLGridManager() :
	mGrid("test.grid.lindenlab.com"),
	mPlatform(NOPLATFORM)
{
}
std::string LLGridManager::getUpdateServiceURL() const
{
	return "https://update.secondlife.com/update";
}
LLGridManager::~LLGridManager()
{
}


std::string LLDir::getExpandedFilename(ELLPath location, 
									   const std::string &filename) const 
{
	return "";
}

std::string LLUpdateDownloader::downloadMarkerPath(void)
{
	return "";
}

void LLUpdateDownloader::resume(void) {}
void LLUpdateDownloader::cancel(void) {}
void LLUpdateDownloader::setBandwidthLimit(U64 bytesPerSecond) {}

int ll_install_update(std::string const &, std::string const &, std::string const &, std::string const &, bool, LLInstallScriptMode)
{
	return 0;
}

std::string const & ll_install_failed_marker_path()
{
	static std::string wubba;
	return wubba;
}

/*
#pragma warning(disable: 4273)
llus_mock_llifstream::llus_mock_llifstream(const std::string& _Filename,
										   ios_base::openmode _Mode,
										   int _Prot) :
	std::basic_istream<char,std::char_traits< char > >(NULL,true)
{}

llus_mock_llifstream::~llus_mock_llifstream() {}
bool llus_mock_llifstream::is_open() const {return true;}
void llus_mock_llifstream::close() {}
*/

/*****************************************************************************
*   TUT
*****************************************************************************/
namespace tut
{
    struct llupdaterservice_data
    {
		llupdaterservice_data() :
            pumps(LLEventPumps::instance()),
			test_url("dummy_url"),
			test_channel("dummy_channel"),
			test_version("dummy_version")
		{}
		LLEventPumps& pumps;
		std::string test_url;
		std::string test_channel;
		std::string test_version;
	};

    typedef test_group<llupdaterservice_data> llupdaterservice_group;
    typedef llupdaterservice_group::object llupdaterservice_object;
    llupdaterservice_group llupdaterservicegrp("LLUpdaterService");

    template<> template<>
    void llupdaterservice_object::test<1>()
    {
        DEBUG;
		LLUpdaterService updater;
		bool got_usage_error = false;
		try
		{
			updater.startChecking();
		}
		catch(LLUpdaterService::UsageError)
		{
			got_usage_error = true;
		}
		ensure("Caught start before params", got_usage_error);
	}

    template<> template<>
    void llupdaterservice_object::test<2>()
    {
        DEBUG;
		LLUpdaterService updater;
		bool got_usage_error = false;
		try
		{
			unsigned char id1[MD5HEX_STR_SIZE] = "11111111111111111111111111111111";
			updater.initialize(test_channel, test_version, "win", "1.2.3", id1, true);
			updater.startChecking();
			unsigned char id2[MD5HEX_STR_SIZE] = "22222222222222222222222222222222";
			updater.initialize(test_channel, test_version, "win", "4.5.6", id2, true);
		}
		catch(LLUpdaterService::UsageError)
		{
			got_usage_error = true;
		}
		ensure("Caught params while running", got_usage_error);
	}

    template<> template<>
    void llupdaterservice_object::test<3>()
    {
        DEBUG;
		LLUpdaterService updater;
		unsigned char id[MD5HEX_STR_SIZE] = "33333333333333333333333333333333";
		updater.initialize(test_channel, test_version, "win", "7.8.9", id, true);
		updater.startChecking();
		ensure(updater.isChecking());
		updater.stopChecking();
		ensure(!updater.isChecking());
	}
}
