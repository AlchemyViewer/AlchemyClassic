// L'Oréal Féria - Multifasceted shimmering color. BUY HAIR COLOR NOW AT LOCAL PHARMANY! GO NOW!
/**
* @file alviewermenu.cpp
* @brief Builds menus out of items. Imagine the fast, easy, fun Alchemy style
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Copyright (C) 2013 Alchemy Developer Group
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
#include "alviewermenu.h"

// library
#include "llaudioengine.h"
#include "llavatarappearancedefines.h"
#include "llclipboard.h"
#include "llfloaterreg.h"
#include "llmenugl.h"
#include "llselectmgr.h"
#include "llsdserialize.h"
#include "llstreamingaudio.h"
#include "lltextureentry.h"
#include "lltrans.h"

// newview
#include "alavatarcolormgr.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llavataractions.h"
#include "llavatarpropertiesprocessor.h"
#include "llfloaterparticleeditor.h"
#include "llhudobject.h"
#include "lltexturecache.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewernetwork.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llvovolume.h"
#include "pipeline.h"
#include "roles_constants.h"

using namespace LLAvatarAppearanceDefines;

void handle_object_copy_key()
{
	LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
	if (!object) return;

	const LLWString id = utf8str_to_wstring(object->getID().asString());
	LLClipboard::instance().copyToClipboard(id, 0, id.length());
}

void al_handle_object_derender()
{
	LLSelectMgr* select_mgr = LLSelectMgr::getInstance();
	LLObjectSelectionHandle selection = select_mgr->getSelection();
	std::vector<LLViewerObject*> objects;
	for (LLObjectSelection::iterator iter = selection->begin(); iter != selection->end(); iter++)
	{
		LLSelectNode* nodep = *iter;
		if (!nodep)
		{
			continue;
		}

		LLViewerObject* selected_objectp = nodep->getObject();
		if (!selected_objectp)
		{
			continue;
		}

		objects.emplace_back(selected_objectp);
	}

	select_mgr->deselectAll();

	for (LLViewerObject* objectp : objects)
	{
		if (!objectp)
		{
			continue;
		}

		LLViewerRegion* regionp = objectp->getRegion();
		const LLUUID& id = objectp->getID();
		if (id.isNull() || id == gAgentID || !regionp)
		{
			continue;
		}

		// Display green bubble on kill
		if (gShowObjectUpdates)
		{
			gPipeline.addDebugBlip(objectp->getPositionAgent(), LLColor4(0.f, 1.f, 0.f, 1.f));
		}

		// Do the kill
		gObjectList.killObject(objectp);

		if (LLViewerRegion::sVOCacheCullingEnabled)
		{
			regionp->killCacheEntry(objectp->getLocalID());
		}
	}
}

class LLEnableEditParticleSource : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
		for (LLObjectSelection::valid_root_iterator iter = selection->valid_root_begin();
			iter != selection->valid_root_end(); iter++)
		{
			LLSelectNode* node = *iter;
			if (node->mPermissions->getOwner() == gAgent.getID())
			{
				return true;
			}
		}
		return false;
	}
};

class LLEditParticleSource : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
		if (objectp)
		{
			LLFloaterParticleEditor* particleEditor = LLFloaterReg::showTypedInstance<LLFloaterParticleEditor>("particle_editor", LLSD(objectp->getID()), TAKE_FOCUS_YES);
			if (particleEditor)
				particleEditor->setObject(objectp);
		}
		return true;
	}
};

class LLSpawnDebugSimFeatures : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		if (LLViewerRegion* regionp = gAgent.getRegion())
		{
			LLSD sim_features, args;
			std::stringstream features_str;
			regionp->getSimulatorFeatures(sim_features);
			LLSDSerialize::toPrettyXML(sim_features, features_str);
			args["title"] = llformat("%s - %s", LLTrans::getString("SimulatorFeaturesTitle").c_str(), regionp->getName().c_str());
			args["data"] = features_str.str();
			LLFloaterReg::showInstance("generic_text", args);
		}
		return true;
	}
};

class LLSyncAnimations : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		for (S32 i = 0; i < gObjectList.getNumObjects(); ++i)
		{
			LLViewerObject* object = gObjectList.getObject(i);
			if (object &&
				object->isAvatar())
			{
				LLVOAvatar* avatarp = static_cast<LLVOAvatar*>(object);
				if (avatarp)
				{
					for (const auto& playpair : avatarp->mPlayingAnimations)
					{
						avatarp->stopMotion(playpair.first, TRUE);
						avatarp->startMotion(playpair.first);
					}
				}
			}
		}
		return true;
	}
};

class ALMarkViewerEffectsDead : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		LLHUDObject::markViewerEffectsDead();
		return true;
	}
};

class ALToggleLocationBar : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		const U32 val = userdata.asInteger();
		gSavedSettings.setU32("NavigationBarStyle", val);
		return true;
	}
};

class ALCheckLocationBar : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		return userdata.asInteger() == gSavedSettings.getU32("NavigationBarStyle");
	}
};

void destroy_texture(const LLUUID& id)
{
	if (id.isNull() || id == IMG_DEFAULT) return;
	LLViewerFetchedTexture* texture = LLViewerTextureManager::getFetchedTexture(id);
	if (texture)
		texture->clearFetchedResults();
	LLAppViewer::getTextureCache()->removeFromCache(id);
}

class LLRefreshTexturesObject : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		for (LLObjectSelection::valid_iterator iter = LLSelectMgr::getInstance()->getSelection()->valid_begin();
			iter != LLSelectMgr::getInstance()->getSelection()->valid_end();
			++iter)
		{
			LLSelectNode* node = *iter;
			if (!node) continue;
			std::map< LLUUID, std::vector<U8> > faces_per_tex;
			for (U8 i = 0; i < node->getObject()->getNumTEs(); ++i)
			{
				if (!node->isTESelected(i)) continue;
				LLViewerTexture* img = node->getObject()->getTEImage(i);
				faces_per_tex[img->getID()].push_back(i);

				if (node->getObject()->getTE(i)->getMaterialParams().notNull())
				{
					LLViewerTexture* norm_img = node->getObject()->getTENormalMap(i);
					faces_per_tex[norm_img->getID()].push_back(i);
					LLViewerTexture* spec_img = node->getObject()->getTESpecularMap(i);
					faces_per_tex[spec_img->getID()].push_back(i);
				}
			}

			for (auto it : faces_per_tex)
			{
				destroy_texture(it.first);
			}

			if (node->getObject()->isSculpted() && !node->getObject()->isMesh())
			{
				LLSculptParams* sculpt_params = dynamic_cast<LLSculptParams*>(node->getObject()->getParameterEntry(LLNetworkData::PARAMS_SCULPT));
				if (sculpt_params)
				{
					LLUUID sculptie = sculpt_params->getSculptTexture();
					LLViewerFetchedTexture* tx = LLViewerTextureManager::getFetchedTexture(sculptie);
					if (tx)
					{
						const LLViewerTexture::ll_volume_list_t* pVolumeList = tx->getVolumeList();
						destroy_texture(sculptie);
						for (S32 idxVolume = 0; idxVolume < tx->getNumVolumes(); ++idxVolume)
						{
							LLVOVolume* pVolume = pVolumeList->at(idxVolume);
							if (pVolume) pVolume->notifyMeshLoaded();
						}
					}
				}
			}
		}
		return true;
	}
};

class LLRefreshTexturesAvatar : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		LLVOAvatar* avatar = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
		if (!avatar) return true;

		for (U32 baked_idx = 0; baked_idx < BAKED_NUM_INDICES; ++baked_idx)
		{
			ETextureIndex te_idx = LLAvatarAppearanceDictionary::bakedToLocalTextureIndex((EBakedTextureIndex)baked_idx);
			destroy_texture(avatar->getTE(te_idx)->getID());
		}
		LLAvatarPropertiesProcessor::getInstance()->sendAvatarTexturesRequest(avatar->getID());

		// *TODO: We want to refresh their attachments too!

		return true;
	}
};

class LLObjectExplode : public view_listener_t
{
	bool handleEvent(LLSD const& userdata) override
	{
		auto* sel_man = LLSelectMgr::getInstance();
		LLViewerObject *objectp = sel_man->getSelection()->getFirstRootObject();
		if (objectp == nullptr) return false;

		sel_man->selectionUpdateTemporary(TRUE);
		sel_man->selectionUpdatePhysics(TRUE);
		sel_man->sendDelink();
		sel_man->deselectAll();
		return true;
	}
};

bool enable_object_explode()
{
	bool enable = LLSelectMgr::getInstance()->selectGetModify();
	if (enable)
	{
		LLViewerObject *objectp = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
		enable = objectp && !objectp->isAttachment();
	}
	return enable;
}

class LLUndeformSelf : public view_listener_t
{
	bool handleEvent(LLSD const& userdata) override
	{
		if (!isAgentAvatarValid()) return true;

		gAgentAvatarp->resetSkeleton(true);
		LLMessageSystem *msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_AgentAnimation);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_AnimationList);
		msg->addUUIDFast(_PREHASH_AnimID, LLUUID("e5afcabe-1601-934b-7e89-b0c78cac373a"));
		msg->addBOOLFast(_PREHASH_StartAnim, TRUE);
		msg->nextBlockFast(_PREHASH_AnimationList);
		msg->addUUIDFast(_PREHASH_AnimID, LLUUID("d307c056-636e-dda6-4a3c-b3a43c431ca8"));
		msg->addBOOLFast(_PREHASH_StartAnim, TRUE);
		msg->nextBlockFast(_PREHASH_AnimationList);
		msg->addUUIDFast(_PREHASH_AnimID, LLUUID("319b4e7a-18fc-1f9e-6411-dd10326c0c7e"));
		msg->addBOOLFast(_PREHASH_StartAnim, TRUE);
		msg->nextBlockFast(_PREHASH_AnimationList);
		msg->addUUIDFast(_PREHASH_AnimID, LLUUID("f05d765d-0e01-5f9a-bfc2-fdc054757e55"));
		msg->addBOOLFast(_PREHASH_StartAnim, TRUE);
		msg->nextBlockFast(_PREHASH_PhysicalAvatarEventList);
		msg->addBinaryDataFast(_PREHASH_TypeData, nullptr, 0);
		msg->sendReliable(gAgent.getRegion()->getHost());
		return true;
	}
};

class LLEnableGrid : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		const std::string& grid_type = userdata.asString();
		if (grid_type == "secondlife")
		{
			return LLGridManager::getInstance()->isInSecondlife();
		}
		else if (grid_type == "opensim")
		{
			return LLGridManager::getInstance()->isInOpenSim();
		}
		else if (grid_type == "halcyon")
		{
			return LLGridManager::getInstance()->isInHalcyon();
		}
		else
		{
			LL_WARNS("ViewerMenu") << "Unhandled or bad on_visible gridcheck parameter!" << LL_ENDL;
		}
		return true;
	}
};

bool enable_estate_management(const LLSD& avatar_id)
{
	// Use avatar_id if available, otherwise default to right-click avatar
	LLVOAvatar* avatar = nullptr;
	if (avatar_id.asUUID().notNull())
	{
		avatar = find_avatar_from_object(avatar_id.asUUID());
	}
	else
	{
		avatar = find_avatar_from_object(
			LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
	}
	if (!avatar)
		return false;

	// Gods can always manage estates
	if (gAgent.isGodlike())
		return true;

	// Estate owners / managers can freeze
	// Parcel owners can also freeze
	auto region = avatar->getRegion();
	if (!region)
		return false;

	return (region->getOwner() == gAgentID || region->canManageEstate());
}

class LLAvatarCopyData : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
		if (avatarp)
		{
			const std::string& param = userdata.asString();
			if (param == "copy_name")
			{
				LLAvatarActions::copyData(avatarp->getID(), LLAvatarActions::E_DATA_NAME);
				return true;
			}
			else if (param == "copy_slurl")
			{
				LLAvatarActions::copyData(avatarp->getID(), LLAvatarActions::E_DATA_SLURL);
				return true;
			}
			else if (param == "copy_key")
			{
				LLAvatarActions::copyData(avatarp->getID(), LLAvatarActions::E_DATA_UUID);
				return true;
			}
		}
		return false;
	}
};

class ALAvatarColorize : public view_listener_t
{
	bool handleEvent(const LLSD& userdata) override
	{
		LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
		if (avatarp)
		{
			const std::string& param = userdata.asString();
			if (param == "color1")
			{
				ALAvatarColorMgr::instance().addOrUpdateCustomColor(avatarp->getID(), ALAvatarColorMgr::E_FIRST_COLOR);
			}
			else if (param == "color2")
			{
				ALAvatarColorMgr::instance().addOrUpdateCustomColor(avatarp->getID(), ALAvatarColorMgr::E_SECOND_COLOR);
			}
			else if (param == "color3")
			{
				ALAvatarColorMgr::instance().addOrUpdateCustomColor(avatarp->getID(), ALAvatarColorMgr::E_THIRD_COLOR);
			}
			else if (param == "color4")
			{
				ALAvatarColorMgr::instance().addOrUpdateCustomColor(avatarp->getID(), ALAvatarColorMgr::E_FOURTH_COLOR);
			}
			else if (param == "clear")
			{
				ALAvatarColorMgr::instance().clearCustomColor(avatarp->getID());
			}
		}
		return false;
	}
};

bool enable_music_ticker()
{
	return gAudiop
		&& gAudiop->getStreamingAudioImpl() 
		&& gAudiop->getStreamingAudioImpl()->supportsMetaData();
}

////////////////////////////////////////////////////////
// Menu registry

void ALViewerMenu::initialize_menus()
{
	LLUICtrl::EnableCallbackRegistry::Registrar& enable = LLUICtrl::EnableCallbackRegistry::currentRegistrar();
	LLUICtrl::CommitCallbackRegistry::Registrar& commit = LLUICtrl::CommitCallbackRegistry::currentRegistrar();

	enable.add("Avatar.EnableEstateManage", boost::bind(&enable_estate_management, _2));
	enable.add("EnableMusicTicker", boost::bind(&enable_music_ticker));
	enable.add("Object.EnableExplode", std::bind(&enable_object_explode));

	commit.add("Object.CopyKey", boost::bind(&handle_object_copy_key));
	commit.add("Alchemy.Derender", boost::bind(&al_handle_object_derender));


	view_listener_t::addMenu(new ALAvatarColorize(), "Avatar.Colorize");
	view_listener_t::addMenu(new LLAvatarCopyData(), "Avatar.CopyData");
	view_listener_t::addMenu(new LLRefreshTexturesObject(), "Object.RefreshTex");
	view_listener_t::addMenu(new LLRefreshTexturesAvatar(), "Avatar.RefreshTex");
	view_listener_t::addMenu(new LLEditParticleSource(), "Object.EditParticles");
	view_listener_t::addMenu(new LLEnableEditParticleSource(), "Object.EnableEditParticles");
	view_listener_t::addMenu(new LLObjectExplode(), "Object.Explode");
	
	view_listener_t::addMenu(new LLSpawnDebugSimFeatures(), "Advanced.DebugSimFeatures");
	view_listener_t::addMenu(new LLSyncAnimations(), "Tools.ResyncAnimations");
	view_listener_t::addMenu(new LLUndeformSelf(), "Tools.UndeformSelf");
	view_listener_t::addMenu(new ALMarkViewerEffectsDead(), "Tools.AllVEDead");
	view_listener_t::addMenu(new ALToggleLocationBar(), "ToggleLocationBar");
	view_listener_t::addMenu(new ALCheckLocationBar(), "CheckLocationBar");
	view_listener_t::addEnable(new LLEnableGrid(), "EnableGrid");
}
