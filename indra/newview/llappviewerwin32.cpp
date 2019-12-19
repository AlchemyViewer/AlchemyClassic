/**
 * @file llappviewerwin32.cpp
 * @brief The LLAppViewerWin32 class definitions
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#include "llviewerbuildconfig.h"

#ifdef INCLUDE_VLD
#define VLD_FORCE_ENABLE 1
#include "vld.h"
#endif
#include "llwin32headerslean.h"
# include <psapi.h>

#include "llwindow.h" // *FIX: for setting gIconResource.

#include "llappviewerwin32.h"

#include "llgl.h"
//#include "res/resource.h" // *FIX: for setting gIconResource.

#include <fcntl.h>		//_O_APPEND
#include <io.h>			//_open_osfhandle()
#include <process.h>	// _spawnl()
#include <tchar.h>		// For TCHAR support
#include <Werapi.h>
#include <VersionHelpers.h>

#include "llviewercontrol.h"
#include "lldxhardware.h"

#if USE_NVAPI
#include "nvapi/nvapi.h"
#include "nvapi/NvApiDriverSettings.h"
#endif

#include <cstdlib>

#include "llweb.h"

#include "llviewernetwork.h"
#include "llmd5.h"
#include "llfindlocale.h"
#include "llversioninfo.h"

#include "llcommandlineparser.h"
#include "lltrans.h"

#include "lldir.h"
#include "llerrorcontrol.h"
#include <fstream>
#include <exception>

#if defined(USE_CRASHPAD)
#pragma warning(disable:4265)

#include "client/crash_report_database.h"
#include <client/crashpad_client.h>
#include <client/prune_crash_reports.h>
#include <client/settings.h>
#endif

namespace
{
    void (*gOldTerminateHandler)() = nullptr;
}

static void exceptionTerminateHandler()
{
	// reinstall default terminate() handler in case we re-terminate.
	if (gOldTerminateHandler) std::set_terminate(gOldTerminateHandler);
	// treat this like a regular viewer crash, with nice stacktrace etc.
    long *null_ptr;
    null_ptr = nullptr;
    *null_ptr = 0xDEADBEEF; //Force an exception that will trigger breakpad.
	//LLAppViewer::handleViewerCrash();
	// we've probably been killed-off before now, but...
	gOldTerminateHandler(); // call old terminate() handler
}

LONG WINAPI catchallCrashHandler(EXCEPTION_POINTERS * /*ExceptionInfo*/)
{
	LL_WARNS() << "Hit last ditch-effort attempt to catch crash." << LL_ENDL;
	exceptionTerminateHandler();
	return 0;
}

const std::string LLAppViewerWin32::sWindowClass = "Alchemy";

// Create app mutex creates a unique global windows object. 
// If the object can be created it returns true, otherwise
// it returns false. The false result can be used to determine 
// if another instance of a second life app (this vers. or later)
// is running.
// *NOTE: Do not use this method to run a single instance of the app.
// This is intended to help debug problems with the cross-platform 
// locked file method used for that purpose.
bool create_app_mutex()
{
	bool result = true;
	LPCWSTR unique_mutex_name = L"SecondLifeAppMutex";
	HANDLE hMutex;
	hMutex = CreateMutex(nullptr, TRUE, unique_mutex_name); 
	if(GetLastError() == ERROR_ALREADY_EXISTS) 
	{     
		result = false;
	}
	return result;
}

#if USE_NVAPI
#define NVAPI_APPNAME L"Second Life"

/*
This function is used to print to the command line a text message
describing the nvapi error and quits
*/
void nvapi_error(NvAPI_Status status)
{
	NvAPI_ShortString szDesc = { 0 };
	NvAPI_GetErrorMessage(status, szDesc);
	LL_WARNS() << szDesc << LL_ENDL;

	//should always trigger when asserts are enabled
	//llassert(status == NVAPI_OK);
}

