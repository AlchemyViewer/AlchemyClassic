// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/** 
 * @file llbutton.cpp
 * @brief LLButton base class
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

#define LLBUTTON_CPP
#include "llbutton.h"

// Linden library includes
#include "v4color.h"
#include "llstring.h"

// Project includes
#include "llkeyboard.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llresmgr.h"
#include "llcriticaldamp.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llwindow.h"
#include "llnotificationsutil.h"
#include "llrender.h"
#include "lluictrlfactory.h"
#include "llhelp.h"
#include "lldockablefloater.h"
#include "llviewereventrecorder.h"

static LLDefaultChildRegistry::Register<LLButton> r("button");

// Compiler optimization, generate extern template
template class LLButton* LLView::getChild<class LLButton>(
	const std::string& name, BOOL recurse) const;

// globals loaded from settings.xml
S32	LLBUTTON_H_PAD	= 0;
S32 BTN_HEIGHT_SMALL= 0;
S32 BTN_HEIGHT		= 0;

LLButton::Params::Params()
:	label_selected("label_selected"),				// requires is_toggle true
	label_shadow("label_shadow", true),
	auto_resize("auto_resize", false),
	use_ellipses("use_ellipses", false),
	image_unselected("image_unselected"),
	image_selected("image_selected"),
	image_hover_selected("image_hover_selected"),
	image_hover_unselected("image_hover_unselected"),
	image_disabled_selected("image_disabled_selected"),
	image_disabled("image_disabled"),
	image_pressed("image_pressed"),
	image_pressed_selected("image_pressed_selected"),
	image_overlay("image_overlay"),
	image_overlay_alignment("image_overlay_alignment", std::string("center")),
	image_overlay_enable("image_overlay_enable", true),
	label_color("label_color"),
	label_color_selected("label_color_selected"),
	label_color_disabled("label_color_disabled"),
	label_color_disabled_selected("label_color_disabled_selected"),
	image_color("image_color"),	// requires is_toggle true
	image_color_disabled("image_color_disabled"),
	image_overlay_color("image_overlay_color", LLColor4::white % 0.75f),
	image_overlay_selected_color("image_overlay_selected_color", LLColor4::white),
	image_overlay_disabled_color("image_overlay_disabled_color", LLColor4::white % 0.3f),
	flash_color("flash_color"),
	pad_right("pad_right", LLUI::sSettingGroups["config"]->getS32("ButtonHPad")),
	pad_left("pad_left", LLUI::sSettingGroups["config"]->getS32("ButtonHPad")),
	pad_bottom("pad_bottom"),
	image_top_pad("image_top_pad"),
	image_bottom_pad("image_bottom_pad"),
	imgoverlay_label_space("imgoverlay_label_space", 1),
	click_callback("click_callback"),
	mouse_down_callback("mouse_down_callback"),
	mouse_up_callback("mouse_up_callback"),
	mouse_held_callback("mouse_held_callback"),
	is_toggle("is_toggle", false),
	scale_image("scale_image", true),
	commit_on_return("commit_on_return", true),
	display_pressed_state("display_pressed_state", true),
	hover_glow_amount("hover_glow_amount"),
	held_down_delay("held_down_delay"),
	use_draw_context_alpha("use_draw_context_alpha", true),
	badge("badge"),
	handle_right_mouse("handle_right_mouse"),
	button_flash_enable("button_flash_enable", false),
	button_flash_count("button_flash_count"),
	button_flash_rate("button_flash_rate")
{
	addSynonym(is_toggle, "toggle");
	changeDefault(initial_value, LLSD(false));
}


