// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llfontfreetype.cpp
 * @brief Freetype font library wrapper
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

#include "linden_common.h"

#include "llfontfreetype.h"
#include "llfontgl.h"

// Freetype stuff
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#include "llerror.h"
#include "llimage.h"
//#include "llimagej2c.h"
#include "llmath.h"	// Linden math
#include "llstring.h"
//#include "imdebug.h"
#include "llfontbitmapcache.h"
#include "llgl.h"

LLFontManager *gFontManagerp = nullptr;

FT_Library gFTLibrary = nullptr;

//static
void LLFontManager::initClass()
{
	if (!gFontManagerp) 
	{
		gFontManagerp = new LLFontManager;
	}
}

//static
void LLFontManager::cleanupClass()
{
	delete gFontManagerp;
	gFontManagerp = nullptr;
}

LLFontManager::LLFontManager()
{
	int error;
	error = FT_Init_FreeType(&gFTLibrary);
	if (error)
	{
		// Clean up freetype libs.
		LL_ERRS() << "Freetype initialization failure!" << LL_ENDL;
		FT_Done_FreeType(gFTLibrary);
	}
}

LLFontManager::~LLFontManager()
{
	FT_Done_FreeType(gFTLibrary);
}


LLFontGlyphInfo::LLFontGlyphInfo(U32 index)
:	mGlyphIndex(index),
	mWidth(0),			// In pixels
	mHeight(0),			// In pixels
	mXAdvance(0.f),		// In pixels
	mYAdvance(0.f),		// In pixels
	mXBitmapOffset(0), 	// Offset to the origin in the bitmap
	mYBitmapOffset(0), 	// Offset to the origin in the bitmap
	mXBearing(0),		// Distance from baseline to left in pixels
	mYBearing(0),		// Distance from baseline to top in pixels
	mBitmapNum(0) // Which bitmap in the bitmap cache contains this glyph
{
}

LLFontFreetype::LLFontFreetype()
:	LLTrace::MemTrackable<LLFontFreetype>("LLFontFreetype"),
	mStyle(0),
	mPointSize(0),
	mAscender(0.f),
	mDescender(0.f),
	mLineHeight(0.f),
	mFTFace(nullptr),
	mIsFallback(FALSE),
	mFontBitmapCachep(new LLFontBitmapCache),
	mRenderGlyphCount(0),
	mAddGlyphCount(0)
{
	mCharGlyphInfoMap.reserve(500);
}


LLFontFreetype::~LLFontFreetype()
{
	// Clean up freetype libs.
	if (mFTFace)
		FT_Done_Face(mFTFace);
	mFTFace = nullptr;

	// Delete glyph info
	std::for_each(mCharGlyphInfoMap.begin(), mCharGlyphInfoMap.end(), DeletePairedPointer());
	mCharGlyphInfoMap.clear();

	delete mFontBitmapCachep;
	// mFallbackFonts cleaned up by LLPointer destructor
}

