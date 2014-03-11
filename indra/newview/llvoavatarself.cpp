/** 
 * @file llvoavatar.cpp
 * @brief Implementation of LLVOAvatar class which is a derivation fo LLViewerObject
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

#if LL_MSVC
// disable warning about boost::lexical_cast returning uninitialized data
// when it fails to parse the string
#pragma warning (disable:4701)
#endif

#include "llviewerprecompiledheaders.h"

#include "llvoavatarself.h"
#include "llvoavatar.h"

#include "pipeline.h"

#include "llagent.h" //  Get state values from here
#include "llagentcamera.h"
#include "llagentwearables.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llinventoryfunctions.h"
#include "llnotificationsutil.h"
#include "llselectmgr.h"
#include "lltoolgrab.h"	// for needsRenderBeam
#include "lltoolmgr.h" // for needsRenderBeam
#include "lltoolmorph.h"
#include "lltrans.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerstats.h"
#include "llviewerregion.h"
#include "llviewertexlayer.h"
#include "llviewerwearable.h"
#include "llappearancemgr.h"
#include "llmeshrepository.h"
#include "llvovolume.h"
#include "llsdutil.h"
#include "llstartup.h"
#include "llsdserialize.h"

#if LL_MSVC
// disable boost::lexical_cast warning
#pragma warning (disable:4702)
#endif

#include <boost/lexical_cast.hpp>

LLPointer<LLVOAvatarSelf> gAgentAvatarp = NULL;

BOOL isAgentAvatarValid()
{
	return (gAgentAvatarp.notNull() && gAgentAvatarp->isValid());
}

void selfStartPhase(const std::string& phase_name)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->startPhase(phase_name);
	}
}

void selfStopPhase(const std::string& phase_name, bool err_check)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->stopPhase(phase_name, err_check);
	}
}

void selfClearPhases()
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->clearPhases();
	}
}

using namespace LLAvatarAppearanceDefines;

/*********************************************************************************
 **                                                                             **
 ** Begin private LLVOAvatarSelf Support classes
 **
 **/

struct LocalTextureData
{
	LocalTextureData() : 
		mIsBakedReady(false), 
		mDiscard(MAX_DISCARD_LEVEL+1), 
		mImage(NULL), 
		mWearableID(IMG_DEFAULT_AVATAR),
		mTexEntry(NULL)
	{}
	LLPointer<LLViewerFetchedTexture> mImage;
	bool mIsBakedReady;
	S32 mDiscard;
	LLUUID mWearableID;	// UUID of the wearable that this texture belongs to, not of the image itself
	LLTextureEntry *mTexEntry;
};

//-----------------------------------------------------------------------------
// Callback data
//-----------------------------------------------------------------------------


/**
 **
 ** End LLVOAvatarSelf Support classes
 **                                                                             **
 *********************************************************************************/


//-----------------------------------------------------------------------------
// Static Data
//-----------------------------------------------------------------------------
S32 LLVOAvatarSelf::sScratchTexBytes = 0;
LLMap< LLGLenum, LLGLuint*> LLVOAvatarSelf::sScratchTexNames;
LLMap< LLGLenum, F32*> LLVOAvatarSelf::sScratchTexLastBindTime;


/*********************************************************************************
 **                                                                             **
 ** Begin LLVOAvatarSelf Constructor routines
 **
 **/

LLVOAvatarSelf::LLVOAvatarSelf(const LLUUID& id,
							   const LLPCode pcode,
							   LLViewerRegion* regionp) :
	LLVOAvatar(id, pcode, regionp),
	mScreenp(NULL),
	mLastRegionHandle(0),
	mRegionCrossingCount(0),
	mInitialBakesLoaded(false)
{
	gAgentWearables.setAvatarObject(this);

	mMotionController.mIsSelf = TRUE;

	lldebugs << "Marking avatar as self " << id << llendl;
}

// Called periodically for diagnostics, return true when done.
bool output_self_av_texture_diagnostics()
{
	if (!isAgentAvatarValid())
		return true; // done checking

	gAgentAvatarp->outputRezDiagnostics();

	return false;
}

bool update_avatar_rez_metrics()
{
	if (!isAgentAvatarValid())
		return true;
	
	gAgentAvatarp->updateAvatarRezMetrics(false);
	return false;
}

bool check_for_unsupported_baked_appearance()
{
	if (!isAgentAvatarValid())
		return true;

	gAgentAvatarp->checkForUnsupportedServerBakeAppearance();
	return false;
}

void LLVOAvatarSelf::initInstance()
{
	BOOL status = TRUE;
	// creates hud joint(mScreen) among other things
	status &= loadAvatarSelf();

	// adds attachment points to mScreen among other things
	LLVOAvatar::initInstance();

	llinfos << "Self avatar object created. Starting timer." << llendl;
	mDebugSelfLoadTimer.reset();
	// clear all times to -1 for debugging
	for (U32 i =0; i < LLAvatarAppearanceDefines::TEX_NUM_INDICES; ++i)
	{
		for (U32 j = 0; j <= MAX_DISCARD_LEVEL; ++j)
		{
			mDebugTextureLoadTimes[i][j] = -1.0f;
		}
	}

	for (U32 i =0; i < LLAvatarAppearanceDefines::BAKED_NUM_INDICES; ++i)
	{
		mDebugBakedTextureTimes[i][0] = -1.0f;
		mDebugBakedTextureTimes[i][1] = -1.0f;
		mInitialBakeIDs[i] = LLUUID::null;
	}

	status &= buildMenus();
	if (!status)
	{
		llerrs << "Unable to load user's avatar" << llendl;
		return;
	}

	//doPeriodically(output_self_av_texture_diagnostics, 30.0);
	doPeriodically(update_avatar_rez_metrics, 5.0);
	doPeriodically(check_for_unsupported_baked_appearance, 120.0);
}

// virtual
void LLVOAvatarSelf::markDead()
{
	for(int i = 0; i < 8; i++) mBeam[i] = NULL;
	LLVOAvatar::markDead();
}

/*virtual*/ BOOL LLVOAvatarSelf::loadAvatar()
{
	BOOL success = LLVOAvatar::loadAvatar();

	// set all parameters sotred directly in the avatar to have
	// the isSelfParam to be TRUE - this is used to prevent
	// them from being animated or trigger accidental rebakes
	// when we copy params from the wearable to the base avatar.
	for (LLViewerVisualParam* param = (LLViewerVisualParam*) getFirstVisualParam(); 
		 param;
		 param = (LLViewerVisualParam*) getNextVisualParam())
	{
		if (param->getWearableType() != LLWearableType::WT_INVALID)
		{
			param->setIsDummy(TRUE);
		}
	}

	return success;
}


BOOL LLVOAvatarSelf::loadAvatarSelf()
{
	BOOL success = TRUE;
	// avatar_skeleton.xml
	if (!buildSkeletonSelf(sAvatarSkeletonInfo))
	{
		llwarns << "avatar file: buildSkeleton() failed" << llendl;
		return FALSE;
	}

	return success;
}

BOOL LLVOAvatarSelf::buildSkeletonSelf(const LLAvatarSkeletonInfo *info)
{
	// add special-purpose "screen" joint
	mScreenp = new LLViewerJoint("mScreen", NULL);
	// for now, put screen at origin, as it is only used during special
	// HUD rendering mode
	F32 aspect = LLViewerCamera::getInstance()->getAspect();
	LLVector3 scale(1.f, aspect, 1.f);
	mScreenp->setScale(scale);
	mScreenp->setWorldPosition(LLVector3::zero);
	// need to update screen agressively when sidebar opens/closes, for example
	mScreenp->mUpdateXform = TRUE;
	return TRUE;
}

BOOL LLVOAvatarSelf::buildMenus()
{
	//-------------------------------------------------------------------------
	// build the attach and detach menus
	//-------------------------------------------------------------------------
	gAttachBodyPartPieMenus[0] = NULL;

	LLContextMenu::Params params;
	params.label(LLTrans::getString("BodyPartsRightArm"));
	params.name(params.label);
	params.visible(false);
	gAttachBodyPartPieMenus[1] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsHead"));
	params.name(params.label);
	gAttachBodyPartPieMenus[2] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsLeftArm"));
	params.name(params.label);
	gAttachBodyPartPieMenus[3] = LLUICtrlFactory::create<LLContextMenu> (params);

	gAttachBodyPartPieMenus[4] = NULL;

	params.label(LLTrans::getString("BodyPartsLeftLeg"));
	params.name(params.label);
	gAttachBodyPartPieMenus[5] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsTorso"));
	params.name(params.label);
	gAttachBodyPartPieMenus[6] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsRightLeg"));
	params.name(params.label);
	gAttachBodyPartPieMenus[7] = LLUICtrlFactory::create<LLContextMenu> (params);

	gDetachBodyPartPieMenus[0] = NULL;

	params.label(LLTrans::getString("BodyPartsRightArm"));
	params.name(params.label);
	gDetachBodyPartPieMenus[1] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsHead"));
	params.name(params.label);
	gDetachBodyPartPieMenus[2] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsLeftArm"));
	params.name(params.label);
	gDetachBodyPartPieMenus[3] = LLUICtrlFactory::create<LLContextMenu> (params);

	gDetachBodyPartPieMenus[4] = NULL;

	params.label(LLTrans::getString("BodyPartsLeftLeg"));
	params.name(params.label);
	gDetachBodyPartPieMenus[5] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsTorso"));
	params.name(params.label);
	gDetachBodyPartPieMenus[6] = LLUICtrlFactory::create<LLContextMenu> (params);

	params.label(LLTrans::getString("BodyPartsRightLeg"));
	params.name(params.label);
	gDetachBodyPartPieMenus[7] = LLUICtrlFactory::create<LLContextMenu> (params);

	for (S32 i = 0; i < 8; i++)
	{
		if (gAttachBodyPartPieMenus[i])
		{
			gAttachPieMenu->appendContextSubMenu( gAttachBodyPartPieMenus[i] );
		}
		else
		{
			for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
				 iter != mAttachmentPoints.end();
				 ++iter)
			{
				LLViewerJointAttachment* attachment = iter->second;
				if (attachment->getGroup() == i)
				{
					LLMenuItemCallGL::Params item_params;
						
					std::string sub_piemenu_name = attachment->getName();
					if (LLTrans::getString(sub_piemenu_name) != "")
					{
						item_params.label = LLTrans::getString(sub_piemenu_name);
					}
					else
					{
						item_params.label = sub_piemenu_name;
					}
					item_params.name =(item_params.label );
					item_params.on_click.function_name = "Object.AttachToAvatar";
					item_params.on_click.parameter = iter->first;
					item_params.on_enable.function_name = "Object.EnableWear";
					item_params.on_enable.parameter = iter->first;
					LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);

					gAttachPieMenu->addChild(item);

					break;

				}
			}
		}

		if (gDetachBodyPartPieMenus[i])
		{
			gDetachPieMenu->appendContextSubMenu( gDetachBodyPartPieMenus[i] );
		}
		else
		{
			for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
				 iter != mAttachmentPoints.end();
				 ++iter)
			{
				LLViewerJointAttachment* attachment = iter->second;
				if (attachment->getGroup() == i)
				{
					LLMenuItemCallGL::Params item_params;
					std::string sub_piemenu_name = attachment->getName();
					if (LLTrans::getString(sub_piemenu_name) != "")
					{
						item_params.label = LLTrans::getString(sub_piemenu_name);
					}
					else
					{
						item_params.label = sub_piemenu_name;
					}
					item_params.name =(item_params.label );
					item_params.on_click.function_name = "Attachment.DetachFromPoint";
					item_params.on_click.parameter = iter->first;
					item_params.on_enable.function_name = "Attachment.PointFilled";
					item_params.on_enable.parameter = iter->first;
					LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);

					gDetachPieMenu->addChild(item);
						
					break;
				}
			}
		}
	}

	// add screen attachments
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end();
		 ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment->getGroup() == 8)
		{
			LLMenuItemCallGL::Params item_params;
			std::string sub_piemenu_name = attachment->getName();
			if (LLTrans::getString(sub_piemenu_name) != "")
			{
				item_params.label = LLTrans::getString(sub_piemenu_name);
			}
			else
			{
				item_params.label = sub_piemenu_name;
			}
			item_params.name =(item_params.label );
			item_params.on_click.function_name = "Object.AttachToAvatar";
			item_params.on_click.parameter = iter->first;
			item_params.on_enable.function_name = "Object.EnableWear";
			item_params.on_enable.parameter = iter->first;
			LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
			gAttachScreenPieMenu->addChild(item);

			item_params.on_click.function_name = "Attachment.DetachFromPoint";
			item_params.on_click.parameter = iter->first;
			item_params.on_enable.function_name = "Attachment.PointFilled";
			item_params.on_enable.parameter = iter->first;
			item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
			gDetachScreenPieMenu->addChild(item);
		}
	}

	for (S32 pass = 0; pass < 2; pass++)
	{
		// *TODO: Skinning - gAttachSubMenu is an awful, awful hack
		if (!gAttachSubMenu)
		{
			break;
		}
		for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
			 iter != mAttachmentPoints.end();
			 ++iter)
		{
			LLViewerJointAttachment* attachment = iter->second;
			if (attachment->getIsHUDAttachment() != (pass == 1))
			{
				continue;
			}
			LLMenuItemCallGL::Params item_params;
			std::string sub_piemenu_name = attachment->getName();
			if (LLTrans::getString(sub_piemenu_name) != "")
			{
				item_params.label = LLTrans::getString(sub_piemenu_name);
			}
			else
			{
				item_params.label = sub_piemenu_name;
			}
			item_params.name =(item_params.label );
			item_params.on_click.function_name = "Object.AttachToAvatar";
			item_params.on_click.parameter = iter->first;
			item_params.on_enable.function_name = "Object.EnableWear";
			item_params.on_enable.parameter = iter->first;
			//* TODO: Skinning:
			//LLSD params;
			//params["index"] = iter->first;
			//params["label"] = attachment->getName();
			//item->addEventHandler("on_enable", LLMenuItemCallGL::MenuCallback().function_name("Attachment.Label").parameter(params));
				
			LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
			gAttachSubMenu->addChild(item);

			item_params.on_click.function_name = "Attachment.DetachFromPoint";
			item_params.on_click.parameter = iter->first;
			item_params.on_enable.function_name = "Attachment.PointFilled";
			item_params.on_enable.parameter = iter->first;
			//* TODO: Skinning: item->addEventHandler("on_enable", LLMenuItemCallGL::MenuCallback().function_name("Attachment.Label").parameter(params));

			item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
			gDetachSubMenu->addChild(item);
		}
		if (pass == 0)
		{
			// put separator between non-hud and hud attachments
			gAttachSubMenu->addSeparator();
			gDetachSubMenu->addSeparator();
		}
	}

	for (S32 group = 0; group < 8; group++)
	{
		// skip over groups that don't have sub menus
		if (!gAttachBodyPartPieMenus[group] || !gDetachBodyPartPieMenus[group])
		{
			continue;
		}

		std::multimap<S32, S32> attachment_pie_menu_map;

		// gather up all attachment points assigned to this group, and throw into map sorted by pie slice number
		for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
			 iter != mAttachmentPoints.end();
			 ++iter)
		{
			LLViewerJointAttachment* attachment = iter->second;
			if(attachment->getGroup() == group)
			{
				// use multimap to provide a partial order off of the pie slice key
				S32 pie_index = attachment->getPieSlice();
				attachment_pie_menu_map.insert(std::make_pair(pie_index, iter->first));
			}
		}

		// add in requested order to pie menu, inserting separators as necessary
		for (std::multimap<S32, S32>::iterator attach_it = attachment_pie_menu_map.begin();
			 attach_it != attachment_pie_menu_map.end(); ++attach_it)
		{
			S32 attach_index = attach_it->second;

			LLViewerJointAttachment* attachment = get_if_there(mAttachmentPoints, attach_index, (LLViewerJointAttachment*)NULL);
			if (attachment)
			{
				LLMenuItemCallGL::Params item_params;
				item_params.name = attachment->getName();
				item_params.label = LLTrans::getString(attachment->getName());
				item_params.on_click.function_name = "Object.AttachToAvatar";
				item_params.on_click.parameter = attach_index;
				item_params.on_enable.function_name = "Object.EnableWear";
				item_params.on_enable.parameter = attach_index;

				LLMenuItemCallGL* item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
				gAttachBodyPartPieMenus[group]->addChild(item);
					
				item_params.on_click.function_name = "Attachment.DetachFromPoint";
				item_params.on_click.parameter = attach_index;
				item_params.on_enable.function_name = "Attachment.PointFilled";
				item_params.on_enable.parameter = attach_index;
				item = LLUICtrlFactory::create<LLMenuItemCallGL>(item_params);
				gDetachBodyPartPieMenus[group]->addChild(item);
			}
		}
	}
	return TRUE;
}

