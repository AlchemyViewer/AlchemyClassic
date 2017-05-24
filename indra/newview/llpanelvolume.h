/** 
 * @file llpanelvolume.h
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

#ifndef LL_LLPANELVOLUME_H
#define LL_LLPANELVOLUME_H

#include "v3math.h"
#include "llpanel.h"
#include "llpointer.h"
#include "llvolume.h"

class LLSpinCtrl;
class LLCheckBoxCtrl;
class LLTextBox;
class LLUICtrl;
class LLButton;
class LLViewerObject;
class LLComboBox;
class LLColorSwatchCtrl;
class LLTextureCtrl;

class LLPanelVolume : public LLPanel
{
public:
	LLPanelVolume();
	virtual ~LLPanelVolume();

	void	draw() override;
	void 	clearCtrls() override;

	BOOL	postBuild() override;

	void refresh() override;

	void sendIsLight();
	void sendIsFlexible();

	
	void onCommitIsLight();
	void onCommitLight();
	void onCommitIsFlexible();
	void onCommitFlexible();
	void onCommitMaterial(LLUICtrl* ctrl);

	void onLightCancelColor(const LLSD& data);
	void onLightSelectColor(const LLSD& data);

	void onLightCancelTexture(const LLSD& data); 
	void onLightSelectTexture(const LLSD& data);


protected:
	void			getState();
	void			refreshCost();

protected:
	void sendPhysicsShapeType(const LLSD& user_data);
	void sendPhysicsGravity(const LLSD& user_data);
	void sendPhysicsFriction(const LLSD& user_data);
	void sendPhysicsRestitution(const LLSD& user_data);
	void sendPhysicsDensity(const LLSD& user_data);

	void            handleResponseChangeToFlexible(const LLSD &pNotification, const LLSD &pResponse);

	LLTextBox*		mLabelSelectSingleMessage = nullptr;
	LLTextBox*		mLabelEditObject = nullptr;

	// Light
	LLCheckBoxCtrl*	mCheckLight = nullptr;
	LLTextBox*		mLabelLightColor = nullptr;
	LLTextureCtrl*		mLightTexturePicker = nullptr;
	LLColorSwatchCtrl* mLightColorSwatch = nullptr;
	LLSpinCtrl*		mLightIntensity = nullptr;
	LLSpinCtrl*		mLightRadius = nullptr;
	LLSpinCtrl*		mLightFalloff = nullptr;
	LLSpinCtrl*		mLightFOV = nullptr;
	LLSpinCtrl*		mLightFocus = nullptr;
	LLSpinCtrl*		mLightAmbiance = nullptr;

	// Flexibile
	LLCheckBoxCtrl*	mCheckFlexible1D;
	LLSpinCtrl*		mFlexSections = nullptr;
	LLSpinCtrl*		mFlexGravity = nullptr;
	LLSpinCtrl*		mFlexTension = nullptr;
	LLSpinCtrl*		mFlexFriction = nullptr;
	LLSpinCtrl*		mFlexWind = nullptr;
	LLSpinCtrl*		mFlexForce[3] = { nullptr, nullptr, nullptr };

	// Physics 
	LLTextBox*		mLabelPhysicsShapeType = nullptr;
	LLComboBox*     mComboPhysicsShapeType = nullptr;
	LLSpinCtrl*     mSpinPhysicsGravity = nullptr;
	LLSpinCtrl*     mSpinPhysicsFriction = nullptr;
	LLSpinCtrl*     mSpinPhysicsDensity = nullptr;
	LLSpinCtrl*     mSpinPhysicsRestitution = nullptr;

	S32			mComboMaterialItemCount;
	LLComboBox*		mComboMaterial = nullptr;
	

	LLColor4		mLightSavedColor;
	LLUUID			mLightSavedTexture;
	LLPointer<LLViewerObject> mObject;
	LLPointer<LLViewerObject> mRootObject;


};

#endif