BOOL LLFontFreetype::loadFace(const std::string& filename, F32 point_size, F32 vert_dpi, F32 horz_dpi, S32 components, BOOL is_fallback)
{
	// Don't leak face objects.  This is also needed to deal with
	// changed font file names.
	if (mFTFace)
	{
		FT_Done_Face(mFTFace);
		mFTFace = nullptr;
	}
	
	int error;

	error = FT_New_Face( gFTLibrary,
						 filename.c_str(),
						 0,
						 &mFTFace );

    if (error)
	{
		return FALSE;
	}

	mIsFallback = is_fallback;
	F32 pixels_per_em = (point_size / 72.f)*vert_dpi; // Size in inches * dpi

	error = FT_Set_Char_Size(mFTFace,    /* handle to face object           */
							0,       /* char_width in 1/64th of points  */
							(S32)(point_size*64),   /* char_height in 1/64th of points */
							(U32)horz_dpi,     /* horizontal device resolution    */
							(U32)vert_dpi);   /* vertical device resolution      */

	if (error)
	{
		// Clean up freetype libs.
		FT_Done_Face(mFTFace);
		mFTFace = nullptr;
		return FALSE;
	}

	F32 y_max, y_min, x_max, x_min;
	F32 ems_per_unit = 1.f/ mFTFace->units_per_EM;
	F32 pixels_per_unit = pixels_per_em * ems_per_unit;

	// Get size of bbox in pixels
	y_max = mFTFace->bbox.yMax * pixels_per_unit;
	y_min = mFTFace->bbox.yMin * pixels_per_unit;
	x_max = mFTFace->bbox.xMax * pixels_per_unit;
	x_min = mFTFace->bbox.xMin * pixels_per_unit;
	mAscender = mFTFace->ascender * pixels_per_unit;
	mDescender = -mFTFace->descender * pixels_per_unit;
	mLineHeight = mFTFace->height * pixels_per_unit;

	S32 max_char_width = ll_round(0.5f + (x_max - x_min));
	S32 max_char_height = ll_round(0.5f + (y_max - y_min));

	mFontBitmapCachep->init(components, max_char_width, max_char_height);
	claimMem(mFontBitmapCachep);


	if (!mFTFace->charmap)
	{
		//LL_INFOS() << " no unicode encoding, set whatever encoding there is..." << LL_ENDL;
		FT_Set_Charmap(mFTFace, mFTFace->charmaps[0]);
	}

	if (!mIsFallback)
	{
		// Add the default glyph
		addGlyphFromFont(this, 0, 0);
	}

	mName = filename;
	claimMem(mName);
	mPointSize = point_size;

	mStyle = LLFontGL::NORMAL;
	if(mFTFace->style_flags & FT_STYLE_FLAG_BOLD)
	{
		mStyle |= LLFontGL::BOLD;
		mStyle &= ~LLFontGL::NORMAL;
	}

	if(mFTFace->style_flags & FT_STYLE_FLAG_ITALIC)
	{
		mStyle |= LLFontGL::ITALIC;
		mStyle &= ~LLFontGL::NORMAL;
	}

	return TRUE;
}

void LLFontFreetype::setFallbackFonts(const font_vector_t &font)
{
	mFallbackFonts = font;
}

const LLFontFreetype::font_vector_t &LLFontFreetype::getFallbackFonts() const
{
	return mFallbackFonts;
}

F32 LLFontFreetype::getLineHeight() const
{
	return mLineHeight;
}

F32 LLFontFreetype::getAscenderHeight() const
{
	return mAscender;
}

F32 LLFontFreetype::getDescenderHeight() const
{
	return mDescender;
}

F32 LLFontFreetype::getXAdvance(llwchar wch) const
{
	if (mFTFace == nullptr)
		return 0.0;

	// Return existing info only if it is current
	LLFontGlyphInfo* gi = getGlyphInfo(wch);
	if (gi)
	{
		return gi->mXAdvance;
	}
	else
	{
		char_glyph_info_map_t::iterator found_it = mCharGlyphInfoMap.find((llwchar)0);
		if (found_it != mCharGlyphInfoMap.end())
		{
			return found_it->second->mXAdvance;
		}
	}

	// Last ditch fallback - no glyphs defined at all.
	return (F32)mFontBitmapCachep->getMaxCharWidth();
}

F32 LLFontFreetype::getXAdvance(const LLFontGlyphInfo* glyph) const
{
	if (mFTFace == nullptr)
		return 0.0;

	return glyph->mXAdvance;
}