void LLVOAvatarSelf::cleanup()
{
	markDead();
 	delete mScreenp;
 	mScreenp = NULL;
	mRegionp = NULL;
}

LLVOAvatarSelf::~LLVOAvatarSelf()
{
	cleanup();
}

/**
 **
 ** End LLVOAvatarSelf Constructor routines
 **                                                                             **
 *********************************************************************************/

//virtual
BOOL LLVOAvatarSelf::updateCharacter(LLAgent &agent)
{
	// update screen joint size
	if (mScreenp)
	{
		F32 aspect = LLViewerCamera::getInstance()->getAspect();
		LLVector3 scale(1.f, aspect, 1.f);
		mScreenp->setScale(scale);
		mScreenp->updateWorldMatrixChildren();
		resetHUDAttachments();
	}
	
	return LLVOAvatar::updateCharacter(agent);
}

// virtual
BOOL LLVOAvatarSelf::isValid() const
{
	return ((getRegion() != NULL) && !isDead());
}

// virtual
void LLVOAvatarSelf::idleUpdate(LLAgent &agent, const F64 &time)
{
	if (isValid())
	{
		LLVOAvatar::idleUpdate(agent, time);
		idleUpdateTractorBeam();
	}
}

// virtual
LLJoint *LLVOAvatarSelf::getJoint(const std::string &name)
{
	// <alchemy> - findJoint Opt
	LLJoint* jointp = LLVOAvatar::getJoint(name);
	if (!jointp && mScreenp)
	{
		jointp = mScreenp->findJoint(name);
	}
	return jointp;

	// <alchemy>
	//if (mScreenp)
	//{
	//	LLJoint* jointp = mScreenp->findJoint(name);
	//	if (jointp) return jointp;
	//}
	//return LLVOAvatar::getJoint(name);
}
//virtual
void LLVOAvatarSelf::resetJointPositions( void )
{
	return LLVOAvatar::resetJointPositions();
}
// virtual
BOOL LLVOAvatarSelf::setVisualParamWeight(const LLVisualParam *which_param, F32 weight, BOOL upload_bake )
{
	if (!which_param)
	{
		return FALSE;
	}
	LLViewerVisualParam *param = (LLViewerVisualParam*) LLCharacter::getVisualParam(which_param->getID());
	return setParamWeight(param,weight,upload_bake);
}

// virtual
BOOL LLVOAvatarSelf::setVisualParamWeight(const char* param_name, F32 weight, BOOL upload_bake )
{
	if (!param_name)
	{
		return FALSE;
	}
	LLViewerVisualParam *param = (LLViewerVisualParam*) LLCharacter::getVisualParam(param_name);
	return setParamWeight(param,weight,upload_bake);
}

// virtual
BOOL LLVOAvatarSelf::setVisualParamWeight(S32 index, F32 weight, BOOL upload_bake )
{
	LLViewerVisualParam *param = (LLViewerVisualParam*) LLCharacter::getVisualParam(index);
	return setParamWeight(param,weight,upload_bake);
}

BOOL LLVOAvatarSelf::setParamWeight(const LLViewerVisualParam *param, F32 weight, BOOL upload_bake )
{
	if (!param)
	{
		return FALSE;
	}

#if 0
	// FIXME DRANO - kludgy way to avoid overwriting avatar state from wearables.
	if (isUsingServerBakes() && !isUsingLocalAppearance())
	{
		return FALSE;
	}
#endif

	if (param->getCrossWearable())
	{
		LLWearableType::EType type = (LLWearableType::EType)param->getWearableType();
		U32 size = gAgentWearables.getWearableCount(type);
		for (U32 count = 0; count < size; ++count)
		{
			LLViewerWearable *wearable = gAgentWearables.getViewerWearable(type,count);
			if (wearable)
			{
				wearable->setVisualParamWeight(param->getID(), weight, upload_bake);
			}
		}
	}

	return LLCharacter::setVisualParamWeight(param,weight,upload_bake);
}

/*virtual*/ 
void LLVOAvatarSelf::updateVisualParams()
{
	LLVOAvatar::updateVisualParams();
}

/*virtual*/
void LLVOAvatarSelf::idleUpdateAppearanceAnimation()
{
	// Animate all top-level wearable visual parameters
	gAgentWearables.animateAllWearableParams(calcMorphAmount(), FALSE);

	// apply wearable visual params to avatar
	for (U32 type = 0; type < LLWearableType::WT_COUNT; type++)
	{
		LLWearable *wearable = gAgentWearables.getTopWearable((LLWearableType::EType)type);
		if (wearable)
		{
			wearable->writeToAvatar(this);
		}
	}

	//allow avatar to process updates
	LLVOAvatar::idleUpdateAppearanceAnimation();

}

// virtual
void LLVOAvatarSelf::requestStopMotion(LLMotion* motion)
{
	// Only agent avatars should handle the stop motion notifications.

	// Notify agent that motion has stopped
	gAgent.requestStopMotion(motion);
}

// virtual
void LLVOAvatarSelf::stopMotionFromSource(const LLUUID& source_id)
{
	for (AnimSourceIterator motion_it = mAnimationSources.find(source_id); motion_it != mAnimationSources.end(); )
	{
		gAgent.sendAnimationRequest(motion_it->second, ANIM_REQUEST_STOP);
		mAnimationSources.erase(motion_it++);
	}

	LLViewerObject* object = gObjectList.findObject(source_id);
	if (object)
	{
		object->setFlagsWithoutUpdate(FLAGS_ANIM_SOURCE, FALSE);
	}
}

//virtual
U32  LLVOAvatarSelf::processUpdateMessage(LLMessageSystem *mesgsys,
													 void **user_data,
													 U32 block_num,
													 const EObjectUpdateType update_type,
													 LLDataPacker *dp)
{
	U32 retval = LLVOAvatar::processUpdateMessage(mesgsys,user_data,block_num,update_type,dp);

#if 0
	// DRANO - it's not clear this does anything useful. If we wait
	// until an appearance message has been received, we already have
	// the texture ids. If we don't wait, we don't yet know where to
	// look for baked textures, because we haven't received the
	// appearance version data from the appearance message. This looks
	// like an old optimization that's incompatible with server-side
	// texture baking.
	
	// FIXME DRANO - skipping in the case of !mFirstAppearanceMessageReceived prevents us from trying to
	// load textures before we know where they come from (ie, from baking service or not);
	// unknown impact on performance.
	if (mInitialBakesLoaded == false && retval == 0x0 && mFirstAppearanceMessageReceived)
	{
		// call update textures to force the images to be created
		updateMeshTextures();

		// unpack the texture UUIDs to the texture slots
		retval = unpackTEMessage(mesgsys, _PREHASH_ObjectData, (S32) block_num);

		// need to trigger a few operations to get the avatar to use the new bakes
		for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
		{
			const LLAvatarAppearanceDefines::ETextureIndex te = mBakedTextureDatas[i].mTextureIndex;
			LLUUID texture_id = getTEImage(te)->getID();
			setNewBakedTexture(te, texture_id);
			mInitialBakeIDs[i] = texture_id;
		}

		onFirstTEMessageReceived();

		mInitialBakesLoaded = true;
	}
#endif

	return retval;
}


void LLVOAvatarSelf::setLocalTextureTE(U8 te, LLViewerTexture* image, U32 index)
{
	if (te >= TEX_NUM_INDICES)
	{
		llassert(0);
		return;
	}

	if (getTEImage(te)->getID() == image->getID())
	{
		return;
	}

	if (isIndexBakedTexture((ETextureIndex)te))
	{
		llassert(0);
		return;
	}

	setTEImage(te, image);
}

//virtual
void LLVOAvatarSelf::removeMissingBakedTextures()
{	
	BOOL removed = FALSE;
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		const S32 te = mBakedTextureDatas[i].mTextureIndex;
		const LLViewerTexture* tex = getTEImage(te);

		// Replace with default if we can't find the asset, assuming the
		// default is actually valid (which it should be unless something
		// is seriously wrong).
		if (!tex || tex->isMissingAsset())
		{
			LLViewerTexture *imagep = LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
			if (imagep && imagep != tex)
			{
				setTEImage(te, imagep);
				removed = TRUE;
			}
		}
	}

	if (removed)
	{
		for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
		{
			LLViewerTexLayerSet *layerset = getTexLayerSet(i);
			layerset->setUpdatesEnabled(TRUE);
			invalidateComposite(layerset, FALSE);
		}
		updateMeshTextures();	// may call back into this function
		if (getRegion() && !getRegion()->getCentralBakeVersion())
		{
			requestLayerSetUploads();
		}
	}
}

//virtual
void LLVOAvatarSelf::updateRegion(LLViewerRegion *regionp)
{
	// Save the global position
	LLVector3d global_pos_from_old_region = getPositionGlobal();

	// Change the region
	setRegion(regionp);

	if (regionp)
	{	// Set correct region-relative position from global coordinates
		setPositionGlobal(global_pos_from_old_region);

		// Diagnostic info
		//LLVector3d pos_from_new_region = getPositionGlobal();
		//llinfos << "pos_from_old_region is " << global_pos_from_old_region
		//	<< " while pos_from_new_region is " << pos_from_new_region
		//	<< llendl;
	}

	if (!regionp || (regionp->getHandle() != mLastRegionHandle))
	{
		if (mLastRegionHandle != 0)
		{
			++mRegionCrossingCount;
			F64 delta = (F64)mRegionCrossingTimer.getElapsedTimeF32();
			F64 avg = (mRegionCrossingCount == 1) ? 0 : LLViewerStats::getInstance()->getStat(LLViewerStats::ST_CROSSING_AVG);
			F64 delta_avg = (delta + avg*(mRegionCrossingCount-1)) / mRegionCrossingCount;
			LLViewerStats::getInstance()->setStat(LLViewerStats::ST_CROSSING_AVG, delta_avg);
			
			F64 max = (mRegionCrossingCount == 1) ? 0 : LLViewerStats::getInstance()->getStat(LLViewerStats::ST_CROSSING_MAX);
			max = llmax(delta, max);
			LLViewerStats::getInstance()->setStat(LLViewerStats::ST_CROSSING_MAX, max);

			// Diagnostics
			llinfos << "Region crossing took " << (F32)(delta * 1000.0) << " ms " << llendl;
		}
		if (regionp)
		{
			mLastRegionHandle = regionp->getHandle();
		}
	}
	mRegionCrossingTimer.reset();
	LLViewerObject::updateRegion(regionp);
}

