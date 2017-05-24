// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file llpanelface.cpp
 * @brief Panel in the tools floater for editing face textures, colors, etc.
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
#include "llpanelface.h"

// library includes
#include "llcalc.h"
#include "llerror.h"
#include "llfocusmgr.h"
#include "llrect.h"
#include "llstring.h"
#include "llfontgl.h"

// project includes
#include "llagentdata.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "lllineeditor.h"
#include "llmaterialmgr.h"
#include "llmediaentry.h"
#include "llnotificationsutil.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llvovolume.h"
#include "llpluginclassmedia.h"
#include "llviewertexturelist.h"// Update sel manager as to which channel we're editing so it can reflect the correct overlay UI

//
// Constant definitions for comboboxes
// Must match the commbobox definitions in panel_tools_texture.xml
//
const S32 MATMEDIA_MATERIAL = 0;	// Material
const S32 MATMEDIA_MEDIA = 1;		// Media
const S32 MATTYPE_DIFFUSE = 0;		// Diffuse material texture
const S32 MATTYPE_NORMAL = 1;		// Normal map
const S32 MATTYPE_SPECULAR = 2;		// Specular map
const S32 ALPHAMODE_MASK = 2;		// Alpha masking mode
const S32 BUMPY_TEXTURE = 18;		// use supplied normal map
const S32 SHINY_TEXTURE = 4;		// use supplied specular map

template<class T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
T normalize(const T value, const T start, const T end)
{
	const T width = end - start;
	const T offset_val = value - start;
	return (offset_val - ((offset_val / width) * width)) + start;
}

template<class T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
T normalize(const T value, const T start, const T end)
{
	const T width = end - start;
	const T offset_val = value - start;
	return (offset_val - (llfloor(offset_val / width) * width)) + start;
}

//
// "Use texture" label for normal/specular type comboboxes
// Filled in at initialization from translated strings
//
std::string USE_TEXTURE;

LLRender::eTexIndex LLPanelFace::getTextureChannelToEdit()
{
	// <alchemy>
	LLRender::eTexIndex channel_to_edit = (mComboMatMedia && mComboMatMedia->getCurrentIndex() == MATMEDIA_MATERIAL) ?
	(mComboMatType ? (LLRender::eTexIndex)mComboMatType->getCurrentIndex() : LLRender::DIFFUSE_MAP) : LLRender::DIFFUSE_MAP;
	
	channel_to_edit = (channel_to_edit == LLRender::NORMAL_MAP)		? (getCurrentNormalMap().isNull()		? LLRender::DIFFUSE_MAP : channel_to_edit) : channel_to_edit;
	channel_to_edit = (channel_to_edit == LLRender::SPECULAR_MAP)	? (getCurrentSpecularMap().isNull()		? LLRender::DIFFUSE_MAP : channel_to_edit) : channel_to_edit;
	return channel_to_edit;
 // </alchemy>
}

// Things the UI provides...
//
const LLUUID&	LLPanelFace::getCurrentNormalMap() { return mBumpyTextureCtrl->getImageAssetID(); } // <alchemy/>
const LLUUID&	LLPanelFace::getCurrentSpecularMap() { return mShinyTextureCtrl->getImageAssetID(); } // <alchemy/>
U32		LLPanelFace::getCurrentShininess() { return mComboShiny->getCurrentIndex(); }
U32		LLPanelFace::getCurrentBumpiness() { return mComboBump->getCurrentIndex(); }
U8		LLPanelFace::getCurrentDiffuseAlphaMode() { return (U8) mComboAlphaMode->getCurrentIndex(); }
U8		LLPanelFace::getCurrentAlphaMaskCutoff() { return (U8) getChild<LLUICtrl>("maskcutoff")->getValue().asInteger(); }
U8		LLPanelFace::getCurrentEnvIntensity() { return (U8) getChild<LLUICtrl>("environment")->getValue().asInteger(); }
U8		LLPanelFace::getCurrentGlossiness() { return (U8) getChild<LLUICtrl>("glossiness")->getValue().asInteger(); }
F32		LLPanelFace::getCurrentBumpyRot() { return getChild<LLUICtrl>("bumpyRot")->getValue().asReal(); }
F32		LLPanelFace::getCurrentBumpyScaleU() { return getChild<LLUICtrl>("bumpyScaleU")->getValue().asReal(); }
F32		LLPanelFace::getCurrentBumpyScaleV() { return getChild<LLUICtrl>("bumpyScaleV")->getValue().asReal(); }
F32		LLPanelFace::getCurrentBumpyOffsetU() { return getChild<LLUICtrl>("bumpyOffsetU")->getValue().asReal(); }
F32		LLPanelFace::getCurrentBumpyOffsetV() { return getChild<LLUICtrl>("bumpyOffsetV")->getValue().asReal(); }
F32		LLPanelFace::getCurrentShinyRot() { return getChild<LLUICtrl>("shinyRot")->getValue().asReal(); }
F32		LLPanelFace::getCurrentShinyScaleU() { return getChild<LLUICtrl>("shinyScaleU")->getValue().asReal(); }
F32		LLPanelFace::getCurrentShinyScaleV() { return getChild<LLUICtrl>("shinyScaleV")->getValue().asReal(); }
F32		LLPanelFace::getCurrentShinyOffsetU() { return getChild<LLUICtrl>("shinyOffsetU")->getValue().asReal(); }
F32		LLPanelFace::getCurrentShinyOffsetV() { return getChild<LLUICtrl>("shinyOffsetV")->getValue().asReal(); }
F32		LLPanelFace::getCurrentTextureRot() { return getChild<LLUICtrl>("TexRot")->getValue().asReal(); }
F32		LLPanelFace::getCurrentTextureScaleU() { return getChild<LLUICtrl>("TexScaleU")->getValue().asReal(); }
F32		LLPanelFace::getCurrentTextureScaleV() { return getChild<LLUICtrl>("TexScaleV")->getValue().asReal(); }
F32		LLPanelFace::getCurrentTextureOffsetU() { return getChild<LLUICtrl>("TexOffsetU")->getValue().asReal(); }
F32		LLPanelFace::getCurrentTextureOffsetV() { return getChild<LLUICtrl>("TexOffsetV")->getValue().asReal(); }

//
// Methods
//

BOOL LLPanelFace::postBuild()
{
	// Diffuse
	{
		mTextureCtrl = getChild<LLTextureCtrl>("texture control");
		mTextureCtrl->setDefaultImageAssetID(LLUUID(gSavedSettings.getString("DefaultObjectTexture")));
		mTextureCtrl->setCommitCallback(boost::bind(&LLPanelFace::onCommitTexture, this));
		mTextureCtrl->setOnCancelCallback(boost::bind(&LLPanelFace::onCancelTexture, this));
		mTextureCtrl->setOnSelectCallback(boost::bind(&LLPanelFace::onSelectTexture, this));
		mTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mTextureCtrl->setOnCloseCallback(boost::bind(&::LLPanelFace::refresh, this));
		mTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);

		mComboAlphaMode = getChild<LLComboBox>("combobox alphamode");
		mComboAlphaMode->setCommitCallback(boost::bind(&LLPanelFace::onCommitAlphaMode, this));

		childSetCommitCallback("maskcutoff", &LLPanelFace::onCommitMaterialMaskCutoff, this);
		
	}

	// Normal
	{
		mBumpyTextureCtrl = getChild<LLTextureCtrl>("bumpytexture control");
		mBumpyTextureCtrl->setDefaultImageAssetID(LLUUID(gSavedSettings.getString("DefaultObjectNormalTexture")));
		mBumpyTextureCtrl->setBlankImageAssetID(LLUUID(gSavedSettings.getString("DefaultBlankNormalTexture")));
		mBumpyTextureCtrl->setCommitCallback(boost::bind(&LLPanelFace::onCommitNormalTexture, this));
		mBumpyTextureCtrl->setOnCancelCallback(boost::bind(&LLPanelFace::onCancelNormalTexture, this));
		mBumpyTextureCtrl->setOnSelectCallback(boost::bind(&LLPanelFace::onSelectNormalTexture, this));
		mBumpyTextureCtrl->setOnCloseCallback(boost::bind(&LLPanelFace::refresh, this));

		mBumpyTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mBumpyTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mBumpyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mBumpyTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);

		mComboBump = getChild<LLComboBox>("combobox bumpiness");
		mComboBump->setCommitCallback(boost::bind(&LLPanelFace::onCommitBump, this, _2));

		childSetCommitCallback("bumpyScaleU", &LLPanelFace::onCommitMaterialBumpyScaleX, this);
		childSetCommitCallback("bumpyScaleV", &LLPanelFace::onCommitMaterialBumpyScaleY, this);
		childSetCommitCallback("bumpyRot", &LLPanelFace::onCommitMaterialBumpyRot, this);
		childSetCommitCallback("bumpyOffsetU", &LLPanelFace::onCommitMaterialBumpyOffsetX, this);
		childSetCommitCallback("bumpyOffsetV", &LLPanelFace::onCommitMaterialBumpyOffsetY, this);
	}

	// Specular
	{
		mShinyTextureCtrl = getChild<LLTextureCtrl>("shinytexture control");
		mShinyTextureCtrl->setDefaultImageAssetID(LLUUID(gSavedSettings.getString("DefaultObjectSpecularTexture")));
		mShinyTextureCtrl->setCommitCallback(boost::bind(&LLPanelFace::onCommitSpecularTexture, this));
		mShinyTextureCtrl->setOnCancelCallback(boost::bind(&LLPanelFace::onCancelSpecularTexture, this));
		mShinyTextureCtrl->setOnSelectCallback(boost::bind(&LLPanelFace::onSelectSpecularTexture, this));
		mShinyTextureCtrl->setOnCloseCallback(boost::bind(&LLPanelFace::refresh, this));
		mShinyTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mShinyTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mShinyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mShinyTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);

		mShinyColorSwatch = getChild<LLColorSwatchCtrl>("shinycolorswatch");
		mShinyColorSwatch->setCommitCallback(boost::bind(&LLPanelFace::onCommitShinyColor, this, _1));
		mShinyColorSwatch->setOnCancelCallback(boost::bind(&LLPanelFace::onCancelShinyColor, this));
		mShinyColorSwatch->setOnSelectCallback(boost::bind(&LLPanelFace::onSelectShinyColor, this, _1));
		mShinyColorSwatch->setCanApplyImmediately(TRUE);

		mComboShiny = getChild<LLComboBox>("combobox shininess");
		mComboShiny->setCommitCallback(boost::bind(&LLPanelFace::onCommitShiny, this, _2));

		childSetCommitCallback("shinyScaleU", &LLPanelFace::onCommitMaterialShinyScaleX, this);
		childSetCommitCallback("shinyScaleV", &LLPanelFace::onCommitMaterialShinyScaleY, this);
		childSetCommitCallback("shinyRot", &LLPanelFace::onCommitMaterialShinyRot, this);
		childSetCommitCallback("shinyOffsetU", &LLPanelFace::onCommitMaterialShinyOffsetX, this);
		childSetCommitCallback("shinyOffsetV", &LLPanelFace::onCommitMaterialShinyOffsetY, this);
		childSetCommitCallback("glossiness", &LLPanelFace::onCommitMaterialGloss, this);
		childSetCommitCallback("environment", &LLPanelFace::onCommitMaterialEnv, this);
	}


	getChild<LLUICtrl>("TexScaleU")->setCommitCallback(boost::bind(&LLPanelFace::sendTextureInfo, this));
	getChild<LLUICtrl>("TexScaleV")->setCommitCallback(boost::bind(&LLPanelFace::sendTextureInfo, this));
	getChild<LLUICtrl>("TexRot")->setCommitCallback(boost::bind(&LLPanelFace::sendTextureInfo, this));
	getChild<LLUICtrl>("rptctrl")->setCommitCallback(boost::bind(&LLPanelFace::onCommitRepeatsPerMeter, this, _1));
	getChild<LLUICtrl>("checkbox planar align")->setCommitCallback(boost::bind(&LLPanelFace::onCommitPlanarAlign, this));
	getChild<LLUICtrl>("TexOffsetU")->setCommitCallback(boost::bind(&LLPanelFace::sendTextureInfo, this));
	getChild<LLUICtrl>("TexOffsetV")->setCommitCallback(boost::bind(&LLPanelFace::sendTextureInfo, this));
	getChild<LLUICtrl>("button align")->setCommitCallback(boost::bind(&LLPanelFace::onClickAutoFix, this));
	getChild<LLUICtrl>("checkbox align mats")->setCommitCallback(boost::bind(&LLPanelFace::onClickAlignMats, this, _2));
	
	setMouseOpaque(FALSE);

	mColorSwatch = getChild<LLColorSwatchCtrl>("colorswatch");
	mColorSwatch->setCommitCallback(boost::bind(&LLPanelFace::sendColor, this));
	mColorSwatch->setOnCancelCallback(boost::bind(&LLPanelFace::onCancelColor, this));
	mColorSwatch->setOnSelectCallback(boost::bind(&LLPanelFace::onSelectColor, this));
	mColorSwatch->setCanApplyImmediately(TRUE);

	mLabelColorTransp = getChild<LLTextBox>("color trans");
	
	mCtrlColorTransp = getChild<LLSpinCtrl>("ColorTrans");
	mCtrlColorTransp->setCommitCallback(boost::bind(&LLPanelFace::sendAlpha, this, _2));
	
	mCheckFullbright = getChild<LLCheckBoxCtrl>("checkbox fullbright");
	mCheckFullbright->setCommitCallback(boost::bind(&LLPanelFace::sendFullbright, this, _2));
	
	mComboTexGen = getChild<LLComboBox>("combobox texgen");
	mComboTexGen->setCommitCallback(boost::bind(&LLPanelFace::sendTexGen, this, _2));
	
	mComboMatMedia = getChild<LLComboBox>("combobox matmedia");
	mComboMatMedia->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialsMedia, this));
	mComboMatMedia->selectNthItem(MATMEDIA_MATERIAL);
	
	mComboMatType = getChild<LLComboBox>("combobox mattype");
	mComboMatType->setCommitCallback(boost::bind(&LLPanelFace::onCommitMaterialType, this));
	mComboMatType->selectNthItem(MATTYPE_DIFFUSE);
	
	mCtrlGlow = getChild<LLSpinCtrl>("glow");
	mCtrlGlow->setCommitCallback(boost::bind(&LLPanelFace::sendGlow, this, _2));
	
	clearCtrls();
	
	return TRUE;
}

