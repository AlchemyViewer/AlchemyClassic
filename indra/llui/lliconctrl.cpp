// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file lliconctrl.cpp
 * @brief LLIconCtrl base class
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

#include "lliconctrl.h"

// Linden library includes 

// Project includes
#include "llui.h"
#include "lluictrlfactory.h"

static LLDefaultChildRegistry::Register<LLIconCtrl> r("icon");

LLIconCtrl::Params::Params()
:	image("image_name"),
	color("color"),
	use_draw_context_alpha("use_draw_context_alpha", true),
	min_width("min_width", 0),
	min_height("min_height", 0),
	scale_image("scale_image")
{}

LLIconCtrl::LLIconCtrl(const LLIconCtrl::Params& p)
:	LLUICtrl(p),
	mPriority(0),
	mMinWidth(p.min_width),
	mMinHeight(p.min_height),
	mMaxWidth(0),
	mMaxHeight(0),
	mUseDrawContextAlpha(p.use_draw_context_alpha),
	mColor(p.color()),
	mImagep(p.image)
{
	if (mImagep.notNull())
	{
		LLUICtrl::setValue(mImagep->getName());
	}
}

LLIconCtrl::~LLIconCtrl()
{
	mImagep = nullptr;
}


void LLIconCtrl::draw()
{
	if( mImagep.notNull() )
	{
		const F32 alpha = mUseDrawContextAlpha ? getDrawContext().mAlpha : getCurrentTransparency();
		mImagep->draw(getLocalRect(), mColor.get() % alpha );
	}

	LLUICtrl::draw();
}

// virtual
// value might be a string or a UUID
void LLIconCtrl::setValue(const LLSD& value)
{
    setValue(value, mPriority);
}

void LLIconCtrl::setValue(const LLSD& value, S32 priority)
{
	LLSD tvalue(value);
	if (value.isString() && LLUUID::validate(value.asString()))
	{
		//RN: support UUIDs masquerading as strings
		tvalue = LLSD(LLUUID(value.asString()));
	}
	LLUICtrl::setValue(tvalue);
	if (tvalue.isUUID())
	{
        mImagep = LLUI::getUIImageByID(tvalue.asUUID(), priority);
	}
	else
	{
        mImagep = LLUI::getUIImage(tvalue.asString(), priority);
	}

	if(mImagep.notNull() 
		&& mImagep->getImage().notNull() 
		&& mMinWidth 
		&& mMinHeight)
	{
        S32 desired_draw_width = llmax(mMinWidth, mImagep->getWidth());
        S32 desired_draw_height = llmax(mMinHeight, mImagep->getHeight());
        if (mMaxWidth)
        {
            desired_draw_width = llmin(desired_draw_width, mMaxWidth);
        }
        if (mMaxHeight)
        {
            desired_draw_height = llmin(desired_draw_height, mMaxHeight);
        }
        
        mImagep->getImage()->setKnownDrawSize(desired_draw_width, desired_draw_height);
	}
}

std::string LLIconCtrl::getImageName() const
{
	if (getValue().isString())
		return getValue().asString();
	else
		return std::string();
}