//--------------------------------------------------------------------
// draw tractor beam when editing objects
//--------------------------------------------------------------------
//virtual
void LLVOAvatarSelf::idleUpdateTractorBeam()
{
	static LLCachedControl<bool> pointAtDisable(gSavedSettings, "AlchemyPointAtDisable");
	if (pointAtDisable)
	{
		return;
	}

	// This is only done for yourself (maybe it should be in the agent?)
	if (!needsRenderBeam() || !isBuilt())
	{
		for(int i = 0; i < 8; i++) mBeam[i] = NULL;
		return;
	}

	static LLCachedControl<bool> AlchemyBoundingBoxBeam(gSavedSettings, "AlchemyBoundingBoxBeam");
	static int prev_beam_count = 1;
	int beam_count = AlchemyBoundingBoxBeam ? 8 : 1;

	// if beam count is lowered remove extra beams
	if (beam_count < prev_beam_count)
	{
		for(int i = 1; i < 8; i++) { mBeam[i] = NULL; }
	}

	bool reset_timer = false;
	for(int i = 0; i < 8; i++)
	{
		if (!mBeam[i] || mBeam[i]->isDead())
		{
			// VEFFECT: Tractor Beam
			mBeam[i] = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_BEAM);
			mBeam[i]->setColor(LLColor4U(gAgent.getEffectColor()));
			mBeam[i]->setSourceObject(this);

			reset_timer = true;
		}
	}

	if(reset_timer)
	{
		mBeamTimer.reset();
	}

	LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();

	bool use_single_beam = true;
	if(AlchemyBoundingBoxBeam)
	{
		LLBBox bounding_box;
		use_single_beam = false;
		for(LLObjectSelection::iterator iter = selection->begin(); iter != selection->end(); iter++)
		{
			LLSelectNode* node = *iter;
			LLViewerObject* object = node->getObject();
			if (!object)
				continue;
			
			LLViewerObject *root = object->getRootEdit();
			if ( selection->getSelectType() != SELECT_TYPE_WORLD || // this is an attachment
				root->isChild(gAgentAvatarp) || // this is the object you're sitting on
				object->isAvatar()) // this is another avatar
			{
				use_single_beam = true;
				break;
			}
			bounding_box.addBBoxAgent( object->getBoundingBoxAgent() );
		}
		if(!use_single_beam)
		{
			LLMatrix4 transform;
			transform.initAll(LLVector3(1.f, 1.f, 1.f), bounding_box.getRotation(), bounding_box.getPositionAgent());

			LLVector3 min = bounding_box.getMinLocal();
			LLVector3 max = bounding_box.getMaxLocal();
			
			LLVector3 manipbb[8] = {
				min,
				LLVector3(max.mV[VX], min.mV[VY], min.mV[VZ]),
				LLVector3(min.mV[VX], max.mV[VY], min.mV[VZ]),
				LLVector3(max.mV[VX], max.mV[VY], min.mV[VZ]),
				LLVector3(max.mV[VX], min.mV[VY], max.mV[VZ]),
				LLVector3(min.mV[VX], min.mV[VY], max.mV[VZ]),
				LLVector3(min.mV[VX], max.mV[VY], max.mV[VZ]),
				max
			};


			for(int i = 0; i < 8; i++)
			{
				mBeam[i]->setTargetObject(NULL);
				mBeam[i]->setPositionAgent(manipbb[i] * transform);
			}
		}
	}

	if(use_single_beam)
	{
		// we can't do bounding box beams here so clear out the beam list
		if(mBeam[1].notNull()) for(int i = 1; i < 8; i++) { mBeam[i] = NULL; }
			
		if (gAgentCamera.mPointAt.notNull())
		{
			// get point from pointat effect
			mBeam[0]->setPositionGlobal(gAgentCamera.mPointAt->getPointAtPosGlobal());
			mBeam[0]->triggerLocal();
		}
		else
		{
			if(selection->getFirstRootObject()
				&& selection->getSelectType() != SELECT_TYPE_HUD)
			{
				LLViewerObject* objectp = selection->getFirstRootObject();
				mBeam[0]->setTargetObject(objectp);
			}
			else
			{
				mBeam[0]->setTargetObject(NULL);
				LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();
				if (tool->isEditing())
				{
					if (tool->getEditingObject())
					{
						mBeam[0]->setTargetObject(tool->getEditingObject());
					}
					else
					{
						mBeam[0]->setPositionGlobal(tool->getEditingPointGlobal());
					}
				}
				else
				{
					const LLPickInfo& pick = gViewerWindow->getLastPick();
					mBeam[0]->setPositionGlobal(pick.mPosGlobal);
				}
			}
		}
	}

	if (mBeamTimer.getElapsedTimeF32() > 0.25f)
	{
		LLColor4U effect_color(gAgent.getEffectColor());
		for(int i = 0; i < 8; i++)
		{
			if(mBeam[i].isNull())
				break;
			mBeam[i]->setColor(effect_color);
			mBeam[i]->setNeedsSendToSim(TRUE);
		}
		mBeamTimer.reset();
	}
}

//-----------------------------------------------------------------------------
// restoreMeshData()
//-----------------------------------------------------------------------------
// virtual
void LLVOAvatarSelf::restoreMeshData()
{
	//llinfos << "Restoring" << llendl;
	mMeshValid = TRUE;
	updateJointLODs();
	updateAttachmentVisibility(gAgentCamera.getCameraMode());

	// force mesh update as LOD might not have changed to trigger this
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_GEOMETRY, TRUE);
}



//-----------------------------------------------------------------------------
// updateAttachmentVisibility()
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::updateAttachmentVisibility(U32 camera_mode)
{
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end();
		 ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment->getIsHUDAttachment())
		{
			attachment->setAttachmentVisibility(TRUE);
		}
		else
		{
			switch (camera_mode)
			{
				case CAMERA_MODE_MOUSELOOK:
					if (LLVOAvatar::sVisibleInFirstPerson && attachment->getVisibleInFirstPerson())
					{
						attachment->setAttachmentVisibility(TRUE);
					}
					else
					{
						attachment->setAttachmentVisibility(FALSE);
					}
					break;
				default:
					attachment->setAttachmentVisibility(TRUE);
					break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// updatedWearable( LLWearableType::EType type )
// forces an update to any baked textures relevant to type.
// will force an upload of the resulting bake if the second parameter is TRUE
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::wearableUpdated( LLWearableType::EType type, BOOL upload_result )
{
	for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator baked_iter = LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().begin();
		 baked_iter != LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().end();
		 ++baked_iter)
	{
		const LLAvatarAppearanceDictionary::BakedEntry *baked_dict = baked_iter->second;
		const LLAvatarAppearanceDefines::EBakedTextureIndex index = baked_iter->first;

		if (baked_dict)
		{
			for (LLAvatarAppearanceDefines::wearables_vec_t::const_iterator type_iter = baked_dict->mWearables.begin();
				type_iter != baked_dict->mWearables.end();
				 ++type_iter)
			{
				const LLWearableType::EType comp_type = *type_iter;
				if (comp_type == type)
				{
					LLViewerTexLayerSet *layerset = getLayerSet(index);
					if (layerset)
					{
						layerset->setUpdatesEnabled(true);
						invalidateComposite(layerset, upload_result);
					}
					break;
				}
			}
		}
	}
	
	// Physics type has no associated baked textures, but change of params needs to be sent to
	// other avatars.
	if (type == LLWearableType::WT_PHYSICS)
	  {
	    gAgent.sendAgentSetAppearance();
	  }
}

//-----------------------------------------------------------------------------
// isWearingAttachment()
//-----------------------------------------------------------------------------
BOOL LLVOAvatarSelf::isWearingAttachment(const LLUUID& inv_item_id) const
{
	const LLUUID& base_inv_item_id = gInventory.getLinkedItemID(inv_item_id);
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end();
		 ++iter)
	{
		const LLViewerJointAttachment* attachment = iter->second;
		if (attachment->getAttachedObject(base_inv_item_id))
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
BOOL LLVOAvatarSelf::attachmentWasRequested(const LLUUID& inv_item_id) const
{
	const F32 REQUEST_EXPIRATION_SECONDS = 5.0;  // any request older than this is ignored/removed.
	std::map<LLUUID,LLTimer>::iterator it = mAttachmentRequests.find(inv_item_id);
	if (it != mAttachmentRequests.end())
	{
		const LLTimer& request_time = it->second;
		F32 request_time_elapsed = request_time.getElapsedTimeF32();
		if (request_time_elapsed > REQUEST_EXPIRATION_SECONDS)
		{
			mAttachmentRequests.erase(it);
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}
	else
	{
		return FALSE;
	}
}

//-----------------------------------------------------------------------------
void LLVOAvatarSelf::addAttachmentRequest(const LLUUID& inv_item_id)
{
	LLTimer current_time;
	mAttachmentRequests[inv_item_id] = current_time;
}

//-----------------------------------------------------------------------------
void LLVOAvatarSelf::removeAttachmentRequest(const LLUUID& inv_item_id)
{
	mAttachmentRequests.erase(inv_item_id);
}

//-----------------------------------------------------------------------------
// getWornAttachment()
//-----------------------------------------------------------------------------
LLViewerObject* LLVOAvatarSelf::getWornAttachment(const LLUUID& inv_item_id)
{
	const LLUUID& base_inv_item_id = gInventory.getLinkedItemID(inv_item_id);
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end();
		 ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
 		if (LLViewerObject *attached_object = attachment->getAttachedObject(base_inv_item_id))
		{
			return attached_object;
		}
	}
	return NULL;
}

const std::string LLVOAvatarSelf::getAttachedPointName(const LLUUID& inv_item_id) const
{
	const LLUUID& base_inv_item_id = gInventory.getLinkedItemID(inv_item_id);
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(); 
		 iter != mAttachmentPoints.end(); 
		 ++iter)
	{
		const LLViewerJointAttachment* attachment = iter->second;
		if (attachment->getAttachedObject(base_inv_item_id))
		{
			return attachment->getName();
		}
	}

	return LLStringUtil::null;
}

//virtual
const LLViewerJointAttachment *LLVOAvatarSelf::attachObject(LLViewerObject *viewer_object)
{
	const LLViewerJointAttachment *attachment = LLVOAvatar::attachObject(viewer_object);
	if (!attachment)
	{
		return 0;
	}

	updateAttachmentVisibility(gAgentCamera.getCameraMode());
	
	// Then make sure the inventory is in sync with the avatar.

	// Should just be the last object added
	if (attachment->isObjectAttached(viewer_object))
	{
		const LLUUID& attachment_id = viewer_object->getAttachmentItemID();
		LLAppearanceMgr::instance().registerAttachment(attachment_id);
		// Clear any pending requests once the attachment arrives.
		removeAttachmentRequest(attachment_id);
		updateLODRiggedAttachments();		
	}

	return attachment;
}

//virtual
BOOL LLVOAvatarSelf::detachObject(LLViewerObject *viewer_object)
{
	const LLUUID attachment_id = viewer_object->getAttachmentItemID();
	if ( LLVOAvatar::detachObject(viewer_object) )
	{
		LLVOAvatar::cleanupAttachedMesh( viewer_object );
		
		// the simulator should automatically handle permission revocation
		
		stopMotionFromSource(attachment_id);
		LLFollowCamMgr::setCameraActive(viewer_object->getID(), FALSE);
		
		LLViewerObject::const_child_list_t& child_list = viewer_object->getChildren();
		for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
			 iter != child_list.end(); 
			 ++iter)
		{
			LLViewerObject* child_objectp = *iter;
			// the simulator should automatically handle
			// permissions revocation
			
			stopMotionFromSource(child_objectp->getID());
			LLFollowCamMgr::setCameraActive(child_objectp->getID(), FALSE);
		}
		
		// Make sure the inventory is in sync with the avatar.

		// Update COF contents, don't trigger appearance update.
		if (!isValid())
		{
			llinfos << "removeItemLinks skipped, avatar is under destruction" << llendl;
		}
		else
		{
			LLAppearanceMgr::instance().unregisterAttachment(attachment_id);
		}
		
		return TRUE;
	}
	return FALSE;
}

// static
BOOL LLVOAvatarSelf::detachAttachmentIntoInventory(const LLUUID &item_id)
{
	LLInventoryItem* item = gInventory.getItem(item_id);
	if (item)
	{
		gMessageSystem->newMessageFast(_PREHASH_DetachAttachmentIntoInv);
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_ItemID, item_id);
		gMessageSystem->sendReliable(gAgent.getRegion()->getHost());
		
		// This object might have been selected, so let the selection manager know it's gone now
		LLViewerObject *found_obj = gObjectList.findObject(item_id);
		if (found_obj)
		{
			LLSelectMgr::getInstance()->remove(found_obj);
		}

		// Error checking in case this object was attached to an invalid point
		// In that case, just remove the item from COF preemptively since detach 
		// will fail.
		if (isAgentAvatarValid())
		{
			const LLViewerObject *attached_obj = gAgentAvatarp->getWornAttachment(item_id);
			if (!attached_obj)
			{
				LLAppearanceMgr::instance().removeCOFItemLinks(item_id);
			}
		}
		return TRUE;
	}
	return FALSE;
}