LLButton::LLButton(const LLButton::Params& p)
:	LLUICtrl(p),
	LLBadgeOwner(getHandle()),
	mNeedsHighlight(FALSE),
	mMouseDownSignal(nullptr),
	mMouseUpSignal(nullptr),
	mHeldDownSignal(nullptr),
	mGLFont(p.font),
	mMouseDownFrame(0),
	mMouseHeldDownCount(0),
	mHeldDownDelay(p.held_down_delay.seconds),
	mHeldDownFrameDelay(p.held_down_delay.frames),
	mLastDrawCharsCount(0),			// seconds until held-down callback is called
	mImageOverlayEnable(p.image_overlay_enable),
	mImageOverlay(p.image_overlay()),
	mImageOverlayAlignment(LLFontGL::hAlignFromName(p.image_overlay_alignment)),
	mImageOverlayColor(p.image_overlay_color()),
	mImageOverlaySelectedColor(p.image_overlay_selected_color()),
	mImageOverlayDisabledColor(p.image_overlay_disabled_color()),
	mImageUnselected(p.image_unselected),
	mUnselectedLabel(p.label()),
	mUnselectedLabelColor(p.label_color()),
	mImageSelected(p.image_selected),
	mSelectedLabel(p.label_selected()),
	mSelectedLabelColor(p.label_color_selected()),
	mImageHoverSelected(p.image_hover_selected),
	mImageHoverUnselected(p.image_hover_unselected),
	mImageDisabled(p.image_disabled),
	mDisabledLabelColor(p.label_color_disabled()),
	mImageDisabledSelected(p.image_disabled_selected),
	mDisabledSelectedLabelColor(p.label_color_disabled_selected()),
	mImagePressed(p.image_pressed),
	mImagePressedSelected(p.image_pressed_selected),
	mImageFlash(p.image_flash),
	mFlashBgColor(p.flash_color()),
	mImageColor(p.image_color()),
	mDisabledImageColor(p.image_color_disabled()),
	mIsToggle(p.is_toggle),
	mScaleImage(p.scale_image),
	mDropShadowedText(p.label_shadow),
	mAutoResize(p.auto_resize),
	mUseEllipses( p.use_ellipses ),
	mBorderEnabled( FALSE ),
	mFlashing( FALSE ),
	mHAlign(p.font_halign),
	mLeftHPad(p.pad_left),
	mRightHPad(p.pad_right),
	mBottomVPad(p.pad_bottom),
	mImageOverlayTopPad(p.image_top_pad),
	mImageOverlayBottomPad(p.image_bottom_pad),
	mUseDrawContextAlpha(p.use_draw_context_alpha),
	mImgOverlayLabelSpace(p.imgoverlay_label_space),
	mHoverGlowStrength(p.hover_glow_amount),
	mCurGlowStrength(0.f),
	mCommitOnReturn(p.commit_on_return),
	mFadeWhenDisabled(FALSE),
	mForcePressedState(false),
	mDisplayPressedState(p.display_pressed_state),
	mFlashingTimer(nullptr),
	mHandleRightMouse(p.handle_right_mouse)
{
	if (p.button_flash_enable)
	{
		// If optional parameter "p.button_flash_count" is not provided, LLFlashTimer will be
		// used instead it a "default" value from gSavedSettings.getS32("FlashCount")).
		// Likewise, missing "p.button_flash_rate" is replaced by gSavedSettings.getF32("FlashPeriod").
		// Note: flashing should be allowed in settings.xml (boolean key "EnableButtonFlashing").
		S32 flash_count = p.button_flash_count.isProvided()? p.button_flash_count : 0;
		F32 flash_rate = p.button_flash_rate.isProvided()? p.button_flash_rate : 0.0;
		mFlashingTimer = new LLFlashTimer ((LLFlashTimer::callback_t)NULL, flash_count, flash_rate);
	}
	else
	{
		mButtonFlashCount = p.button_flash_count;
		mButtonFlashRate = p.button_flash_rate;
	}

	static LLUICachedControl<S32> llbutton_orig_h_pad ("UIButtonOrigHPad", 0);
	static Params default_params(LLUICtrlFactory::getDefaultParams<LLButton>());

	if (!p.label_selected.isProvided())
	{
		mSelectedLabel = mUnselectedLabel;
	}

	// Hack to make sure there is space for at least one character
	if (getRect().getWidth() - (mRightHPad + mLeftHPad) < mGLFont->getWidth(std::string(" ")))
	{
		// Use old defaults
		mLeftHPad = llbutton_orig_h_pad;
		mRightHPad = llbutton_orig_h_pad;
	}
	
	mMouseDownTimer.stop();

	// if custom unselected button image provided...
	if (p.image_unselected != default_params.image_unselected)
	{
		//...fade it out for disabled image by default...
		if (p.image_disabled() == default_params.image_disabled() )
		{
			mImageDisabled = p.image_unselected;
			mFadeWhenDisabled = TRUE;
		}

		if (p.image_pressed_selected == default_params.image_pressed_selected)
		{
			mImagePressedSelected = mImageUnselected;
		}
	}

	// if custom selected button image provided...
	if (p.image_selected != default_params.image_selected)
	{
		//...fade it out for disabled image by default...
		if (p.image_disabled_selected() == default_params.image_disabled_selected())
		{
			mImageDisabledSelected = p.image_selected;
			mFadeWhenDisabled = TRUE;
		}

		if (p.image_pressed == default_params.image_pressed)
		{
			mImagePressed = mImageSelected;
		}
	}

	if (!p.image_pressed.isProvided())
	{
		mImagePressed = mImageSelected;
	}

	if (!p.image_pressed_selected.isProvided())
	{
		mImagePressedSelected = mImageUnselected;
	}
	
	if (mImageUnselected.isNull())
	{
		LL_WARNS() << "Button: " << getName() << " with no image!" << LL_ENDL;
	}
	
	if (p.click_callback.isProvided())
	{
		setCommitCallback(initCommitCallback(p.click_callback)); // alias -> commit_callback
	}
	if (p.mouse_down_callback.isProvided())
	{
		setMouseDownCallback(initCommitCallback(p.mouse_down_callback));
	}
	if (p.mouse_up_callback.isProvided())
	{
		setMouseUpCallback(initCommitCallback(p.mouse_up_callback));
	}
	if (p.mouse_held_callback.isProvided())
	{
		setHeldDownCallback(initCommitCallback(p.mouse_held_callback));
	}

	if (p.badge.isProvided())
	{
		LLBadgeOwner::initBadgeParams(p.badge());
	}
}

