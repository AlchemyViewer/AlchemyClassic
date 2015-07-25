/**
 * @file alpanelquicksettings.h
 * @brief Base panel for quick settings popdown and floater
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2013-2014, Alchemy Viewer Project.
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
 * $/LicenseInfo$
 */

#ifndef AL_ALPANELQUICKSETTINGS_H
#define AL_ALPANELQUICKSETTINGS_H

#include "llpanel.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLSlider;
class LLSpinCtrl;

class ALPanelQuickSettings : public LLPanel
{
public:
	ALPanelQuickSettings();
	~ALPanelQuickSettings();

	BOOL postBuild() override;
	void refresh() override;

private:
	void onSwitchRegionSettings();
	void onClickWindlightPrev(bool water_or_sky);
	void onClickWindlightNext(bool water_or_sky);

	void applyWindlight();
	void populateWaterPresetsList();
	void populateSkyPresetsList();
	
	void onHoverSliderMoved(const LLSD& val);
	void onHoverSliderFinalCommit();
	void syncFromPreferenceSetting();
	
	void onRegionChanged();
	void onSimulatorFeaturesReceived(const LLUUID &region_id);
	void updateEditHoverEnabled();

	LLButton* mWaterPrevBtn;
	LLButton* mWaterNextBtn;
	LLButton* mSkyPrevBtn;
	LLButton* mSkyNextBtn;

	LLCheckBoxCtrl* mRegionSettingsCheckBox;

	LLComboBox* mWaterPresetCombo;
	LLComboBox* mSkyPresetCombo;
	
	LLSlider* mHoverSlider;
	LLSpinCtrl* mHoverSpinner;
	
	boost::signals2::connection mRegionChangedSlot;
};

#endif // AL_ALPANELQUICKSETTINGS_H
