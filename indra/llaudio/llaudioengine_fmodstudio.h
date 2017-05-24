/** 
 * @file audioengine_fmodstudio.h
 * @brief Definition of LLAudioEngine class abstracting the audio 
 * support as a FMOD Studio implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2014, Linden Research, Inc.
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

#ifndef LL_AUDIOENGINE_FMODSTUDIO_H
#define LL_AUDIOENGINE_FMODSTUDIO_H

#include "llaudioengine.h"
#include "llwindgen.h"

//Stubs
class LLAudioStreamManagerFMODSTUDIO;
namespace FMOD
{
	class System;
	class Channel;
	class ChannelGroup;
	class Sound;
	class DSP;
}
typedef struct FMOD_DSP_DESCRIPTION FMOD_DSP_DESCRIPTION;

//Interfaces
class LLAudioEngine_FMODSTUDIO : public LLAudioEngine 
{
public:
	enum
	{
		RESAMPLE_LINEAR=0,
		RESAMPLE_CUBIC,
		RESAMPLE_SPLINE
	};
	LLAudioEngine_FMODSTUDIO(bool enable_profiler, U32 resample_method);
	virtual ~LLAudioEngine_FMODSTUDIO();

	// initialization/startup/shutdown
	bool init(const S32 num_channels, void *user_data) override;
	std::string getDriverName(bool verbose) override;
	void allocateListener() override;

	void shutdown() override;

	/*virtual*/ bool initWind() override;
	/*virtual*/ void cleanupWind() override;

	/*virtual*/void updateWind(LLVector3 direction, F32 camera_height_above_water) override;

	typedef F32 MIXBUFFERFORMAT;

	FMOD::System *getSystem()				const {return mSystem;}
protected:
	/*virtual*/ LLAudioBuffer *createBuffer() override; // Get a free buffer, or flush an existing one if you have to.
	/*virtual*/ LLAudioChannel *createChannel() override; // Create a new audio channel.

	/*virtual*/ void setInternalGain(F32 gain) override;

	bool mInited;

	LLWindGen<MIXBUFFERFORMAT> *mWindGen;

	FMOD_DSP_DESCRIPTION *mWindDSPDesc;
	FMOD::DSP *mWindDSP;
	FMOD::System *mSystem;
	bool mEnableProfiler;
	U32 mResampleMethod;

public:
	static FMOD::ChannelGroup *mChannelGroups[LLAudioEngine::AUDIO_TYPE_COUNT];
};


class LLAudioChannelFMODSTUDIO : public LLAudioChannel
{
public:
	LLAudioChannelFMODSTUDIO(FMOD::System *audioengine);
	virtual ~LLAudioChannelFMODSTUDIO();

protected:
	/*virtual*/ void play() override;
	/*virtual*/ void playSynced(LLAudioChannel *channelp) override;
	/*virtual*/ void cleanup() override;
	/*virtual*/ bool isPlaying() override;

	/*virtual*/ bool updateBuffer() override;
	/*virtual*/ void update3DPosition() override;
	/*virtual*/ void updateLoop() override;

	void set3DMode(bool use3d);
protected:
	FMOD::System *getSystem()	const {return mSystemp;}
	FMOD::System *mSystemp;
	FMOD::Channel *mChannelp;
	S32 mLastSamplePos;
};


class LLAudioBufferFMODSTUDIO : public LLAudioBuffer
{
public:
	LLAudioBufferFMODSTUDIO(FMOD::System *audioengine);
	virtual ~LLAudioBufferFMODSTUDIO();

	/*virtual*/ bool loadWAV(const std::string& filename) override;
	/*virtual*/ U32 getLength() override;
	friend class LLAudioChannelFMODSTUDIO;
protected:
	FMOD::System *getSystem()	const {return mSystemp;}
	FMOD::System *mSystemp;
	FMOD::Sound *getSound()		const{ return mSoundp; }
	FMOD::Sound *mSoundp;
};


#endif // LL_AUDIOENGINE_FMODSTUDIO_H
