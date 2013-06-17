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
 **/

#include "llviewerprecompiledheaders.h"

#include "alchatcommand.h"

// system includes
#include <boost/lexical_cast.hpp>

// lib includes
#include "llcalc.h"
#include "llstring.h"
#include "material_codes.h"
#include "object_flags.h"

// viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llfloaterreg.h"
#include "llfloaterimnearbychat.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"
#include "llvoavatarself.h"
#include "llvolume.h"
#include "llvolumemessage.h"

void teleport_to_z(const F64 &z)
{
	LLVector3d pos_global = gAgent.getPositionGlobal();
	pos_global.mdV[VZ] = z;
	gAgent.teleportViaLocation(pos_global);
}

void add_system_chat(const std::string &msg)
{
	if(msg.empty()) return;

	LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::findTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
	if(nearby_chat)
	{		
		LLChat chat(msg);
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		nearby_chat->addMessage(chat);
	}
}

bool ALChatCommand::parseCommand(std::string data)
{
	static LLCachedControl<bool> enableChatCmd(gSavedSettings, "AlchemyChatCommandEnable");
	if(enableChatCmd)
	{
		LLStringUtil::toLower(data);
		std::istringstream input(data);
		std::string cmd;

		if(!(input >> cmd))	return false;

		static LLCachedControl<std::string> sDrawDistanceCommand(gSavedSettings, "AlchemyChatCommandDrawDistance");
		static LLCachedControl<std::string> sHeightCommand(gSavedSettings, "AlchemyChatCommandHeight");
		static LLCachedControl<std::string> sGroundCommand(gSavedSettings, "AlchemyChatCommandGround");
		static LLCachedControl<std::string> sRezPlatCommand(gSavedSettings, "AlchemyChatCommandRezPlat");
		static LLCachedControl<std::string> sHomeCommand(gSavedSettings, "AlchemyChatCommandHome");

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
		else if (cmd == std::string(sHeightCommand)) // gth
		{
			F64 z;
			if (input >> z)
			{
				teleport_to_z(z);
				return true;
			}
		}
		else if (cmd == std::string(sGroundCommand)) // flr
		{
			teleport_to_z(0.0);
			return true;
		}
		else if(cmd == std::string(sRezPlatCommand))
		{
			F32 size;
			static LLCachedControl<F32> platSize(gSavedSettings, "AlchemyChatCommandRezPlatSize");
			if (!(input >> size)) 
				size = (F32)platSize;

			const LLVector3 agent_pos = gAgent.getPositionAgent();
			const LLVector3 rez_pos(agent_pos.mV[VX], agent_pos.mV[VY], agent_pos.mV[VZ] - ((gAgentAvatarp->getScale().mV[VZ] / 2.f) + 0.25f + (gAgent.getVelocity().magVec() * 0.333f)));

			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ObjectAdd);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->addUUIDFast(_PREHASH_GroupID, gAgent.getGroupID());
			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addU8Fast(_PREHASH_PCode, LL_PCODE_VOLUME);
			msg->addU8Fast(_PREHASH_Material, LL_MCODE_STONE);
			msg->addU32Fast(_PREHASH_AddFlags, agent_pos.mV[2] > 4096.f ? FLAGS_CREATE_SELECTED : 0U);

			LLVolumeParams volume_params;
			volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
			volume_params.setBeginAndEndS(0.f, 1.f);
			volume_params.setBeginAndEndT(0.f, 1.f);
			volume_params.setRatio(1, 1);
			volume_params.setShear(0, 0);
			LLVolumeMessage::packVolumeParams(&volume_params, msg);

			msg->addVector3Fast(_PREHASH_Scale, LLVector3(size, size, 0.25f));
			msg->addQuatFast(_PREHASH_Rotation,     LLQuaternion());
			msg->addVector3Fast(_PREHASH_RayStart, rez_pos);
			msg->addVector3Fast(_PREHASH_RayEnd, rez_pos);
			msg->addUUIDFast(_PREHASH_RayTargetID, LLUUID::null);
			msg->addU8Fast(_PREHASH_BypassRaycast, TRUE);
			msg->addU8Fast(_PREHASH_RayEndIsIntersection, FALSE);
			msg->addU8Fast(_PREHASH_State, FALSE);
			msg->sendReliable(gAgent.getRegionHost());

			return true;
		}
		else if (cmd == std::string(sHomeCommand))
		{
			gAgent.teleportHome();
			return true;
		}
	}
	return false;
}
