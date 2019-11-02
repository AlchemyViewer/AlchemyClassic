/** 
 * @file llmodel.cpp
 * @brief Model handling implementation
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

#include "llmodel.h"
#include "llmemory.h"
#include "llconvexdecomposition.h"
#include "llsdserialize.h"
#include "llvector4a.h"

#ifdef LL_USESYSTEMLIBS
# include <zlib.h>
#else
# include <zlib/zlib.h>
#endif

std::string model_names[] =
{
	"lowest_lod",
	"low_lod",
	"medium_lod",
	"high_lod",
	"physics_mesh"
};

const int MODEL_NAMES_LENGTH = sizeof(model_names) / sizeof(std::string);

LLModel::LLModel(LLVolumeParams& params, F32 detail)
	: LLVolume(params, detail),
	  mNormalizedScale(1, 1, 1),
	  mNormalizedTranslation(0, 0, 0),
	  mPelvisOffset(0.0f), 
	  mHullPoints(0),
	  mStatus(NO_ERRORS),
	  mSubmodelID(0)
{
	mDecompID = -1;
	mLocalID = -1;
}

LLModel::~LLModel()
{
	if (mDecompID >= 0)
	{
		LLConvexDecomposition::getInstance()->deleteDecomposition(mDecompID);
	}
}

//static
std::string LLModel::getStatusString(U32 status)
{
	const static std::string status_strings[(S32)INVALID_STATUS] = {"status_no_error", "status_vertex_number_overflow","bad_element"};

	if(status < INVALID_STATUS)
	{
		if(status_strings[status].empty())
		{
			//LL_ERRS() << "No valid status string for this status: " << (U32)status << LL_ENDL();
		}
		return status_strings[status] ;
	}

	//LL_ERRS() << "Invalid model status: " << (U32)status << LL_ENDL();

	return std::string() ;
}


void LLModel::offsetMesh( const LLVector3& pivotPoint )
{
	LLVector4a pivot( pivotPoint[VX], pivotPoint[VY], pivotPoint[VZ] );
	
	for (auto faceIt = mVolumeFaces.begin(); faceIt != mVolumeFaces.end(); )
	{
        auto currentFaceIt = faceIt++;
		LLVolumeFace& face = *currentFaceIt;
        auto*pos = static_cast<LLVector4a*>(face.mPositions);
		
		for (auto i=0; i<face.mNumVertices; ++i )
		{
			pos[i].add( pivot );
		}
	}
}

void LLModel::optimizeVolumeFaces()
{
	for (auto i = 0; i < getNumVolumeFaces(); ++i)
	{
		mVolumeFaces[i].optimize();
	}
}

struct MaterialBinding
{
	int				index;
	std::string		matName;
};

struct MaterialSort
{
	bool operator()(const MaterialBinding& lhs, const MaterialBinding& rhs) const
    {
		return LLStringUtil::compareInsensitive(lhs.matName, rhs.matName) < 0;
	}
};

void LLModel::sortVolumeFacesByMaterialName()
{
	std::vector<MaterialBinding> bindings;
	bindings.resize(mVolumeFaces.size());

	for (auto i = 0; i < bindings.size(); i++)
	{
		bindings[i].index = i;
		if(i < mMaterialList.size())
		{
			bindings[i].matName = mMaterialList[i];
		}
	}
	std::sort(bindings.begin(), bindings.end(), MaterialSort());
	std::vector< LLVolumeFace > new_faces;

	// remap the faces to be in the same order the mats now are...
	//
	new_faces.resize(bindings.size());
	for (auto i = 0; i < bindings.size(); i++)
	{
		new_faces[i] = mVolumeFaces[bindings[i].index];
		if(i < mMaterialList.size())
		{
			mMaterialList[i] = bindings[i].matName;
		}
	}

	mVolumeFaces = std::move(new_faces);	
}

void LLModel::trimVolumeFacesToSize(U32 new_count, LLVolume::face_list_t* remainder)
{
	llassert(new_count <= LL_SCULPT_MESH_MAX_FACES);

	if (new_count && (getNumVolumeFaces() > static_cast<S32>(new_count)))
	{
		// Copy out remaining volume faces for alternative handling, if provided
		//
		if (remainder)
		{
			(*remainder).assign(mVolumeFaces.begin() + new_count, mVolumeFaces.end());
		}		

		// Trim down to the final set of volume faces (now stuffed to the gills!)
		//
		mVolumeFaces.resize(new_count);
	}
}

// Shrink the model to fit
// on a 1x1x1 cube centered at the origin.
// The positions and extents
// multiplied by  mNormalizedScale
// and offset by mNormalizedTranslation
// to be the "original" extents and position.
// Also, the positions will fit
// within the unit cube.
void LLModel::normalizeVolumeFaces()
{
	if (!mVolumeFaces.empty())
	{
		LLVector4a min, max;
		
		// For all of the volume faces
		// in the model, loop over
		// them and see what the extents
		// of the volume along each axis.
		min = mVolumeFaces[0].mExtents[0];
		max = mVolumeFaces[0].mExtents[1];

		for (auto i = 1; i < mVolumeFaces.size(); ++i)
		{
			LLVolumeFace& face = mVolumeFaces[i];

			update_min_max(min, max, face.mExtents[0]);
			update_min_max(min, max, face.mExtents[1]);

			if (face.mTexCoords)
			{
				LLVector2& min_tc = face.mTexCoordExtents[0];
				LLVector2& max_tc = face.mTexCoordExtents[1];

				min_tc = face.mTexCoords[0];
				max_tc = face.mTexCoords[0];

				for (auto j = 1; j < face.mNumVertices; ++j)
				{
					update_min_max(min_tc, max_tc, face.mTexCoords[j]);
				}
			}
			else
			{
				face.mTexCoordExtents[0].set(0,0);
				face.mTexCoordExtents[1].set(1,1);
			}
		}

		// Now that we have the extents of the model
		// we can compute the offset needed to center
		// the model at the origin.

		// Compute center of the model
		// and make it negative to get translation
		// needed to center at origin.
		LLVector4a trans;
		trans.setAdd(min, max);
		trans.mul(-0.5f);

		// Compute the total size along all
		// axes of the model.
		LLVector4a size;
		size.setSub(max, min);

		// Prevent division by zero.
		F32 x = size[0];
		F32 y = size[1];
		F32 z = size[2];
		F32 w = size[3];
		if (fabs(x)<F_APPROXIMATELY_ZERO)
		{
			x = 1.0;
		}
		if (fabs(y)<F_APPROXIMATELY_ZERO)
		{
			y = 1.0;
		}
		if (fabs(z)<F_APPROXIMATELY_ZERO)
		{
			z = 1.0;
		}
		size.set(x,y,z,w);

		// Compute scale as reciprocal of size
		LLVector4a scale;
		scale.splat(1.f);
		scale.div(size);

		LLVector4a inv_scale(1.f);
		inv_scale.div(scale);

		for (auto& face : mVolumeFaces)
        {
            // We shrink the extents so
			// that they fall within
			// the unit cube.
			// VFExtents change
			face.mExtents[0].add(trans);
			face.mExtents[0].mul(scale);

			face.mExtents[1].add(trans);
			face.mExtents[1].mul(scale);

			// For all the positions, we scale
			// the positions to fit within the unit cube.
            auto* pos = static_cast<LLVector4a*>(face.mPositions);
            auto* norm = static_cast<LLVector4a*>(face.mNormals);

			for (auto j = 0; j < face.mNumVertices; ++j)
			{
			 	pos[j].add(trans);
				pos[j].mul(scale);
				if (norm && !norm[j].equals3(LLVector4a::getZero()))
				{
					norm[j].mul(inv_scale);
					norm[j].normalize3();
				}
			}
		}

		// mNormalizedScale is the scale at which
		// we would need to multiply the model
		// by to get the original size of the
		// model instead of the normalized size.
		LLVector4a normalized_scale;
		normalized_scale.splat(1.f);
		normalized_scale.div(scale);
		mNormalizedScale.set(normalized_scale.getF32ptr());
		mNormalizedTranslation.set(trans.getF32ptr());
		mNormalizedTranslation *= -1.f; 
	}
}

void LLModel::getNormalizedScaleTranslation(LLVector3& scale_out, LLVector3& translation_out) const
{
	scale_out = mNormalizedScale;
	translation_out = mNormalizedTranslation;
}

LLVector3 LLModel::getTransformedCenter(const LLMatrix4& mat)
{
	LLVector3 ret;

	if (!mVolumeFaces.empty())
	{
		LLMatrix4a m;
		m.loadu(mat);

		LLVector4a minv,maxv;

		LLVector4a t;
		m.affineTransform(mVolumeFaces[0].mPositions[0], t);
		minv = maxv = t;

		for (auto& face : mVolumeFaces)
        {
            for (auto j = 0; j < face.mNumVertices; ++j)
			{
				m.affineTransform(face.mPositions[j],t);
				update_min_max(minv, maxv, t);
			}
		}

		minv.add(maxv);
		minv.mul(0.5f);

		ret.set(minv.getF32ptr());
	}

	return ret;
}



void LLModel::setNumVolumeFaces(S32 count)
{
	mVolumeFaces.resize(count);
}

void LLModel::setVolumeFaceData(
	S32 f, 
	LLStrider<LLVector3> pos, 
	LLStrider<LLVector3> norm, 
	LLStrider<LLVector2> tc, 
	LLStrider<U16> ind, 
	U32 num_verts, 
	U32 num_indices)
{
	LLVolumeFace& face = mVolumeFaces[f];

	face.resizeVertices(num_verts);
	face.resizeIndices(num_indices);

	LLVector4a::memcpyNonAliased16(reinterpret_cast<F32*>(face.mPositions), 
                                   reinterpret_cast<F32*>(pos.get()), 
                                   num_verts*4*sizeof(F32));
	if (norm.get())
	{
		LLVector4a::memcpyNonAliased16(reinterpret_cast<F32*>(face.mNormals), 
                                       reinterpret_cast<F32*>(norm.get()), 
                                       num_verts*4*sizeof(F32));
	}
	else
	{
		//ll_aligned_free_16(face.mNormals);
		face.mNormals = nullptr;
	}

	if (tc.get())
	{
		U32 tex_size = (num_verts*2*sizeof(F32)+0xF)&~0xF;
		LLVector4a::memcpyNonAliased16(reinterpret_cast<F32*>(face.mTexCoords), 
                                       reinterpret_cast<F32*>(tc.get()), tex_size);
	}
	else
	{
		//ll_aligned_free_16(face.mTexCoords);
		face.mTexCoords = nullptr;
	}

    auto size = (num_indices*2+0xF)&~0xF;
	LLVector4a::memcpyNonAliased16(reinterpret_cast<F32*>(face.mIndices), 
                                   reinterpret_cast<F32*>(ind.get()), size);
}

void LLModel::addFace(const LLVolumeFace& face)
{
	if (face.mNumVertices == 0)
	{
		LL_ERRS() << "Cannot add empty face." << LL_ENDL;
	}

	mVolumeFaces.push_back(face);

	if (mVolumeFaces.size() > MAX_MODEL_FACES)
	{
		LL_ERRS() << "Model prims cannot have more than " << MAX_MODEL_FACES << " faces!" << LL_ENDL;
	}
}


void LLModel::generateNormals(F32 angle_cutoff)
{
	//generate normals for all faces by:
	// 1 - Create faceted copy of face with no texture coordinates
	// 2 - Weld vertices in faceted copy that are shared between triangles with less than "angle_cutoff" difference between normals
	// 3 - Generate smoothed set of normals based on welding results
	// 4 - Create faceted copy of face with texture coordinates
	// 5 - Copy smoothed normals to faceted copy, using closest normal to triangle normal where more than one normal exists for a given position
	// 6 - Remove redundant vertices from new faceted (now smooth) copy

	angle_cutoff = cosf(angle_cutoff);
	for (auto& vol_face : mVolumeFaces)
    {
		if (vol_face.mNumIndices > 65535)
		{
			LL_WARNS() << "Too many vertices for normal generation to work." << LL_ENDL;
			continue;
		}

		//create faceted copy of current face with no texture coordinates (step 1)
		LLVolumeFace faceted;

        LLVector4a* src_pos = static_cast<LLVector4a*>(vol_face.mPositions);
		//LLVector4a* src_norm = (LLVector4a*) vol_face.mNormals;


		faceted.resizeVertices(vol_face.mNumIndices);
		faceted.resizeIndices(vol_face.mNumIndices);
		//bake out triangles into temporary face, clearing texture coordinates
		for (auto i = 0; i < vol_face.mNumIndices; ++i)
		{
			auto idx = vol_face.mIndices[i];
		
			faceted.mPositions[i] = src_pos[idx];
			faceted.mTexCoords[i] = LLVector2(0,0);
			faceted.mIndices[i] = i;
		}

		//generate normals for temporary face
		for (auto i = 0; i < faceted.mNumIndices; i += 3)
		{ //for each triangle
            auto i0 = faceted.mIndices[i+0];
            auto i1 = faceted.mIndices[i+1];
            auto i2 = faceted.mIndices[i+2];
			
            auto& p0 = faceted.mPositions[i0];
            auto& p1 = faceted.mPositions[i1];
            auto& p2 = faceted.mPositions[i2];

            auto& n0 = faceted.mNormals[i0];
            auto& n1 = faceted.mNormals[i1];
            auto& n2 = faceted.mNormals[i2];

			LLVector4a lhs, rhs;
			lhs.setSub(p1, p0);
			rhs.setSub(p2, p0);

			n0.setCross3(lhs, rhs);
			n0.normalize3();
			n1 = n0;
			n2 = n0;
		}

		//weld vertices in temporary face, respecting angle_cutoff (step 2)
		faceted.optimize(angle_cutoff);

		//generate normals for welded face based on new topology (step 3)

		for (auto i = 0; i < faceted.mNumVertices; i++)
		{
			faceted.mNormals[i].clear();
		}

		for (auto i = 0; i < faceted.mNumIndices; i += 3)
		{ //for each triangle
            auto i0 = faceted.mIndices[i+0];
            auto i1 = faceted.mIndices[i+1];
            auto i2 = faceted.mIndices[i+2];
			
            auto& p0 = faceted.mPositions[i0];
            auto& p1 = faceted.mPositions[i1];
            auto& p2 = faceted.mPositions[i2];

            auto& n0 = faceted.mNormals[i0];
            auto& n1 = faceted.mNormals[i1];
            auto& n2 = faceted.mNormals[i2];

			LLVector4a lhs, rhs;
			lhs.setSub(p1, p0);
			rhs.setSub(p2, p0);

			LLVector4a n;
			n.setCross3(lhs, rhs);

			n0.add(n);
			n1.add(n);
			n2.add(n);
		}

		//normalize normals and build point map
		LLVolumeFace::VertexMapData::PointMap point_map;

		for (auto i = 0; i < faceted.mNumVertices; ++i)
		{
			faceted.mNormals[i].normalize3();

			LLVolumeFace::VertexMapData v;
			v.setPosition(faceted.mPositions[i]);
			v.setNormal(faceted.mNormals[i]);

			point_map[LLVector3(v.getPosition().getF32ptr())].push_back(v);
		}

		//create faceted copy of current face with texture coordinates (step 4)
		LLVolumeFace new_face;

		//bake out triangles into new face
		new_face.resizeIndices(vol_face.mNumIndices);
		new_face.resizeVertices(vol_face.mNumIndices);
		
		for (auto i = 0; i < vol_face.mNumIndices; ++i)
		{
			U32 idx = vol_face.mIndices[i];
			LLVolumeFace::VertexData v;
			new_face.mPositions[i] = vol_face.mPositions[idx];
			new_face.mNormals[i].clear();
			new_face.mIndices[i] = i;
		}

		if (vol_face.mTexCoords)
		{
			for (auto i = 0; i < vol_face.mNumIndices; i++)
			{
				U32 idx = vol_face.mIndices[i];
				new_face.mTexCoords[i] = vol_face.mTexCoords[idx];
			}
		}
		else
		{
			//ll_aligned_free_16(new_face.mTexCoords);
			new_face.mTexCoords = nullptr;
		}

		//generate normals for new face
		for (auto i = 0; i < new_face.mNumIndices; i += 3)
		{ //for each triangle
			U16 i0 = new_face.mIndices[i+0];
			U16 i1 = new_face.mIndices[i+1];
			U16 i2 = new_face.mIndices[i+2];
			
			LLVector4a& p0 = new_face.mPositions[i0];
			LLVector4a& p1 = new_face.mPositions[i1];
			LLVector4a& p2 = new_face.mPositions[i2];

			LLVector4a& n0 = new_face.mNormals[i0];
			LLVector4a& n1 = new_face.mNormals[i1];
			LLVector4a& n2 = new_face.mNormals[i2];

			LLVector4a lhs, rhs;
			lhs.setSub(p1, p0);
			rhs.setSub(p2, p0);

			n0.setCross3(lhs, rhs);
			n0.normalize3();
			n1 = n0;
			n2 = n0;
		}

		//swap out normals in new_face with best match from point map (step 5)
		for (auto i = 0; i < new_face.mNumVertices; ++i)
		{
			//LLVolumeFace::VertexData v = new_face.mVertices[i];

			LLVector4a ref_norm = new_face.mNormals[i];

            auto iter = point_map.find(LLVector3(new_face.mPositions[i].getF32ptr()));

			if (iter != point_map.end())
			{
				F32 best = -2.f;
				for (auto& k : iter->second)
                {
					LLVector4a& n = k.getNormal();

					F32 cur = n.dot3(ref_norm).getF32();

					if (cur > best)
					{
						best = cur;
						new_face.mNormals[i] = n;
					}
				}
			}
		}
		
		//remove redundant vertices from new face (step 6)
		new_face.optimize();

        vol_face = new_face;
	}
}


std::string LLModel::getName() const
{
    return mRequestedLabel.empty() ? mLabel : mRequestedLabel;
}

//static
LLSD LLModel::writeModel(
	std::ostream& ostr,
	LLModel* physics,
	LLModel* high,
	LLModel* medium,
	LLModel* low,
	LLModel* impostor,
	const LLModel::Decomposition& decomp,
	BOOL upload_skin,
	BOOL upload_joints,
    BOOL lock_scale_if_joint_position,
	BOOL nowrite,
	BOOL as_slm,
	int submodel_id)
{
	LLSD mdl;

	LLModel* model[] = 
	{
		impostor,
		low,
		medium,
		high,
		physics
	};

	bool skinning = upload_skin && high && !high->mSkinWeights.empty();

	if (skinning)
	{ //write skinning block
		mdl["skin"] = high->mSkinInfo.asLLSD(upload_joints, lock_scale_if_joint_position);
	}

	if (!decomp.mBaseHull.empty() ||
		!decomp.mHull.empty())		
	{
		mdl["physics_convex"] = decomp.asLLSD();
		if (!decomp.mHull.empty() && !as_slm)
		{ //convex decomposition exists, physics mesh will not be used (unless this is an slm file)
			model[LLModel::LOD_PHYSICS] = nullptr;
		}
	}
	else if (submodel_id)
	{
		const LLModel::Decomposition fake_decomp;
		mdl["secondary"] = true;
        mdl["submodel_id"] = submodel_id;
		mdl["physics_convex"] = fake_decomp.asLLSD();
		model[LLModel::LOD_PHYSICS] = nullptr;
	}

	if (as_slm)
	{ //save material list names
		for (auto i = 0; i < high->mMaterialList.size(); ++i)
		{
			mdl["material_list"][i] = high->mMaterialList[i];
		}
	}

	for (auto idx = 0; idx < MODEL_NAMES_LENGTH; ++idx)
	{
		if (model[idx] && (model[idx]->getNumVolumeFaces() > 0) && model[idx]->getVolumeFace(0).mPositions != nullptr)
		{
			LLVector3 min_pos = LLVector3(model[idx]->getVolumeFace(0).mPositions[0].getF32ptr());
			LLVector3 max_pos = min_pos;

			//find position domain
			for (auto i = 0; i < model[idx]->getNumVolumeFaces(); ++i)
			{ //for each face
				const LLVolumeFace& face = model[idx]->getVolumeFace(i);
				for (auto j = 0; j < face.mNumVertices; ++j)
				{
					update_min_max(min_pos, max_pos, face.mPositions[j].getF32ptr());
				}
			}

			LLVector3 pos_range = max_pos - min_pos;

			for (auto i = 0; i < model[idx]->getNumVolumeFaces(); ++i)
			{ //for each face
				const LLVolumeFace& face = model[idx]->getVolumeFace(i);
				if (face.mNumVertices < 3)
				{ //don't export an empty face
					mdl[model_names[idx]][i]["NoGeometry"] = true;
					continue;
				}
				LLSD::Binary verts(face.mNumVertices*3*2);
				LLSD::Binary tc(face.mNumVertices*2*2);
				LLSD::Binary normals(face.mNumVertices*3*2);
				LLSD::Binary indices(face.mNumIndices*2);

				U32 vert_idx = 0;
				U32 norm_idx = 0;
				U32 tc_idx = 0;
			
				LLVector2* ftc = static_cast<LLVector2*>(face.mTexCoords);
				LLVector2 min_tc;
				LLVector2 max_tc;

				if (ftc)
				{
					min_tc = ftc[0];
					max_tc = min_tc;
					
					//get texture coordinate domain
					for (auto j = 0; j < face.mNumVertices; ++j)
					{
						update_min_max(min_tc, max_tc, ftc[j]);
					}
				}

				LLVector2 tc_range = max_tc - min_tc;

				for (auto j = 0; j < face.mNumVertices; ++j)
				{ //for each vert
		
					F32* pos = face.mPositions[j].getF32ptr();
										
					//position
					for (auto k = 0; k < 3; ++k)
					{ //for each component
						//convert to 16-bit normalized across domain
						U16 val = static_cast<U16>((pos[k] - min_pos.mV[k]) / pos_range.mV[k] * 65535);

						U8* buff = reinterpret_cast<U8*>(&val);
						//write to binary buffer
						verts[vert_idx++] = buff[0];
						verts[vert_idx++] = buff[1];
					}

					if (face.mNormals)
					{ //normals
						F32* norm = face.mNormals[j].getF32ptr();

						for (auto k = 0; k < 3; ++k)
						{ //for each component
							//convert to 16-bit normalized
							U16 val = static_cast<U16>((norm[k] + 1.f) * 0.5f * 65535);
							U8* buff = reinterpret_cast<U8*>(&val);

							//write to binary buffer
							normals[norm_idx++] = buff[0];
							normals[norm_idx++] = buff[1];
						}
					}
					
					//texcoord
					if (face.mTexCoords)
					{
						F32* src_tc = static_cast<F32*>(face.mTexCoords[j].mV);

						for (auto k = 0; k < 2; ++k)
						{ //for each component
							//convert to 16-bit normalized
							U16 val = static_cast<U16>((src_tc[k] - min_tc.mV[k]) / tc_range.mV[k] * 65535);

							U8* buff = reinterpret_cast<U8*>(&val);
							//write to binary buffer
							tc[tc_idx++] = buff[0];
							tc[tc_idx++] = buff[1];
						}
					}
				}

				U32 idx_idx = 0;
				for (auto j = 0; j < face.mNumIndices; ++j)
				{
					U8* buff = reinterpret_cast<U8*>(&(face.mIndices[j]));
					indices[idx_idx++] = buff[0];
					indices[idx_idx++] = buff[1];
				}

				//write out face data
				mdl[model_names[idx]][i]["PositionDomain"]["Min"] = min_pos.getValue();
				mdl[model_names[idx]][i]["PositionDomain"]["Max"] = max_pos.getValue();
				mdl[model_names[idx]][i]["Position"] = verts;
				
				if (face.mNormals)
				{
					mdl[model_names[idx]][i]["Normal"] = normals;
				}

				if (face.mTexCoords)
				{
					mdl[model_names[idx]][i]["TexCoord0Domain"]["Min"] = min_tc.getValue();
					mdl[model_names[idx]][i]["TexCoord0Domain"]["Max"] = max_tc.getValue();
					mdl[model_names[idx]][i]["TexCoord0"] = tc;
				}

				mdl[model_names[idx]][i]["TriangleList"] = indices;

				if (skinning)
				{
					//write out skin weights

					//each influence list entry is up to 4 24-bit values
					// first 8 bits is bone index
					// last 16 bits is bone influence weight
					// a bone index of 0xFF signifies no more influences for this vertex

					std::stringstream ostr;

					for (auto j = 0; j < face.mNumVertices; ++j)
					{
						LLVector3 pos(face.mPositions[j].getF32ptr());

						weight_list& weights = high->getJointInfluences(pos);

						S32 count = 0;
						for (auto& weight : weights)
                        {
							// Note joint index cannot exceed 255.
							if (weight.mJointIdx < 255 && weight.mJointIdx >= 0)
							{
								U8 idx = static_cast<U8>(weight.mJointIdx);
								ostr.write(reinterpret_cast<const char*>(&idx), 1);

								U16 influence = static_cast<U16>(weight.mWeight * 65535);
								ostr.write(reinterpret_cast<const char*>(&influence), 2);

								++count;
							}		
						}
						U8 end_list = 0xFF;
						if (count < 4)
						{
							ostr.write(reinterpret_cast<const char*>(&end_list), 1);
						}
					}

					//copy ostr to binary buffer
					std::string data = ostr.str();
					const U8* buff = reinterpret_cast<U8*>(data.data());
					U32 bytes = data.size();

					LLSD::Binary w(bytes);
					for (U32 j = 0; j < bytes; ++j)
					{
						w[j] = buff[j];
					}

					mdl[model_names[idx]][i]["Weights"] = w;
				}
			}
		}
	}
	
	return writeModelToStream(ostr, mdl, nowrite, as_slm);
}

LLSD LLModel::writeModelToStream(std::ostream& ostr, LLSD& mdl, BOOL nowrite, BOOL as_slm)
{
	U32 bytes = 0;
	
	std::string::size_type cur_offset = 0;

	LLSD header;

	if (as_slm && mdl.has("material_list"))
	{ //save material binding names to header
		header["material_list"] = mdl["material_list"];
	}

	std::string skin;

	if (mdl.has("skin"))
	{ //write out skin block
		skin = zip_llsd(mdl["skin"]);

		U32 size = skin.size();
		if (size > 0)
		{
			header["skin"]["offset"] = static_cast<LLSD::Integer>(cur_offset);
			header["skin"]["size"] = static_cast<LLSD::Integer>(size);
			cur_offset += size;
			bytes += size;
		}
	}

	std::string decomposition;

	if (mdl.has("physics_convex"))
	{ //write out convex decomposition
		decomposition = zip_llsd(mdl["physics_convex"]);

		U32 size = decomposition.size();
		if (size > 0)
		{
			header["physics_convex"]["offset"] = static_cast<LLSD::Integer>(cur_offset);
			header["physics_convex"]["size"] = static_cast<LLSD::Integer>(size);
			cur_offset += size;
			bytes += size;
		}
	}

        if (mdl.has("submodel_id"))
        { //write out submodel id
        header["submodel_id"] = static_cast<LLSD::Integer>(mdl["submodel_id"]);
        }

	std::string out[MODEL_NAMES_LENGTH];

	for (auto i = 0; i < MODEL_NAMES_LENGTH; i++)
	{
		if (mdl.has(model_names[i]))
		{
			out[i] = zip_llsd(mdl[model_names[i]]);

            auto size = out[i].size();

			header[model_names[i]]["offset"] = static_cast<LLSD::Integer>(cur_offset);
			header[model_names[i]]["size"] = static_cast<LLSD::Integer>(size);
			cur_offset += size;
			bytes += size;
		}
	}

	if (!nowrite)
	{
		LLSDSerialize::toBinary(header, ostr);

		if (!skin.empty())
		{ //write skin block
			ostr.write(static_cast<const char*>(skin.data()), header["skin"]["size"].asInteger());
		}

		if (!decomposition.empty())
		{ //write decomposition block
			ostr.write(static_cast<const char*>(decomposition.data()), header["physics_convex"]["size"].asInteger());
		}

		for (auto i = 0; i < MODEL_NAMES_LENGTH; i++)
		{
			if (!out[i].empty())
			{
				ostr.write(static_cast<const char*>(out[i].data()), header[model_names[i]]["size"].asInteger());
			}
		}
	}
	
	return header;
}

LLModel::weight_list& LLModel::getJointInfluences(const LLVector3& pos)
{
	//1. If a vertex has been weighted then we'll find it via pos and return its weight list
    auto iterPos = mSkinWeights.begin();
    auto iterEnd = mSkinWeights.end();
	
	for ( ; iterPos!=iterEnd; ++iterPos )
	{
		if ( jointPositionalLookup( iterPos->first, pos ) )
		{
			return iterPos->second;
		}
	}
	
	//2. Otherwise we'll use the older implementation
    auto iter = mSkinWeights.find(pos);
	
	if (iter != mSkinWeights.end())
	{
		if ((iter->first - pos).magVec() > 0.1f)
		{
			LL_ERRS() << "Couldn't find weight list." << LL_ENDL;
		}

		return iter->second;
	}
	else
	{  //no exact match found, get closest point
		const F32 epsilon = 1e-5f;
        auto iter_up = mSkinWeights.lower_bound(pos);
        auto iter_down = iter_up;
		if (iter_up != mSkinWeights.end())
		{
			iter_down = ++iter_up;
		}
        auto best = iter_up;

		F32 min_dist = (iter_up->first - pos).magVec();

		bool done = false;
		while (!done)
		{ //search up and down mSkinWeights from lower bound of pos until a 
		  //match is found within epsilon.  If no match is found within epsilon,
		  //return closest match
			done = true;
			if (iter_up != mSkinWeights.end() && ++iter_up != mSkinWeights.end())
			{
				done = false;
				F32 dist = (iter_up->first - pos).magVec();

				if (dist < epsilon)
				{
					return iter_up->second;
				}

				if (dist < min_dist)
				{
					best = iter_up;
					min_dist = dist;
				}
			}

			if (iter_down != mSkinWeights.begin() && --iter_down != mSkinWeights.begin())
			{
				done = false;

				F32 dist = (iter_down->first - pos).magVec();

				if (dist < epsilon)
				{
					return iter_down->second;
				}

				if (dist < min_dist)
				{
					best = iter_down;
					min_dist = dist;
				}

			}
		}
		
		return best->second;
	}					
}

void LLModel::setConvexHullDecomposition(
	const LLModel::convex_hull_decomposition& decomp)
{
	mPhysics.mHull = decomp;
	mPhysics.mMesh.clear();
	updateHullCenters();
}

void LLModel::updateHullCenters()
{
	mHullCenter.resize(mPhysics.mHull.size());
	mHullPoints = 0;
	mCenterOfHullCenters.clear();

	for (auto i = 0; i < mPhysics.mHull.size(); ++i)
	{
		LLVector3 cur_center;

		for (const auto& j : mPhysics.mHull[i])
        {
			cur_center += j;
		}
		mCenterOfHullCenters += cur_center;
		cur_center *= 1.f/mPhysics.mHull[i].size();
		mHullCenter[i] = cur_center;
		mHullPoints += mPhysics.mHull[i].size();
	}

	if (mHullPoints > 0)
	{
		mCenterOfHullCenters *= 1.f / mHullPoints;
		llassert(mPhysics.hasHullList());
	}
}

bool LLModel::loadModel(std::istream& is)
{
	mSculptLevel = -1;  // default is an error occured

	LLSD header;
	{
		if (!LLSDSerialize::fromBinary(header, is, 1024*1024*1024))
		{
			LL_WARNS() << "Mesh header parse error.  Not a valid mesh asset!" << LL_ENDL;
			return false;
		}
	}
	
	if (header.has("material_list"))
	{ //load material list names
		mMaterialList.clear();
		for (auto i = 0; i < header["material_list"].size(); ++i)
		{
			mMaterialList.push_back(header["material_list"][i].asString());
		}
	}

	mSubmodelID = header.has("submodel_id") ? header["submodel_id"].asInteger() : false;
    
    static const std::array<std::string, 5> lod_name = {{
        "lowest_lod",
        "low_lod",
        "medium_lod",
        "high_lod",
        "physics_mesh",
    }};

	S32 lod = llclamp(static_cast<S32>(mDetail), 0, static_cast<S32>(lod_name.size()) - 1);

	if (header[lod_name[lod]]["offset"].asInteger() == -1 || 
		header[lod_name[lod]]["size"].asInteger() == 0 )
	{ //cannot load requested LOD
		LL_WARNS() << "LoD data is invalid!" << LL_ENDL;
		return false;
	}

	bool has_skin = header["skin"]["offset"].asInteger() >=0 &&
					header["skin"]["size"].asInteger() > 0;

	if ((lod == LLModel::LOD_HIGH) && !mSubmodelID)
	{ //try to load skin info and decomp info
		std::ios::pos_type cur_pos = is.tellg();
		loadSkinInfo(header, is);
		is.seekg(cur_pos);
	}

	if ((lod == LLModel::LOD_HIGH || lod == LLModel::LOD_PHYSICS) && !mSubmodelID)
	{
		std::ios::pos_type cur_pos = is.tellg();
		loadDecomposition(header, is);
		is.seekg(cur_pos);
	}

	is.seekg(header[lod_name[lod]]["offset"].asInteger(), std::ios_base::cur);

	if (unpackVolumeFaces(is, header[lod_name[lod]]["size"].asInteger()))
	{
		if (has_skin)
		{ 
			//build out mSkinWeight from face info
			for (auto i = 0; i < getNumVolumeFaces(); ++i)
			{
				const LLVolumeFace& face = getVolumeFace(i);

				if (face.mWeights)
				{
					for (auto j = 0; j < face.mNumVertices; ++j)
					{
						LLVector4a& w = face.mWeights[j];

						std::vector<JointWeight> wght;

						for (auto k = 0; k < 4; ++k)
						{
							S32 idx = static_cast<S32>(w[k]);
							F32 f = w[k] - idx;
							if (f > 0.f)
							{
								wght.emplace_back(idx, f);
							}
						}

						if (!wght.empty())
						{
							LLVector3 pos(face.mPositions[j].getF32ptr());
							mSkinWeights[pos] = wght;
						}
					}
				}
			}
		}
		return true;
	}
	else
	{
		LL_WARNS() << "unpackVolumeFaces failed!" << LL_ENDL;
	}

	return false;
}

bool LLModel::isMaterialListSubset( LLModel* ref )
{
	auto refCnt = ref->mMaterialList.size();
	auto modelCnt = mMaterialList.size();
	
	for (auto src = 0; src < modelCnt; ++src)
	{				
		bool foundRef = false;
		
		for (auto dst = 0; dst < refCnt; ++dst)
		{
			//LL_INFOS()<<mMaterialList[src]<<" "<<ref->mMaterialList[dst]<<LL_ENDL;
			foundRef = mMaterialList[src] == ref->mMaterialList[dst];									
				
			if ( foundRef )
			{	
				break;
			}										
		}

		if (!foundRef)
		{
            LL_INFOS() << "Could not find material " << mMaterialList[src] << " in reference model " << ref->mLabel << LL_ENDL;
			return false;
		}
	}
	
	return true;
}

bool LLModel::needToAddFaces( LLModel* ref, int& refFaceCnt, int& modelFaceCnt )
{
	bool changed = false;
	if ( refFaceCnt< modelFaceCnt )
	{
		refFaceCnt += modelFaceCnt - refFaceCnt;
		changed = true;
	}
	else if ( modelFaceCnt < refFaceCnt )
	{
		modelFaceCnt += refFaceCnt - modelFaceCnt;
		changed = true;
	}
	
	return changed;
}

bool LLModel::matchMaterialOrder(LLModel* ref, int& refFaceCnt, int& modelFaceCnt )
{
	//Is this a subset?
	//LODs cannot currently add new materials, e.g.
	//1. ref = a,b,c lod1 = d,e => This is not permitted
	//2. ref = a,b,c lod1 = c => This would be permitted
	
	bool isASubset = isMaterialListSubset( ref );
	if ( !isASubset )
	{
		LL_INFOS()<<"Material of model is not a subset of reference."<<LL_ENDL;
		return false;
	}
	
	std::map<std::string, U32> index_map;
	
	//build a map of material slot names to face indexes
	bool reorder = false;

	std::set<std::string> base_mat;
	std::set<std::string> cur_mat;

	for (auto i = 0; i < mMaterialList.size(); i++)
	{
		index_map[ref->mMaterialList[i]] = i;
		//if any material name does not match reference, we need to reorder
		reorder |= ref->mMaterialList[i] != mMaterialList[i];
		base_mat.insert(ref->mMaterialList[i]);
		cur_mat.insert(mMaterialList[i]);
	}


	if (reorder &&  (base_mat == cur_mat)) //don't reorder if material name sets don't match
	{
		std::vector<LLVolumeFace> new_face_list;
		new_face_list.resize(mMaterialList.size());

		std::vector<std::string> new_material_list;
		new_material_list.resize(mMaterialList.size());

		//rebuild face list so materials have the same order 
		//as the reference model
		for (auto i = 0; i < mMaterialList.size(); ++i)
		{ 
			U32 ref_idx = index_map[mMaterialList[i]];

			if (i < mVolumeFaces.size())
			{
				new_face_list[ref_idx] = mVolumeFaces[i];
			}
			new_material_list[ref_idx] = mMaterialList[i];
		}

		llassert(new_material_list == ref->mMaterialList);
		
		mVolumeFaces = std::move(new_face_list);

		//override material list with reference model ordering
		mMaterialList = ref->mMaterialList;
	}
	
	return true;
}

bool LLModel::loadSkinInfo(LLSD& header, std::istream &is)
{
	S32 offset = header["skin"]["offset"].asInteger();
	S32 size = header["skin"]["size"].asInteger();

	if (offset >= 0 && size > 0)
	{
		is.seekg(offset, std::ios_base::cur);

		LLSD skin_data;

		if (LLUZipHelper::unzip_llsd(skin_data, is, size) == LLUZipHelper::ZR_OK)
		{
			mSkinInfo.fromLLSD(skin_data);
			return true;
		}
	}

	return false;
}

bool LLModel::loadDecomposition(LLSD& header, std::istream& is)
{
	S32 offset = header["physics_convex"]["offset"].asInteger();
	S32 size = header["physics_convex"]["size"].asInteger();

	if (offset >= 0 && size > 0 && !mSubmodelID)
	{
		is.seekg(offset, std::ios_base::cur);

		LLSD data;

		if (LLUZipHelper::unzip_llsd(data, is, size) == LLUZipHelper::ZR_OK)
		{
			mPhysics.fromLLSD(data);
			updateHullCenters();
		}
	}

	return true;
}

LLMeshSkinInfo::LLMeshSkinInfo()
:   mBindShapeMatrix()
,   mPelvisOffset(0.0)
,   mLockScaleIfJointPosition(false)
,   mInvalidJointsScrubbed(false)
,   mJointNumsInitialized(false)
{
}

LLMeshSkinInfo::LLMeshSkinInfo(LLSD& skin)
:   mBindShapeMatrix()
,   mPelvisOffset(0.0)
,   mLockScaleIfJointPosition(false)
,   mInvalidJointsScrubbed(false)
,   mJointNumsInitialized(false)
{
	fromLLSD(skin);
}

void LLMeshSkinInfo::fromLLSD(LLSD& skin)
{
	if (skin.has("joint_names"))
	{
		const auto& joint_names = skin["joint_names"];
		for(auto jnt_nm_iter = joint_names.beginArray(), jnt_nm_end = joint_names.endArray(); jnt_nm_iter != jnt_nm_end; ++jnt_nm_iter)
		{
			mJointNames.emplace_back(jnt_nm_iter->asString());
			mJointNums.emplace_back(-1);
		}
	}

	if (skin.has("inverse_bind_matrix"))
	{
		const auto& inv_bind_mat = skin["inverse_bind_matrix"];
		for (auto i = 0; i < inv_bind_mat.size(); ++i)
		{
			LLMatrix4 mat;
			for (auto j = 0; j < 4; j++)
			{
				for (auto k = 0; k < 4; k++)
				{
					mat.mMatrix[j][k] = inv_bind_mat[i][j*4+k].asReal();
				}
			}
			LLMatrix4a in_mat;
			in_mat.loadu(mat);

			mInvBindMatrix.emplace_back(in_mat);
		}
	}

	if (skin.has("bind_shape_matrix"))
	{
		const auto& bind_shape_mat = skin["bind_shape_matrix"];
		LLMatrix4 mat;
		for (auto j = 0; j < 4; j++)
		{
			for (auto k = 0; k < 4; k++)
			{
				mat.mMatrix[j][k] = bind_shape_mat[j*4+k].asReal();
			}
		}
		mBindShapeMatrix.loadu(mat);
	}

	if (skin.has("alt_inverse_bind_matrix"))
	{
		const auto& alt_inv_bind_mat = skin["alt_inverse_bind_matrix"];
		for (auto i = 0; i < alt_inv_bind_mat.size(); ++i)
		{
			LLMatrix4 mat;
			for (auto j = 0; j < 4; j++)
			{
				for (auto k = 0; k < 4; k++)
				{
					mat.mMatrix[j][k] = alt_inv_bind_mat[i][j*4+k].asReal();
				}
			}
			
			mAlternateBindMatrix.emplace_back(mat);
		}
	}

	if (skin.has("pelvis_offset"))
	{
		mPelvisOffset = skin["pelvis_offset"].asReal();
	}

    if (skin.has("lock_scale_if_joint_position"))
    {
        mLockScaleIfJointPosition = skin["lock_scale_if_joint_position"].asBoolean();
    }
    else
    {
        mLockScaleIfJointPosition = false;
    }
}

LLSD LLMeshSkinInfo::asLLSD(bool include_joints, bool lock_scale_if_joint_position) const
{
	LLSD ret;

	for (auto i = 0; i < mJointNames.size(); ++i)
	{
		ret["joint_names"][i] = mJointNames[i];

		const F32* inv_bind_mat = mInvBindMatrix[i].getF32ptr();
		for (auto j = 0; j < 4; j++)
		{
			for (auto k = 0; k < 4; k++)
			{
				ret["inverse_bind_matrix"][i][j * 4 + k] = inv_bind_mat[j * 4 + k];
			}
		}
	}

	const F32* bind_shape_mat = mBindShapeMatrix.getF32ptr();
	for (auto i = 0; i < 4; i++)
	{
		for (auto j = 0; j < 4; j++)
		{
			ret["bind_shape_matrix"][i * 4 + j] = bind_shape_mat[i * 4  + j];
		}
	}
		
	if ( include_joints && !mAlternateBindMatrix.empty())
	{
		for (auto i = 0; i < mJointNames.size(); ++i)
		{
			for (auto j = 0; j < 4; j++)
			{
				for (auto k = 0; k < 4; k++)
				{
					ret["alt_inverse_bind_matrix"][i][j*4+k] = mAlternateBindMatrix[i].mMatrix[j][k]; 
				}
			}
		}

        if (lock_scale_if_joint_position)
        {
            ret["lock_scale_if_joint_position"] = lock_scale_if_joint_position;
        }

		ret["pelvis_offset"] = mPelvisOffset;
	}

	return ret;
}

LLModel::Decomposition::Decomposition(LLSD& data)
{
	fromLLSD(data);
}

void LLModel::Decomposition::fromLLSD(LLSD& decomp)
{
	if (decomp.has("HullList") && decomp.has("Positions"))
	{
		// updated for const-correctness. gcc is picky about this type of thing - Nyx
		const LLSD::Binary& hulls = decomp["HullList"].asBinary();
		const LLSD::Binary& position = decomp["Positions"].asBinary();

		U16* p = (U16*) &position[0];

		mHull.resize(hulls.size());

		LLVector3 min;
		LLVector3 max;

        if (decomp.has("Min"))
        {
            min.setValue(decomp["Min"]);
            max.setValue(decomp["Max"]);
        }
        else
        {
            min.set(-0.5f, -0.5f, -0.5f);
            max.set(0.5f, 0.5f, 0.5f);
        }

		LLVector3 range = max - min;

		for (auto i = 0; i < hulls.size(); ++i)
		{
			U16 count = (hulls[i] == 0) ? 256 : hulls[i];
			
			std::set<U64> valid;

			//must have at least 4 points
			//llassert(count > 3);

			for (auto j = 0; j < count; ++j)
			{
				U64 test = static_cast<U64>(p[0]) | static_cast<U64>(p[1]) << 16 | static_cast<U64>(p[2]) << 32;
				//point must be unique
				//llassert(valid.find(test) == valid.end());
				valid.insert(test);

				mHull[i].emplace_back(
                    static_cast<F32>(p[0])/65535.f*range.mV[0]+min.mV[0],
					static_cast<F32>(p[1])/65535.f*range.mV[1]+min.mV[1],
					static_cast<F32>(p[2])/65535.f*range.mV[2]+min.mV[2]);
				p += 3;


			}

			//each hull must contain at least 4 unique points
			//llassert(valid.size() > 3);
		}
	}

	if (decomp.has("BoundingVerts"))
	{
		const LLSD::Binary& position = decomp["BoundingVerts"].asBinary();

		U16* p = (U16*) &position[0];

		LLVector3 min;
		LLVector3 max;

        if (decomp.has("Min"))
        {
            min.setValue(decomp["Min"]);
            max.setValue(decomp["Max"]);
        }
        else
        {
            min.set(-0.5f, -0.5f, -0.5f);
            max.set(0.5f, 0.5f, 0.5f);
        }

		LLVector3 range = max - min;

		size_t count = position.size()/6;
		
		for (auto j = 0; j < count; ++j)
		{
			mBaseHull.push_back(LLVector3(
				static_cast<F32>(p[0])/65535.f*range.mV[0]+min.mV[0],
				static_cast<F32>(p[1])/65535.f*range.mV[1]+min.mV[1],
				static_cast<F32>(p[2])/65535.f*range.mV[2]+min.mV[2]));
			p += 3;
		}		 
	}
	else
	{
		//empty base hull mesh to indicate decomposition has been loaded
		//but contains no base hull
		mBaseHullMesh.clear();
	}
}

bool LLModel::Decomposition::hasHullList() const
{
	return !mHull.empty() ;
}

LLSD LLModel::Decomposition::asLLSD() const
{
	LLSD ret;
	
	if (mBaseHull.empty() && mHull.empty())
	{ //nothing to write
		return ret;
	}

	//write decomposition block
	// ["physics_convex"]["HullList"] -- list of 8 bit integers, each entry represents a hull with specified number of points
	// ["physics_convex"]["Position"] -- list of 16-bit integers to be decoded to given domain, encoded 3D points
	// ["physics_convex"]["BoundingVerts"] -- list of 16-bit integers to be decoded to given domain, encoded 3D points representing a single hull approximation of given shape
	
	//get minimum and maximum
	LLVector3 min;
	
	if (mHull.empty())
	{  
		min = mBaseHull[0];
	}
	else
	{
		min = mHull[0][0];
	}

	LLVector3 max = min;

	LLSD::Binary hulls(mHull.size());

	U32 total = 0;

	for (auto i = 0; i < mHull.size(); ++i)
	{
		U32 size = mHull[i].size();
		total += size;
		hulls[i] = static_cast<U8>(size);

		for (auto j : mHull[i])
        {
			update_min_max(min, max, j);
		}
	}

	for (auto i : mBaseHull)
    {
		update_min_max(min, max, i);	
	}

	ret["Min"] = min.getValue();
	ret["Max"] = max.getValue();

	LLVector3 range = max-min;

	if (!hulls.empty())
	{
		ret["HullList"] = hulls;
	}

	if (total > 0)
	{
		LLSD::Binary p(total*3*2);

		U32 vert_idx = 0;
		
		for (const auto& i : mHull)
        {
			std::set<U64> valid;

			llassert(!i.empty());

			for (auto j : i)
            {
				U64 test = 0;
				const F32* src = j.mV;

				for (auto k = 0; k < 3; k++)
				{
					//convert to 16-bit normalized across domain
					U16 val = static_cast<U16>(((src[k] - min.mV[k]) / range.mV[k]) * 65535);

					if(valid.size() < 3)
					{
						switch (k)
						{
							case 0: test = test | static_cast<U64>(val); break;
							case 1: test = test | (static_cast<U64>(val) << 16); break;
							case 2: test = test | (static_cast<U64>(val) << 32); break;
						};

						valid.insert(test);
					}
					
					U8* buff = reinterpret_cast<U8*>(&val);
					//write to binary buffer
					p[vert_idx++] = buff[0];
					p[vert_idx++] = buff[1];

					//makes sure we haven't run off the end of the array
					llassert(vert_idx <= p.size());
				}
			}

			//must have at least 3 unique points
			llassert(valid.size() > 2);
		}

		ret["Positions"] = p;
	}

	//llassert(!mBaseHull.empty());

	if (!mBaseHull.empty())
	{
		LLSD::Binary p(mBaseHull.size()*3*2);

		U32 vert_idx = 0;
		for (auto j : mBaseHull)
        {
			const F32* v = j.mV;

			for (auto k = 0; k < 3; k++)
			{
				//convert to 16-bit normalized across domain
				U16 val = static_cast<U16>(((v[k] - min.mV[k]) / range.mV[k]) * 65535);

				U8* buff = reinterpret_cast<U8*>(&val);
				//write to binary buffer
				p[vert_idx++] = buff[0];
				p[vert_idx++] = buff[1];

				if (vert_idx > p.size())
				{
					LL_ERRS() << "Index out of bounds" << LL_ENDL;
				}
			}
		}
		
		ret["BoundingVerts"] = p;
	}

	return ret;
}

void LLModel::Decomposition::merge(const LLModel::Decomposition* rhs)
{
	if (!rhs)
	{
		return;
	}

	if (mMeshID != rhs->mMeshID)
	{
		LL_ERRS() << "Attempted to merge with decomposition of some other mesh." << LL_ENDL;
	}

	if (mBaseHull.empty())
	{ //take base hull and decomposition from rhs
		mHull = rhs->mHull;
		mBaseHull = rhs->mBaseHull;
		mMesh = rhs->mMesh;
		mBaseHullMesh = rhs->mBaseHullMesh;
	}

	if (mPhysicsShapeMesh.empty())
	{ //take physics shape mesh from rhs
		mPhysicsShapeMesh = rhs->mPhysicsShapeMesh;
	}
}

bool ll_is_degenerate(const LLVector4a& a, const LLVector4a& b, const LLVector4a& c, F32 tolerance)
{
	// small area check
	{
		LLVector4a edge1; edge1.setSub( a, b );
		LLVector4a edge2; edge2.setSub( a, c );
		//////////////////////////////////////////////////////////////////////////
		/// Linden Modified
		//////////////////////////////////////////////////////////////////////////

		// If no one edge is more than 10x longer than any other edge, we weaken
		// the tolerance by a factor of 1e-4f.

		LLVector4a edge3; edge3.setSub( c, b );
		const F32 len1sq = edge1.dot3(edge1).getF32();
		const F32 len2sq = edge2.dot3(edge2).getF32();
		const F32 len3sq = edge3.dot3(edge3).getF32();
		bool abOK = (len1sq <= 100.f * len2sq) && (len1sq <= 100.f * len3sq);
		bool acOK = (len2sq <= 100.f * len1sq) && (len1sq <= 100.f * len3sq);
		bool cbOK = (len3sq <= 100.f * len1sq) && (len1sq <= 100.f * len2sq);
		if ( abOK && acOK && cbOK )
		{
			tolerance *= 1e-4f;
		}

		//////////////////////////////////////////////////////////////////////////
		/// End Modified
		//////////////////////////////////////////////////////////////////////////

		LLVector4a cross; cross.setCross3( edge1, edge2 );

		LLVector4a edge1b; edge1b.setSub( b, a );
		LLVector4a edge2b; edge2b.setSub( b, c );
		LLVector4a crossb; crossb.setCross3( edge1b, edge2b );

		if ( ( cross.dot3(cross).getF32() < tolerance ) || ( crossb.dot3(crossb).getF32() < tolerance ))
		{
			return true;
		}
	}

	// point triangle distance check
	{
		LLVector4a Q; Q.setSub(a, b);
		LLVector4a R; R.setSub(c, b);

		const F32 QQ = dot3fpu(Q, Q);
		const F32 RR = dot3fpu(R, R);
		const F32 QR = dot3fpu(R, Q);

		volatile F32 QQRR = QQ * RR;
		volatile F32 QRQR = QR * QR;
		F32 Det = (QQRR - QRQR);

		if( Det == 0.0f )
		{
			return true;
		}
	}

	return false;
}

bool validate_face(const LLVolumeFace& face)
{
	for (auto i = 0; i < face.mNumIndices; ++i)
	{
		if (face.mIndices[i] >= face.mNumVertices)
		{
			LL_WARNS() << "Face has invalid index." << LL_ENDL;
			return false;
		}
	}

	if (face.mNumIndices % 3 != 0 || face.mNumIndices == 0)
	{
		LL_WARNS() << "Face has invalid number of indices." << LL_ENDL;
		return false;
	}

	/*const LLVector4a scale(0.5f);

	for (auto i = 0; i < face.mNumIndices; i+=3)
	{
		U16 idx1 = face.mIndices[i];
		U16 idx2 = face.mIndices[i+1];
		U16 idx3 = face.mIndices[i+2];

		LLVector4a v1; v1.setMul(face.mPositions[idx1], scale);
		LLVector4a v2; v2.setMul(face.mPositions[idx2], scale);
		LLVector4a v3; v3.setMul(face.mPositions[idx3], scale);

		if (ll_is_degenerate(v1,v2,v3))
		{
			llwarns << "Degenerate face found!" << LL_ENDL;
			return false;
		}
	}*/

	return true;
}

