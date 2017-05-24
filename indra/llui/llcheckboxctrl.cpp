// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llcheckboxctrl.cpp
 * @brief LLCheckBoxCtrl base class
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

// The mutants are coming!
#include "linden_common.h"

#define LLCHECKBOXCTRL_CPP
#include "llcheckboxctrl.h"

#include "llgl.h"
#include "llui.h"
#include "lluiconstants.h"
#include "lluictrlfactory.h"
#include "llcontrol.h"

#include "llstring.h"
#include "llfontgl.h"
#include "lltextbox.h"
#include "llkeyboard.h"

static LLDefaultChildRegistry::Register<LLCheckBoxCtrl> r("check_box");

// Compiler optimization, generate extern template
template class LLCheckBoxCtrl* LLView::getChild<class LLCheckBoxCtrl>(
	const std::string& name, BOOL recurse) const;

LLCheckBoxCtrl::Params::Params()
:	initial_value("initial_value", false),
	label_text("label_text"),
	check_button("check_button"),
	radio_style("radio_style")
{}


LLCheckBoxCtrl::LLCheckBoxCtrl(const LLCheckBoxCtrl::Params& p)
:	LLUICtrl(p),
	mFont(p.font()),
	mTextEnabledColor(p.label_text.text_color()),
	mTextDisabledColor(p.label_text.text_readonly_color())
{
	mViewModel->setValue(LLSD(p.initial_value));
	mViewModel->resetDirty();
	static LLUICachedControl<S32> llcheckboxctrl_spacing ("UICheckboxctrlSpacing", 0);
	static LLUICachedControl<S32> llcheckboxctrl_hpad ("UICheckboxctrlHPad", 0);
	static LLUICachedControl<S32> llcheckboxctrl_vpad ("UICheckboxctrlVPad", 0);
	static LLUICachedControl<S32> llcheckboxctrl_btn_size ("UICheckboxctrlBtnSize", 0);

	// must be big enough to hold all children
	setUseBoundingRect(TRUE);

	// *HACK Get rid of this with SL-55508... 
	// this allows blank check boxes and radio boxes for now
	std::string local_label = p.label;
	if(local_label.empty())
	{
		local_label = " ";
	}

	LLTextBox::Params tbparams = p.label_text;
	tbparams.initial_value(local_label);
	if (p.font.isProvided())
	{
		tbparams.font(p.font);
	}
	mLabel = LLUICtrlFactory::create<LLTextBox> (tbparams);
	mLabel->reshapeToFitText();
	addChild(mLabel);

	LLRect label_rect = mLabel->getRect();

	// Button
	// Note: button cover the label by extending all the way to the right.
	LLRect btn_rect = p.check_button.rect();
	btn_rect.setOriginAndSize(
		btn_rect.mLeft,
		btn_rect.mBottom,
		llmax(btn_rect.mRight, label_rect.mRight - btn_rect.mLeft),
		llmax( label_rect.getHeight(), btn_rect.mTop));

	LLButton::Params params = p.check_button;
	params.rect(btn_rect);
	//params.control_name(p.control_name);
	params.click_callback.function(boost::bind(&LLCheckBoxCtrl::onCommit, this));
	params.commit_on_return(false);
	// Checkboxes only allow boolean initial values, but buttons can
	// take any LLSD.
	params.initial_value(LLSD(p.initial_value));
	params.follows.flags(FOLLOWS_LEFT | FOLLOWS_BOTTOM);

	mButton = LLUICtrlFactory::create<LLButton>(params);
	addChild(mButton);
}

LLCheckBoxCtrl::~LLCheckBoxCtrl()
{
	// Children all cleaned up by default view destructor.
}

void LLCheckBoxCtrl::onCommit()
{
	if( getEnabled() )
	{
		setTentative(FALSE);
		setControlValue(getValue());
		LLUICtrl::onCommit();
	}
}

void LLCheckBoxCtrl::setEnabled(BOOL b)
{
	LLView::setEnabled(b);

	if (b)
	{
		mLabel->setColor( mTextEnabledColor.get() );
	}
	else
	{
		mLabel->setColor( mTextDisabledColor.get() );
	}
}

void LLCheckBoxCtrl::clear()
{
	setValue( FALSE );
}

void LLCheckBoxCtrl::reshape(S32 width, S32 height, BOOL called_from_parent)
{

	mLabel->reshapeToFitText();

	LLRect label_rect = mLabel->getRect();

	// Button
	// Note: button cover the label by extending all the way to the right.
	LLRect btn_rect = mButton->getRect();
	btn_rect.setOriginAndSize(
		btn_rect.mLeft,
		btn_rect.mBottom,
		llmax(btn_rect.getWidth(), label_rect.mRight - btn_rect.mLeft),
		llmax(label_rect.mTop - btn_rect.mBottom, btn_rect.getHeight()));
	mButton->setShape(btn_rect);
}

//virtual
void LLCheckBoxCtrl::setValue(const LLSD& value )
{
	mButton->setValue( value );
}

//virtual
LLSD LLCheckBoxCtrl::getValue() const
{
	return mButton->getValue();
}

//virtual
void LLCheckBoxCtrl::setTentative(BOOL b)
{
	mButton->setTentative(b);
}

//virtual
BOOL LLCheckBoxCtrl::getTentative() const
{
	return mButton->getTentative();
}

void LLCheckBoxCtrl::setLabel( const LLStringExplicit& label )
{
	mLabel->setText( label );
	reshape(getRect().getWidth(), getRect().getHeight(), FALSE);
}

std::string LLCheckBoxCtrl::getLabel() const
{
	return mLabel->getText();
}

BOOL LLCheckBoxCtrl::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	BOOL res = mLabel->setTextArg(key, text);
	reshape(getRect().getWidth(), getRect().getHeight(), FALSE);
	return res;
}

// virtual
void LLCheckBoxCtrl::setControlName(const std::string& control_name, LLView* context)
{
	mButton->setControlName(control_name, context);
}


// virtual		Returns TRUE if the user has modified this control.
BOOL	 LLCheckBoxCtrl::isDirty() const
{
	if ( mButton )
	{
		return mButton->isDirty();
	}
	return FALSE;		// Shouldn't get here
}


// virtual			Clear dirty state
void	LLCheckBoxCtrl::resetDirty()
{
	if ( mButton )
	{
		mButton->resetDirty();
	}
}
