// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file alpanelquicksettings.cpp
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

#include "llviewerprecompiledheaders.h"

#include "alpanelquicksettings.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llslider.h"
#include "llspinctrl.h"

#include "llenvmanager.h"
#include "llwaterparammanager.h"
#include "llwlparammanager.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"

static LLPanelInjector<ALPanelQuickSettings> t_quick_settings("quick_settings");

ALPanelQuickSettings::ALPanelQuickSettings()
	: LLPanel(),
	mWaterPrevBtn(nullptr),
	mWaterNextBtn(nullptr),
	mSkyPrevBtn(nullptr),
	mSkyNextBtn(nullptr),
	mRegionSettingsCheckBox(nullptr),
	mWaterPresetCombo(nullptr),
	mSkyPresetCombo(nullptr),
	mHoverSlider(nullptr),
	mHoverSpinner(nullptr)
{
}

ALPanelQuickSettings::~ALPanelQuickSettings()
{
	if (mRegionChangedSlot.connected())
	{
		mRegionChangedSlot.disconnect();
	}
}

// virtual
BOOL ALPanelQuickSettings::postBuild()
{
	// Windlight
	mRegionSettingsCheckBox = getChild<LLCheckBoxCtrl>("region_settings_checkbox");
	mRegionSettingsCheckBox->setCommitCallback(boost::bind(&ALPanelQuickSettings::onSwitchRegionSettings, this));

	mWaterPresetCombo = getChild<LLComboBox>("water_settings_preset_combo");
	mWaterPresetCombo->setCommitCallback(boost::bind(&ALPanelQuickSettings::applyWindlight, this));
	mWaterPrevBtn = getChild<LLButton>("water_settings_prev_btn");
	mWaterPrevBtn->setCommitCallback(boost::bind(&ALPanelQuickSettings::onClickWindlightPrev, this, true));
	mWaterNextBtn = getChild<LLButton>("water_settings_next_btn");
	mWaterNextBtn->setCommitCallback(boost::bind(&ALPanelQuickSettings::onClickWindlightNext, this, true));
	populateWaterPresetsList();

	mSkyPresetCombo = getChild<LLComboBox>("sky_settings_preset_combo");
	mSkyPresetCombo->setCommitCallback(boost::bind(&ALPanelQuickSettings::applyWindlight, this));
	mSkyPrevBtn = getChild<LLButton>("sky_settings_prev_btn");
	mSkyPrevBtn->setCommitCallback(boost::bind(&ALPanelQuickSettings::onClickWindlightPrev, this, false));
	mSkyNextBtn = getChild<LLButton>("sky_settings_next_btn");
	mSkyNextBtn->setCommitCallback(boost::bind(&ALPanelQuickSettings::onClickWindlightNext, this, false));
	populateSkyPresetsList();

	LLEnvManagerNew::instance().setPreferencesChangeCallback(boost::bind(&ALPanelQuickSettings::refresh, this));
	LLWLParamManager::instance().setPresetListChangeCallback(boost::bind(&ALPanelQuickSettings::populateSkyPresetsList, this));
	LLWaterParamManager::instance().setPresetListChangeCallback(boost::bind(&ALPanelQuickSettings::populateWaterPresetsList, this));

	refresh();

	// Hover height
	mHoverSlider = getChild<LLSlider>("hover_slider_bar");
	mHoverSlider->setMinValue(MIN_HOVER_Z);
	mHoverSlider->setMaxValue(MAX_HOVER_Z);
	mHoverSlider->setMouseUpCallback(boost::bind(&ALPanelQuickSettings::onHoverSliderFinalCommit, this));
	mHoverSlider->setCommitCallback(boost::bind(&ALPanelQuickSettings::onHoverSliderMoved, this, _2));

	mHoverSpinner = getChild<LLSpinCtrl>("hover_spinner");
	mHoverSpinner->setMinValue(MIN_HOVER_Z);
	mHoverSpinner->setMaxValue(MAX_HOVER_Z);

	// Initialize slider from pref setting.
	syncFromPreferenceSetting();

	// Update slider on future pref changes.
	gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ")->getCommitSignal()->connect(boost::bind(&ALPanelQuickSettings::syncFromPreferenceSetting, this));

	updateEditHoverEnabled();

	if (!mRegionChangedSlot.connected())
	{
		mRegionChangedSlot = gAgent.addRegionChangedCallback(boost::bind(&ALPanelQuickSettings::onRegionChanged, this));
	}
	// Set up based on initial region.
	onRegionChanged();

	return LLPanel::postBuild();
}