U32 LLVOAvatarSelf::getNumWearables(LLAvatarAppearanceDefines::ETextureIndex i) const
{
	LLWearableType::EType type = LLAvatarAppearanceDictionary::getInstance()->getTEWearableType(i);
	return gAgentWearables.getWearableCount(type);
}

// virtual
void LLVOAvatarSelf::localTextureLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src_raw, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{	

	const LLUUID& src_id = src_vi->getID();
	LLAvatarTexData *data = (LLAvatarTexData *)userdata;
	ETextureIndex index = data->mIndex;
	if (!isIndexLocalTexture(index)) return;

	LLLocalTextureObject *local_tex_obj = getLocalTextureObject(index, 0);

	// fix for EXT-268. Preventing using of NULL pointer
	if(NULL == local_tex_obj)
	{
		LL_WARNS("TAG") << "There is no Local Texture Object with index: " << index 
			<< ", final: " << final
			<< LL_ENDL;
		return;
	}
	if (success)
	{
		if (!local_tex_obj->getBakedReady() &&
			local_tex_obj->getImage() != NULL &&
			(local_tex_obj->getID() == src_id) &&
			discard_level < local_tex_obj->getDiscard())
		{
			local_tex_obj->setDiscard(discard_level);
			requestLayerSetUpdate(index);
			if (isEditingAppearance())
			{
				LLVisualParamHint::requestHintUpdates();
			}
			updateMeshTextures();
		}
	}
	else if (final)
	{
		// Failed: asset is missing
		if (!local_tex_obj->getBakedReady() &&
			local_tex_obj->getImage() != NULL &&
			local_tex_obj->getImage()->getID() == src_id)
		{
			local_tex_obj->setDiscard(0);
			requestLayerSetUpdate(index);
			updateMeshTextures();
		}
	}
}

// virtual
BOOL LLVOAvatarSelf::getLocalTextureGL(ETextureIndex type, LLViewerTexture** tex_pp, U32 index) const
{
	*tex_pp = NULL;

	if (!isIndexLocalTexture(type)) return FALSE;
	if (getLocalTextureID(type, index) == IMG_DEFAULT_AVATAR) return TRUE;

	const LLLocalTextureObject *local_tex_obj = getLocalTextureObject(type, index);
	if (!local_tex_obj)
	{
		return FALSE;
	}
	*tex_pp = dynamic_cast<LLViewerTexture*> (local_tex_obj->getImage());
	return TRUE;
}

LLViewerFetchedTexture* LLVOAvatarSelf::getLocalTextureGL(LLAvatarAppearanceDefines::ETextureIndex type, U32 index) const
{
	if (!isIndexLocalTexture(type))
	{
		return NULL;
	}

	const LLLocalTextureObject *local_tex_obj = getLocalTextureObject(type, index);
	if (!local_tex_obj)
	{
		return NULL;
	}
	if (local_tex_obj->getID() == IMG_DEFAULT_AVATAR)
	{
		return LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
	}
	return dynamic_cast<LLViewerFetchedTexture*> (local_tex_obj->getImage());
}

const LLUUID& LLVOAvatarSelf::getLocalTextureID(ETextureIndex type, U32 index) const
{
	if (!isIndexLocalTexture(type)) return IMG_DEFAULT_AVATAR;

	const LLLocalTextureObject *local_tex_obj = getLocalTextureObject(type, index);
	if (local_tex_obj && local_tex_obj->getImage() != NULL)
	{
		return local_tex_obj->getImage()->getID();
	}
	return IMG_DEFAULT_AVATAR;
} 


//-----------------------------------------------------------------------------
// isLocalTextureDataAvailable()
// Returns true if at least the lowest quality discard level exists for every texture
// in the layerset.
//-----------------------------------------------------------------------------
BOOL LLVOAvatarSelf::isLocalTextureDataAvailable(const LLViewerTexLayerSet* layerset) const
{
	/* if (layerset == mBakedTextureDatas[BAKED_HEAD].mTexLayerSet)
	   return getLocalDiscardLevel(TEX_HEAD_BODYPAINT) >= 0; */
	for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator baked_iter = LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().begin();
		 baked_iter != LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().end();
		 ++baked_iter)
	{
		const EBakedTextureIndex baked_index = baked_iter->first;
		if (layerset == mBakedTextureDatas[baked_index].mTexLayerSet)
		{
			BOOL ret = true;
			const LLAvatarAppearanceDictionary::BakedEntry *baked_dict = baked_iter->second;
			for (texture_vec_t::const_iterator local_tex_iter = baked_dict->mLocalTextures.begin();
				 local_tex_iter != baked_dict->mLocalTextures.end();
				 ++local_tex_iter)
			{
				const ETextureIndex tex_index = *local_tex_iter;
				const LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(tex_index);
				const U32 wearable_count = gAgentWearables.getWearableCount(wearable_type);
				for (U32 wearable_index = 0; wearable_index < wearable_count; wearable_index++)
				{
					BOOL tex_avail = (getLocalDiscardLevel(tex_index, wearable_index) >= 0);
					ret &= tex_avail;
				}
			}
			return ret;
		}
	}
	llassert(0);
	return FALSE;
}

//-----------------------------------------------------------------------------
// virtual
// isLocalTextureDataFinal()
// Returns true if the highest quality discard level exists for every texture
// in the layerset.
//-----------------------------------------------------------------------------
BOOL LLVOAvatarSelf::isLocalTextureDataFinal(const LLViewerTexLayerSet* layerset) const
{
	static LLCachedControl<U32> desired_tex_discard_level(gSavedSettings, "TextureDiscardLevel");
	// const U32 desired_tex_discard_level = 0; // hack to not bake textures on lower discard levels.

	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		if (layerset == mBakedTextureDatas[i].mTexLayerSet)
		{
			const LLAvatarAppearanceDictionary::BakedEntry *baked_dict = LLAvatarAppearanceDictionary::getInstance()->getBakedTexture((EBakedTextureIndex)i);
			for (texture_vec_t::const_iterator local_tex_iter = baked_dict->mLocalTextures.begin();
				 local_tex_iter != baked_dict->mLocalTextures.end();
				 ++local_tex_iter)
			{
				const ETextureIndex tex_index = *local_tex_iter;
				const LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(tex_index);
				const U32 wearable_count = gAgentWearables.getWearableCount(wearable_type);
				for (U32 wearable_index = 0; wearable_index < wearable_count; wearable_index++)
				{
					S32 local_discard_level = getLocalDiscardLevel(*local_tex_iter, wearable_index);
					if ((local_discard_level > (S32)(desired_tex_discard_level)) ||
						(local_discard_level < 0 ))
					{
						return FALSE;
					}
				}
			}
			return TRUE;
		}
	}
	llassert(0);
	return FALSE;
}


BOOL LLVOAvatarSelf::isAllLocalTextureDataFinal() const
{
	static LLCachedControl<U32> desired_tex_discard_level(gSavedSettings, "TextureDiscardLevel");
	// const U32 desired_tex_discard_level = 0; // hack to not bake textures on lower discard levels

	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		const LLAvatarAppearanceDictionary::BakedEntry *baked_dict = LLAvatarAppearanceDictionary::getInstance()->getBakedTexture((EBakedTextureIndex)i);
		for (texture_vec_t::const_iterator local_tex_iter = baked_dict->mLocalTextures.begin();
			 local_tex_iter != baked_dict->mLocalTextures.end();
			 ++local_tex_iter)
		{
			const ETextureIndex tex_index = *local_tex_iter;
			const LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(tex_index);
			const U32 wearable_count = gAgentWearables.getWearableCount(wearable_type);
			for (U32 wearable_index = 0; wearable_index < wearable_count; wearable_index++)
			{
				S32 local_discard_level = getLocalDiscardLevel(*local_tex_iter, wearable_index);
				if ((local_discard_level > (S32)(desired_tex_discard_level)) ||
					(local_discard_level < 0 ))
				{
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}

BOOL LLVOAvatarSelf::isBakedTextureFinal(const LLAvatarAppearanceDefines::EBakedTextureIndex index) const
{
	const LLViewerTexLayerSet *layerset = getLayerSet(index);
	if (!layerset) return FALSE;
	const LLViewerTexLayerSetBuffer *layerset_buffer = layerset->getViewerComposite();
	if (!layerset_buffer) return FALSE;
	return !layerset_buffer->uploadNeeded();
}

BOOL LLVOAvatarSelf::isTextureDefined(LLAvatarAppearanceDefines::ETextureIndex type, U32 index) const
{
	LLUUID id;
	BOOL isDefined = TRUE;
	if (isIndexLocalTexture(type))
	{
		const LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(type);
		const U32 wearable_count = gAgentWearables.getWearableCount(wearable_type);
		if (index >= wearable_count)
		{
			// invalid index passed in. check all textures of a given type
			for (U32 wearable_index = 0; wearable_index < wearable_count; wearable_index++)
			{
				id = getLocalTextureID(type, wearable_index);
				isDefined &= (id != IMG_DEFAULT_AVATAR && id != IMG_DEFAULT);
			}
		}
		else
		{
			id = getLocalTextureID(type, index);
			isDefined &= (id != IMG_DEFAULT_AVATAR && id != IMG_DEFAULT);
		}
	}
	else
	{
		id = getTEImage(type)->getID();
		isDefined &= (id != IMG_DEFAULT_AVATAR && id != IMG_DEFAULT);
	}
	
	return isDefined;
}

//virtual
BOOL LLVOAvatarSelf::isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type, U32 index) const
{
	if (isIndexBakedTexture(type))
	{
		return LLVOAvatar::isTextureVisible(type, (U32)0);
	}

	LLUUID tex_id = getLocalTextureID(type,index);
	return (tex_id != IMG_INVISIBLE) 
			|| (LLDrawPoolAlpha::sShowDebugAlpha);
}

//virtual
BOOL LLVOAvatarSelf::isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type, LLViewerWearable *wearable) const
{
	if (isIndexBakedTexture(type))
	{
		return LLVOAvatar::isTextureVisible(type);
	}

	U32 index = gAgentWearables.getWearableIndex(wearable);
	return isTextureVisible(type,index);
}


//-----------------------------------------------------------------------------
// requestLayerSetUploads()
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::requestLayerSetUploads()
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		requestLayerSetUpload((EBakedTextureIndex)i);
	}
}

void LLVOAvatarSelf::requestLayerSetUpload(LLAvatarAppearanceDefines::EBakedTextureIndex i)
{
	ETextureIndex tex_index = mBakedTextureDatas[i].mTextureIndex;
	const BOOL layer_baked = isTextureDefined(tex_index, gAgentWearables.getWearableCount(tex_index));
	LLViewerTexLayerSet *layerset = getLayerSet(i);
	if (!layer_baked && layerset)
	{
		layerset->requestUpload();
	}
}

bool LLVOAvatarSelf::areTexturesCurrent() const
{
	return !hasPendingBakedUploads() && gAgentWearables.areWearablesLoaded();
}

// virtual
bool LLVOAvatarSelf::hasPendingBakedUploads() const
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		LLViewerTexLayerSet* layerset = getTexLayerSet(i);
		if (layerset && layerset->getViewerComposite() && layerset->getViewerComposite()->uploadPending())
		{
			return true;
		}
	}
	return false;
}

void LLVOAvatarSelf::invalidateComposite( LLTexLayerSet* layerset, BOOL upload_result )
{
	LLViewerTexLayerSet *layer_set = dynamic_cast<LLViewerTexLayerSet*>(layerset);
	if( !layer_set || !layer_set->getUpdatesEnabled() )
	{
		return;
	}
	// llinfos << "LLVOAvatar::invalidComposite() " << layerset->getBodyRegionName() << llendl;

	layer_set->requestUpdate();
	layer_set->invalidateMorphMasks();

	if( upload_result  && (getRegion() && !getRegion()->getCentralBakeVersion()))
	{
		llassert(isSelf());

		ETextureIndex baked_te = getBakedTE( layer_set );
		setTEImage( baked_te, LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR) );
		layer_set->requestUpload();
		updateMeshTextures();
	}
}

void LLVOAvatarSelf::invalidateAll()
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		LLViewerTexLayerSet *layerset = getTexLayerSet(i);
		invalidateComposite(layerset, TRUE);
	}
	//mDebugSelfLoadTimer.reset();
}

//-----------------------------------------------------------------------------
// setCompositeUpdatesEnabled()
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::setCompositeUpdatesEnabled( bool b )
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		setCompositeUpdatesEnabled(i, b);
	}
}

void LLVOAvatarSelf::setCompositeUpdatesEnabled(U32 index, bool b)
{
	LLViewerTexLayerSet *layerset = getTexLayerSet(index);
	if (layerset )
	{
		layerset->setUpdatesEnabled( b );
	}
}

bool LLVOAvatarSelf::isCompositeUpdateEnabled(U32 index)
{
	LLViewerTexLayerSet *layerset = getTexLayerSet(index);
	if (layerset)
	{
		return layerset->getUpdatesEnabled();
	}
	return false;
}

