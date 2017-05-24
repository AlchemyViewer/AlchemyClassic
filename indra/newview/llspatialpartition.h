/** 
 * @file llspatialpartition.h
 * @brief LLSpatialGroup header file including definitions for supporting functions
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
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

#ifndef LL_LLSPATIALPARTITION_H
#define LL_LLSPATIALPARTITION_H

#define SG_MIN_DIST_RATIO 0.00001f

#include "lldrawable.h"
#include "lloctree.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llvertexbuffer.h"
#include "llgltypes.h"
#include "llcubemap.h"
#include "lldrawpool.h"
#include "llface.h"
#include "llviewercamera.h"
#include "llvector4a.h"
#include <queue>

#include <boost/container/flat_map.hpp>

#define SG_STATE_INHERIT_MASK (OCCLUDED)
#define SG_INITIAL_STATE_MASK (DIRTY | GEOM_DIRTY)

class LLViewerOctreePartition;
class LLSpatialPartition;
class LLSpatialBridge;
class LLSpatialGroup;
class LLViewerRegion;

void pushVerts(LLFace* face, U32 mask);

class LLDrawInfo : public LLRefCount, public LLTrace::MemTrackableNonVirtual<LLDrawInfo, 16>
{
protected:
	~LLDrawInfo();	
	
public:
	LLDrawInfo(const LLDrawInfo& rhs)
	:	LLTrace::MemTrackableNonVirtual<LLDrawInfo, 16>("LLDrawInfo")
	{
		*this = rhs;
	}

	const LLDrawInfo& operator=(const LLDrawInfo& rhs)
	{
		LL_ERRS() << "Illegal operation!" << LL_ENDL;
		return *this;
	}

	LLDrawInfo(U16 start, U16 end, U32 count, U32 offset, 
				LLViewerTexture* image, LLVertexBuffer* buffer, 
				BOOL fullbright = FALSE, U8 bump = 0, BOOL particle = FALSE, F32 part_size = 0);
	

	void validate();

	LLVector4a mExtents[2];
	
	LLPointer<LLVertexBuffer> mVertexBuffer;
	LLPointer<LLViewerTexture>     mTexture;
	std::vector<LLPointer<LLViewerTexture> > mTextureList;

	LLColor4U mDebugColor;
	const LLMatrix4* mTextureMatrix;
	const LLMatrix4* mModelMatrix;
	U16 mStart;
	U16 mEnd;
	U32 mCount;
	U32 mOffset;
	BOOL mFullbright;
	U8 mBump;
	U8 mShiny;
	BOOL mParticle;
	F32 mPartSize;
	F32 mVSize;
	LLSpatialGroup* mGroup;
	LL_ALIGN_16(LLFace* mFace); //associated face
	F32 mDistance;
	U32 mDrawMode;
	LLMaterialPtr mMaterial; // If this is null, the following parameters are unused.
	LLMaterialID mMaterialID;
	U32 mShaderMask;
	U32 mBlendFuncSrc;
	U32 mBlendFuncDst;
	BOOL mHasGlow;
	LLPointer<LLViewerTexture> mSpecularMap;
	LLPointer<LLViewerTexture> mNormalMap;
	LLVector4 mSpecColor; // XYZ = Specular RGB, W = Specular Exponent
	F32  mEnvIntensity;
	F32  mAlphaMaskCutoff;
	U8   mDiffuseAlphaMode;


	struct CompareTexture
	{
		bool operator()(const LLDrawInfo& lhs, const LLDrawInfo& rhs)
		{
			return lhs.mTexture > rhs.mTexture;
		}
	};

	struct CompareTexturePtr
	{ //sort by texture
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs)	
		{
			// sort by pointer, sort NULL down to the end
			return lhs.get() != rhs.get() 
						&& (lhs.isNull() || (rhs.notNull() && lhs->mTexture.get() > rhs->mTexture.get()));
		}
	};

	struct CompareVertexBuffer
	{ //sort by texture
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs)	
		{
			// sort by pointer, sort NULL down to the end
			return lhs.get() != rhs.get() 
					&& (lhs.isNull() || (rhs.notNull() && lhs->mVertexBuffer.get() > rhs->mVertexBuffer.get()));
		}
	};

	struct CompareTexturePtrMatrix
	{
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs)	
		{
			return lhs.get() != rhs.get() 
						&& (lhs.isNull() || (rhs.notNull() && (lhs->mTexture.get() > rhs->mTexture.get() ||
															   (lhs->mTexture.get() == rhs->mTexture.get() && lhs->mModelMatrix > rhs->mModelMatrix))));
		}

	};

	struct CompareMatrixTexturePtr
	{
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs)	
		{
			return lhs.get() != rhs.get() 
				&& (lhs.isNull() || (rhs.notNull() && (lhs->mModelMatrix > rhs->mModelMatrix ||
													   (lhs->mModelMatrix == rhs->mModelMatrix && lhs->mTexture.get() > rhs->mTexture.get()))));
		}

	};

	struct CompareBump
	{
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs) 
		{
			// sort by mBump value, sort NULL down to the end
			return lhs.get() != rhs.get() 
						&& (lhs.isNull() || (rhs.notNull() && lhs->mBump > rhs->mBump));
		}
	};

	struct CompareDistanceGreater
	{
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs) 
		{
			// sort by mBump value, sort NULL down to the end
			return lhs.get() != rhs.get() 
						&& (lhs.isNull() || (rhs.notNull() && lhs->mDistance > rhs->mDistance));
		}
	};
};

LL_ALIGN_PREFIX(64)
class LLSpatialGroup : public LLOcclusionCullingGroup
{
	friend class LLSpatialPartition;
	friend class LLOctreeStateCheck;
public:

	// <alchemy>
	void* operator new(size_t size)
	{
		return ll_aligned_malloc<64>(size);
	}

	void operator delete(void* ptr)
	{
		ll_aligned_free<64>(ptr);
	}
	// </alchemy>

	LLSpatialGroup(const LLSpatialGroup& rhs) : LLOcclusionCullingGroup(rhs)
	{
		*this = rhs;
	}

	const LLSpatialGroup& operator=(const LLSpatialGroup& rhs)
	{
		LL_ERRS() << "Illegal operation!" << LL_ENDL;
		return *this;
	}

	static U32 sNodeCount;
	static BOOL sNoDelete; //deletion of spatial groups and draw info not allowed if TRUE

	typedef std::vector<LLPointer<LLSpatialGroup> > sg_vector_t;
	typedef std::vector<LLPointer<LLSpatialBridge> > bridge_list_t;
	typedef std::vector<LLPointer<LLDrawInfo> > drawmap_elem_t; 
	typedef boost::container::flat_map<U32, drawmap_elem_t > draw_map_t;	
	typedef std::vector<LLPointer<LLVertexBuffer> > buffer_list_t;
	typedef boost::container::flat_map<LLFace*, buffer_list_t> buffer_texture_map_t;
	typedef boost::container::flat_map<U32, buffer_texture_map_t> buffer_map_t;

	struct CompareDistanceGreater
	{
		bool operator()(const LLSpatialGroup* const& lhs, const LLSpatialGroup* const& rhs)
		{
			return lhs->mDistance > rhs->mDistance;
		}
	};

	struct CompareUpdateUrgency
	{
		bool operator()(const LLPointer<LLSpatialGroup> lhs, const LLPointer<LLSpatialGroup> rhs)
		{
			return lhs->getUpdateUrgency() > rhs->getUpdateUrgency();
		}
	};

	struct CompareDepthGreater
	{
		bool operator()(const LLSpatialGroup* const& lhs, const LLSpatialGroup* const& rhs)
		{
			return lhs->mDepth > rhs->mDepth;
		}
	};

	typedef enum
	{
		GEOM_DIRTY				= LLViewerOctreeGroup::INVALID_STATE,
		ALPHA_DIRTY				= (GEOM_DIRTY << 1),
		IN_IMAGE_QUEUE			= (ALPHA_DIRTY << 1),
		IMAGE_DIRTY				= (IN_IMAGE_QUEUE << 1),
		MESH_DIRTY				= (IMAGE_DIRTY << 1),
		NEW_DRAWINFO			= (MESH_DIRTY << 1),
		IN_BUILD_Q1				= (NEW_DRAWINFO << 1),
		IN_BUILD_Q2				= (IN_BUILD_Q1 << 1),
		STATE_MASK				= 0x0000FFFF,
	} eSpatialState;

	LLSpatialGroup(OctreeNode* node, LLSpatialPartition* part);

	BOOL isHUDGroup() ;
	
	void clearDrawMap();
	void validate();
	void validateDrawMap();
	
	void setState(U32 state, S32 mode);
	void clearState(U32 state, S32 mode);
	void clearState(U32 state)     {mState &= ~state;}		

	LLSpatialGroup* getParent();

	BOOL addObject(LLDrawable *drawablep);
	BOOL removeObject(LLDrawable *drawablep, BOOL from_octree = FALSE);
	BOOL updateInGroup(LLDrawable *drawablep, BOOL immediate = FALSE); // Update position if it's in the group
	void shift(const LLVector4a &offset);
	void destroyGL(bool keep_occlusion = false);
	
	void updateDistance(LLCamera& camera);
	F32 getUpdateUrgency() const;
	BOOL changeLOD();
	void rebuildGeom();
	void rebuildMesh();

	void setState(U32 state)       {mState |= state;}
	void dirtyGeom() { setState(GEOM_DIRTY); }
	void dirtyMesh() { setState(MESH_DIRTY); }

	void drawObjectBox(LLColor4 col);

	LLSpatialPartition* getSpatialPartition() {return (LLSpatialPartition*)mSpatialPartition;}

	 //LISTENER FUNCTIONS
	void handleInsertion(const TreeNode* node, LLViewerOctreeEntry* face) override;
	void handleRemoval(const TreeNode* node, LLViewerOctreeEntry* face) override;
	void handleDestruction(const TreeNode* node) override;
	void handleChildAddition(const OctreeNode* parent, OctreeNode* child) override;

public:
	LL_ALIGN_16(LLVector4a mViewAngle);
	LL_ALIGN_16(LLVector4a mLastUpdateViewAngle);

	F32 mObjectBoxSize; //cached mObjectBounds[1].getLength3()

protected:
	virtual ~LLSpatialGroup();

	static S32 sLODSeed;

public:
	bridge_list_t mBridgeList;
	buffer_map_t mBufferMap; //used by volume buffers to attempt to reuse vertex buffers

	U32 mGeometryBytes; //used by volumes to track how many bytes of geometry data are in this node
	F32 mSurfaceArea; //used by volumes to track estimated surface area of geometry in this node

	F32 mBuilt;
	
	LLPointer<LLVertexBuffer> mVertexBuffer;

	U32 mBufferUsage;
	draw_map_t mDrawMap;
	
	F32 mDistance;
	F32 mDepth;
	F32 mLastUpdateDistance;
	F32 mLastUpdateTime;
	
	F32 mPixelArea;
	F32 mRadius;
} LL_ALIGN_POSTFIX(64);

class LLGeometryManager
{
public:
	std::vector<LLFace*> mFaceList;
	virtual ~LLGeometryManager() { }
	virtual void rebuildGeom(LLSpatialGroup* group) = 0;
	virtual void rebuildMesh(LLSpatialGroup* group) = 0;
	virtual void getGeometry(LLSpatialGroup* group) = 0;
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32 &index_count);
	
	virtual LLVertexBuffer* createVertexBuffer(U32 type_mask, U32 usage);
};

class LLSpatialPartition: public LLViewerOctreePartition, public LLGeometryManager
{
public:
	LLSpatialPartition(U32 data_mask,  BOOL render_by_group, U32 mBufferUsage, LLViewerRegion* regionp);
	virtual ~LLSpatialPartition();

	LLSpatialGroup *put(LLDrawable *drawablep, BOOL was_visible = FALSE);
	BOOL remove(LLDrawable *drawablep, LLSpatialGroup *curp);
	
	LLDrawable* lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
									 BOOL pick_transparent, 
									 BOOL pick_rigged,
									 S32* face_hit,                          // return the face hit
									 LLVector4a* intersection = nullptr,         // return the intersection point
									 LLVector2* tex_coord = nullptr,            // return the texture coordinates of the intersection point
									 LLVector4a* normal = nullptr,               // return the surface normal at the intersection point
									 LLVector4a* tangent = nullptr             // return the surface tangent at the intersection point
		);
	
	
	// If the drawable moves, move it here.
	virtual void move(LLDrawable *drawablep, LLSpatialGroup *curp, BOOL immediate = FALSE);
	virtual void shift(const LLVector4a &offset);

	virtual F32 calcDistance(LLSpatialGroup* group, LLCamera& camera);
	virtual F32 calcPixelArea(LLSpatialGroup* group, LLCamera& camera);

	void rebuildGeom(LLSpatialGroup* group) override;
	void rebuildMesh(LLSpatialGroup* group) override;

	BOOL visibleObjectsInFrustum(LLCamera& camera);
	/*virtual*/ S32 cull(LLCamera &camera, bool do_occlusion=false) override; // Cull on arbitrary frustum
	S32 cull(LLCamera &camera, std::vector<LLDrawable *>* results, BOOL for_select); // Cull on arbitrary frustum
	
	BOOL isVisible(const LLVector3& v);
	bool isHUDPartition() ;
	
	LLSpatialBridge* asBridge() { return mBridge; }
	BOOL isBridge() { return asBridge() != nullptr; }

	void renderPhysicsShapes();
	void renderDebug();
	void renderIntersectingBBoxes(LLCamera* camera);
	void restoreGL();
	void resetVertexBuffers();

	BOOL getVisibleExtents(LLCamera& camera, LLVector3& visMin, LLVector3& visMax);