void ll_nvapi_init(NvDRSSessionHandle hSession)
{
	// (2) load all the system settings into the session
	NvAPI_Status status = NvAPI_DRS_LoadSettings(hSession);
	if (status != NVAPI_OK) 
	{
		nvapi_error(status);
		return;
	}

	NvAPI_UnicodeString wsz = { 0 };
	memcpy_s(wsz, sizeof(wsz), NVAPI_APPNAME, sizeof(NVAPI_APPNAME));
	status = NvAPI_DRS_SetCurrentGlobalProfile(hSession, wsz);
	if (status != NVAPI_OK)
	{
		nvapi_error(status);
		return;
	}

	// (3) Obtain the current profile. 
	NvDRSProfileHandle hProfile = 0;
	status = NvAPI_DRS_GetCurrentGlobalProfile(hSession, &hProfile);
	if (status != NVAPI_OK) 
	{
		nvapi_error(status);
		return;
	}

	// load settings for querying 
	status = NvAPI_DRS_LoadSettings(hSession);
	if (status != NVAPI_OK)
	{
		nvapi_error(status);
		return;
	}

	//get the preferred power management mode for Second Life
	NVDRS_SETTING drsSetting = {0};
	drsSetting.version = NVDRS_SETTING_VER;
	status = NvAPI_DRS_GetSetting(hSession, hProfile, PREFERRED_PSTATE_ID, &drsSetting);
	if (status == NVAPI_SETTING_NOT_FOUND)
	{ //only override if the user hasn't specifically set this setting
		// (4) Specify that we want the VSYNC disabled setting
		// first we fill the NVDRS_SETTING struct, then we call the function
		drsSetting.version = NVDRS_SETTING_VER;
		drsSetting.settingId = PREFERRED_PSTATE_ID;
		drsSetting.settingType = NVDRS_DWORD_TYPE;
		drsSetting.u32CurrentValue = PREFERRED_PSTATE_PREFER_MAX;
		status = NvAPI_DRS_SetSetting(hSession, hProfile, &drsSetting);
		if (status != NVAPI_OK) 
		{
			nvapi_error(status);
			return;
		}

        // (5) Now we apply (or save) our changes to the system
        status = NvAPI_DRS_SaveSettings(hSession);
        if (status != NVAPI_OK) 
        {
            nvapi_error(status);
            return;
        }
	}
	else if (status != NVAPI_OK)
	{
		nvapi_error(status);
		return;
	}
}
#endif

//#define DEBUGGING_SEH_FILTER 1
#if DEBUGGING_SEH_FILTER
#	define WINMAIN DebuggingWinMain
#else
#	define WINMAIN wWinMain
#endif

int APIENTRY WINMAIN(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     PWSTR     pCmdLine,
                     int       nCmdShow)
{
#if WINDOWS_CRT_MEM_CHECKS && !INCLUDE_VLD
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF ); // dump memory leaks on exit
#endif
	
	LLAppViewerWin32* viewer_app_ptr = new LLAppViewerWin32(ll_convert_wide_to_string(pCmdLine).c_str());

	gOldTerminateHandler = std::set_terminate(exceptionTerminateHandler);

#if !defined(USE_CRASHPAD)
	viewer_app_ptr->setErrorHandler(LLAppViewer::handleViewerCrash);
#endif

	// Set a debug info flag to indicate if multiple instances are running.
	bool found_other_instance = !create_app_mutex();
	gDebugInfo["FoundOtherInstanceAtStartup"] = LLSD::Boolean(found_other_instance);

	bool ok = viewer_app_ptr->init();
	if(!ok)
	{
		LL_WARNS() << "Application init failed." << LL_ENDL;
		return -1;
	}

#if USE_NVAPI
	NvAPI_Status status;
    
	// Initialize NVAPI
	status = NvAPI_Initialize();
	NvDRSSessionHandle hSession = 0;

    if (status == NVAPI_OK) 
	{
		// Create the session handle to access driver settings
		status = NvAPI_DRS_CreateSession(&hSession);
		if (status != NVAPI_OK) 
		{
			nvapi_error(status);
		}
		else
		{
			//override driver setting as needed
			ll_nvapi_init(hSession);
		}
	}