LLPanelFace::LLPanelFace()
:	LLPanel()
// <alchemy>
,	mTextureCtrl(nullptr)
,	mShinyTextureCtrl(nullptr)
,	mBumpyTextureCtrl(nullptr)
,	mColorSwatch(nullptr)
,	mShinyColorSwatch(nullptr)
,	mComboTexGen(nullptr)
,	mComboMatMedia(nullptr)
,	mComboMatType(nullptr)
,	mCheckFullbright(nullptr)
,	mLabelColorTransp(nullptr)
,	mCtrlColorTransp(nullptr)	// transparency = 1 - alpha
,	mCtrlGlow(nullptr)
// </alchemy>
,	mIsAlpha(false)
{
	USE_TEXTURE = LLTrans::getString("use_texture");
}


LLPanelFace::~LLPanelFace()
{
	// Children all cleaned up by default view destructor.
}


void LLPanelFace::sendTexture()
{
	if( !mTextureCtrl->getTentative() )
	{
		// we grab the item id first, because we want to do a
		// permissions check in the selection manager.
		LLUUID id = mTextureCtrl->getImageItemID();
		if(id.isNull())
		{
			id = mTextureCtrl->getImageAssetID();
		}
		LLSelectMgr::getInstance()->selectionSetImage(id);
	}
}

void LLPanelFace::sendBump(U32 bumpiness)
{
	if (bumpiness < BUMPY_TEXTURE)
	{
		LL_DEBUGS("Materials") << "clearing bumptexture control" << LL_ENDL;
		mBumpyTextureCtrl->clear();
		mBumpyTextureCtrl->setImageAssetID(LLUUID());
	}
	
	updateBumpyControls(bumpiness == BUMPY_TEXTURE, true);
	
	LLUUID current_normal_map = mBumpyTextureCtrl->getImageAssetID();
	
	U8 bump = (U8) bumpiness & TEM_BUMP_MASK;
	
	// Clear legacy bump to None when using an actual normal map
	//
	if (!current_normal_map.isNull())
		bump = 0;
	
	// Set the normal map or reset it to null as appropriate
	//
	LLSelectedTEMaterial::setNormalID(this, current_normal_map);
	
	LLSelectMgr::getInstance()->selectionSetBumpmap( bump );
}

void LLPanelFace::sendTexGen(const LLSD& userdata)
{
	U8 tex_gen = (U8) userdata.asInteger() << TEM_TEX_GEN_SHIFT;
	LLSelectMgr::getInstance()->selectionSetTexGen( tex_gen );
}

void LLPanelFace::sendShiny(U32 shininess)
{
	if (shininess < SHINY_TEXTURE)
	{
		mShinyTextureCtrl->clear();
		mShinyTextureCtrl->setImageAssetID(LLUUID());
	}
	
	LLUUID specmap = getCurrentSpecularMap();
	
	U8 shiny = (U8) shininess & TEM_SHINY_MASK;
	if (!specmap.isNull())
		shiny = 0;
	
	LLSelectedTEMaterial::setSpecularID(this, specmap);
	
	LLSelectMgr::getInstance()->selectionSetShiny( shiny );
	
	updateShinyControls(!specmap.isNull(), true);
}

void LLPanelFace::sendFullbright(const LLSD& userdata)
{
	U8 fullbright = userdata.asBoolean() ? TEM_FULLBRIGHT_MASK : 0;
	LLSelectMgr::getInstance()->selectionSetFullbright( fullbright );
}

void LLPanelFace::sendColor()
{
	LLColor4 color = mColorSwatch->get();
	
	LLSelectMgr::getInstance()->selectionSetColorOnly( color );
}

void LLPanelFace::sendAlpha(const LLSD& userdata)
{
	F32 alpha = (100.f - userdata.asReal()) / 100.f;
	LLSelectMgr::getInstance()->selectionSetAlphaOnly( alpha );
}

void LLPanelFace::sendGlow(const LLSD& userdata)
{
	LLSelectMgr::getInstance()->selectionSetGlow( userdata.asReal() );
}

struct LLPanelFaceSetTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceSetTEFunctor(LLPanelFace* panel) : mPanel(panel) {}

	bool apply(LLViewerObject* object, S32 te) override
	{
		BOOL valid;
		F32 value;
		LLSpinCtrl*	ctrlTexScaleS = mPanel->getChild<LLSpinCtrl>("TexScaleU");
		LLSpinCtrl*	ctrlTexScaleT = mPanel->getChild<LLSpinCtrl>("TexScaleV");
		LLSpinCtrl*	ctrlTexOffsetS = mPanel->getChild<LLSpinCtrl>("TexOffsetU");
		LLSpinCtrl*	ctrlTexOffsetT = mPanel->getChild<LLSpinCtrl>("TexOffsetV");
		LLSpinCtrl*	ctrlTexRotation = mPanel->getChild<LLSpinCtrl>("TexRot");
		LLComboBox*	comboTexGen = mPanel->getChild<LLComboBox>("combobox texgen");
		llassert(object);
		
		valid = !ctrlTexScaleS->getTentative(); // || !checkFlipScaleS->getTentative();
		if (valid)
		{
			value = ctrlTexScaleS->get();
			if (comboTexGen &&
				comboTexGen->getCurrentIndex() == 1)
			{
				value *= 0.5f;
			}
			object->setTEScaleS( te, value );
		}
		
		valid = !ctrlTexScaleT->getTentative(); // || !checkFlipScaleT->getTentative();
		if (valid)
		{
			value = ctrlTexScaleT->get();
			//if( checkFlipScaleT->get() )
			//{
			//	value = -value;
			//}
			if (comboTexGen &&
				comboTexGen->getCurrentIndex() == 1)
			{
				value *= 0.5f;
			}
			object->setTEScaleT( te, value );
		}
		
		valid = !ctrlTexOffsetS->getTentative();
		if (valid)
		{
			value = ctrlTexOffsetS->get();
			object->setTEOffsetS( te, value );
		}
		
		valid = !ctrlTexOffsetT->getTentative();
		if (valid)
		{
			value = ctrlTexOffsetT->get();
			object->setTEOffsetT( te, value );
		}
		
		valid = !ctrlTexRotation->getTentative();
		if (valid)
		{
			value = ctrlTexRotation->get() * DEG_TO_RAD;
			object->setTERotation( te, value );
		}
		return true;
	}
