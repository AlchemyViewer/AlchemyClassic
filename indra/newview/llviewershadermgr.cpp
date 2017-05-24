// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llviewershadermgr.cpp
 * @brief Viewer shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
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

#include <boost/lexical_cast.hpp>

#include "llfeaturemanager.h"
#include "llviewershadermgr.h"

#include "llfile.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llviewercontrol.h"
#include "pipeline.h"
#include "llworld.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llsky.h"
#include "llvosky.h"
#include "llrender.h"
#include "lljoint.h"
#include "llskinningutil.h"

#ifdef LL_RELEASE_FOR_DOWNLOAD
#define UNIFORM_ERRS LL_WARNS_ONCE("Shader")
#else
#define UNIFORM_ERRS LL_ERRS("Shader")
#endif

static LLStaticHashedString sTexture0("texture0");
static LLStaticHashedString sTexture1("texture1");
static LLStaticHashedString sTex0("tex0");
static LLStaticHashedString sTex1("tex1");
static LLStaticHashedString sDitherTex("dither_tex");
static LLStaticHashedString sGlowMap("glowMap");
static LLStaticHashedString sScreenMap("screenMap");

BOOL				LLViewerShaderMgr::sInitialized = FALSE;
bool				LLViewerShaderMgr::sSkipReload = false;

LLVector4			gShinyOrigin;

//utility shaders
LLGLSLShader	gOcclusionProgram;
LLGLSLShader	gOcclusionCubeProgram;
LLGLSLShader	gCustomAlphaProgram;
LLGLSLShader	gGlowCombineProgram;
LLGLSLShader	gSplatTextureRectProgram;
LLGLSLShader	gGlowCombineFXAAProgram;
LLGLSLShader	gTwoTextureAddProgram;
LLGLSLShader	gTwoTextureCompareProgram;
LLGLSLShader	gOneTextureFilterProgram;
LLGLSLShader	gOneTextureNoColorProgram;
LLGLSLShader	gDebugProgram;
LLGLSLShader	gClipProgram;
LLGLSLShader	gDownsampleDepthProgram;
LLGLSLShader	gAlphaMaskProgram;
LLGLSLShader	gBenchmarkProgram;


//object shaders
LLGLSLShader		gObjectSimpleProgram;
LLGLSLShader		gObjectSimpleImpostorProgram;
LLGLSLShader		gObjectPreviewProgram;
LLGLSLShader		gObjectSimpleWaterProgram;
LLGLSLShader		gObjectSimpleAlphaMaskProgram;
LLGLSLShader		gObjectSimpleWaterAlphaMaskProgram;
LLGLSLShader		gObjectFullbrightProgram;
LLGLSLShader		gObjectFullbrightWaterProgram;
LLGLSLShader		gObjectEmissiveProgram;
LLGLSLShader		gObjectEmissiveWaterProgram;
LLGLSLShader		gObjectFullbrightAlphaMaskProgram;
LLGLSLShader		gObjectFullbrightWaterAlphaMaskProgram;
LLGLSLShader		gObjectFullbrightShinyProgram;
LLGLSLShader		gObjectFullbrightShinyWaterProgram;
LLGLSLShader		gObjectShinyProgram;
LLGLSLShader		gObjectShinyWaterProgram;
LLGLSLShader		gObjectBumpProgram;
LLGLSLShader		gTreeProgram;
LLGLSLShader		gTreeWaterProgram;
LLGLSLShader		gObjectFullbrightNoColorProgram;
LLGLSLShader		gObjectFullbrightNoColorWaterProgram;

LLGLSLShader		gObjectSimpleNonIndexedProgram;
LLGLSLShader		gObjectSimpleNonIndexedTexGenProgram;
LLGLSLShader		gObjectSimpleNonIndexedTexGenWaterProgram;
LLGLSLShader		gObjectSimpleNonIndexedWaterProgram;
LLGLSLShader		gObjectAlphaMaskNonIndexedProgram;
LLGLSLShader		gObjectAlphaMaskNonIndexedWaterProgram;
LLGLSLShader		gObjectAlphaMaskNoColorProgram;
LLGLSLShader		gObjectAlphaMaskNoColorWaterProgram;
LLGLSLShader		gObjectFullbrightNonIndexedProgram;
LLGLSLShader		gObjectFullbrightNonIndexedWaterProgram;
LLGLSLShader		gObjectEmissiveNonIndexedProgram;
LLGLSLShader		gObjectEmissiveNonIndexedWaterProgram;
LLGLSLShader		gObjectFullbrightShinyNonIndexedProgram;
LLGLSLShader		gObjectFullbrightShinyNonIndexedWaterProgram;
LLGLSLShader		gObjectShinyNonIndexedProgram;
LLGLSLShader		gObjectShinyNonIndexedWaterProgram;

//object hardware skinning shaders
LLGLSLShader		gSkinnedObjectSimpleProgram;
LLGLSLShader		gSkinnedObjectFullbrightProgram;
LLGLSLShader		gSkinnedObjectEmissiveProgram;
LLGLSLShader		gSkinnedObjectFullbrightShinyProgram;
LLGLSLShader		gSkinnedObjectShinySimpleProgram;

LLGLSLShader		gSkinnedObjectSimpleWaterProgram;
LLGLSLShader		gSkinnedObjectFullbrightWaterProgram;
LLGLSLShader		gSkinnedObjectEmissiveWaterProgram;
LLGLSLShader		gSkinnedObjectFullbrightShinyWaterProgram;
LLGLSLShader		gSkinnedObjectShinySimpleWaterProgram;

//environment shaders
LLGLSLShader		gTerrainProgram;
LLGLSLShader		gTerrainWaterProgram;
LLGLSLShader		gWaterProgram;
LLGLSLShader		gUnderWaterProgram;

//interface shaders
LLGLSLShader		gHighlightProgram;
LLGLSLShader		gHighlightNormalProgram;
LLGLSLShader		gHighlightSpecularProgram;

//avatar shader handles
LLGLSLShader		gAvatarProgram;
LLGLSLShader		gAvatarWaterProgram;
LLGLSLShader		gAvatarPickProgram;
LLGLSLShader		gImpostorProgram;

// WindLight shader handles
LLGLSLShader			gWLSkyProgram;
LLGLSLShader			gWLCloudProgram;


// Effects Shaders
LLGLSLShader			gGlowProgram;
LLGLSLShader			gGlowExtractProgram;

// Deferred rendering shaders
LLGLSLShader			gDeferredImpostorProgram;
LLGLSLShader			gDeferredWaterProgram;
LLGLSLShader			gDeferredUnderWaterProgram;
LLGLSLShader			gDeferredDiffuseProgram;
LLGLSLShader			gDeferredDiffuseAlphaMaskProgram;
LLGLSLShader			gDeferredNonIndexedDiffuseProgram;
LLGLSLShader			gDeferredNonIndexedDiffuseAlphaMaskProgram;
LLGLSLShader			gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
LLGLSLShader			gDeferredSkinnedDiffuseProgram;
LLGLSLShader			gDeferredSkinnedBumpProgram;
LLGLSLShader			gDeferredSkinnedAlphaProgram;
LLGLSLShader			gDeferredBumpProgram;
LLGLSLShader			gDeferredTerrainProgram;
LLGLSLShader			gDeferredTreeProgram;
LLGLSLShader			gDeferredTreeShadowProgram;
LLGLSLShader			gDeferredAvatarProgram;
LLGLSLShader			gDeferredAvatarAlphaProgram;
LLGLSLShader			gDeferredLightProgram;
LLGLSLShader			gDeferredMultiLightProgram[16];
LLGLSLShader			gDeferredSpotLightProgram;
LLGLSLShader			gDeferredMultiSpotLightProgram;
LLGLSLShader			gDeferredSSAOProgram;
LLGLSLShader			gDeferredDownsampleDepthNearestProgram;
LLGLSLShader			gDeferredSunProgram;
LLGLSLShader			gDeferredBlurLightProgram;
LLGLSLShader			gDeferredSoftenProgram;
LLGLSLShader			gDeferredSoftenWaterProgram;
LLGLSLShader			gDeferredShadowProgram;
LLGLSLShader			gDeferredShadowCubeProgram;
LLGLSLShader			gDeferredShadowAlphaMaskProgram;
LLGLSLShader			gDeferredAvatarShadowProgram;
LLGLSLShader			gDeferredAttachmentShadowProgram;
LLGLSLShader			gDeferredAlphaProgram;
LLGLSLShader			gDeferredAlphaImpostorProgram;
LLGLSLShader			gDeferredAlphaWaterProgram;
LLGLSLShader			gDeferredAvatarEyesProgram;
LLGLSLShader			gDeferredFullbrightProgram;
LLGLSLShader			gDeferredFullbrightAlphaMaskProgram;
LLGLSLShader			gDeferredFullbrightWaterProgram;
LLGLSLShader			gDeferredFullbrightAlphaMaskWaterProgram;
LLGLSLShader			gDeferredEmissiveProgram;
LLGLSLShader			gDeferredPostProgram;
LLGLSLShader			gDeferredCoFProgram;
LLGLSLShader			gDeferredDoFCombineProgram;
LLGLSLShader			gDeferredPostGammaCorrectProgram;
LLGLSLShader			gFXAAProgram;
LLGLSLShader			gDeferredPostNoDoFProgram;
LLGLSLShader			gDeferredWLSkyProgram;
LLGLSLShader			gDeferredWLCloudProgram;
LLGLSLShader			gDeferredStarProgram;
LLGLSLShader			gDeferredFullbrightShinyProgram;
LLGLSLShader			gDeferredSkinnedFullbrightShinyProgram;
LLGLSLShader			gDeferredSkinnedFullbrightProgram;
LLGLSLShader			gNormalMapGenProgram;

// Deferred materials shaders
LLGLSLShader			gDeferredMaterialProgram[LLMaterial::SHADER_COUNT*2];
LLGLSLShader			gDeferredMaterialWaterProgram[LLMaterial::SHADER_COUNT*2];

LLViewerShaderMgr::LLViewerShaderMgr() :
	mVertexShaderLevel(SHADER_COUNT, 0),
	mMaxAvatarShaderLevel(0)
{	
	/// Make sure WL Sky is the first program
	//ONLY shaders that need WL Param management should be added here
	mShaderList.push_back(&gWLSkyProgram);
	mShaderList.push_back(&gWLCloudProgram);
	mShaderList.push_back(&gAvatarProgram);
	mShaderList.push_back(&gObjectShinyProgram);
	mShaderList.push_back(&gObjectShinyNonIndexedProgram);
	mShaderList.push_back(&gWaterProgram);
	mShaderList.push_back(&gObjectSimpleProgram);
	mShaderList.push_back(&gObjectSimpleImpostorProgram);
	mShaderList.push_back(&gObjectPreviewProgram);
	mShaderList.push_back(&gImpostorProgram);
	mShaderList.push_back(&gObjectFullbrightNoColorProgram);
	mShaderList.push_back(&gObjectFullbrightNoColorWaterProgram);
	mShaderList.push_back(&gObjectSimpleAlphaMaskProgram);
	mShaderList.push_back(&gObjectBumpProgram);
	mShaderList.push_back(&gObjectEmissiveProgram);
	mShaderList.push_back(&gObjectEmissiveWaterProgram);
	mShaderList.push_back(&gObjectFullbrightProgram);
	mShaderList.push_back(&gObjectFullbrightAlphaMaskProgram);
	mShaderList.push_back(&gObjectFullbrightShinyProgram);
	mShaderList.push_back(&gObjectFullbrightShinyWaterProgram);
	mShaderList.push_back(&gObjectSimpleNonIndexedProgram);
	mShaderList.push_back(&gObjectSimpleNonIndexedTexGenProgram);
	mShaderList.push_back(&gObjectSimpleNonIndexedTexGenWaterProgram);
	mShaderList.push_back(&gObjectSimpleNonIndexedWaterProgram);
	mShaderList.push_back(&gObjectAlphaMaskNonIndexedProgram);
	mShaderList.push_back(&gObjectAlphaMaskNonIndexedWaterProgram);
	mShaderList.push_back(&gObjectAlphaMaskNoColorProgram);
	mShaderList.push_back(&gObjectAlphaMaskNoColorWaterProgram);
	mShaderList.push_back(&gTreeProgram);
	mShaderList.push_back(&gTreeWaterProgram);
	mShaderList.push_back(&gObjectFullbrightNonIndexedProgram);
	mShaderList.push_back(&gObjectFullbrightNonIndexedWaterProgram);
	mShaderList.push_back(&gObjectEmissiveNonIndexedProgram);
	mShaderList.push_back(&gObjectEmissiveNonIndexedWaterProgram);
	mShaderList.push_back(&gObjectFullbrightShinyNonIndexedProgram);
	mShaderList.push_back(&gObjectFullbrightShinyNonIndexedWaterProgram);
	mShaderList.push_back(&gSkinnedObjectSimpleProgram);
	mShaderList.push_back(&gSkinnedObjectFullbrightProgram);
	mShaderList.push_back(&gSkinnedObjectEmissiveProgram);
	mShaderList.push_back(&gSkinnedObjectFullbrightShinyProgram);
	mShaderList.push_back(&gSkinnedObjectShinySimpleProgram);
	mShaderList.push_back(&gSkinnedObjectSimpleWaterProgram);
	mShaderList.push_back(&gSkinnedObjectFullbrightWaterProgram);
	mShaderList.push_back(&gSkinnedObjectEmissiveWaterProgram);
	mShaderList.push_back(&gSkinnedObjectFullbrightShinyWaterProgram);
	mShaderList.push_back(&gSkinnedObjectShinySimpleWaterProgram);
	mShaderList.push_back(&gTerrainProgram);
	mShaderList.push_back(&gTerrainWaterProgram);
	mShaderList.push_back(&gObjectSimpleWaterProgram);
	mShaderList.push_back(&gObjectFullbrightWaterProgram);
	mShaderList.push_back(&gObjectSimpleWaterAlphaMaskProgram);
	mShaderList.push_back(&gObjectFullbrightWaterAlphaMaskProgram);
	mShaderList.push_back(&gAvatarWaterProgram);
	mShaderList.push_back(&gObjectShinyWaterProgram);
	mShaderList.push_back(&gObjectShinyNonIndexedWaterProgram);
	mShaderList.push_back(&gUnderWaterProgram);
	mShaderList.push_back(&gDeferredSunProgram);
	mShaderList.push_back(&gDeferredSoftenProgram);
	mShaderList.push_back(&gDeferredSoftenWaterProgram);
	mShaderList.push_back(&gDeferredMaterialProgram[1]);
	mShaderList.push_back(&gDeferredMaterialProgram[5]);
	mShaderList.push_back(&gDeferredMaterialProgram[9]);
	mShaderList.push_back(&gDeferredMaterialProgram[13]);
	mShaderList.push_back(&gDeferredMaterialProgram[1+LLMaterial::SHADER_COUNT]);
	mShaderList.push_back(&gDeferredMaterialProgram[5+LLMaterial::SHADER_COUNT]);
	mShaderList.push_back(&gDeferredMaterialProgram[9+LLMaterial::SHADER_COUNT]);
	mShaderList.push_back(&gDeferredMaterialProgram[13+LLMaterial::SHADER_COUNT]);	
	mShaderList.push_back(&gDeferredMaterialWaterProgram[1]);
	mShaderList.push_back(&gDeferredMaterialWaterProgram[5]);
	mShaderList.push_back(&gDeferredMaterialWaterProgram[9]);
	mShaderList.push_back(&gDeferredMaterialWaterProgram[13]);
	mShaderList.push_back(&gDeferredMaterialWaterProgram[1+LLMaterial::SHADER_COUNT]);
	mShaderList.push_back(&gDeferredMaterialWaterProgram[5+LLMaterial::SHADER_COUNT]);
	mShaderList.push_back(&gDeferredMaterialWaterProgram[9+LLMaterial::SHADER_COUNT]);
	mShaderList.push_back(&gDeferredMaterialWaterProgram[13+LLMaterial::SHADER_COUNT]);	
	mShaderList.push_back(&gDeferredAlphaProgram);
	mShaderList.push_back(&gDeferredAlphaImpostorProgram);
	mShaderList.push_back(&gDeferredAlphaWaterProgram);
	mShaderList.push_back(&gDeferredSkinnedAlphaProgram);
	mShaderList.push_back(&gDeferredFullbrightProgram);
	mShaderList.push_back(&gDeferredFullbrightAlphaMaskProgram);
	mShaderList.push_back(&gDeferredFullbrightWaterProgram);
	mShaderList.push_back(&gDeferredFullbrightAlphaMaskWaterProgram);	
	mShaderList.push_back(&gDeferredFullbrightShinyProgram);
	mShaderList.push_back(&gDeferredSkinnedFullbrightShinyProgram);
	mShaderList.push_back(&gDeferredSkinnedFullbrightProgram);
	mShaderList.push_back(&gDeferredEmissiveProgram);
	mShaderList.push_back(&gDeferredAvatarEyesProgram);
	mShaderList.push_back(&gDeferredWaterProgram);
	mShaderList.push_back(&gDeferredUnderWaterProgram);	
	mShaderList.push_back(&gDeferredAvatarAlphaProgram);
	mShaderList.push_back(&gDeferredWLSkyProgram);
	mShaderList.push_back(&gDeferredWLCloudProgram);
}

