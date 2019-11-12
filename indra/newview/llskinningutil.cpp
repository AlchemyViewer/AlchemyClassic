/** 
* @file llskinningutil.cpp
* @brief  Functions for mesh object skinning
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2015&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2015, Linden Research, Inc.
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

#include "llskinningutil.h"
#include "llvoavatar.h"
#include "llviewercontrol.h"
#include "llmeshrepository.h"
#include "llvolume.h"
#include "llrigginginfo.h"

#include "llvector4a.h"

void dump_avatar_and_skin_state(const std::string& reason, LLVOAvatar *avatar, const LLMeshSkinInfo *skin)
{
    static S32 dump_count = 0;
    const S32 max_dump = 10;

    if (dump_count < max_dump)
    {
        LL_WARNS("Avatar") << avatar->getFullname() << " dumping, reason " << reason
                           << " avatar build state: isBuilt() " << avatar->isBuilt() 
                           << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
        LL_WARNS("Avatar") << "Skin num joints " << skin->mJointNames.size() << " " << skin->mJointNums.size() << LL_ENDL;
        LL_WARNS("Avatar") << "Skin scrubbed " << skin->mInvalidJointsScrubbed 
                           << " nums init " << skin->mJointNumsInitialized << LL_ENDL;
        for (S32 j=0; j<skin->mJointNames.size(); j++)
        {
            LL_WARNS("Avatar") << "skin joint idx " << j << " name [" << skin->mJointNames[j] 
                               << "] num " << skin->mJointNums[j] << LL_ENDL;
            const std::string& name = skin->mJointNames[j];
            S32 joint_num = skin->mJointNums[j];

            LLJoint *name_joint = avatar->getJoint(name);
            LLJoint *num_joint = avatar->getJoint(joint_num);
            if (!name_joint)
            {
                LL_WARNS("Avatar") << "failed to find joint by name" << LL_ENDL; 
            }
            if (!num_joint)
            {
                LL_WARNS("Avatar") << "failed to find joint by num" << LL_ENDL; 
            }
            if (num_joint != name_joint)
            {
                LL_WARNS("Avatar") << "joint pointers don't match" << LL_ENDL;
            }
            if (num_joint && num_joint->getJointNum() != joint_num)
            {
                LL_WARNS("Avatar") << "joint found by num has wrong num " << joint_num << "!=" << num_joint->getJointNum() << LL_ENDL;
            }
            if (name_joint && name_joint->getJointNum() != joint_num)
            {
                LL_WARNS("Avatar") << "joint found by name has wrong num " << joint_num << "!=" << name_joint->getJointNum() << LL_ENDL;
            }
        }
        LL_WARNS("Avatar") << LL_ENDL;

        dump_count++;
    }
}

U32 LLSkinningUtil::getMaxJointCount()
{
    U32 result = LL_MAX_JOINTS_PER_MESH_OBJECT;
	return result;
}

U32 LLSkinningUtil::getMeshJointCount(const LLMeshSkinInfo *skin)
{
	return llmin((U32)getMaxJointCount(), (U32)skin->mJointNames.size());
}

void LLSkinningUtil::scrubInvalidJoints(LLVOAvatar *avatar, LLMeshSkinInfo* skin)
{
    if (skin->mInvalidJointsScrubbed)
    {
        return;
    }
    for (U32 j = 0; j < skin->mJointNames.size(); ++j)
    {
        // Fix invalid names to "mPelvis". Currently meshes with
        // invalid names will be blocked on upload, so this is just
        // needed for handling of any legacy bad data.
        if (!avatar->getJoint(skin->mJointNames[j]))
        {
            LL_DEBUGS("Avatar") << avatar->getFullname() << " mesh rigged to invalid joint " << skin->mJointNames[j] << LL_ENDL;
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " mesh rigged to invalid joint" << skin->mJointNames[j] << LL_ENDL;
            skin->mJointNames[j] = "mPelvis";
            skin->mJointNumsInitialized = false; // force update after names change.
        }
    }
    skin->mInvalidJointsScrubbed = true;
}

void LLSkinningUtil::initSkinningMatrixPalette(
    LLMatrix4a* mat,
    S32 count, 
    const LLMeshSkinInfo* skin,
    LLVOAvatar *avatar)
{
    initJointNums(const_cast<LLMeshSkinInfo*>(skin), avatar);
	LLMatrix4a world;
    for (S32 j = 0; j < count; ++j)
    {
        LLJoint *joint = avatar->getJoint(skin->mJointNums[j]);
        if (joint)
        {
			world.loadu(joint->getWorldMatrix());
			mat[j].setMul(world, skin->mInvBindMatrix[j]);
        }
        else
        {
            mat[j] = skin->mInvBindMatrix[j];
            // This  shouldn't  happen   -  in  mesh  upload,  skinned
            // rendering  should  be disabled  unless  all joints  are
            // valid.  In other  cases of  skinned  rendering, invalid
            // joints should already have  been removed during scrubInvalidJoints().
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() 
                                    << " rigged to invalid joint name " << skin->mJointNames[j] 
                                    << " num " << skin->mJointNums[j] << LL_ENDL;
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() 
                                    << " avatar build state: isBuilt() " << avatar->isBuilt() 
                                    << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
#if 0
            dump_avatar_and_skin_state("initSkinningMatrixPalette joint not found", avatar, skin);
#endif
        }
    }
}

void LLSkinningUtil::checkSkinWeights(LLVector4a* weights, U32 num_vertices, const LLMeshSkinInfo* skin)
{
#ifdef SHOW_ASSERT                  // same condition that controls llassert()
	const S32 max_joints = skin->mJointNames.size();
    for (U32 j=0; j<num_vertices; j++)
    {
        F32 *w = weights[j].getF32ptr();
            
        F32 wsum = 0.0;
        for (U32 k=0; k<4; ++k)
        {
            S32 i = llfloor(w[k]);
            llassert(i>=0);
            llassert(i<max_joints);
            wsum += w[k]-i;
        }
        llassert(wsum > 0.0f);
    }
#endif
}

void LLSkinningUtil::scrubSkinWeights(LLVector4a* weights, U32 num_vertices, const LLMeshSkinInfo* skin)
{
    const S32 max_joints = skin->mJointNames.size();
    LLIVector4a max_joint((S16)max_joints - 1);
    LLIVector4a cur_joint;
    LLVector4a weight;
    for (U32 j=0; j<num_vertices; j++)
    {
        cur_joint.setFloatTrunc(weights[j]);
        weight.setSub(weights[j], cur_joint);
        cur_joint.min16(max_joint);
        cur_joint.max16(LLIVector4a::getZero());
        weights[j].setAdd(weight, cur_joint);
    }
	checkSkinWeights(weights, num_vertices, skin);
}

void LLSkinningUtil::getPerVertexSkinMatrix(
    const LLVector4a& weights,
    LLMatrix4a* mat,
    bool handle_bad_scale,
    LLMatrix4a& final_mat,
    U32 max_joints)
{
#if LL_DEBUG
    bool valid_weights = true;
#endif
	LL_ALIGN_16(S32 idx[4]);

	LLIVector4a max_joint_count((S16)max_joints - 1);
	LLIVector4a current_joint_index;
	current_joint_index.setFloatTrunc(weights);

	LLVector4a weight;
	weight.setSub(weights, current_joint_index);

	current_joint_index.min16(max_joint_count);
	current_joint_index.store128a(idx);

	LLVector4a scale;
	scale.setMoveHighLow(weight);
	scale.add(weight);
	scale.addFirst(scale.getVectorAt<1>());
	scale.splat<0>(scale);

	bool scale_invalid = scale.lessEqual(LLVector4a::getEpsilon()).areAnySet(LLVector4Logical::MASK_XYZW);
	if (handle_bad_scale && scale_invalid)
	{
		static const LLVector4a safeVal(1.0f, 0.0f, 0.0f, 0.0f);
		weight = safeVal;
#if LL_DEBUG
		valid_weights = false;
#endif
		LL_WARNS() << "Invalid scale" << LL_ENDL;
	}
	else
	{
		// This is enforced  in unpackVolumeFaces()
		weight.div(scale);
	}

	final_mat.setMul(mat[idx[0]], weight.getVectorAt<0>());
	final_mat.setMulAdd(mat[idx[1]], weight.getVectorAt<1>());
	final_mat.setMulAdd(mat[idx[2]], weight.getVectorAt<2>());
	final_mat.setMulAdd(mat[idx[3]], weight.getVectorAt<3>());
#if LL_DEBUG
    // SL-366 - with weight validation/cleanup code, it should no longer be
    // possible to hit the bad scale case.
    llassert(valid_weights);
#endif
}

void LLSkinningUtil::initJointNums(LLMeshSkinInfo* skin, LLVOAvatar *avatar)
{
    if (!skin->mJointNumsInitialized)
    {
        for (U32 j = 0; j < skin->mJointNames.size(); ++j)
        {
            LLJoint *joint = NULL;
            if (skin->mJointNums[j] == -1)
            {
                joint = avatar->getJoint(skin->mJointNames[j]);
                if (joint)
                {
                    skin->mJointNums[j] = joint->getJointNum();
                    if (skin->mJointNums[j] < 0)
                    {
                        LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " joint has unusual number " << skin->mJointNames[j] << ": " << skin->mJointNums[j] << LL_ENDL;
                        LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " avatar build state: isBuilt() " << avatar->isBuilt() << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
                    }
                }
                else
                {
                    LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " unable to find joint " << skin->mJointNames[j] << LL_ENDL;
                    LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " avatar build state: isBuilt() " << avatar->isBuilt() << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
#if 0
                    dump_avatar_and_skin_state("initJointNums joint not found", avatar, skin);
#endif
                }
            }
        }
        skin->mJointNumsInitialized = true;
    }
}

static LLTrace::BlockTimerStatHandle FTM_FACE_RIGGING_INFO("Face Rigging Info");

void LLSkinningUtil::updateRiggingInfo(const LLMeshSkinInfo* skin, LLVOAvatar *avatar, LLVolumeFace& vol_face)
{
    LL_RECORD_BLOCK_TIME(FTM_FACE_RIGGING_INFO);

    if (vol_face.mJointRiggingInfoTab.needsUpdate())
    {
        S32 num_verts = vol_face.mNumVertices;
        if (num_verts>0 && vol_face.mWeights && (!skin->mJointNames.empty()))
        {
            initJointNums(const_cast<LLMeshSkinInfo*>(skin), avatar);
            if (vol_face.mJointRiggingInfoTab.size()==0)
            {
                //std::set<S32> active_joints;
                //S32 active_verts = 0;
                vol_face.mJointRiggingInfoTab.resize(LL_CHARACTER_MAX_ANIMATED_JOINTS);
                LLJointRiggingInfoTab &rig_info_tab = vol_face.mJointRiggingInfoTab;

                LLMatrix4a matrixPalette[LL_CHARACTER_MAX_ANIMATED_JOINTS];
                for (U32 i = 0; i < llmin(skin->mInvBindMatrix.size(), (size_t)LL_CHARACTER_MAX_ANIMATED_JOINTS); ++i)
                {
                    matrixPalette[i].setMul(skin->mInvBindMatrix[i], skin->mBindShapeMatrix);
                }

                LLIVector4a max_joint_count((S16)LL_CHARACTER_MAX_ANIMATED_JOINTS - 1);
                for (S32 i=0; i<vol_face.mNumVertices; i++)
                {
                    LLVector4a& pos = vol_face.mPositions[i];

					LL_ALIGN_16(S32 idx[4]);
					LL_ALIGN_16(F32 wght[4]);

					LLIVector4a current_joint_index;
					current_joint_index.setFloatTrunc(vol_face.mWeights[i]);

					LLVector4a weight;
					weight.setSub(vol_face.mWeights[i], current_joint_index);

					current_joint_index.min16(max_joint_count);
					current_joint_index.store128a(idx);

					LLVector4a scale;
					scale.setMoveHighLow(weight);
					scale.add(weight);
					scale.addFirst(scale.getVectorAt<1>());
					scale.splat<0>(scale);

					bool scale_invalid = scale.lessEqual(LLVector4a::getEpsilon()).areAnySet(LLVector4Logical::MASK_XYZW);
					if (scale_invalid)
					{
						static const LLVector4a safeVal(1.0f, 0.0f, 0.0f, 0.0f);
						safeVal.store4a(wght);
						LL_WARNS() << "Invalid scale" << LL_ENDL;
					}
					else
					{
						// This is enforced  in unpackVolumeFaces()
						weight.div(scale);
						weight.store4a(wght);
					}

					for (U32 k=0; k<4; ++k)
                    {
						S32 joint_index = idx[k];
                        if (wght[k] > 0.0f)
                        {
                            S32 joint_num = skin->mJointNums[joint_index];
                            if (joint_num >= 0 && joint_num < LL_CHARACTER_MAX_ANIMATED_JOINTS)
                            {
                                rig_info_tab[joint_num].setIsRiggedTo(true);
                                LLVector4a pos_joint_space;
                                matrixPalette[joint_index].affineTransform(pos, pos_joint_space);
                                pos_joint_space.mul(wght[k]);
                                LLVector4a *extents = rig_info_tab[joint_num].getRiggedExtents();
                                update_min_max(extents[0], extents[1], pos_joint_space);
                            }
                        }
                    }
                }
                //LL_DEBUGS("RigSpammish") << "built rigging info for vf " << &vol_face 
                //                         << " num_verts " << vol_face.mNumVertices
                //                         << " active joints " << active_joints.size()
                //                         << " active verts " << active_verts
                //                         << LL_ENDL; 
                vol_face.mJointRiggingInfoTab.setNeedsUpdate(false);
            }
        }
#if LL_DEBUG
        if (vol_face.mJointRiggingInfoTab.size()!=0)
        {
            LL_DEBUGS("RigSpammish") << "we have rigging info for vf " << &vol_face 
                                     << " num_verts " << vol_face.mNumVertices << LL_ENDL; 
        }
        else
        {
            LL_DEBUGS("RigSpammish") << "no rigging info for vf " << &vol_face 
                                     << " num_verts " << vol_face.mNumVertices << LL_ENDL; 
        }
#endif
    }
}

// This is used for extracting rotation from a bind shape matrix that
// already has scales baked in
LLQuaternion LLSkinningUtil::getUnscaledQuaternion(const LLMatrix4a& mat4)
{
    LLMatrix3 bind_mat = LLMatrix4(mat4.getF32ptr()).getMat3();
    for (auto& i : bind_mat.mMatrix)
    {
        F32 len = 0.0f;
        for (auto j : i)
        {
            len += j * j;
        }
        if (len > 0.0f)
        {
            len = sqrt(len);
            for (auto& j : i)
            {
                j /= len;
            }
        }
    }
    bind_mat.invert();
    LLQuaternion bind_rot = bind_mat.quaternion();
    bind_rot.normalize();
    return bind_rot;
}