private:
	LLPanelFace* mPanel;
};

// Functor that aligns a face to mCenterFace
struct LLPanelFaceSetAlignedTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceSetAlignedTEFunctor(LLPanelFace* panel, LLFace* center_face) :
	mPanel(panel),
	mCenterFace(center_face) {}

	bool apply(LLViewerObject* object, S32 te) override
	{
		LLFace* facep = object->mDrawable->getFace(te);
		if (!facep)
		{
			return true;
		}
		
		if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
		{
			return true;
		}
		
		bool set_aligned = true;
		if (facep == mCenterFace)
		{
			set_aligned = false;
		}
		if (set_aligned)
		{
			LLVector2 uv_offset, uv_scale;
			F32 uv_rot;
			set_aligned = facep->calcAlignedPlanarTE(mCenterFace, &uv_offset, &uv_scale, &uv_rot);
			if (set_aligned)
			{
				object->setTEOffset(te, uv_offset.mV[VX], uv_offset.mV[VY]);
				object->setTEScale(te, uv_scale.mV[VX], uv_scale.mV[VY]);
				object->setTERotation(te, uv_rot);
			}
		}
		if (!set_aligned)
		{
			LLPanelFaceSetTEFunctor setfunc(mPanel);
			setfunc.apply(object, te);
		}
		return true;
	}
private:
	LLPanelFace* mPanel;
	LLFace* mCenterFace;
};

// Functor that tests if a face is aligned to mCenterFace
struct LLPanelFaceGetIsAlignedTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceGetIsAlignedTEFunctor(LLFace* center_face) :
	mCenterFace(center_face) {}

	bool apply(LLViewerObject* object, S32 te) override
	{
		LLFace* facep = object->mDrawable->getFace(te);
		if (!facep)
		{
			return false;
		}

		if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
		{ //volume face does not exist, can't be aligned
			return false;
		}

		if (facep == mCenterFace)
		{
			return true;
		}
		
		LLVector2 aligned_st_offset, aligned_st_scale;
		F32 aligned_st_rot;
		if ( facep->calcAlignedPlanarTE(mCenterFace, &aligned_st_offset, &aligned_st_scale, &aligned_st_rot) )
		{
			const LLTextureEntry* tep = facep->getTextureEntry();
			LLVector2 st_offset, st_scale;
			tep->getOffset(&st_offset.mV[VX], &st_offset.mV[VY]);
			tep->getScale(&st_scale.mV[VX], &st_scale.mV[VY]);
			F32 st_rot = tep->getRotation();
			// needs a fuzzy comparison, because of fp errors
			if (is_approx_equal_fraction(st_offset.mV[VX], aligned_st_offset.mV[VX], 12) &&
				is_approx_equal_fraction(st_offset.mV[VY], aligned_st_offset.mV[VY], 12) &&
				is_approx_equal_fraction(st_scale.mV[VX], aligned_st_scale.mV[VX], 12) &&
				is_approx_equal_fraction(st_scale.mV[VY], aligned_st_scale.mV[VY], 12) &&
				is_approx_equal_fraction(st_rot, aligned_st_rot, 14))
			{
				return true;
			}
		}
		return false;
	}
private:
	LLFace* mCenterFace;
};

struct LLPanelFaceSendFunctor : public LLSelectedObjectFunctor
{
	bool apply(LLViewerObject* object) override
	{
		object->sendTEUpdate();
		return true;
	}
};

void LLPanelFace::sendTextureInfo()
{
	if (childGetValue("checkbox planar align").asBoolean())
	{
		LLFace* last_face = nullptr;
		bool identical_face =false;
		LLSelectedTE::getFace(last_face, identical_face);
		LLPanelFaceSetAlignedTEFunctor setfunc(this, last_face);
		LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
	}
	else
	{
		LLPanelFaceSetTEFunctor setfunc(this);
		LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
	}

	LLPanelFaceSendFunctor sendfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToObjects(&sendfunc);
	
	if (gSavedSettings.getBOOL("AlchemyAlignMaterials"))
	{
		alignMaterialProperties();
	}
}

