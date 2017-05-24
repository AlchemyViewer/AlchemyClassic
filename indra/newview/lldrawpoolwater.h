/** 
 * @file lldrawpoolwater.h
 * @brief LLDrawPoolWater class definition
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

#ifndef LL_LLDRAWPOOLWATER_H
#define LL_LLDRAWPOOLWATER_H

#include "lldrawpool.h"


class LLFace;
class LLHeavenBody;
class LLWaterSurface;

class LLDrawPoolWater: public LLFacePool
{
protected:
	LLPointer<LLViewerTexture> mHBTex[2];
	LLPointer<LLViewerTexture> mWaterImagep;
	LLPointer<LLViewerTexture> mOpaqueWaterImagep;
	LLPointer<LLViewerTexture> mWaterNormp;

public:
	static BOOL sSkipScreenCopy;
	static BOOL sNeedsReflectionUpdate;
	static BOOL sNeedsDistortionUpdate;
	static LLVector3 sLightDir;

	static LLColor4 sWaterFogColor;
	static F32 sWaterFogEnd;

	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0	
	};

	U32 getVertexDataMask() override { return VERTEX_DATA_MASK; }

	LLDrawPoolWater();
	/*virtual*/ ~LLDrawPoolWater();

	/*virtual*/ LLDrawPool *instancePool() override;
	static void restoreGL();
	
	/*virtual*/ S32 getNumPostDeferredPasses() override { return 0; } //getNumPasses(); }
	/*virtual*/ void beginPostDeferredPass(S32 pass) override;
	/*virtual*/ void endPostDeferredPass(S32 pass) override;
	/*virtual*/ void renderPostDeferred(S32 pass) override { render(pass); }
	/*virtual*/ S32 getNumDeferredPasses() override { return 1; }
	/*virtual*/ void renderDeferred(S32 pass = 0) override;

	/*virtual*/ S32 getNumPasses() override;
	/*virtual*/ void render(S32 pass = 0) override;
	/*virtual*/ void prerender() override;

	/*virtual*/ LLViewerTexture *getDebugTexture() override;
	/*virtual*/ LLColor3 getDebugColor() const; // For AGP debug display

	void renderReflection(LLFace* face);
	void shade();

protected:
	void renderOpaqueLegacyWater();
};

void cgErrorCallback();

#endif // LL_LLDRAWPOOLWATER_H