// virtual
void ALPanelQuickSettings::refresh()
{
	LLEnvManagerNew& env_mgr = LLEnvManagerNew::instance();
	bool use_region_settings = env_mgr.getUseRegionSettings();

	// Set up radio buttons according to user preferences.
	mRegionSettingsCheckBox->set(use_region_settings);

	// Enable/disable other controls based on user preferences.
	mWaterPresetCombo->setEnabled(!use_region_settings);
	mWaterPrevBtn->setEnabled(!use_region_settings);
	mWaterNextBtn->setEnabled(!use_region_settings);
	mSkyPresetCombo->setEnabled(!use_region_settings);
	mSkyPrevBtn->setEnabled(!use_region_settings);
	mSkyNextBtn->setEnabled(!use_region_settings);

	// Select the current presets in combo boxes.
	mWaterPresetCombo->selectByValue(env_mgr.getWaterPresetName());
	mSkyPresetCombo->selectByValue(env_mgr.getSkyPresetName());
}

void ALPanelQuickSettings::onSwitchRegionSettings()
{
	mWaterPresetCombo->setEnabled(!mRegionSettingsCheckBox->get());
	mSkyPresetCombo->setEnabled(!mRegionSettingsCheckBox->get());

	applyWindlight();
}

void ALPanelQuickSettings::onClickWindlightPrev(bool water_or_sky)
{
	LLComboBox* windlightCombo = water_or_sky ? mWaterPresetCombo : mSkyPresetCombo;
	S32 previousIndex = windlightCombo->getCurrentIndex() - 1;
	S32 lastItem = windlightCombo->getItemCount() - 1;
	if (previousIndex < 0)
		previousIndex = lastItem;
	else if (previousIndex > lastItem)
		previousIndex = 0;

	if (!windlightCombo->setCurrentByIndex(previousIndex))
	{
		--previousIndex;
		windlightCombo->setCurrentByIndex(previousIndex);
	}

	applyWindlight();
}

void ALPanelQuickSettings::onClickWindlightNext(bool water_or_sky)
{
	LLComboBox* windlightCombo = water_or_sky ? mWaterPresetCombo : mSkyPresetCombo;
	S32 nextIndex = windlightCombo->getCurrentIndex() + 1;
	S32 lastItem = windlightCombo->getItemCount() - 1;
	if (nextIndex < 0)
		nextIndex = lastItem;
	else if (nextIndex > lastItem)
		nextIndex = 0;

	if (!windlightCombo->setCurrentByIndex(nextIndex))
	{
		++nextIndex;
		windlightCombo->setCurrentByIndex(nextIndex);
	}

	applyWindlight();
}

void ALPanelQuickSettings::applyWindlight()
{
	// Update environment with the user choice.
	const bool use_region_settings = mRegionSettingsCheckBox->getValue();
	const std::string& water_preset = mWaterPresetCombo->getValue().asString();
	const std::string& sky_preset = mSkyPresetCombo->getValue().asString();

	LLEnvManagerNew& env_mgr = LLEnvManagerNew::instance();
	if (use_region_settings)
	{
		env_mgr.setUseRegionSettings(use_region_settings);
	}
	else
	{
		env_mgr.setUseSkyPreset(sky_preset);
		env_mgr.setUseWaterPreset(water_preset);
	}
}