F32 LLFontFreetype::getXKerning(llwchar char_left, llwchar char_right) const
{
	if (mFTFace == nullptr)
		return 0.0;

	//llassert(!mIsFallback);
	LLFontGlyphInfo* left_glyph_info = getGlyphInfo(char_left);;
	U32 left_glyph = left_glyph_info ? left_glyph_info->mGlyphIndex : 0;
	// Kern this puppy.
	LLFontGlyphInfo* right_glyph_info = getGlyphInfo(char_right);
	U32 right_glyph = right_glyph_info ? right_glyph_info->mGlyphIndex : 0;

	F32 kerning = 0.0f;
	if (getKerningCache(left_glyph,  right_glyph, kerning))
		return kerning;

	FT_Vector  delta;

	llverify(!FT_Get_Kerning(mFTFace, left_glyph, right_glyph, ft_kerning_unfitted, &delta));

	kerning = delta.x*(1.f/64.f);
	setKerningCache(left_glyph, right_glyph, kerning);
	return kerning;
}

F32 LLFontFreetype::getXKerning(const LLFontGlyphInfo* left_glyph_info, const LLFontGlyphInfo* right_glyph_info) const
{
	if (mFTFace == nullptr)
		return 0.0;

	U32 left_glyph = left_glyph_info ? left_glyph_info->mGlyphIndex : 0;
	U32 right_glyph = right_glyph_info ? right_glyph_info->mGlyphIndex : 0;

	F32 kerning = 0.0f;
	if (getKerningCache(left_glyph,  right_glyph, kerning))
		return kerning;

	FT_Vector  delta;

	llverify(!FT_Get_Kerning(mFTFace, left_glyph, right_glyph, ft_kerning_unfitted, &delta));

	kerning = delta.x*(1.f/64.f);
	setKerningCache(left_glyph, right_glyph, kerning);
	return kerning;
}

BOOL LLFontFreetype::hasGlyph(llwchar wch) const
{
	llassert(!mIsFallback);
	return(mCharGlyphInfoMap.find(wch) != mCharGlyphInfoMap.end());
}

LLFontGlyphInfo* LLFontFreetype::addGlyph(llwchar wch) const
{
	if (mFTFace == nullptr)
		return FALSE;

	llassert(!mIsFallback);
	//LL_DEBUGS() << "Adding new glyph for " << wch << " to font" << LL_ENDL;

	FT_UInt glyph_index;

	// Initialize char to glyph map
	glyph_index = FT_Get_Char_Index(mFTFace, wch);
	if (glyph_index == 0)
	{
		//LL_INFOS() << "Trying to add glyph from fallback font!" << LL_ENDL;
		for(auto iter = mFallbackFonts.begin(); iter != mFallbackFonts.end(); ++iter)
		{
			glyph_index = FT_Get_Char_Index((*iter)->mFTFace, wch);
			if (glyph_index)
			{
				return addGlyphFromFont(*iter, wch, glyph_index);
			}
		}
	}
	
	char_glyph_info_map_t::iterator iter = mCharGlyphInfoMap.find(wch);
	if (iter == mCharGlyphInfoMap.end())
	{
		return addGlyphFromFont(this, wch, glyph_index);
	}
	return nullptr;
}

