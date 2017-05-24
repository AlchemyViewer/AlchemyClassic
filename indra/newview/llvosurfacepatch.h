/** 
 * @file llvosurfacepatch.h
 * @brief Description of LLVOSurfacePatch class
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

#ifndef LL_VOSURFACEPATCH_H
#define LL_VOSURFACEPATCH_H

#include "llviewerobject.h"
#include "llstrider.h"

class LLSurfacePatch;
class LLDrawPool;
class LLVector2;
class LLFacePool;
class LLFace;

class LLVOSurfacePatch : public LLStaticViewerObject
{
public:
	static F32 sLODFactor;

	enum
	{
		VERTEX_DATA_MASK =	(1 << LLVertexBuffer::TYPE_VERTEX) |
							(1 << LLVertexBuffer::TYPE_NORMAL) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD0) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD1)
	};

	LLVOSurfacePatch(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp);

	/*virtual*/ void markDead() override;

	// Initialize data that's only inited once per class.
	static void initClass();

	U32 getPartitionType() const override;

	/*virtual*/ LLDrawable* createDrawable(LLPipeline *pipeline) override;
	/*virtual*/ void		updateGL() override;
	/*virtual*/ BOOL        updateGeometry(LLDrawable *drawable) override;
	/*virtual*/ BOOL		updateLOD() override;
	/*virtual*/ void		updateFaceSize(S32 idx) override;
	void getGeometry(LLStrider<LLVector3> &verticesp,
								LLStrider<LLVector3> &normalsp,
								LLStrider<LLVector2> &texCoords0p,
								LLStrider<LLVector2> &texCoords1p,
								LLStrider<U16> &indicesp);

	/*virtual*/ void updateTextures() override;
	/*virtual*/ void setPixelAreaAndAngle(LLAgent &agent) override; // generate accurate apparent angle and area

	/*virtual*/ void updateSpatialExtents(LLVector4a& newMin, LLVector4a& newMax) override;
	/*virtual*/ BOOL isActive() const override; // Whether this object needs to do an idleUpdate.

	void setPatch(LLSurfacePatch *patchp);
	LLSurfacePatch	*getPatch() const		{ return mPatchp; }

	void dirtyPatch();
	void dirtyGeom();

	/*virtual*/ BOOL lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end, 
										  S32 face = -1,                        // which face to check, -1 = ALL_SIDES
										  BOOL pick_transparent = FALSE,
										  BOOL pick_rigged = FALSE,
										  S32* face_hit = nullptr,                 // which face was hit
										  LLVector4a* intersection = nullptr,       // return the intersection point
										  LLVector2* tex_coord = nullptr,          // return the texture coordinates of the intersection point
										  LLVector4a* normal = nullptr,             // return the surface normal at the intersection point
										  LLVector4a* tangent = nullptr           // return the surface tangent at the intersection point
		) override;

	BOOL			mDirtiedPatch;
protected:
	~LLVOSurfacePatch();

	LLFacePool		*mPool;
	LLFacePool		*getPool();
	S32				mBaseComp;
	LLSurfacePatch	*mPatchp;
	BOOL			mDirtyTexture;
	BOOL			mDirtyTerrain;

	S32				mLastNorthStride;
	S32				mLastEastStride;
	S32				mLastStride;
	S32				mLastLength;

	void getGeomSizesMain(const S32 stride, S32 &num_vertices, S32 &num_indices);
	void getGeomSizesNorth(const S32 stride, const S32 north_stride,
								  S32 &num_vertices, S32 &num_indices);
	void getGeomSizesEast(const S32 stride, const S32 east_stride,
								 S32 &num_vertices, S32 &num_indices);

	void updateMainGeometry(LLFace *facep,
					   LLStrider<LLVector3> &verticesp,
					   LLStrider<LLVector3> &normalsp,
					   LLStrider<LLVector2> &texCoords0p,
					   LLStrider<LLVector2> &texCoords1p,
					   LLStrider<U16> &indicesp,
					   U32 &index_offset);
	void updateNorthGeometry(LLFace *facep,
					   LLStrider<LLVector3> &verticesp,
					   LLStrider<LLVector3> &normalsp,
					   LLStrider<LLVector2> &texCoords0p,
					   LLStrider<LLVector2> &texCoords1p,
					   LLStrider<U16> &indicesp,
					   U32 &index_offset);
	void updateEastGeometry(LLFace *facep,
					   LLStrider<LLVector3> &verticesp,
					   LLStrider<LLVector3> &normalsp,
					   LLStrider<LLVector2> &texCoords0p,
					   LLStrider<LLVector2> &texCoords1p,
					   LLStrider<U16> &indicesp,
					   U32 &index_offset);
};

#endif // LL_VOSURFACEPATCH_H