LLViewerShaderMgr::~LLViewerShaderMgr()
{
	mVertexShaderLevel.clear();
	mShaderList.clear();
}

// static
LLViewerShaderMgr * LLViewerShaderMgr::instance()
{
	if(nullptr == sInstance)
	{
		sInstance = new LLViewerShaderMgr();
	}

	return static_cast<LLViewerShaderMgr*>(sInstance);
}

// static
void LLViewerShaderMgr::releaseInstance()
{
	if (sInstance != nullptr)
	{
		delete sInstance;
		sInstance = nullptr;
	}
}

void LLViewerShaderMgr::initAttribsAndUniforms(void)
{
	if (mReservedAttribs.empty())
	{
		LLShaderMgr::initAttribsAndUniforms();
	}	
}
	

//============================================================================
// Set Levels

S32 LLViewerShaderMgr::getVertexShaderLevel(S32 type)
{
	return LLPipeline::sDisableShaders ? 0 : mVertexShaderLevel[type];
}

//============================================================================
// Shader Management

void LLViewerShaderMgr::setShaders()
{
	//setShaders might be called redundantly by gSavedSettings, so return on reentrance
	static bool reentrance = false;
	
	if (!gPipeline.mInitialized || !sInitialized || reentrance || sSkipReload)
	{
		return;
	}

	LLGLSLShader::sIndexedTextureChannels = llmax(llmin(gGLManager.mNumTextureImageUnits, (S32) gSavedSettings.getU32("RenderMaxTextureIndex")), 1);

	//NEVER use more than 16 texture channels (work around for prevalent driver bug)
	LLGLSLShader::sIndexedTextureChannels = llmin(LLGLSLShader::sIndexedTextureChannels, 16);

	if (gGLManager.mGLSLVersionMajor < 1 ||
		(gGLManager.mGLSLVersionMajor == 1 && gGLManager.mGLSLVersionMinor <= 20))
	{ //NEVER use indexed texture rendering when GLSL version is 1.20 or earlier
		LLGLSLShader::sIndexedTextureChannels = 1;
	}

	reentrance = true;

	if (LLRender::sGLCoreProfile)
	{  
		if (!gSavedSettings.getBOOL("VertexShaderEnable"))
		{ //vertex shaders MUST be enabled to use core profile
			gSavedSettings.setBOOL("VertexShaderEnable", TRUE);
		}
	}
	
	// Make sure the compiled shader map is cleared before we recompile shaders.
	LLShaderMgr::instance()->mProgramObjects.clear();
	LLShaderMgr::instance()->mShaderObjects.clear();
	
	initAttribsAndUniforms();
	gPipeline.releaseGLBuffers();

	if (gSavedSettings.getBOOL("VertexShaderEnable"))
	{
		LLPipeline::sWaterReflections = gGLManager.mHasCubeMap;
		LLPipeline::sRenderGlow = gSavedSettings.getBOOL("RenderGlow"); 
		LLPipeline::updateRenderDeferred();
	}
	else
	{
		LLPipeline::sRenderGlow = FALSE;
		LLPipeline::sWaterReflections = FALSE;
	}
	
	//hack to reset buffers that change behavior with shaders
	gPipeline.resetVertexBuffers();

	if (gViewerWindow)
	{
		gViewerWindow->setCursor(UI_CURSOR_WAIT);
	}

	// Lighting
	gPipeline.setLightingDetail(-1);

	// Shaders
	LL_INFOS("ShaderLoading") << "\n~~~~~~~~~~~~~~~~~~\n Loading Shaders:\n~~~~~~~~~~~~~~~~~~" << LL_ENDL;
	LL_INFOS("ShaderLoading") << llformat("Using GLSL %d.%d", gGLManager.mGLSLVersionMajor, gGLManager.mGLSLVersionMinor) << LL_ENDL;

	for (S32 i = 0; i < SHADER_COUNT; i++)
	{
		mVertexShaderLevel[i] = 0;
	}
	mMaxAvatarShaderLevel = 0;

	LLGLSLShader::sNoFixedFunction = false;
	LLVertexBuffer::unbind();
	if (LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable") 
		&& (gGLManager.mGLSLVersionMajor > 1 || gGLManager.mGLSLVersionMinor >= 10)
		&& gSavedSettings.getBOOL("VertexShaderEnable"))
	{
		//using shaders, disable fixed function
		LLGLSLShader::sNoFixedFunction = true;

		S32 light_class = 2;
		S32 env_class = 2;
		S32 obj_class = 2;
		S32 effect_class = 2;
		S32 wl_class = 2;
		S32 water_class = 2;
		S32 deferred_class = 0;
		
		if (LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") &&
		    gSavedSettings.getBOOL("RenderDeferred") &&
			gSavedSettings.getBOOL("RenderAvatarVP") &&
			gSavedSettings.getBOOL("WindLightUseAtmosShaders"))
		{
			if (gSavedSettings.getS32("RenderShadowDetail") > 0)
			{ //shadows
				deferred_class = 2;
			}
			else
			{ //no shadows
				deferred_class = 1;
			}

			//make sure hardware skinning is enabled
			//gSavedSettings.setBOOL("RenderAvatarVP", TRUE);
			
			//make sure atmospheric shaders are enabled
			//gSavedSettings.setBOOL("WindLightUseAtmosShaders", TRUE);
		}


		if (!(LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders")
			  && gSavedSettings.getBOOL("WindLightUseAtmosShaders")))
		{
			// user has disabled WindLight in their settings, downgrade
			// windlight shaders to stub versions.
			wl_class = 1;
		}

		
		// Trigger a full rebuild of the fallback skybox / cubemap if we've toggled windlight shaders
		if (mVertexShaderLevel[SHADER_WINDLIGHT] != wl_class && gSky.mVOSkyp.notNull())
		{
			gSky.mVOSkyp->forceSkyUpdate();
		}

		
		// Load lighting shaders
		mVertexShaderLevel[SHADER_LIGHTING] = light_class;
		mVertexShaderLevel[SHADER_INTERFACE] = light_class;
		mVertexShaderLevel[SHADER_ENVIRONMENT] = env_class;
		mVertexShaderLevel[SHADER_WATER] = water_class;
		mVertexShaderLevel[SHADER_OBJECT] = obj_class;
		mVertexShaderLevel[SHADER_EFFECT] = effect_class;
		mVertexShaderLevel[SHADER_WINDLIGHT] = wl_class;
		mVertexShaderLevel[SHADER_DEFERRED] = deferred_class;

		BOOL loaded = loadBasicShaders();

		if (loaded)
		{
			gPipeline.mVertexShadersEnabled = TRUE;
			gPipeline.mVertexShadersLoaded = 1;

			// Load all shaders to set max levels
			loaded = loadShadersEnvironment();

			if (loaded)
			{
				loaded = loadShadersWater();
			}

			if (loaded)
			{
				loaded = loadShadersWindLight();
			}

			if (loaded)
			{
				loaded = loadShadersEffects();
			}

			if (loaded)
			{
				loaded = loadShadersInterface();
			}

			if (loaded)
			{
				// Load max avatar shaders to set the max level
				mVertexShaderLevel[SHADER_AVATAR] = 3;
				mMaxAvatarShaderLevel = 3;
				
				if (LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP") && 
					gSavedSettings.getBOOL("RenderAvatarVP") && loadShadersObject())
				{ //hardware skinning is enabled and rigged attachment shaders loaded correctly
					BOOL avatar_cloth = gSavedSettings.getBOOL("RenderAvatarCloth");
					S32 avatar_class = 1;
				
					// cloth is a class3 shader
					if(avatar_cloth)
					{
						avatar_class = 3;
					}

					// Set the actual level
					mVertexShaderLevel[SHADER_AVATAR] = avatar_class;
					loadShadersAvatar();
					if (mVertexShaderLevel[SHADER_AVATAR] != avatar_class)
					{
						if (mVertexShaderLevel[SHADER_AVATAR] == 0)
						{
							gSavedSettings.setBOOL("RenderAvatarVP", FALSE);
						}
						if(llmax(mVertexShaderLevel[SHADER_AVATAR]-1,0) >= 3)
						{
							avatar_cloth = true;
						}
						else
						{
							avatar_cloth = false;
						}
						gSavedSettings.setBOOL("RenderAvatarCloth", avatar_cloth);
					}
				}
				else
				{ //hardware skinning not possible, neither is deferred rendering
					mVertexShaderLevel[SHADER_AVATAR] = 0;
					mVertexShaderLevel[SHADER_DEFERRED] = 0;

					if (gSavedSettings.getBOOL("RenderAvatarVP"))
					{
						gSavedSettings.setBOOL("RenderDeferred", FALSE);
						gSavedSettings.setBOOL("RenderAvatarCloth", FALSE);
						gSavedSettings.setBOOL("RenderAvatarVP", FALSE);
					}

					loadShadersAvatar(); // unloads

					loaded = loadShadersObject();
				}
			}

			if (!loaded)
			{ //some shader absolutely could not load, try to fall back to a simpler setting
				if (gSavedSettings.getBOOL("WindLightUseAtmosShaders"))
				{ //disable windlight and try again
					gSavedSettings.setBOOL("WindLightUseAtmosShaders", FALSE);
					LLShaderMgr::instance()->cleanupShaderSources();
					unloadShaders();
					reentrance = false;
					setShaders();
					return;
				}

				if (gSavedSettings.getBOOL("VertexShaderEnable"))
				{ //disable shaders outright and try again
					gSavedSettings.setBOOL("VertexShaderEnable", FALSE);
					LLShaderMgr::instance()->cleanupShaderSources();
					unloadShaders();
					reentrance = false;
					setShaders();
					return;
				}
			}		

			if (loaded && !loadShadersDeferred())
			{ //everything else succeeded but deferred failed, disable deferred and try again
				gSavedSettings.setBOOL("RenderDeferred", FALSE);
				LLShaderMgr::instance()->cleanupShaderSources();
				unloadShaders();
				reentrance = false;
				setShaders();
				return;
			}
			LLShaderMgr::instance()->cleanupShaderSources();
		}
		else
		{
			LLGLSLShader::sNoFixedFunction = false;
			gPipeline.mVertexShadersEnabled = FALSE;
			gPipeline.mVertexShadersLoaded = 0;
			mVertexShaderLevel[SHADER_LIGHTING] = 0;
			mVertexShaderLevel[SHADER_INTERFACE] = 0;
			mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
			mVertexShaderLevel[SHADER_WATER] = 0;
			mVertexShaderLevel[SHADER_OBJECT] = 0;
			mVertexShaderLevel[SHADER_EFFECT] = 0;
			mVertexShaderLevel[SHADER_WINDLIGHT] = 0;
			mVertexShaderLevel[SHADER_AVATAR] = 0;
		}
	}
	else
	{
		LLGLSLShader::sNoFixedFunction = false;
		gPipeline.mVertexShadersEnabled = FALSE;
		gPipeline.mVertexShadersLoaded = 0;
		mVertexShaderLevel[SHADER_LIGHTING] = 0;
		mVertexShaderLevel[SHADER_INTERFACE] = 0;
		mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
		mVertexShaderLevel[SHADER_WATER] = 0;
		mVertexShaderLevel[SHADER_OBJECT] = 0;
		mVertexShaderLevel[SHADER_EFFECT] = 0;
		mVertexShaderLevel[SHADER_WINDLIGHT] = 0;
		mVertexShaderLevel[SHADER_AVATAR] = 0;
	}
	
	if (gViewerWindow)
	{
		gViewerWindow->setCursor(UI_CURSOR_ARROW);
	}
	gPipeline.createGLBuffers();

	reentrance = false;
}

void LLViewerShaderMgr::unloadShaders()
{
	gOcclusionProgram.unload();
	gOcclusionCubeProgram.unload();
	gDebugProgram.unload();
	gClipProgram.unload();
	gDownsampleDepthProgram.unload();
	gBenchmarkProgram.unload();
	gAlphaMaskProgram.unload();
	gUIProgram.unload();
	gCustomAlphaProgram.unload();
	gGlowCombineProgram.unload();
	gSplatTextureRectProgram.unload();
	gGlowCombineFXAAProgram.unload();
	gTwoTextureAddProgram.unload();
	gTwoTextureCompareProgram.unload();
	gOneTextureFilterProgram.unload();
	gOneTextureNoColorProgram.unload();
	gSolidColorProgram.unload();

	gObjectFullbrightNoColorProgram.unload();
	gObjectFullbrightNoColorWaterProgram.unload();
	gObjectSimpleProgram.unload();
	gObjectSimpleImpostorProgram.unload();
	gObjectPreviewProgram.unload();
	gImpostorProgram.unload();
	gObjectSimpleAlphaMaskProgram.unload();
	gObjectBumpProgram.unload();
	gObjectSimpleWaterProgram.unload();
	gObjectSimpleWaterAlphaMaskProgram.unload();
	gObjectFullbrightProgram.unload();
	gObjectFullbrightWaterProgram.unload();
	gObjectEmissiveProgram.unload();
	gObjectEmissiveWaterProgram.unload();
	gObjectFullbrightAlphaMaskProgram.unload();
	gObjectFullbrightWaterAlphaMaskProgram.unload();

	gObjectShinyProgram.unload();
	gObjectFullbrightShinyProgram.unload();
	gObjectFullbrightShinyWaterProgram.unload();
	gObjectShinyWaterProgram.unload();

	gObjectSimpleNonIndexedProgram.unload();
	gObjectSimpleNonIndexedTexGenProgram.unload();
	gObjectSimpleNonIndexedTexGenWaterProgram.unload();
	gObjectSimpleNonIndexedWaterProgram.unload();
	gObjectAlphaMaskNonIndexedProgram.unload();
	gObjectAlphaMaskNonIndexedWaterProgram.unload();
	gObjectAlphaMaskNoColorProgram.unload();
	gObjectAlphaMaskNoColorWaterProgram.unload();
	gObjectFullbrightNonIndexedProgram.unload();
	gObjectFullbrightNonIndexedWaterProgram.unload();
	gObjectEmissiveNonIndexedProgram.unload();
	gObjectEmissiveNonIndexedWaterProgram.unload();
	gTreeProgram.unload();
	gTreeWaterProgram.unload();

	gObjectShinyNonIndexedProgram.unload();
	gObjectFullbrightShinyNonIndexedProgram.unload();
	gObjectFullbrightShinyNonIndexedWaterProgram.unload();
	gObjectShinyNonIndexedWaterProgram.unload();

	gSkinnedObjectSimpleProgram.unload();
	gSkinnedObjectFullbrightProgram.unload();
	gSkinnedObjectEmissiveProgram.unload();
	gSkinnedObjectFullbrightShinyProgram.unload();
	gSkinnedObjectShinySimpleProgram.unload();
	
	gSkinnedObjectSimpleWaterProgram.unload();
	gSkinnedObjectFullbrightWaterProgram.unload();
	gSkinnedObjectEmissiveWaterProgram.unload();
	gSkinnedObjectFullbrightShinyWaterProgram.unload();
	gSkinnedObjectShinySimpleWaterProgram.unload();
	

	gWaterProgram.unload();
	gUnderWaterProgram.unload();
	gTerrainProgram.unload();
	gTerrainWaterProgram.unload();
	gGlowProgram.unload();
	gGlowExtractProgram.unload();
	gAvatarProgram.unload();
	gAvatarWaterProgram.unload();
	gAvatarPickProgram.unload();
	gHighlightProgram.unload();
	gHighlightNormalProgram.unload();
	gHighlightSpecularProgram.unload();

	gWLSkyProgram.unload();
	gWLCloudProgram.unload();

	gDeferredDiffuseProgram.unload();
	gDeferredDiffuseAlphaMaskProgram.unload();
	gDeferredNonIndexedDiffuseAlphaMaskProgram.unload();
	gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.unload();
	gDeferredNonIndexedDiffuseProgram.unload();
	gDeferredSkinnedDiffuseProgram.unload();
	gDeferredSkinnedBumpProgram.unload();
	gDeferredSkinnedAlphaProgram.unload();

	mVertexShaderLevel[SHADER_LIGHTING] = 0;
	mVertexShaderLevel[SHADER_OBJECT] = 0;
	mVertexShaderLevel[SHADER_AVATAR] = 0;
	mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
	mVertexShaderLevel[SHADER_WATER] = 0;
	mVertexShaderLevel[SHADER_INTERFACE] = 0;
	mVertexShaderLevel[SHADER_EFFECT] = 0;
	mVertexShaderLevel[SHADER_WINDLIGHT] = 0;

	gPipeline.mVertexShadersLoaded = 0;
}