void LLVOAvatarSelf::setupComposites()
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		ETextureIndex tex_index = mBakedTextureDatas[i].mTextureIndex;
		BOOL layer_baked = isTextureDefined(tex_index, gAgentWearables.getWearableCount(tex_index));
		LLViewerTexLayerSet *layerset = getTexLayerSet(i);
		if (layerset)
		{
			layerset->setUpdatesEnabled(!layer_baked);
		}
	}
}

void LLVOAvatarSelf::updateComposites()
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		LLViewerTexLayerSet *layerset = getTexLayerSet(i);
		if (layerset 
			&& ((i != BAKED_SKIRT) || isWearingWearableType(LLWearableType::WT_SKIRT)))
		{
			layerset->updateComposite();
		}
	}
}

// virtual
S32 LLVOAvatarSelf::getLocalDiscardLevel(ETextureIndex type, U32 wearable_index) const
{
	if (!isIndexLocalTexture(type)) return FALSE;

	const LLLocalTextureObject *local_tex_obj = getLocalTextureObject(type, wearable_index);
	if (local_tex_obj)
	{
		const LLViewerFetchedTexture* image = dynamic_cast<LLViewerFetchedTexture*>( local_tex_obj->getImage() );
		if (type >= 0
			&& local_tex_obj->getID() != IMG_DEFAULT_AVATAR
			&& !image->isMissingAsset())
		{
			return image->getDiscardLevel();
		}
		else
		{
			// We don't care about this (no image associated with the layer) treat as fully loaded.
			return 0;
		}
	}
	return 0;
}

// virtual
// Counts the memory footprint of local textures.
void LLVOAvatarSelf::getLocalTextureByteCount(S32* gl_bytes) const
{
	*gl_bytes = 0;
	for (S32 type = 0; type < TEX_NUM_INDICES; type++)
	{
		if (!isIndexLocalTexture((ETextureIndex)type)) continue;
		U32 max_tex = getNumWearables((ETextureIndex) type);
		for (U32 num = 0; num < max_tex; num++)
		{
			const LLLocalTextureObject *local_tex_obj = getLocalTextureObject((ETextureIndex) type, num);
			if (local_tex_obj)
			{
				const LLViewerFetchedTexture* image_gl = dynamic_cast<LLViewerFetchedTexture*>( local_tex_obj->getImage() );
				if (image_gl)
				{
					S32 bytes = (S32)image_gl->getWidth() * image_gl->getHeight() * image_gl->getComponents();
					
					if (image_gl->hasGLTexture())
					{
						*gl_bytes += bytes;
					}
				}
			}
		}
	}
}

// virtual 
void LLVOAvatarSelf::setLocalTexture(ETextureIndex type, LLViewerTexture* src_tex, BOOL baked_version_ready, U32 index)
{
	if (!isIndexLocalTexture(type)) return;

	LLViewerFetchedTexture* tex = LLViewerTextureManager::staticCastToFetchedTexture(src_tex, TRUE) ;
	if(!tex)
	{
		return ;
	}

	S32 desired_discard = isSelf() ? 0 : 2;
	LLLocalTextureObject *local_tex_obj = getLocalTextureObject(type,index);
	if (!local_tex_obj)
	{
		if (type >= TEX_NUM_INDICES)
		{
			llerrs << "Tried to set local texture with invalid type: (" << (U32) type << ", " << index << ")" << llendl;
			return;
		}
		LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getInstance()->getTEWearableType(type);
		if (!gAgentWearables.getViewerWearable(wearable_type,index))
		{
			// no wearable is loaded, cannot set the texture.
			return;
		}
		gAgentWearables.addLocalTextureObject(wearable_type,type,index);
		local_tex_obj = getLocalTextureObject(type,index);
		if (!local_tex_obj)
		{
			llerrs << "Unable to create LocalTextureObject for wearable type & index: (" << (U32) wearable_type << ", " << index << ")" << llendl;
			return;
		}
		
		LLViewerTexLayerSet *layer_set = getLayerSet(type);
		if (layer_set)
		{
			layer_set->cloneTemplates(local_tex_obj, type, gAgentWearables.getViewerWearable(wearable_type,index));
		}

	}
	if (!baked_version_ready)
	{
		if (tex != local_tex_obj->getImage() || local_tex_obj->getBakedReady())
		{
			local_tex_obj->setDiscard(MAX_DISCARD_LEVEL+1);
		}
		if (tex->getID() != IMG_DEFAULT_AVATAR)
		{
			if (local_tex_obj->getDiscard() > desired_discard)
			{
				S32 tex_discard = tex->getDiscardLevel();
				if (tex_discard >= 0 && tex_discard <= desired_discard)
				{
					local_tex_obj->setDiscard(tex_discard);
					if (isSelf())
					{
						requestLayerSetUpdate(type);
						if (isEditingAppearance())
						{
							LLVisualParamHint::requestHintUpdates();
						}
					}
				}
				else
				{					
					tex->setLoadedCallback(onLocalTextureLoaded, desired_discard, TRUE, FALSE, new LLAvatarTexData(getID(), type), NULL);
				}
			}
			tex->setMinDiscardLevel(desired_discard);
		}
	}
	local_tex_obj->setImage(tex);
	local_tex_obj->setID(tex->getID());
	setBakedReady(type,baked_version_ready,index);
}

//virtual
void LLVOAvatarSelf::setBakedReady(LLAvatarAppearanceDefines::ETextureIndex type, BOOL baked_version_exists, U32 index)
{
	if (!isIndexLocalTexture(type)) return;
	LLLocalTextureObject *local_tex_obj = getLocalTextureObject(type,index);
	if (local_tex_obj)
	{
		local_tex_obj->setBakedReady( baked_version_exists );
	}
}


// virtual
void LLVOAvatarSelf::dumpLocalTextures() const
{
	llinfos << "Local Textures:" << llendl;

	/* ETextureIndex baked_equiv[] = {
	   TEX_UPPER_BAKED,
	   if (isTextureDefined(baked_equiv[i])) */
	for (LLAvatarAppearanceDictionary::Textures::const_iterator iter = LLAvatarAppearanceDictionary::getInstance()->getTextures().begin();
		 iter != LLAvatarAppearanceDictionary::getInstance()->getTextures().end();
		 ++iter)
	{
		const LLAvatarAppearanceDictionary::TextureEntry *texture_dict = iter->second;
		if (!texture_dict->mIsLocalTexture || !texture_dict->mIsUsedByBakedTexture)
			continue;

		const EBakedTextureIndex baked_index = texture_dict->mBakedTextureIndex;
		const ETextureIndex baked_equiv = LLAvatarAppearanceDictionary::getInstance()->getBakedTexture(baked_index)->mTextureIndex;

		const std::string &name = texture_dict->mName;
		const LLLocalTextureObject *local_tex_obj = getLocalTextureObject(iter->first, 0);
		// index is baked texture - index is not relevant. putting in 0 as placeholder
		if (isTextureDefined(baked_equiv, 0))
		{
#if LL_RELEASE_FOR_DOWNLOAD
			// End users don't get to trivially see avatar texture IDs, makes textures
			// easier to steal. JC
			llinfos << "LocTex " << name << ": Baked " << llendl;
#else
			llinfos << "LocTex " << name << ": Baked " << getTEImage(baked_equiv)->getID() << llendl;
#endif
		}
		else if (local_tex_obj && local_tex_obj->getImage() != NULL)
		{
			if (local_tex_obj->getImage()->getID() == IMG_DEFAULT_AVATAR)
			{
				llinfos << "LocTex " << name << ": None" << llendl;
			}
			else
			{
				const LLViewerFetchedTexture* image = dynamic_cast<LLViewerFetchedTexture*>( local_tex_obj->getImage() );

				llinfos << "LocTex " << name << ": "
						<< "Discard " << image->getDiscardLevel() << ", "
						<< "(" << image->getWidth() << ", " << image->getHeight() << ") " 
#if !LL_RELEASE_FOR_DOWNLOAD
					// End users don't get to trivially see avatar texture IDs,
					// makes textures easier to steal
						<< image->getID() << " "
#endif
						<< "Priority: " << image->getDecodePriority()
						<< llendl;
			}
		}
		else
		{
			llinfos << "LocTex " << name << ": No LLViewerTexture" << llendl;
		}
	}
}

//-----------------------------------------------------------------------------
// static 
// onLocalTextureLoaded()
//-----------------------------------------------------------------------------

void LLVOAvatarSelf::onLocalTextureLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src_raw, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
	LLAvatarTexData *data = (LLAvatarTexData *)userdata;
	LLVOAvatarSelf *self = (LLVOAvatarSelf *)gObjectList.findObject(data->mAvatarID);
	if (self)
	{
		// We should only be handling local textures for ourself
		self->localTextureLoaded(success, src_vi, src_raw, aux_src, discard_level, final, userdata);
	}
	// ensure data is cleaned up
	if (final || !success)
	{
		delete data;
	}
}

/*virtual*/	void LLVOAvatarSelf::setImage(const U8 te, LLViewerTexture *imagep, const U32 index)
{
	if (isIndexLocalTexture((ETextureIndex)te))
	{
		setLocalTexture((ETextureIndex)te, imagep, FALSE ,index);
	}
	else 
	{
		setTEImage(te,imagep);
	}
}

/*virtual*/ LLViewerTexture* LLVOAvatarSelf::getImage(const U8 te, const U32 index) const
{
	if (isIndexLocalTexture((ETextureIndex)te))
	{
		return getLocalTextureGL((ETextureIndex)te,index);
	}
	else 
	{
		return getTEImage(te);
	}
}


// static
void LLVOAvatarSelf::dumpTotalLocalTextureByteCount()
{
	S32 gl_bytes = 0;
	gAgentAvatarp->getLocalTextureByteCount(&gl_bytes);
	llinfos << "Total Avatar LocTex GL:" << (gl_bytes/1024) << "KB" << llendl;
}

BOOL LLVOAvatarSelf::getIsCloud() const
{
	// Let people know why they're clouded without spamming them into oblivion.
	bool do_warn = false;
	static LLTimer time_since_notice;
	F32 update_freq = 30.0;
	if (time_since_notice.getElapsedTimeF32() > update_freq)
	{
		time_since_notice.reset();
		do_warn = true;
	}
	
	// do we have our body parts?
	S32 shape_count = gAgentWearables.getWearableCount(LLWearableType::WT_SHAPE);
	S32 hair_count = gAgentWearables.getWearableCount(LLWearableType::WT_HAIR);
	S32 eye_count = gAgentWearables.getWearableCount(LLWearableType::WT_EYES);
	S32 skin_count = gAgentWearables.getWearableCount(LLWearableType::WT_SKIN);
	if (!shape_count || !hair_count || !eye_count || !skin_count)
	{
		if (do_warn)
		{
			llinfos << "Self is clouded due to missing one or more required body parts: "
					<< (shape_count ? "" : "SHAPE ")
					<< (hair_count ? "" : "HAIR ")
					<< (eye_count ? "" : "EYES ")
					<< (skin_count ? "" : "SKIN ")
					<< llendl;
		}
		return TRUE;
	}

	if (!isTextureDefined(TEX_HAIR, 0))
	{
		if (do_warn)
		{
			llinfos << "Self is clouded because of no hair texture" << llendl;
		}
		return TRUE;
	}

	if (!mPreviousFullyLoaded)
	{
		if (!isLocalTextureDataAvailable(getLayerSet(BAKED_LOWER)) &&
			(!isTextureDefined(TEX_LOWER_BAKED, 0)))
		{
			if (do_warn)
			{
				llinfos << "Self is clouded because lower textures not baked" << llendl;
			}
			return TRUE;
		}

		if (!isLocalTextureDataAvailable(getLayerSet(BAKED_UPPER)) &&
			(!isTextureDefined(TEX_UPPER_BAKED, 0)))
		{
			if (do_warn)
			{
				llinfos << "Self is clouded because upper textures not baked" << llendl;
			}
			return TRUE;
		}

		for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
		{
			if (i == BAKED_SKIRT && !isWearingWearableType(LLWearableType::WT_SKIRT))
				continue;

			const BakedTextureData& texture_data = mBakedTextureDatas[i];
			if (!isTextureDefined(texture_data.mTextureIndex, 0))
				continue;

			// Check for the case that texture is defined but not sufficiently loaded to display anything.
			const LLViewerTexture* baked_img = getImage( texture_data.mTextureIndex, 0 );
			if (!baked_img || !baked_img->hasGLTexture())
			{
				if (do_warn)
				{
					llinfos << "Self is clouded because texture at index " << i
							<< " (texture index is " << texture_data.mTextureIndex << ") is not loaded" << llendl;
				}
				return TRUE;
			}
		}

		lldebugs << "Avatar de-clouded" << llendl;
	}
	return FALSE;
}

/*static*/
void LLVOAvatarSelf::debugOnTimingLocalTexLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
	gAgentAvatarp->debugTimingLocalTexLoaded(success, src_vi, src, aux_src, discard_level, final, userdata);
}

void LLVOAvatarSelf::debugTimingLocalTexLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
	LLAvatarTexData *data = (LLAvatarTexData *)userdata;
	if (!data)
	{
		return;
	}

	ETextureIndex index = data->mIndex;
	
	if (index < 0 || index >= TEX_NUM_INDICES)
	{
		return;
	}

	if (discard_level >=0 && discard_level <= MAX_DISCARD_LEVEL) // ignore discard level -1, as it means we have no data.
	{
		mDebugTextureLoadTimes[(U32)index][(U32)discard_level] = mDebugSelfLoadTimer.getElapsedTimeF32();
	}
	if (final)
	{
		delete data;
	}
}