void LLPanelFace::refresh()
{
	LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
	
	if (objectp && objectp->getPCode() == LL_PCODE_VOLUME && objectp->permModify())
	{
		BOOL editable = objectp->permModify() && !objectp->isPermanentEnforced();
		static LLCachedControl<bool> sAlignMaterials(gSavedSettings, "AlchemyAlignMaterials");

		// only turn on auto-adjust button if there is a media renderer and the media is loaded
		getChildView("button align")->setEnabled(editable);

		if (mComboMatMedia->getCurrentIndex() < MATMEDIA_MATERIAL)
		{
			mComboMatMedia->selectNthItem(MATMEDIA_MATERIAL);
		}
		mComboMatMedia->setEnabled(editable);

		if (mComboMatType->getCurrentIndex() < MATTYPE_DIFFUSE)
		{
			mComboMatType->selectNthItem(MATTYPE_DIFFUSE);
		}
		mComboMatType->setEnabled(editable);

		updateVisibility();

		bool identical = true;	// true because it is anded below
		bool identical_diffuse = false;
		bool identical_norm = false;
		bool identical_spec = false;

		LLUUID id;
		LLUUID normmap_id;
		LLUUID specmap_id;

		// Color swatch
		getChildView("color label")->setEnabled(editable);

		LLColor4 color = LLColor4::white;
		bool identical_color = false;

		LLSelectedTE::getColor(color, identical_color);

		mColorSwatch->setOriginal(color);
		mColorSwatch->set(color, TRUE);

		mColorSwatch->setValid(editable);
		mColorSwatch->setEnabled(editable);
		mColorSwatch->setCanApplyImmediately(editable);

		// Color transparency
		mLabelColorTransp->setEnabled(editable);

		F32 transparency = (1.f - color.mV[VALPHA]) * 100.f;
		mCtrlColorTransp->setValue(editable ? transparency : 0);
		mCtrlColorTransp->setEnabled(editable);

		// Specular map
		LLSelectedTEMaterial::getSpecularID(specmap_id, identical_spec);

		U8 shiny = 0;
		bool identical_shiny = false;

		// Shiny
		LLSelectedTE::getShiny(shiny, identical_shiny);
		identical = identical && identical_shiny;

		shiny = specmap_id.isNull() ? shiny : SHINY_TEXTURE;

		mComboShiny->selectNthItem((S32) shiny);

		getChildView("label shininess")->setEnabled(editable);
		mComboShiny->setEnabled(editable);

		getChildView("label glossiness")->setEnabled(editable);
		getChildView("glossiness")->setEnabled(editable);

		getChildView("label environment")->setEnabled(editable);
		getChildView("environment")->setEnabled(editable);
		getChildView("label shinycolor")->setEnabled(editable);

		mComboShiny->setTentative(!identical_spec);
		getChild<LLUICtrl>("glossiness")->setTentative(!identical_spec);
		getChild<LLUICtrl>("environment")->setTentative(!identical_spec);
		mShinyColorSwatch->setTentative(!identical_spec);

		mShinyColorSwatch->setValid(editable);
		mShinyColorSwatch->setEnabled(editable);
		mShinyColorSwatch->setCanApplyImmediately(editable);

		U8 bumpy = 0;
		// Bumpy
		bool identical_bumpy = false;
		LLSelectedTE::getBumpmap(bumpy, identical_bumpy);

		mComboBump->selectNthItem((S32) getCurrentNormalMap().isNull() ? bumpy : BUMPY_TEXTURE);

		mComboBump->setEnabled(editable);
		mComboBump->setTentative(!identical_bumpy);
		getChildView("label bumpiness")->setEnabled(editable);

		// Texture
		LLSelectedTE::getTexId(id, identical_diffuse);

		// Normal map
		LLSelectedTEMaterial::getNormalID(normmap_id, identical_norm);

		LLGLenum image_format = GL_RGB;
		bool identical_image_format = false;
		LLSelectedTE::getImageFormat(image_format, identical_image_format);

		switch (image_format)
		{
		case GL_RGBA:
		case GL_ALPHA:
			mIsAlpha = TRUE;
			break;
		default:
			LL_WARNS() << "Unexpected tex format in LLPanelFace...resorting to no alpha" << LL_ENDL;
		case GL_RGB:
			mIsAlpha = FALSE;
			break;
		}

		if (LLViewerMedia::textureHasMedia(id))
		{
			getChildView("button align")->setEnabled(editable);
		}

		// Diffuse Alpha Mode

		// Init to the default that is appropriate for the alpha content of the asset
		//
		U8 alpha_mode = mIsAlpha ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

		bool identical_alpha_mode = false;

		// See if that's been overridden by a material setting for same...
		//
		LLSelectedTEMaterial::getCurrentDiffuseAlphaMode(alpha_mode, identical_alpha_mode, mIsAlpha);

		//it is invalid to have any alpha mode other than blend if transparency is greater than zero ...
		// Want masking? Want emissive? Tough! You get BLEND!
		if (transparency > 0.f) 
		{
			alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
		}

		// ... unless there is no alpha channel in the texture, in which case alpha mode MUST be none
		if (!mIsAlpha)
		{
			alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
		}
		mComboAlphaMode->selectNthItem(alpha_mode);

		updateAlphaControls();
		
		if (identical_diffuse)
		{
			mTextureCtrl->setTentative( FALSE );
			mTextureCtrl->setEnabled( editable );
			mTextureCtrl->setImageAssetID( id );
			mComboAlphaMode->setEnabled(editable && mIsAlpha && transparency <= 0.f);
			getChildView("label alphamode")->setEnabled(editable && mIsAlpha);
			getChildView("maskcutoff")->setEnabled(editable && mIsAlpha);
			getChildView("label maskcutoff")->setEnabled(editable && mIsAlpha);
		}
		else if (id.isNull())
		{
			// None selected
			mTextureCtrl->setTentative( FALSE );
			mTextureCtrl->setEnabled( FALSE );
			mTextureCtrl->setImageAssetID( LLUUID::null );
			mComboAlphaMode->setEnabled( FALSE );
			getChildView("label alphamode")->setEnabled( FALSE );
			getChildView("maskcutoff")->setEnabled( FALSE);
			getChildView("label maskcutoff")->setEnabled( FALSE );
		}
		else
		{
			// Tentative: multiple selected with different textures
			mTextureCtrl->setTentative( TRUE );
			mTextureCtrl->setEnabled( editable );
			mTextureCtrl->setImageAssetID( id );
			mComboAlphaMode->setEnabled(editable && mIsAlpha && transparency <= 0.f);
			getChildView("label alphamode")->setEnabled(editable && mIsAlpha);
			getChildView("maskcutoff")->setEnabled(editable && mIsAlpha);
			getChildView("label maskcutoff")->setEnabled(editable && mIsAlpha);
		}
		
		mShinyTextureCtrl->setTentative( !identical_spec );
		mShinyTextureCtrl->setEnabled( editable );
		mShinyTextureCtrl->setImageAssetID( specmap_id );
		
		mBumpyTextureCtrl->setTentative( !identical_norm );
		mBumpyTextureCtrl->setEnabled( editable );
		mBumpyTextureCtrl->setImageAssetID( normmap_id );
		
		// planar align
		bool align_planar = false;
		bool identical_planar_aligned = false;
		
		LLCheckBoxCtrl*	cb_planar_align = getChild<LLCheckBoxCtrl>("checkbox planar align");
		align_planar = (cb_planar_align && cb_planar_align->get());
		
		bool enabled = (editable && isIdenticalPlanarTexgen());
		childSetValue("checkbox planar align", align_planar && enabled);
		childSetEnabled("checkbox planar align", enabled);
		
		if (align_planar && enabled)
		{
			LLFace* last_face = nullptr;
			bool identical_face = false;
			LLSelectedTE::getFace(last_face, identical_face);
			
			LLPanelFaceGetIsAlignedTEFunctor get_is_aligend_func(last_face);
			// this will determine if the texture param controls are tentative:
			identical_planar_aligned = LLSelectMgr::getInstance()->getSelection()->applyToTEs(&get_is_aligend_func);
		}
		
		// Needs to be public and before tex scale settings below to properly reflect
		// behavior when in planar vs default texgen modes in the
		// NORSPEC-84 et al
		LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
		bool identical_texgen = true;
		bool identical_planar_texgen = false;
		
		LLSelectedTE::getTexGen(selected_texgen, identical_texgen);
		identical_planar_texgen = (identical_texgen && (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR));
		
		// Texture scale
		bool identical_diff_scale_s = false;
		bool identical_spec_scale_s = false;
		bool identical_norm_scale_s = false;
		
		identical = align_planar ? identical_planar_aligned : identical;
		
		F32 diff_scale_s = 1.f;
		F32 spec_scale_s = 1.f;
		F32 norm_scale_s = 1.f;
		
		LLSelectedTE::getScaleS(diff_scale_s, identical_diff_scale_s);
		LLSelectedTEMaterial::getSpecularRepeatX(spec_scale_s, identical_spec_scale_s);
		LLSelectedTEMaterial::getNormalRepeatX(norm_scale_s, identical_norm_scale_s);
		
		diff_scale_s = editable ? diff_scale_s : 1.0f;
		diff_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;
		
		norm_scale_s = editable ? norm_scale_s : 1.0f;
		norm_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;
		
		spec_scale_s = editable ? spec_scale_s : 1.0f;
		spec_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;
		
		getChild<LLUICtrl>("TexScaleU")->setValue(diff_scale_s);
		getChild<LLUICtrl>("shinyScaleU")->setValue(spec_scale_s);
		getChild<LLUICtrl>("bumpyScaleU")->setValue(norm_scale_s);
		
		getChildView("TexScaleU")->setEnabled(editable);
		getChildView("shinyScaleU")->setEnabled(editable && specmap_id.notNull() && !sAlignMaterials);
		getChildView("bumpyScaleU")->setEnabled(editable && normmap_id.notNull() && !sAlignMaterials);
		
		BOOL diff_scale_tentative = !(identical && identical_diff_scale_s);
		BOOL norm_scale_tentative = !(identical && identical_norm_scale_s);
		BOOL spec_scale_tentative = !(identical && identical_spec_scale_s);
		
		getChild<LLUICtrl>("TexScaleU")->setTentative( LLSD(diff_scale_tentative));
		getChild<LLUICtrl>("shinyScaleU")->setTentative(LLSD(spec_scale_tentative));
		getChild<LLUICtrl>("bumpyScaleU")->setTentative(LLSD(norm_scale_tentative));
		
		{
			bool identical_diff_scale_t = false;
			bool identical_spec_scale_t = false;
			bool identical_norm_scale_t = false;

			F32 diff_scale_t = 1.f;
			F32 spec_scale_t = 1.f;
			F32 norm_scale_t = 1.f;

			LLSelectedTE::getScaleT(diff_scale_t, identical_diff_scale_t);
			LLSelectedTEMaterial::getSpecularRepeatY(spec_scale_t, identical_spec_scale_t);
			LLSelectedTEMaterial::getNormalRepeatY(norm_scale_t, identical_norm_scale_t);

			diff_scale_t = editable ? diff_scale_t : 1.0f;
			diff_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			norm_scale_t = editable ? norm_scale_t : 1.0f;
			norm_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			spec_scale_t = editable ? spec_scale_t : 1.0f;
			spec_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			BOOL diff_scale_tentative = !identical_diff_scale_t;
			BOOL norm_scale_tentative = !identical_norm_scale_t;
			BOOL spec_scale_tentative = !identical_spec_scale_t;

			getChildView("TexScaleV")->setEnabled(editable);
			getChildView("shinyScaleV")->setEnabled(editable && specmap_id.notNull() && !sAlignMaterials);
			getChildView("bumpyScaleV")->setEnabled(editable && normmap_id.notNull() && !sAlignMaterials);
			
			getChild<LLUICtrl>("TexScaleV")->setValue(diff_scale_t);
			getChild<LLUICtrl>("shinyScaleV")->setValue(norm_scale_t);
			getChild<LLUICtrl>("bumpyScaleV")->setValue(spec_scale_t);
			
			getChild<LLUICtrl>("TexScaleV")->setTentative(LLSD(diff_scale_tentative));
			getChild<LLUICtrl>("shinyScaleV")->setTentative(LLSD(norm_scale_tentative));
			getChild<LLUICtrl>("bumpyScaleV")->setTentative(LLSD(spec_scale_tentative));
		}
		
		// Texture offset
		{
			bool identical_diff_offset_s = false;
			bool identical_norm_offset_s = false;
			bool identical_spec_offset_s = false;
			
			F32 diff_offset_s = 0.0f;
			F32 norm_offset_s = 0.0f;
			F32 spec_offset_s = 0.0f;
			
			LLSelectedTE::getOffsetS(diff_offset_s, identical_diff_offset_s);
			LLSelectedTEMaterial::getNormalOffsetX(norm_offset_s, identical_norm_offset_s);
			LLSelectedTEMaterial::getSpecularOffsetX(spec_offset_s, identical_spec_offset_s);
			
			BOOL diff_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_diff_offset_s);
			BOOL norm_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_norm_offset_s);
			BOOL spec_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_spec_offset_s);
			
			getChild<LLUICtrl>("TexOffsetU")->setValue(editable ? diff_offset_s : 0.0f);
			getChild<LLUICtrl>("bumpyOffsetU")->setValue(editable ? norm_offset_s : 0.0f);
			getChild<LLUICtrl>("shinyOffsetU")->setValue(editable ? spec_offset_s : 0.0f);
			
			getChild<LLUICtrl>("TexOffsetU")->setTentative(LLSD(diff_offset_u_tentative));
			getChild<LLUICtrl>("shinyOffsetU")->setTentative(LLSD(norm_offset_u_tentative));
			getChild<LLUICtrl>("bumpyOffsetU")->setTentative(LLSD(spec_offset_u_tentative));
			
			getChildView("TexOffsetU")->setEnabled(editable);
			getChildView("shinyOffsetU")->setEnabled(editable && specmap_id.notNull() && !sAlignMaterials);
			getChildView("bumpyOffsetU")->setEnabled(editable && normmap_id.notNull() && !sAlignMaterials);
		}
		
		{
			bool identical_diff_offset_t = false;
			bool identical_norm_offset_t = false;
			bool identical_spec_offset_t = false;
			
			F32 diff_offset_t = 0.0f;
			F32 norm_offset_t = 0.0f;
			F32 spec_offset_t = 0.0f;
			
			LLSelectedTE::getOffsetT(diff_offset_t, identical_diff_offset_t);
			LLSelectedTEMaterial::getNormalOffsetY(norm_offset_t, identical_norm_offset_t);
			LLSelectedTEMaterial::getSpecularOffsetY(spec_offset_t, identical_spec_offset_t);
			
			BOOL diff_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_diff_offset_t);
			BOOL norm_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_norm_offset_t);
			BOOL spec_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_spec_offset_t);
			
			getChild<LLUICtrl>("TexOffsetV")->setValue(  editable ? diff_offset_t : 0.0f);
			getChild<LLUICtrl>("bumpyOffsetV")->setValue(editable ? norm_offset_t : 0.0f);
			getChild<LLUICtrl>("shinyOffsetV")->setValue(editable ? spec_offset_t : 0.0f);
			
			getChild<LLUICtrl>("TexOffsetV")->setTentative(LLSD(diff_offset_v_tentative));
			getChild<LLUICtrl>("shinyOffsetV")->setTentative(LLSD(norm_offset_v_tentative));
			getChild<LLUICtrl>("bumpyOffsetV")->setTentative(LLSD(spec_offset_v_tentative));
			
			getChildView("TexOffsetV")->setEnabled(editable);
			getChildView("shinyOffsetV")->setEnabled(editable && specmap_id.notNull() && !sAlignMaterials);
			getChildView("bumpyOffsetV")->setEnabled(editable && normmap_id.notNull() && !sAlignMaterials);
		}
		
		// Texture rotation
		{
			bool identical_diff_rotation = false;
			bool identical_norm_rotation = false;
			bool identical_spec_rotation = false;
			
			F32 diff_rotation = 0.f;
			F32 norm_rotation = 0.f;
			F32 spec_rotation = 0.f;
			
			LLSelectedTE::getRotation(diff_rotation,identical_diff_rotation);
			LLSelectedTEMaterial::getSpecularRotation(spec_rotation,identical_spec_rotation);
			LLSelectedTEMaterial::getNormalRotation(norm_rotation,identical_norm_rotation);
			
			BOOL diff_rot_tentative = !(align_planar ? identical_planar_aligned : identical_diff_rotation);
			BOOL norm_rot_tentative = !(align_planar ? identical_planar_aligned : identical_norm_rotation);
			BOOL spec_rot_tentative = !(align_planar ? identical_planar_aligned : identical_spec_rotation);
			
			F32 diff_rot_deg = diff_rotation * RAD_TO_DEG;
			F32 norm_rot_deg = norm_rotation * RAD_TO_DEG;
			F32 spec_rot_deg = spec_rotation * RAD_TO_DEG;
			
			getChildView("TexRot")->setEnabled(editable);
			getChildView("shinyRot")->setEnabled(editable && specmap_id.notNull() && !sAlignMaterials);
			getChildView("bumpyRot")->setEnabled(editable && normmap_id.notNull() && !sAlignMaterials);
			
			getChild<LLUICtrl>("TexRot")->setTentative(diff_rot_tentative);
			getChild<LLUICtrl>("shinyRot")->setTentative(LLSD(norm_rot_tentative));
			getChild<LLUICtrl>("bumpyRot")->setTentative(LLSD(spec_rot_tentative));
			
			getChild<LLUICtrl>("TexRot")->setValue(  editable ? diff_rot_deg : 0.0f);
			getChild<LLUICtrl>("shinyRot")->setValue(editable ? spec_rot_deg : 0.0f);
			getChild<LLUICtrl>("bumpyRot")->setValue(editable ? norm_rot_deg : 0.0f);
		}
		
		F32 glow = 0.f;
		bool identical_glow = false;
		LLSelectedTE::getGlow(glow,identical_glow);
		mCtrlGlow->setValue(glow);
		mCtrlGlow->setTentative(!identical_glow);
		mCtrlGlow->setEnabled(editable);
		getChildView("glow label")->setEnabled(editable);
		
		{
			LLCtrlSelectionInterface* combobox_texgen = mComboTexGen->getSelectionInterface();
			if (combobox_texgen)
			{
				// Maps from enum to combobox entry index
				combobox_texgen->selectNthItem(((S32)selected_texgen) >> 1);
			}
			else
			{
				LL_WARNS() << "failed childGetSelectionInterface for 'combobox texgen'" << LL_ENDL;
			}
			
			mComboTexGen->setEnabled(editable);
			mComboTexGen->setTentative(!identical);
			getChildView("tex gen")->setEnabled(editable);
			
			if (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR)
			{
				// EXP-1507 (change label based on the mapping mode)
				getChild<LLSpinCtrl>("rptctrl")->setLabel(getString("string repeats per meter"));
			}
			else if (selected_texgen == LLTextureEntry::TEX_GEN_DEFAULT)
			{
				getChild<LLSpinCtrl>("rptctrl")->setLabel(getString("string repeats per face"));
			}
		}
		
		U8 fullbright_flag = 0;
		bool identical_fullbright = false;
		
		LLSelectedTE::getFullbright(fullbright_flag,identical_fullbright);
		
		mCheckFullbright->setValue(fullbright_flag != 0);
		mCheckFullbright->setEnabled(editable);
		mCheckFullbright->setTentative(!identical_fullbright);
		
		getChildView("checkbox align mats")->setEnabled(editable && (specmap_id.notNull() || normmap_id.notNull()));
		
		// Repeats per meter
		{
			F32 repeats_diff = 1.f;
			F32 repeats_norm = 1.f;
			F32 repeats_spec = 1.f;
			
			bool identical_diff_repeats = false;
			bool identical_norm_repeats = false;
			bool identical_spec_repeats = false;
			
			LLSelectedTE::getMaxDiffuseRepeats(repeats_diff, identical_diff_repeats);
			LLSelectedTEMaterial::getMaxNormalRepeats(repeats_norm, identical_norm_repeats);
			LLSelectedTEMaterial::getMaxSpecularRepeats(repeats_spec, identical_spec_repeats);
			
			S32 index = mComboTexGen ? mComboTexGen->getCurrentIndex() : 0;
			BOOL enabled = editable && (index != 1);
			BOOL identical_repeats = true;
			F32  repeats = 1.0f;
			
			U32 material_type = (mComboMatMedia->getCurrentIndex() == MATMEDIA_MATERIAL) ? mComboMatType->getCurrentIndex() : MATTYPE_DIFFUSE;
			
			LLSelectMgr::getInstance()->setTextureChannel(LLRender::eTexIndex(material_type));
			
			switch (material_type)
			{
				default:
				case MATTYPE_DIFFUSE:
					enabled = editable && !id.isNull();
					identical_repeats = identical_diff_repeats;
					repeats = repeats_diff;
					break;
					
				case MATTYPE_SPECULAR:
					enabled = (editable && ((shiny == SHINY_TEXTURE) && !specmap_id.isNull())
							   && !sAlignMaterials);
					identical_repeats = identical_spec_repeats;
					repeats = repeats_spec;
					break;
					
				case MATTYPE_NORMAL:
					enabled = (editable && ((bumpy == BUMPY_TEXTURE) && !normmap_id.isNull())
							   && !sAlignMaterials);
					identical_repeats = identical_norm_repeats;
					repeats = repeats_norm;
					break;
			}
			
			BOOL repeats_tentative = !identical_repeats;
			getChildView("rptctrl")->setEnabled(identical_planar_texgen ? FALSE : enabled);
			getChild<LLUICtrl>("rptctrl")->setValue(editable ? repeats : 1.0f);
			getChild<LLUICtrl>("rptctrl")->setTentative(LLSD(repeats_tentative));
		}

		// Materials
		LLMaterialPtr material;
		LLSelectedTEMaterial::getCurrent(material, identical);
		
		if (material && editable)
		{
			LL_DEBUGS("Materials") << " OnMatererialsLoaded: " << material->asLLSD() << LL_ENDL;
			
			// Alpha
			U32 alpha_mode = material->getDiffuseAlphaMode();

			if (transparency > 0.f)
			{ //it is invalid to have any alpha mode other than blend if transparency is greater than zero ...
				alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
			}

			if (!mIsAlpha)
			{ // ... unless there is no alpha channel in the texture, in which case alpha mode MUST ebe none
				alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
			}

			mComboAlphaMode->selectNthItem(alpha_mode);

			getChild<LLUICtrl>("maskcutoff")->setValue(material->getAlphaMaskCutoff());
			updateAlphaControls();
			
			identical_planar_texgen = isIdenticalPlanarTexgen();
			
			// Shiny (specular)
			F32 offset_x, offset_y, repeat_x, repeat_y, rot;
			mShinyTextureCtrl->setImageAssetID(material->getSpecularID());
			
			if (!material->getSpecularID().isNull() && (shiny == SHINY_TEXTURE))
			{
				material->getSpecularOffset(offset_x,offset_y);
				material->getSpecularRepeat(repeat_x,repeat_y);
				
				if (identical_planar_texgen)
				{
					repeat_x *= 2.0f;
					repeat_y *= 2.0f;
				}
				
				rot = material->getSpecularRotation();
				getChild<LLUICtrl>("shinyScaleU")->setValue(repeat_x);
				getChild<LLUICtrl>("shinyScaleV")->setValue(repeat_y);
				getChild<LLUICtrl>("shinyRot")->setValue(rot*RAD_TO_DEG);
				getChild<LLUICtrl>("shinyOffsetU")->setValue(offset_x);
				getChild<LLUICtrl>("shinyOffsetV")->setValue(offset_y);
				getChild<LLUICtrl>("glossiness")->setValue(material->getSpecularLightExponent());
				getChild<LLUICtrl>("environment")->setValue(material->getEnvironmentIntensity());
				
				updateShinyControls(!material->getSpecularID().isNull(), true);
			}
			
			// Assert desired colorswatch color to match material AFTER updateShinyControls
			// to avoid getting overwritten with the default on some UI state changes.
			//
			if (!material->getSpecularID().isNull())
			{
				mShinyColorSwatch->setOriginal(material->getSpecularLightColor());
				mShinyColorSwatch->set(material->getSpecularLightColor(),TRUE);
			}
			
			// Bumpy (normal)
			mBumpyTextureCtrl->setImageAssetID(material->getNormalID());
			
			if (!material->getNormalID().isNull())
			{
				material->getNormalOffset(offset_x,offset_y);
				material->getNormalRepeat(repeat_x,repeat_y);
				
				if (identical_planar_texgen)
				{
					repeat_x *= 2.0f;
					repeat_y *= 2.0f;
				}
				
				rot = material->getNormalRotation();
				getChild<LLUICtrl>("bumpyScaleU")->setValue(repeat_x);
				getChild<LLUICtrl>("bumpyScaleV")->setValue(repeat_y);
				getChild<LLUICtrl>("bumpyRot")->setValue(rot*RAD_TO_DEG);
				getChild<LLUICtrl>("bumpyOffsetU")->setValue(offset_x);
				getChild<LLUICtrl>("bumpyOffsetV")->setValue(offset_y);
				
				updateBumpyControls(!material->getNormalID().isNull(), true);
			}
		}

		// Set variable values for numeric expressions
		LLCalc* calcp = LLCalc::getInstance();
		calcp->setVar(LLCalc::TEX_U_SCALE, childGetValue("TexScaleU").asReal());
		calcp->setVar(LLCalc::TEX_V_SCALE, childGetValue("TexScaleV").asReal());
		calcp->setVar(LLCalc::TEX_U_OFFSET, childGetValue("TexOffsetU").asReal());
		calcp->setVar(LLCalc::TEX_V_OFFSET, childGetValue("TexOffsetV").asReal());
		calcp->setVar(LLCalc::TEX_ROTATION, childGetValue("TexRot").asReal());
		calcp->setVar(LLCalc::TEX_TRANSPARENCY, mCtrlColorTransp->getValue().asReal());
		calcp->setVar(LLCalc::TEX_GLOW, mCtrlGlow->getValue().asReal());
	}
	else
	{
		// Disable all UICtrls
		clearCtrls();
		
		// Disable non-UICtrls
		mTextureCtrl->setImageAssetID( LLUUID::null );
		mTextureCtrl->setEnabled( FALSE );  // this is a LLUICtrl, but we don't want it to have keyboard focus so we add it as a child, not a ctrl.

		mColorSwatch->setEnabled( FALSE );
		mColorSwatch->setFallbackImage(LLUI::getUIImage("locked_image.j2c") );
		mColorSwatch->setValid(FALSE);

		mLabelColorTransp->setEnabled(FALSE);
		getChildView("rptctrl")->setEnabled(FALSE);
		getChildView("tex gen")->setEnabled(FALSE);
		getChildView("label shininess")->setEnabled(FALSE);
		getChildView("label bumpiness")->setEnabled(FALSE);
		getChildView("button align")->setEnabled(FALSE);
		//getChildView("has media")->setEnabled(FALSE);
		//getChildView("media info set")->setEnabled(FALSE);
		
		updateVisibility();

		// Set variable values for numeric expressions
		LLCalc* calcp = LLCalc::getInstance();
		calcp->clearVar(LLCalc::TEX_U_SCALE);
		calcp->clearVar(LLCalc::TEX_V_SCALE);
		calcp->clearVar(LLCalc::TEX_U_OFFSET);
		calcp->clearVar(LLCalc::TEX_V_OFFSET);
		calcp->clearVar(LLCalc::TEX_ROTATION);
		calcp->clearVar(LLCalc::TEX_TRANSPARENCY);
		calcp->clearVar(LLCalc::TEX_GLOW);
	}
}

