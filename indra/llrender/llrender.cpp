// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
 /** 
 * @file llrender.cpp
 * @brief LLRender implementation
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

#include "linden_common.h"

#include "llrender.h"

#include "llvertexbuffer.h"
#include "llcubemap.h"
#include "llglslshader.h"
#include "llimagegl.h"
#include "llrendertarget.h"
#include "lltexture.h"
#include "llshadermgr.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

LLRender gGL;

// Handy copies of last good GL matrices
F32	gGLModelView[16];
F32	gGLLastModelView[16];
F32	gGLLastProjection[16];
F32	gGLProjection[16];
S32	gGLViewport[4];

U32 LLRender::sUICalls = 0;
U32 LLRender::sUIVerts = 0;
U32 LLTexUnit::sWhiteTexture = 0;
bool LLRender::sGLCoreProfile = false;

static const U32 LL_NUM_TEXTURE_LAYERS = 32; 
static const U32 LL_NUM_LIGHT_UNITS = 8;

static const GLenum sGLTextureType[] =
{
	GL_TEXTURE_2D,
	GL_TEXTURE_CUBE_MAP_ARB
};

static const GLint sGLAddressMode[] =
{	
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE
};

static const GLenum sGLCompareFunc[] =
{
	GL_NEVER,
	GL_ALWAYS,
	GL_LESS,
	GL_LEQUAL,
	GL_EQUAL,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_GREATER
};

const U32 immediate_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXCOORD0;

static const GLenum sGLBlendFactor[] =
{
	GL_ONE,
	GL_ZERO,
	GL_DST_COLOR,
	GL_SRC_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_DST_ALPHA,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,

	GL_ZERO // 'BF_UNDEF'
};

LLTexUnit::LLTexUnit(S32 index)
	: mIndex(index), mCurrTexture(0), 
	mCurrTexType(TT_NONE), mCurrBlendType(TB_MULT),
	mCurrColorOp(TBO_MULT), mCurrColorSrc1(TBS_TEX_COLOR),
	mCurrColorSrc2(TBS_PREV_COLOR), mCurrAlphaOp(TBO_MULT),
	mCurrAlphaSrc1(TBS_TEX_ALPHA), mCurrAlphaSrc2(TBS_PREV_ALPHA), mCurrColorScale(1),
	mCurrAlphaScale(1),
	mHasMipMaps(false)
{
	llassert_always(index < (S32)LL_NUM_TEXTURE_LAYERS);
}

//static
U32 LLTexUnit::getInternalType(eTextureType type)
{
	return sGLTextureType[type];
}

void LLTexUnit::refreshState(void)
{
	// We set dirty to true so that the tex unit knows to ignore caching
	// and we reset the cached tex unit state

	gGL.flush();
	
	glActiveTextureARB(GL_TEXTURE0_ARB + mIndex);

	//
	// Per apple spec, don't call glEnable/glDisable when index exceeds max texture units
	// http://www.mailinglistarchive.com/html/mac-opengl@lists.apple.com/2008-07/msg00653.html
	//
	bool enableDisable = !LLGLSLShader::sNoFixedFunction && 
		(mIndex < gGLManager.mNumTextureUnits);
		
	if (mCurrTexType != TT_NONE)
	{
		if (enableDisable)
		{
			glEnable(sGLTextureType[mCurrTexType]);
		}
		
		glBindTexture(sGLTextureType[mCurrTexType], mCurrTexture);
	}
	else
	{
		if (enableDisable)
		{
			glDisable(GL_TEXTURE_2D);
		}
		
		glBindTexture(GL_TEXTURE_2D, 0);	
	}

	if (mCurrBlendType != TB_COMBINE)
	{
		setTextureBlendType(mCurrBlendType);
	}
	else
	{
		setTextureCombiner(mCurrColorOp, mCurrColorSrc1, mCurrColorSrc2, false);
		setTextureCombiner(mCurrAlphaOp, mCurrAlphaSrc1, mCurrAlphaSrc2, true);
	}
}

void LLTexUnit::activate(void)
{
	if (mIndex < 0) return;

	if ((S32)gGL.mCurrTextureUnitIndex != mIndex || gGL.mDirty)
	{
		gGL.flush();
		glActiveTextureARB(GL_TEXTURE0_ARB + mIndex);
		gGL.mCurrTextureUnitIndex = mIndex;
	}
}

void LLTexUnit::enable(eTextureType type)
{
	if (mIndex < 0) return;

	if ( (mCurrTexType != type || gGL.mDirty) && (type != TT_NONE) )
	{
		stop_glerror();
		activate();
		stop_glerror();
		if (mCurrTexType != TT_NONE && !gGL.mDirty)
		{
			disable(); // Force a disable of a previous texture type if it's enabled.
			stop_glerror();
		}
		mCurrTexType = type;

		gGL.flush();
		if (!LLGLSLShader::sNoFixedFunction && 
			mIndex < gGLManager.mNumTextureUnits)
		{
			stop_glerror();
			glEnable(sGLTextureType[type]);
			stop_glerror();
		}
	}
}

void LLTexUnit::disable(void)
{
	if (mIndex < 0) return;

	if (mCurrTexType != TT_NONE)
	{
		activate();
		unbind(mCurrTexType);
		gGL.flush();
		if (!LLGLSLShader::sNoFixedFunction &&
			mIndex < gGLManager.mNumTextureUnits)
		{
			glDisable(sGLTextureType[mCurrTexType]);
		}
		
		mCurrTexType = TT_NONE;
	}
}

bool LLTexUnit::bind(LLTexture* texture, bool for_rendering, bool forceBind)
{
	stop_glerror();
	if (mIndex >= 0)
	{
		gGL.flush();

		LLImageGL* gl_tex = nullptr ;

		if (texture != nullptr && (gl_tex = texture->getGLTexture()))
		{
			if (gl_tex->getTexName()) //if texture exists
			{
				//in audit, replace the selected texture by the default one.
				if ((mCurrTexture != gl_tex->getTexName()) || forceBind)
				{
					activate();
					enable(gl_tex->getTarget());
					mCurrTexture = gl_tex->getTexName();
					glBindTexture(sGLTextureType[gl_tex->getTarget()], mCurrTexture);
					if(gl_tex->updateBindStats(gl_tex->mTextureMemory))
					{
						texture->setActive() ;
						texture->updateBindStatsForTester() ;
					}
					mHasMipMaps = gl_tex->mHasMipMaps;
					if (gl_tex->mTexOptionsDirty)
					{
						gl_tex->mTexOptionsDirty = false;
						setTextureAddressMode(gl_tex->mAddressMode);
						setTextureFilteringOption(gl_tex->mFilterOption);
					}
				}
			}
			else
			{
				//if deleted, will re-generate it immediately
				texture->forceImmediateUpdate() ;

				gl_tex->forceUpdateBindStats() ;
				return texture->bindDefaultImage(mIndex);
			}
		}
		else
		{
			if (texture)
			{
				LL_DEBUGS() << "NULL LLTexUnit::bind GL image" << LL_ENDL;
			}
			else
			{
				LL_DEBUGS() << "NULL LLTexUnit::bind texture" << LL_ENDL;
			}
			return false;
		}
	}
	else
	{ // mIndex < 0
		return false;
	}

	return true;
}

bool LLTexUnit::bind(LLImageGL* texture, bool for_rendering, bool forceBind)
{
	stop_glerror();
	if (mIndex < 0) return false;

	if(!texture)
	{
		LL_DEBUGS() << "NULL LLTexUnit::bind texture" << LL_ENDL;
		return false;
	}

	if(!texture->getTexName())
	{
		if(LLImageGL::sDefaultGLTexture && LLImageGL::sDefaultGLTexture->getTexName())
		{
			return bind(LLImageGL::sDefaultGLTexture) ;
		}
		stop_glerror();
		return false ;
	}

	if ((mCurrTexture != texture->getTexName()) || forceBind)
	{
		gGL.flush();
		stop_glerror();
		activate();
		stop_glerror();
		enable(texture->getTarget());
		stop_glerror();
		mCurrTexture = texture->getTexName();
		glBindTexture(sGLTextureType[texture->getTarget()], mCurrTexture);
		stop_glerror();
		texture->updateBindStats(texture->mTextureMemory);		
		mHasMipMaps = texture->mHasMipMaps;
		if (texture->mTexOptionsDirty)
		{
			stop_glerror();
			texture->mTexOptionsDirty = false;
			setTextureAddressMode(texture->mAddressMode);
			setTextureFilteringOption(texture->mFilterOption);
			stop_glerror();
		}
	}

	stop_glerror();

	return true;
}

bool LLTexUnit::bind(LLCubeMap* cubeMap)
{
	if (mIndex < 0) return false;

	gGL.flush();

	if (cubeMap == nullptr)
	{
		LL_WARNS() << "NULL LLTexUnit::bind cubemap" << LL_ENDL;
		return false;
	}

	if (mCurrTexture != cubeMap->mImages[0]->getTexName())
	{
		if (gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps)
		{
			activate();
			enable(LLTexUnit::TT_CUBE_MAP);
			mCurrTexture = cubeMap->mImages[0]->getTexName();
			glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, mCurrTexture);
			mHasMipMaps = cubeMap->mImages[0]->mHasMipMaps;
			cubeMap->mImages[0]->updateBindStats(cubeMap->mImages[0]->mTextureMemory);
			if (cubeMap->mImages[0]->mTexOptionsDirty)
			{
				cubeMap->mImages[0]->mTexOptionsDirty = false;
				setTextureAddressMode(cubeMap->mImages[0]->mAddressMode);
				setTextureFilteringOption(cubeMap->mImages[0]->mFilterOption);
			}
			return true;
		}
		else
		{
			LL_WARNS() << "Using cube map without extension!" << LL_ENDL;
			return false;
		}
	}
	return true;
}

// LLRenderTarget is unavailible on the mapserver since it uses FBOs.
bool LLTexUnit::bind(LLRenderTarget* renderTarget, bool bindDepth)
{
	if (mIndex < 0) return false;

	gGL.flush();

	if (bindDepth)
	{
		if (renderTarget->hasStencil())
		{
			LL_ERRS() << "Cannot bind a render buffer for sampling.  Allocate render target without a stencil buffer if sampling of depth buffer is required." << LL_ENDL;
		}

		bindManual(renderTarget->getUsage(), renderTarget->getDepth());
	}
	else
	{
		bindManual(renderTarget->getUsage(), renderTarget->getTexture());
	}

	return true;
}

bool LLTexUnit::bindManual(eTextureType type, U32 texture, bool hasMips)
{
	if (mIndex < 0)  
	{
		return false;
	}
	
	if(mCurrTexture != texture)
	{
		gGL.flush();
		
		activate();
		enable(type);
		mCurrTexture = texture;
		glBindTexture(sGLTextureType[type], texture);
		mHasMipMaps = hasMips;
	}
	return true;
}

void LLTexUnit::unbind(eTextureType type)
{
	stop_glerror();

	if (mIndex < 0) return;

	//always flush and activate for consistency 
	//   some code paths assume unbind always flushes and sets the active texture
	gGL.flush();
	activate();

	// Disabled caching of binding state.
	if (mCurrTexType == type)
	{
		mCurrTexture = 0;
		if (LLGLSLShader::sNoFixedFunction && type == LLTexUnit::TT_TEXTURE)
		{
			glBindTexture(sGLTextureType[type], sWhiteTexture);
		}
		else
		{
			glBindTexture(sGLTextureType[type], 0);
		}
		stop_glerror();
	}
}

void LLTexUnit::setTextureAddressMode(eTextureAddressMode mode)
{
	if (mIndex < 0 || mCurrTexture == 0) return;

	gGL.flush();

	activate();

	glTexParameteri (sGLTextureType[mCurrTexType], GL_TEXTURE_WRAP_S, sGLAddressMode[mode]);
	glTexParameteri (sGLTextureType[mCurrTexType], GL_TEXTURE_WRAP_T, sGLAddressMode[mode]);
	if (mCurrTexType == TT_CUBE_MAP)
	{
		glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R, sGLAddressMode[mode]);
	}
}

void LLTexUnit::setTextureFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
	if (mIndex < 0 || mCurrTexture == 0) return;

	gGL.flush();

	if (option == TFO_POINT)
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (option >= TFO_TRILINEAR && mHasMipMaps)
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	} 
	else if (option >= TFO_BILINEAR)
	{
		if (mHasMipMaps)
		{
			glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		}
		else
		{
			glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
	}
	else
	{
		if (mHasMipMaps)
		{
			glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		}
		else
		{
			glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
	}

	if (gGLManager.mHasAnisotropic)
	{
		if (LLImageGL::sGlobalUseAnisotropic && option == TFO_ANISOTROPIC)
		{
			if (gGL.mMaxAnisotropy < 1.f)
			{
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gGL.mMaxAnisotropy);

				LL_INFOS() << "gGL.mMaxAnisotropy: " << gGL.mMaxAnisotropy << LL_ENDL ;
				gGL.mMaxAnisotropy = llmax(1.f, gGL.mMaxAnisotropy) ;
			}
			glTexParameterf(sGLTextureType[mCurrTexType], GL_TEXTURE_MAX_ANISOTROPY_EXT, gGL.mMaxAnisotropy);
		}
		else
		{
			glTexParameterf(sGLTextureType[mCurrTexType], GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);
		}
	}
}

void LLTexUnit::setTextureBlendType(eTextureBlendType type)
{
	if (LLGLSLShader::sNoFixedFunction)
	{ //texture blend type means nothing when using shaders
		return;
	}

	if (mIndex < 0) return;

	// Do nothing if it's already correctly set.
	if (mCurrBlendType == type && !gGL.mDirty)
	{
		return;
	}

	gGL.flush();

	activate();
	mCurrBlendType = type;
	S32 scale_amount = 1;
	switch (type) 
	{
		case TB_REPLACE:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			break;
		case TB_ADD:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			break;
		case TB_MULT:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			break;
		case TB_MULT_X2:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			scale_amount = 2;
			break;
		case TB_ALPHA_BLEND:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			break;
		case TB_COMBINE:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			break;
		default:
			LL_ERRS() << "Unknown Texture Blend Type: " << type << LL_ENDL;
			break;
	}
	setColorScale(scale_amount);
	setAlphaScale(1);
}

GLint LLTexUnit::getTextureSource(eTextureBlendSrc src)
{
	switch(src)
	{
		// All four cases should return the same value.
		case TBS_PREV_COLOR:
		case TBS_PREV_ALPHA:
		case TBS_ONE_MINUS_PREV_COLOR:
		case TBS_ONE_MINUS_PREV_ALPHA:
			return GL_PREVIOUS_ARB;

		// All four cases should return the same value.
		case TBS_TEX_COLOR:
		case TBS_TEX_ALPHA:
		case TBS_ONE_MINUS_TEX_COLOR:
		case TBS_ONE_MINUS_TEX_ALPHA:
			return GL_TEXTURE;

		// All four cases should return the same value.
		case TBS_VERT_COLOR:
		case TBS_VERT_ALPHA:
		case TBS_ONE_MINUS_VERT_COLOR:
		case TBS_ONE_MINUS_VERT_ALPHA:
			return GL_PRIMARY_COLOR_ARB;

		// All four cases should return the same value.
		case TBS_CONST_COLOR:
		case TBS_CONST_ALPHA:
		case TBS_ONE_MINUS_CONST_COLOR:
		case TBS_ONE_MINUS_CONST_ALPHA:
			return GL_CONSTANT_ARB;

		default:
			LL_WARNS() << "Unknown eTextureBlendSrc: " << src << ".  Using Vertex Color instead." << LL_ENDL;
			return GL_PRIMARY_COLOR_ARB;
	}
}

GLint LLTexUnit::getTextureSourceType(eTextureBlendSrc src, bool isAlpha)
{
	switch(src)
	{
		// All four cases should return the same value.
		case TBS_PREV_COLOR:
		case TBS_TEX_COLOR:
		case TBS_VERT_COLOR:
		case TBS_CONST_COLOR:
			return (isAlpha) ? GL_SRC_ALPHA: GL_SRC_COLOR;

		// All four cases should return the same value.
		case TBS_PREV_ALPHA:
		case TBS_TEX_ALPHA:
		case TBS_VERT_ALPHA:
		case TBS_CONST_ALPHA:
			return GL_SRC_ALPHA;

		// All four cases should return the same value.
		case TBS_ONE_MINUS_PREV_COLOR:
		case TBS_ONE_MINUS_TEX_COLOR:
		case TBS_ONE_MINUS_VERT_COLOR:
		case TBS_ONE_MINUS_CONST_COLOR:
			return (isAlpha) ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE_MINUS_SRC_COLOR;

		// All four cases should return the same value.
		case TBS_ONE_MINUS_PREV_ALPHA:
		case TBS_ONE_MINUS_TEX_ALPHA:
		case TBS_ONE_MINUS_VERT_ALPHA:
		case TBS_ONE_MINUS_CONST_ALPHA:
			return GL_ONE_MINUS_SRC_ALPHA;

		default:
			LL_WARNS() << "Unknown eTextureBlendSrc: " << src << ".  Using Source Color or Alpha instead." << LL_ENDL;
			return (isAlpha) ? GL_SRC_ALPHA: GL_SRC_COLOR;
	}
}

void LLTexUnit::setTextureCombiner(eTextureBlendOp op, eTextureBlendSrc src1, eTextureBlendSrc src2, bool isAlpha)
{
	if (LLGLSLShader::sNoFixedFunction)
	{ //register combiners do nothing when not using fixed function
		return;
	}	

	if (mIndex < 0) return;

	activate();
	if (mCurrBlendType != TB_COMBINE || gGL.mDirty)
	{
		mCurrBlendType = TB_COMBINE;
		gGL.flush();
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	}
	
	// We want an early out, because this function does a LOT of stuff.
	if ( ( (isAlpha && (mCurrAlphaOp == op) && (mCurrAlphaSrc1 == src1) && (mCurrAlphaSrc2 == src2))
			|| (!isAlpha && (mCurrColorOp == op) && (mCurrColorSrc1 == src1) && (mCurrColorSrc2 == src2)) ) && !gGL.mDirty)
	{
		return;
	}

	gGL.flush();

	// Get the gl source enums according to the eTextureBlendSrc sources passed in
	GLint source1 = getTextureSource(src1);
	GLint source2 = getTextureSource(src2);
	// Get the gl operand enums according to the eTextureBlendSrc sources passed in
	GLint operand1 = getTextureSourceType(src1, isAlpha);
	GLint operand2 = getTextureSourceType(src2, isAlpha);
	// Default the scale amount to 1
	S32 scale_amount = 1;
	GLenum comb_enum, src0_enum, src1_enum, src2_enum, operand0_enum, operand1_enum, operand2_enum;
	
	if (isAlpha)
	{
		// Set enums to ALPHA ones
		comb_enum = GL_COMBINE_ALPHA_ARB;
		src0_enum = GL_SOURCE0_ALPHA_ARB;
		src1_enum = GL_SOURCE1_ALPHA_ARB;
		src2_enum = GL_SOURCE2_ALPHA_ARB;
		operand0_enum = GL_OPERAND0_ALPHA_ARB;
		operand1_enum = GL_OPERAND1_ALPHA_ARB;
		operand2_enum = GL_OPERAND2_ALPHA_ARB;

		// cache current combiner
		mCurrAlphaOp = op;
		mCurrAlphaSrc1 = src1;
		mCurrAlphaSrc2 = src2;
	}
	else 
	{
		// Set enums to RGB ones
		comb_enum = GL_COMBINE_RGB_ARB;
		src0_enum = GL_SOURCE0_RGB_ARB;
		src1_enum = GL_SOURCE1_RGB_ARB;
		src2_enum = GL_SOURCE2_RGB_ARB;
		operand0_enum = GL_OPERAND0_RGB_ARB;
		operand1_enum = GL_OPERAND1_RGB_ARB;
		operand2_enum = GL_OPERAND2_RGB_ARB;

		// cache current combiner
		mCurrColorOp = op;
		mCurrColorSrc1 = src1;
		mCurrColorSrc2 = src2;
	}

	switch(op)
	{
		case TBO_REPLACE:
			// Slightly special syntax (no second sources), just set all and return.
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
			glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
			(isAlpha) ? setAlphaScale(1) : setColorScale(1);
			return;

		case TBO_MULT:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			break;

		case TBO_MULT_X2:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			scale_amount = 2;
			break;

		case TBO_MULT_X4:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			scale_amount = 4;
			break;

		case TBO_ADD:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_ADD);
			break;

		case TBO_ADD_SIGNED:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_ADD_SIGNED_ARB);
			break;

		case TBO_SUBTRACT:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_SUBTRACT_ARB);
			break;

		case TBO_LERP_VERT_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PRIMARY_COLOR_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_TEX_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_PREV_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_CONST_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_CONSTANT_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_VERT_COLOR:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PRIMARY_COLOR_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, (isAlpha) ? GL_SRC_ALPHA : GL_SRC_COLOR);
			break;

		default:
			LL_WARNS() << "Unknown eTextureBlendOp: " << op << ".  Setting op to replace." << LL_ENDL;
			// Slightly special syntax (no second sources), just set all and return.
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
			glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
			(isAlpha) ? setAlphaScale(1) : setColorScale(1);
			return;
	}

	// Set sources, operands, and scale accordingly
	glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
	glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
	glTexEnvi(GL_TEXTURE_ENV, src1_enum, source2);
	glTexEnvi(GL_TEXTURE_ENV, operand1_enum, operand2);
	(isAlpha) ? setAlphaScale(scale_amount) : setColorScale(scale_amount);
}

void LLTexUnit::setColorScale(S32 scale)
{
	if (mCurrColorScale != scale || gGL.mDirty)
	{
		mCurrColorScale = scale;
		gGL.flush();
		glTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE, scale );
	}
}

void LLTexUnit::setAlphaScale(S32 scale)
{
	if (mCurrAlphaScale != scale || gGL.mDirty)
	{
		mCurrAlphaScale = scale;
		gGL.flush();
		glTexEnvi( GL_TEXTURE_ENV, GL_ALPHA_SCALE, scale );
	}
}

// Useful for debugging that you've manually assigned a texture operation to the correct 
// texture unit based on the currently set active texture in opengl.
void LLTexUnit::debugTextureUnit(void)
{
	if (mIndex < 0) return;

	GLint activeTexture;
	glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);
	if ((GL_TEXTURE0_ARB + mIndex) != activeTexture)
	{
		U32 set_unit = (activeTexture - GL_TEXTURE0_ARB);
		LL_WARNS() << "Incorrect Texture Unit!  Expected: " << set_unit << " Actual: " << mIndex << LL_ENDL;
	}
}

LLLightState::LLLightState(S32 index)
: mIndex(index),
  mEnabled(false),
  mConstantAtten(1.f),
  mLinearAtten(0.f),
  mQuadraticAtten(0.f),
  mSpotExponent(0.f),
  mSpotCutoff(180.f)
{
	if (mIndex == 0)
	{
		mDiffuse.set(1,1,1,1);
		mSpecular.set(1,1,1,1);
	}

	mAmbient.set(0,0,0,1);
	mPosition.set(0,0,1,0);
	mSpotDirection.set(0,0,-1);
}

void LLLightState::enable()
{
	if (!mEnabled)
	{
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glEnable(GL_LIGHT0+mIndex);
		}
		mEnabled = true;
	}
}

void LLLightState::disable()
{
	if (mEnabled)
	{
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glDisable(GL_LIGHT0+mIndex);
		}
		mEnabled = false;
	}
}

void LLLightState::setDiffuse(const LLColor4& diffuse)
{
	if (mDiffuse != diffuse)
	{
		++gGL.mLightHash;
		mDiffuse = diffuse;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightfv(GL_LIGHT0+mIndex, GL_DIFFUSE, mDiffuse.mV);
		}
	}
}

void LLLightState::setAmbient(const LLColor4& ambient)
{
	if (mAmbient != ambient)
	{
		++gGL.mLightHash;
		mAmbient = ambient;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightfv(GL_LIGHT0+mIndex, GL_AMBIENT, mAmbient.mV);
		}
	}
}

void LLLightState::setSpecular(const LLColor4& specular)
{
	if (mSpecular != specular)
	{
		++gGL.mLightHash;
		mSpecular = specular;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightfv(GL_LIGHT0+mIndex, GL_SPECULAR, mSpecular.mV);
		}
	}
}

void LLLightState::setPosition(const LLVector4& position)
{
	//always set position because modelview matrix may have changed
	++gGL.mLightHash;
	mPosition = position;
	if (!LLGLSLShader::sNoFixedFunction)
	{
		glLightfv(GL_LIGHT0+mIndex, GL_POSITION, mPosition.mV);
	}
	else
	{ //transform position by current modelview matrix
		glm::vec4 pos = glm::make_vec4(position.mV);
		pos = gGL.getModelviewMatrix() * pos;

		mPosition.set(glm::value_ptr(pos));
	}

}

void LLLightState::setConstantAttenuation(const F32& atten)
{
	if (mConstantAtten != atten)
	{
		mConstantAtten = atten;
		++gGL.mLightHash;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightf(GL_LIGHT0+mIndex, GL_CONSTANT_ATTENUATION, atten);
		}
	}
}

void LLLightState::setLinearAttenuation(const F32& atten)
{
	if (mLinearAtten != atten)
	{
		++gGL.mLightHash;
		mLinearAtten = atten;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightf(GL_LIGHT0+mIndex, GL_LINEAR_ATTENUATION, atten);
		}
	}
}

void LLLightState::setQuadraticAttenuation(const F32& atten)
{
	if (mQuadraticAtten != atten)
	{
		++gGL.mLightHash;
		mQuadraticAtten = atten;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightf(GL_LIGHT0+mIndex, GL_QUADRATIC_ATTENUATION, atten);
		}
	}
}

void LLLightState::setSpotExponent(const F32& exponent)
{
	if (mSpotExponent != exponent)
	{
		++gGL.mLightHash;
		mSpotExponent = exponent;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightf(GL_LIGHT0+mIndex, GL_SPOT_EXPONENT, exponent);
		}
	}
}

void LLLightState::setSpotCutoff(const F32& cutoff)
{
	if (mSpotCutoff != cutoff)
	{
		++gGL.mLightHash;
		mSpotCutoff = cutoff;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightf(GL_LIGHT0+mIndex, GL_SPOT_CUTOFF, cutoff);
		}
	}
}

void LLLightState::setSpotDirection(const LLVector3& direction)
{
	//always set direction because modelview matrix may have changed
	++gGL.mLightHash;
	mSpotDirection = direction;
	if (!LLGLSLShader::sNoFixedFunction)
	{
		glLightfv(GL_LIGHT0+mIndex, GL_SPOT_DIRECTION, direction.mV);
	}
	else
	{ //transform direction by current modelview matrix
		glm::vec3 dir(glm::make_vec3(direction.mV));
		dir = glm::mat3(gGL.getModelviewMatrix()) * dir;

		mSpotDirection.set(glm::value_ptr(dir));
	}
}

LLRender::LLRender()
  : mDirty(false),
    mCount(0),
    mMode(LLRender::TRIANGLES),
    mCurrTextureUnitIndex(0),
    mLineWidth(1.f),
	mMaxAnisotropy(0.f),
	mPrimitiveReset(false)
{	
	mTexUnits.reserve(LL_NUM_TEXTURE_LAYERS);
	for (U32 i = 0; i < LL_NUM_TEXTURE_LAYERS; i++)
	{
		mTexUnits.push_back(new LLTexUnit(i));
	}
	mDummyTexUnit = new LLTexUnit(-1);

	for (U32 i = 0; i < LL_NUM_LIGHT_UNITS; ++i)
	{
		mLightState.push_back(new LLLightState(i));
	}

	for (U32 i = 0; i < 4; i++)
	{
		mCurrColorMask[i] = true;
	}

	mCurrAlphaFunc = CF_DEFAULT;
	mCurrAlphaFuncVal = 0.01f;
	mCurrBlendColorSFactor = BF_UNDEF;
	mCurrBlendAlphaSFactor = BF_UNDEF;
	mCurrBlendColorDFactor = BF_UNDEF;
	mCurrBlendAlphaDFactor = BF_UNDEF;

	mMatrixMode = LLRender::MM_MODELVIEW;
	
	for (U32 i = 0; i < NUM_MATRIX_MODES; ++i)
	{
		mMatIdx[i] = 0;
		mMatHash[i] = 0;
		mCurMatHash[i] = 0xFFFFFFFF;
	}

	mLightHash = 0;
}

LLRender::~LLRender()
{
	shutdown();
}

void LLRender::init()
{
	if (sGLCoreProfile && !LLVertexBuffer::sUseVAO)
	{ //bind a dummy vertex array object so we're core profile compliant
#ifdef GL_ARB_vertex_array_object
		U32 ret;
		glGenVertexArrays(1, &ret);
		glBindVertexArray(ret);
#endif
	}
	restoreVertexBuffers();
}

void LLRender::shutdown()
{
	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		delete mTexUnits[i];
	}
	mTexUnits.clear();
	delete mDummyTexUnit;
	mDummyTexUnit = nullptr;

	for (U32 i = 0; i < mLightState.size(); ++i)
	{
		delete mLightState[i];
	}
	mLightState.clear();
	mBuffer = nullptr ;
}

void LLRender::refreshState(void)
{
	mDirty = true;

	U32 active_unit = mCurrTextureUnitIndex;

	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		mTexUnits[i]->refreshState();
	}
	
	mTexUnits[active_unit]->activate();

	setColorMask(mCurrColorMask[0], mCurrColorMask[1], mCurrColorMask[2], mCurrColorMask[3]);
	
	setAlphaRejectSettings(mCurrAlphaFunc, mCurrAlphaFuncVal);

	mDirty = false;
}

void LLRender::resetVertexBuffers()
{
	mBuffer = nullptr;
}

void LLRender::restoreVertexBuffers()
{
	llassert_always(mBuffer.isNull());
	stop_glerror();
	mBuffer = new LLVertexBuffer(immediate_mask, 0);
	mBuffer->allocateBuffer(4096, 0, TRUE);
	mBuffer->getVertexStrider(mVerticesp);
	mBuffer->getTexCoord0Strider(mTexcoordsp);
	mBuffer->getColorStrider(mColorsp);
	stop_glerror();
}

void LLRender::syncLightState()
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

	if (!shader)
	{
		return;
	}

	if (shader->mLightHash != mLightHash)
	{
		shader->mLightHash = mLightHash;

		LLVector4 position[8];
		LLVector3 direction[8];
		LLVector3 attenuation[8];
		LLVector3 diffuse[8];

		for (U32 i = 0; i < 8; i++)
		{
			LLLightState* light = mLightState[i];

			position[i] = light->mPosition;
			direction[i] = light->mSpotDirection;
			attenuation[i].set(light->mLinearAtten, light->mQuadraticAtten, light->mSpecular.mV[3]);
			diffuse[i].set(light->mDiffuse.mV);
		}

		shader->uniform4fv(LLShaderMgr::LIGHT_POSITION, 8, position[0].mV);
		shader->uniform3fv(LLShaderMgr::LIGHT_DIRECTION, 8, direction[0].mV);
		shader->uniform3fv(LLShaderMgr::LIGHT_ATTENUATION, 8, attenuation[0].mV);
		shader->uniform3fv(LLShaderMgr::LIGHT_DIFFUSE, 8, diffuse[0].mV);
		shader->uniform4fv(LLShaderMgr::LIGHT_AMBIENT, 1, mAmbientLightColor.mV);
		//HACK -- duplicate sunlight color for compatibility with drivers that can't deal with multiple shader objects referencing the same uniform
		shader->uniform3fv(LLShaderMgr::SUNLIGHT_COLOR, 1, diffuse[0].mV);
	}
}

void LLRender::syncMatrices()
{
	stop_glerror();

	static const U32 name[] = 
	{
		LLShaderMgr::MODELVIEW_MATRIX,
		LLShaderMgr::PROJECTION_MATRIX,
		LLShaderMgr::TEXTURE_MATRIX0,
		LLShaderMgr::TEXTURE_MATRIX1,
		LLShaderMgr::TEXTURE_MATRIX2,
		LLShaderMgr::TEXTURE_MATRIX3,
	};

	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

	static glm::mat4 cached_mvp;
	static U32 cached_mvp_mdv_hash = 0xFFFFFFFF;
	static U32 cached_mvp_proj_hash = 0xFFFFFFFF;
	
	static glm::mat4 cached_normal;
	static U32 cached_normal_hash = 0xFFFFFFFF;

	if (shader)
	{
		//llassert(shader);

		bool mvp_done = false;

		U32 i = MM_MODELVIEW;
		if (mMatHash[i] != shader->mMatHash[i])
		{ //update modelview, normal, and MVP
			const glm::mat4& mat = mMatrix[i][mMatIdx[i]];

			shader->uniformMatrix4fv(name[i], 1, GL_FALSE, glm::value_ptr(mat));
			shader->mMatHash[i] = mMatHash[i];

			//update normal matrix
			S32 loc = shader->getUniformLocation(LLShaderMgr::NORMAL_MATRIX);
			if (loc > -1)
			{
				if (cached_normal_hash != mMatHash[i])
				{
					cached_normal = glm::transpose(glm::inverse(mat));
					cached_normal_hash = mMatHash[i];
				}

				shader->uniformMatrix3fv(LLShaderMgr::NORMAL_MATRIX, 1, GL_FALSE, glm::value_ptr(glm::mat3(cached_normal)));
			}

			//update MVP matrix
			mvp_done = true;
			loc = shader->getUniformLocation(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX);
			if (loc > -1)
			{
				U32 proj = MM_PROJECTION;

				if (cached_mvp_mdv_hash != mMatHash[i] || cached_mvp_proj_hash != mMatHash[MM_PROJECTION])
				{
					cached_mvp = mat;
					cached_mvp = mMatrix[proj][mMatIdx[proj]] * cached_mvp;
					cached_mvp_mdv_hash = mMatHash[i];
					cached_mvp_proj_hash = mMatHash[MM_PROJECTION];
				}

				shader->uniformMatrix4fv(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX, 1, GL_FALSE, glm::value_ptr(cached_mvp));
			}
		}


		i = MM_PROJECTION;
		if (mMatHash[i] != shader->mMatHash[i])
		{ //update projection matrix, normal, and MVP
			const glm::mat4& mat = mMatrix[i][mMatIdx[i]];

			shader->uniformMatrix4fv(name[i], 1, GL_FALSE, glm::value_ptr(mat));
			shader->mMatHash[i] = mMatHash[i];

			if (!mvp_done)
			{
				//update MVP matrix
				S32 loc = shader->getUniformLocation(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX);
				if (loc > -1)
				{
					if (cached_mvp_mdv_hash != mMatHash[i] || cached_mvp_proj_hash != mMatHash[MM_PROJECTION])
					{
						U32 mdv = MM_MODELVIEW;
						cached_mvp = mat;
						cached_mvp *= mMatrix[mdv][mMatIdx[mdv]];
						cached_mvp_mdv_hash = mMatHash[MM_MODELVIEW];
						cached_mvp_proj_hash = mMatHash[MM_PROJECTION];
					}
									
					shader->uniformMatrix4fv(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX, 1, GL_FALSE, glm::value_ptr(cached_mvp));
				}
			}
		}

		for (i = MM_TEXTURE0; i < NUM_MATRIX_MODES; ++i)
		{
			if (mMatHash[i] != shader->mMatHash[i])
			{
				shader->uniformMatrix4fv(name[i], 1, GL_FALSE, glm::value_ptr(mMatrix[i][mMatIdx[i]]));
				shader->mMatHash[i] = mMatHash[i];
			}
		}


		if (shader->mFeatures.hasLighting || shader->mFeatures.calculatesLighting)
		{ //also sync light state
			syncLightState();
		}
	}
	else if (!LLGLSLShader::sNoFixedFunction)
	{
		static const GLenum mode[] = 
		{
			GL_MODELVIEW,
			GL_PROJECTION,
			GL_TEXTURE,
			GL_TEXTURE,
			GL_TEXTURE,
			GL_TEXTURE,
		};

		for (U32 i = 0; i < 2; ++i)
		{
			if (mMatHash[i] != mCurMatHash[i])
			{
				glMatrixMode(mode[i]);
				glLoadMatrixf(glm::value_ptr(mMatrix[i][mMatIdx[i]]));
				mCurMatHash[i] = mMatHash[i];
			}
		}

		for (U32 i = 2; i < NUM_MATRIX_MODES; ++i)
		{
			if (mMatHash[i] != mCurMatHash[i])
			{
				gGL.getTexUnit(i-2)->activate();
				glMatrixMode(mode[i]);
				glLoadMatrixf(glm::value_ptr(mMatrix[i][mMatIdx[i]]));
				mCurMatHash[i] = mMatHash[i];
			}
		}
	}

	stop_glerror();
}

LLMatrix4a LLRender::genRot(const GLfloat& a, const LLVector4a& axis) const
{
	F32 r = a * DEG_TO_RAD;

	F32 c = cosf(r);
	F32 s = sinf(r);

	F32 ic = 1.f-c;

	const LLVector4a add1(c,axis[VZ]*s,-axis[VY]*s);	//1,z,-y
	const LLVector4a add2(-axis[VZ]*s,c,axis[VX]*s);	//-z,1,x
	const LLVector4a add3(axis[VY]*s,-axis[VX]*s,c);	//y,-x,1

	LLVector4a axis_x;
	axis_x.splat<0>(axis);
	LLVector4a axis_y;
	axis_y.splat<1>(axis);
	LLVector4a axis_z;
	axis_z.splat<2>(axis);

	LLVector4a c_axis;
	c_axis.setMul(axis,ic);

	LLMatrix4a rot_mat;
	rot_mat.getRow<0>().setMul(c_axis,axis_x);
	rot_mat.getRow<0>().add(add1);
	rot_mat.getRow<1>().setMul(c_axis,axis_y);
	rot_mat.getRow<1>().add(add2);
	rot_mat.getRow<2>().setMul(c_axis,axis_z);
	rot_mat.getRow<2>().add(add3);
	rot_mat.setRow<3>(LLVector4a(0,0,0,1));

	return rot_mat;
}

void LLRender::translatef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
	flush();

	{
		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::translate(mMatrix[mMatrixMode][mMatIdx[mMatrixMode]], glm::vec3(x, y, z));
		mMatHash[mMatrixMode]++;
	}
}

void LLRender::scalef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
	flush();
	
	{
		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::scale(mMatrix[mMatrixMode][mMatIdx[mMatrixMode]], glm::vec3(x, y, z));
		mMatHash[mMatrixMode]++;
	}
}

void LLRender::ortho(F32 left, F32 right, F32 bottom, F32 top, F32 zNear, F32 zFar)
{
	flush();

	{
		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] *= glm::ortho(left, right, bottom, top, zNear, zFar);
		mMatHash[mMatrixMode]++;
	}
}

void LLRender::rotatef(const GLfloat& a, const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
	flush();

	{
		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::rotate(mMatrix[mMatrixMode][mMatIdx[mMatrixMode]], glm::radians(a), glm::vec3(x, y, z));
		mMatHash[mMatrixMode]++;
	}
}

void LLRender::pushMatrix()
{
	flush();
	
	{
		if (mMatIdx[mMatrixMode] < LL_MATRIX_STACK_DEPTH-1)
		{
			mMatrix[mMatrixMode][mMatIdx[mMatrixMode]+1] = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
			++mMatIdx[mMatrixMode];
		}
		else
		{
			LL_WARNS() << "Matrix stack overflow." << LL_ENDL;
		}
	}
}

void LLRender::popMatrix()
{
	flush();
	{
		if (mMatIdx[mMatrixMode] > 0)
		{
			--mMatIdx[mMatrixMode];
			mMatHash[mMatrixMode]++;
		}
		else
		{
			LL_WARNS() << "Matrix stack underflow." << LL_ENDL;
		}
	}
}

void LLRender::loadMatrix(const GLfloat* m)
{
	flush();
	{
		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::make_mat4(m);
		mMatHash[mMatrixMode]++;
	}
}

void LLRender::multMatrix(const GLfloat* m)
{
	flush();
	{
		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] *= glm::make_mat4(m);
		mMatHash[mMatrixMode]++;
	}
}

void LLRender::matrixMode(U32 mode)
{
	if (mode == MM_TEXTURE)
	{
		mode = MM_TEXTURE0 + gGL.getCurrentTexUnitIndex();
	}

	llassert(mode < NUM_MATRIX_MODES);
	mMatrixMode = mode;
}

U32 LLRender::getMatrixMode()
{
	if (mMatrixMode >= MM_TEXTURE0 && mMatrixMode <= MM_TEXTURE3)
	{ //always return MM_TEXTURE if current matrix mode points at any texture matrix
		return MM_TEXTURE;
	}

	return mMatrixMode;
}


void LLRender::loadIdentity()
{
	flush();

	{
		llassert_always(mMatrixMode < NUM_MATRIX_MODES) ;

		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::mat4(1.f);
		mMatHash[mMatrixMode]++;
	}
}

const glm::mat4& LLRender::getModelviewMatrix()
{
	return mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];
}

const glm::mat4& LLRender::getProjectionMatrix()
{
	return mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];
}

void LLRender::translateUI(F32 x, F32 y, F32 z)
{
	if (mUIOffset.empty())
	{
		LL_ERRS() << "Need to push a UI translation frame before offsetting" << LL_ENDL;
	}

	LLVector4a add(x,y,z);
	mUIOffset.back().add(add);
}

void LLRender::scaleUI(F32 x, F32 y, F32 z)
{
	if (mUIScale.empty())
	{
		LL_ERRS() << "Need to push a UI transformation frame before scaling." << LL_ENDL;
	}

	LLVector4a scale(x,y,z);
	mUIScale.back().mul(scale);
}

void LLRender::rotateUI(LLQuaternion& rot)
{
	if (mUIRotation.empty())
	{
		mUIRotation.push_back(rot);
	}
	else
	{
		mUIRotation.push_back(mUIRotation.back()*rot);
	}
}

void LLRender::pushUIMatrix()
{
	if (mUIOffset.empty())
	{
		mUIOffset.emplace_back(LLVector4a(0.f));
	}
	else
	{
		mUIOffset.push_back(mUIOffset.back());
	}
	
	if (mUIScale.empty())
	{
		mUIScale.emplace_back(LLVector4a(1.f));
	}
	else
	{
		mUIScale.push_back(mUIScale.back());
	}
	if (!mUIRotation.empty())
	{
		mUIRotation.push_back(mUIRotation.back());
	}
}

void LLRender::popUIMatrix()
{
	if (mUIOffset.empty() || mUIScale.empty())
	{
		LL_ERRS() << "UI offset or scale stack blown." << LL_ENDL;
	}
	mUIOffset.pop_back();
	mUIScale.pop_back();
	if (!mUIRotation.empty())
	{
		mUIRotation.pop_back();
	}
}

LLVector3 LLRender::getUITranslation()
{
	if (mUIOffset.empty())
	{
		return LLVector3(0,0,0);
	}
	return LLVector3(mUIOffset.back().getF32ptr());
}

LLVector3 LLRender::getUIScale()
{
	if (mUIScale.empty())
	{
		return LLVector3(1,1,1);
	}
	return LLVector3(mUIScale.back().getF32ptr());
}


void LLRender::loadUIIdentity()
{
	if (mUIOffset.empty() || mUIScale.empty())
	{
		LL_ERRS() << "Need to push UI translation frame before clearing offset." << LL_ENDL;
	}
	mUIOffset.back().splat(0.f);
	mUIScale.back().splat(1.f);
	if (!mUIRotation.empty())
		mUIRotation.push_back(LLQuaternion());
}

void LLRender::setColorMask(bool writeColor, bool writeAlpha)
{
	setColorMask(writeColor, writeColor, writeColor, writeAlpha);
}

void LLRender::setColorMask(bool writeColorR, bool writeColorG, bool writeColorB, bool writeAlpha)
{
	flush();

	if (mCurrColorMask[0] != writeColorR ||
		mCurrColorMask[1] != writeColorG ||
		mCurrColorMask[2] != writeColorB ||
		mCurrColorMask[3] != writeAlpha)
	{
		mCurrColorMask[0] = writeColorR;
		mCurrColorMask[1] = writeColorG;
		mCurrColorMask[2] = writeColorB;
		mCurrColorMask[3] = writeAlpha;

		glColorMask(writeColorR ? GL_TRUE : GL_FALSE, 
					writeColorG ? GL_TRUE : GL_FALSE,
					writeColorB ? GL_TRUE : GL_FALSE,
					writeAlpha ? GL_TRUE : GL_FALSE);
	}
}

void LLRender::setSceneBlendType(eBlendType type)
{
	switch (type) 
	{
		case BT_ALPHA:
			blendFunc(BF_SOURCE_ALPHA, BF_ONE_MINUS_SOURCE_ALPHA);
			break;
		case BT_ADD:
			blendFunc(BF_ONE, BF_ONE);
			break;
		case BT_ADD_WITH_ALPHA:
			blendFunc(BF_SOURCE_ALPHA, BF_ONE);
			break;
		case BT_MULT:
			blendFunc(BF_DEST_COLOR, BF_ZERO);
			break;
		case BT_MULT_ALPHA:
			blendFunc(BF_DEST_ALPHA, BF_ZERO);
			break;
		case BT_MULT_X2:
			blendFunc(BF_DEST_COLOR, BF_SOURCE_COLOR);
			break;
		case BT_REPLACE:
			blendFunc(BF_ONE, BF_ZERO);
			break;
		default:
			LL_ERRS() << "Unknown Scene Blend Type: " << type << LL_ENDL;
			break;
	}
}

void LLRender::setAlphaRejectSettings(eCompareFunc func, F32 value)
{
	flush();

	if (LLGLSLShader::sNoFixedFunction)
	{ //glAlphaFunc is deprecated in OpenGL 3.3
		return;
	}

	if (mCurrAlphaFunc != func ||
		mCurrAlphaFuncVal != value)
	{
		mCurrAlphaFunc = func;
		mCurrAlphaFuncVal = value;
		if (func == CF_DEFAULT)
		{
			glAlphaFunc(GL_GREATER, 0.01f);
		} 
		else
		{
			glAlphaFunc(sGLCompareFunc[func], value);
		}
	}

	if (gDebugGL)
	{ //make sure cached state is correct
		GLint cur_func = 0;
		glGetIntegerv(GL_ALPHA_TEST_FUNC, &cur_func);

		if (func == CF_DEFAULT)
		{
			func = CF_GREATER;
		}

		if (cur_func != sGLCompareFunc[func])
		{
			LL_ERRS() << "Alpha test function corrupted!" << LL_ENDL;
		}

		F32 ref = 0.f;
		glGetFloatv(GL_ALPHA_TEST_REF, &ref);

		if (ref != value)
		{
			LL_ERRS() << "Alpha test value corrupted!" << LL_ENDL;
		}
	}
}

void LLRender::blendFunc(eBlendFactor sfactor, eBlendFactor dfactor)
{
	llassert(sfactor < BF_UNDEF);
	llassert(dfactor < BF_UNDEF);
	if (mCurrBlendColorSFactor != sfactor || mCurrBlendColorDFactor != dfactor ||
	    mCurrBlendAlphaSFactor != sfactor || mCurrBlendAlphaDFactor != dfactor)
	{
		mCurrBlendColorSFactor = sfactor;
		mCurrBlendAlphaSFactor = sfactor;
		mCurrBlendColorDFactor = dfactor;
		mCurrBlendAlphaDFactor = dfactor;
		flush();
		glBlendFunc(sGLBlendFactor[sfactor], sGLBlendFactor[dfactor]);
	}
}

void LLRender::blendFunc(eBlendFactor color_sfactor, eBlendFactor color_dfactor,
			 eBlendFactor alpha_sfactor, eBlendFactor alpha_dfactor)
{
	llassert(color_sfactor < BF_UNDEF);
	llassert(color_dfactor < BF_UNDEF);
	llassert(alpha_sfactor < BF_UNDEF);
	llassert(alpha_dfactor < BF_UNDEF);
	if (!gGLManager.mHasBlendFuncSeparate)
	{
		LL_WARNS_ONCE("render") << "no glBlendFuncSeparateEXT(), using color-only blend func" << LL_ENDL;
		blendFunc(color_sfactor, color_dfactor);
		return;
	}
	if (mCurrBlendColorSFactor != color_sfactor || mCurrBlendColorDFactor != color_dfactor ||
	    mCurrBlendAlphaSFactor != alpha_sfactor || mCurrBlendAlphaDFactor != alpha_dfactor)
	{
		mCurrBlendColorSFactor = color_sfactor;
		mCurrBlendAlphaSFactor = alpha_sfactor;
		mCurrBlendColorDFactor = color_dfactor;
		mCurrBlendAlphaDFactor = alpha_dfactor;
		flush();
		glBlendFuncSeparateEXT(sGLBlendFactor[color_sfactor], sGLBlendFactor[color_dfactor],
				       sGLBlendFactor[alpha_sfactor], sGLBlendFactor[alpha_dfactor]);
	}
}

LLTexUnit* LLRender::getTexUnit(U32 index)
{
	if (index < mTexUnits.size())
	{
		return mTexUnits[index];
	}
	else 
	{
		LL_DEBUGS() << "Non-existing texture unit layer requested: " << index << LL_ENDL;
		return mDummyTexUnit;
	}
}

LLLightState* LLRender::getLight(U32 index)
{
	if (index < mLightState.size())
	{
		return mLightState[index];
	}
	
	return nullptr;
}

void LLRender::setAmbientLightColor(const LLColor4& color)
{
	if (color != mAmbientLightColor)
	{
		++mLightHash;
		mAmbientLightColor = color;
		if (!LLGLSLShader::sNoFixedFunction)
		{
			glLightModelfv(GL_LIGHT_MODEL_AMBIENT, color.mV);
		}
	}
}

void LLRender::setLineWidth(F32 line_width)
{
	if (LLRender::sGLCoreProfile)
	{
		line_width = 1.f;
	}
	if (mLineWidth != line_width)
	{
		if (mMode == LLRender::LINES || mMode == LLRender::LINE_STRIP)
		{
			flush();
		}
		mLineWidth = line_width;
		glLineWidth(line_width);
	}
}

bool LLRender::verifyTexUnitActive(U32 unitToVerify)
{
	if (mCurrTextureUnitIndex == unitToVerify)
	{
		return true;
	}
	else 
	{
		LL_WARNS() << "TexUnit currently active: " << mCurrTextureUnitIndex << " (expecting " << unitToVerify << ")" << LL_ENDL;
		return false;
	}
}

void LLRender::clearErrors()
{
	while (glGetError())
	{
		//loop until no more error flags left
	}
}

void LLRender::begin(const GLuint& mode)
{
	if (mode != mMode)
	{
		if (mMode == LLRender::LINES ||
			mMode == LLRender::TRIANGLES ||
			mMode == LLRender::POINTS ||
			mMode == LLRender::TRIANGLE_STRIP )
		{
			flush();
		}
		else if (mCount != 0)
		{
			LL_ERRS() << "gGL.begin() called redundantly." << LL_ENDL;
		}
		
		mMode = mode;
	}
}

void LLRender::end()
{ 
	if (mCount == 0)
	{
		return;
		//IMM_ERRS << "GL begin and end called with no vertices specified." << LL_ENDL;
	}

	if ((mMode != LLRender::LINES &&
		mMode != LLRender::TRIANGLES &&
		mMode != LLRender::POINTS &&
		mMode != LLRender::TRIANGLE_STRIP) ||
		mCount > 2048)
	{
		flush();
	}
	else if (mMode == LLRender::TRIANGLE_STRIP)
	{
		mPrimitiveReset = true;
	}
}

void LLRender::flush()
{
	if (mCount > 0)
	{
#if 0
		if (!glIsEnabled(GL_VERTEX_ARRAY))
		{
			LL_ERRS() << "foo 1" << LL_ENDL;
		}

		if (!glIsEnabled(GL_COLOR_ARRAY))
		{
			LL_ERRS() << "foo 2" << LL_ENDL;
		}

		if (!glIsEnabled(GL_TEXTURE_COORD_ARRAY))
		{
			LL_ERRS() << "foo 3" << LL_ENDL;
		}

		if (glIsEnabled(GL_NORMAL_ARRAY))
		{
			LL_ERRS() << "foo 7" << LL_ENDL;
		}

		GLvoid* pointer;

		glGetPointerv(GL_VERTEX_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].v))
		{
			LL_ERRS() << "foo 4" << LL_ENDL;
		}

		glGetPointerv(GL_COLOR_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].c))
		{
			LL_ERRS() << "foo 5" << LL_ENDL;
		}

		glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].uv))
		{
			LL_ERRS() << "foo 6" << LL_ENDL;
		}
#endif
				
		if (!mUIOffset.empty())
		{
			sUICalls++;
			sUIVerts += mCount;
		}
		
		//store mCount in a local variable to avoid re-entrance (drawArrays may call flush)
		U32 count = mCount;
		if (mMode == LLRender::TRIANGLES)
		{
			if (mCount%3 != 0)
			{
				count -= (mCount % 3);
				LL_WARNS() << "Incomplete triangle requested." << LL_ENDL;
			}
		}
			
		if (mMode == LLRender::LINES)
		{
			if (mCount%2 != 0)
			{
				count -= (mCount % 2);
				LL_WARNS() << "Incomplete line requested." << LL_ENDL;
			}
		}

		mCount = 0;

		if (mBuffer->useVBOs() && !mBuffer->isLocked())
		{ //hack to only flush the part of the buffer that was updated (relies on stream draw using buffersubdata)
			mBuffer->getVertexStrider(mVerticesp, 0, count);
			mBuffer->getTexCoord0Strider(mTexcoordsp, 0, count);
			mBuffer->getColorStrider(mColorsp, 0, count);
		}
		
		mBuffer->flush();
		mBuffer->setBuffer(immediate_mask);

		mBuffer->drawArrays(mMode, 0, count);
		
		mVerticesp[0] = mVerticesp[count];
		mTexcoordsp[0] = mTexcoordsp[count];
		mColorsp[0] = mColorsp[count];
		
		mCount = 0;
		mPrimitiveReset = false;
	}
}

void LLRender::vertex4a(const LLVector4a& vertex)
{ 
	//the range of mVerticesp, mColorsp and mTexcoordsp is [0, 4095]
	if (mCount > 2048)
	{ //break when buffer gets reasonably full to keep GL command buffers happy and avoid overflow below
		switch (mMode)
		{
			case LLRender::POINTS: flush(); break;
			case LLRender::TRIANGLES: if (mCount%3==0) flush(); break;
			case LLRender::LINES: if (mCount%2 == 0) flush(); break;
			case LLRender::TRIANGLE_STRIP:
			{
				LLVector4a vert[] = { mVerticesp[mCount - 2], mVerticesp[mCount - 1], mVerticesp[mCount] };
				LLColor4U col[] = { mColorsp[mCount - 2], mColorsp[mCount - 1], mColorsp[mCount] };
				LLVector2 tc[] = { mTexcoordsp[mCount - 2], mTexcoordsp[mCount - 1], mTexcoordsp[mCount] };
				flush();
				for (int i = 0; i < LL_ARRAY_SIZE(vert); ++i)
				{
					mVerticesp[i] = vert[i];
					mColorsp[i] = col[i];
					mTexcoordsp[i] = tc[i];
				}
				mCount = 2;
				break;
			}
		}
	}
			
	if (mCount > 4094)
	{
	//	LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
		return;
	}

	if (mPrimitiveReset && mCount)
	{
		// Insert degenerate
		++mCount;
		mVerticesp[mCount] = mVerticesp[mCount - 1];
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
		mVerticesp[mCount - 1] = mVerticesp[mCount - 2];
		mColorsp[mCount - 1] = mColorsp[mCount - 2];
		mTexcoordsp[mCount - 1] = mTexcoordsp[mCount - 2];
	}

	if (mUIOffset.empty())
	{
		if (!mUIRotation.empty() && mUIRotation.back().isNotIdentity())
		{
			LLVector4 vert(vertex.getF32ptr());
			mVerticesp[mCount].loadua((vert*mUIRotation.back()).mV);
		}
		else
		{
			mVerticesp[mCount] = vertex;
		}
	}
	else
	{
		if (!mUIRotation.empty() && mUIRotation.back().isNotIdentity())
		{
			LLVector4 vert(vertex.getF32ptr());
			vert = vert * mUIRotation.back();
			LLVector4a postrot_vert;
			postrot_vert.loadua(vert.mV);
			mVerticesp[mCount].setAdd(postrot_vert, mUIOffset.back());
			mVerticesp[mCount].mul(mUIScale.back());
		}
		else
		{
			mVerticesp[mCount].setAdd(vertex, mUIOffset.back());
			mVerticesp[mCount].mul(mUIScale.back());
		}
	}

	mCount++;
	mVerticesp[mCount] = mVerticesp[mCount-1];
	mColorsp[mCount] = mColorsp[mCount-1];
	mTexcoordsp[mCount] = mTexcoordsp[mCount-1];

	if (mPrimitiveReset && mCount)
	{
		mCount++;
		mVerticesp[mCount] = mVerticesp[mCount - 1];
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	mPrimitiveReset = false;
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, S32 vert_count)
{
	if (mCount + vert_count > 4094)
	{
		//	LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
		return;
	}

	if (mPrimitiveReset && mCount)
	{
		// Insert degenerate
		++mCount;
		mVerticesp[mCount] = verts[0];
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
		mVerticesp[mCount - 1] = mVerticesp[mCount - 2];
		mColorsp[mCount - 1] = mColorsp[mCount - 2];
		mTexcoordsp[mCount - 1] = mTexcoordsp[mCount - 2];
		++mCount;
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	for (S32 i = 0; i < vert_count; i++)
	{
		mVerticesp[mCount] = verts[i];

		mCount++;
		mTexcoordsp[mCount] = mTexcoordsp[mCount-1];
		mColorsp[mCount] = mColorsp[mCount-1];
	}

	mVerticesp[mCount] = mVerticesp[mCount-1];

	mPrimitiveReset = false;
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, LLVector2* uvs, S32 vert_count)
{
	if (mCount + vert_count > 4094)
	{
		//	LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
		return;
	}

	if (mPrimitiveReset && mCount)
	{
		// Insert degenerate
		++mCount;
		mVerticesp[mCount] = verts[0];
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = uvs[0];
		mVerticesp[mCount - 1] = mVerticesp[mCount - 2];
		mColorsp[mCount - 1] = mColorsp[mCount - 2];
		mTexcoordsp[mCount - 1] = mTexcoordsp[mCount - 2];
		++mCount;
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	for (S32 i = 0; i < vert_count; i++)
	{
		mVerticesp[mCount] = verts[i];
		mTexcoordsp[mCount] = uvs[i];

		mCount++;
		mColorsp[mCount] = mColorsp[mCount-1];
	}
	
	mVerticesp[mCount] = mVerticesp[mCount-1];
	mTexcoordsp[mCount] = mTexcoordsp[mCount-1];

	mPrimitiveReset = false;
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, LLVector2* uvs, LLColor4U* colors, S32 vert_count)
{
	if (mCount + vert_count > 4094)
	{
		//	LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
		return;
	}

	if (mPrimitiveReset && mCount)
	{
		// Insert degenerate
		++mCount;
		mVerticesp[mCount] = verts[0];
		mColorsp[mCount] = colors[mCount - 1];
		mTexcoordsp[mCount] = uvs[0];
		mVerticesp[mCount - 1] = mVerticesp[mCount - 2];
		mColorsp[mCount - 1] = mColorsp[mCount - 2];
		mTexcoordsp[mCount - 1] = mTexcoordsp[mCount - 2];
		++mCount;
		mColorsp[mCount] = mColorsp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}

	for (S32 i = 0; i < vert_count; i++)
	{
		mVerticesp[mCount] = verts[i];
		mTexcoordsp[mCount] = uvs[i];
		mColorsp[mCount] = colors[i];
		
		mCount++;
	}

	mVerticesp[mCount] = mVerticesp[mCount-1];
	mTexcoordsp[mCount] = mTexcoordsp[mCount-1];
	mColorsp[mCount] = mColorsp[mCount-1];

	mPrimitiveReset = false;
}

void LLRender::texCoord2f(const GLfloat& x, const GLfloat& y)
{ 
	mTexcoordsp[mCount] = LLVector2(x,y);
}

void LLRender::texCoord2i(const GLint& x, const GLint& y)
{ 
	texCoord2f((GLfloat) x, (GLfloat) y);
}

void LLRender::texCoord2fv(const GLfloat* tc)
{ 
	texCoord2f(tc[0], tc[1]);
}

void LLRender::color4ub(const GLubyte& r, const GLubyte& g, const GLubyte& b, const GLubyte& a)
{
	if (!LLGLSLShader::sCurBoundShaderPtr ||
		LLGLSLShader::sCurBoundShaderPtr->mAttributeMask & LLVertexBuffer::MAP_COLOR)
	{
		mColorsp[mCount] = LLColor4U(r,g,b,a);
	}
	else
	{ //not using shaders or shader reads color from a uniform
		diffuseColor4ub(r,g,b,a);
	}
}
void LLRender::color4ubv(const GLubyte* c)
{
	color4ub(c[0], c[1], c[2], c[3]);
}

void LLRender::color4f(const GLfloat& r, const GLfloat& g, const GLfloat& b, const GLfloat& a)
{
	color4ub((GLubyte) (llclamp(r, 0.f, 1.f)*255),
		(GLubyte) (llclamp(g, 0.f, 1.f)*255),
		(GLubyte) (llclamp(b, 0.f, 1.f)*255),
		(GLubyte) (llclamp(a, 0.f, 1.f)*255));
}

void LLRender::color4fv(const GLfloat* c)
{ 
	color4f(c[0],c[1],c[2],c[3]);
}

void LLRender::color3f(const GLfloat& r, const GLfloat& g, const GLfloat& b)
{ 
	color4f(r,g,b,1);
}

void LLRender::color3fv(const GLfloat* c)
{ 
	color4f(c[0],c[1],c[2],1);
}

void LLRender::diffuseColor3f(F32 r, F32 g, F32 b)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r,g,b,1.f);
	}
	else
	{
		glColor3f(r,g,b);
	}
}

void LLRender::diffuseColor3fv(const F32* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0], c[1], c[2], 1.f);
	}
	else
	{
		glColor3fv(c);
	}
}

void LLRender::diffuseColor4f(F32 r, F32 g, F32 b, F32 a)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r,g,b,a);
	}
	else
	{
		glColor4f(r,g,b,a);
	}
}

void LLRender::diffuseColor4fv(const F32* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, c);
	}
	else
	{
		glColor4fv(c);
	}
}

void LLRender::diffuseColor4ubv(const U8* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f);
	}
	else
	{
		glColor4ubv(c);
	}
}

void LLRender::diffuseColor4ub(U8 r, U8 g, U8 b, U8 a)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(!LLGLSLShader::sNoFixedFunction || shader != NULL);

	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r/255.f, g/255.f, b/255.f, a/255.f);
	}
	else
	{
		glColor4ub(r,g,b,a);
	}
}


void LLRender::debugTexUnits(void)
{
	LL_INFOS("TextureUnit") << "Active TexUnit: " << mCurrTextureUnitIndex << LL_ENDL;
	std::string active_enabled = "false";
	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		if (getTexUnit(i)->mCurrTexType != LLTexUnit::TT_NONE)
		{
			if (i == mCurrTextureUnitIndex) active_enabled = "true";
			LL_INFOS("TextureUnit") << "TexUnit: " << i << " Enabled" << LL_ENDL;
			LL_INFOS("TextureUnit") << "Enabled As: " ;
			switch (getTexUnit(i)->mCurrTexType)
			{
				case LLTexUnit::TT_TEXTURE:
					LL_CONT << "Texture 2D";
					break;
				case LLTexUnit::TT_CUBE_MAP:
					LL_CONT << "Cube Map";
					break;
				default:
					LL_CONT << "ARGH!!! NONE!";
					break;
			}
			LL_CONT << ", Texture Bound: " << getTexUnit(i)->mCurrTexture << LL_ENDL;
		}
	}
	LL_INFOS("TextureUnit") << "Active TexUnit Enabled : " << active_enabled << LL_ENDL;
}