void LLVOAvatarSelf::debugBakedTextureUpload(EBakedTextureIndex index, BOOL finished)
{
	U32 done = 0;
	if (finished)
	{
		done = 1;
	}
	mDebugBakedTextureTimes[index][done] = mDebugSelfLoadTimer.getElapsedTimeF32();
}

const std::string LLVOAvatarSelf::verboseDebugDumpLocalTextureDataInfo(const LLViewerTexLayerSet* layerset) const
{
	std::ostringstream outbuf;
	for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator baked_iter =
			 LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().begin();
		 baked_iter != LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().end();
		 ++baked_iter)
	{
		const EBakedTextureIndex baked_index = baked_iter->first;
		if (layerset == mBakedTextureDatas[baked_index].mTexLayerSet)
		{
			outbuf << "baked_index: " << baked_index << "\n";
			const LLAvatarAppearanceDictionary::BakedEntry *baked_dict = baked_iter->second;
			for (texture_vec_t::const_iterator local_tex_iter = baked_dict->mLocalTextures.begin();
				 local_tex_iter != baked_dict->mLocalTextures.end();
				 ++local_tex_iter)
			{
				const ETextureIndex tex_index = *local_tex_iter;
				const std::string tex_name = LLAvatarAppearanceDictionary::getInstance()->getTexture(tex_index)->mName;
				outbuf << "  tex_index " << (S32) tex_index << " name " << tex_name << "\n";
				const LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(tex_index);
				const U32 wearable_count = gAgentWearables.getWearableCount(wearable_type);
				if (wearable_count > 0)
				{
					for (U32 wearable_index = 0; wearable_index < wearable_count; wearable_index++)
					{
						outbuf << "    " << LLWearableType::getTypeName(wearable_type) << " " << wearable_index << ":";
						const LLLocalTextureObject *local_tex_obj = getLocalTextureObject(tex_index, wearable_index);
						if (local_tex_obj)
						{
							LLViewerFetchedTexture* image = dynamic_cast<LLViewerFetchedTexture*>( local_tex_obj->getImage() );
							if (tex_index >= 0
								&& local_tex_obj->getID() != IMG_DEFAULT_AVATAR
								&& !image->isMissingAsset())
							{
								outbuf << " id: " << image->getID()
									   << " refs: " << image->getNumRefs()
									   << " glocdisc: " << getLocalDiscardLevel(tex_index, wearable_index)
									   << " discard: " << image->getDiscardLevel()
									   << " desired: " << image->getDesiredDiscardLevel()
									   << " decode: " << image->getDecodePriority()
									   << " addl: " << image->getAdditionalDecodePriority()
									   << " ts: " << image->getTextureState()
									   << " bl: " << image->getBoostLevel()
									   << " fl: " << image->isFullyLoaded() // this is not an accessor for mFullyLoaded - see comment there.
									   << " cl: " << (image->isFullyLoaded() && image->getDiscardLevel()==0) // "completely loaded"
									   << " mvs: " << image->getMaxVirtualSize()
									   << " mvsc: " << image->getMaxVirtualSizeResetCounter()
									   << " mem: " << image->getTextureMemory();
							}
						}
						outbuf << "\n";
					}
				}
			}
			break;
		}
	}
	return outbuf.str();
}

void LLVOAvatarSelf::dumpAllTextures() const
{
	std::string vd_text = "Local textures per baked index and wearable:\n";
	for (LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::BakedTextures::const_iterator baked_iter = LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().begin();
		 baked_iter != LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().end();
		 ++baked_iter)
	{
		const LLAvatarAppearanceDefines::EBakedTextureIndex baked_index = baked_iter->first;
		const LLViewerTexLayerSet *layerset = debugGetLayerSet(baked_index);
		if (!layerset) continue;
		const LLViewerTexLayerSetBuffer *layerset_buffer = layerset->getViewerComposite();
		if (!layerset_buffer) continue;
		vd_text += verboseDebugDumpLocalTextureDataInfo(layerset);
	}
	LL_DEBUGS("Avatar") << vd_text << llendl;
}

const std::string LLVOAvatarSelf::debugDumpLocalTextureDataInfo(const LLViewerTexLayerSet* layerset) const
{
	std::string text="";

	text = llformat("[Final:%d Avail:%d] ",isLocalTextureDataFinal(layerset), isLocalTextureDataAvailable(layerset));

	/* if (layerset == mBakedTextureDatas[BAKED_HEAD].mTexLayerSet)
	   return getLocalDiscardLevel(TEX_HEAD_BODYPAINT) >= 0; */
	for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator baked_iter = LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().begin();
		 baked_iter != LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().end();
		 ++baked_iter)
	{
		const EBakedTextureIndex baked_index = baked_iter->first;
		if (layerset == mBakedTextureDatas[baked_index].mTexLayerSet)
		{
			const LLAvatarAppearanceDictionary::BakedEntry *baked_dict = baked_iter->second;
			text += llformat("%d-%s ( ",baked_index, baked_dict->mName.c_str());
			for (texture_vec_t::const_iterator local_tex_iter = baked_dict->mLocalTextures.begin();
				 local_tex_iter != baked_dict->mLocalTextures.end();
				 ++local_tex_iter)
			{
				const ETextureIndex tex_index = *local_tex_iter;
				const LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(tex_index);
				const U32 wearable_count = gAgentWearables.getWearableCount(wearable_type);
				if (wearable_count > 0)
				{
					text += LLWearableType::getTypeName(wearable_type) + ":";
					for (U32 wearable_index = 0; wearable_index < wearable_count; wearable_index++)
					{
						const U32 discard_level = getLocalDiscardLevel(tex_index, wearable_index);
						std::string discard_str = llformat("%d ",discard_level);
						text += llformat("%d ",discard_level);
					}
				}
			}
			text += ")";
			break;
		}
	}
	return text;
}

const std::string LLVOAvatarSelf::debugDumpAllLocalTextureDataInfo() const
{
	std::string text;
	const U32 override_tex_discard_level = gSavedSettings.getU32("TextureDiscardLevel");

	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		const LLAvatarAppearanceDictionary::BakedEntry *baked_dict = LLAvatarAppearanceDictionary::getInstance()->getBakedTexture((EBakedTextureIndex)i);
		BOOL is_texture_final = TRUE;
		for (texture_vec_t::const_iterator local_tex_iter = baked_dict->mLocalTextures.begin();
			 local_tex_iter != baked_dict->mLocalTextures.end();
			 ++local_tex_iter)
		{
			const ETextureIndex tex_index = *local_tex_iter;
			const LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(tex_index);
			const U32 wearable_count = gAgentWearables.getWearableCount(wearable_type);
			for (U32 wearable_index = 0; wearable_index < wearable_count; wearable_index++)
			{
				is_texture_final &= (getLocalDiscardLevel(*local_tex_iter, wearable_index) <= (S32)(override_tex_discard_level));
			}
		}
		text += llformat("%s:%d ",baked_dict->mName.c_str(),is_texture_final);
	}
	return text;
}


#if 0
// Dump avatar metrics data.
LLSD LLVOAvatarSelf::metricsData()
{
	// runway - add region info
	LLSD result;
	result["rez_status"] = LLVOAvatar::rezStatusToString(getRezzedStatus());
	result["timers"]["debug_existence"] = mDebugExistenceTimer.getElapsedTimeF32();
	result["timers"]["ruth_debug"] = mRuthDebugTimer.getElapsedTimeF32();
	result["timers"]["ruth"] = mRuthTimer.getElapsedTimeF32();
	result["timers"]["invisible"] = mInvisibleTimer.getElapsedTimeF32();
	result["timers"]["fully_loaded"] = mFullyLoadedTimer.getElapsedTimeF32();
	result["startup"] = LLStartUp::getPhases().dumpPhases();
	
	return result;
}
#endif

class ViewerAppearanceChangeMetricsResponder: public LLCurl::Responder
{
public:
	ViewerAppearanceChangeMetricsResponder( S32 expected_sequence,
											volatile const S32 & live_sequence,
											volatile bool & reporting_started):
		mExpectedSequence(expected_sequence),
		mLiveSequence(live_sequence),
		mReportingStarted(reporting_started)
	{
	}

	virtual void completed(U32 status,
						   const std::string& reason,
						   const LLSD& content)
	{
		gPendingMetricsUploads--; // if we add retry, this should be moved to the isGoodStatus case.
		if (isGoodStatus(status))
		{
			LL_DEBUGS("Avatar") << "OK" << LL_ENDL;
			result(content);
		}
		else
		{
			LL_WARNS("Avatar") << "Failed " << status << " reason " << reason << LL_ENDL;
			errorWithContent(status,reason,content);
		}
	}

	// virtual
	void result(const LLSD & content)
	{
		if (mLiveSequence == mExpectedSequence)
		{
			mReportingStarted = true;
		}
	}

private:
	S32 mExpectedSequence;
	volatile const S32 & mLiveSequence;
	volatile bool & mReportingStarted;
};

bool LLVOAvatarSelf::updateAvatarRezMetrics(bool force_send)
{
	const F32 AV_METRICS_INTERVAL_QA = 30.0;
	F32 send_period = 300.0;
	if (gSavedSettings.getBOOL("QAModeMetrics"))
	{
		send_period = AV_METRICS_INTERVAL_QA;
	}

	if (force_send || mTimeSinceLastRezMessage.getElapsedTimeF32() > send_period)
	{
		// Stats for completed phases have been getting logged as they
		// complete.  This will give us stats for any timers that
		// haven't finished as of the metric's being sent.
		
		if (force_send)
		{
			LLVOAvatar::logPendingPhasesAllAvatars();
		}
		sendViewerAppearanceChangeMetrics();
	}

	return false;
}

void LLVOAvatarSelf::addMetricsTimerRecord(const LLSD& record)
{
	mPendingTimerRecords.push_back(record);
}

bool operator<(const LLSD& a, const LLSD& b)
{
	std::ostringstream aout, bout;
	aout << LLSDNotationStreamer(a);
	bout << LLSDNotationStreamer(b);
	std::string astring = aout.str();
	std::string bstring = bout.str();

	return astring < bstring;

}

// Given a vector of LLSD records, return an LLSD array of bucketed stats for val_field.
LLSD summarize_by_buckets(std::vector<LLSD> in_records,
						  std::vector<std::string> by_fields,
						  std::string val_field)
{
	LLSD result = LLSD::emptyArray();
	std::map<LLSD,LLViewerStats::StatsAccumulator> accum;
	for (std::vector<LLSD>::iterator in_record_iter = in_records.begin();
		 in_record_iter != in_records.end(); ++in_record_iter)
	{
		LLSD& record = *in_record_iter;
		LLSD key;
		for (std::vector<std::string>::iterator field_iter = by_fields.begin();
			 field_iter != by_fields.end(); ++field_iter)
		{
			const std::string& field = *field_iter;
			key[field] = record[field];
		}
		LLViewerStats::StatsAccumulator& stats = accum[key];
		F32 value = record[val_field].asReal();
		stats.push(value);
	}
	for (std::map<LLSD,LLViewerStats::StatsAccumulator>::iterator accum_it = accum.begin();
		 accum_it != accum.end(); ++accum_it)
	{
		LLSD out_record = accum_it->first;
		out_record["stats"] = accum_it->second.getData();
		result.append(out_record);
	}
	return result;
}

void LLVOAvatarSelf::sendViewerAppearanceChangeMetrics()
{
	// gAgentAvatarp->stopAllPhases();
	static volatile bool reporting_started(false);
	static volatile S32 report_sequence(0);

	LLSD msg; // = metricsData();
	msg["message"] = "ViewerAppearanceChangeMetrics";
	msg["session_id"] = gAgentSessionID;
	msg["agent_id"] = gAgentID;
	msg["sequence"] = report_sequence;
	msg["initial"] = !reporting_started;
	msg["break"] = false;
	msg["duration"] = mTimeSinceLastRezMessage.getElapsedTimeF32();

	// Status of our own rezzing.
	msg["rez_status"] = LLVOAvatar::rezStatusToString(getRezzedStatus());

	// Status of all nearby avs including ourself.
	msg["nearby"] = LLSD::emptyArray();
	std::vector<S32> rez_counts;
	LLVOAvatar::getNearbyRezzedStats(rez_counts);
	for (S32 rez_stat=0; rez_stat < rez_counts.size(); ++rez_stat)
	{
		std::string rez_status_name = LLVOAvatar::rezStatusToString(rez_stat);
		msg["nearby"][rez_status_name] = rez_counts[rez_stat];
	}

	//	std::vector<std::string> bucket_fields("timer_name","is_self","grid_x","grid_y","is_using_server_bake");
	std::vector<std::string> by_fields;
	by_fields.push_back("timer_name");
	by_fields.push_back("completed");
	by_fields.push_back("grid_x");
	by_fields.push_back("grid_y");
	by_fields.push_back("is_using_server_bakes");
	by_fields.push_back("is_self");
	by_fields.push_back("central_bake_version");
	LLSD summary = summarize_by_buckets(mPendingTimerRecords, by_fields, std::string("elapsed"));
	msg["timers"] = summary;

	mPendingTimerRecords.clear();

	// Update sequence number
	if (S32_MAX == ++report_sequence)
		report_sequence = 0;

	LL_DEBUGS("Avatar") << avString() << "message: " << ll_pretty_print_sd(msg) << LL_ENDL;
	std::string	caps_url;
	if (getRegion())
	{
		// runway - change here to activate.
		caps_url = getRegion()->getCapability("ViewerMetrics");
	}
	if (!caps_url.empty())
	{
		gPendingMetricsUploads++;
		LLCurlRequest::headers_t headers;
		LLHTTPClient::post(caps_url,
						   msg,
						   new ViewerAppearanceChangeMetricsResponder(report_sequence,
																	  report_sequence,
																	  reporting_started));
		mTimeSinceLastRezMessage.reset();
	}
}

