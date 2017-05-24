// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llpanelvolume.cpp
 * @brief Object editing (position, scale, etc.) in the tools floater
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

// file include
#include "llpanelvolume.h"

// linden library includes
#include "llclickaction.h"
#include "lleconomy.h"
#include "llerror.h"
#include "llfontgl.h"
#include "llflexibleobject.h"
#include "llmaterialtable.h"
#include "llpermissionsflags.h"
#include "llstring.h"
#include "llvolume.h"
#include "m3math.h"
#include "material_codes.h"

// project includes
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "lltexturectrl.h"
#include "llcombobox.h"
//#include "llfirstuse.h"
#include "llfocusmgr.h"
#include "llmanipscale.h"
#include "llpreviewscript.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltool.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "llworld.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llnotificationsutil.h"

#include "lldrawpool.h"

// For mesh physics
#include "llagent.h"
#include "llviewercontrol.h"
#include "llmeshrepository.h"

// "Features" Tab
LLPanelVolume::LLPanelVolume()
	: LLPanel(),
	mComboMaterialItemCount(0)
{
	setMouseOpaque(FALSE);

}


LLPanelVolume::~LLPanelVolume()
{
	// Children all cleaned up by default view destructor.
}


BOOL	LLPanelVolume::postBuild()
{
	mLabelSelectSingleMessage = getChild<LLTextBox>("select_single");
	mLabelEditObject = getChild<LLTextBox>("edit_object");

	// Flexible Objects Parameters
	{
		mCheckFlexible1D = getChild<LLCheckBoxCtrl>("Flexible1D Checkbox Ctrl");
		mCheckFlexible1D->setCommitCallback(boost::bind(&LLPanelVolume::onCommitIsFlexible, this));

		mFlexSections = getChild<LLSpinCtrl>("FlexNumSections");
		mFlexSections->setCommitCallback(boost::bind(&LLPanelVolume::onCommitFlexible, this));

		mFlexGravity = getChild<LLSpinCtrl>("FlexGravity");
		mFlexGravity->setCommitCallback(boost::bind(&LLPanelVolume::onCommitFlexible, this));

		mFlexFriction = getChild<LLSpinCtrl>("FlexFriction");
		mFlexFriction->setCommitCallback(boost::bind(&LLPanelVolume::onCommitFlexible, this));

		mFlexWind = getChild<LLSpinCtrl>("FlexWind");
		mFlexWind->setCommitCallback(boost::bind(&LLPanelVolume::onCommitFlexible, this));

		mFlexTension = getChild<LLSpinCtrl>("FlexTension");
		mFlexTension->setCommitCallback(boost::bind(&LLPanelVolume::onCommitFlexible, this));

		mFlexForce[VX] = getChild<LLSpinCtrl>("FlexForceX");
		mFlexForce[VX]->setCommitCallback(boost::bind(&LLPanelVolume::onCommitFlexible, this));

		mFlexForce[VY] = getChild<LLSpinCtrl>("FlexForceY");
		mFlexForce[VY]->setCommitCallback(boost::bind(&LLPanelVolume::onCommitFlexible, this));

		mFlexForce[VZ] = getChild<LLSpinCtrl>("FlexForceZ");
		mFlexForce[VZ]->setCommitCallback(boost::bind(&LLPanelVolume::onCommitFlexible, this));
	}

	// LIGHT Parameters
	{
		mCheckLight = getChild<LLCheckBoxCtrl>("Light Checkbox Ctrl");
		mCheckLight->setCommitCallback(boost::bind(&LLPanelVolume::onCommitIsLight, this));

		mLightColorSwatch = getChild<LLColorSwatchCtrl>("colorswatch");
		mLightColorSwatch->setOnCancelCallback(boost::bind(&LLPanelVolume::onLightCancelColor, this, _2));
		mLightColorSwatch->setOnSelectCallback(boost::bind(&LLPanelVolume::onLightSelectColor, this, _2));
		mLightColorSwatch->setCommitCallback(boost::bind(&LLPanelVolume::onCommitLight, this));

		mLightTexturePicker = getChild<LLTextureCtrl>("light texture control");
		mLightTexturePicker->setOnCancelCallback(boost::bind(&LLPanelVolume::onLightCancelTexture, this, _2));
		mLightTexturePicker->setOnSelectCallback(boost::bind(&LLPanelVolume::onLightSelectTexture, this, _2));
		mLightTexturePicker->setCommitCallback(boost::bind(&LLPanelVolume::onCommitLight, this));

		mLightIntensity = getChild<LLSpinCtrl>("Light Intensity");
		mLightIntensity->setCommitCallback(boost::bind(&LLPanelVolume::onCommitLight, this));

		mLightRadius = getChild<LLSpinCtrl>("Light Radius");
		mLightRadius->setCommitCallback(boost::bind(&LLPanelVolume::onCommitLight, this));

		mLightFalloff = getChild<LLSpinCtrl>("Light Falloff");
		mLightFalloff->setCommitCallback(boost::bind(&LLPanelVolume::onCommitLight, this));

		mLightFOV = getChild<LLSpinCtrl>("Light FOV");
		mLightFOV->setCommitCallback(boost::bind(&LLPanelVolume::onCommitLight, this));

		mLightFocus = getChild<LLSpinCtrl>("Light Focus");
		mLightFocus->setCommitCallback(boost::bind(&LLPanelVolume::onCommitLight, this));

		mLightAmbiance = getChild<LLSpinCtrl>("Light Ambiance");
		mLightAmbiance->setCommitCallback(boost::bind(&LLPanelVolume::onCommitLight, this));
	}
	
	// PHYSICS Parameters
	{
		mLabelPhysicsShapeType = getChild<LLTextBox>("label physicsshapetype");

		// PhysicsShapeType combobox
		mComboPhysicsShapeType = getChild<LLComboBox>("Physics Shape Type Combo Ctrl");
		mComboPhysicsShapeType->setCommitCallback(boost::bind(&LLPanelVolume::sendPhysicsShapeType, this, _2));
	
		// PhysicsGravity
		mSpinPhysicsGravity = getChild<LLSpinCtrl>("Physics Gravity");
		mSpinPhysicsGravity->setCommitCallback(boost::bind(&LLPanelVolume::sendPhysicsGravity, this, _2));

		// PhysicsFriction
		mSpinPhysicsFriction = getChild<LLSpinCtrl>("Physics Friction");
		mSpinPhysicsFriction->setCommitCallback(boost::bind(&LLPanelVolume::sendPhysicsFriction, this, _2));

		// PhysicsDensity
		mSpinPhysicsDensity = getChild<LLSpinCtrl>("Physics Density");
		mSpinPhysicsDensity->setCommitCallback(boost::bind(&LLPanelVolume::sendPhysicsDensity, this, _2));

		// PhysicsRestitution
		mSpinPhysicsRestitution = getChild<LLSpinCtrl>("Physics Restitution");
		mSpinPhysicsRestitution->setCommitCallback(boost::bind(&LLPanelVolume::sendPhysicsRestitution, this, _2));
	}

	std::map<std::string, std::string> material_name_map;
	material_name_map["Stone"]= LLTrans::getString("Stone");
	material_name_map["Metal"]= LLTrans::getString("Metal");	
	material_name_map["Glass"]= LLTrans::getString("Glass");	
	material_name_map["Wood"]= LLTrans::getString("Wood");	
	material_name_map["Flesh"]= LLTrans::getString("Flesh");
	material_name_map["Plastic"]= LLTrans::getString("Plastic");
	material_name_map["Rubber"]= LLTrans::getString("Rubber");	
	material_name_map["Light"]= LLTrans::getString("Light");		
	
	LLMaterialTable::basic.initTableTransNames(material_name_map);

	// material type popup
	mComboMaterial = getChild<LLComboBox>("material");
	mComboMaterial->setCommitCallback(boost::bind(&LLPanelVolume::onCommitMaterial, this, _1));
	mComboMaterial->removeall();

	for (LLMaterialTable::info_list_t::iterator iter = LLMaterialTable::basic.mMaterialInfoList.begin();
		 iter != LLMaterialTable::basic.mMaterialInfoList.end(); ++iter)
	{
		LLMaterialInfo* minfop = *iter;
		if (minfop->mMCode != LL_MCODE_LIGHT)
		{
			mComboMaterial->add(minfop->mName);  
		}
	}
	mComboMaterialItemCount = mComboMaterial->getItemCount();
	
	// Start with everyone disabled
	clearCtrls();

	return TRUE;
}