bool validate_model(const LLModel* mdl)
{
	if (mdl->getNumVolumeFaces() == 0)
	{
		LL_WARNS() << "Model has no faces!" << LL_ENDL;
		return false;
	}

	for (auto i = 0; i < mdl->getNumVolumeFaces(); ++i)
	{
		if (mdl->getVolumeFace(i).mNumVertices == 0)
		{
			LL_WARNS() << "Face has no vertices." << LL_ENDL;
			return false;
		}

		if (mdl->getVolumeFace(i).mNumIndices == 0)
		{
			LL_WARNS() << "Face has no indices." << LL_ENDL;
			return false;
		}

		if (!validate_face(mdl->getVolumeFace(i)))
		{
			return false;
		}
	}

	return true;
}

LLModelInstance::LLModelInstance(LLSD& data)
{
	mLocalMeshID = data["mesh_id"].asInteger();
	mLabel = data["label"].asString();
	mTransform.setValue(data["transform"]);

	for (auto i = 0; i < data["material"].size(); ++i)
	{
		LLImportMaterial mat(data["material"][i]);
		mMaterial[mat.mBinding] = mat;
	}
}


LLSD LLModelInstance::asLLSD()
{	
	LLSD ret;

	ret["mesh_id"] = mModel->mLocalID;
	ret["label"] = mLabel;
	ret["transform"] = mTransform.getValue();

	U32 i = 0;
	for (auto& iter : mMaterial)
    {
		ret["material"][i++] = iter.second.asLLSD();
	}

	return ret;
}

