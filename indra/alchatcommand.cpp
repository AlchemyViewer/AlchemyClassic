/** 
 * @file alchatcommand.cpp
 * @brief ALChatCommand implementation for chat input commands
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright (C) 2013 Drake Arconis
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alchatcommand.h"

// lib includes
#include "llstring.h"

// viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewercontrol.h"

void teleport_to_z(const F64 &z)
{
	LLVector3d pos_global = gAgent.getPositionGlobal();
	pos_global.mdV[VZ] = z;
	gAgent.teleportViaLocation(pos_global);
}

bool ALChatCommand::parseCommand(std::string data)
{
	static LLCachedControl<bool> enableChatCmd(gSavedSettings, "ALChatCommandEnable");
	if(enableChatCmd)
	{
		LLStringUtil::toLower(data);
		std::istringstream input(data);
		std::string cmd;

		if(!(input >> cmd))	return false;

		static LLCachedControl<std::string> sDrawDistanceCommand(gSavedSettings, "ALChatCommandDrawDistance");
		if(cmd == std::string(sDrawDistanceCommand)) // dd
		{
			S32 dist;
			if (input >> dist)
			{
				gSavedSettings.setF32("RenderFarClip", dist);
				gAgentCamera.mDrawDistance = dist;
				return true;
			}
		}
		
		static LLCachedControl<std::string> sHeightCommand(gSavedSettings, "ALChatCommandHeight");
		if (cmd == std::string(sHeightCommand)) // gth
		{
			F64 z;
			if (input >> z)
			{
				teleport_to_z(z);
				return true;
			}
		}

		static LLCachedControl<std::string> sGroundCommand(gSavedSettings, "ALChatCommandGround");
		if (cmd == std::string(sGroundCommand)) // flr
		{
			teleport_to_z(0.0);
			return true;
		}
	}
	return false;
}