void LLPanelVolume::getState( )
{
	LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
	LLViewerObject* root_objectp = objectp;
	if(!objectp)
	{
		objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
		// *FIX: shouldn't we just keep the child?
		if (objectp)
		{
			LLViewerObject* parentp = objectp->getRootEdit();

			if (parentp)
			{
				root_objectp = parentp;
			}
			else
			{
				root_objectp = objectp;
			}
		}
	}

	LLVOVolume *volobjp = nullptr;
	if ( objectp && (objectp->getPCode() == LL_PCODE_VOLUME))
	{
		volobjp = (LLVOVolume *)objectp;
	}
	
	if( !objectp )
	{
		//forfeit focus
		if (gFocusMgr.childHasKeyboardFocus(this))
		{
			gFocusMgr.setKeyboardFocus(nullptr);
		}

		// Disable all text input fields
		clearCtrls();

		return;
	}

	LLUUID owner_id;
	std::string owner_name;
	LLSelectMgr::getInstance()->selectGetOwner(owner_id, owner_name);

	// BUG? Check for all objects being editable?
	BOOL editable = root_objectp->permModify() && !root_objectp->isPermanentEnforced();
	BOOL single_volume = LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME )
		&& LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1;

	// Select Single Message
	if (single_volume)
	{
		mLabelEditObject->setVisible(true);
		mLabelEditObject->setEnabled(true);
		mLabelSelectSingleMessage->setVisible(false);
	}
	else
	{	
		mLabelEditObject->setVisible(false);
		mLabelSelectSingleMessage->setVisible(true);
		mLabelSelectSingleMessage->setEnabled(true);
	}
	
	// Light properties
	BOOL is_light = volobjp && volobjp->getIsLight();
	mCheckLight->setValue(is_light);
	mCheckLight->setEnabled(editable && single_volume && volobjp);
	
	if (is_light && editable && single_volume)
	{
		//mLabelColor		 ->setEnabled( TRUE );

		mLightColorSwatch->setEnabled(TRUE);
		mLightColorSwatch->setValid(TRUE);
		mLightColorSwatch->set(volobjp->getLightBaseColor());

		mLightTexturePicker->setEnabled(TRUE);
		mLightTexturePicker->setValid(TRUE);
		mLightTexturePicker->setImageAssetID(volobjp->getLightTextureID());

		mLightIntensity->setEnabled(true);
		mLightRadius->setEnabled(true);
		mLightFalloff->setEnabled(true);

		mLightFOV->setEnabled(true);
		mLightFocus->setEnabled(true);
		mLightAmbiance->setEnabled(true);
		
		mLightIntensity->setValue(volobjp->getLightIntensity());
		mLightRadius->setValue(volobjp->getLightRadius());
		mLightFalloff->setValue(volobjp->getLightFalloff());

		LLVector3 params = volobjp->getSpotLightParams();
		mLightFOV->setValue(params.mV[0]);
		mLightFocus->setValue(params.mV[1]);
		mLightAmbiance->setValue(params.mV[2]);

		mLightSavedColor = volobjp->getLightColor();
	}
	else
	{
		mLightIntensity->clear();
		mLightRadius->clear();
		mLightFalloff->clear();

		mLightFOV->clear();
		mLightFocus->clear();
		mLightAmbiance->clear();

		mLightColorSwatch->setEnabled(FALSE);
		mLightColorSwatch->setValid(FALSE);

		mLightTexturePicker->setEnabled(FALSE);
		mLightTexturePicker->setValid(FALSE);

		mLightIntensity->setEnabled(false);
		mLightRadius->setEnabled(false);
		mLightFalloff->setEnabled(false);

		mLightFOV->setEnabled(false);
		mLightFocus->setEnabled(false);
		mLightAmbiance->setEnabled(false);
	}
	
	// Flexible properties
	BOOL is_flexible = volobjp && volobjp->isFlexible();
	mCheckFlexible1D->setValue(is_flexible);
	if (is_flexible || (volobjp && volobjp->canBeFlexible()))
	{
		mCheckFlexible1D->setEnabled(editable && single_volume && volobjp && !volobjp->isMesh() && !objectp->isPermanentEnforced());
	}
	else
	{
		mCheckFlexible1D->setEnabled(false);
	}
	if (is_flexible && editable && single_volume)
	{
		mFlexSections->setVisible(true);
		mFlexGravity->setVisible(true);
		mFlexTension->setVisible(true);
		mFlexFriction->setVisible(true);
		mFlexWind->setVisible(true);
		mFlexForce[VX]->setVisible(true);
		mFlexForce[VY]->setVisible(true);
		mFlexForce[VZ]->setVisible(true);

		mFlexSections->setEnabled(true);
		mFlexGravity->setEnabled(true);
		mFlexTension->setEnabled(true);
		mFlexFriction->setEnabled(true);
		mFlexWind->setEnabled(true);
		mFlexForce[VX]->setEnabled(true);
		mFlexForce[VY]->setEnabled(true);
		mFlexForce[VZ]->setEnabled(true);

		LLFlexibleObjectData *attributes = (LLFlexibleObjectData *)objectp->getParameterEntry(LLNetworkData::PARAMS_FLEXIBLE);
		
		mFlexSections->setValue((F32)attributes->getSimulateLOD());
		mFlexGravity->setValue(attributes->getGravity());
		mFlexTension->setValue(attributes->getTension());
		mFlexFriction->setValue(attributes->getAirFriction());
		mFlexWind->setValue(attributes->getWindSensitivity());
		mFlexForce[VX]->setValue(attributes->getUserForce().mV[VX]);
		mFlexForce[VY]->setValue(attributes->getUserForce().mV[VY]);
		mFlexForce[VZ]->setValue(attributes->getUserForce().mV[VZ]);
	}
	else
	{
		mFlexSections->clear();
		mFlexGravity->clear();
		mFlexTension->clear();
		mFlexFriction->clear();
		mFlexWind->clear();
		mFlexForce[VX]->clear();
		mFlexForce[VY]->clear();
		mFlexForce[VZ]->clear();

		mFlexSections->setEnabled(false);
		mFlexGravity->setEnabled(false);
		mFlexTension->setEnabled(false);
		mFlexFriction->setEnabled(false);
		mFlexWind->setEnabled(false);
		mFlexForce[VX]->setEnabled(false);
		mFlexForce[VY]->setEnabled(false);
		mFlexForce[VZ]->setEnabled(false);
	}
	
	// Material properties

	// Update material part
	// slightly inefficient - materials are unique per object, not per TE
	U8 material_code = 0;
	struct f : public LLSelectedTEGetFunctor<U8>
	{
		U8 get(LLViewerObject* object, S32 te) override
		{
			return object->getMaterial();
		}
	} func;
	bool material_same = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, material_code );
	std::string LEGACY_FULLBRIGHT_DESC = LLTrans::getString("Fullbright");
	if (editable && single_volume && material_same)
	{
		mComboMaterial->setEnabled( TRUE );
		if (material_code == LL_MCODE_LIGHT)
		{
			if (mComboMaterial->getItemCount() == mComboMaterialItemCount)
			{
				mComboMaterial->add(LEGACY_FULLBRIGHT_DESC);
			}
			mComboMaterial->setSimple(LEGACY_FULLBRIGHT_DESC);
		}
		else
		{
			if (mComboMaterial->getItemCount() != mComboMaterialItemCount)
			{
				mComboMaterial->remove(LEGACY_FULLBRIGHT_DESC);
			}
			
			mComboMaterial->setSimple(std::string(LLMaterialTable::basic.getName(material_code)));
		}
	}
	else
	{
		mComboMaterial->setEnabled( FALSE );
	}

	// Physics properties
	
	mSpinPhysicsGravity->set(objectp->getPhysicsGravity());
	mSpinPhysicsGravity->setEnabled(editable);

	mSpinPhysicsFriction->set(objectp->getPhysicsFriction());
	mSpinPhysicsFriction->setEnabled(editable);
	
	mSpinPhysicsDensity->set(objectp->getPhysicsDensity());
	mSpinPhysicsDensity->setEnabled(editable);
	
	mSpinPhysicsRestitution->set(objectp->getPhysicsRestitution());
	mSpinPhysicsRestitution->setEnabled(editable);

	// update the physics shape combo to include allowed physics shapes
	mComboPhysicsShapeType->removeall();
	mComboPhysicsShapeType->add(getString("None"), LLSD(1));

	BOOL isMesh = FALSE;
	LLSculptParams *sculpt_params = (LLSculptParams *)objectp->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
	if (sculpt_params)
	{
		U8 sculpt_type = sculpt_params->getSculptType();
		U8 sculpt_stitching = sculpt_type & LL_SCULPT_TYPE_MASK;
		isMesh = (sculpt_stitching == LL_SCULPT_TYPE_MESH);
	}

	if(isMesh && objectp)
	{
		const LLVolumeParams &volume_params = objectp->getVolume()->getParams();
		LLUUID mesh_id = volume_params.getSculptID();
		if(gMeshRepo.hasPhysicsShape(mesh_id))
		{
			// if a mesh contains an uploaded or decomposed physics mesh,
			// allow 'Prim'
			mComboPhysicsShapeType->add(getString("Prim"), LLSD(0));			
		}
	}
	else
	{
		// simple prims always allow physics shape prim
		mComboPhysicsShapeType->add(getString("Prim"), LLSD(0));	
	}

	mComboPhysicsShapeType->add(getString("Convex Hull"), LLSD(2));	
	mComboPhysicsShapeType->setValue(LLSD(objectp->getPhysicsShapeType()));
	mComboPhysicsShapeType->setEnabled(editable && !objectp->isPermanentEnforced() && ((root_objectp == nullptr) || !root_objectp->isPermanentEnforced()));

	mObject = objectp;
	mRootObject = root_objectp;
}