//
// Static functions
//

// static
F32 LLPanelFace::valueGlow(LLViewerObject* object, S32 face)
{
	return object->getTE(face)->getGlow();
}

void LLPanelFace::onCommitShinyColor(LLUICtrl* ctrl)
{
	LLColorSwatchCtrl* cow = static_cast<LLColorSwatchCtrl*>(ctrl);
	LLSelectedTEMaterial::setSpecularLightColor(this, cow->get());
}

void LLPanelFace::onCancelColor()
{
	LLSelectMgr::getInstance()->selectionRevertColors();
}

void LLPanelFace::onCancelShinyColor()
{
	LLSelectMgr::getInstance()->selectionRevertShinyColors();
}

void LLPanelFace::onSelectColor()
{
	LLSelectMgr::getInstance()->saveSelectedObjectColors();
	sendColor();
}

void LLPanelFace::onSelectShinyColor(LLUICtrl* ctrl)
{
	LLColorSwatchCtrl* csw = static_cast<LLColorSwatchCtrl*>(ctrl);
	LLSelectedTEMaterial::setSpecularLightColor(this, csw->get());
	LLSelectMgr::getInstance()->saveSelectedShinyColors();
}

void LLPanelFace::onCommitMaterialsMedia()
{
	updateShinyControls(false,true);
	updateBumpyControls(false,true);
	refresh();
}

