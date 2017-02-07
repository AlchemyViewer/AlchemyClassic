// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llformat.cpp
 * @date   January 2007
 * @brief string formatting utility
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

#include "linden_common.h"

#include "llcrashlock.h"

#include "llapp.h"
#include "lldir.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llframetimer.h"
#include <boost/filesystem.hpp>  
#include <string>
#include <iostream>
#include <stdio.h>


#if LL_WINDOWS   //For windows platform.
#include <llwin32headers.h>
#include <TlHelp32.h>

bool LLCrashLock::isProcessAlive(U32 pid, const std::string& pname)
{
	std::wstring wpname = utf8str_to_utf16str(pname);

	HANDLE snapshot;
	PROCESSENTRY32 pe32;

	bool matched = false;

	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	else
	{
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(snapshot, &pe32))
		{
			do {
				std::wstring wexecname = pe32.szExeFile; 
				if (!wpname.compare(wexecname))
				{
					if (pid == (U32)pe32.th32ProcessID)
					{
						matched = true;
						break;
					}
				}
			} while (Process32Next(snapshot, &pe32));
		}
	}
	
	CloseHandle(snapshot);
	return matched;
}

#else   //Everyone Else
bool LLCrashLock::isProcessAlive(U32 pid, const std::string& pname)
{
    //Will boost.process ever become a reality? 
    std::stringstream cmd;
    
    cmd << "pgrep '" << pname << "' | grep '^" << pid << "$'";
    return (!system(cmd.str().c_str()));
}
#endif //Everyone else.


LLCrashLock::LLCrashLock() : mCleanUp(true), mWaitingPID(0)
{
}

void LLCrashLock::setCleanUp(bool cleanup)
{
    mCleanUp = cleanup;  //Allow cleanup to be disabled for debugging.
}  

LLSD LLCrashLock::getLockFile(std::string filename)
{
	LLSD lock_sd = LLSD::emptyMap();
    
	llifstream ifile(filename.c_str());
    
	if (ifile.is_open())
	{									            
        LLSDSerialize::fromXML(lock_sd, ifile);
		ifile.close();
	}

    return lock_sd;
}

bool LLCrashLock::putLockFile(std::string filename, const LLSD& data)
{    
    bool result = true;
    llofstream ofile(filename.c_str());
    
	if (!LLSDSerialize::toXML(data,ofile))
	{
        result=false;
	}
	ofile.close();
    return result;
}

bool LLCrashLock::requestMaster( F32 timeout )
{
	if (mMaster.empty())
	{
		    mMaster = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
                                            "crash_master.lock");
	}

    LLSD lock_sd=getLockFile(mMaster);
    
    if (lock_sd.has("pid"))
    {
        mWaitingPID = lock_sd["pid"].asInteger();
        if ( isProcessAlive(mWaitingPID, gDirUtilp->getExecutableFilename()) )
        {
            mTimer.resetWithExpiry(timeout);
            return false;
        }
    }
    
	S32 pid = LLApp::getPid();
    lock_sd["pid"] = (LLSD::Integer)pid;
    return putLockFile(mMaster,lock_sd);
}

bool LLCrashLock::checkMaster()
{
    if (mWaitingPID)
    {
        return (!isProcessAlive(mWaitingPID, gDirUtilp->getExecutableFilename()));
    }
    return  false;
}

bool LLCrashLock::isWaiting()
{
    return !mTimer.hasExpired();
}

void LLCrashLock::releaseMaster()
{
    //Yeeeeeeehaw
#if LL_WINDOWS
	_unlink(mMaster.c_str());
#else
	unlink(mMaster.c_str());
#endif
}

LLSD LLCrashLock::getProcessList()
{
    if (mDumpTable.empty())
	{
		mDumpTable= gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
			                                    "crash_table.lock");
	}
   return getLockFile(mDumpTable);
}

//static
bool LLCrashLock::fileExists(std::string filename)
{
#if LL_WINDOWS
	return boost::filesystem::exists(boost::filesystem::path(utf8str_to_utf16str(filename).c_str()));
#else
	return boost::filesystem::exists(boost::filesystem::path(filename.c_str()));
#endif
}

void LLCrashLock::cleanupProcess(std::string proc_dir)
{
#if LL_WINDOWS
	boost::filesystem::remove_all(boost::filesystem::path(utf8str_to_utf16str(proc_dir).c_str()));
#else
    boost::filesystem::remove_all(boost::filesystem::path(proc_dir));
#endif
}

bool LLCrashLock::putProcessList(const LLSD& proc_sd)
{
    return putLockFile(mDumpTable,proc_sd);
}