void LLPanelVolume::refresh()
{
	getState();
	if (mObject.notNull() && mObject->isDead())
	{
		mObject = nullptr;
	}

	if (mRootObject.notNull() && mRootObject->isDead())
	{
		mRootObject = nullptr;
	}

	BOOL visible = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_DEFERRED) > 0 ? TRUE : FALSE;

	mLightFOV->setVisible( visible);
	mLightFocus->setVisible( visible);
	mLightAmbiance->setVisible( visible);
	mLightTexturePicker->setVisible( visible);

	bool enable_mesh = false;

	LLSD sim_features;
	LLViewerRegion *region = gAgent.getRegion();
	if(region)
	{
		LLSD sim_features;
		region->getSimulatorFeatures(sim_features);		 
		enable_mesh = sim_features.has("PhysicsShapeTypes");
	}
	mLabelPhysicsShapeType->setVisible(enable_mesh);
	mComboPhysicsShapeType->setVisible(enable_mesh);
	mSpinPhysicsGravity->setVisible(enable_mesh);
	mSpinPhysicsFriction->setVisible(enable_mesh);
	mSpinPhysicsDensity->setVisible(enable_mesh);
	mSpinPhysicsRestitution->setVisible(enable_mesh);
	
    /* TODO: add/remove individual physics shape types as per the PhysicsShapeTypes simulator features */
}