void LLPanelFace::updateVisibility()
{
	U32 materials_media = mComboMatMedia->getCurrentIndex();
	U32 material_type = mComboMatType->getCurrentIndex();
	bool show_media = (materials_media == MATMEDIA_MEDIA) && mComboMatMedia->getEnabled();
	bool show_texture = (show_media || ((material_type == MATTYPE_DIFFUSE) && mComboMatMedia->getEnabled()));
	bool show_bumpiness = (!show_media) && (material_type == MATTYPE_NORMAL) && mComboMatMedia->getEnabled();
	bool show_shininess = (!show_media) && (material_type == MATTYPE_SPECULAR) && mComboMatMedia->getEnabled();
	mComboMatType->setVisible(!show_media);
	
	// Media controls
	getChildView("media_info")->setVisible(show_media);
	getChildView("add_media")->setVisible(show_media);
	getChildView("delete_media")->setVisible(show_media);
	getChildView("button align")->setVisible(show_media);
	
	// Diffuse texture controls
	mTextureCtrl->setVisible(show_texture && !show_media);
	getChildView("label alphamode")->setVisible(show_texture && !show_media);
	mComboAlphaMode->setVisible(show_texture && !show_media);
	getChildView("label maskcutoff")->setVisible(false);
	getChildView("maskcutoff")->setVisible(false);
	if (show_texture && !show_media)
	{
		updateAlphaControls();
	}
	getChildView("tex gen")->setVisible(show_texture);
	mComboTexGen->setVisible(show_texture);
	getChildView("TexScaleU")->setVisible(show_texture);
	getChildView("TexScaleV")->setVisible(show_texture);
	getChildView("rptctrl")->setVisible(show_texture);
	getChildView("TexRot")->setVisible(show_texture);
	getChildView("TexOffsetU")->setVisible(show_texture);
	getChildView("TexOffsetV")->setVisible(show_texture);
	getChildView("checkbox planar align")->setVisible(show_texture);
	
	// Specular map controls
	mShinyTextureCtrl->setVisible(show_shininess);
	mComboShiny->setVisible(show_shininess);
	getChildView("label shininess")->setVisible(show_shininess);
	getChildView("label glossiness")->setVisible(false);
	getChildView("glossiness")->setVisible(false);
	getChildView("label environment")->setVisible(false);
	getChildView("environment")->setVisible(false);
	getChildView("label shinycolor")->setVisible(false);
	mShinyColorSwatch->setVisible(false);
	if (show_shininess)
	{
		updateShinyControls();
	}
	getChildView("shinyScaleU")->setVisible(show_shininess);
	getChildView("shinyScaleV")->setVisible(show_shininess);
	getChildView("shinyRot")->setVisible(show_shininess);
	getChildView("shinyOffsetU")->setVisible(show_shininess);
	getChildView("shinyOffsetV")->setVisible(show_shininess);

	// Normal map controls
	if (show_bumpiness)
	{
		updateBumpyControls();
	}
	mBumpyTextureCtrl->setVisible(show_bumpiness);
	mComboBump->setVisible(show_bumpiness);
	getChildView("label bumpiness")->setVisible(show_bumpiness);
	getChildView("bumpyScaleU")->setVisible(show_bumpiness);
	getChildView("bumpyScaleV")->setVisible(show_bumpiness);
	getChildView("bumpyRot")->setVisible(show_bumpiness);
	getChildView("bumpyOffsetU")->setVisible(show_bumpiness);
	getChildView("bumpyOffsetV")->setVisible(show_bumpiness);
}

void LLPanelFace::onCommitMaterialType()
{
	updateShinyControls(false, true);
	updateBumpyControls(false, true);
	refresh();
}

// static
void LLPanelFace::updateShinyControls(bool is_setting_texture, bool mess_with_shiny_combobox)
{
	LLUUID shiny_texture_ID = mShinyTextureCtrl->getImageAssetID();
	LL_DEBUGS("Materials") << "Shiny texture selected: " << shiny_texture_ID << LL_ENDL;

	if (mess_with_shiny_combobox)
	{
		if (!shiny_texture_ID.isNull() && is_setting_texture)
		{
			if (!mComboShiny->itemExists(USE_TEXTURE))
			{
				mComboShiny->add(USE_TEXTURE, LLSD(SHINY_TEXTURE));
			}
			mComboShiny->setSimple(USE_TEXTURE);
		}
		else
		{
			if (mComboShiny->itemExists(USE_TEXTURE))
			{
				mComboShiny->remove(SHINY_TEXTURE);
				mComboShiny->selectFirstItem();
			}
		}
	}
	
	bool show_media = mComboMatMedia->getCurrentIndex() == MATMEDIA_MEDIA && mComboMatType->getEnabled();
	bool show_shininess = (!show_media && mComboMatType->getCurrentIndex() == MATTYPE_SPECULAR
						   && mComboMatType->getEnabled());
	bool show_shinyctrls = (mComboShiny->getCurrentIndex() == SHINY_TEXTURE && show_shininess); // Use texture
	getChildView("label glossiness")->setVisible(show_shinyctrls);
	getChildView("glossiness")->setVisible(show_shinyctrls);
	getChildView("label environment")->setVisible(show_shinyctrls);
	getChildView("environment")->setVisible(show_shinyctrls);
	getChildView("label shinycolor")->setVisible(show_shinyctrls);
	mShinyColorSwatch->setVisible(show_shinyctrls);
}

void LLPanelFace::updateBumpyControls(bool is_setting_texture, bool mess_with_combobox)
{
	LLUUID bumpy_texture_ID = mBumpyTextureCtrl->getImageAssetID();
	LL_DEBUGS("Materials") << "texture: " << bumpy_texture_ID << (mess_with_combobox ? "" : " do not") << " update combobox" << LL_ENDL;

	if (mess_with_combobox)
	{
		LLUUID bumpy_texture_ID = mBumpyTextureCtrl->getImageAssetID();
		LL_DEBUGS("Materials") << "texture: " << bumpy_texture_ID << (mess_with_combobox ? "" : " do not") << " update combobox" << LL_ENDL;
		
		if (!bumpy_texture_ID.isNull() && is_setting_texture)
		{
			if (!mComboBump->itemExists(USE_TEXTURE))
			{
				mComboBump->add(USE_TEXTURE, LLSD(BUMPY_TEXTURE));
			}
			mComboBump->setSimple(USE_TEXTURE);
		}
		else
		{
			if (mComboBump->itemExists(USE_TEXTURE))
			{
				mComboBump->remove(BUMPY_TEXTURE);
				mComboBump->selectFirstItem();
			}
		}
	}
}

void LLPanelFace::onCommitShiny(const LLSD& userdata)
{
	sendShiny(userdata.asInteger());
}

void LLPanelFace::onCommitBump(const LLSD& userdata)
{
	sendBump(userdata.asInteger());
}

void LLPanelFace::updateAlphaControls()
{
	U32 alpha_value = mComboAlphaMode->getCurrentIndex();
	bool show_alphactrls = (alpha_value == ALPHAMODE_MASK); // Alpha masking
	
	U32 mat_media = MATMEDIA_MATERIAL;
	mat_media = mComboMatMedia->getCurrentIndex();
	
	U32 mat_type = MATTYPE_DIFFUSE;
	mat_type = mComboMatType->getCurrentIndex();
	
	show_alphactrls = show_alphactrls && (mat_media == MATMEDIA_MATERIAL);
	show_alphactrls = show_alphactrls && (mat_type == MATTYPE_DIFFUSE);
	
	getChildView("label maskcutoff")->setVisible(show_alphactrls);
	getChildView("maskcutoff")->setVisible(show_alphactrls);
}

void LLPanelFace::onCommitAlphaMode()
{
	updateAlphaControls();
	LLSelectedTEMaterial::setDiffuseAlphaMode(this, getCurrentDiffuseAlphaMode());
}

