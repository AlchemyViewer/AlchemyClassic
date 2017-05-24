/** 
 * @file llviewershadermgr.h
 * @brief Viewer Shader Manager
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

#ifndef LL_VIEWER_SHADER_MGR_H
#define LL_VIEWER_SHADER_MGR_H

#include "llshadermgr.h"
#include "llmaterial.h"

#define LL_DEFERRED_MULTI_LIGHT_COUNT 16

class LLViewerShaderMgr: public LLShaderMgr
{
public:
	static BOOL sInitialized;
	static bool sSkipReload;

	LLViewerShaderMgr();
	/* virtual */ ~LLViewerShaderMgr();

	// singleton pattern implementation
	static LLViewerShaderMgr * instance();
	static void releaseInstance();

	void initAttribsAndUniforms(void) override;
	void setShaders();
	void unloadShaders();
	S32 getVertexShaderLevel(S32 type);
	BOOL loadBasicShaders();
	BOOL loadShadersEffects();
	BOOL loadShadersDeferred();
	BOOL loadShadersObject();
	BOOL loadShadersAvatar();
	BOOL loadShadersEnvironment();
	BOOL loadShadersWater();
	BOOL loadShadersInterface();
	BOOL loadShadersWindLight();

	std::vector<S32> mVertexShaderLevel;
	S32	mMaxAvatarShaderLevel;

	enum EShaderClass
	{
		SHADER_LIGHTING,
		SHADER_OBJECT,
		SHADER_AVATAR,
		SHADER_ENVIRONMENT,
		SHADER_INTERFACE,
		SHADER_EFFECT,
		SHADER_WINDLIGHT,
		SHADER_WATER,
		SHADER_DEFERRED,
		SHADER_COUNT
	};

	// simple model of forward iterator
	// http://www.sgi.com/tech/stl/ForwardIterator.html
	class shader_iter
	{
	private:
		friend bool operator == (shader_iter const & a, shader_iter const & b);
		friend bool operator != (shader_iter const & a, shader_iter const & b);

		typedef std::vector<LLGLSLShader *>::const_iterator base_iter_t;
	public:
		shader_iter()
		{
		}

		shader_iter(base_iter_t iter) : mIter(iter)
		{
		}

		LLGLSLShader & operator * () const
		{
			return **mIter;
		}

		LLGLSLShader * operator -> () const
		{
			return *mIter;
		}

		shader_iter & operator++ ()
		{
			++mIter;
			return *this;
		}

		shader_iter operator++ (int)
		{
			return mIter++;
		}

	private:
		base_iter_t mIter;
	};

	shader_iter beginShaders() const;
	shader_iter endShaders() const;

	/* virtual */ std::string getShaderDirPrefix(void) override;

	/* virtual */ void updateShaderUniforms(LLGLSLShader * shader) override;

private:
	
	std::vector<std::string> mShinyUniforms;

	//water parameters
	std::vector<std::string> mWaterUniforms;

	std::vector<std::string> mWLUniforms;

	//terrain parameters
	std::vector<std::string> mTerrainUniforms;

	//glow parameters
	std::vector<std::string> mGlowUniforms;

	std::vector<std::string> mGlowExtractUniforms;

	std::vector<std::string> mAvatarUniforms;

	// the list of shaders we need to propagate parameters to.
	std::vector<LLGLSLShader *> mShaderList;

}; //LLViewerShaderMgr

inline bool operator == (LLViewerShaderMgr::shader_iter const & a, LLViewerShaderMgr::shader_iter const & b)
{
	return a.mIter == b.mIter;
}

inline bool operator != (LLViewerShaderMgr::shader_iter const & a, LLViewerShaderMgr::shader_iter const & b)
{
	return a.mIter != b.mIter;
}

extern LLVector4			gShinyOrigin;

//utility shaders
extern LLGLSLShader			gOcclusionProgram;
extern LLGLSLShader			gOcclusionCubeProgram;
extern LLGLSLShader			gCustomAlphaProgram;
extern LLGLSLShader			gGlowCombineProgram;
extern LLGLSLShader			gSplatTextureRectProgram;
extern LLGLSLShader			gGlowCombineFXAAProgram;
extern LLGLSLShader			gDebugProgram;
extern LLGLSLShader			gClipProgram;
extern LLGLSLShader			gDownsampleDepthProgram;
extern LLGLSLShader			gBenchmarkProgram;