BOOL LLViewerShaderMgr::loadBasicShaders()
{
	// Load basic dependency shaders first
	// All of these have to load for any shaders to function
	
	S32 sum_lights_class = 3;

	// class one cards will get the lower sum lights
	// class zero we're not going to think about
	// since a class zero card COULD be a ridiculous new card
	// and old cards should have the features masked
	if(LLFeatureManager::getInstance()->getGPUClass() == GPU_CLASS_1)
	{
		sum_lights_class = 2;
	}

	// If we have sun and moon only checked, then only sum those lights.
	if (gPipeline.getLightingDetail() == 0)
	{
		sum_lights_class = 1;
	}

#if LL_DARWIN
	// Work around driver crashes on older Macs when using deferred rendering
	// NORSPEC-59
	//
	if (gGLManager.mIsMobileGF)
		sum_lights_class = 3;
#endif
	
	// Use the feature table to mask out the max light level to use.  Also make sure it's at least 1.
	S32 max_light_class = gSavedSettings.getS32("RenderShaderLightingMaxLevel");
	sum_lights_class = llclamp(sum_lights_class, 1, max_light_class);

	// Load the Basic Vertex Shaders at the appropriate level. 
	// (in order of shader function call depth for reference purposes, deepest level first)

	std::vector< std::pair<std::string, S32> > shaders;
	shaders.push_back( std::make_pair( "windlight/atmosphericsVarsV.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( std::make_pair( "windlight/atmosphericsVarsWaterV.glsl",	mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( std::make_pair( "windlight/atmosphericsHelpersV.glsl",	mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( std::make_pair( "lighting/lightFuncV.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( std::make_pair( "lighting/sumLightsV.glsl",				sum_lights_class ) );
	shaders.push_back( std::make_pair( "lighting/lightV.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( std::make_pair( "lighting/lightFuncSpecularV.glsl",		mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( std::make_pair( "lighting/sumLightsSpecularV.glsl",		sum_lights_class ) );
	shaders.push_back( std::make_pair( "lighting/lightSpecularV.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( std::make_pair( "windlight/atmosphericsV.glsl",			mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( std::make_pair( "avatar/avatarSkinV.glsl",				1 ) );
	shaders.push_back( std::make_pair( "avatar/objectSkinV.glsl",				1 ) );
	if (gGLManager.mGLSLVersionMajor >= 2 || gGLManager.mGLSLVersionMinor >= 30)
	{
		shaders.push_back( std::make_pair( "objects/indexedTextureV.glsl",			1 ) );
	}
	shaders.push_back( std::make_pair( "objects/nonindexedTextureV.glsl",		1 ) );

	boost::unordered_map<std::string, std::string> attribs;
	attribs["MAX_JOINTS_PER_MESH_OBJECT"] = 
		boost::lexical_cast<std::string>(LLSkinningUtil::getMaxJointCount());

	// We no longer have to bind the shaders to global glhandles, they are automatically added to a map now.
	for (U32 i = 0; i < shaders.size(); i++)
	{
		// Note usage of GL_VERTEX_SHADER
		if (loadShaderFile(shaders[i].first, shaders[i].second, GL_VERTEX_SHADER, &attribs) == 0)
		{
			return FALSE;
		}
	}

	// Load the Basic Fragment Shaders at the appropriate level. 
	// (in order of shader function call depth for reference purposes, deepest level first)

	shaders.clear();
	S32 ch = 1;

	if (gGLManager.mGLSLVersionMajor > 1 || gGLManager.mGLSLVersionMinor >= 30)
	{ //use indexed texture rendering for GLSL >= 1.30
		ch = llmax(LLGLSLShader::sIndexedTextureChannels-1, 1);
	}

	std::vector<S32> index_channels;
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "windlight/atmosphericsVarsF.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "windlight/atmosphericsVarsWaterF.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "windlight/gammaF.glsl",					mVertexShaderLevel[SHADER_WINDLIGHT]) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "windlight/atmosphericsF.glsl",			mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "windlight/transportF.glsl",				mVertexShaderLevel[SHADER_WINDLIGHT] ) );	
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "environment/waterFogF.glsl",				mVertexShaderLevel[SHADER_WATER] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightNonIndexedF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightAlphaMaskNonIndexedF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightNonIndexedF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightNonIndexedAlphaMaskF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightWaterNonIndexedF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightWaterAlphaMaskNonIndexedF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightWaterNonIndexedF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightWaterNonIndexedAlphaMaskF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightShinyNonIndexedF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightShinyNonIndexedF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightShinyWaterNonIndexedF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(-1);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightShinyWaterNonIndexedF.glsl", mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightAlphaMaskF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightAlphaMaskF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightWaterF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightWaterAlphaMaskF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightWaterF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightWaterAlphaMaskF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightShinyF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightShinyF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightShinyWaterF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	index_channels.push_back(ch);	 shaders.push_back( std::make_pair( "lighting/lightFullbrightShinyWaterF.glsl", mVertexShaderLevel[SHADER_LIGHTING] ) );
	
	for (U32 i = 0; i < shaders.size(); i++)
	{
		// Note usage of GL_FRAGMENT_SHADER
		if (loadShaderFile(shaders[i].first, shaders[i].second, GL_FRAGMENT_SHADER, &attribs, index_channels[i]) == 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersEnvironment()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_ENVIRONMENT] == 0)
	{
		gTerrainProgram.unload();
		return TRUE;
	}

	if (success)
	{
		gTerrainProgram.mName = "Terrain Shader";
		gTerrainProgram.mFeatures.calculatesLighting = true;
		gTerrainProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainProgram.mFeatures.hasAtmospherics = true;
		gTerrainProgram.mFeatures.mIndexedTextureChannels = 0;
		gTerrainProgram.mFeatures.disableTextureIndex = true;
		gTerrainProgram.mFeatures.hasGamma = true;
		gTerrainProgram.mShaderFiles.clear();
		gTerrainProgram.mShaderFiles.push_back(std::make_pair("environment/terrainV.glsl", GL_VERTEX_SHADER));
		gTerrainProgram.mShaderFiles.push_back(std::make_pair("environment/terrainF.glsl", GL_FRAGMENT_SHADER));
		gTerrainProgram.mShaderLevel = mVertexShaderLevel[SHADER_ENVIRONMENT];
		success = gTerrainProgram.createShader(nullptr, nullptr);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
		return FALSE;
	}
	
	LLWorld::getInstance()->updateWaterObjects();
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersWater()
{
	BOOL success = TRUE;
	BOOL terrainWaterSuccess = TRUE;

	if (mVertexShaderLevel[SHADER_WATER] == 0)
	{
		gWaterProgram.unload();
		gUnderWaterProgram.unload();
		gTerrainWaterProgram.unload();
		return TRUE;
	}

	if (success)
	{
		// load water shader
		gWaterProgram.mName = "Water Shader";
		gWaterProgram.mFeatures.calculatesAtmospherics = true;
		gWaterProgram.mFeatures.hasGamma = true;
		gWaterProgram.mFeatures.hasTransport = true;
		gWaterProgram.mShaderFiles.clear();
		gWaterProgram.mShaderFiles.push_back(std::make_pair("environment/waterV.glsl", GL_VERTEX_SHADER));
		gWaterProgram.mShaderFiles.push_back(std::make_pair("environment/waterF.glsl", GL_FRAGMENT_SHADER));
		gWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_WATER];
		success = gWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		//load under water vertex shader
		gUnderWaterProgram.mName = "Underwater Shader";
		gUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
		gUnderWaterProgram.mShaderFiles.clear();
		gUnderWaterProgram.mShaderFiles.push_back(std::make_pair("environment/waterV.glsl", GL_VERTEX_SHADER));
		gUnderWaterProgram.mShaderFiles.push_back(std::make_pair("environment/underWaterF.glsl", GL_FRAGMENT_SHADER));
		gUnderWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_WATER];
		gUnderWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		
		success = gUnderWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		//load terrain water shader
		gTerrainWaterProgram.mName = "Terrain Water Shader";
		gTerrainWaterProgram.mFeatures.calculatesLighting = true;
		gTerrainWaterProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasWaterFog = true;
		gTerrainWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gTerrainWaterProgram.mFeatures.disableTextureIndex = true;
		gTerrainWaterProgram.mShaderFiles.clear();
		gTerrainWaterProgram.mShaderFiles.push_back(std::make_pair("environment/terrainV.glsl", GL_VERTEX_SHADER));
		gTerrainWaterProgram.mShaderFiles.push_back(std::make_pair("environment/terrainWaterF.glsl", GL_FRAGMENT_SHADER));
		gTerrainWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_ENVIRONMENT];
		gTerrainWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		terrainWaterSuccess = gTerrainWaterProgram.createShader(nullptr, nullptr);
	}	

	/// Keep track of water shader levels
	if (gWaterProgram.mShaderLevel != mVertexShaderLevel[SHADER_WATER]
		|| gUnderWaterProgram.mShaderLevel != mVertexShaderLevel[SHADER_WATER])
	{
		mVertexShaderLevel[SHADER_WATER] = llmin(gWaterProgram.mShaderLevel, gUnderWaterProgram.mShaderLevel);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_WATER] = 0;
		return FALSE;
	}

	// if we failed to load the terrain water shaders and we need them (using class2 water),
	// then drop down to class1 water.
	if (mVertexShaderLevel[SHADER_WATER] > 1 && !terrainWaterSuccess)
	{
		mVertexShaderLevel[SHADER_WATER]--;
		return loadShadersWater();
	}
	
	LLWorld::getInstance()->updateWaterObjects();

	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersEffects()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_EFFECT] == 0)
	{
		gGlowProgram.unload();
		gGlowExtractProgram.unload();
		return TRUE;
	}

	if (success)
	{
		gGlowProgram.mName = "Glow Shader (Post)";
		gGlowProgram.mShaderFiles.clear();
		gGlowProgram.mShaderFiles.push_back(std::make_pair("effects/glowV.glsl", GL_VERTEX_SHADER));
		gGlowProgram.mShaderFiles.push_back(std::make_pair("effects/glowF.glsl", GL_FRAGMENT_SHADER));
		gGlowProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		success = gGlowProgram.createShader(nullptr, nullptr);
		if (!success)
		{
			LLPipeline::sRenderGlow = FALSE;
		}
	}
	
	if (success)
	{
		gGlowExtractProgram.mName = "Glow Extract Shader (Post)";
		gGlowExtractProgram.mShaderFiles.clear();
		gGlowExtractProgram.mShaderFiles.push_back(std::make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
		gGlowExtractProgram.mShaderFiles.push_back(std::make_pair("effects/glowExtractF.glsl", GL_FRAGMENT_SHADER));
		gGlowExtractProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		success = gGlowExtractProgram.createShader(nullptr, nullptr);
		if (!success)
		{
			LLPipeline::sRenderGlow = FALSE;
		}
	}
	
	return success;

}

