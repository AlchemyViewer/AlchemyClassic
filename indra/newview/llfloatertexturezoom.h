/*
 * @file llfloatertexturezoom.h
 * @brief Large image preview floater
 *
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2014, Cinder Roxley @ Second Life <cinder@alchemyviewer.org>
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
 */

#ifndef LL_FLOATERTEXTUREZOOM_H
#define LL_FLOATERTEXTUREZOOM_H

#include "llpreview.h"
#include "llviewertexture.h"

class LLFloaterTextureZoom : public LLPreview
{
public:
	LLFloaterTextureZoom(const LLSD& key);
	
	/* virtual */ BOOL postBuild();
	/* virtual */ void draw();
	/* virtual */ void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);
	
	/* virtual */ void loadAsset();
	/* virtual */ EAssetStatus getAssetStatus();
	/* virtual */ void setObjectID(const LLUUID& object_id);
	/* virtual */ BOOL handleKeyHere(KEY key, MASK mask);
	/* virtual */ void onFocusLost();
	
private:
	~LLFloaterTextureZoom();
	void updateDimensions();
	void updateImageID();
	void adjustAspectRatio();
	
	bool mLoadingFullImage;
	bool mUpdateDimensions;
	LLUUID mImageID;
	LLPointer<LLViewerFetchedTexture> mImage;
	S32 mImageOldBoostLevel;
	
	LLLoadedCallbackEntry::source_callback_list_t mCallbackTextureList;
};

#endif // LL_FLOATERTEXTUREZOOM_H