class CheckAgentAppearanceServiceResponder: public LLHTTPClient::Responder
{
public:
	CheckAgentAppearanceServiceResponder()
	{
	}
	
	virtual ~CheckAgentAppearanceServiceResponder()
	{
	}

	/* virtual */ void result(const LLSD& content)
	{
		LL_DEBUGS("Avatar") << "status OK" << llendl;
	}

	// Error
	/*virtual*/ void errorWithContent(U32 status, const std::string& reason, const LLSD& content)
	{
		if (isAgentAvatarValid())
		{
			LL_DEBUGS("Avatar") << "failed, will rebake [status:"
					<< status << "]: " << content << llendl;
			forceAppearanceUpdate();
		}
	}	

	static void forceAppearanceUpdate()
	{
		// Trying to rebake immediately after crossing region boundary
		// seems to be failure prone; adding a delay factor. Yes, this
		// fix is ad-hoc and not guaranteed to work in all cases.
		doAfterInterval(boost::bind(&LLVOAvatarSelf::forceBakeAllTextures,
									gAgentAvatarp.get(), true), 5.0);
	}
};

void LLVOAvatarSelf::checkForUnsupportedServerBakeAppearance()
{
	// Need to check only if we have a server baked appearance and are
	// in a non-baking region.
	if (!gAgentAvatarp->isUsingServerBakes())
		return;
	if (!gAgent.getRegion() || gAgent.getRegion()->getCentralBakeVersion()!=0)
		return;

	// if baked image service is unknown, need to refresh.
	if (LLAppearanceMgr::instance().getAppearanceServiceURL().empty())
	{
		CheckAgentAppearanceServiceResponder::forceAppearanceUpdate();
	}
	// query baked image service to check status.
	std::string image_url = gAgentAvatarp->getImageURL(TEX_HEAD_BAKED,
													   getTE(TEX_HEAD_BAKED)->getID());
	LLHTTPClient::head(image_url, new CheckAgentAppearanceServiceResponder);
}

const LLUUID& LLVOAvatarSelf::grabBakedTexture(EBakedTextureIndex baked_index) const
{
	if (canGrabBakedTexture(baked_index))
	{
		ETextureIndex tex_index = LLAvatarAppearanceDictionary::bakedToLocalTextureIndex(baked_index);
		if (tex_index == TEX_NUM_INDICES)
		{
			return LLUUID::null;
		}
		return getTEImage( tex_index )->getID();
	}
	return LLUUID::null;
}

BOOL LLVOAvatarSelf::canGrabBakedTexture(EBakedTextureIndex baked_index) const
{
	ETextureIndex tex_index = LLAvatarAppearanceDictionary::bakedToLocalTextureIndex(baked_index);
	if (tex_index == TEX_NUM_INDICES)
	{
		return FALSE;
	}
	// Check if the texture hasn't been baked yet.
	if (!isTextureDefined(tex_index, 0))
	{
		lldebugs << "getTEImage( " << (U32) tex_index << " )->getID() == IMG_DEFAULT_AVATAR" << llendl;
		return FALSE;
	}

	if (gAgent.isGodlikeWithoutAdminMenuFakery())
		return TRUE;

	// Check permissions of textures that show up in the
	// baked texture.  We don't want people copying people's
	// work via baked textures.

	const LLAvatarAppearanceDictionary::BakedEntry *baked_dict = LLAvatarAppearanceDictionary::getInstance()->getBakedTexture(baked_index);
	for (texture_vec_t::const_iterator iter = baked_dict->mLocalTextures.begin();
		 iter != baked_dict->mLocalTextures.end();
		 ++iter)
	{
		const ETextureIndex t_index = (*iter);
		LLWearableType::EType wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(t_index);
		U32 count = gAgentWearables.getWearableCount(wearable_type);
		lldebugs << "Checking index " << (U32) t_index << " count: " << count << llendl;
		
		for (U32 wearable_index = 0; wearable_index < count; ++wearable_index)
		{
			LLViewerWearable *wearable = gAgentWearables.getViewerWearable(wearable_type, wearable_index);
			if (wearable)
			{
				const LLLocalTextureObject *texture = wearable->getLocalTextureObject((S32)t_index);
				const LLUUID& texture_id = texture->getID();
				if (texture_id != IMG_DEFAULT_AVATAR)
				{
					// Search inventory for this texture.
					LLViewerInventoryCategory::cat_array_t cats;
					LLViewerInventoryItem::item_array_t items;
					LLAssetIDMatches asset_id_matches(texture_id);
					gInventory.collectDescendentsIf(LLUUID::null,
													cats,
													items,
													LLInventoryModel::INCLUDE_TRASH,
													asset_id_matches);

					BOOL can_grab = FALSE;
					lldebugs << "item count for asset " << texture_id << ": " << items.count() << llendl;
					if (items.count())
					{
						// search for full permissions version
						for (S32 i = 0; i < items.count(); i++)
						{
							LLViewerInventoryItem* itemp = items[i];
												if (itemp->getIsFullPerm())
							{
								can_grab = TRUE;
								break;
							}
						}
					}
					if (!can_grab) return FALSE;
				}
			}
		}
	}

	return TRUE;
}

void LLVOAvatarSelf::addLocalTextureStats( ETextureIndex type, LLViewerFetchedTexture* imagep,
										   F32 texel_area_ratio, BOOL render_avatar, BOOL covered_by_baked)
{
	if (!isIndexLocalTexture(type)) return;

	// Sunshine - ignoring covered_by_baked will force local textures
	// to always load.  Fix for SH-4001 and many related issues.  Do
	// not restore this without some more targetted fix for the local
	// textures failing to load issue.
	//if (!covered_by_baked)
	{
		if (imagep->getID() != IMG_DEFAULT_AVATAR)
		{
			imagep->setNoDelete();
			if (imagep->getDiscardLevel() != 0)
			{
				F32 desired_pixels;
				desired_pixels = llmin(mPixelArea, (F32)getTexImageArea());
				
				imagep->setBoostLevel(getAvatarBoostLevel());
				imagep->setAdditionalDecodePriority(SELF_ADDITIONAL_PRI) ;
				imagep->resetTextureStats();
				imagep->setMaxVirtualSizeResetInterval(MAX_TEXTURE_VIRTURE_SIZE_RESET_INTERVAL);
				imagep->addTextureStats( desired_pixels / texel_area_ratio );
				imagep->forceUpdateBindStats() ;
				if (imagep->getDiscardLevel() < 0)
				{
					mHasGrey = TRUE; // for statistics gathering
				}
			}
		}
		else
		{
			// texture asset is missing
			mHasGrey = TRUE; // for statistics gathering
		}
	}
}

LLLocalTextureObject* LLVOAvatarSelf::getLocalTextureObject(LLAvatarAppearanceDefines::ETextureIndex i, U32 wearable_index) const
{
	LLWearableType::EType type = LLAvatarAppearanceDictionary::getInstance()->getTEWearableType(i);
	LLViewerWearable* wearable = gAgentWearables.getViewerWearable(type, wearable_index);
	if (wearable)
	{
		return wearable->getLocalTextureObject(i);
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// getBakedTE()
// Used by the LayerSet.  (Layer sets don't in general know what textures depend on them.)
//-----------------------------------------------------------------------------
ETextureIndex LLVOAvatarSelf::getBakedTE( const LLViewerTexLayerSet* layerset ) const
{
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		if (layerset == mBakedTextureDatas[i].mTexLayerSet )
		{
			return mBakedTextureDatas[i].mTextureIndex;
		}
	}
	llassert(0);
	return TEX_HEAD_BAKED;
}


void LLVOAvatarSelf::setNewBakedTexture(LLAvatarAppearanceDefines::EBakedTextureIndex i, const LLUUID &uuid)
{
	ETextureIndex index = LLAvatarAppearanceDictionary::bakedToLocalTextureIndex(i);
	setNewBakedTexture(index, uuid);
}


//-----------------------------------------------------------------------------
// setNewBakedTexture()
// A new baked texture has been successfully uploaded and we can start using it now.
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::setNewBakedTexture( ETextureIndex te, const LLUUID& uuid )
{
	// Baked textures live on other sims.
	LLHost target_host = getObjectHost();	
	setTEImage( te, LLViewerTextureManager::getFetchedTextureFromHost( uuid, FTT_HOST_BAKE, target_host ) );
	updateMeshTextures();
	dirtyMesh();

	LLVOAvatar::cullAvatarsByPixelArea();

	/* switch(te)
		case TEX_HEAD_BAKED:
			llinfos << "New baked texture: HEAD" << llendl; */
	const LLAvatarAppearanceDictionary::TextureEntry *texture_dict = LLAvatarAppearanceDictionary::getInstance()->getTexture(te);
	if (texture_dict->mIsBakedTexture)
	{
		debugBakedTextureUpload(texture_dict->mBakedTextureIndex, TRUE); // FALSE for start of upload, TRUE for finish.
		llinfos << "New baked texture: " << texture_dict->mName << " UUID: " << uuid <<llendl;
	}
	else
	{
		llwarns << "New baked texture: unknown te " << te << llendl;
	}
	
	//	dumpAvatarTEs( "setNewBakedTexture() send" );
	// RN: throttle uploads
	if (!hasPendingBakedUploads())
	{
		gAgent.sendAgentSetAppearance();

		if (gSavedSettings.getBOOL("DebugAvatarRezTime"))
		{
			LLSD args;
			args["EXISTENCE"] = llformat("%d",(U32)mDebugExistenceTimer.getElapsedTimeF32());
			args["TIME"] = llformat("%d",(U32)mDebugSelfLoadTimer.getElapsedTimeF32());
			if (isAllLocalTextureDataFinal())
			{
				LLNotificationsUtil::add("AvatarRezSelfBakedDoneNotification",args);
				LL_DEBUGS("Avatar") << "REZTIME: [ " << (U32)mDebugExistenceTimer.getElapsedTimeF32()
						<< "sec ]"
						<< avString() 
						<< "RuthTimer " << (U32)mRuthDebugTimer.getElapsedTimeF32()
						<< " SelfLoadTimer " << (U32)mDebugSelfLoadTimer.getElapsedTimeF32()
						<< " Notification " << "AvatarRezSelfBakedDoneNotification"
						<< llendl;
			}
			else
			{
				args["STATUS"] = debugDumpAllLocalTextureDataInfo();
				LLNotificationsUtil::add("AvatarRezSelfBakedUpdateNotification",args);
				LL_DEBUGS("Avatar") << "REZTIME: [ " << (U32)mDebugExistenceTimer.getElapsedTimeF32()
						<< "sec ]"
						<< avString() 
						<< "RuthTimer " << (U32)mRuthDebugTimer.getElapsedTimeF32()
						<< " SelfLoadTimer " << (U32)mDebugSelfLoadTimer.getElapsedTimeF32()
						<< " Notification " << "AvatarRezSelfBakedUpdateNotification"
						<< llendl;
			}
		}

		outputRezDiagnostics();
	}
}

// FIXME: This is not called consistently. Something may be broken.
void LLVOAvatarSelf::outputRezDiagnostics() const
{
	if(!gSavedSettings.getBOOL("DebugAvatarLocalTexLoadedTime"))
	{
		return ;
	}

	const F32 final_time = mDebugSelfLoadTimer.getElapsedTimeF32();
	LL_DEBUGS("Avatar") << "REZTIME: Myself rez stats:" << llendl;
	LL_DEBUGS("Avatar") << "\t Time from avatar creation to load wearables: " << (S32)mDebugTimeWearablesLoaded << llendl;
	LL_DEBUGS("Avatar") << "\t Time from avatar creation to de-cloud: " << (S32)mDebugTimeAvatarVisible << llendl;
	LL_DEBUGS("Avatar") << "\t Time from avatar creation to de-cloud for others: " << (S32)final_time << llendl;
	LL_DEBUGS("Avatar") << "\t Load time for each texture: " << llendl;
	for (U32 i = 0; i < LLAvatarAppearanceDefines::TEX_NUM_INDICES; ++i)
	{
		std::stringstream out;
		out << "\t\t (" << i << ") ";
		U32 j=0;
		for (j=0; j <= MAX_DISCARD_LEVEL; j++)
		{
			out << "\t";
			S32 load_time = (S32)mDebugTextureLoadTimes[i][j];
			if (load_time == -1)
			{
				out << "*";
				if (j == 0)
					break;
			}
			else
			{
				out << load_time;
			}
		}

		// Don't print out non-existent textures.
		if (j != 0)
		{
			LL_DEBUGS("Avatar") << out.str() << LL_ENDL;
		}
	}
	LL_DEBUGS("Avatar") << "\t Time points for each upload (start / finish)" << llendl;
	for (U32 i = 0; i < LLAvatarAppearanceDefines::BAKED_NUM_INDICES; ++i)
	{
		LL_DEBUGS("Avatar") << "\t\t (" << i << ") \t" << (S32)mDebugBakedTextureTimes[i][0] << " / " << (S32)mDebugBakedTextureTimes[i][1] << llendl;
	}

	for (LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::BakedTextures::const_iterator baked_iter = LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().begin();
		 baked_iter != LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::getInstance()->getBakedTextures().end();
		 ++baked_iter)
	{
		const LLAvatarAppearanceDefines::EBakedTextureIndex baked_index = baked_iter->first;
		const LLViewerTexLayerSet *layerset = debugGetLayerSet(baked_index);
		if (!layerset) continue;
		const LLViewerTexLayerSetBuffer *layerset_buffer = layerset->getViewerComposite();
		if (!layerset_buffer) continue;
		LL_DEBUGS("Avatar") << layerset_buffer->dumpTextureInfo() << llendl;
	}

	dumpAllTextures();
}

void LLVOAvatarSelf::outputRezTiming(const std::string& msg) const
{
	LL_DEBUGS("Avatar")
		<< avString()
		<< llformat("%s. Time from avatar creation: %.2f", msg.c_str(), mDebugSelfLoadTimer.getElapsedTimeF32())
		<< LL_ENDL;
}

void LLVOAvatarSelf::reportAvatarRezTime() const
{
	// TODO: report mDebugSelfLoadTimer.getElapsedTimeF32() somehow.
}

//-----------------------------------------------------------------------------
// setCachedBakedTexture()
// A baked texture id was received from a cache query, make it active
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::setCachedBakedTexture( ETextureIndex te, const LLUUID& uuid )
{
	setTETexture( te, uuid );

	/* switch(te)
		case TEX_HEAD_BAKED:
			if( mHeadLayerSet )
				mHeadLayerSet->cancelUpload(); */
	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		LLViewerTexLayerSet *layerset = getTexLayerSet(i);
		if ( mBakedTextureDatas[i].mTextureIndex == te && layerset)
		{
			if (mInitialBakeIDs[i] != LLUUID::null)
			{
				if (mInitialBakeIDs[i] == uuid)
				{
					llinfos << "baked texture correctly loaded at login! " << i << llendl;
				}
				else
				{
					llwarns << "baked texture does not match id loaded at login!" << i << llendl;
				}
				mInitialBakeIDs[i] = LLUUID::null;
			}
			layerset->cancelUpload();
		}
	}
}