void LLPanelVolume::draw()
{
	LLPanel::draw();
}

// virtual
void LLPanelVolume::clearCtrls()
{
	LLPanel::clearCtrls();

	mLabelSelectSingleMessage->setEnabled(false);
	mLabelSelectSingleMessage->setVisible(true);
	mLabelEditObject->setEnabled(false);
	mLabelEditObject->setVisible(false);
	mCheckLight->setEnabled(false);

	mLightColorSwatch->setEnabled(FALSE);
	mLightColorSwatch->setValid(FALSE);

	mLightTexturePicker->setEnabled(FALSE);
	mLightTexturePicker->setValid(FALSE);

	mLightIntensity->setEnabled(false);
	mLightRadius->setEnabled(false);
	mLightFalloff->setEnabled(false);

	mCheckFlexible1D->setEnabled(false);
	mFlexSections->setEnabled(false);
	mFlexGravity->setEnabled(false);
	mFlexTension->setEnabled(false);
	mFlexFriction->setEnabled(false);
	mFlexWind->setEnabled(false);
	mFlexForce[VX]->setEnabled(false);
	mFlexForce[VY]->setEnabled(false);
	mFlexForce[VZ]->setEnabled(false);

	mSpinPhysicsGravity->setEnabled(FALSE);
	mSpinPhysicsFriction->setEnabled(FALSE);
	mSpinPhysicsDensity->setEnabled(FALSE);
	mSpinPhysicsRestitution->setEnabled(FALSE);

	mComboMaterial->setEnabled( FALSE );
}