// static
BOOL LLPanelFace::onDragTexture(LLUICtrl*, LLInventoryItem* item)
{
	BOOL accept = TRUE;
	for (LLObjectSelection::root_iterator iter = LLSelectMgr::getInstance()->getSelection()->root_begin();
		 iter != LLSelectMgr::getInstance()->getSelection()->root_end(); iter++)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* obj = node->getObject();
		if(!LLToolDragAndDrop::isInventoryDropAcceptable(obj, item))
		{
			accept = FALSE;
			break;
		}
	}
	return accept;
}

void LLPanelFace::onCommitTexture()
{
	add(LLStatViewer::EDIT_TEXTURE, 1);
	sendTexture();
}

void LLPanelFace::onCancelTexture()
{
	LLSelectMgr::getInstance()->selectionRevertTextures();
}

void LLPanelFace::onSelectTexture()
{
	LLSelectMgr::getInstance()->saveSelectedObjectTextures();
	sendTexture();

	LLGLenum image_format;
	bool identical_image_format = false;
	LLSelectedTE::getImageFormat(image_format, identical_image_format);

	U32 alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
	{
		switch (image_format)
		{
			case GL_RGBA:
			case GL_ALPHA:
				alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
				break;
			default:
				LL_WARNS() << "Unexpected tex format in LLPanelFace...resorting to no alpha" << LL_ENDL;
			case GL_RGB:
				break;
		}

		mComboAlphaMode->selectNthItem(alpha_mode);
	}
	LLSelectedTEMaterial::setDiffuseAlphaMode(this, getCurrentDiffuseAlphaMode());
}

void LLPanelFace::onCommitSpecularTexture()
{
	sendShiny(SHINY_TEXTURE);
}

void LLPanelFace::onCommitNormalTexture()
{
	LLUUID nmap_id = getCurrentNormalMap();
	sendBump(nmap_id.isNull() ? 0 : BUMPY_TEXTURE);
}

void LLPanelFace::onCancelSpecularTexture()
{
	U8 shiny = 0;
	bool identical_shiny = false;
	LLSelectedTE::getShiny(shiny, identical_shiny);
	LLUUID spec_map_id = mShinyTextureCtrl->getImageAssetID();
	shiny = spec_map_id.isNull() ? shiny : SHINY_TEXTURE;
	sendShiny(shiny);
}

void LLPanelFace::onCancelNormalTexture()
{
	U8 bumpy = 0;
	bool identical_bumpy = false;
	LLSelectedTE::getBumpmap(bumpy, identical_bumpy);
	LLUUID spec_map_id = mBumpyTextureCtrl->getImageAssetID();
	bumpy = spec_map_id.isNull() ? bumpy : BUMPY_TEXTURE;
	sendBump(bumpy);
}

void LLPanelFace::onSelectSpecularTexture()
{
	sendShiny(SHINY_TEXTURE);
}

void LLPanelFace::onSelectNormalTexture()
{
	LLUUID nmap_id = getCurrentNormalMap();
	sendBump(nmap_id.isNull() ? 0 : BUMPY_TEXTURE);
}

//static
void LLPanelFace::onCommitMaterialBumpyOffsetX(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setNormalOffsetX(self,self->getCurrentBumpyOffsetU());
}

//static
void LLPanelFace::onCommitMaterialBumpyOffsetY(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setNormalOffsetY(self,self->getCurrentBumpyOffsetV());
}

//static
void LLPanelFace::onCommitMaterialShinyOffsetX(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setSpecularOffsetX(self,self->getCurrentShinyOffsetU());
}

//static
void LLPanelFace::onCommitMaterialShinyOffsetY(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setSpecularOffsetY(self,self->getCurrentShinyOffsetV());
}

//static
void LLPanelFace::onCommitMaterialBumpyScaleX(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	F32 bumpy_scale_u = self->getCurrentBumpyScaleU();
	if (self->isIdenticalPlanarTexgen())
	{
		bumpy_scale_u *= 0.5f;
	}
	LLSelectedTEMaterial::setNormalRepeatX(self,bumpy_scale_u);
}

//static
void LLPanelFace::onCommitMaterialBumpyScaleY(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	F32 bumpy_scale_v = self->getCurrentBumpyScaleV();
	if (self->isIdenticalPlanarTexgen())
	{
		bumpy_scale_v *= 0.5f;
	}
	LLSelectedTEMaterial::setNormalRepeatY(self,bumpy_scale_v);
}

//static
void LLPanelFace::onCommitMaterialShinyScaleX(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	F32 shiny_scale_u = self->getCurrentShinyScaleU();
	if (self->isIdenticalPlanarTexgen())
	{
		shiny_scale_u *= 0.5f;
	}
	LLSelectedTEMaterial::setSpecularRepeatX(self,shiny_scale_u);
}

//static
void LLPanelFace::onCommitMaterialShinyScaleY(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	F32 shiny_scale_v = self->getCurrentShinyScaleV();
	if (self->isIdenticalPlanarTexgen())
	{
		shiny_scale_v *= 0.5f;
	}
	LLSelectedTEMaterial::setSpecularRepeatY(self,shiny_scale_v);
}

//static
void LLPanelFace::onCommitMaterialBumpyRot(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setNormalRotation(self,self->getCurrentBumpyRot() * DEG_TO_RAD);
}

//static
void LLPanelFace::onCommitMaterialShinyRot(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setSpecularRotation(self,self->getCurrentShinyRot() * DEG_TO_RAD);
}

//static
void LLPanelFace::onCommitMaterialGloss(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setSpecularLightExponent(self,self->getCurrentGlossiness());
}

//static
void LLPanelFace::onCommitMaterialEnv(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setEnvironmentIntensity(self,self->getCurrentEnvIntensity());
}

//static
void LLPanelFace::onCommitMaterialMaskCutoff(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	LLSelectedTEMaterial::setAlphaMaskCutoff(self,self->getCurrentAlphaMaskCutoff());
}

// Commit the number of repeats per meter
void LLPanelFace::onCommitRepeatsPerMeter(LLUICtrl* ctrl)
{
	U32 materials_media = mComboMatMedia->getCurrentIndex();
	
	U32 material_type = (materials_media == MATMEDIA_MATERIAL) ? mComboMatType->getCurrentIndex() : 0;
	F32 repeats_per_meter = ctrl->getValue().asReal();
	
	F32 obj_scale_s = 1.0f;
	F32 obj_scale_t = 1.0f;
	
	bool identical_scale_s = false;
	bool identical_scale_t = false;

	LLSelectedTE::getObjectScaleS(obj_scale_s, identical_scale_s);
	LLSelectedTE::getObjectScaleS(obj_scale_t, identical_scale_t);
 
	switch (material_type)
	{
		case MATTYPE_DIFFUSE:
		{
			LLSelectMgr::getInstance()->selectionTexScaleAutofit( repeats_per_meter );
		}
		break;

		case MATTYPE_NORMAL:
		{
			LLUICtrl* bumpy_scale_u = getChild<LLUICtrl>("bumpyScaleU");
			LLUICtrl* bumpy_scale_v = getChild<LLUICtrl>("bumpyScaleV");
			
			bumpy_scale_u->setValue(obj_scale_s * repeats_per_meter);
			bumpy_scale_v->setValue(obj_scale_t * repeats_per_meter);

			LLSelectedTEMaterial::setNormalRepeatX(this, obj_scale_s * repeats_per_meter);
			LLSelectedTEMaterial::setNormalRepeatY(this, obj_scale_t * repeats_per_meter);
		}
		break;

		case MATTYPE_SPECULAR:
		{
			LLUICtrl* shiny_scale_u = getChild<LLUICtrl>("shinyScaleU");
			LLUICtrl* shiny_scale_v = getChild<LLUICtrl>("shinyScaleV");
			
			shiny_scale_u->setValue(obj_scale_s * repeats_per_meter);
			shiny_scale_v->setValue(obj_scale_t * repeats_per_meter);

			LLSelectedTEMaterial::setSpecularRepeatX(this, obj_scale_s * repeats_per_meter);
			LLSelectedTEMaterial::setSpecularRepeatY(this, obj_scale_t * repeats_per_meter);
		}
		break;

		default:
			llassert(false);
			break;
	}
}

struct LLPanelFaceSetMediaFunctor : public LLSelectedTEFunctor
{
	bool apply(LLViewerObject* object, S32 te) override
	{
		viewer_media_t pMediaImpl;
		
		const LLTextureEntry* tep = object->getTE(te);
		const LLMediaEntry* mep = tep->hasMedia() ? tep->getMediaData() : NULL;
		if ( mep )
		{
			pMediaImpl = LLViewerMedia::getMediaImplFromTextureID(mep->getMediaID());
		}
		
		if ( pMediaImpl.isNull())
		{
			// If we didn't find face media for this face, check whether this face is showing parcel media.
			pMediaImpl = LLViewerMedia::getMediaImplFromTextureID(tep->getID());
		}
		
		if ( pMediaImpl.notNull())
		{
			LLPluginClassMedia *media = pMediaImpl->getMediaPlugin();
			if(media)
			{
				S32 media_width = media->getWidth();
				S32 media_height = media->getHeight();
				S32 texture_width = media->getTextureWidth();
				S32 texture_height = media->getTextureHeight();
				F32 scale_s = (F32)media_width / (F32)texture_width;
				F32 scale_t = (F32)media_height / (F32)texture_height;

				// set scale and adjust offset
				object->setTEScaleS( te, scale_s );
				object->setTEScaleT( te, scale_t );	// don't need to flip Y anymore since QT does this for us now.
				object->setTEOffsetS( te, -( 1.0f - scale_s ) / 2.0f );
				object->setTEOffsetT( te, -( 1.0f - scale_t ) / 2.0f );
			}
		}
		return true;
	};
};

void LLPanelFace::onClickAutoFix()
{
	LLPanelFaceSetMediaFunctor setfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);

	LLPanelFaceSendFunctor sendfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToObjects(&sendfunc);
}

void LLPanelFace::onClickAlignMats(const LLSD& userdata)
{
	refresh();
	if (userdata.asBoolean())
	{
		alignMaterialProperties();
	}
}