#endif


	// Run the application main loop
	while (! viewer_app_ptr->frame()) 
	{}

	if (!LLApp::isError())
	{
		//
		// We don't want to do cleanup here if the error handler got called -
		// the assumption is that the error handler is responsible for doing
		// app cleanup if there was a problem.
		//
#if WINDOWS_CRT_MEM_CHECKS
		LL_INFOS() << "CRT Checking memory:" << LL_ENDL;
		if (!_CrtCheckMemory())
		{
			LL_WARNS() << "_CrtCheckMemory() failed at prior to cleanup!" << LL_ENDL;
		}
		else
		{
			LL_INFOS() << " No corruption detected." << LL_ENDL;
		}
#endif

		gGLActive = TRUE;

		viewer_app_ptr->cleanup();

#if WINDOWS_CRT_MEM_CHECKS
		LL_INFOS() << "CRT Checking memory:" << LL_ENDL;
		if (!_CrtCheckMemory())
		{
			LL_WARNS() << "_CrtCheckMemory() failed after cleanup!" << LL_ENDL;
		}
		else
		{
			LL_INFOS() << " No corruption detected." << LL_ENDL;
		}
#endif

	}
	delete viewer_app_ptr;
	viewer_app_ptr = nullptr;

#if USE_NVAPI
	// (NVAPI) (6) We clean up. This is analogous to doing a free()
	if (hSession)
	{
		NvAPI_DRS_DestroySession(hSession);
		hSession = 0;
	}
#endif

	return 0;
}