LLButton::~LLButton()
{
	delete mMouseDownSignal;
	delete mMouseUpSignal;
	delete mHeldDownSignal;

	if (mFlashingTimer)
	{
		mFlashingTimer->unset();
	}
}

// HACK: Committing a button is the same as instantly clicking it.
// virtual
void LLButton::onCommit()
{
	// WARNING: Sometimes clicking a button destroys the floater or
	// panel containing it.  Therefore we need to call 	LLUICtrl::onCommit()
	// LAST, otherwise this becomes deleted memory.

	if (mMouseDownSignal) (*mMouseDownSignal)(this, LLSD());
	
	if (mMouseUpSignal) (*mMouseUpSignal)(this, LLSD());

	if (getSoundFlags() & MOUSE_DOWN)
	{
		make_ui_sound("UISndClick");
	}

	if (getSoundFlags() & MOUSE_UP)
	{
		make_ui_sound("UISndClickRelease");
	}

	if (mIsToggle)
	{
		toggleState();
	}

	// do this last, as it can result in destroying this button
	LLUICtrl::onCommit();
}

boost::signals2::connection LLButton::setClickedCallback(const CommitCallbackParam& cb)
{
	return setClickedCallback(initCommitCallback(cb));
}
boost::signals2::connection LLButton::setMouseDownCallback(const CommitCallbackParam& cb)
{
	return setMouseDownCallback(initCommitCallback(cb));
}
boost::signals2::connection LLButton::setMouseUpCallback(const CommitCallbackParam& cb)
{
	return setMouseUpCallback(initCommitCallback(cb));
}
boost::signals2::connection LLButton::setHeldDownCallback(const CommitCallbackParam& cb)
{
	return setHeldDownCallback(initCommitCallback(cb));
}


boost::signals2::connection LLButton::setClickedCallback( const commit_signal_t::slot_type& cb )
{
	if (!mCommitSignal) mCommitSignal = new commit_signal_t();
	return mCommitSignal->connect(cb);
}
boost::signals2::connection LLButton::setMouseDownCallback( const commit_signal_t::slot_type& cb )
{
	if (!mMouseDownSignal) mMouseDownSignal = new commit_signal_t();
	return mMouseDownSignal->connect(cb);
}
boost::signals2::connection LLButton::setMouseUpCallback( const commit_signal_t::slot_type& cb )
{
	if (!mMouseUpSignal) mMouseUpSignal = new commit_signal_t();
	return mMouseUpSignal->connect(cb);
}
boost::signals2::connection LLButton::setHeldDownCallback( const commit_signal_t::slot_type& cb )
{
	if (!mHeldDownSignal) mHeldDownSignal = new commit_signal_t();
	return mHeldDownSignal->connect(cb);
}


// *TODO: Deprecate (for backwards compatibility only)
boost::signals2::connection LLButton::setClickedCallback( button_callback_t cb, void* data )
{
	return setClickedCallback(boost::bind(cb, data));
}
boost::signals2::connection LLButton::setMouseDownCallback( button_callback_t cb, void* data )
{
	return setMouseDownCallback(boost::bind(cb, data));
}
boost::signals2::connection LLButton::setMouseUpCallback( button_callback_t cb, void* data )
{
	return setMouseUpCallback(boost::bind(cb, data));
}
boost::signals2::connection LLButton::setHeldDownCallback( button_callback_t cb, void* data )
{
	return setHeldDownCallback(boost::bind(cb, data));
}

BOOL LLButton::postBuild()
{
	autoResize();

	addBadgeToParentHolder();

	return LLUICtrl::postBuild();
}

BOOL LLButton::handleUnicodeCharHere(llwchar uni_char)
{
	BOOL handled = FALSE;
	if(' ' == uni_char 
		&& !gKeyboard->getKeyRepeated(' '))
	{
		if (mIsToggle)
		{
			toggleState();
		}

		LLUICtrl::onCommit();
		
		handled = TRUE;		
	}
	return handled;	
}

BOOL LLButton::handleKeyHere(KEY key, MASK mask )
{
	BOOL handled = FALSE;
	if( mCommitOnReturn && KEY_RETURN == key && mask == MASK_NONE && !gKeyboard->getKeyRepeated(key))
	{
		if (mIsToggle)
		{
			toggleState();
		}

		handled = TRUE;

		LLUICtrl::onCommit();
	}
	return handled;
}