public:
	LLSpatialBridge* mBridge; // NULL for non-LLSpatialBridge instances, otherwise, mBridge == this
							// use a pointer instead of making "isBridge" and "asBridge" virtual so it's safe
							// to call asBridge() from the destructor
	
	BOOL mInfiniteFarClip; // if TRUE, frustum culling ignores far clip plane
	U32 mBufferUsage;
	const BOOL mRenderByGroup;
	U32 mVertexDataMask;
	F32 mSlopRatio; //percentage distance must change before drawables receive LOD update (default is 0.25);
	BOOL mDepthMask; //if TRUE, objects in this partition will be written to depth during alpha rendering

	static BOOL sTeleportRequested; //started to issue a teleport request
};

// class for creating bridges between spatial partitions
class LLSpatialBridge : public LLDrawable, public LLSpatialPartition
{
protected:
	~LLSpatialBridge();

public:
	typedef std::vector<LLPointer<LLSpatialBridge> > bridge_vector_t;
	
	LLSpatialBridge(LLDrawable* root, BOOL render_by_group, U32 data_mask, LLViewerRegion* regionp);
	
	void destroyTree();

	BOOL isSpatialBridge() const override { return TRUE; }
	void updateSpatialExtents() override;
	void updateBinRadius() override;
	void setVisible(LLCamera& camera_in, std::vector<LLDrawable*>* results = nullptr, BOOL for_select = FALSE) override;
	void updateDistance(LLCamera& camera_in, bool force_update) override;
	void makeActive() override;
	void move(LLDrawable *drawablep, LLSpatialGroup *curp, BOOL immediate = FALSE) override;
	BOOL updateMove() override;
	void shiftPos(const LLVector4a& vec) override;
	void cleanupReferences() override;
	LLSpatialPartition* asPartition() override { return this; }
		