//
// Static functions
//

void LLPanelVolume::sendIsLight()
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}	
	LLVOVolume *volobjp = (LLVOVolume *)objectp;
	
	BOOL value = mCheckLight->getValue();
	volobjp->setIsLight(value);
	LL_INFOS() << "update light sent" << LL_ENDL;
}

void LLPanelVolume::sendIsFlexible()
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}	
	LLVOVolume *volobjp = (LLVOVolume *)objectp;
	
	BOOL is_flexible = mCheckFlexible1D->get();
	//BOOL is_flexible = mCheckFlexible1D->get();

	if (is_flexible)
	{
		//LLFirstUse::useFlexible();

		if (objectp->getClickAction() == CLICK_ACTION_SIT)
		{
			LLSelectMgr::getInstance()->selectionSetClickAction(CLICK_ACTION_NONE);
		}

	}

	if (volobjp->setIsFlexible(is_flexible))
	{
		mObject->sendShapeUpdate();
		LLSelectMgr::getInstance()->selectionUpdatePhantom(volobjp->flagPhantom());
	}

	LL_INFOS() << "update flexible sent" << LL_ENDL;
}

void LLPanelVolume::sendPhysicsShapeType(const LLSD& user_data)
{
	U8 type = user_data.asInteger();
	LLSelectMgr::getInstance()->selectionSetPhysicsType(type);

	refreshCost();
}

