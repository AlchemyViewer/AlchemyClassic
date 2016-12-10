// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llupdateinstaller.cpp
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

#include "linden_common.h"
#include "llprocess.h"
#include "llupdateinstaller.h"
#include "lldir.h" 
#include "llsd.h"
#include "llexception.h"

namespace {
	struct RelocateError: public LLException
	{
		RelocateError(): LLException("llupdateinstaller: RelocateError") {}
	};

	std::string copy_to_temp(std::string const & path)
	{
		std::string scriptFile = gDirUtilp->getBaseFileName(path);
		std::string newPath = gDirUtilp->getExpandedFilename(LL_PATH_TEMP, scriptFile);
		if(!LLFile::copy(path, newPath)) LLTHROW(RelocateError());
		
		return newPath;
	}
}


int ll_install_update(std::string const & script,
					  std::string const & updatePath,
					  std::string const & updateChannel,
					  std::string const & viewerChannel,
					  bool required,
					  LLInstallScriptMode mode)
{
	std::string actualScriptPath;
	switch(mode) {
		case LL_COPY_INSTALL_SCRIPT_TO_TEMP:
			try {
				actualScriptPath = copy_to_temp(script);
			}
			catch (const RelocateError &) {
				return -1;
			}
			break;
		case LL_RUN_INSTALL_SCRIPT_IN_PLACE:
			actualScriptPath = script;
			break;
		default:
			llassert(!"unpossible copy mode");
	}
	
	LL_INFOS("Updater") << "UpdateInstaller: installing " << updatePath << " using " <<
		actualScriptPath << LL_ENDL;
	
	LLProcess::Params params;
	params.executable = actualScriptPath;
	params.args.add(updatePath);
	params.args.add(ll_install_failed_marker_path());
	params.args.add(std::to_string(required));
#if LL_WINDOWS
	if (updateChannel != viewerChannel)
	{
		params.args.add("/UPDATE");
		params.args.add("/OLDCHANNEL");
		std::string viewer_channel = viewerChannel;
		viewer_channel.erase(std::remove_if(viewer_channel.begin(), viewer_channel.end(), isspace), viewer_channel.end());
		params.args.add(viewer_channel);
	}
#endif
	params.autokill = false;
	return LLProcess::create(params)? 0 : -1;
}


std::string const & ll_install_failed_marker_path(void)
{
	static std::string path;
	if(path.empty()) {
		path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "AlchemyInstallFailed.marker");
	}
	return path;
}