	virtual LLCamera transformCamera(LLCamera& camera);
	
	LLDrawable* mDrawable;
	LLPointer<LLVOAvatar> mAvatar;

};

class LLCullResult 
{
public:
	LLCullResult();

	typedef std::vector<LLSpatialGroup*> sg_list_t;
	typedef std::vector<LLDrawable*> drawable_list_t;
	typedef std::vector<LLSpatialBridge*> bridge_list_t;
	typedef std::vector<LLDrawInfo*> drawinfo_list_t;

	typedef LLSpatialGroup** sg_iterator;
	typedef LLSpatialBridge** bridge_iterator;
	typedef LLDrawInfo** drawinfo_iterator;
	typedef LLDrawable** drawable_iterator;

	void clear();
	
	sg_iterator beginVisibleGroups();
	sg_iterator endVisibleGroups();

	sg_iterator beginAlphaGroups();
	sg_iterator endAlphaGroups();

	bool hasOcclusionGroups() { return mOcclusionGroupsSize > 0; }
	sg_iterator beginOcclusionGroups();
	sg_iterator endOcclusionGroups();

	sg_iterator beginDrawableGroups();
	sg_iterator endDrawableGroups();

	drawable_iterator beginVisibleList();
	drawable_iterator endVisibleList();

	bridge_iterator beginVisibleBridge();
	bridge_iterator endVisibleBridge();