// static
void LLVOAvatarSelf::processRebakeAvatarTextures(LLMessageSystem* msg, void**)
{
	LLUUID texture_id;
	msg->getUUID("TextureData", "TextureID", texture_id);
	if (!isAgentAvatarValid()) return;

	// If this is a texture corresponding to one of our baked entries, 
	// just rebake that layer set.
	BOOL found = FALSE;

	/* ETextureIndex baked_texture_indices[BAKED_NUM_INDICES] =
			TEX_HEAD_BAKED,
			TEX_UPPER_BAKED, */
	for (LLAvatarAppearanceDictionary::Textures::const_iterator iter = LLAvatarAppearanceDictionary::getInstance()->getTextures().begin();
		 iter != LLAvatarAppearanceDictionary::getInstance()->getTextures().end();
		 ++iter)
	{
		const ETextureIndex index = iter->first;
		const LLAvatarAppearanceDictionary::TextureEntry *texture_dict = iter->second;
		if (texture_dict->mIsBakedTexture)
		{
			if (texture_id == gAgentAvatarp->getTEImage(index)->getID())
			{
				LLViewerTexLayerSet* layer_set = gAgentAvatarp->getLayerSet(index);
				if (layer_set)
				{
					llinfos << "TAT: rebake - matched entry " << (S32)index << llendl;
					gAgentAvatarp->invalidateComposite(layer_set, TRUE);
					found = TRUE;
					LLViewerStats::getInstance()->incStat(LLViewerStats::ST_TEX_REBAKES);
				}
			}
		}
	}

	// If texture not found, rebake all entries.
	if (!found)
	{
		gAgentAvatarp->forceBakeAllTextures();
	}
	else
	{
		// Not sure if this is necessary, but forceBakeAllTextures() does it.
		gAgentAvatarp->updateMeshTextures();
	}
}


void LLVOAvatarSelf::forceBakeAllTextures(bool slam_for_debug)
{
	llinfos << "TAT: forced full rebake. " << llendl;

	for (U32 i = 0; i < mBakedTextureDatas.size(); i++)
	{
		ETextureIndex baked_index = mBakedTextureDatas[i].mTextureIndex;
		LLViewerTexLayerSet* layer_set = getLayerSet(baked_index);
		if (layer_set)
		{
			if (slam_for_debug)
			{
				layer_set->setUpdatesEnabled(TRUE);
				layer_set->cancelUpload();
			}

			invalidateComposite(layer_set, TRUE);
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_TEX_REBAKES);
		}
		else
		{
			llwarns << "TAT: NO LAYER SET FOR " << (S32)baked_index << llendl;
		}
	}

	// Don't know if this is needed
	updateMeshTextures();
}

//-----------------------------------------------------------------------------
// requestLayerSetUpdate()
//-----------------------------------------------------------------------------
void LLVOAvatarSelf::requestLayerSetUpdate(ETextureIndex index )
{
	/* switch(index)
		case LOCTEX_UPPER_BODYPAINT:  
		case LOCTEX_UPPER_SHIRT:
			if( mUpperBodyLayerSet )
				mUpperBodyLayerSet->requestUpdate(); */
	const LLAvatarAppearanceDictionary::TextureEntry *texture_dict = LLAvatarAppearanceDictionary::getInstance()->getTexture(index);
	if (!texture_dict->mIsLocalTexture || !texture_dict->mIsUsedByBakedTexture)
		return;
	const EBakedTextureIndex baked_index = texture_dict->mBakedTextureIndex;
	if (mBakedTextureDatas[baked_index].mTexLayerSet)
	{
		mBakedTextureDatas[baked_index].mTexLayerSet->requestUpdate();
	}
}

LLViewerTexLayerSet* LLVOAvatarSelf::getLayerSet(ETextureIndex index) const
{
       /* switch(index)
               case TEX_HEAD_BAKED:
               case TEX_HEAD_BODYPAINT:
                       return mHeadLayerSet; */
       const LLAvatarAppearanceDictionary::TextureEntry *texture_dict = LLAvatarAppearanceDictionary::getInstance()->getTexture(index);
       if (texture_dict->mIsUsedByBakedTexture)
       {
               const EBakedTextureIndex baked_index = texture_dict->mBakedTextureIndex;
               return getLayerSet(baked_index);
       }
       return NULL;
}

LLViewerTexLayerSet* LLVOAvatarSelf::getLayerSet(EBakedTextureIndex baked_index) const
{
       /* switch(index)
               case TEX_HEAD_BAKED:
               case TEX_HEAD_BODYPAINT:
                       return mHeadLayerSet; */
       if (baked_index >= 0 && baked_index < BAKED_NUM_INDICES)
       {
		   return  getTexLayerSet(baked_index);
       }
       return NULL;
}




// static
void LLVOAvatarSelf::onCustomizeStart(bool disable_camera_switch)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->mIsEditingAppearance = true;
		gAgentAvatarp->mUseLocalAppearance = true;

		if (gSavedSettings.getBOOL("AppearanceCameraMovement") && !disable_camera_switch)
		{
			gAgentCamera.changeCameraToCustomizeAvatar();
		}

#if 0
		gAgentAvatarp->clearVisualParamWeights();
		gAgentAvatarp->idleUpdateAppearanceAnimation();
#endif
		
		gAgentAvatarp->invalidateAll(); // mark all bakes as dirty, request updates
		gAgentAvatarp->updateMeshTextures(); // make sure correct textures are applied to the avatar mesh.
		gAgentAvatarp->updateTextures(); // call updateTextureStats
	}
}

// static
void LLVOAvatarSelf::onCustomizeEnd(bool disable_camera_switch)
{

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->mIsEditingAppearance = false;
		if (gAgentAvatarp->getRegion() && !gAgentAvatarp->getRegion()->getCentralBakeVersion())
		{
			// FIXME DRANO - move to sendAgentSetAppearance, make conditional on upload complete.
			gAgentAvatarp->mUseLocalAppearance = false;
		}

		gAgentAvatarp->invalidateAll();

		if (gSavedSettings.getBOOL("AppearanceCameraMovement") && !disable_camera_switch)
		{
			gAgentCamera.changeCameraToDefault();
			gAgentCamera.resetView();
		}
	
		LLAppearanceMgr::instance().updateAppearanceFromCOF();	
	}
}

// HACK: this will null out the avatar's local texture IDs before the TE message is sent
//       to ensure local texture IDs are not sent to other clients in the area.
//       this is a short-term solution. The long term solution will be to not set the texture
//       IDs in the avatar object, and keep them only in the wearable.
//       This will involve further refactoring that is too risky for the initial release of 2.0.
bool LLVOAvatarSelf::sendAppearanceMessage(LLMessageSystem *mesgsys) const
{
	LLUUID texture_id[TEX_NUM_INDICES];
	// pack away current TEs to make sure we don't send them out
	for (LLAvatarAppearanceDictionary::Textures::const_iterator iter = LLAvatarAppearanceDictionary::getInstance()->getTextures().begin();
		 iter != LLAvatarAppearanceDictionary::getInstance()->getTextures().end();
		 ++iter)
	{
		const ETextureIndex index = iter->first;
		const LLAvatarAppearanceDictionary::TextureEntry *texture_dict = iter->second;
		if (!texture_dict->mIsBakedTexture)
		{
			LLTextureEntry* entry = getTE((U8) index);
			texture_id[index] = entry->getID();
			entry->setID(IMG_DEFAULT_AVATAR);
		}
	}

	bool success = packTEMessage(mesgsys);

	// unpack TEs to make sure we don't re-trigger a bake
	for (LLAvatarAppearanceDictionary::Textures::const_iterator iter = LLAvatarAppearanceDictionary::getInstance()->getTextures().begin();
		 iter != LLAvatarAppearanceDictionary::getInstance()->getTextures().end();
		 ++iter)
	{
		const ETextureIndex index = iter->first;
		const LLAvatarAppearanceDictionary::TextureEntry *texture_dict = iter->second;
		if (!texture_dict->mIsBakedTexture)
		{
			LLTextureEntry* entry = getTE((U8) index);
			entry->setID(texture_id[index]);
		}
	}

	return success;
}


//------------------------------------------------------------------------
// needsRenderBeam()
//------------------------------------------------------------------------
BOOL LLVOAvatarSelf::needsRenderBeam()
{
	LLTool *tool = LLToolMgr::getInstance()->getCurrentTool();

	BOOL is_touching_or_grabbing = (tool == LLToolGrab::getInstance() && LLToolGrab::getInstance()->isEditing());
	if (LLToolGrab::getInstance()->getEditingObject() && 
		LLToolGrab::getInstance()->getEditingObject()->isAttachment())
	{
		// don't render selection beam on hud objects
		is_touching_or_grabbing = FALSE;
	}
	return is_touching_or_grabbing || (mState & AGENT_STATE_EDITING && LLSelectMgr::getInstance()->shouldShowSelection());
}

// static
void LLVOAvatarSelf::deleteScratchTextures()
{
	for( LLGLuint* namep = sScratchTexNames.getFirstData(); 
		 namep; 
		 namep = sScratchTexNames.getNextData() )
	{
		LLImageGL::deleteTextures(1, (U32 *)namep );
		stop_glerror();
	}

	if( sScratchTexBytes )
	{
		lldebugs << "Clearing Scratch Textures " << (sScratchTexBytes/1024) << "KB" << llendl;

		sScratchTexNames.deleteAllData();
		sScratchTexLastBindTime.deleteAllData();
		LLImageGL::sGlobalTextureMemoryInBytes -= sScratchTexBytes;
		sScratchTexBytes = 0;
	}
}

// static 
void LLVOAvatarSelf::dumpScratchTextureByteCount()
{
	llinfos << "Scratch Texture GL: " << (sScratchTexBytes/1024) << "KB" << llendl;
}

void LLVOAvatarSelf::dumpWearableInfo(LLAPRFile& outfile)
{
	apr_file_t* file = outfile.getFileHandle();
	if (!file)
	{
		return;
	}

	
	apr_file_printf( file, "\n<wearable_info>\n" );

	LLWearableData *wd = getWearableData();
	for (S32 type = 0; type < LLWearableType::WT_COUNT; type++)
	{
		const std::string& type_name = LLWearableType::getTypeName((LLWearableType::EType)type);
		for (U32 j=0; j< wd->getWearableCount((LLWearableType::EType)type); j++)
		{
			LLViewerWearable *wearable = gAgentWearables.getViewerWearable((LLWearableType::EType)type,j);
			apr_file_printf( file, "\n\t    <wearable type=\"%s\" name=\"%s\"/>\n",
							 type_name.c_str(), wearable->getName().c_str() );
			LLWearable::visual_param_vec_t v_params;
			wearable->getVisualParams(v_params);
			for (LLWearable::visual_param_vec_t::iterator it = v_params.begin();
				 it != v_params.end(); ++it)
			{
				LLVisualParam *param = *it;
				dump_visual_param(file, param, param->getWeight());
			}
		}
	}
	apr_file_printf( file, "\n</wearable_info>\n" );
}
