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
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

// lib includes
#include "llcalc.h"
#include "llstring.h"
#include "material_codes.h"
#include "object_flags.h"

// viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llcommandhandler.h"
#include "llfloaterreg.h"
#include "llfloaterimnearbychat.h"
#include "llstartup.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llvolume.h"
#include "llvolumemessage.h"

void add_system_chat(const std::string &msg)
{
	if (msg.empty()) return;

	LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::findTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
	if (nearby_chat)
	{
		LLChat chat(msg);
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		nearby_chat->addMessage(chat);
	}
}

bool ALChatCommand::parseCommand(std::string data)
{
	static LLCachedControl<bool> enableChatCmd(gSavedSettings, "AlchemyChatCommandEnable", true);
	if (enableChatCmd)
	{
		utf8str_tolower(data);
		std::istringstream input(data);
		std::string cmd;

		if (!(input >> cmd))	return false;

		static LLCachedControl<std::string> sDrawDistanceCommand(gSavedSettings, "AlchemyChatCommandDrawDistance", "/dd");
		static LLCachedControl<std::string> sHeightCommand(gSavedSettings, "AlchemyChatCommandHeight", "/gth");
		static LLCachedControl<std::string> sGroundCommand(gSavedSettings, "AlchemyChatCommandGround", "/flr");
		static LLCachedControl<std::string> sPosCommand(gSavedSettings, "AlchemyChatCommandPos", "/pos");
		static LLCachedControl<std::string> sRezPlatCommand(gSavedSettings, "AlchemyChatCommandRezPlat", "/plat");
		static LLCachedControl<std::string> sHomeCommand(gSavedSettings, "AlchemyChatCommandHome", "/home");
		static LLCachedControl<std::string> sSetHomeCommand(gSavedSettings, "AlchemyChatCommandSetHome", "/sethome");
		static LLCachedControl<std::string> sCalcCommand(gSavedSettings, "AlchemyChatCommandCalc", "/calc");
		static LLCachedControl<std::string> sMaptoCommand(gSavedSettings, "AlchemyChatCommandMapto", "/mapto");
		static LLCachedControl<std::string> sClearCommand(gSavedSettings, "AlchemyChatCommandClearNearby", "/clr");

		if (cmd == utf8str_tolower(sDrawDistanceCommand)) // dd
		{
			F32 dist;
			if (input >> dist)
			{
				gSavedSettings.setF32("RenderFarClip", dist);
				gAgentCamera.mDrawDistance = dist;
				return true;
			}
		}
		else if (cmd == utf8str_tolower(sHeightCommand)) // gth
		{
			F64 z;
			if (input >> z)
			{
				LLVector3d pos_global = gAgent.getPositionGlobal();
				pos_global.mdV[VZ] = z;
				gAgent.teleportViaLocation(pos_global);
				return true;
			}
		}
		else if (cmd == utf8str_tolower(sGroundCommand)) // flr
		{
			LLVector3d pos_global = gAgent.getPositionGlobal();
			pos_global.mdV[VZ] = 0.0;
			gAgent.teleportViaLocation(pos_global);
			return true;
		}
		else if (cmd == utf8str_tolower(sPosCommand)) // pos
		{
			F64 x, y, z;
			if ((input >> x) && (input >> y) && (input >> z))
			{
				LLViewerRegion* regionp = gAgent.getRegion();
				if (regionp)
				{
					LLVector3d target_pos = regionp->getPosGlobalFromRegion(LLVector3((F32) x, (F32) y, (F32) z));
					gAgent.teleportViaLocation(target_pos);
				}
				return true;
			}
		}
		else if (cmd == utf8str_tolower(sRezPlatCommand)) // plat
		{
			F32 size;
			static LLCachedControl<F32> platSize(gSavedSettings, "AlchemyChatCommandRezPlatSize");
			if (!(input >> size))
				size = static_cast<F32>(platSize);

			const LLVector3& agent_pos = gAgent.getPositionAgent();
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
			msg->addU32Fast(_PREHASH_AddFlags, agent_pos.mV[VZ] > 4096.f ? FLAGS_CREATE_SELECTED : 0U);

			LLVolumeParams volume_params;
			volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
			volume_params.setBeginAndEndS(0.f, 1.f);
			volume_params.setBeginAndEndT(0.f, 1.f);
			volume_params.setRatio(1.f, 1.f);
			volume_params.setShear(0.f, 0.f);
			LLVolumeMessage::packVolumeParams(&volume_params, msg);

			msg->addVector3Fast(_PREHASH_Scale, LLVector3(size, size, 0.25f));
			msg->addQuatFast(_PREHASH_Rotation, LLQuaternion());
			msg->addVector3Fast(_PREHASH_RayStart, rez_pos);
			msg->addVector3Fast(_PREHASH_RayEnd, rez_pos);
			msg->addUUIDFast(_PREHASH_RayTargetID, LLUUID::null);
			msg->addU8Fast(_PREHASH_BypassRaycast, TRUE);
			msg->addU8Fast(_PREHASH_RayEndIsIntersection, FALSE);
			msg->addU8Fast(_PREHASH_State, FALSE);
			msg->sendReliable(gAgent.getRegionHost());

			return true;
		}
		else if (cmd == utf8str_tolower(sHomeCommand)) // home
		{
			gAgent.teleportHome();
			return true;
		}
		else if (cmd == utf8str_tolower(sSetHomeCommand)) // sethome
		{
			gAgent.setStartPosition(START_LOCATION_ID_HOME);
			return true;
		}
		else if (cmd == utf8str_tolower(sCalcCommand)) // calc
		{
			if (data.length() > cmd.length() + 1)
			{
				F32 result = 0.f;
				std::string expr = data.substr(cmd.length() + 1);
				LLStringUtil::toUpper(expr);
				const bool success = LLCalc::getInstance()->evalString(expr, result);
				std::string out = LLTrans::getString("ALChatCalculationFailure");
				if (success)
				{
					out = (boost::format("%1%: %2% = %3%") % LLTrans::getString("ALChatCalculation") % expr % result).str();
				}
				add_system_chat(out);
				return true;
			}
		}
		else if (cmd == utf8str_tolower(sMaptoCommand)) // mapto
		{
			const std::string::size_type length = cmd.length() + 1;
			if (data.length() > length)
			{
				const LLVector3d& pos = gAgent.getPositionGlobal();
				LLSD params;
				params.append(data.substr(length));
				params.append(fmodf(static_cast<F32>(pos.mdV[VX]), REGION_WIDTH_METERS));
				params.append(fmodf(static_cast<F32>(pos.mdV[VY]), REGION_WIDTH_METERS));
				params.append(fmodf(static_cast<F32>(pos.mdV[VZ]), REGION_HEIGHT_METERS));
				LLCommandDispatcher::dispatch("teleport", params, LLSD(), NULL, "clicked", true);
				return true;
			}
		}
		else if (cmd == utf8str_tolower(sClearCommand))
		{
			LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::findTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
			if (nearby_chat)
			{
				nearby_chat->reloadMessages(true);
			}
			return true;
		}
	}
	return false;
}