	drawinfo_iterator beginRenderMap(U32 type);
	drawinfo_iterator endRenderMap(U32 type);

	void pushVisibleGroup(LLSpatialGroup* group);
	void pushAlphaGroup(LLSpatialGroup* group);
	void pushOcclusionGroup(LLSpatialGroup* group);
	void pushDrawableGroup(LLSpatialGroup* group);
	void pushDrawable(LLDrawable* drawable);
	void pushBridge(LLSpatialBridge* bridge);
	void pushDrawInfo(U32 type, LLDrawInfo* draw_info);
	
	U32 getVisibleGroupsSize()		{ return mVisibleGroupsSize; }
	U32	getAlphaGroupsSize()		{ return mAlphaGroupsSize; }
	U32	getDrawableGroupsSize()		{ return mDrawableGroupsSize; }
	U32	getVisibleListSize()		{ return mVisibleListSize; }
	U32	getVisibleBridgeSize()		{ return mVisibleBridgeSize; }
	U32	getRenderMapSize(U32 type)	{ return mRenderMapSize[type]; }

	void assertDrawMapsEmpty();

private:

	template <class T, class V> void pushBack(T &head, U32& count, V* val);

	U32					mVisibleGroupsSize;
	U32					mAlphaGroupsSize;
	U32					mOcclusionGroupsSize;
	U32					mDrawableGroupsSize;
	U32					mVisibleListSize;
	U32					mVisibleBridgeSize;