#if DEBUGGING_SEH_FILTER
// The compiler doesn't like it when you use __try/__except blocks
// in a method that uses object destructors. Go figure.
// This winmain just calls the real winmain inside __try.
// The __except calls our exception filter function. For debugging purposes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     PWSTR     lpCmdLine,
                     int       nCmdShow)
{
    __try
    {
        WINMAIN(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
    }
    __except( viewer_windows_exception_handler( GetExceptionInformation() ) )
    {
        _tprintf( _T("Exception handled.\n") );
    }
}
#endif

void LLAppViewerWin32::disableWinErrorReporting()
{
	std::string const& executable_name = gDirUtilp->getExecutableFilename();

	if( S_OK == WerAddExcludedApplication( ll_convert_string_to_wide(executable_name).c_str(), FALSE ) )
	{
		LL_INFOS() << "WerAddExcludedApplication() succeeded for " << executable_name << LL_ENDL;
	}
	else
	{
		LL_INFOS() << "WerAddExcludedApplication() failed for " << executable_name << LL_ENDL;
	}
}

const S32 MAX_CONSOLE_LINES = 500;

static bool create_console()
{

	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	// allocate a console for this app
	const bool isConsoleAllocated = AllocConsole();

	// set the screen buffer to be big enough to let us scroll text
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = MAX_CONSOLE_LINES;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	// Redirect the CRT standard input, output, and error handles to the console
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	setvbuf( stdin, nullptr, _IONBF, 0 );
	setvbuf( stdout, nullptr, _IONBF, 0 );
	setvbuf( stderr, nullptr, _IONBF, 0 );

    return isConsoleAllocated;
}

LLAppViewerWin32::LLAppViewerWin32(const char* cmd_line) :
	mCmdLine(cmd_line),
	mIsConsoleAllocated(false)
{
}

LLAppViewerWin32::~LLAppViewerWin32()
{
}

bool LLAppViewerWin32::init()
{
	// Platform specific initialization.
	
	// Turn off Windows Error Reporting
	// (Don't send our data to Microsoft--at least until we are Logo approved and have a way
	// of getting the data back from them.)
	//
	// LL_INFOS() << "Turning off Windows error reporting." << LL_ENDL;
	disableWinErrorReporting();

#if defined(USE_CRASHPAD)
	LLAppViewer* pApp = LLAppViewer::instance();
	pApp->initCrashReporting();
#endif

	bool success = LLAppViewer::init();

    return success;
}

bool LLAppViewerWin32::cleanup()
{
	bool result = LLAppViewer::cleanup();

	gDXHardware.cleanup();

	if (mIsConsoleAllocated)
	{
		FreeConsole();
		mIsConsoleAllocated = false;
	}

	return result;
}

void LLAppViewerWin32::initLoggingAndGetLastDuration()
{
	LLAppViewer::initLoggingAndGetLastDuration();
}

void LLAppViewerWin32::initConsole()
{
	// pop up debug console
	mIsConsoleAllocated = create_console();
	return LLAppViewer::initConsole();
}

void write_debug_dx(const char* str)
{
	std::string value = gDebugInfo["DXInfo"].asString();
	value += str;
	gDebugInfo["DXInfo"] = value;
}

void write_debug_dx(const std::string& str)
{
	write_debug_dx(str.c_str());
}

bool LLAppViewerWin32::initHardwareTest()
{
	//
	// Do driver verification and initialization based on DirectX
	// hardware polling and driver versions
	//
	if (FALSE == gSavedSettings.getBOOL("NoHardwareProbe"))
	{
		LLSplashScreen::update(LLTrans::getString("StartupDetectingHardware"));

		LL_DEBUGS("AppInit") << "Attempting to poll DXGI for vram info" << LL_ENDL;
		gDXHardware.setWriteDebugFunc(write_debug_dx);
		gDXHardware.updateVRAM();
		LL_DEBUGS("AppInit") << "Done polling DXGI for vram info" << LL_ENDL;

		// Disable so debugger can work
		std::string splash_msg;
		LLStringUtil::format_map_t args;
		args["[APP_NAME]"] = LLAppViewer::instance()->getSecondLifeTitle();
		splash_msg = LLTrans::getString("StartupLoading", args);

		LLSplashScreen::update(splash_msg);
	}

	if (!restoreErrorTrap())
	{
		LL_WARNS("AppInit") << " Someone took over my exception handler (post hardware probe)!" << LL_ENDL;
	}

	if (gGLManager.mVRAM == 0)
	{
		gGLManager.mVRAM = gDXHardware.getVRAM();
	}

	LL_INFOS("AppInit") << "Detected VRAM: " << gGLManager.mVRAM << LL_ENDL;

	return true;
}

bool LLAppViewerWin32::initParseCommandLine(LLCommandLineParser& clp)
{
	if (!clp.parseCommandLineString(mCmdLine))
	{
		return false;
	}

	// Find the system language.
	FL_Locale *locale = nullptr;
	FL_Success success = FL_FindLocale(&locale, FL_MESSAGES);
	if (success != 0)
	{
		if (success >= 2 && locale->lang) // confident!
		{
			LL_INFOS("AppInit") << "Language: " << ll_safe_string(locale->lang) << LL_ENDL;
			LL_INFOS("AppInit") << "Location: " << ll_safe_string(locale->country) << LL_ENDL;
			LL_INFOS("AppInit") << "Variant: " << ll_safe_string(locale->variant) << LL_ENDL;
			LLControlVariable* c = gSavedSettings.getControl("SystemLanguage");
			if(c)
			{
				c->setValue(std::string(locale->lang), false);
			}
		}
	}
	FL_FreeLocale(&locale);

	return true;
}

bool LLAppViewerWin32::beingDebugged()
{
    return IsDebuggerPresent();
}

bool LLAppViewerWin32::restoreErrorTrap()
{	
	return true;
}

void LLAppViewerWin32::initCrashReporting(bool reportFreeze)
{
#if defined(USE_CRASHPAD)
	// Path to the out-of-process handler executable
	std::string handler_path = gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "crashpad_handler.exe");
	if (!gDirUtilp->fileExists(handler_path))
	{
		LL_ERRS() << "Failed to initialize crashpad due to missing handler executable." << LL_ENDL;
		return;
	}
	base::FilePath handler(ll_convert_string_to_wide(handler_path));

	// Cache directory that will store crashpad information and minidumps
	std::string crashpad_path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "crashpad");
	base::FilePath database(ll_convert_string_to_wide(crashpad_path));

	// URL used to submit minidumps to
	std::string url(CRASHPAD_URL);

	// Optional annotations passed via --annotations to the handler
	std::map<std::string, std::string> annotations;

#if 0
	unsigned char node_id[6];
	if (LLUUID::getNodeID(node_id) > 0)
	{
		char md5str[MD5HEX_STR_SIZE] = { 0 };
		LLMD5 hashed_unique_id;
		hashed_unique_id.update(node_id, 6);
		hashed_unique_id.finalize();
		hashed_unique_id.hex_digest((char*)md5str);
		annotations.emplace("sentry[contexts][app][device_app_hash]", std::string(md5str));
	}
