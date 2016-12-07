// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
 * @file llfloatertexturezoom.cpp
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

#include "llviewerprecompiledheaders.h"
#include "llfloatertexturezoom.h"

#include "llwindow.h"
#include "lltrans.h"
#include "llviewerinventory.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"	// for MIPMAP_TRUE

LLFloaterTextureZoom::LLFloaterTextureZoom(const LLSD& key)
:	LLPreview(key)
,	mLoadingFullImage(false)
,	mUpdateDimensions(true)
,	mImageID(LLUUID::null)
,	mImage(nullptr)
,	mImageOldBoostLevel(LLGLTexture::BOOST_NONE)
{
	updateImageID();
}

LLFloaterTextureZoom::~LLFloaterTextureZoom()
{
	LLLoadedCallbackEntry::cleanUpCallbackList(&mCallbackTextureList) ;
	
	if( mLoadingFullImage )
	{
		getWindow()->decBusyCount();
	}
	mImage->setBoostLevel(mImageOldBoostLevel);
	mImage = nullptr;
}

BOOL LLFloaterTextureZoom::postBuild()
{
	getChild<LLUICtrl>("yusteal")->setFocus(TRUE);
	return TRUE;
}

void LLFloaterTextureZoom::draw()
{
	updateDimensions();
	
	LLPreview::draw();
	
	if (!isMinimized())
	{
		LLGLSUIDefault gls_ui;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		
		const LLRect& border = mClientRect;
		LLRect interior = mClientRect;
		interior.stretch( -PREVIEW_BORDER_WIDTH );
		
		// ...border
		gl_rect_2d( border, LLColor4(0.f, 0.f, 0.f, 1.f));
		gl_rect_2d_checkerboard( interior );
		
		if ( mImage.notNull() )
		{
			// Draw the texture
			gGL.diffuseColor3f( 1.f, 1.f, 1.f );
			gl_draw_scaled_image(interior.mLeft,
								 interior.mBottom,
								 interior.getWidth(),
								 interior.getHeight(),
								 mImage);
			
			// Pump the texture priority
			F32 pixel_area = mLoadingFullImage ? (F32)MAX_IMAGE_AREA  : (F32)(interior.getWidth() * interior.getHeight() );
			mImage->addTextureStats( pixel_area );
			
			// Don't bother decoding more than we can display, unless
			// we're loading the full image.
			if (!mLoadingFullImage)
			{
				S32 int_width = interior.getWidth();
				S32 int_height = interior.getHeight();
				mImage->setKnownDrawSize(int_width, int_height);
			}
			else
			{
				// Don't use this feature
				mImage->setKnownDrawSize(0, 0);
			}
			
			if( mLoadingFullImage )
			{
				LLFontGL::getFontSansSerif()->renderUTF8(LLTrans::getString("Receiving"), 0,
														 interior.mLeft + 4,
														 interior.mBottom + 4,
														 LLColor4::white, LLFontGL::LEFT, LLFontGL::BOTTOM,
														 LLFontGL::NORMAL,
														 LLFontGL::DROP_SHADOW);
				
				F32 data_progress = mImage->getDownloadProgress() ;
				
				// Draw the progress bar.
				const S32 BAR_HEIGHT = 12;
				const S32 BAR_LEFT_PAD = 80;
				S32 left = interior.mLeft + 4 + BAR_LEFT_PAD;
				S32 bar_width = getRect().getWidth() - left - RESIZE_HANDLE_WIDTH - 2;
				S32 top = interior.mBottom + 4 + BAR_HEIGHT;
				S32 right = left + bar_width;
				S32 bottom = top - BAR_HEIGHT;
				
				LLColor4 background_color(0.f, 0.f, 0.f, 0.75f);
				LLColor4 decoded_color(0.f, 1.f, 0.f, 1.0f);
				LLColor4 downloaded_color(0.f, 0.5f, 0.f, 1.0f);
				
				gl_rect_2d(left, top, right, bottom, background_color);
				
				if (data_progress > 0.0f)
				{
					// Downloaded bytes
					right = left + llfloor(data_progress * (F32)bar_width);
					if (right > left)
					{
						gl_rect_2d(left, top, right, bottom, downloaded_color);
					}
				}
			}
		}
	}
}

void LLFloaterTextureZoom::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPreview::reshape(width, height, called_from_parent);
	
	//S32 padding = PREVIEW_PAD + PREVIEW_RESIZE_HANDLE_SIZE;
	S32 padding = PREVIEW_VPAD;
	
	LLRect client_rect(padding, getRect().getHeight(), getRect().getWidth() - padding, 0);
	client_rect.mTop -= padding;
	client_rect.mBottom += padding;
	
	S32 client_width = client_rect.getWidth();
	S32 client_height = client_rect.getHeight();
	
	mClientRect.setLeftTopAndSize(client_rect.getCenterX() - (client_width / 2), client_rect.getCenterY() +  (client_height / 2), client_width, client_height);
	
}

// It takes a while until we get height and width information.
// When we receive it, reshape the window accordingly.
void LLFloaterTextureZoom::updateDimensions()
{
	if (!mImage)
	{
		return;
	}
	if ((mImage->getFullWidth() * mImage->getFullHeight()) == 0)
	{
		return;
	}
	
	if (mAssetStatus != PREVIEW_ASSET_LOADED)
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	
	// Reshape the floater only when required
	if (mUpdateDimensions)
	{
		mUpdateDimensions = false;
		
		//reshape floater
		reshape(getRect().getWidth(), getRect().getHeight());
		
		gFloaterView->adjustToFitScreen(this, FALSE);
	}
}

void LLFloaterTextureZoom::loadAsset()
{
	mImage = LLViewerTextureManager::getFetchedTexture(mImageID, FTT_DEFAULT, MIPMAP_TRUE, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
	mImageOldBoostLevel = mImage->getBoostLevel();
	mImage->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
	mImage->forceToSaveRawImage(0) ;
	mAssetStatus = PREVIEW_ASSET_LOADING;
	mUpdateDimensions = true;
	updateDimensions();
}

LLPreview::EAssetStatus LLFloaterTextureZoom::getAssetStatus()
{
	if (mImage.notNull() && (mImage->getFullWidth() * mImage->getFullHeight() > 0))
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	return mAssetStatus;
}

void LLFloaterTextureZoom::updateImageID()
{
	const LLViewerInventoryItem *item = static_cast<const LLViewerInventoryItem*>(getItem());
	if(item)
	{
		mImageID = item->getAssetUUID();
	}
	else // not an item, assume it's an asset id
	{
		mImageID = mItemUUID;
	}
}

void LLFloaterTextureZoom::setObjectID(const LLUUID& object_id)
{
	mObjectUUID = object_id;
	
	const LLUUID old_image_id = mImageID;
	
	// Update what image we're pointing to, such as if we just specified the mObjectID
	// that this mItemID is part of.
	updateImageID();
	
	// If the imageID has changed, start over and reload the new image.
	if (mImageID != old_image_id)
	{
		mAssetStatus = PREVIEW_ASSET_UNLOADED;
		loadAsset();
	}
}

BOOL LLFloaterTextureZoom::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_ESCAPE || key == KEY_RETURN)
	{
		closeFloater();
		return TRUE;
	}
	return FALSE;
}

void LLFloaterTextureZoom::onFocusLost()
{
	closeFloater();
}