BOOL LLViewerShaderMgr::loadShadersDeferred()
{
	if (mVertexShaderLevel[SHADER_DEFERRED] == 0)
	{
		gDeferredTreeProgram.unload();
		gDeferredTreeShadowProgram.unload();
		gDeferredDiffuseProgram.unload();
		gDeferredDiffuseAlphaMaskProgram.unload();
		gDeferredNonIndexedDiffuseAlphaMaskProgram.unload();
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.unload();
		gDeferredNonIndexedDiffuseProgram.unload();
		gDeferredSkinnedDiffuseProgram.unload();
		gDeferredSkinnedBumpProgram.unload();
		gDeferredSkinnedAlphaProgram.unload();
		gDeferredBumpProgram.unload();
		gDeferredImpostorProgram.unload();
		gDeferredTerrainProgram.unload();
		gDeferredLightProgram.unload();
		for (U32 i = 0; i < LL_DEFERRED_MULTI_LIGHT_COUNT; ++i)
		{
			gDeferredMultiLightProgram[i].unload();
		}
		gDeferredSpotLightProgram.unload();
		gDeferredMultiSpotLightProgram.unload();
		gDeferredSSAOProgram.unload();
		gDeferredDownsampleDepthNearestProgram.unload();
		gDeferredSunProgram.unload();
		gDeferredBlurLightProgram.unload();
		gDeferredSoftenProgram.unload();
		gDeferredSoftenWaterProgram.unload();
		gDeferredShadowProgram.unload();
		gDeferredShadowCubeProgram.unload();
		gDeferredShadowAlphaMaskProgram.unload();
		gDeferredAvatarShadowProgram.unload();
		gDeferredAttachmentShadowProgram.unload();
		gDeferredAvatarProgram.unload();
		gDeferredAvatarAlphaProgram.unload();
		gDeferredAlphaProgram.unload();
		gDeferredAlphaWaterProgram.unload();
		gDeferredFullbrightProgram.unload();
		gDeferredFullbrightAlphaMaskProgram.unload();
		gDeferredFullbrightWaterProgram.unload();
		gDeferredFullbrightAlphaMaskWaterProgram.unload();
		gDeferredEmissiveProgram.unload();
		gDeferredAvatarEyesProgram.unload();
		gDeferredPostProgram.unload();		
		gDeferredCoFProgram.unload();		
		gDeferredDoFCombineProgram.unload();
		gDeferredPostGammaCorrectProgram.unload();
		gFXAAProgram.unload();
		gDeferredWaterProgram.unload();
		gDeferredUnderWaterProgram.unload();
		gDeferredWLSkyProgram.unload();
		gDeferredWLCloudProgram.unload();
		gDeferredStarProgram.unload();
		gDeferredFullbrightShinyProgram.unload();
		gDeferredSkinnedFullbrightShinyProgram.unload();
		gDeferredSkinnedFullbrightProgram.unload();

		gNormalMapGenProgram.unload();
		for (U32 i = 0; i < LLMaterial::SHADER_COUNT*2; ++i)
		{
			gDeferredMaterialProgram[i].unload();
			gDeferredMaterialWaterProgram[i].unload();
		}
		return TRUE;
	}

	BOOL success = TRUE;

	if (success)
	{
		gDeferredDiffuseProgram.mName = "Deferred Diffuse Shader";
		gDeferredDiffuseProgram.mShaderFiles.clear();
		gDeferredDiffuseProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER));
		gDeferredDiffuseProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseIndexedF.glsl", GL_FRAGMENT_SHADER));
		gDeferredDiffuseProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredDiffuseProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredDiffuseProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredDiffuseAlphaMaskProgram.mName = "Deferred Diffuse Alpha Mask Shader";
		gDeferredDiffuseAlphaMaskProgram.mShaderFiles.clear();
		gDeferredDiffuseAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER));
		gDeferredDiffuseAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseAlphaMaskIndexedF.glsl", GL_FRAGMENT_SHADER));
		gDeferredDiffuseAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredDiffuseAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredDiffuseAlphaMaskProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mName = "Deferred Diffuse Non-Indexed Alpha Mask Shader";
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.clear();
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER));
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseAlphaMaskF.glsl", GL_FRAGMENT_SHADER));
		gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredNonIndexedDiffuseAlphaMaskProgram.createShader(nullptr, nullptr);
	}
	
	if (success)
	{
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mName = "Deferred Diffuse Non-Indexed No Color Alpha Mask Shader";
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.clear();
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseNoColorV.glsl", GL_VERTEX_SHADER));
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseAlphaMaskNoColorF.glsl", GL_FRAGMENT_SHADER));
		gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredNonIndexedDiffuseProgram.mName = "Non Indexed Deferred Diffuse Shader";
		gDeferredNonIndexedDiffuseProgram.mShaderFiles.clear();
		gDeferredNonIndexedDiffuseProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER));
		gDeferredNonIndexedDiffuseProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER));
		gDeferredNonIndexedDiffuseProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredNonIndexedDiffuseProgram.createShader(nullptr, nullptr);
	}
		

	if (success)
	{
		gDeferredSkinnedDiffuseProgram.mName = "Deferred Skinned Diffuse Shader";
		gDeferredSkinnedDiffuseProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedDiffuseProgram.mShaderFiles.clear();
		gDeferredSkinnedDiffuseProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseSkinnedV.glsl", GL_VERTEX_SHADER));
		gDeferredSkinnedDiffuseProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER));
		gDeferredSkinnedDiffuseProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSkinnedDiffuseProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredSkinnedBumpProgram.mName = "Deferred Skinned Bump Shader";
		gDeferredSkinnedBumpProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedBumpProgram.mShaderFiles.clear();
		gDeferredSkinnedBumpProgram.mShaderFiles.push_back(std::make_pair("deferred/bumpSkinnedV.glsl", GL_VERTEX_SHADER));
		gDeferredSkinnedBumpProgram.mShaderFiles.push_back(std::make_pair("deferred/bumpF.glsl", GL_FRAGMENT_SHADER));
		gDeferredSkinnedBumpProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSkinnedBumpProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredSkinnedAlphaProgram.mName = "Deferred Skinned Alpha Shader";
		gDeferredSkinnedAlphaProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedAlphaProgram.mFeatures.calculatesLighting = false;
		gDeferredSkinnedAlphaProgram.mFeatures.hasLighting = false;
		gDeferredSkinnedAlphaProgram.mFeatures.isAlphaLighting = true;
		gDeferredSkinnedAlphaProgram.mFeatures.disableTextureIndex = true;
		gDeferredSkinnedAlphaProgram.mShaderFiles.clear();
		gDeferredSkinnedAlphaProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER));
		gDeferredSkinnedAlphaProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER));
		gDeferredSkinnedAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredSkinnedAlphaProgram.addPermutation("USE_DIFFUSE_TEX", "1");
		gDeferredSkinnedAlphaProgram.addPermutation("HAS_SKIN", "1");
		gDeferredSkinnedAlphaProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredSkinnedAlphaProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		success = gDeferredSkinnedAlphaProgram.createShader(nullptr, nullptr);
		
		// Hack to include uniforms for lighting without linking in lighting file
		gDeferredSkinnedAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredSkinnedAlphaProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredBumpProgram.mName = "Deferred Bump Shader";
		gDeferredBumpProgram.mShaderFiles.clear();
		gDeferredBumpProgram.mShaderFiles.push_back(std::make_pair("deferred/bumpV.glsl", GL_VERTEX_SHADER));
		gDeferredBumpProgram.mShaderFiles.push_back(std::make_pair("deferred/bumpF.glsl", GL_FRAGMENT_SHADER));
		gDeferredBumpProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredBumpProgram.createShader(nullptr, nullptr);
	}

	gDeferredMaterialProgram[1].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[5].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[9].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[13].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;

	gDeferredMaterialWaterProgram[1].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[5].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[9].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[13].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
	gDeferredMaterialWaterProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;

	for (U32 i = 0; i < LLMaterial::SHADER_COUNT*2; ++i)
	{
		if (success)
		{
			gDeferredMaterialProgram[i].mName = llformat("Deferred Material Shader %d", i);
			
			U32 alpha_mode = i & 0x3;

			gDeferredMaterialProgram[i].mShaderFiles.clear();
			gDeferredMaterialProgram[i].mShaderFiles.push_back(std::make_pair("deferred/materialV.glsl", GL_VERTEX_SHADER));
			gDeferredMaterialProgram[i].mShaderFiles.push_back(std::make_pair("deferred/materialF.glsl", GL_FRAGMENT_SHADER));
			gDeferredMaterialProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			gDeferredMaterialProgram[i].addPermutation("HAS_NORMAL_MAP", i & 0x8? "1" : "0");
			gDeferredMaterialProgram[i].addPermutation("HAS_SPECULAR_MAP", i & 0x4 ? "1" : "0");
			gDeferredMaterialProgram[i].addPermutation("DIFFUSE_ALPHA_MODE", llformat("%d", alpha_mode));
			gDeferredMaterialProgram[i].addPermutation("HAS_SUN_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
			bool has_skin = i & 0x10;
			gDeferredMaterialProgram[i].addPermutation("HAS_SKIN",has_skin ? "1" : "0");

			if (has_skin)
			{
				gDeferredMaterialProgram[i].mFeatures.hasObjectSkinning = true;
			}

			success = gDeferredMaterialProgram[i].createShader(nullptr, nullptr);
		}

		if (success)
		{
			gDeferredMaterialWaterProgram[i].mName = llformat("Deferred Underwater Material Shader %d", i);

			U32 alpha_mode = i & 0x3;

			gDeferredMaterialWaterProgram[i].mShaderFiles.clear();
			gDeferredMaterialWaterProgram[i].mShaderFiles.push_back(std::make_pair("deferred/materialV.glsl", GL_VERTEX_SHADER));
			gDeferredMaterialWaterProgram[i].mShaderFiles.push_back(std::make_pair("deferred/materialF.glsl", GL_FRAGMENT_SHADER));
			gDeferredMaterialWaterProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			gDeferredMaterialWaterProgram[i].mShaderGroup = LLGLSLShader::SG_WATER;

			gDeferredMaterialWaterProgram[i].addPermutation("HAS_NORMAL_MAP", i & 0x8? "1" : "0");
			gDeferredMaterialWaterProgram[i].addPermutation("HAS_SPECULAR_MAP", i & 0x4 ? "1" : "0");
			gDeferredMaterialWaterProgram[i].addPermutation("DIFFUSE_ALPHA_MODE", llformat("%d", alpha_mode));
			gDeferredMaterialWaterProgram[i].addPermutation("HAS_SUN_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
			bool has_skin = i & 0x10;
			gDeferredMaterialWaterProgram[i].addPermutation("HAS_SKIN",has_skin ? "1" : "0");
			gDeferredMaterialWaterProgram[i].addPermutation("WATER_FOG","1");

			if (has_skin)
			{
				gDeferredMaterialWaterProgram[i].mFeatures.hasObjectSkinning = true;
			}

			success = gDeferredMaterialWaterProgram[i].createShader(nullptr, nullptr);//&mWLUniforms);
		}
	}

	gDeferredMaterialProgram[1].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[5].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[9].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[13].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;

	gDeferredMaterialWaterProgram[1].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[5].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[9].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[13].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
	gDeferredMaterialWaterProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;

	
	if (success)
	{
		gDeferredTreeProgram.mName = "Deferred Tree Shader";
		gDeferredTreeProgram.mShaderFiles.clear();
		gDeferredTreeProgram.mShaderFiles.push_back(std::make_pair("deferred/treeV.glsl", GL_VERTEX_SHADER));
		gDeferredTreeProgram.mShaderFiles.push_back(std::make_pair("deferred/treeF.glsl", GL_FRAGMENT_SHADER));
		gDeferredTreeProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredTreeProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredTreeShadowProgram.mName = "Deferred Tree Shadow Shader";
		gDeferredTreeShadowProgram.mShaderFiles.clear();
		gDeferredTreeShadowProgram.mShaderFiles.push_back(std::make_pair("deferred/treeShadowV.glsl", GL_VERTEX_SHADER));
		gDeferredTreeShadowProgram.mShaderFiles.push_back(std::make_pair("deferred/treeShadowF.glsl", GL_FRAGMENT_SHADER));
		gDeferredTreeShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredTreeShadowProgram.addPermutation("DEPTH_CLAMP", /*gGLManager.mHasDepthClamp ? "1" :*/ "0");
		success = gDeferredTreeShadowProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredImpostorProgram.mName = "Deferred Impostor Shader";
		gDeferredImpostorProgram.mShaderFiles.clear();
		gDeferredImpostorProgram.mShaderFiles.push_back(std::make_pair("deferred/impostorV.glsl", GL_VERTEX_SHADER));
		gDeferredImpostorProgram.mShaderFiles.push_back(std::make_pair("deferred/impostorF.glsl", GL_FRAGMENT_SHADER));
		gDeferredImpostorProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredImpostorProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{		
		gDeferredLightProgram.mName = "Deferred Light Shader";
		gDeferredLightProgram.mShaderFiles.clear();
		gDeferredLightProgram.mShaderFiles.push_back(std::make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER));
		gDeferredLightProgram.mShaderFiles.push_back(std::make_pair("deferred/pointLightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredLightProgram.createShader(nullptr, nullptr);
	}

	for (U32 i = 0; i < LL_DEFERRED_MULTI_LIGHT_COUNT; i++)
	{
	if (success)
	{
			gDeferredMultiLightProgram[i].mName = llformat("Deferred MultiLight Shader %d", i);
			gDeferredMultiLightProgram[i].mShaderFiles.clear();
			gDeferredMultiLightProgram[i].mShaderFiles.push_back(std::make_pair("deferred/multiPointLightV.glsl", GL_VERTEX_SHADER));
			gDeferredMultiLightProgram[i].mShaderFiles.push_back(std::make_pair("deferred/multiPointLightF.glsl", GL_FRAGMENT_SHADER));
			gDeferredMultiLightProgram[i].mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			gDeferredMultiLightProgram[i].addPermutation("LIGHT_COUNT", llformat("%d", i+1));
			success = gDeferredMultiLightProgram[i].createShader(nullptr, nullptr);
		}
	}

	if (success)
	{
		gDeferredSpotLightProgram.mName = "Deferred SpotLight Shader";
		gDeferredSpotLightProgram.mShaderFiles.clear();
		gDeferredSpotLightProgram.mShaderFiles.push_back(std::make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER));
		gDeferredSpotLightProgram.mShaderFiles.push_back(std::make_pair("deferred/spotLightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredSpotLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredSpotLightProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredMultiSpotLightProgram.mName = "Deferred MultiSpotLight Shader";
		gDeferredMultiSpotLightProgram.mShaderFiles.clear();
		gDeferredMultiSpotLightProgram.mShaderFiles.push_back(std::make_pair("deferred/multiPointLightV.glsl", GL_VERTEX_SHADER));
		gDeferredMultiSpotLightProgram.mShaderFiles.push_back(std::make_pair("deferred/multiSpotLightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredMultiSpotLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredMultiSpotLightProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		std::string fragment;
		std::string vertex = "deferred/postDeferredNoTCV.glsl";

		if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
		{
			fragment = "deferred/sunLightSSAOF.glsl";
		}
		else
		{
			fragment = "deferred/sunLightF.glsl";
			if (mVertexShaderLevel[SHADER_DEFERRED] == 1)
			{ //no shadows, no SSAO, no frag coord
				vertex = "deferred/sunLightNoFragCoordV.glsl";
			}
		}

		gDeferredSunProgram.mName = "Deferred Sun Shader";
		gDeferredSunProgram.mShaderFiles.clear();
		gDeferredSunProgram.mShaderFiles.push_back(std::make_pair(vertex, GL_VERTEX_SHADER));
		gDeferredSunProgram.mShaderFiles.push_back(std::make_pair(fragment, GL_FRAGMENT_SHADER));
		gDeferredSunProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredSunProgram.createShader(nullptr, nullptr);
	}

	if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
	 {
		if (success)
		{
			gDeferredSSAOProgram.mName = "Deferred Ambient Occlusion Shader";
			gDeferredSSAOProgram.mShaderFiles.clear();
			gDeferredSSAOProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredSSAOProgram.mShaderFiles.push_back(std::make_pair("deferred/SSAOF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredSSAOProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredSSAOProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gDeferredDownsampleDepthNearestProgram.mName = "Deferred Nearest Downsample Depth Shader";
			gDeferredDownsampleDepthNearestProgram.mShaderFiles.clear();
			gDeferredDownsampleDepthNearestProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredDownsampleDepthNearestProgram.mShaderFiles.push_back(std::make_pair("deferred/downsampleDepthNearestF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredDownsampleDepthNearestProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredDownsampleDepthNearestProgram.createShader(nullptr, nullptr);
		}
	}

	if (success)
	{
		gDeferredBlurLightProgram.mName = "Deferred Blur Light Shader";
		gDeferredBlurLightProgram.mShaderFiles.clear();
		gDeferredBlurLightProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gDeferredBlurLightProgram.mShaderFiles.push_back(std::make_pair("deferred/blurLightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredBlurLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredBlurLightProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredAlphaProgram.mName = "Deferred Alpha Shader";

		gDeferredAlphaProgram.mFeatures.calculatesLighting = false;
		gDeferredAlphaProgram.mFeatures.hasLighting = false;
		gDeferredAlphaProgram.mFeatures.isAlphaLighting = true;
		gDeferredAlphaProgram.mFeatures.disableTextureIndex = true; //hack to disable auto-setup of texture channels
		if (mVertexShaderLevel[SHADER_DEFERRED] < 1)
		{
			gDeferredAlphaProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{ //shave off some texture units for shadow maps
			gDeferredAlphaProgram.mFeatures.mIndexedTextureChannels = llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}
			
		gDeferredAlphaProgram.mShaderFiles.clear();
		gDeferredAlphaProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER));
		gDeferredAlphaProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER));
		gDeferredAlphaProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		gDeferredAlphaProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredAlphaProgram.createShader(nullptr, nullptr);

		// Hack
		gDeferredAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredAlphaImpostorProgram.mName = "Deferred Alpha Impostor Shader";

		gDeferredAlphaImpostorProgram.mFeatures.calculatesLighting = false;
		gDeferredAlphaImpostorProgram.mFeatures.hasLighting = false;
		gDeferredAlphaImpostorProgram.mFeatures.isAlphaLighting = true;
		gDeferredAlphaImpostorProgram.mFeatures.disableTextureIndex = true; //hack to disable auto-setup of texture channels
		if (mVertexShaderLevel[SHADER_DEFERRED] < 1)
		{
			gDeferredAlphaImpostorProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{ //shave off some texture units for shadow maps
			gDeferredAlphaImpostorProgram.mFeatures.mIndexedTextureChannels = llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}

		gDeferredAlphaImpostorProgram.mShaderFiles.clear();
		gDeferredAlphaImpostorProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER));
		gDeferredAlphaImpostorProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER));
		gDeferredAlphaImpostorProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaImpostorProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		gDeferredAlphaImpostorProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredAlphaImpostorProgram.addPermutation("FOR_IMPOSTOR", "1");

		gDeferredAlphaImpostorProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredAlphaImpostorProgram.createShader(nullptr, nullptr);

		// Hack
		gDeferredAlphaImpostorProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaImpostorProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredAlphaWaterProgram.mName = "Deferred Alpha Underwater Shader";
		gDeferredAlphaWaterProgram.mFeatures.calculatesLighting = false;
		gDeferredAlphaWaterProgram.mFeatures.hasLighting = false;
		gDeferredAlphaWaterProgram.mFeatures.isAlphaLighting = true;
		gDeferredAlphaWaterProgram.mFeatures.disableTextureIndex = true; //hack to disable auto-setup of texture channels
		if (mVertexShaderLevel[SHADER_DEFERRED] < 1)
		{
			gDeferredAlphaWaterProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		}
		else
		{ //shave off some texture units for shadow maps
			gDeferredAlphaWaterProgram.mFeatures.mIndexedTextureChannels = llmax(LLGLSLShader::sIndexedTextureChannels - 6, 1);
		}
		gDeferredAlphaWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredAlphaWaterProgram.mShaderFiles.clear();
		gDeferredAlphaWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER));
		gDeferredAlphaWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER));
		gDeferredAlphaWaterProgram.addPermutation("USE_INDEXED_TEX", "1");
		gDeferredAlphaWaterProgram.addPermutation("WATER_FOG", "1");
		gDeferredAlphaWaterProgram.addPermutation("USE_VERTEX_COLOR", "1");
		gDeferredAlphaWaterProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		gDeferredAlphaWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredAlphaWaterProgram.createShader(nullptr, nullptr);

		// Hack
		gDeferredAlphaWaterProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaWaterProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredAvatarEyesProgram.mName = "Deferred Avatar Eyes Shader";
		gDeferredAvatarEyesProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredAvatarEyesProgram.mFeatures.hasGamma = true;
		gDeferredAvatarEyesProgram.mFeatures.hasTransport = true;
		gDeferredAvatarEyesProgram.mFeatures.disableTextureIndex = true;
		gDeferredAvatarEyesProgram.mShaderFiles.clear();
		gDeferredAvatarEyesProgram.mShaderFiles.push_back(std::make_pair("deferred/avatarEyesV.glsl", GL_VERTEX_SHADER));
		gDeferredAvatarEyesProgram.mShaderFiles.push_back(std::make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER));
		gDeferredAvatarEyesProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAvatarEyesProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredFullbrightProgram.mName = "Deferred Fullbright Shader";
		gDeferredFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightProgram.mShaderFiles.clear();
		gDeferredFullbrightProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
		gDeferredFullbrightProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredFullbrightProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredFullbrightAlphaMaskProgram.mName = "Deferred Fullbright Alpha Masking Shader";
		gDeferredFullbrightAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightAlphaMaskProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightAlphaMaskProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightAlphaMaskProgram.mShaderFiles.clear();
		gDeferredFullbrightAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
		gDeferredFullbrightAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredFullbrightAlphaMaskProgram.addPermutation("HAS_ALPHA_MASK","1");
		gDeferredFullbrightAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredFullbrightAlphaMaskProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredFullbrightWaterProgram.mName = "Deferred Fullbright Underwater Shader";
		gDeferredFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightWaterProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightWaterProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightWaterProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightWaterProgram.mShaderFiles.clear();
		gDeferredFullbrightWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
		gDeferredFullbrightWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredFullbrightWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredFullbrightWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredFullbrightWaterProgram.addPermutation("WATER_FOG","1");
		success = gDeferredFullbrightWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredFullbrightAlphaMaskWaterProgram.mName = "Deferred Fullbright Underwater Alpha Masking Shader";
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightAlphaMaskWaterProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderFiles.clear();
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredFullbrightAlphaMaskWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		gDeferredFullbrightAlphaMaskWaterProgram.addPermutation("HAS_ALPHA_MASK","1");
		gDeferredFullbrightAlphaMaskWaterProgram.addPermutation("WATER_FOG","1");
		success = gDeferredFullbrightAlphaMaskWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredFullbrightShinyProgram.mName = "Deferred Fullbright Shiny Shader";
		gDeferredFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightShinyProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightShinyProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightShinyProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels-1;
		gDeferredFullbrightShinyProgram.mShaderFiles.clear();
		gDeferredFullbrightShinyProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightShinyV.glsl", GL_VERTEX_SHADER));
		gDeferredFullbrightShinyProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER));
		gDeferredFullbrightShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredFullbrightShinyProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredSkinnedFullbrightProgram.mName = "Deferred Skinned Fullbright Shader";
		gDeferredSkinnedFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasGamma = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasTransport = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedFullbrightProgram.mFeatures.disableTextureIndex = true;
		gDeferredSkinnedFullbrightProgram.mShaderFiles.clear();
		gDeferredSkinnedFullbrightProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightSkinnedV.glsl", GL_VERTEX_SHADER));
		gDeferredSkinnedFullbrightProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredSkinnedFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gDeferredSkinnedFullbrightProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredSkinnedFullbrightShinyProgram.mName = "Deferred Skinned Fullbright Shiny Shader";
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasGamma = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasTransport = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.hasObjectSkinning = true;
		gDeferredSkinnedFullbrightShinyProgram.mFeatures.disableTextureIndex = true;
		gDeferredSkinnedFullbrightShinyProgram.mShaderFiles.clear();
		gDeferredSkinnedFullbrightShinyProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinySkinnedV.glsl", GL_VERTEX_SHADER));
		gDeferredSkinnedFullbrightShinyProgram.mShaderFiles.push_back(std::make_pair("deferred/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER));
		gDeferredSkinnedFullbrightShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gDeferredSkinnedFullbrightShinyProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredEmissiveProgram.mName = "Deferred Emissive Shader";
		gDeferredEmissiveProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredEmissiveProgram.mFeatures.hasGamma = true;
		gDeferredEmissiveProgram.mFeatures.hasTransport = true;
		gDeferredEmissiveProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredEmissiveProgram.mShaderFiles.clear();
		gDeferredEmissiveProgram.mShaderFiles.push_back(std::make_pair("deferred/emissiveV.glsl", GL_VERTEX_SHADER));
		gDeferredEmissiveProgram.mShaderFiles.push_back(std::make_pair("deferred/emissiveF.glsl", GL_FRAGMENT_SHADER));
		gDeferredEmissiveProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredEmissiveProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		// load water shader
		gDeferredWaterProgram.mName = "Deferred Water Shader";
		gDeferredWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredWaterProgram.mFeatures.hasGamma = true;
		gDeferredWaterProgram.mFeatures.hasTransport = true;
		gDeferredWaterProgram.mShaderFiles.clear();
		gDeferredWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/waterV.glsl", GL_VERTEX_SHADER));
		gDeferredWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/waterF.glsl", GL_FRAGMENT_SHADER));
		gDeferredWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		// load water shader
		gDeferredUnderWaterProgram.mName = "Deferred Under Water Shader";
		gDeferredUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredUnderWaterProgram.mFeatures.hasGamma = true;
		gDeferredUnderWaterProgram.mFeatures.hasTransport = true;
		gDeferredUnderWaterProgram.mShaderFiles.clear();
		gDeferredUnderWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/waterV.glsl", GL_VERTEX_SHADER));
		gDeferredUnderWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/underWaterF.glsl", GL_FRAGMENT_SHADER));
		gDeferredUnderWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredUnderWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredSoftenProgram.mName = "Deferred Soften Shader";
		gDeferredSoftenProgram.mShaderFiles.clear();
		gDeferredSoftenProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gDeferredSoftenProgram.mShaderFiles.push_back(std::make_pair("deferred/softenLightF.glsl", GL_FRAGMENT_SHADER));
		gDeferredSoftenProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
		{ //if using SSAO, take screen space light map into account as if shadows are enabled
			gDeferredSoftenProgram.mShaderLevel = llmax(gDeferredSoftenProgram.mShaderLevel, 2);
		}

		success = gDeferredSoftenProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredSoftenWaterProgram.mName = "Deferred Soften Underwater Shader";
		gDeferredSoftenWaterProgram.mShaderFiles.clear();
		gDeferredSoftenWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gDeferredSoftenWaterProgram.mShaderFiles.push_back(std::make_pair("deferred/softenLightF.glsl", GL_FRAGMENT_SHADER));

		gDeferredSoftenWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredSoftenWaterProgram.addPermutation("WATER_FOG", "1");
		gDeferredSoftenWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;

		if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
		{ //if using SSAO, take screen space light map into account as if shadows are enabled
			gDeferredSoftenWaterProgram.mShaderLevel = llmax(gDeferredSoftenWaterProgram.mShaderLevel, 2);
		}

		success = gDeferredSoftenWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredShadowProgram.mName = "Deferred Shadow Shader";
		gDeferredShadowProgram.mShaderFiles.clear();
		gDeferredShadowProgram.mShaderFiles.push_back(std::make_pair("deferred/shadowV.glsl", GL_VERTEX_SHADER));
		gDeferredShadowProgram.mShaderFiles.push_back(std::make_pair("deferred/shadowF.glsl", GL_FRAGMENT_SHADER));
		gDeferredShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredShadowProgram.addPermutation("DEPTH_CLAMP", /*gGLManager.mHasDepthClamp ? "1" :*/ "0");
		success = gDeferredShadowProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredShadowCubeProgram.mName = "Deferred Shadow Cube Shader";
		gDeferredShadowCubeProgram.mShaderFiles.clear();
		gDeferredShadowCubeProgram.mShaderFiles.push_back(std::make_pair("deferred/shadowCubeV.glsl", GL_VERTEX_SHADER));
		gDeferredShadowCubeProgram.mShaderFiles.push_back(std::make_pair("deferred/shadowF.glsl", GL_FRAGMENT_SHADER));
		gDeferredShadowCubeProgram.addPermutation("DEPTH_CLAMP", /*gGLManager.mHasDepthClamp ? "1" :*/ "0");
		gDeferredShadowCubeProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredShadowCubeProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredShadowAlphaMaskProgram.mName = "Deferred Shadow Alpha Mask Shader";
		gDeferredShadowAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
		gDeferredShadowAlphaMaskProgram.mShaderFiles.clear();
		gDeferredShadowAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("deferred/shadowAlphaMaskV.glsl", GL_VERTEX_SHADER));
		gDeferredShadowAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("deferred/shadowAlphaMaskF.glsl", GL_FRAGMENT_SHADER));
		gDeferredShadowAlphaMaskProgram.addPermutation("DEPTH_CLAMP", /*gGLManager.mHasDepthClamp ? "1" :*/ "0");
		gDeferredShadowAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredShadowAlphaMaskProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredAvatarShadowProgram.mName = "Deferred Avatar Shadow Shader";
		gDeferredAvatarShadowProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarShadowProgram.mShaderFiles.clear();
		gDeferredAvatarShadowProgram.mShaderFiles.push_back(std::make_pair("deferred/avatarShadowV.glsl", GL_VERTEX_SHADER));
		gDeferredAvatarShadowProgram.mShaderFiles.push_back(std::make_pair("deferred/avatarShadowF.glsl", GL_FRAGMENT_SHADER));
		gDeferredAvatarShadowProgram.addPermutation("DEPTH_CLAMP", /*gGLManager.mHasDepthClamp ? "1" :*/ "0");
		gDeferredAvatarShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAvatarShadowProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredAttachmentShadowProgram.mName = "Deferred Attachment Shadow Shader";
		gDeferredAttachmentShadowProgram.mFeatures.hasObjectSkinning = true;
		gDeferredAttachmentShadowProgram.mShaderFiles.clear();
		gDeferredAttachmentShadowProgram.mShaderFiles.push_back(std::make_pair("deferred/attachmentShadowV.glsl", GL_VERTEX_SHADER));
		gDeferredAttachmentShadowProgram.mShaderFiles.push_back(std::make_pair("deferred/attachmentShadowF.glsl", GL_FRAGMENT_SHADER));
		gDeferredAttachmentShadowProgram.addPermutation("DEPTH_CLAMP", /*gGLManager.mHasDepthClamp ? "1" :*/ "0");
		gDeferredAttachmentShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAttachmentShadowProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gTerrainProgram.mName = "Deferred Terrain Shader";
		gDeferredTerrainProgram.mShaderFiles.clear();
		gDeferredTerrainProgram.mShaderFiles.push_back(std::make_pair("deferred/terrainV.glsl", GL_VERTEX_SHADER));
		gDeferredTerrainProgram.mShaderFiles.push_back(std::make_pair("deferred/terrainF.glsl", GL_FRAGMENT_SHADER));
		gDeferredTerrainProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredTerrainProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredAvatarProgram.mName = "Deferred Avatar Shader";
		gDeferredAvatarProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarProgram.mShaderFiles.clear();
		gDeferredAvatarProgram.mShaderFiles.push_back(std::make_pair("deferred/avatarV.glsl", GL_VERTEX_SHADER));
		gDeferredAvatarProgram.mShaderFiles.push_back(std::make_pair("deferred/avatarF.glsl", GL_FRAGMENT_SHADER));
		gDeferredAvatarProgram.addPermutation("AVATAR_CLOTH", (mVertexShaderLevel[SHADER_AVATAR] == 3) ? "1" : "0");
		gDeferredAvatarProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAvatarProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredAvatarAlphaProgram.mName = "Deferred Avatar Alpha Shader";
		gDeferredAvatarAlphaProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarAlphaProgram.mFeatures.calculatesLighting = false;
		gDeferredAvatarAlphaProgram.mFeatures.hasLighting = false;
		gDeferredAvatarAlphaProgram.mFeatures.isAlphaLighting = true;
		gDeferredAvatarAlphaProgram.mFeatures.disableTextureIndex = true;
		gDeferredAvatarAlphaProgram.mShaderFiles.clear();
		gDeferredAvatarAlphaProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER));
		gDeferredAvatarAlphaProgram.mShaderFiles.push_back(std::make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER));
		gDeferredAvatarAlphaProgram.addPermutation("USE_DIFFUSE_TEX", "1");
		gDeferredAvatarAlphaProgram.addPermutation("IS_AVATAR_SKIN", "1");
		gDeferredAvatarAlphaProgram.addPermutation("HAS_SHADOW", mVertexShaderLevel[SHADER_DEFERRED] > 1 ? "1" : "0");
		gDeferredAvatarAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];

		success = gDeferredAvatarAlphaProgram.createShader(nullptr, nullptr);

		gDeferredAvatarAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredAvatarAlphaProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gDeferredPostGammaCorrectProgram.mName = "Deferred Gamma Correction Post Process";
		gDeferredPostGammaCorrectProgram.mShaderFiles.clear();
		gDeferredPostGammaCorrectProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gDeferredPostGammaCorrectProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredGammaCorrectF.glsl", GL_FRAGMENT_SHADER));
		gDeferredPostGammaCorrectProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredPostGammaCorrectProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gFXAAProgram.mName = "FXAA Shader";
		gFXAAProgram.mShaderFiles.clear();
		gFXAAProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gFXAAProgram.mShaderFiles.push_back(std::make_pair("deferred/fxaaF.glsl", GL_FRAGMENT_SHADER));
		gFXAAProgram.addPermutation("FXAA_QUALITY_PRESET", std::to_string(gSavedSettings.getU32("RenderDeferredFXAAQuality"))); // <alchemy/>
		gFXAAProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gFXAAProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredPostProgram.mName = "Deferred Post Shader";
		gDeferredPostProgram.mShaderFiles.clear();
		gDeferredPostProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gDeferredPostProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredF.glsl", GL_FRAGMENT_SHADER));
		gDeferredPostProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredPostProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredCoFProgram.mName = "Deferred CoF Shader";
		gDeferredCoFProgram.mShaderFiles.clear();
		gDeferredCoFProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gDeferredCoFProgram.mShaderFiles.push_back(std::make_pair("deferred/cofF.glsl", GL_FRAGMENT_SHADER));
		gDeferredCoFProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredCoFProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredDoFCombineProgram.mName = "Deferred DoFCombine Shader";
		gDeferredDoFCombineProgram.mShaderFiles.clear();
		gDeferredDoFCombineProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gDeferredDoFCombineProgram.mShaderFiles.push_back(std::make_pair("deferred/dofCombineF.glsl", GL_FRAGMENT_SHADER));
		gDeferredDoFCombineProgram.addPermutation("USE_FILM_GRAIN", gSavedSettings.getBOOL("RenderDeferredDoFGrain") ? "1" : "0");
		gDeferredDoFCombineProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredDoFCombineProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredPostNoDoFProgram.mName = "Deferred Post No DoF Shader";
		gDeferredPostNoDoFProgram.mShaderFiles.clear();
		gDeferredPostNoDoFProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
		gDeferredPostNoDoFProgram.mShaderFiles.push_back(std::make_pair("deferred/postDeferredNoDoFF.glsl", GL_FRAGMENT_SHADER));
		gDeferredPostNoDoFProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredPostNoDoFProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredWLSkyProgram.mName = "Deferred Windlight Sky Shader";
		//gWLSkyProgram.mFeatures.hasGamma = true;
		gDeferredWLSkyProgram.mShaderFiles.clear();
		gDeferredWLSkyProgram.mShaderFiles.push_back(std::make_pair("deferred/skyV.glsl", GL_VERTEX_SHADER));
		gDeferredWLSkyProgram.mShaderFiles.push_back(std::make_pair("deferred/skyF.glsl", GL_FRAGMENT_SHADER));
		gDeferredWLSkyProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredWLSkyProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredWLCloudProgram.mName = "Deferred Windlight Cloud Program";
		gDeferredWLCloudProgram.mShaderFiles.clear();
		gDeferredWLCloudProgram.mShaderFiles.push_back(std::make_pair("deferred/cloudsV.glsl", GL_VERTEX_SHADER));
		gDeferredWLCloudProgram.mShaderFiles.push_back(std::make_pair("deferred/cloudsF.glsl", GL_FRAGMENT_SHADER));
		gDeferredWLCloudProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredWLCloudProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDeferredStarProgram.mName = "Deferred Star Program";
		static std::vector<LLStaticHashedString> shaderUniforms = { LLStaticHashedString("custom_alpha") };
		gDeferredStarProgram.mShaderFiles.clear();
		gDeferredStarProgram.mShaderFiles.push_back(std::make_pair("deferred/starsV.glsl", GL_VERTEX_SHADER));
		gDeferredStarProgram.mShaderFiles.push_back(std::make_pair("deferred/starsF.glsl", GL_FRAGMENT_SHADER));
		gDeferredStarProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gDeferredStarProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gDeferredStarProgram.createShader(nullptr, &shaderUniforms);
	}

	if (success)
	{
		gNormalMapGenProgram.mName = "Normal Map Generation Program";
		gNormalMapGenProgram.mShaderFiles.clear();
		gNormalMapGenProgram.mShaderFiles.push_back(std::make_pair("deferred/normgenV.glsl", GL_VERTEX_SHADER));
		gNormalMapGenProgram.mShaderFiles.push_back(std::make_pair("deferred/normgenF.glsl", GL_FRAGMENT_SHADER));
		gNormalMapGenProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		gNormalMapGenProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gNormalMapGenProgram.createShader(nullptr, nullptr);
	}

	return success;
}