//output tex0[tc0] + tex1[tc1]
extern LLGLSLShader			gTwoTextureAddProgram;
//output tex0[tc0] - tex1[tc1]
extern LLGLSLShader			gTwoTextureCompareProgram;
//discard some fragments based on user-set color tolerance
extern LLGLSLShader			gOneTextureFilterProgram;
						
extern LLGLSLShader			gOneTextureNoColorProgram;

//object shaders
extern LLGLSLShader			gObjectSimpleProgram;
extern LLGLSLShader			gObjectSimpleImpostorProgram;
extern LLGLSLShader			gObjectPreviewProgram;
extern LLGLSLShader			gObjectSimpleAlphaMaskProgram;
extern LLGLSLShader			gObjectSimpleWaterProgram;
extern LLGLSLShader			gObjectSimpleWaterAlphaMaskProgram;
extern LLGLSLShader			gObjectSimpleNonIndexedProgram;
extern LLGLSLShader			gObjectSimpleNonIndexedTexGenProgram;
extern LLGLSLShader			gObjectSimpleNonIndexedTexGenWaterProgram;
extern LLGLSLShader			gObjectSimpleNonIndexedWaterProgram;
extern LLGLSLShader			gObjectAlphaMaskNonIndexedProgram;
extern LLGLSLShader			gObjectAlphaMaskNonIndexedWaterProgram;
extern LLGLSLShader			gObjectAlphaMaskNoColorProgram;
extern LLGLSLShader			gObjectAlphaMaskNoColorWaterProgram;
extern LLGLSLShader			gObjectFullbrightProgram;
extern LLGLSLShader			gObjectFullbrightWaterProgram;
extern LLGLSLShader			gObjectFullbrightNoColorProgram;
extern LLGLSLShader			gObjectFullbrightNoColorWaterProgram;
extern LLGLSLShader			gObjectEmissiveProgram;
extern LLGLSLShader			gObjectEmissiveWaterProgram;
extern LLGLSLShader			gObjectFullbrightAlphaMaskProgram;
extern LLGLSLShader			gObjectFullbrightWaterAlphaMaskProgram;
extern LLGLSLShader			gObjectFullbrightNonIndexedProgram;
extern LLGLSLShader			gObjectFullbrightNonIndexedWaterProgram;
extern LLGLSLShader			gObjectEmissiveNonIndexedProgram;
extern LLGLSLShader			gObjectEmissiveNonIndexedWaterProgram;
extern LLGLSLShader			gObjectBumpProgram;
extern LLGLSLShader			gTreeProgram;
extern LLGLSLShader			gTreeWaterProgram;

extern LLGLSLShader			gObjectSimpleLODProgram;
extern LLGLSLShader			gObjectFullbrightLODProgram;

extern LLGLSLShader			gObjectFullbrightShinyProgram;
extern LLGLSLShader			gObjectFullbrightShinyWaterProgram;
extern LLGLSLShader			gObjectFullbrightShinyNonIndexedProgram;
extern LLGLSLShader			gObjectFullbrightShinyNonIndexedWaterProgram;

extern LLGLSLShader			gObjectShinyProgram;
extern LLGLSLShader			gObjectShinyWaterProgram;
extern LLGLSLShader			gObjectShinyNonIndexedProgram;
extern LLGLSLShader			gObjectShinyNonIndexedWaterProgram;

extern LLGLSLShader			gSkinnedObjectSimpleProgram;
extern LLGLSLShader			gSkinnedObjectFullbrightProgram;
extern LLGLSLShader			gSkinnedObjectEmissiveProgram;
extern LLGLSLShader			gSkinnedObjectFullbrightShinyProgram;
extern LLGLSLShader			gSkinnedObjectShinySimpleProgram;

extern LLGLSLShader			gSkinnedObjectSimpleWaterProgram;
extern LLGLSLShader			gSkinnedObjectFullbrightWaterProgram;
extern LLGLSLShader			gSkinnedObjectEmissiveWaterProgram;
extern LLGLSLShader			gSkinnedObjectFullbrightShinyWaterProgram;
extern LLGLSLShader			gSkinnedObjectShinySimpleWaterProgram;

//environment shaders
extern LLGLSLShader			gTerrainProgram;
extern LLGLSLShader			gTerrainWaterProgram;
extern LLGLSLShader			gWaterProgram;
extern LLGLSLShader			gUnderWaterProgram;
extern LLGLSLShader			gGlowProgram;
extern LLGLSLShader			gGlowExtractProgram;

//interface shaders
extern LLGLSLShader			gHighlightProgram;
extern LLGLSLShader			gHighlightNormalProgram;
extern LLGLSLShader			gHighlightSpecularProgram;

