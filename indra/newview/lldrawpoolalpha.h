/** 
 * @file lldrawpoolalpha.h
 * @brief LLDrawPoolAlpha class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#ifndef LL_LLDRAWPOOLALPHA_H
#define LL_LLDRAWPOOLALPHA_H

#include "lldrawpool.h"
#include "llrender.h"
#include "llframetimer.h"

class LLFace;
class LLColor4;
class LLGLSLShader;

class LLDrawPoolAlpha: public LLRenderPass
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_TEXCOORD0
	};

	U32 getVertexDataMask() override { return VERTEX_DATA_MASK; }

	LLDrawPoolAlpha(U32 type = LLDrawPool::POOL_ALPHA);
	/*virtual*/ ~LLDrawPoolAlpha();

	/*virtual*/ S32 getNumPostDeferredPasses() override;
	/*virtual*/ void beginPostDeferredPass(S32 pass) override;
	/*virtual*/ void endPostDeferredPass(S32 pass) override;
	/*virtual*/ void renderPostDeferred(S32 pass) override;

	/*virtual*/ void beginRenderPass(S32 pass = 0) override;
	/*virtual*/ void endRenderPass( S32 pass ) override;
	/*virtual*/ S32	 getNumPasses() override { return 1; }

	void render(S32 pass = 0) override;
	/*virtual*/ void prerender() override;

	void renderGroupAlpha(LLSpatialGroup* group, U32 type, U32 mask, BOOL texture = TRUE);
	void renderAlpha(U32 mask, S32 pass);
	void renderAlphaHighlight(U32 mask);
		
	static BOOL sShowDebugAlpha;

private:
	LLGLSLShader* current_shader;
	LLGLSLShader* target_shader;
	LLGLSLShader* simple_shader;
	LLGLSLShader* fullbright_shader;	
	LLGLSLShader* emissive_shader;

	// our 'normal' alpha blend function for this pass
	LLRender::eBlendFactor mColorSFactor;
	LLRender::eBlendFactor mColorDFactor;	
	LLRender::eBlendFactor mAlphaSFactor;
	LLRender::eBlendFactor mAlphaDFactor;
};

class LLDrawPoolAlphaPostWater : public LLDrawPoolAlpha
{
public:
	LLDrawPoolAlphaPostWater();
	void render(S32 pass = 0) override;
};

#endif // LL_LLDRAWPOOLALPHA_H