void ALPanelQuickSettings::populateWaterPresetsList()
{
	mWaterPresetCombo->removeall();

	LLWLParamManager::preset_name_list_t user_presets, system_presets;
	LLWaterParamManager::instance().getPresetNames(user_presets, system_presets);

	// Add user presets first.
	for (const auto& user_preset_string : user_presets)
	{
		mWaterPresetCombo->add(user_preset_string);
	}

	if (!user_presets.empty())
	{
		mWaterPresetCombo->addSeparator();
	}

	// Add system presets.
	for (const auto& system_preset_string : system_presets)
	{
		mWaterPresetCombo->add(system_preset_string);
	}
}

void ALPanelQuickSettings::populateSkyPresetsList()
{
	mSkyPresetCombo->removeall();

	LLWLParamManager::preset_name_list_t region_presets, user_presets, system_presets;
	LLWLParamManager::instance().getPresetNames(region_presets, user_presets, system_presets);

	// Add user presets.
	for (const auto& user_preset_string : user_presets)
	{
		mSkyPresetCombo->add(user_preset_string);
	}

	if (!user_presets.empty())
	{
		mSkyPresetCombo->addSeparator();
	}

	// Add system presets.
	for (const auto& system_preset_string : system_presets)
	{
		mSkyPresetCombo->add(system_preset_string);
	}
}

void ALPanelQuickSettings::syncFromPreferenceSetting()
{
	F32 value = gSavedPerAccountSettings.getF32("AvatarHoverOffsetZ");
	mHoverSlider->setValue(value, FALSE);
	mHoverSpinner->setValue(value);

	if (isAgentAvatarValid())
	{
		LLVector3 offset(0.0, 0.0, llclamp(value, MIN_HOVER_Z, MAX_HOVER_Z));
		LL_INFOS("Avatar") << "setting hover from preference setting " << offset[2] << LL_ENDL;
		gAgentAvatarp->setHoverOffset(offset);
	}
}

void ALPanelQuickSettings::onHoverSliderMoved(const LLSD& val)
{
	if (isAgentAvatarValid())
	{
		F32 value = val.asReal();
		LLVector3 offset(0.0, 0.0, llclamp(value, MIN_HOVER_Z, MAX_HOVER_Z));
		LL_INFOS("Avatar") << "setting hover from slider moved" << offset[2] << LL_ENDL;
		gAgentAvatarp->setHoverOffset(offset, false);
	}
}

// Do send-to-the-server work when slider drag completes, or new
// value entered as text.
void ALPanelQuickSettings::onHoverSliderFinalCommit()
{
	F32 value = mHoverSlider->getValueF32();
	gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ", value);
	if (isAgentAvatarValid())
	{
		LLVector3 offset(0.0, 0.0, llclamp(value, MIN_HOVER_Z, MAX_HOVER_Z));
		LL_INFOS("Avatar") << "setting hover from slider final commit " << offset[2] << LL_ENDL;
		gAgentAvatarp->setHoverOffset(offset, true); // will send update this time.
	}
}

void ALPanelQuickSettings::onRegionChanged()
{
	LLViewerRegion *region = gAgent.getRegion();
	if (region && region->simulatorFeaturesReceived())
	{
		updateEditHoverEnabled();
	}
	else if (region)
	{
		region->setSimulatorFeaturesReceivedCallback(boost::bind(&ALPanelQuickSettings::onSimulatorFeaturesReceived, this, _1));
	}
}

void ALPanelQuickSettings::onSimulatorFeaturesReceived(const LLUUID &region_id)
{
	LLViewerRegion *region = gAgent.getRegion();
	if (region && (region->getRegionID() == region_id))
	{
		updateEditHoverEnabled();
	}
}

void ALPanelQuickSettings::updateEditHoverEnabled()
{
	bool enabled = gAgent.getRegion() && gAgent.getRegion()->avatarHoverHeightEnabled();
	mHoverSlider->setEnabled(enabled);
	mHoverSpinner->setEnabled(enabled);
	if (enabled)
	{
		syncFromPreferenceSetting();
	}
}
