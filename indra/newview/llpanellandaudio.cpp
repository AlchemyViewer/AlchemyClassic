/**
 * @file llpanellandaudio.cpp
 * @brief Allows configuration of "media" for a land parcel,
 *   for example movies, web pages, and audio.
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#include "llpanellandaudio.h"

// viewer includes
#include "llmimetypes.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "lluictrlfactory.h"

// library includes
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterurlentry.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llparcel.h"
#include "lltextbox.h"
#include "llradiogroup.h"
#include "llspinctrl.h"
#include "llsdutil.h"
#include "lltexturectrl.h"
#include "roles_constants.h"
#include "llscrolllistctrl.h"

// Values for the parcel voice settings radio group
enum
{
	kRadioVoiceChatEstate = 0,
	kRadioVoiceChatPrivate = 1,
	kRadioVoiceChatDisable = 2
};

//---------------------------------------------------------------------------
// LLPanelLandAudio
//---------------------------------------------------------------------------

LLPanelLandAudio::LLPanelLandAudio(LLParcelSelectionHandle& parcel)
:	LLPanel(/*std::string("land_media_panel")*/), mParcel(parcel)
{
}


// virtual
LLPanelLandAudio::~LLPanelLandAudio()
{
}


BOOL LLPanelLandAudio::postBuild()
{
	mCheckSoundLocal = getChild<LLCheckBoxCtrl>("check sound local");
	mCheckSoundLocal->setCommitCallback(boost::bind(&LLPanelLandAudio::onCommitAny, this));

	mCheckParcelEnableVoice = getChild<LLCheckBoxCtrl>("parcel_enable_voice_channel");
	mCheckParcelEnableVoice->setCommitCallback(boost::bind(&LLPanelLandAudio::onCommitAny, this));

	// This one is always disabled so no need for a commit callback
	mCheckEstateDisabledVoice = getChild<LLCheckBoxCtrl>("parcel_enable_voice_channel_is_estate_disabled");

	mCheckParcelVoiceLocal = getChild<LLCheckBoxCtrl>("parcel_enable_voice_channel_local");
	mCheckParcelVoiceLocal->setCommitCallback(boost::bind(&LLPanelLandAudio::onCommitAny, this));

	mMusicURLEdit = getChild<LLLineEditor>("music_url");
	mMusicURLEdit->setCommitCallback(boost::bind(&LLPanelLandAudio::onCommitAny, this));

	mCheckAVSoundAny = getChild<LLCheckBoxCtrl>("all av sound check");
	mCheckAVSoundAny->setCommitCallback(boost::bind(&LLPanelLandAudio::onCommitAny, this));

	mCheckAVSoundGroup = getChild<LLCheckBoxCtrl>("group av sound check");
	mCheckAVSoundGroup->setCommitCallback(boost::bind(&LLPanelLandAudio::onCommitAny, this));

	return TRUE;
}


// public
void LLPanelLandAudio::refresh()
{
	LLParcel *parcel = mParcel->getParcel();

	if (!parcel)
	{
		clearCtrls();
	}
	else
	{
		// something selected, hooray!

		// Display options
		BOOL can_change_media = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_CHANGE_MEDIA);

		mCheckSoundLocal->set( parcel->getSoundLocal() );
		mCheckSoundLocal->setEnabled( can_change_media );

		bool allow_voice = parcel->getParcelFlagAllowVoice();

		LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
		if (region && region->isVoiceEnabled())
		{
			mCheckEstateDisabledVoice->setVisible(false);

			mCheckParcelEnableVoice->setVisible(true);
			mCheckParcelEnableVoice->setEnabled( can_change_media );
			mCheckParcelEnableVoice->set(allow_voice);

			mCheckParcelVoiceLocal->setEnabled( can_change_media && allow_voice );
		}
		else
		{
			// Voice disabled at estate level, overrides parcel settings
			// Replace the parcel voice checkbox with a disabled one
			// labelled with an explanatory message
			mCheckEstateDisabledVoice->setVisible(true);

			mCheckParcelEnableVoice->setVisible(false);
			mCheckParcelEnableVoice->setEnabled(false);
			mCheckParcelVoiceLocal->setEnabled(false);
		}

		mCheckParcelEnableVoice->set(allow_voice);
		mCheckParcelVoiceLocal->set(!parcel->getParcelFlagUseEstateVoiceChannel());

		mMusicURLEdit->setText(parcel->getMusicURL());
		mMusicURLEdit->setEnabled( can_change_media );

		BOOL can_change_av_sounds = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_OPTIONS) && parcel->getHaveNewParcelLimitData();
		mCheckAVSoundAny->set(parcel->getAllowAnyAVSounds());
		mCheckAVSoundAny->setEnabled(can_change_av_sounds);

		mCheckAVSoundGroup->set(parcel->getAllowGroupAVSounds() || parcel->getAllowAnyAVSounds());	// On if "Everyone" is on
		mCheckAVSoundGroup->setEnabled(can_change_av_sounds && !parcel->getAllowAnyAVSounds());		// Enabled if "Everyone" is off
	}
}
// static
void LLPanelLandAudio::onCommitAny()
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	BOOL sound_local		= mCheckSoundLocal->get();
	std::string music_url	= mMusicURLEdit->getText();

	BOOL voice_enabled = mCheckParcelEnableVoice->get();
	BOOL voice_estate_chan = !mCheckParcelVoiceLocal->get();

	BOOL any_av_sound		= mCheckAVSoundAny->get();
	BOOL group_av_sound		= TRUE;		// If set to "Everyone" then group is checked as well
	if (!any_av_sound)
	{	// If "Everyone" is off, use the value from the checkbox
		group_av_sound = mCheckAVSoundGroup->get();
	}

	// Remove leading/trailing whitespace (common when copying/pasting)
	LLStringUtil::trim(music_url);

	// Push data into current parcel
	parcel->setParcelFlag(PF_ALLOW_VOICE_CHAT, voice_enabled);
	parcel->setParcelFlag(PF_USE_ESTATE_VOICE_CHAN, voice_estate_chan);
	parcel->setParcelFlag(PF_SOUND_LOCAL, sound_local);
	parcel->setMusicURL(music_url);
	parcel->setAllowAnyAVSounds(any_av_sound);
	parcel->setAllowGroupAVSounds(group_av_sound);

	// Send current parcel data upstream to server
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

	// Might have changed properties, so let's redraw!
	refresh();
}