#endif

	annotations.emplace("sentry[contexts][app][app_name]", LLVersionInfo::getChannel());
	annotations.emplace("sentry[contexts][app][app_version]", LLVersionInfo::getVersion());
	annotations.emplace("sentry[contexts][app][app_build]", LLVersionInfo::getChannelAndVersion());

	annotations.emplace("sentry[release]", LLVersionInfo::getChannelAndVersion());

	annotations.emplace("sentry[tags][second_instance]", fmt::to_string(isSecondInstance()));
	annotations.emplace("sentry[tags][bitness]", fmt::to_string(ADDRESS_SIZE));


	// Optional arguments to pass to the handler
	std::vector<std::string> arguments;
	arguments.push_back("--no-rate-limit");
	arguments.push_back("--monitor-self");

	if (isSecondInstance())
	{
		arguments.push_back("--no-periodic-tasks");
	}
	else
	{
		auto db = crashpad::CrashReportDatabase::Initialize(database);
		if (db == nullptr)
		{
			LL_WARNS() << "Failed to initialize crashpad database at path: " << crashpad_path << LL_ENDL;
			return;
		}

		if (db->GetSettings() != nullptr)
		{
			if (!db->GetSettings()->SetUploadsEnabled(true))
			{
				LL_WARNS() << "Failed to enable upload of crash database." << LL_ENDL;
			}
		}

		auto prune_condition = crashpad::PruneCondition::GetDefault();
		if (prune_condition != nullptr)
		{
			auto ret = crashpad::PruneCrashReportDatabase(db.get(), prune_condition.get());
			LL_INFOS() << "Pruned " << ret << " reports from the crashpad database." << LL_ENDL;
		}
	}

	crashpad::CrashpadClient client;
	bool success = client.StartHandler(
		handler,
		database,
		database,
		url,
		annotations,
		arguments,
		/* restartable */ true,
		/* asynchronous_start */ false
	);
	if (success)
		LL_INFOS() << "Crashpad init success" << LL_ENDL;
	else
		LL_WARNS() << "FAILED TO INITIALIZE CRASHPAD" << LL_ENDL;

#endif
}

//virtual
bool LLAppViewerWin32::sendURLToOtherInstance(const std::string& url)
{
	wchar_t window_class[256]; /* Flawfinder: ignore */   // Assume max length < 255 chars.
	mbstowcs(window_class, sWindowClass.c_str(), 255);
	window_class[255] = 0;
	// Use the class instead of the window name.
	HWND other_window = FindWindow(window_class, nullptr);

	if (other_window != nullptr)
	{
		LL_DEBUGS() << "Found other window with the name '" << getWindowTitle() << "'" << LL_ENDL;
		COPYDATASTRUCT cds;
		const S32 SLURL_MESSAGE_TYPE = 0;
		cds.dwData = SLURL_MESSAGE_TYPE;
		cds.cbData = url.length() + 1;
		cds.lpData = (void*)url.c_str();

		LRESULT msg_result = SendMessage(other_window, WM_COPYDATA, NULL, (LPARAM)&cds);
		LL_DEBUGS() << "SendMessage(WM_COPYDATA) to other window '" 
				 << getWindowTitle() << "' returned " << msg_result << LL_ENDL;
		return true;
	}
	return false;
}


std::string LLAppViewerWin32::generateSerialNumber()
{
	char serial_md5[MD5HEX_STR_SIZE];		// Flawfinder: ignore
	serial_md5[0] = 0;

	DWORD serial = 0;
	DWORD flags = 0;
	BOOL success = GetVolumeInformation(
			TEXT("C:\\"),
			nullptr,		// volume name buffer
			0,			// volume name buffer size
			&serial,	// volume serial
			nullptr,		// max component length
			&flags,		// file system flags
			nullptr,		// file system name buffer
			0);			// file system name buffer size
	if (success)
	{
		LLMD5 md5;
		md5.update( (unsigned char*)&serial, sizeof(DWORD));
		md5.finalize();
		md5.hex_digest(serial_md5);
	}
	else
	{
		LL_WARNS() << "GetVolumeInformation failed" << LL_ENDL;
	}
	return serial_md5;
}