	U32					mVisibleGroupsAllocated;
	U32					mAlphaGroupsAllocated;
	U32					mOcclusionGroupsAllocated;
	U32					mDrawableGroupsAllocated;
	U32					mVisibleListAllocated;
	U32					mVisibleBridgeAllocated;

	U32					mRenderMapSize[LLRenderPass::NUM_RENDER_TYPES];

	sg_list_t			mVisibleGroups;
	sg_iterator			mVisibleGroupsEnd;
	sg_list_t			mAlphaGroups;
	sg_iterator			mAlphaGroupsEnd;
	sg_list_t			mOcclusionGroups;
	sg_iterator			mOcclusionGroupsEnd;
	sg_list_t			mDrawableGroups;
	sg_iterator			mDrawableGroupsEnd;
	drawable_list_t		mVisibleList;
	drawable_iterator	mVisibleListEnd;
	bridge_list_t		mVisibleBridge;
	bridge_iterator		mVisibleBridgeEnd;
	drawinfo_list_t		mRenderMap[LLRenderPass::NUM_RENDER_TYPES];
	U32					mRenderMapAllocated[LLRenderPass::NUM_RENDER_TYPES];
	drawinfo_iterator mRenderMapEnd[LLRenderPass::NUM_RENDER_TYPES];

};


//spatial partition for water (implemented in LLVOWater.cpp)
class LLWaterPartition : public LLSpatialPartition
{
public:
	LLWaterPartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override {  }
	void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) override { }
};

//spatial partition for hole and edge water (implemented in LLVOWater.cpp)
class LLVoidWaterPartition : public LLWaterPartition
{
public:
	LLVoidWaterPartition(LLViewerRegion* regionp);
};

//spatial partition for terrain (impelmented in LLVOSurfacePatch.cpp)
class LLTerrainPartition : public LLSpatialPartition
{
public:
	LLTerrainPartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override;
	LLVertexBuffer* createVertexBuffer(U32 type_mask, U32 usage) override;
};

//spatial partition for trees
class LLTreePartition : public LLSpatialPartition
{
public:
	LLTreePartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override { }
	void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) override { }

};

//spatial partition for particles (implemented in LLVOPartGroup.cpp)
class LLParticlePartition : public LLSpatialPartition
{
public:
	LLParticlePartition(LLViewerRegion* regionp);
	void rebuildGeom(LLSpatialGroup* group) override;
	void getGeometry(LLSpatialGroup* group) override;
	void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) override;
	F32 calcPixelArea(LLSpatialGroup* group, LLCamera& camera) override;