void LLPanelVolume::sendPhysicsGravity(const LLSD& user_data)
{
	F32 val = user_data.asReal();
	LLSelectMgr::getInstance()->selectionSetGravity(val);
}

void LLPanelVolume::sendPhysicsFriction(const LLSD& user_data)
{
	F32 val = user_data.asReal();
	LLSelectMgr::getInstance()->selectionSetFriction(val);
}

void LLPanelVolume::sendPhysicsRestitution(const LLSD& user_data)
{
	F32 val = user_data.asReal();
	LLSelectMgr::getInstance()->selectionSetRestitution(val);
}

void LLPanelVolume::sendPhysicsDensity(const LLSD& user_data)
{
	F32 val = user_data.asReal();
	LLSelectMgr::getInstance()->selectionSetDensity(val);
}

void LLPanelVolume::refreshCost()
{
	LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
	
	if (obj)
	{
		obj->getObjectCost();
	}
}

void LLPanelVolume::onLightCancelColor(const LLSD& data)
{
	mLightColorSwatch->setColor(mLightSavedColor);

	onLightSelectColor(data);
}

void LLPanelVolume::onLightCancelTexture(const LLSD& data)
{
	mLightTexturePicker->setImageAssetID(mLightSavedTexture);

	LLVOVolume *volobjp = (LLVOVolume *) mObject.get();
	if(volobjp)
	{
		// Cancel the light texture as requested
		// NORSPEC-292
		//
        bool is_spotlight = volobjp->isLightSpotlight();
        volobjp->setLightTextureID(mLightSavedTexture); //updates spotlight

        if (!is_spotlight && mLightSavedTexture.notNull())
        {
            LLVector3 spot_params = volobjp->getSpotLightParams();
			mLightFOV->setValue(spot_params.mV[0]);
            mLightFocus->setValue(spot_params.mV[1]);
			mLightAmbiance->setValue(spot_params.mV[2]);
        }
	}
}

void LLPanelVolume::onLightSelectColor(const LLSD& data)
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}	
	LLVOVolume *volobjp = (LLVOVolume *)objectp;

	LLColor4	clr = mLightColorSwatch->get();
	LLColor3	clr3( clr );
	volobjp->setLightColor(clr3);
	mLightSavedColor = clr;
}