BOOL LLButton::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (!childrenHandleMouseDown(x, y, mask))
	{
		// Route future Mouse messages here preemptively.  (Release on mouse up.)
		gFocusMgr.setMouseCapture( this );

		if (hasTabStop() && !getIsChrome())
		{
			setFocus(TRUE);
		}

		/*
		 * ATTENTION! This call fires another mouse down callback.
		 * If you wish to remove this call emit that signal directly
		 * by calling LLUICtrl::mMouseDownSignal(x, y, mask);
		 */
		LLUICtrl::handleMouseDown(x, y, mask);

		LLViewerEventRecorder::instance().updateMouseEventInfo(x,y,-55,-55,getPathname());

		if(mMouseDownSignal) (*mMouseDownSignal)(this, LLSD());

		mMouseDownTimer.start();
		mMouseDownFrame = (S32) LLFrameTimer::getFrameCount();
		mMouseHeldDownCount = 0;

		
		if (getSoundFlags() & MOUSE_DOWN)
		{
			make_ui_sound("UISndClick");
		}
	}
	return TRUE;
}


BOOL LLButton::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// We only handle the click if the click both started and ended within us
	if( hasMouseCapture() )
	{
		// Always release the mouse
		gFocusMgr.setMouseCapture(nullptr );

		/*
		 * ATTENTION! This call fires another mouse up callback.
		 * If you wish to remove this call emit that signal directly
		 * by calling LLUICtrl::mMouseUpSignal(x, y, mask);
		 */
		LLUICtrl::handleMouseUp(x, y, mask);
		LLViewerEventRecorder::instance().updateMouseEventInfo(x,y,-55,-55,getPathname()); 

		// Regardless of where mouseup occurs, handle callback
		if(mMouseUpSignal) (*mMouseUpSignal)(this, LLSD());

		resetMouseDownTimer();

		// DO THIS AT THE VERY END to allow the button to be destroyed as a result of being clicked.
		// If mouseup in the widget, it's been clicked
		if (pointInView(x, y))
		{
			if (getSoundFlags() & MOUSE_UP)
			{
				make_ui_sound("UISndClickRelease");
			}

			if (mIsToggle)
			{
				toggleState();
			}

			LLUICtrl::onCommit();
		}
	}
	else
	{
		childrenHandleMouseUp(x, y, mask);
	}

	return TRUE;
}

BOOL	LLButton::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mHandleRightMouse && !childrenHandleRightMouseDown(x, y, mask))
	{
		// Route future Mouse messages here preemptively.  (Release on mouse up.)
		gFocusMgr.setMouseCapture( this );

		if (hasTabStop() && !getIsChrome())
		{
			setFocus(TRUE);
		}

//		if (pointInView(x, y))
//		{
//		}
		// send the mouse down signal
		LLUICtrl::handleRightMouseDown(x,y,mask);
		// *TODO: Return result of LLUICtrl call above?  Should defer to base class
		// but this might change the mouse handling of existing buttons in a bad way
		// if they are not mouse opaque.
	}

	return TRUE;
}

BOOL	LLButton::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	if (mHandleRightMouse)
	{
		// We only handle the click if the click both started and ended within us
		if( hasMouseCapture() )
		{
			// Always release the mouse
			gFocusMgr.setMouseCapture(nullptr );

	//		if (pointInView(x, y))
	//		{
	//			mRightMouseUpSignal(this, x,y,mask);
	//		}
		}
		else 
		{
			childrenHandleRightMouseUp(x, y, mask);
		}
	
		// send the mouse up signal
		LLUICtrl::handleRightMouseUp(x,y,mask);
		// *TODO: Return result of LLUICtrl call above?  Should defer to base class
		// but this might change the mouse handling of existing buttons in a bad way.
		// if they are not mouse opaque.
	}
	return TRUE;
}

void LLButton::onMouseLeave(S32 x, S32 y, MASK mask)
{
	LLUICtrl::onMouseLeave(x, y, mask);

	mNeedsHighlight = FALSE;
}

void LLButton::setHighlight(bool b)
{
	mNeedsHighlight = b;
}

BOOL LLButton::handleHover(S32 x, S32 y, MASK mask)
{
	if (isInEnabledChain() 
		&& (!gFocusMgr.getMouseCapture() || gFocusMgr.getMouseCapture() == this))
		mNeedsHighlight = TRUE;

	if (!childrenHandleHover(x, y, mask))
	{
		if (mMouseDownTimer.getStarted())
		{
			F32 elapsed = getHeldDownTime();
			if( mHeldDownDelay <= elapsed && mHeldDownFrameDelay <= (S32)LLFrameTimer::getFrameCount() - mMouseDownFrame)
			{
				LLSD param;
				param["count"] = mMouseHeldDownCount++;
				if (mHeldDownSignal) (*mHeldDownSignal)(this, param);
			}
		}

		// We only handle the click if the click both started and ended within us
		getWindow()->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName() << LL_ENDL;
	}
	return TRUE;
}

