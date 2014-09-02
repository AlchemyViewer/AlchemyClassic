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

#include "llenvmanager.h"
#include "llwaterparammanager.h"
#include "llwlparammanager.h"

static LLPanelInjector<ALPanelQuickSettings> t_quick_settings("quick_settings");

ALPanelQuickSettings::ALPanelQuickSettings()
	: LLPanel(),
	mWaterPrevBtn(NULL),
	mWaterNextBtn(NULL),
	mSkyPrevBtn(NULL),
	mSkyNextBtn(NULL),
	mRegionSettingsCheckBox(NULL),
	mWaterPresetCombo(NULL),
	mSkyPresetCombo(NULL)
{
}

// virtual
BOOL ALPanelQuickSettings::postBuild()
{
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