LLImportMaterial::LLImportMaterial(LLSD& data) : mOpaqueData(nullptr)
{
	mDiffuseMapFilename = data["diffuse"]["filename"].asString();
	mDiffuseMapLabel = data["diffuse"]["label"].asString();
	mDiffuseColor.setValue(data["diffuse"]["color"]);
	mFullbright = data["fullbright"].asBoolean();
	mBinding = data["binding"].asString();
}

LLSD LLImportMaterial::asLLSD() const
{
	LLSD ret;

	ret["diffuse"]["filename"] = mDiffuseMapFilename;
	ret["diffuse"]["label"] = mDiffuseMapLabel;
	ret["diffuse"]["color"] = mDiffuseColor.getValue();
	ret["fullbright"] = mFullbright;
	ret["binding"] = mBinding;

	return ret;
}

bool LLImportMaterial::operator<(const LLImportMaterial &rhs) const
{

	if (mDiffuseMapID != rhs.mDiffuseMapID)
	{
		return mDiffuseMapID < rhs.mDiffuseMapID;
	}

	if (mDiffuseMapFilename != rhs.mDiffuseMapFilename)
	{
		return mDiffuseMapFilename < rhs.mDiffuseMapFilename;
	}

	if (mDiffuseMapLabel != rhs.mDiffuseMapLabel)
	{
		return mDiffuseMapLabel < rhs.mDiffuseMapLabel;
	}

	if (mDiffuseColor != rhs.mDiffuseColor)
	{
		return mDiffuseColor < rhs.mDiffuseColor;
	}

	if (mBinding != rhs.mBinding)
	{
		return mBinding < rhs.mBinding;
	}

	return mFullbright < rhs.mFullbright;
}