void LLButton::getOverlayImageSize(S32& overlay_width, S32& overlay_height)
{
	overlay_width = mImageOverlay->getWidth();
	overlay_height = mImageOverlay->getHeight();

	F32 scale_factor = llmin((F32)getRect().getWidth() / (F32)overlay_width, (F32)getRect().getHeight() / (F32)overlay_height, 1.f);
	overlay_width = ll_round((F32)overlay_width * scale_factor);
	overlay_height = ll_round((F32)overlay_height * scale_factor);
}


// virtual
void LLButton::draw()
{
	static LLUICachedControl<bool> sEnableButtonFlashing("EnableButtonFlashing", true);
	F32 alpha = mUseDrawContextAlpha ? getDrawContext().mAlpha : getCurrentTransparency();

	bool pressed_by_keyboard = FALSE;
	if (hasFocus())
	{
		pressed_by_keyboard = gKeyboard->getKeyDown(' ') || (mCommitOnReturn && gKeyboard->getKeyDown(KEY_RETURN));
	}

	bool mouse_pressed_and_over = false;
	if (hasMouseCapture())
	{
		S32 local_mouse_x ;
		S32 local_mouse_y;
		LLUI::getMousePositionLocal(this, &local_mouse_x, &local_mouse_y);
		mouse_pressed_and_over = pointInView(local_mouse_x, local_mouse_y);
	}

	bool enabled = isInEnabledChain();

	bool pressed = pressed_by_keyboard 
					|| mouse_pressed_and_over
					|| mForcePressedState;
	bool selected = getToggleState();
	
	bool use_glow_effect = FALSE;
	LLColor4 highlighting_color = LLUIColorTable::instance().getColor("ButtonHighlightColor", LLColor4::white); // <alchemy/>
	LLColor4 glow_color = LLUIColorTable::instance().getColor("ButtonGlowColor", LLColor4::white); // <alchemy/>
	LLRender::eBlendType glow_type = LLRender::BT_ADD_WITH_ALPHA;
	LLUIImage* imagep = nullptr;

    //  Cancel sticking of color, if the button is pressed,
	//  or when a flashing of the previously selected button is ended
	if (mFlashingTimer
		&& ((selected && !mFlashingTimer->isFlashingInProgress() && !mForceFlashing) || pressed))
	{
		mFlashing = false;
	}

	bool flash = mFlashing && sEnableButtonFlashing;

	if (pressed && mDisplayPressedState)
	{
		imagep = selected ? mImagePressedSelected : mImagePressed;
	}
	else if ( mNeedsHighlight )
	{
		if (selected)
		{
			if (mImageHoverSelected)
			{
				imagep = mImageHoverSelected;
			}
			else
			{
				imagep = mImageSelected;
				use_glow_effect = TRUE;
			}
		}
		else
		{
			if (mImageHoverUnselected)
			{
				imagep = mImageHoverUnselected;
			}
			else
			{
				imagep = mImageUnselected;
				use_glow_effect = TRUE;
			}
		}
	}
	else 
	{
		imagep = selected ? mImageSelected : mImageUnselected;
	}

	// Override if more data is available
	// HACK: Use gray checked state to mean either:
	//   enabled and tentative
	// or
	//   disabled but checked
	if (!mImageDisabledSelected.isNull() 
		&& 
			( (enabled && getTentative()) 
			|| (!enabled && selected ) ) )
	{
		imagep = mImageDisabledSelected;
	}
	else if (!mImageDisabled.isNull() 
		&& !enabled 
		&& !selected)
	{
		imagep = mImageDisabled;
	}

	if (mFlashing)
	{
		// if button should flash and we have icon for flashing, use it as image for button
		if(flash && mImageFlash)
		{
			// setting flash to false to avoid its further influence on glow
			flash = false;
			imagep = mImageFlash;
		}
		// else use usual flashing via flash_color
		else if (mFlashingTimer)
		{
			LLColor4 flash_color = mFlashBgColor.get();
			use_glow_effect = TRUE;
			glow_type = LLRender::BT_ALPHA; // blend the glow

			if (mFlashingTimer->isCurrentlyHighlighted() || !mFlashingTimer->isFlashingInProgress())
			{
				glow_color = flash_color;
			}
			else if (mNeedsHighlight)
			{
                glow_color = highlighting_color;
			}
		}
	}

	if (mNeedsHighlight && !imagep)
	{
		use_glow_effect = TRUE;
	}

	// Figure out appropriate color for the text
	LLColor4 label_color;

	// label changes when button state changes, not when pressed
	if ( enabled )
	{
		if ( getToggleState() )
		{
			label_color = mSelectedLabelColor.get();
		}
		else
		{
			label_color = mUnselectedLabelColor.get();
		}
	}
	else
	{
		if ( getToggleState() )
		{
			label_color = mDisabledSelectedLabelColor.get();
		}
		else
		{
			label_color = mDisabledLabelColor.get();
		}
	}

	// Unselected label assignments
	LLWString label = getCurrentLabel();

	// overlay with keyboard focus border
	if (hasFocus())
	{
		F32 lerp_amt = gFocusMgr.getFocusFlashAmt();
		drawBorder(imagep, gFocusMgr.getFocusColor() % alpha, ll_round(lerp(1.f, 3.f, lerp_amt)));
	}
	
	if (use_glow_effect)
	{
		mCurGlowStrength = lerp(mCurGlowStrength,
					mFlashing ? (mFlashingTimer->isCurrentlyHighlighted() || !mFlashingTimer->isFlashingInProgress() || mNeedsHighlight? 1.f : 0.f) : mHoverGlowStrength,
					LLSmoothInterpolation::getInterpolant(0.05f));
	}
	else
	{
		mCurGlowStrength = lerp(mCurGlowStrength, 0.f, LLSmoothInterpolation::getInterpolant(0.05f));
	}

	// Draw button image, if available.
	// Otherwise draw basic rectangular button.
	if (imagep != nullptr)
	{
		// apply automatic 50% alpha fade to disabled image
		LLColor4 disabled_color = mFadeWhenDisabled ? mDisabledImageColor.get() % 0.5f : mDisabledImageColor.get();
		if ( mScaleImage)
		{
			imagep->draw(getLocalRect(), (enabled ? mImageColor.get() : disabled_color) % alpha  );
			if (mCurGlowStrength > 0.01f)
			{
				gGL.setSceneBlendType(glow_type);
				imagep->drawSolid(0, 0, getRect().getWidth(), getRect().getHeight(), glow_color % (mCurGlowStrength * alpha));
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
		}
		else
		{
			imagep->draw(0, 0, (enabled ? mImageColor.get() : disabled_color) % alpha );
			if (mCurGlowStrength > 0.01f)
			{
				gGL.setSceneBlendType(glow_type);
				imagep->drawSolid(0, 0, glow_color % (mCurGlowStrength * alpha));
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
		}
	}
	else
	{
		// no image
		LL_DEBUGS() << "No image for button " << getName() << LL_ENDL;
		// draw it in pink so we can find it
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, LLColor4::pink1 % alpha, FALSE);
	}

	// let overlay image and text play well together
	S32 text_left = mLeftHPad;
	S32 text_right = getRect().getWidth() - mRightHPad;
	S32 text_width = getRect().getWidth() - mLeftHPad - mRightHPad;

	// draw overlay image
	if (mImageOverlayEnable && mImageOverlay.notNull())
	{
		// get max width and height (discard level 0)
		S32 overlay_width;
		S32 overlay_height;

		getOverlayImageSize(overlay_width, overlay_height);

		S32 center_x = getLocalRect().getCenterX();
		S32 center_y = getLocalRect().getCenterY();

		//FUGLY HACK FOR "DEPRESSED" BUTTONS
		if (pressed && mDisplayPressedState)
		{
			center_y--;
			center_x++;
		}

		center_y += (mImageOverlayBottomPad - mImageOverlayTopPad);
		// fade out overlay images on disabled buttons
		LLColor4 overlay_color = mImageOverlayColor.get();
		if (!enabled)
		{
			overlay_color = mImageOverlayDisabledColor.get();
		}
		else if (getToggleState())
		{
			overlay_color = mImageOverlaySelectedColor.get();
		}
		overlay_color.mV[VALPHA] *= alpha;

		switch(mImageOverlayAlignment)
		{
		case LLFontGL::LEFT:
			text_left += overlay_width + mImgOverlayLabelSpace;
			text_width -= overlay_width + mImgOverlayLabelSpace;
			mImageOverlay->draw(
				mLeftHPad,
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		case LLFontGL::HCENTER:
			mImageOverlay->draw(
				center_x - (overlay_width / 2), 
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		case LLFontGL::RIGHT:
			text_right -= overlay_width + mImgOverlayLabelSpace;
			text_width -= overlay_width + mImgOverlayLabelSpace;
			mImageOverlay->draw(
				getRect().getWidth() - mRightHPad - overlay_width,
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		default:
			// draw nothing
			break;
		}
	}

	// Draw label
	if( !label.empty() )
	{
		LLWStringUtil::trim(label);

		S32 x;
		switch( mHAlign )
		{
		case LLFontGL::RIGHT:
			x = text_right;
			break;
		case LLFontGL::HCENTER:
			x = text_left + (text_width / 2);
			break;
		case LLFontGL::LEFT:
		default:
			x = text_left;
			break;
		}

		S32 y_offset = 2 + (getRect().getHeight() - 20)/2;
	
		if (pressed && mDisplayPressedState)
		{
			y_offset--;
			x++;
		}

		// *NOTE: mantipov: before mUseEllipses is implemented in EXT-279 U32_MAX has been passed as
		// max_chars.
		// LLFontGL::render expects S32 max_chars variable but process in a separate way -1 value.
		// Due to U32_MAX is equal to S32 -1 value I have rest this value for non-ellipses mode.
		// Not sure if it is really needed. Probably S32_MAX should be always passed as max_chars.
		mLastDrawCharsCount = mGLFont->render(label, 0,
			(F32)x,
			(F32)(getRect().getHeight() / 2 + mBottomVPad),
			label_color % alpha,
			mHAlign, LLFontGL::VCENTER,
			LLFontGL::NORMAL,
			mDropShadowedText ? LLFontGL::DROP_SHADOW_SOFT : LLFontGL::NO_SHADOW,
			S32_MAX, text_width,
			nullptr, mUseEllipses);
	}

	LLUICtrl::draw();
}

void LLButton::drawBorder(LLUIImage* imagep, const LLColor4& color, S32 size)
{
	if (imagep == nullptr) return;
	if (mScaleImage)
	{
		imagep->drawBorder(getLocalRect(), color, size);
	}
	else
	{
		imagep->drawBorder(0, 0, color, size);
	}
}

BOOL LLButton::getToggleState() const
{
    return getValue().asBoolean();
}

void LLButton::setToggleState(BOOL b)
{
	if( b != getToggleState() )
	{
		setControlValue(b); // will fire LLControlVariable callbacks (if any)
		setValue(b);        // may or may not be redundant
		setFlashing(false);	// stop flash state whenever the selected/unselected state if reset
		// Unselected label assignments
		autoResize();
	}
}

void LLButton::setFlashing(bool b, bool force_flashing/* = false */)
{ 
	mForceFlashing = force_flashing;
	if (mFlashingTimer)
	{
		mFlashing = b; 
		(b ? mFlashingTimer->startFlashing() : mFlashingTimer->stopFlashing());
	}
	else if (b != mFlashing)
	{
		mFlashing = b; 
		mFrameTimer.reset();
	}
}

BOOL LLButton::toggleState()			
{
    bool flipped = ! getToggleState();
	setToggleState(flipped); 

	return flipped; 
}

void LLButton::setLabel( const LLStringExplicit& label )
{
	setLabelUnselected(label);
	setLabelSelected(label);
}

//virtual
BOOL LLButton::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	mUnselectedLabel.setArg(key, text);
	mSelectedLabel.setArg(key, text);
	return TRUE;
}

void LLButton::setLabelUnselected( const LLStringExplicit& label )
{
	mUnselectedLabel = label;
}

void LLButton::setLabelSelected( const LLStringExplicit& label )
{
	mSelectedLabel = label;
}

bool LLButton::labelIsTruncated() const
{
	return getCurrentLabel().getString().size() > mLastDrawCharsCount;
}

const LLUIString& LLButton::getCurrentLabel() const
{
	if( getToggleState() )
	{
		return mSelectedLabel;
	}
	else
	{
		return mUnselectedLabel;
	}
}

void LLButton::setImageUnselected(LLPointer<LLUIImage> image)
{
	mImageUnselected = image;
	if (mImageUnselected.isNull())
	{
		LL_WARNS() << "Setting default button image for: " << getName() << " to NULL" << LL_ENDL;
	}
}

void LLButton::autoResize()
{
	resize(getCurrentLabel());
}

void LLButton::resize(LLUIString label)
{
	// get label length 
	S32 label_width = mGLFont->getWidth(label.getString());
	// get current btn length 
	S32 btn_width =getRect().getWidth();
    // check if it need resize 
	if (mAutoResize)
	{ 
		S32 min_width = label_width + mLeftHPad + mRightHPad;
		if (mImageOverlay)
		{
			S32 overlay_width = mImageOverlay->getWidth();
			F32 scale_factor = (getRect().getHeight() - (mImageOverlayBottomPad + mImageOverlayTopPad)) / (F32)mImageOverlay->getHeight();
			overlay_width = ll_round((F32)overlay_width * scale_factor);

			switch(mImageOverlayAlignment)
			{
			case LLFontGL::LEFT:
			case LLFontGL::RIGHT:
				min_width += overlay_width + mImgOverlayLabelSpace;
				break;
			case LLFontGL::HCENTER:
				min_width = llmax(min_width, overlay_width + mLeftHPad + mRightHPad);
				break;
			default:
				// draw nothing
				break;
			}
		}
		if (btn_width < min_width)
		{
			reshape(min_width, getRect().getHeight());
		}
	} 
}
void LLButton::setImages( const std::string &image_name, const std::string &selected_name )
{
	setImageUnselected(LLUI::getUIImage(image_name));
	setImageSelected(LLUI::getUIImage(selected_name));
}

void LLButton::setImageSelected(LLPointer<LLUIImage> image)
{
	mImageSelected = image;
}

void LLButton::setImageColor(const LLColor4& c)		
{ 
	mImageColor = c; 
}

void LLButton::setColor(const LLColor4& color)
{
	setImageColor(color);
}

void LLButton::setImageDisabled(LLPointer<LLUIImage> image)
{
	mImageDisabled = image;
	mDisabledImageColor = mImageColor;
	mFadeWhenDisabled = TRUE;
}

void LLButton::setImageDisabledSelected(LLPointer<LLUIImage> image)
{
	mImageDisabledSelected = image;
	mDisabledImageColor = mImageColor;
	mFadeWhenDisabled = TRUE;
}

void LLButton::setImagePressed(LLPointer<LLUIImage> image)
{
	mImagePressed = image;
}

void LLButton::setImageHoverSelected(LLPointer<LLUIImage> image)
{
	mImageHoverSelected = image;
}

void LLButton::setImageHoverUnselected(LLPointer<LLUIImage> image)
{
	mImageHoverUnselected = image;
}

void LLButton::setImageFlash(LLPointer<LLUIImage> image)
{
	mImageFlash = image;
}

void LLButton::setImageOverlay(const std::string& image_name, LLFontGL::HAlign alignment, const LLColor4& color)
{
	if (image_name.empty())
	{
		mImageOverlay = nullptr;
	}
	else
	{
		mImageOverlay = LLUI::getUIImage(image_name);
		mImageOverlayAlignment = alignment;
		mImageOverlayColor = color;
	}
}

void LLButton::setImageOverlay(const LLUUID& image_id, LLFontGL::HAlign alignment, const LLColor4& color)
{
	if (image_id.isNull())
	{
		mImageOverlay = nullptr;
	}
	else
	{
		mImageOverlay = LLUI::getUIImageByID(image_id);
		mImageOverlayAlignment = alignment;
		mImageOverlayColor = color;
	}
}

void LLButton::onMouseCaptureLost()
{
	resetMouseDownTimer();
}

//-------------------------------------------------------------------------
// Utilities
//-------------------------------------------------------------------------
S32 round_up(S32 grid, S32 value)
{
	S32 mod = value % grid;

	if (mod > 0)
	{
		// not even multiple
		return value + (grid - mod);
	}
	else
	{
		return value;
	}
}

void LLButton::addImageAttributeToXML(LLXMLNodePtr node, 
									  const std::string& image_name,
									  const LLUUID&	image_id,
									  const std::string& xml_tag_name) const
{
	if( !image_name.empty() )
	{
		node->createChild(xml_tag_name.c_str(), TRUE)->setStringValue(image_name);
	}
	else if( image_id != LLUUID::null )
	{
		node->createChild((xml_tag_name + "_id").c_str(), TRUE)->setUUIDValue(image_id);
	}
}


// static
void LLButton::toggleFloaterAndSetToggleState(LLUICtrl* ctrl, const LLSD& sdname)
{
	bool floater_vis = LLFloaterReg::toggleInstance(sdname.asString());
	LLButton* button = dynamic_cast<LLButton*>(ctrl);
	if (button)
		button->setToggleState(floater_vis);
}

// static
// Gets called once
void LLButton::setFloaterToggle(LLUICtrl* ctrl, const LLSD& sdname)
{
	LLButton* button = dynamic_cast<LLButton*>(ctrl);
	if (!button)
		return;
	// Get the visibility control name for the floater
	std::string vis_control_name = LLFloaterReg::declareVisibilityControl(sdname.asString());
	// Set the button control value (toggle state) to the floater visibility control (Sets the value as well)
	button->setControlVariable(LLFloater::getControlGroup()->getControl(vis_control_name));
	// Set the clicked callback to toggle the floater
	button->setClickedCallback(boost::bind(&LLFloaterReg::toggleInstance, sdname, LLSD()));
}

// static
void LLButton::setDockableFloaterToggle(LLUICtrl* ctrl, const LLSD& sdname)
{
	LLButton* button = dynamic_cast<LLButton*>(ctrl);
	if (!button)
		return;
	// Get the visibility control name for the floater
	std::string vis_control_name = LLFloaterReg::declareVisibilityControl(sdname.asString());
	// Set the button control value (toggle state) to the floater visibility control (Sets the value as well)
	button->setControlVariable(LLFloater::getControlGroup()->getControl(vis_control_name));
	// Set the clicked callback to toggle the floater
	button->setClickedCallback(boost::bind(&LLDockableFloater::toggleInstance, sdname));
}

// static
void LLButton::showHelp(LLUICtrl* ctrl, const LLSD& sdname)
{
	// search back through the button's parents for a panel
	// with a help_topic string defined
	std::string help_topic;
	if (LLUI::sHelpImpl &&
	    ctrl->findHelpTopic(help_topic))
	{
		LLUI::sHelpImpl->showTopic(help_topic);
		return; // success
	}

	// display an error if we can't find a help_topic string.
	// fix this by adding a help_topic attribute to the xui file
	LLNotificationsUtil::add("UnableToFindHelpTopic");
}

void LLButton::resetMouseDownTimer()
{
	mMouseDownTimer.stop();
	mMouseDownTimer.reset();
}

BOOL LLButton::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// just treat a double click as a second click
	return handleMouseDown(x, y, mask);
}
