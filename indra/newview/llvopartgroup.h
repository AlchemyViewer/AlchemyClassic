/** 
 * @file llvopartgroup.h
 * @brief Group of particle systems
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

#ifndef LL_LLVOPARTGROUP_H
#define LL_LLVOPARTGROUP_H

#include "llviewerobject.h"
#include "v3math.h"
#include "v3color.h"
#include "llframetimer.h"
#include "llviewerpartsim.h"
#include "llvertexbuffer.h"

class LLViewerPartGroup;

class LLVOPartGroup : public LLAlphaObject
{
public:

	//vertex buffer for holding all particles
	static LLPointer<LLVertexBuffer> sVB;
	static S32 sVBSlotFree[LL_MAX_PARTICLE_COUNT];
	static S32* sVBSlotCursor;

	static void initClass();
	static void restoreGL();
	static void destroyGL();
	static S32 findAvailableVBSlot();
	static void freeVBSlot(S32 idx);

	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_EMISSIVE |
							LLVertexBuffer::MAP_TEXTURE_INDEX
	};

	LLVOPartGroup(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp);

	/*virtual*/ BOOL    isActive() const override; // Whether this object needs to do an idleUpdate.
	void idleUpdate(LLAgent &agent, const F64 &time) override;

	F32 getBinRadius() override;
	void updateSpatialExtents(LLVector4a& newMin, LLVector4a& newMax) override;
	U32 getPartitionType() const override;
	
	/*virtual*/ BOOL lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
										  S32 face,
										  BOOL pick_transparent,
										  BOOL pick_rigged,
										  S32* face_hit,
										  LLVector4a* intersection,
										  LLVector2* tex_coord,
										  LLVector4a* normal,
										  LLVector4a* tangent) override;

	/*virtual*/ void setPixelAreaAndAngle(LLAgent &agent) override;
	/*virtual*/ void updateTextures() override;

	/*virtual*/ LLDrawable* createDrawable(LLPipeline *pipeline) override;
	/*virtual*/ BOOL        updateGeometry(LLDrawable *drawable) override;
	void		getGeometry(const LLViewerPart& part,							
								LLStrider<LLVector4a>& verticesp);
				
				void		getGeometry(S32 idx,
								LLStrider<LLVector4a>& verticesp,
								LLStrider<LLVector3>& normalsp, 
								LLStrider<LLVector2>& texcoordsp,
								LLStrider<LLColor4U>& colorsp, 
								LLStrider<LLColor4U>& emissivep,
								LLStrider<U16>& indicesp) override;

	void updateFaceSize(S32 idx) override { }
	F32 getPartSize(S32 idx) override;
	void getBlendFunc(S32 idx, U32& src, U32& dst) override;
	LLUUID getPartOwner(S32 idx);
	LLUUID getPartSource(S32 idx);

	void setViewerPartGroup(LLViewerPartGroup *part_groupp)		{ mViewerPartGroupp = part_groupp; }
	LLViewerPartGroup* getViewerPartGroup()	{ return mViewerPartGroupp; }

protected:
	~LLVOPartGroup();

	LLViewerPartGroup *mViewerPartGroupp;

	virtual LLVector3 getCameraPosition() const;

};


class LLVOHUDPartGroup : public LLVOPartGroup
{
public:
	LLVOHUDPartGroup(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp) : 
	  LLVOPartGroup(id, pcode, regionp)   
	{
	}
protected:
	LLDrawable* createDrawable(LLPipeline *pipeline) override;
	U32 getPartitionType() const override;
	LLVector3 getCameraPosition() const override;
};

#endif // LL_LLVOPARTGROUP_H