LLFontGlyphInfo* LLFontFreetype::addGlyphFromFont(const LLFontFreetype *fontp, llwchar wch, U32 glyph_index) const
{
	if (mFTFace == nullptr)
		return nullptr;

	llassert(!mIsFallback);
	fontp->renderGlyph(glyph_index);
	S32 width = fontp->mFTFace->glyph->bitmap.width;
	S32 height = fontp->mFTFace->glyph->bitmap.rows;

	S32 pos_x, pos_y;
	S32 bitmap_num;
	mFontBitmapCachep->nextOpenPos(width, pos_x, pos_y, bitmap_num);
	mAddGlyphCount++;

	LLFontGlyphInfo* gi = new LLFontGlyphInfo(glyph_index);
	gi->mXBitmapOffset = pos_x;
	gi->mYBitmapOffset = pos_y;
	gi->mBitmapNum = bitmap_num;
	gi->mWidth = width;
	gi->mHeight = height;
	gi->mXBearing = fontp->mFTFace->glyph->bitmap_left;
	gi->mYBearing = fontp->mFTFace->glyph->bitmap_top;
	// Convert these from 26.6 units to float pixels.
	gi->mXAdvance = fontp->mFTFace->glyph->advance.x / 64.f;
	gi->mYAdvance = fontp->mFTFace->glyph->advance.y / 64.f;

	insertGlyphInfo(wch, gi);

	llassert(fontp->mFTFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO
	    || fontp->mFTFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);

	if (fontp->mFTFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO
	    || fontp->mFTFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
	{
		U8 *buffer_data = fontp->mFTFace->glyph->bitmap.buffer;
		S32 buffer_row_stride = fontp->mFTFace->glyph->bitmap.pitch;
		U8 *tmp_graydata = nullptr;

		if (fontp->mFTFace->glyph->bitmap.pixel_mode
		    == FT_PIXEL_MODE_MONO)
		{
			// need to expand 1-bit bitmap to 8-bit graymap.
			tmp_graydata = new U8[width * height];
			S32 xpos, ypos;
			for (ypos = 0; ypos < height; ++ypos)
			{
				S32 bm_row_offset = buffer_row_stride * ypos;
				for (xpos = 0; xpos < width; ++xpos)
				{
					U32 bm_col_offsetbyte = xpos / 8;
					U32 bm_col_offsetbit = 7 - (xpos % 8);
					U32 bit =
					!!(buffer_data[bm_row_offset
						       + bm_col_offsetbyte
					   ] & (1 << bm_col_offsetbit) );
					tmp_graydata[width*ypos + xpos] =
						255 * bit;
				}
			}
			// use newly-built graymap.
			buffer_data = tmp_graydata;
			buffer_row_stride = width;
		}

		switch (mFontBitmapCachep->getNumComponents())
		{
		case 1:
			mFontBitmapCachep->getImageRaw(bitmap_num)->setSubImage(pos_x,
																	pos_y,
																	width,
																	height,
																	buffer_data,
																	buffer_row_stride,
																	TRUE);
			break;
		case 2:
			setSubImageLuminanceAlpha(pos_x,	
									  pos_y,
									  bitmap_num,
									  width,
									  height,
									  buffer_data,
									  buffer_row_stride);
			break;
		default:
			break;
		}

		if (tmp_graydata)
			delete[] tmp_graydata;
	} else {
		// we don't know how to handle this pixel format from FreeType;
		// omit it from the font-image.
	}
	
	LLImageGL *image_gl = mFontBitmapCachep->getImageGL(bitmap_num);
	LLImageRaw *image_raw = mFontBitmapCachep->getImageRaw(bitmap_num);
	image_gl->setSubImage(image_raw, 0, 0, image_gl->getWidth(), image_gl->getHeight());

	return gi;
}

LLFontGlyphInfo* LLFontFreetype::getGlyphInfo(llwchar wch) const
{
	char_glyph_info_map_t::iterator iter = mCharGlyphInfoMap.find(wch);
	if (iter != mCharGlyphInfoMap.end())
	{
		return iter->second;
	}
	else
	{
		// this glyph doesn't yet exist, so render it and return the result
		return addGlyph(wch);
	}
}

void LLFontFreetype::insertGlyphInfo(llwchar wch, LLFontGlyphInfo* gi) const
{
	char_glyph_info_map_t::iterator iter = mCharGlyphInfoMap.find(wch);
	if (iter != mCharGlyphInfoMap.end())
	{
		delete iter->second;
		iter->second = gi;
	}
	else
	{
		claimMem(gi);
		mCharGlyphInfoMap[wch] = gi;
	}
}

void LLFontFreetype::renderGlyph(U32 glyph_index) const
{
	if (mFTFace == nullptr)
		return;

	if (FT_Load_Glyph(mFTFace, glyph_index, FT_LOAD_RENDER) != 0)
	{
		// If glyph fails to load and/or render, render a fallback character
		llassert_always(!FT_Load_Char(mFTFace, L'?', FT_LOAD_RENDER));
	}

	mRenderGlyphCount++;
}

void LLFontFreetype::reset(F32 vert_dpi, F32 horz_dpi)
{
	resetBitmapCache(); 
	loadFace(mName, mPointSize, vert_dpi ,horz_dpi, mFontBitmapCachep->getNumComponents(), mIsFallback);
	if (!mIsFallback)
	{
		// This is the head of the list - need to rebuild ourself and all fallbacks.
		if (mFallbackFonts.empty())
		{
			LL_WARNS() << "LLFontGL::reset(), no fallback fonts present" << LL_ENDL;
		}
		else
		{
			for(font_vector_t::iterator it = mFallbackFonts.begin();
				it != mFallbackFonts.end();
				++it)
			{
				(*it)->reset(vert_dpi, horz_dpi);
			}
		}
	}
}

void LLFontFreetype::resetBitmapCache()
{
	for (char_glyph_info_map_t::iterator it = mCharGlyphInfoMap.begin(), end_it = mCharGlyphInfoMap.end();
		it != end_it;
		++it)
	{
		disclaimMem(it->second);
		delete it->second;
	}
	mCharGlyphInfoMap.clear();
	disclaimMem(mFontBitmapCachep);
	mFontBitmapCachep->reset();

	// Adding default glyph is skipped for fallback fonts here as well as in loadFace(). 
	// This if was added as fix for EXT-4971.
	if(!mIsFallback)
	{
		// Add the empty glyph
		addGlyphFromFont(this, 0, 0);
	}
}

void LLFontFreetype::destroyGL()
{
	mFontBitmapCachep->destroyGL();
}

const std::string &LLFontFreetype::getName() const
{
	return mName;
}

const LLFontBitmapCache* LLFontFreetype::getFontBitmapCache() const
{
	return mFontBitmapCachep;
}

void LLFontFreetype::setStyle(U8 style)
{
	mStyle = style;
}

U8 LLFontFreetype::getStyle() const
{
	return mStyle;
}

std::string LLFontFreetype::getVersionString()
{
	if (gFTLibrary != nullptr)
	{
		int major, minor, patch;
		FT_Library_Version(gFTLibrary, &major, &minor, &patch);
		return std::string(llformat("FreeType %i.%i.%i", major, minor, patch));
	}
	else
	{
		return std::string();
	}
}

void LLFontFreetype::setSubImageLuminanceAlpha(U32 x, U32 y, U32 bitmap_num, U32 width, U32 height, U8 *data, S32 stride) const
{
	LLImageRaw *image_raw = mFontBitmapCachep->getImageRaw(bitmap_num);

	llassert(!mIsFallback);
	llassert(image_raw && (image_raw->getComponents() == 2));

	
	U8 *target = image_raw->getData();

	if (!data)
	{
		return;
	}

	if (0 == stride)
		stride = width;

	U32 i, j;
	U32 to_offset;
	U32 from_offset;
	U32 target_width = image_raw->getWidth();
	for (i = 0; i < height; i++)
	{
		to_offset = (y + i)*target_width + x;
		from_offset = (height - 1 - i)*stride;
		for (j = 0; j < width; j++)
		{
			*(target + to_offset*2 + 1) = *(data + from_offset);
			to_offset++;
			from_offset++;
		}
	}
}

static inline U64 kerning_cache_key(const U32 left_glyph, const U32 right_glyph)
{
	return (((U64)left_glyph) << 32) | right_glyph;
}

bool LLFontFreetype::getKerningCache(U32 left_glyph, U32 right_glyph, F32& kerning) const
{
	auto iter = mKerningCache.find(kerning_cache_key(left_glyph, right_glyph));
	if (iter == mKerningCache.end())
		return false;
	kerning = iter->second;
	return true;
}

void LLFontFreetype::setKerningCache(U32 left_glyph, U32 right_glyph, F32 kerning) const
{
	// reserve memory to prevent multiple allocations
	// do this here instead of the constructor to save memory on unused fonts
	if (mKerningCache.capacity() < 500)
		mKerningCache.reserve(500);
	mKerningCache.emplace(kerning_cache_key(left_glyph, right_glyph), kerning);
}