void LLPanelFace::alignMaterialProperties()
{
	bool has_bumpy = (bool)getCurrentNormalMap().notNull();
	bool has_shiny = (bool)getCurrentSpecularMap().notNull();
	
	if (!(has_shiny || has_bumpy)) return;
	
	F32 tex_scale_u = getCurrentTextureScaleU();
	F32 tex_scale_v = getCurrentTextureScaleV();
	F32 tex_offset_u = normalize(getCurrentTextureOffsetU(), 0.f, 1.f);
	F32 tex_offset_v = normalize(getCurrentTextureOffsetV(), 0.f, 1.f);
	F32 tex_rot = normalize(getCurrentTextureRot(), 0.f, 360.f);
	
	bool identical_planar_texgen = isIdenticalPlanarTexgen();
	
	if (has_shiny)
	{
		childSetValue("shinyScaleU",	tex_scale_u);
		childSetValue("shinyScaleV",	tex_scale_v);
		childSetValue("shinyOffsetU",	tex_offset_u);
		childSetValue("shinyOffsetV",	tex_offset_v);
		childSetValue("shinyRot",		tex_rot);
		
		LLSelectedTEMaterial::setSpecularRepeatX(this, identical_planar_texgen ? tex_scale_u * 0.5f : tex_scale_u);
		LLSelectedTEMaterial::setSpecularRepeatY(this, identical_planar_texgen ? tex_scale_v * 0.5f : tex_scale_v);
		LLSelectedTEMaterial::setSpecularOffsetX(this, identical_planar_texgen ? tex_offset_u * 0.5f : tex_offset_u);
		LLSelectedTEMaterial::setSpecularOffsetY(this, identical_planar_texgen ? tex_offset_v * 0.5f : tex_offset_v);
		LLSelectedTEMaterial::setSpecularRotation(this, tex_rot * DEG_TO_RAD);
	}
	if (has_bumpy)
	{
		childSetValue("bumpyScaleU",	tex_scale_u);
		childSetValue("bumpyScaleV",	tex_scale_v);
		childSetValue("bumpyOffsetU",	tex_offset_u);
		childSetValue("bumpyOffsetV",	tex_offset_v);
		childSetValue("bumpyRot",		tex_rot);
		
		LLSelectedTEMaterial::setNormalRepeatX(this, identical_planar_texgen ? tex_scale_u * 0.5f : tex_scale_u);
		LLSelectedTEMaterial::setNormalRepeatY(this, identical_planar_texgen ? tex_scale_v * 0.5f : tex_scale_v);
		LLSelectedTEMaterial::setNormalOffsetX(this, identical_planar_texgen ? tex_offset_u * 0.5f : tex_offset_u);
		LLSelectedTEMaterial::setNormalOffsetY(this, identical_planar_texgen ? tex_offset_v * 0.5f : tex_offset_v);
		LLSelectedTEMaterial::setNormalRotation(this, tex_rot * DEG_TO_RAD);
	}
}

void LLPanelFace::onCommitPlanarAlign()
{
	refresh();
	sendTextureInfo();
}

void LLPanelFace::onTextureSelectionChanged(LLInventoryItem* itemp)
{
	LL_DEBUGS("Materials") << "item asset " << itemp->getAssetUUID() << LL_ENDL;
	U32 mattype = mComboMatType->getCurrentIndex();
	LLTextureCtrl* texture_ctrl = (mattype == MATTYPE_SPECULAR ? mShinyTextureCtrl
										: mattype == MATTYPE_NORMAL ? mBumpyTextureCtrl
										: mTextureCtrl);
	LLUUID obj_owner_id;
	std::string obj_owner_name;
	LLSelectMgr::instance().selectGetOwner(obj_owner_id, obj_owner_name);
		
	LLSaleInfo sale_info;
	LLSelectMgr::instance().selectGetSaleInfo(sale_info);
		
	bool can_copy = itemp->getPermissions().allowCopyBy(gAgentID); // do we have perm to copy this texture?
	bool can_transfer = itemp->getPermissions().allowOperationBy(PERM_TRANSFER, gAgentID); // do we have perm to transfer this texture?
	bool is_object_owner = gAgentID == obj_owner_id; // does object for which we are going to apply texture belong to the agent?
		
	if (can_copy && can_transfer)
	{
		texture_ctrl->setCanApply(true, true);
		return;
	}
		
	// if texture has (no-transfer) attribute it can be applied only for object which we own and is not for sale
	texture_ctrl->setCanApply(false, can_transfer ? true : is_object_owner && !sale_info.isForSale());
		
	if (gSavedSettings.getBOOL("TextureLivePreview"))
	{
		LLNotificationsUtil::add("LivePreviewUnavailable");
	}
}

bool LLPanelFace::isIdenticalPlanarTexgen()
{
	LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
	bool identical_texgen = false;
	LLSelectedTE::getTexGen(selected_texgen, identical_texgen);
	return (identical_texgen && (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR));
}

void LLPanelFace::LLSelectedTE::getFace(LLFace*& face_to_return, bool& identical_face)
{		
	struct LLSelectedTEGetFace : public LLSelectedTEGetFunctor<LLFace *>
	{
		LLFace* get(LLViewerObject* object, S32 te) override
		{
			return (object->mDrawable) ? object->mDrawable->getFace(te): NULL;
		}
	} get_te_face_func;
	identical_face = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&get_te_face_func, face_to_return);
}

void LLPanelFace::LLSelectedTE::getImageFormat(LLGLenum& image_format_to_return, bool& identical_face)
{
	LLGLenum image_format = 0;
	struct LLSelectedTEGetImageFormat : public LLSelectedTEGetFunctor<LLGLenum>
	{
		LLGLenum get(LLViewerObject* object, S32 te_index) override
		{
			LLViewerTexture* image = object->getTEImage(te_index);
			return image ? image->getPrimaryFormat() : GL_RGB;
		}
	} get_glenum;
	identical_face = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&get_glenum, image_format);
	image_format_to_return = image_format;
}

void LLPanelFace::LLSelectedTE::getTexId(LLUUID& id, bool& identical)
{
	struct LLSelectedTEGetTexId : public LLSelectedTEGetFunctor<LLUUID>
	{
		LLUUID get(LLViewerObject* object, S32 te_index) override
		{
			LLUUID id;
			LLViewerTexture* image = object->getTEImage(te_index);
			if (image)
			{
				id = image->getID();
			}

			if (!id.isNull() && LLViewerMedia::textureHasMedia(id))
			{
				LLTextureEntry *te = object->getTE(te_index);
				if (te)
				{
					LLViewerTexture* tex = te->getID().notNull() ? gTextureList.findImage(te->getID(), TEX_LIST_STANDARD) : NULL;
					if(!tex)
					{
						tex = LLViewerFetchedTexture::sDefaultImagep;
					}
					if (tex)
					{
						id = tex->getID();
					}
				}
			}
			return id;
		}
	} func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, id );
}

void LLPanelFace::LLSelectedTEMaterial::getCurrent(LLMaterialPtr& material_ptr, bool& identical_material)
{
	struct MaterialFunctor : public LLSelectedTEGetFunctor<LLMaterialPtr>
	{
		LLMaterialPtr get(LLViewerObject* object, S32 te_index) override
		{
			return object->getTE(te_index)->getMaterialParams();
		}
	} func;
	identical_material = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, material_ptr);
}

void LLPanelFace::LLSelectedTEMaterial::getMaxSpecularRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxSpecRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face) override
		{
			LLMaterial* mat = object->getTE(face)->getMaterialParams().get();
			U32 s_axis = VX;
			U32 t_axis = VY;
			F32 repeats_s = 1.0f;
			F32 repeats_t = 1.0f;
			if (mat)
			{
				mat->getSpecularRepeat(repeats_s, repeats_t);
				repeats_s /= object->getScale().mV[s_axis];
				repeats_t /= object->getScale().mV[t_axis];
			}
			return llmax(repeats_s, repeats_t);
		}

	} max_spec_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_spec_repeats_func, repeats);
}

void LLPanelFace::LLSelectedTEMaterial::getMaxNormalRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxNormRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face) override
		{
			LLMaterial* mat = object->getTE(face)->getMaterialParams().get();
			U32 s_axis = VX;
			U32 t_axis = VY;
			F32 repeats_s = 1.0f;
			F32 repeats_t = 1.0f;
			if (mat)
			{
				mat->getNormalRepeat(repeats_s, repeats_t);
				repeats_s /= object->getScale().mV[s_axis];
				repeats_t /= object->getScale().mV[t_axis];
			}
			return llmax(repeats_s, repeats_t);
		}

	} max_norm_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_norm_repeats_func, repeats);
}

void LLPanelFace::LLSelectedTEMaterial::getCurrentDiffuseAlphaMode(U8& diffuse_alpha_mode, bool& identical, bool diffuse_texture_has_alpha)
{
	struct LLSelectedTEGetDiffuseAlphaMode : public LLSelectedTEGetFunctor<U8>
	{
		LLSelectedTEGetDiffuseAlphaMode() : _isAlpha(false) {}
		LLSelectedTEGetDiffuseAlphaMode(bool diffuse_texture_has_alpha) : _isAlpha(diffuse_texture_has_alpha) {}
		virtual ~LLSelectedTEGetDiffuseAlphaMode() {}

		U8 get(LLViewerObject* object, S32 face) override
		{
			U8 diffuse_mode = _isAlpha ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

			LLTextureEntry* tep = object->getTE(face);
			if (tep)
			{
				LLMaterial* mat = tep->getMaterialParams().get();
				if (mat)
				{
					diffuse_mode = mat->getDiffuseAlphaMode();
				}
			}
			
			return diffuse_mode;
		}
		bool _isAlpha; // whether or not the diffuse texture selected contains alpha information
	} get_diff_mode(diffuse_texture_has_alpha);
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &get_diff_mode, diffuse_alpha_mode);
}

void LLPanelFace::LLSelectedTE::getObjectScaleS(F32& scale_s, bool& identical)
{
	struct LLSelectedTEGetObjectScaleS : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face) override
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			return object->getScale().mV[s_axis];
		}

	} scale_s_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &scale_s_func, scale_s );
}

void LLPanelFace::LLSelectedTE::getObjectScaleT(F32& scale_t, bool& identical)
{
	struct LLSelectedTEGetObjectScaleS : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face) override
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			return object->getScale().mV[t_axis];
		}

	} scale_t_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &scale_t_func, scale_t );
}

void LLPanelFace::LLSelectedTE::getMaxDiffuseRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxDiffuseRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face) override
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			F32 repeats_s = object->getTE(face)->mScaleS / object->getScale().mV[s_axis];
			F32 repeats_t = object->getTE(face)->mScaleT / object->getScale().mV[t_axis];
			return llmax(repeats_s, repeats_t);
		}

	} max_diff_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_diff_repeats_func, repeats );
}