BOOL LLViewerShaderMgr::loadShadersObject()
{
	BOOL success = TRUE;
	
	if (mVertexShaderLevel[SHADER_OBJECT] == 0)
	{
		gObjectShinyProgram.unload();
		gObjectFullbrightShinyProgram.unload();
		gObjectFullbrightShinyWaterProgram.unload();
		gObjectShinyWaterProgram.unload();
		gObjectFullbrightNoColorProgram.unload();
		gObjectFullbrightNoColorWaterProgram.unload();
		gObjectSimpleProgram.unload();
		gObjectSimpleImpostorProgram.unload();
		gObjectPreviewProgram.unload();
		gImpostorProgram.unload();
		gObjectSimpleAlphaMaskProgram.unload();
		gObjectBumpProgram.unload();
		gObjectSimpleWaterProgram.unload();
		gObjectSimpleWaterAlphaMaskProgram.unload();
		gObjectEmissiveProgram.unload();
		gObjectEmissiveWaterProgram.unload();
		gObjectFullbrightProgram.unload();
		gObjectFullbrightAlphaMaskProgram.unload();
		gObjectFullbrightWaterProgram.unload();
		gObjectFullbrightWaterAlphaMaskProgram.unload();
		gObjectShinyNonIndexedProgram.unload();
		gObjectFullbrightShinyNonIndexedProgram.unload();
		gObjectFullbrightShinyNonIndexedWaterProgram.unload();
		gObjectShinyNonIndexedWaterProgram.unload();
		gObjectSimpleNonIndexedTexGenProgram.unload();
		gObjectSimpleNonIndexedTexGenWaterProgram.unload();
		gObjectSimpleNonIndexedWaterProgram.unload();
		gObjectAlphaMaskNonIndexedProgram.unload();
		gObjectAlphaMaskNonIndexedWaterProgram.unload();
		gObjectAlphaMaskNoColorProgram.unload();
		gObjectAlphaMaskNoColorWaterProgram.unload();
		gObjectFullbrightNonIndexedProgram.unload();
		gObjectFullbrightNonIndexedWaterProgram.unload();
		gObjectEmissiveNonIndexedProgram.unload();
		gObjectEmissiveNonIndexedWaterProgram.unload();
		gSkinnedObjectSimpleProgram.unload();
		gSkinnedObjectFullbrightProgram.unload();
		gSkinnedObjectEmissiveProgram.unload();
		gSkinnedObjectFullbrightShinyProgram.unload();
		gSkinnedObjectShinySimpleProgram.unload();
		gSkinnedObjectSimpleWaterProgram.unload();
		gSkinnedObjectFullbrightWaterProgram.unload();
		gSkinnedObjectEmissiveWaterProgram.unload();
		gSkinnedObjectFullbrightShinyWaterProgram.unload();
		gSkinnedObjectShinySimpleWaterProgram.unload();
		gTreeProgram.unload();
		gTreeWaterProgram.unload();
	
		return TRUE;
	}

	if (success)
	{
		gObjectSimpleNonIndexedProgram.mName = "Non indexed Shader";
		gObjectSimpleNonIndexedProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectSimpleNonIndexedProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedProgram.mFeatures.disableTextureIndex = true;
		gObjectSimpleNonIndexedProgram.mShaderFiles.clear();
		gObjectSimpleNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleNonIndexedProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectSimpleNonIndexedProgram.createShader(nullptr, nullptr);
	}
	
	if (success)
	{
		gObjectSimpleNonIndexedTexGenProgram.mName = "Non indexed tex-gen Shader";
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasGamma = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedTexGenProgram.mFeatures.disableTextureIndex = true;
		gObjectSimpleNonIndexedTexGenProgram.mShaderFiles.clear();
		gObjectSimpleNonIndexedTexGenProgram.mShaderFiles.push_back(std::make_pair("objects/simpleTexGenV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleNonIndexedTexGenProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleNonIndexedTexGenProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectSimpleNonIndexedTexGenProgram.createShader(nullptr, nullptr);
	}
	

	if (success)
	{
		gObjectSimpleNonIndexedWaterProgram.mName = "Non indexed Water Shader";
		gObjectSimpleNonIndexedWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectSimpleNonIndexedWaterProgram.mShaderFiles.clear();
		gObjectSimpleNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleNonIndexedWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectSimpleNonIndexedWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectSimpleNonIndexedWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectSimpleNonIndexedTexGenWaterProgram.mName = "Non indexed tex-gen Water Shader";
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderFiles.clear();
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleTexGenV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectSimpleNonIndexedTexGenWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectSimpleNonIndexedTexGenWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectAlphaMaskNonIndexedProgram.mName = "Non indexed alpha mask Shader";
		gObjectAlphaMaskNonIndexedProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNonIndexedProgram.mFeatures.hasAlphaMask = true;
		gObjectAlphaMaskNonIndexedProgram.mShaderFiles.clear();
		gObjectAlphaMaskNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/simpleNonIndexedV.glsl", GL_VERTEX_SHADER));
		gObjectAlphaMaskNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
		gObjectAlphaMaskNonIndexedProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectAlphaMaskNonIndexedProgram.createShader(nullptr, nullptr);
	}
	
	if (success)
	{
		gObjectAlphaMaskNonIndexedWaterProgram.mName = "Non indexed alpha mask Water Shader";
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mFeatures.hasAlphaMask = true;
		gObjectAlphaMaskNonIndexedWaterProgram.mShaderFiles.clear();
		gObjectAlphaMaskNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleNonIndexedV.glsl", GL_VERTEX_SHADER));
		gObjectAlphaMaskNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectAlphaMaskNonIndexedWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectAlphaMaskNonIndexedWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectAlphaMaskNonIndexedWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectAlphaMaskNoColorProgram.mName = "No color alpha mask Shader";
		gObjectAlphaMaskNoColorProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasGamma = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNoColorProgram.mFeatures.hasAlphaMask = true;
		gObjectAlphaMaskNoColorProgram.mShaderFiles.clear();
		gObjectAlphaMaskNoColorProgram.mShaderFiles.push_back(std::make_pair("objects/simpleNoColorV.glsl", GL_VERTEX_SHADER));
		gObjectAlphaMaskNoColorProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
		gObjectAlphaMaskNoColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectAlphaMaskNoColorProgram.createShader(nullptr, nullptr);
	}
	
	if (success)
	{
		gObjectAlphaMaskNoColorWaterProgram.mName = "No color alpha mask Water Shader";
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.calculatesLighting = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasWaterFog = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasLighting = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectAlphaMaskNoColorWaterProgram.mFeatures.hasAlphaMask = true;
		gObjectAlphaMaskNoColorWaterProgram.mShaderFiles.clear();
		gObjectAlphaMaskNoColorWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleNoColorV.glsl", GL_VERTEX_SHADER));
		gObjectAlphaMaskNoColorWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectAlphaMaskNoColorWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectAlphaMaskNoColorWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectAlphaMaskNoColorWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gTreeProgram.mName = "Tree Shader";
		gTreeProgram.mFeatures.calculatesLighting = true;
		gTreeProgram.mFeatures.calculatesAtmospherics = true;
		gTreeProgram.mFeatures.hasGamma = true;
		gTreeProgram.mFeatures.hasAtmospherics = true;
		gTreeProgram.mFeatures.hasLighting = true;
		gTreeProgram.mFeatures.disableTextureIndex = true;
		gTreeProgram.mFeatures.hasAlphaMask = true;
		gTreeProgram.mShaderFiles.clear();
		gTreeProgram.mShaderFiles.push_back(std::make_pair("objects/treeV.glsl", GL_VERTEX_SHADER));
		gTreeProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
		gTreeProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gTreeProgram.createShader(nullptr, nullptr);
	}
	
	if (success)
	{
		gTreeWaterProgram.mName = "Tree Water Shader";
		gTreeWaterProgram.mFeatures.calculatesLighting = true;
		gTreeWaterProgram.mFeatures.calculatesAtmospherics = true;
		gTreeWaterProgram.mFeatures.hasWaterFog = true;
		gTreeWaterProgram.mFeatures.hasAtmospherics = true;
		gTreeWaterProgram.mFeatures.hasLighting = true;
		gTreeWaterProgram.mFeatures.disableTextureIndex = true;
		gTreeWaterProgram.mFeatures.hasAlphaMask = true;
		gTreeWaterProgram.mShaderFiles.clear();
		gTreeWaterProgram.mShaderFiles.push_back(std::make_pair("objects/treeV.glsl", GL_VERTEX_SHADER));
		gTreeWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
		gTreeWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gTreeWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gTreeWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightNonIndexedProgram.mName = "Non Indexed Fullbright Shader";
		gObjectFullbrightNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectFullbrightNonIndexedProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNonIndexedProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNonIndexedProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightNonIndexedProgram.mShaderFiles.clear();
		gObjectFullbrightNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightNonIndexedProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightNonIndexedProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightNonIndexedWaterProgram.mName = "Non Indexed Fullbright Water Shader";
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.hasWaterFog = true;		
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightNonIndexedWaterProgram.mShaderFiles.clear();
		gObjectFullbrightNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightNonIndexedWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightNonIndexedWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightNonIndexedWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectEmissiveNonIndexedProgram.mName = "Non Indexed Emissive Shader";
		gObjectEmissiveNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectEmissiveNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectEmissiveNonIndexedProgram.mFeatures.hasTransport = true;
		gObjectEmissiveNonIndexedProgram.mFeatures.isFullbright = true;
		gObjectEmissiveNonIndexedProgram.mFeatures.disableTextureIndex = true;
		gObjectEmissiveNonIndexedProgram.mShaderFiles.clear();
		gObjectEmissiveNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/emissiveV.glsl", GL_VERTEX_SHADER));
		gObjectEmissiveNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gObjectEmissiveNonIndexedProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectEmissiveNonIndexedProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectEmissiveNonIndexedWaterProgram.mName = "Non Indexed Emissive Water Shader";
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.isFullbright = true;
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.hasWaterFog = true;		
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.hasTransport = true;
		gObjectEmissiveNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectEmissiveNonIndexedWaterProgram.mShaderFiles.clear();
		gObjectEmissiveNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/emissiveV.glsl", GL_VERTEX_SHADER));
		gObjectEmissiveNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectEmissiveNonIndexedWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectEmissiveNonIndexedWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectEmissiveNonIndexedWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightNoColorProgram.mName = "Non Indexed no color Fullbright Shader";
		gObjectFullbrightNoColorProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNoColorProgram.mFeatures.hasGamma = true;
		gObjectFullbrightNoColorProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNoColorProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNoColorProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightNoColorProgram.mShaderFiles.clear();
		gObjectFullbrightNoColorProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightNoColorV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightNoColorProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightNoColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightNoColorProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightNoColorWaterProgram.mName = "Non Indexed no color Fullbright Water Shader";
		gObjectFullbrightNoColorWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.hasWaterFog = true;		
		gObjectFullbrightNoColorWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightNoColorWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightNoColorWaterProgram.mShaderFiles.clear();
		gObjectFullbrightNoColorWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightNoColorV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightNoColorWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightNoColorWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightNoColorWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightNoColorWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectShinyNonIndexedProgram.mName = "Non Indexed Shiny Shader";
		gObjectShinyNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyNonIndexedProgram.mFeatures.calculatesLighting = true;
		gObjectShinyNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectShinyNonIndexedProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyNonIndexedProgram.mFeatures.isShiny = true;
		gObjectShinyNonIndexedProgram.mFeatures.disableTextureIndex = true;
		gObjectShinyNonIndexedProgram.mShaderFiles.clear();
		gObjectShinyNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/shinyV.glsl", GL_VERTEX_SHADER));
		gObjectShinyNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/shinyF.glsl", GL_FRAGMENT_SHADER));		
		gObjectShinyNonIndexedProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectShinyNonIndexedProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectShinyNonIndexedWaterProgram.mName = "Non Indexed Shiny Water Shader";
		gObjectShinyNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.calculatesLighting = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.isShiny = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectShinyNonIndexedWaterProgram.mShaderFiles.clear();
		gObjectShinyNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/shinyWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectShinyNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/shinyV.glsl", GL_VERTEX_SHADER));
		gObjectShinyNonIndexedWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectShinyNonIndexedWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectShinyNonIndexedWaterProgram.createShader(nullptr, nullptr);
	}
	
	if (success)
	{
		gObjectFullbrightShinyNonIndexedProgram.mName = "Non Indexed Fullbright Shiny Shader";
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyNonIndexedProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightShinyNonIndexedProgram.mShaderFiles.clear();
		gObjectFullbrightShinyNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightShinyNonIndexedProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightShinyNonIndexedProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightShinyNonIndexedProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightShinyNonIndexedWaterProgram.mName = "Non Indexed Fullbright Shiny Water Shader";
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mFeatures.disableTextureIndex = true;
		gObjectFullbrightShinyNonIndexedWaterProgram.mShaderFiles.clear();
		gObjectFullbrightShinyNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightShinyNonIndexedWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightShinyNonIndexedWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightShinyNonIndexedWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightShinyNonIndexedWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gImpostorProgram.mName = "Impostor Shader";
		gImpostorProgram.mFeatures.disableTextureIndex = true;
		gImpostorProgram.mShaderFiles.clear();
		gImpostorProgram.mShaderFiles.push_back(std::make_pair("objects/impostorV.glsl", GL_VERTEX_SHADER));
		gImpostorProgram.mShaderFiles.push_back(std::make_pair("objects/impostorF.glsl", GL_FRAGMENT_SHADER));
		gImpostorProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gImpostorProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectPreviewProgram.mName = "Preview Shader";
		gObjectPreviewProgram.mFeatures.calculatesLighting = false;
		gObjectPreviewProgram.mFeatures.calculatesAtmospherics = false;
		gObjectPreviewProgram.mFeatures.hasGamma = false;
		gObjectPreviewProgram.mFeatures.hasAtmospherics = false;
		gObjectPreviewProgram.mFeatures.hasLighting = false;
		gObjectPreviewProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectPreviewProgram.mFeatures.disableTextureIndex = true;
		gObjectPreviewProgram.mShaderFiles.clear();
		gObjectPreviewProgram.mShaderFiles.push_back(std::make_pair("objects/previewV.glsl", GL_VERTEX_SHADER));
		gObjectPreviewProgram.mShaderFiles.push_back(std::make_pair("objects/previewF.glsl", GL_FRAGMENT_SHADER));
		gObjectPreviewProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectPreviewProgram.createShader(nullptr, nullptr);
		gObjectPreviewProgram.mFeatures.hasLighting = true;
	}

	if (success)
	{
		gObjectSimpleProgram.mName = "Simple Shader";
		gObjectSimpleProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleProgram.mFeatures.hasGamma = true;
		gObjectSimpleProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleProgram.mFeatures.hasLighting = true;
		gObjectSimpleProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectSimpleProgram.mShaderFiles.clear();
		gObjectSimpleProgram.mShaderFiles.push_back(std::make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectSimpleProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectSimpleImpostorProgram.mName = "Simple Impostor Shader";
		gObjectSimpleImpostorProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleImpostorProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleImpostorProgram.mFeatures.hasGamma = true;
		gObjectSimpleImpostorProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleImpostorProgram.mFeatures.hasLighting = true;
		gObjectSimpleImpostorProgram.mFeatures.mIndexedTextureChannels = 0;
		// force alpha mask version of lighting so we can weed out
		// transparent pixels from impostor temp buffer
		//
		gObjectSimpleImpostorProgram.mFeatures.hasAlphaMask = true; 
		gObjectSimpleImpostorProgram.mShaderFiles.clear();
		gObjectSimpleImpostorProgram.mShaderFiles.push_back(std::make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleImpostorProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleImpostorProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		
		success = gObjectSimpleImpostorProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectSimpleWaterProgram.mName = "Simple Water Shader";
		gObjectSimpleWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectSimpleWaterProgram.mShaderFiles.clear();
		gObjectSimpleWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectSimpleWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectSimpleWaterProgram.createShader(nullptr, nullptr);
	}
	
	if (success)
	{
		gObjectBumpProgram.mName = "Bump Shader";
		/*gObjectBumpProgram.mFeatures.calculatesLighting = true;
		gObjectBumpProgram.mFeatures.calculatesAtmospherics = true;
		gObjectBumpProgram.mFeatures.hasGamma = true;
		gObjectBumpProgram.mFeatures.hasAtmospherics = true;
		gObjectBumpProgram.mFeatures.hasLighting = true;
		gObjectBumpProgram.mFeatures.mIndexedTextureChannels = 0;*/
		gObjectBumpProgram.mShaderFiles.clear();
		gObjectBumpProgram.mShaderFiles.push_back(std::make_pair("objects/bumpV.glsl", GL_VERTEX_SHADER));
		gObjectBumpProgram.mShaderFiles.push_back(std::make_pair("objects/bumpF.glsl", GL_FRAGMENT_SHADER));
		gObjectBumpProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectBumpProgram.createShader(nullptr, nullptr);
		if (success)
		{ //lldrawpoolbump assumes "texture0" has channel 0 and "texture1" has channel 1
			gObjectBumpProgram.bind();
			gObjectBumpProgram.uniform1i(sTexture0, 0);
			gObjectBumpProgram.uniform1i(sTexture1, 1);
			gObjectBumpProgram.unbind();
		}
	}
	
	
	if (success)
	{
		gObjectSimpleAlphaMaskProgram.mName = "Simple Alpha Mask Shader";
		gObjectSimpleAlphaMaskProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.hasGamma = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.hasLighting = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.hasAlphaMask = true;
		gObjectSimpleAlphaMaskProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectSimpleAlphaMaskProgram.mShaderFiles.clear();
		gObjectSimpleAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectSimpleAlphaMaskProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectSimpleWaterAlphaMaskProgram.mName = "Simple Water Alpha Mask Shader";
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.hasLighting = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.hasAlphaMask = true;
		gObjectSimpleWaterAlphaMaskProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectSimpleWaterAlphaMaskProgram.mShaderFiles.clear();
		gObjectSimpleWaterAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER));
		gObjectSimpleWaterAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectSimpleWaterAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectSimpleWaterAlphaMaskProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectSimpleWaterAlphaMaskProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightProgram.mName = "Fullbright Shader";
		gObjectFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightProgram.mFeatures.hasGamma = true;
		gObjectFullbrightProgram.mFeatures.hasTransport = true;
		gObjectFullbrightProgram.mFeatures.isFullbright = true;
		gObjectFullbrightProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightProgram.mShaderFiles.clear();
		gObjectFullbrightProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightWaterProgram.mName = "Fullbright Water Shader";
		gObjectFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightWaterProgram.mFeatures.hasWaterFog = true;		
		gObjectFullbrightWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightWaterProgram.mShaderFiles.clear();
		gObjectFullbrightWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectEmissiveProgram.mName = "Emissive Shader";
		gObjectEmissiveProgram.mFeatures.calculatesAtmospherics = true;
		gObjectEmissiveProgram.mFeatures.hasGamma = true;
		gObjectEmissiveProgram.mFeatures.hasTransport = true;
		gObjectEmissiveProgram.mFeatures.isFullbright = true;
		gObjectEmissiveProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectEmissiveProgram.mShaderFiles.clear();
		gObjectEmissiveProgram.mShaderFiles.push_back(std::make_pair("objects/emissiveV.glsl", GL_VERTEX_SHADER));
		gObjectEmissiveProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gObjectEmissiveProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectEmissiveProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectEmissiveWaterProgram.mName = "Emissive Water Shader";
		gObjectEmissiveWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectEmissiveWaterProgram.mFeatures.isFullbright = true;
		gObjectEmissiveWaterProgram.mFeatures.hasWaterFog = true;		
		gObjectEmissiveWaterProgram.mFeatures.hasTransport = true;
		gObjectEmissiveWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectEmissiveWaterProgram.mShaderFiles.clear();
		gObjectEmissiveWaterProgram.mShaderFiles.push_back(std::make_pair("objects/emissiveV.glsl", GL_VERTEX_SHADER));
		gObjectEmissiveWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectEmissiveWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectEmissiveWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectEmissiveWaterProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightAlphaMaskProgram.mName = "Fullbright Alpha Mask Shader";
		gObjectFullbrightAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.hasGamma = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.hasTransport = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.isFullbright = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.hasAlphaMask = true;
		gObjectFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightAlphaMaskProgram.mShaderFiles.clear();
		gObjectFullbrightAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightAlphaMaskProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightWaterAlphaMaskProgram.mName = "Fullbright Water Alpha Mask Shader";
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.isFullbright = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.hasWaterFog = true;		
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.hasTransport = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.hasAlphaMask = true;
		gObjectFullbrightWaterAlphaMaskProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightWaterAlphaMaskProgram.mShaderFiles.clear();
		gObjectFullbrightWaterAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightWaterAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightWaterAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightWaterAlphaMaskProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightWaterAlphaMaskProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectShinyProgram.mName = "Shiny Shader";
		gObjectShinyProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyProgram.mFeatures.calculatesLighting = true;
		gObjectShinyProgram.mFeatures.hasGamma = true;
		gObjectShinyProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyProgram.mFeatures.isShiny = true;
		gObjectShinyProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectShinyProgram.mShaderFiles.clear();
		gObjectShinyProgram.mShaderFiles.push_back(std::make_pair("objects/shinyV.glsl", GL_VERTEX_SHADER));
		gObjectShinyProgram.mShaderFiles.push_back(std::make_pair("objects/shinyF.glsl", GL_FRAGMENT_SHADER));		
		gObjectShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectShinyProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectShinyWaterProgram.mName = "Shiny Water Shader";
		gObjectShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyWaterProgram.mFeatures.calculatesLighting = true;
		gObjectShinyWaterProgram.mFeatures.isShiny = true;
		gObjectShinyWaterProgram.mFeatures.hasWaterFog = true;
		gObjectShinyWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectShinyWaterProgram.mShaderFiles.clear();
		gObjectShinyWaterProgram.mShaderFiles.push_back(std::make_pair("objects/shinyWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectShinyWaterProgram.mShaderFiles.push_back(std::make_pair("objects/shinyV.glsl", GL_VERTEX_SHADER));
		gObjectShinyWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectShinyWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectShinyWaterProgram.createShader(nullptr, nullptr);
	}
	
	if (success)
	{
		gObjectFullbrightShinyProgram.mName = "Fullbright Shiny Shader";
		gObjectFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightShinyProgram.mShaderFiles.clear();
		gObjectFullbrightShinyProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightShinyProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightShinyProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gObjectFullbrightShinyWaterProgram.mName = "Fullbright Shiny Water Shader";
		gObjectFullbrightShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.mIndexedTextureChannels = 0;
		gObjectFullbrightShinyWaterProgram.mShaderFiles.clear();
		gObjectFullbrightShinyWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyV.glsl", GL_VERTEX_SHADER));
		gObjectFullbrightShinyWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyWaterF.glsl", GL_FRAGMENT_SHADER));
		gObjectFullbrightShinyWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightShinyWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightShinyWaterProgram.createShader(nullptr, nullptr);
	}

	if (mVertexShaderLevel[SHADER_AVATAR] > 0)
	{ //load hardware skinned attachment shaders
		if (success)
		{
			gSkinnedObjectSimpleProgram.mName = "Skinned Simple Shader";
			gSkinnedObjectSimpleProgram.mFeatures.calculatesLighting = true;
			gSkinnedObjectSimpleProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasGamma = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasAtmospherics = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasLighting = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectSimpleProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectSimpleProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectSimpleProgram.mShaderFiles.clear();
			gSkinnedObjectSimpleProgram.mShaderFiles.push_back(std::make_pair("objects/simpleSkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectSimpleProgram.mShaderFiles.push_back(std::make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectSimpleProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectSimpleProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectFullbrightProgram.mName = "Skinned Fullbright Shader";
			gSkinnedObjectFullbrightProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectFullbrightProgram.mFeatures.hasGamma = true;
			gSkinnedObjectFullbrightProgram.mFeatures.hasTransport = true;
			gSkinnedObjectFullbrightProgram.mFeatures.isFullbright = true;
			gSkinnedObjectFullbrightProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectFullbrightProgram.mFeatures.hasAlphaMask = true;			
			gSkinnedObjectFullbrightProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectFullbrightProgram.mShaderFiles.clear();
			gSkinnedObjectFullbrightProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightSkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectFullbrightProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectFullbrightProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectEmissiveProgram.mName = "Skinned Emissive Shader";
			gSkinnedObjectEmissiveProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectEmissiveProgram.mFeatures.hasGamma = true;
			gSkinnedObjectEmissiveProgram.mFeatures.hasTransport = true;
			gSkinnedObjectEmissiveProgram.mFeatures.isFullbright = true;
			gSkinnedObjectEmissiveProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectEmissiveProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectEmissiveProgram.mShaderFiles.clear();
			gSkinnedObjectEmissiveProgram.mShaderFiles.push_back(std::make_pair("objects/emissiveSkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectEmissiveProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectEmissiveProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectEmissiveProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectEmissiveWaterProgram.mName = "Skinned Emissive Water Shader";
			gSkinnedObjectEmissiveWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.hasTransport = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.isFullbright = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectEmissiveWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectEmissiveWaterProgram.mShaderFiles.clear();
			gSkinnedObjectEmissiveWaterProgram.mShaderFiles.push_back(std::make_pair("objects/emissiveSkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectEmissiveWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectEmissiveWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectEmissiveWaterProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectFullbrightShinyProgram.mName = "Skinned Fullbright Shiny Shader";
			gSkinnedObjectFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.hasGamma = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.hasTransport = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.isShiny = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.isFullbright = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectFullbrightShinyProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectFullbrightShinyProgram.mShaderFiles.clear();
			gSkinnedObjectFullbrightShinyProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinySkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectFullbrightShinyProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectFullbrightShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectFullbrightShinyProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectShinySimpleProgram.mName = "Skinned Shiny Simple Shader";
			gSkinnedObjectShinySimpleProgram.mFeatures.calculatesLighting = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.hasGamma = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.hasAtmospherics = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.isShiny = true;
			gSkinnedObjectShinySimpleProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectShinySimpleProgram.mShaderFiles.clear();
			gSkinnedObjectShinySimpleProgram.mShaderFiles.push_back(std::make_pair("objects/shinySimpleSkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectShinySimpleProgram.mShaderFiles.push_back(std::make_pair("objects/shinyF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectShinySimpleProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectShinySimpleProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectSimpleWaterProgram.mName = "Skinned Simple Water Shader";
			gSkinnedObjectSimpleWaterProgram.mFeatures.calculatesLighting = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasAtmospherics = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasLighting = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectSimpleWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
			gSkinnedObjectSimpleWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectSimpleWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectSimpleWaterProgram.mShaderFiles.clear();
			gSkinnedObjectSimpleWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleSkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectSimpleWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectSimpleWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectSimpleWaterProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectFullbrightWaterProgram.mName = "Skinned Fullbright Water Shader";
			gSkinnedObjectFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasTransport = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.isFullbright = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectFullbrightWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectFullbrightWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
			gSkinnedObjectFullbrightWaterProgram.mShaderFiles.clear();
			gSkinnedObjectFullbrightWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightSkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectFullbrightWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectFullbrightWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectFullbrightWaterProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectFullbrightShinyWaterProgram.mName = "Skinned Fullbright Shiny Water Shader";
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasTransport = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.isShiny = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.isFullbright = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectFullbrightShinyWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
			gSkinnedObjectFullbrightShinyWaterProgram.mShaderFiles.clear();
			gSkinnedObjectFullbrightShinyWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinySkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectFullbrightShinyWaterProgram.mShaderFiles.push_back(std::make_pair("objects/fullbrightShinyWaterF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectFullbrightShinyWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectFullbrightShinyWaterProgram.createShader(nullptr, nullptr);
		}

		if (success)
		{
			gSkinnedObjectShinySimpleWaterProgram.mName = "Skinned Shiny Simple Water Shader";
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.calculatesLighting = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.calculatesAtmospherics = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasGamma = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasAtmospherics = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasObjectSkinning = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasAlphaMask = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.isShiny = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.hasWaterFog = true;
			gSkinnedObjectShinySimpleWaterProgram.mFeatures.disableTextureIndex = true;
			gSkinnedObjectShinySimpleWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
			gSkinnedObjectShinySimpleWaterProgram.mShaderFiles.clear();
			gSkinnedObjectShinySimpleWaterProgram.mShaderFiles.push_back(std::make_pair("objects/shinySimpleSkinnedV.glsl", GL_VERTEX_SHADER));
			gSkinnedObjectShinySimpleWaterProgram.mShaderFiles.push_back(std::make_pair("objects/shinyWaterF.glsl", GL_FRAGMENT_SHADER));
			gSkinnedObjectShinySimpleWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
			success = gSkinnedObjectShinySimpleWaterProgram.createShader(nullptr, nullptr);
		}
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_OBJECT] = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersAvatar()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_AVATAR] == 0)
	{
		gAvatarProgram.unload();
		gAvatarWaterProgram.unload();
		gAvatarPickProgram.unload();
		return TRUE;
	}

	if (success)
	{
		gAvatarProgram.mName = "Avatar Shader";
		gAvatarProgram.mFeatures.hasSkinning = true;
		gAvatarProgram.mFeatures.calculatesAtmospherics = true;
		gAvatarProgram.mFeatures.calculatesLighting = true;
		gAvatarProgram.mFeatures.hasGamma = true;
		gAvatarProgram.mFeatures.hasAtmospherics = true;
		gAvatarProgram.mFeatures.hasLighting = true;
		gAvatarProgram.mFeatures.hasAlphaMask = true;
		gAvatarProgram.mFeatures.disableTextureIndex = true;
		gAvatarProgram.mShaderFiles.clear();
		gAvatarProgram.mShaderFiles.push_back(std::make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER));
		gAvatarProgram.mShaderFiles.push_back(std::make_pair("avatar/avatarF.glsl", GL_FRAGMENT_SHADER));
		gAvatarProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarProgram.createShader(nullptr, nullptr);
			
		if (success)
		{
			gAvatarWaterProgram.mName = "Avatar Water Shader";
			gAvatarWaterProgram.mFeatures.hasSkinning = true;
			gAvatarWaterProgram.mFeatures.calculatesAtmospherics = true;
			gAvatarWaterProgram.mFeatures.calculatesLighting = true;
			gAvatarWaterProgram.mFeatures.hasWaterFog = true;
			gAvatarWaterProgram.mFeatures.hasAtmospherics = true;
			gAvatarWaterProgram.mFeatures.hasLighting = true;
			gAvatarWaterProgram.mFeatures.hasAlphaMask = true;
			gAvatarWaterProgram.mFeatures.disableTextureIndex = true;
			gAvatarWaterProgram.mShaderFiles.clear();
			gAvatarWaterProgram.mShaderFiles.push_back(std::make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER));
			gAvatarWaterProgram.mShaderFiles.push_back(std::make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER));
			// Note: no cloth under water:
			gAvatarWaterProgram.mShaderLevel = llmin(mVertexShaderLevel[SHADER_AVATAR], 1);	
			gAvatarWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;				
			success = gAvatarWaterProgram.createShader(nullptr, nullptr);
		}

		/// Keep track of avatar levels
		if (gAvatarProgram.mShaderLevel != mVertexShaderLevel[SHADER_AVATAR])
		{
			mMaxAvatarShaderLevel = mVertexShaderLevel[SHADER_AVATAR] = gAvatarProgram.mShaderLevel;
		}
	}

	if (success)
	{
		gAvatarPickProgram.mName = "Avatar Pick Shader";
		gAvatarPickProgram.mFeatures.hasSkinning = true;
		gAvatarPickProgram.mFeatures.disableTextureIndex = true;
		gAvatarPickProgram.mShaderFiles.clear();
		gAvatarPickProgram.mShaderFiles.push_back(std::make_pair("avatar/pickAvatarV.glsl", GL_VERTEX_SHADER));
		gAvatarPickProgram.mShaderFiles.push_back(std::make_pair("avatar/pickAvatarF.glsl", GL_FRAGMENT_SHADER));
		gAvatarPickProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarPickProgram.createShader(nullptr, nullptr);
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_AVATAR] = 0;
		mMaxAvatarShaderLevel = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersInterface()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_INTERFACE] == 0)
	{
		gHighlightProgram.unload();
		return TRUE;
	}
	
	if (success)
	{
		gHighlightProgram.mName = "Highlight Shader";
		gHighlightProgram.mShaderFiles.clear();
		gHighlightProgram.mShaderFiles.push_back(std::make_pair("interface/highlightV.glsl", GL_VERTEX_SHADER));
		gHighlightProgram.mShaderFiles.push_back(std::make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER));
		gHighlightProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];		
		success = gHighlightProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gHighlightNormalProgram.mName = "Highlight Normals Shader";
		gHighlightNormalProgram.mShaderFiles.clear();
		gHighlightNormalProgram.mShaderFiles.push_back(std::make_pair("interface/highlightNormV.glsl", GL_VERTEX_SHADER));
		gHighlightNormalProgram.mShaderFiles.push_back(std::make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER));
		gHighlightNormalProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];		
		success = gHighlightNormalProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gHighlightSpecularProgram.mName = "Highlight Spec Shader";
		gHighlightSpecularProgram.mShaderFiles.clear();
		gHighlightSpecularProgram.mShaderFiles.push_back(std::make_pair("interface/highlightSpecV.glsl", GL_VERTEX_SHADER));
		gHighlightSpecularProgram.mShaderFiles.push_back(std::make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER));
		gHighlightSpecularProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];		
		success = gHighlightSpecularProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gUIProgram.mName = "UI Shader";
		gUIProgram.mShaderFiles.clear();
		gUIProgram.mShaderFiles.push_back(std::make_pair("interface/uiV.glsl", GL_VERTEX_SHADER));
		gUIProgram.mShaderFiles.push_back(std::make_pair("interface/uiF.glsl", GL_FRAGMENT_SHADER));
		gUIProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gUIProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gCustomAlphaProgram.mName = "Custom Alpha Shader";
		gCustomAlphaProgram.mShaderFiles.clear();
		gCustomAlphaProgram.mShaderFiles.push_back(std::make_pair("interface/customalphaV.glsl", GL_VERTEX_SHADER));
		gCustomAlphaProgram.mShaderFiles.push_back(std::make_pair("interface/customalphaF.glsl", GL_FRAGMENT_SHADER));
		gCustomAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gCustomAlphaProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gSplatTextureRectProgram.mName = "Splat Texture Rect Shader";
		gSplatTextureRectProgram.mShaderFiles.clear();
		gSplatTextureRectProgram.mShaderFiles.push_back(std::make_pair("interface/splattexturerectV.glsl", GL_VERTEX_SHADER));
		gSplatTextureRectProgram.mShaderFiles.push_back(std::make_pair("interface/splattexturerectF.glsl", GL_FRAGMENT_SHADER));
		gSplatTextureRectProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gSplatTextureRectProgram.createShader(nullptr, nullptr);
		if (success)
		{
			gSplatTextureRectProgram.bind();
			gSplatTextureRectProgram.uniform1i(sScreenMap, 0);
			gSplatTextureRectProgram.unbind();
		}
	}

	if (success)
	{
		gGlowCombineProgram.mName = "Glow Combine Shader";
		gGlowCombineProgram.mShaderFiles.clear();
		gGlowCombineProgram.mShaderFiles.push_back(std::make_pair("interface/glowcombineV.glsl", GL_VERTEX_SHADER));
		gGlowCombineProgram.mShaderFiles.push_back(std::make_pair("interface/glowcombineF.glsl", GL_FRAGMENT_SHADER));
		gGlowCombineProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gGlowCombineProgram.createShader(nullptr, nullptr);
		if (success)
		{
			gGlowCombineProgram.bind();
			gGlowCombineProgram.uniform1i(sGlowMap, 0);
			gGlowCombineProgram.uniform1i(sScreenMap, 1);
			gGlowCombineProgram.unbind();
		}
	}

	if (success)
	{
		gGlowCombineFXAAProgram.mName = "Glow CombineFXAA Shader";
		gGlowCombineFXAAProgram.mShaderFiles.clear();
		gGlowCombineFXAAProgram.mShaderFiles.push_back(std::make_pair("interface/glowcombineFXAAV.glsl", GL_VERTEX_SHADER));
		gGlowCombineFXAAProgram.mShaderFiles.push_back(std::make_pair("interface/glowcombineFXAAF.glsl", GL_FRAGMENT_SHADER));
		gGlowCombineFXAAProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gGlowCombineFXAAProgram.createShader(nullptr, nullptr);
		if (success)
		{
			gGlowCombineFXAAProgram.bind();
			gGlowCombineFXAAProgram.uniform1i(sGlowMap, 0);
			gGlowCombineFXAAProgram.uniform1i(sScreenMap, 1);
			gGlowCombineFXAAProgram.unbind();
		}
	}


	if (success)
	{
		gTwoTextureAddProgram.mName = "Two Texture Add Shader";
		gTwoTextureAddProgram.mShaderFiles.clear();
		gTwoTextureAddProgram.mShaderFiles.push_back(std::make_pair("interface/twotextureaddV.glsl", GL_VERTEX_SHADER));
		gTwoTextureAddProgram.mShaderFiles.push_back(std::make_pair("interface/twotextureaddF.glsl", GL_FRAGMENT_SHADER));
		gTwoTextureAddProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gTwoTextureAddProgram.createShader(nullptr, nullptr);
		if (success)
		{
			gTwoTextureAddProgram.bind();
			gTwoTextureAddProgram.uniform1i(sTex0, 0);
			gTwoTextureAddProgram.uniform1i(sTex1, 1);
		}
	}

#ifdef LL_WINDOWS
	if (success)
	{
		gTwoTextureCompareProgram.mName = "Two Texture Compare Shader";
		gTwoTextureCompareProgram.mShaderFiles.clear();
		gTwoTextureCompareProgram.mShaderFiles.push_back(std::make_pair("interface/twotexturecompareV.glsl", GL_VERTEX_SHADER));
		gTwoTextureCompareProgram.mShaderFiles.push_back(std::make_pair("interface/twotexturecompareF.glsl", GL_FRAGMENT_SHADER));
		gTwoTextureCompareProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gTwoTextureCompareProgram.createShader(nullptr, nullptr);
		if (success)
		{
			gTwoTextureCompareProgram.bind();
			gTwoTextureCompareProgram.uniform1i(sTex0, 0);
			gTwoTextureCompareProgram.uniform1i(sTex1, 1);
			gTwoTextureCompareProgram.uniform1i(sDitherTex, 2);
		}
	}

	if (success)
	{
		gOneTextureFilterProgram.mName = "One Texture Filter Shader";
		gOneTextureFilterProgram.mShaderFiles.clear();
		gOneTextureFilterProgram.mShaderFiles.push_back(std::make_pair("interface/onetexturefilterV.glsl", GL_VERTEX_SHADER));
		gOneTextureFilterProgram.mShaderFiles.push_back(std::make_pair("interface/onetexturefilterF.glsl", GL_FRAGMENT_SHADER));
		gOneTextureFilterProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gOneTextureFilterProgram.createShader(nullptr, nullptr);
		if (success)
		{
			gOneTextureFilterProgram.bind();
			gOneTextureFilterProgram.uniform1i(sTex0, 0);
		}
	}
#endif

	if (success)
	{
		gOneTextureNoColorProgram.mName = "One Texture No Color Shader";
		gOneTextureNoColorProgram.mShaderFiles.clear();
		gOneTextureNoColorProgram.mShaderFiles.push_back(std::make_pair("interface/onetexturenocolorV.glsl", GL_VERTEX_SHADER));
		gOneTextureNoColorProgram.mShaderFiles.push_back(std::make_pair("interface/onetexturenocolorF.glsl", GL_FRAGMENT_SHADER));
		gOneTextureNoColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gOneTextureNoColorProgram.createShader(nullptr, nullptr);
		if (success)
		{
			gOneTextureNoColorProgram.bind();
			gOneTextureNoColorProgram.uniform1i(sTex0, 0);
		}
	}

	if (success)
	{
		gSolidColorProgram.mName = "Solid Color Shader";
		gSolidColorProgram.mShaderFiles.clear();
		gSolidColorProgram.mShaderFiles.push_back(std::make_pair("interface/solidcolorV.glsl", GL_VERTEX_SHADER));
		gSolidColorProgram.mShaderFiles.push_back(std::make_pair("interface/solidcolorF.glsl", GL_FRAGMENT_SHADER));
		gSolidColorProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gSolidColorProgram.createShader(nullptr, nullptr);
		if (success)
		{
			gSolidColorProgram.bind();
			gSolidColorProgram.uniform1i(sTex0, 0);
			gSolidColorProgram.unbind();
		}
	}

	if (success)
	{
		gOcclusionProgram.mName = "Occlusion Shader";
		gOcclusionProgram.mShaderFiles.clear();
		gOcclusionProgram.mShaderFiles.push_back(std::make_pair("interface/occlusionV.glsl", GL_VERTEX_SHADER));
		gOcclusionProgram.mShaderFiles.push_back(std::make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER));
		gOcclusionProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gOcclusionProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gOcclusionCubeProgram.mName = "Occlusion Cube Shader";
		gOcclusionCubeProgram.mShaderFiles.clear();
		gOcclusionCubeProgram.mShaderFiles.push_back(std::make_pair("interface/occlusionCubeV.glsl", GL_VERTEX_SHADER));
		gOcclusionCubeProgram.mShaderFiles.push_back(std::make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER));
		gOcclusionCubeProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gOcclusionCubeProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDebugProgram.mName = "Debug Shader";
		gDebugProgram.mShaderFiles.clear();
		gDebugProgram.mShaderFiles.push_back(std::make_pair("interface/debugV.glsl", GL_VERTEX_SHADER));
		gDebugProgram.mShaderFiles.push_back(std::make_pair("interface/debugF.glsl", GL_FRAGMENT_SHADER));
		gDebugProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gDebugProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gClipProgram.mName = "Clip Shader";
		gClipProgram.mShaderFiles.clear();
		gClipProgram.mShaderFiles.push_back(std::make_pair("interface/clipV.glsl", GL_VERTEX_SHADER));
		gClipProgram.mShaderFiles.push_back(std::make_pair("interface/clipF.glsl", GL_FRAGMENT_SHADER));
		gClipProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gClipProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gDownsampleDepthProgram.mName = "DownsampleDepth Shader";
		gDownsampleDepthProgram.mShaderFiles.clear();
		gDownsampleDepthProgram.mShaderFiles.push_back(std::make_pair("interface/downsampleDepthV.glsl", GL_VERTEX_SHADER));
		gDownsampleDepthProgram.mShaderFiles.push_back(std::make_pair("interface/downsampleDepthF.glsl", GL_FRAGMENT_SHADER));
		gDownsampleDepthProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gDownsampleDepthProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gBenchmarkProgram.mName = "Benchmark Shader";
		gBenchmarkProgram.mShaderFiles.clear();
		gBenchmarkProgram.mShaderFiles.push_back(std::make_pair("interface/benchmarkV.glsl", GL_VERTEX_SHADER));
		gBenchmarkProgram.mShaderFiles.push_back(std::make_pair("interface/benchmarkF.glsl", GL_FRAGMENT_SHADER));
		gBenchmarkProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gBenchmarkProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gAlphaMaskProgram.mName = "Alpha Mask Shader";
		gAlphaMaskProgram.mShaderFiles.clear();
		gAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("interface/alphamaskV.glsl", GL_VERTEX_SHADER));
		gAlphaMaskProgram.mShaderFiles.push_back(std::make_pair("interface/alphamaskF.glsl", GL_FRAGMENT_SHADER));
		gAlphaMaskProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];
		success = gAlphaMaskProgram.createShader(nullptr, nullptr);
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_INTERFACE] = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersWindLight()
{	
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_WINDLIGHT] < 2)
	{
		gWLSkyProgram.unload();
		gWLCloudProgram.unload();
		return TRUE;
	}

	if (success)
	{
		gWLSkyProgram.mName = "Windlight Sky Shader";
		//gWLSkyProgram.mFeatures.hasGamma = true;
		gWLSkyProgram.mShaderFiles.clear();
		gWLSkyProgram.mShaderFiles.push_back(std::make_pair("windlight/skyV.glsl", GL_VERTEX_SHADER));
		gWLSkyProgram.mShaderFiles.push_back(std::make_pair("windlight/skyF.glsl", GL_FRAGMENT_SHADER));
		gWLSkyProgram.mShaderLevel = mVertexShaderLevel[SHADER_WINDLIGHT];
		gWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLSkyProgram.createShader(nullptr, nullptr);
	}

	if (success)
	{
		gWLCloudProgram.mName = "Windlight Cloud Program";
		//gWLCloudProgram.mFeatures.hasGamma = true;
		gWLCloudProgram.mShaderFiles.clear();
		gWLCloudProgram.mShaderFiles.push_back(std::make_pair("windlight/cloudsV.glsl", GL_VERTEX_SHADER));
		gWLCloudProgram.mShaderFiles.push_back(std::make_pair("windlight/cloudsF.glsl", GL_FRAGMENT_SHADER));
		gWLCloudProgram.mShaderLevel = mVertexShaderLevel[SHADER_WINDLIGHT];
		gWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLCloudProgram.createShader(nullptr, nullptr);
	}

	return success;
}

std::string LLViewerShaderMgr::getShaderDirPrefix(void)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders/class");
}

void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)
{
	LLWLParamManager::getInstance()->updateShaderUniforms(shader);
	LLWaterParamManager::getInstance()->updateShaderUniforms(shader);
}

LLViewerShaderMgr::shader_iter LLViewerShaderMgr::beginShaders() const
{
	return mShaderList.begin();
}

LLViewerShaderMgr::shader_iter LLViewerShaderMgr::endShaders() const
{
	return mShaderList.end();
}

