/** 
 * @file lldriverparam.h
 * @brief A visual parameter that drives (controls) other visual parameters.
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

#ifndef LL_LLDRIVERPARAM_H
#define LL_LLDRIVERPARAM_H

#include "llviewervisualparam.h"
#include "llwearabletype.h"
#include <deque>

class LLAvatarAppearance;
class LLDriverParam;
class LLWearable;

//-----------------------------------------------------------------------------

struct LLDrivenEntryInfo
{
	LLDrivenEntryInfo( S32 id, F32 min1, F32 max1, F32 max2, F32 min2 )
		: mDrivenID( id ), mMin1( min1 ), mMax1( max1 ), mMax2( max2 ), mMin2( min2 ) {}
	S32					mDrivenID;
	F32					mMin1;
	F32					mMax1;
	F32					mMax2;
	F32					mMin2;
};

struct LLDrivenEntry
{
	LLDrivenEntry( LLViewerVisualParam* param, LLDrivenEntryInfo *info )
		: mParam( param ), mInfo( info ) {}
	LLViewerVisualParam* mParam;
	LLDrivenEntryInfo*	 mInfo;
};

//-----------------------------------------------------------------------------

class LLDriverParamInfo : public LLViewerVisualParamInfo
{
	friend class LLDriverParam;
public:
	LLDriverParamInfo();
	/*virtual*/ ~LLDriverParamInfo() {};
	
	/*virtual*/ BOOL parseXml(LLXmlTreeNode* node) override;

	/*virtual*/ void toStream(std::ostream &out) override;	

protected:
	typedef std::deque<LLDrivenEntryInfo> entry_info_list_t;
	entry_info_list_t mDrivenInfoList;
	LLDriverParam* mDriverParam; // backpointer
};

//-----------------------------------------------------------------------------

LL_ALIGN_PREFIX(16)
class LLDriverParam : public LLViewerVisualParam
{
private:
	// Hide the default constructor.  Force construction with LLAvatarAppearance.
	LLDriverParam()
	:	mCurrentDistortionParam(nullptr)
	,	mAvatarAppearance(nullptr)
	,	mWearablep(nullptr)
	{ }

public:
	LLDriverParam(LLAvatarAppearance *appearance, LLWearable* wearable = nullptr);
	~LLDriverParam();

	void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	void operator delete(void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	// Special: These functions are overridden by child classes
	LLDriverParamInfo*		getInfo() const { return (LLDriverParamInfo*)mInfo; }
	//   This sets mInfo and calls initialization functions
	BOOL					setInfo(LLDriverParamInfo *info);

	LLAvatarAppearance* getAvatarAppearance() { return mAvatarAppearance; }
	const LLAvatarAppearance* getAvatarAppearance() const { return mAvatarAppearance; }

	void					updateCrossDrivenParams(LLWearableType::EType driven_type);

	/*virtual*/ LLViewerVisualParam* cloneParam(LLWearable* wearable) const override;

	// LLVisualParam Virtual functions
	/*virtual*/ void				apply( ESex sex ) override {} // apply is called separately for each driven param.
	/*virtual*/ void				setWeight(F32 weight, BOOL upload_bake) override;
	/*virtual*/ void				setAnimationTarget( F32 target_value, BOOL upload_bake) override;
	/*virtual*/ void				stopAnimating(BOOL upload_bake) override;
	/*virtual*/ BOOL				linkDrivenParams(visual_param_mapper mapper, BOOL only_cross_params) override;
	/*virtual*/ void				resetDrivenParams() override;
	
	// LLViewerVisualParam Virtual functions
	/*virtual*/ F32					getTotalDistortion() override;
	/*virtual*/ const LLVector4a&	getAvgDistortion() override;
	/*virtual*/ F32					getMaxDistortion() override;
	/*virtual*/ LLVector4a			getVertexDistortion(S32 index, LLPolyMesh *poly_mesh) override;
	/*virtual*/ const LLVector4a*	getFirstDistortion(U32 *index, LLPolyMesh **poly_mesh) override;
	/*virtual*/ const LLVector4a*	getNextDistortion(U32 *index, LLPolyMesh **poly_mesh) override;

	S32								getDrivenParamsCount() const;
	const LLViewerVisualParam*		getDrivenParam(S32 index) const;

	typedef std::vector<LLDrivenEntry> entry_list_t;
    entry_list_t&                   getDrivenList() { return mDriven; }
    void                            setDrivenList(entry_list_t& driven_list) { mDriven = driven_list; }

protected:
	LLDriverParam(const LLDriverParam& pOther);
	F32 getDrivenWeight(const LLDrivenEntry* driven, F32 input_weight);
	void setDrivenWeight(LLDrivenEntry *driven, F32 driven_weight, bool upload_bake);


	LL_ALIGN_16(LLVector4a	mDefaultVec); // temp holder
	entry_list_t mDriven;
	LLViewerVisualParam* mCurrentDistortionParam;
	// Backlink only; don't make this an LLPointer.
	LLAvatarAppearance* mAvatarAppearance;
	LLWearable* mWearablep;
} LL_ALIGN_POSTFIX(16);

#endif  // LL_LLDRIVERPARAM_H