void LLPanelVolume::onLightSelectTexture(const LLSD& data)
{
	if (mObject.isNull() || (mObject->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}	
	LLVOVolume *volobjp = (LLVOVolume *) mObject.get();

	LLUUID id = mLightTexturePicker->getImageAssetID();
	volobjp->setLightTextureID(id);
	mLightSavedTexture = id;
}

// static
void LLPanelVolume::onCommitMaterial(LLUICtrl* ctrl)
{
	//LLPanelObject* self = (LLPanelObject*) userdata;
	LLComboBox* box = (LLComboBox*) ctrl;

	if (box)
	{
		// apply the currently selected material to the object
		const std::string& material_name = box->getSimple();
		std::string LEGACY_FULLBRIGHT_DESC = LLTrans::getString("Fullbright");
		if (material_name != LEGACY_FULLBRIGHT_DESC)
		{
			U8 material_code = LLMaterialTable::basic.getMCode(material_name);
			LLSelectMgr::getInstance()->selectionSetMaterial(material_code);
		}
	}
}

void LLPanelVolume::onCommitLight()
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}	
	LLVOVolume *volobjp = (LLVOVolume *)objectp;
	
	
	volobjp->setLightIntensity((F32)mLightIntensity->getValue().asReal());
	volobjp->setLightRadius((F32)mLightRadius->getValue().asReal());
	volobjp->setLightFalloff((F32)mLightFalloff->getValue().asReal());

	LLColor4 clr = mLightColorSwatch->get();
	volobjp->setLightColor(LLColor3(clr));

	LLUUID id = mLightTexturePicker->getImageAssetID();
	if (id.notNull())
	{
		if (!volobjp->isLightSpotlight())
		{ //this commit is making this a spot light, set UI to default params
			volobjp->setLightTextureID(id);
			LLVector3 spot_params = volobjp->getSpotLightParams();
			mLightFOV->setValue(spot_params.mV[0]);
			mLightFocus->setValue(spot_params.mV[1]);
			mLightAmbiance->setValue(spot_params.mV[2]);
		}
		else
		{ //modifying existing params, this time volobjp won't change params on its own.
			if (volobjp->getLightTextureID() != id)
			{
				volobjp->setLightTextureID(id);
			}

			LLVector3 spot_params;
			spot_params.mV[0] = (F32) mLightFOV->getValue().asReal();
			spot_params.mV[1] = (F32) mLightFocus->getValue().asReal();
			spot_params.mV[2] = (F32) mLightAmbiance->getValue().asReal();
			volobjp->setSpotLightParams(spot_params);
		}
	}
	else if (volobjp->isLightSpotlight())
	{ //no longer a spot light
		volobjp->setLightTextureID(id);
		//mLightFOV->setEnabled(FALSE);
		//mLightFocus->setEnabled(FALSE);
		//mLightAmbiance->setEnabled(FALSE);
	}
}

void LLPanelVolume::onCommitIsLight()
{
	sendIsLight();
}

//----------------------------------------------------------------------------

void LLPanelVolume::onCommitFlexible()
{
	LLViewerObject* objectp = mObject;
	if (!objectp || (objectp->getPCode() != LL_PCODE_VOLUME))
	{
		return;
	}
	
	LLFlexibleObjectData *attributes = (LLFlexibleObjectData *)objectp->getParameterEntry(LLNetworkData::PARAMS_FLEXIBLE);
	if (attributes)
	{
		LLFlexibleObjectData new_attributes;
		new_attributes = *attributes;


		new_attributes.setSimulateLOD(mFlexSections->getValue().asInteger());//(S32)self->mSpinSections->get());
		new_attributes.setGravity((F32)mFlexGravity->getValue().asReal());
		new_attributes.setTension((F32)mFlexTension->getValue().asReal());
		new_attributes.setAirFriction((F32)mFlexFriction->getValue().asReal());
		new_attributes.setWindSensitivity((F32)mFlexWind->getValue().asReal());
		F32 fx = (F32) mFlexForce[VX]->getValue().asReal();
		F32 fy = (F32) mFlexForce[VY]->getValue().asReal();
		F32 fz = (F32) mFlexForce[VZ]->getValue().asReal();
		LLVector3 force(fx,fy,fz);

		new_attributes.setUserForce(force);
		objectp->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, new_attributes, true);
	}

	// Values may fail validation
	refresh();
}

void LLPanelVolume::onCommitIsFlexible()
{
	if (mObject->flagObjectPermanent())
	{
		LLNotificationsUtil::add("PathfindingLinksets_ChangeToFlexiblePath", LLSD(), LLSD(), boost::bind(&LLPanelVolume::handleResponseChangeToFlexible, this, _1, _2));
	}
	else
	{
		sendIsFlexible();
	}
}

void LLPanelVolume::handleResponseChangeToFlexible(const LLSD &pNotification, const LLSD &pResponse)
{
	if (LLNotificationsUtil::getSelectedOption(pNotification, pResponse) == 0)
	{
		sendIsFlexible();
	}
	else
	{
		mCheckFlexible1D->setValue(FALSE);
	}
}
