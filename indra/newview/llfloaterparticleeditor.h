/** 
 * @file llfloaterparticleeditor.h
 * @brief Particle Editor
 *
 * Copyright (C) 2011, Zi Ree @ Second Life
 * Copyright (C) 2015, Cinder Roxley <cinder@sdf.org>
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
 */

#ifndef LL_FLOATERPARTICLEEDITOR_H
#define LL_FLOATERPARTICLEEDITOR_H

#include "llfloater.h"
#include "llpartdata.h"
#include "llviewerinventory.h"

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLTextureCtrl;
class LLUICtrl;
class LLViewerObject;
class LLViewerTexture;

const std::string PARTICLE_SCRIPT_NAME = "New Particle Script";

class LLFloaterParticleEditor : public LLFloater
{
public:
	LLFloaterParticleEditor(const LLSD& key);
	/* virtual */ ~LLFloaterParticleEditor();

	/* virtual */ BOOL postBuild() override;

	void setObject(LLViewerObject* objectp);

private:
	void clearParticles();
	void updateParticles();
	void updateUI();

	std::string createScript();

	void onParameterChange();
	void onClickCopy();
	void onClickInject();

	void onClickClearTarget();
	void onClickTargetPicker();
	static void onTargetPicked(void* userdata);

	void callbackReturned(const LLUUID& inv_item);

	std::string lslVector(F32 x, F32 y, F32 z);
	std::string lslColor(const LLColor4& color);

	LLViewerObject* mObject;
	LLViewerTexture* mTexture;
	LLViewerInventoryItem* mParticleScriptInventoryItem;

	LLViewerTexture* mDefaultParticleTexture;
	
	LLPartSysData mParticles;

	LLComboBox* mPatternTypeCombo;
	LLTextureCtrl* mTexturePicker;

	LLUICtrl* mBurstRateCtrl;
	LLUICtrl* mBurstCountCtrl;
	LLUICtrl* mBurstRadiusCtrl;
	LLUICtrl* mAngleBeginCtrl;
	LLUICtrl* mAngleEndCtrl;
	LLUICtrl* mBurstSpeedMinCtrl;
	LLUICtrl* mBurstSpeedMaxCtrl;
	LLUICtrl* mStartAlphaCtrl;
	LLUICtrl* mEndAlphaCtrl;
	LLUICtrl* mScaleStartXCtrl;
	LLUICtrl* mScaleStartYCtrl;
	LLUICtrl* mScaleEndXCtrl;
	LLUICtrl* mScaleEndYCtrl;
	LLUICtrl* mSourceMaxAgeCtrl;
	LLUICtrl* mParticlesMaxAgeCtrl;
	LLUICtrl* mStartGlowCtrl;
	LLUICtrl* mEndGlowCtrl;
	
	LLUICtrl* mAcellerationXCtrl;
	LLUICtrl* mAcellerationYCtrl;
	LLUICtrl* mAcellerationZCtrl;
	
	LLUICtrl* mOmegaXCtrl;
	LLUICtrl* mOmegaYCtrl;
	LLUICtrl* mOmegaZCtrl;

	LLComboBox* mBlendFuncSrcCombo;
	LLComboBox* mBlendFuncDestCombo;

	LLCheckBoxCtrl* mBounceCheckBox;
	LLCheckBoxCtrl* mEmissiveCheckBox;
	LLCheckBoxCtrl* mFollowSourceCheckBox;
	LLCheckBoxCtrl* mFollowVelocityCheckBox;
	LLCheckBoxCtrl* mInterpolateColorCheckBox;
	LLCheckBoxCtrl* mInterpolateScaleCheckBox;
	LLCheckBoxCtrl* mTargetPositionCheckBox;
	LLCheckBoxCtrl* mTargetLinearCheckBox;
	LLCheckBoxCtrl* mWindCheckBox;
	LLCheckBoxCtrl* mRibbonCheckBox;

	LLUICtrl* mTargetKeyInput;
	LLButton* mClearTargetButton;
	LLButton* mPickTargetButton;
	LLButton* mCopyToLSLButton;
	LLButton* mInjectScriptButton;

	LLColorSwatchCtrl* mStartColorSelector;
	LLColorSwatchCtrl* mEndColorSelector;
	
    static void finishUpload(LLUUID itemId, LLUUID taskId, LLUUID newAssetId, LLSD response, bool isRunning, LLUUID objectId);
    
	class LLParticleScriptCreationCallback : public LLInventoryCallback
	{
	public:
		LLParticleScriptCreationCallback(LLFloaterParticleEditor* editor);
		void fire(const LLUUID& inventoryItem) override;
		
	protected:
		~LLParticleScriptCreationCallback() {}
		
		LLFloaterParticleEditor* mEditor;
	};
};

#endif // LL_FLOATERPARTICLEEDITOR_H