protected:
	U32 mRenderPass;
};

class LLHUDParticlePartition : public LLParticlePartition
{
public:
	LLHUDParticlePartition(LLViewerRegion* regionp);
};

//spatial partition for grass (implemented in LLVOGrass.cpp)
class LLGrassPartition : public LLSpatialPartition
{
public:
	LLGrassPartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override;
	void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) override;
protected:
	U32 mRenderPass;
};

//class for wrangling geometry out of volumes (implemented in LLVOVolume.cpp)
class LLVolumeGeometryManager: public LLGeometryManager
{
 public:
	typedef enum
	{
		NONE = 0,
		BATCH_SORT,
		DISTANCE_SORT
	} eSortType;

	LLVolumeGeometryManager();
	virtual ~LLVolumeGeometryManager();
	void rebuildGeom(LLSpatialGroup* group) override;
	void rebuildMesh(LLSpatialGroup* group) override;
	void getGeometry(LLSpatialGroup* group) override;
	void genDrawInfo(LLSpatialGroup* group, U32 mask, LLFace** faces, U32 face_count, BOOL distance_sort = FALSE, BOOL batch_textures = FALSE);
	void registerFace(LLSpatialGroup* group, LLFace* facep, U32 type);

private:
	void allocateFaces(U32 pMaxFaceCount);
	void freeFaces();

	static int32_t sInstanceCount;
	static LLFace** sFullbrightFaces;
	static LLFace** sBumpFaces;
	static LLFace** sSimpleFaces;
	static LLFace** sNormFaces;
	static LLFace** sSpecFaces;
	static LLFace** sNormSpecFaces;
	static LLFace** sAlphaFaces;
};

//spatial partition that uses volume geometry manager (implemented in LLVOVolume.cpp)
class LLVolumePartition : public LLSpatialPartition, public LLVolumeGeometryManager
{
public:
	LLVolumePartition(LLViewerRegion* regionp);
	void rebuildGeom(LLSpatialGroup* group) override { LLVolumeGeometryManager::rebuildGeom(group); }
	void getGeometry(LLSpatialGroup* group) override { LLVolumeGeometryManager::getGeometry(group); }
	void rebuildMesh(LLSpatialGroup* group) override { LLVolumeGeometryManager::rebuildMesh(group); }
	void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) override { LLVolumeGeometryManager::addGeometryCount(group, vertex_count, index_count); }
};

//spatial bridge that uses volume geometry manager (implemented in LLVOVolume.cpp)
class LLVolumeBridge : public LLSpatialBridge, public LLVolumeGeometryManager
{
public:
	LLVolumeBridge(LLDrawable* drawable, LLViewerRegion* regionp);
	void rebuildGeom(LLSpatialGroup* group) override { LLVolumeGeometryManager::rebuildGeom(group); }
	void getGeometry(LLSpatialGroup* group) override { LLVolumeGeometryManager::getGeometry(group); }
	void rebuildMesh(LLSpatialGroup* group) override { LLVolumeGeometryManager::rebuildMesh(group); }
	void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) override { LLVolumeGeometryManager::addGeometryCount(group, vertex_count, index_count); }
};

class LLHUDBridge : public LLVolumeBridge
{
public:
	LLHUDBridge(LLDrawable* drawablep, LLViewerRegion* regionp);
	void shiftPos(const LLVector4a& vec) override;
	F32 calcPixelArea(LLSpatialGroup* group, LLCamera& camera) override;
};

//spatial partition that holds nothing but spatial bridges
class LLBridgePartition : public LLSpatialPartition
{
public:
	LLBridgePartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override { }
	void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) override {  }
};

class LLHUDPartition : public LLBridgePartition
{
public:
	LLHUDPartition(LLViewerRegion* regionp);
	void shift(const LLVector4a &offset) override;
};

extern const F32 SG_BOX_SIDE;
extern const F32 SG_BOX_OFFSET;
extern const F32 SG_BOX_RAD;

extern const F32 SG_OBJ_SIDE;
extern const F32 SG_MAX_OBJ_RAD;


#endif //LL_LLSPATIALPARTITION_H