// avatar shader handles
extern LLGLSLShader			gAvatarProgram;
extern LLGLSLShader			gAvatarWaterProgram;
extern LLGLSLShader			gAvatarPickProgram;
extern LLGLSLShader			gImpostorProgram;

// WindLight shader handles
extern LLGLSLShader			gWLSkyProgram;
extern LLGLSLShader			gWLCloudProgram;

// Deferred rendering shaders
extern LLGLSLShader			gDeferredImpostorProgram;
extern LLGLSLShader			gDeferredWaterProgram;
extern LLGLSLShader			gDeferredUnderWaterProgram;
extern LLGLSLShader			gDeferredDiffuseProgram;
extern LLGLSLShader			gDeferredDiffuseAlphaMaskProgram;
extern LLGLSLShader			gDeferredNonIndexedDiffuseAlphaMaskProgram;
extern LLGLSLShader			gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
extern LLGLSLShader			gDeferredNonIndexedDiffuseProgram;
extern LLGLSLShader			gDeferredSkinnedDiffuseProgram;
extern LLGLSLShader			gDeferredSkinnedBumpProgram;
extern LLGLSLShader			gDeferredSkinnedAlphaProgram;
extern LLGLSLShader			gDeferredBumpProgram;
extern LLGLSLShader			gDeferredTerrainProgram;
extern LLGLSLShader			gDeferredTreeProgram;
extern LLGLSLShader			gDeferredTreeShadowProgram;
extern LLGLSLShader			gDeferredLightProgram;
extern LLGLSLShader			gDeferredMultiLightProgram[LL_DEFERRED_MULTI_LIGHT_COUNT];
extern LLGLSLShader			gDeferredSpotLightProgram;
extern LLGLSLShader			gDeferredMultiSpotLightProgram;
extern LLGLSLShader			gDeferredSSAOProgram;
extern LLGLSLShader			gDeferredDownsampleDepthNearestProgram;
extern LLGLSLShader			gDeferredSunProgram;
extern LLGLSLShader			gDeferredBlurLightProgram;
extern LLGLSLShader			gDeferredAvatarProgram;
extern LLGLSLShader			gDeferredSoftenProgram;
extern LLGLSLShader			gDeferredSoftenWaterProgram;
extern LLGLSLShader			gDeferredShadowProgram;
extern LLGLSLShader			gDeferredShadowCubeProgram;
extern LLGLSLShader			gDeferredShadowAlphaMaskProgram;
extern LLGLSLShader			gDeferredPostProgram;
extern LLGLSLShader			gDeferredCoFProgram;
extern LLGLSLShader			gDeferredDoFCombineProgram;
extern LLGLSLShader			gFXAAProgram;
extern LLGLSLShader			gDeferredPostNoDoFProgram;
extern LLGLSLShader			gDeferredPostGammaCorrectProgram;
extern LLGLSLShader			gDeferredAvatarShadowProgram;
extern LLGLSLShader			gDeferredAttachmentShadowProgram;
extern LLGLSLShader			gDeferredAlphaProgram;
extern LLGLSLShader			gDeferredAlphaImpostorProgram;
extern LLGLSLShader			gDeferredFullbrightProgram;
extern LLGLSLShader			gDeferredFullbrightAlphaMaskProgram;
extern LLGLSLShader			gDeferredAlphaWaterProgram;
extern LLGLSLShader			gDeferredFullbrightWaterProgram;
extern LLGLSLShader			gDeferredFullbrightAlphaMaskWaterProgram;
extern LLGLSLShader			gDeferredEmissiveProgram;
extern LLGLSLShader			gDeferredAvatarEyesProgram;
extern LLGLSLShader			gDeferredAvatarAlphaProgram;
extern LLGLSLShader			gDeferredWLSkyProgram;
extern LLGLSLShader			gDeferredWLCloudProgram;
extern LLGLSLShader			gDeferredStarProgram;
extern LLGLSLShader			gDeferredFullbrightShinyProgram;
extern LLGLSLShader			gDeferredSkinnedFullbrightShinyProgram;
extern LLGLSLShader			gDeferredSkinnedFullbrightProgram;
extern LLGLSLShader			gNormalMapGenProgram;

// Deferred materials shaders
extern LLGLSLShader			gDeferredMaterialProgram[LLMaterial::SHADER_COUNT*2];
extern LLGLSLShader			gDeferredMaterialWaterProgram[LLMaterial::SHADER_COUNT*2];
#endif
