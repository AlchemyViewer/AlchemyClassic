/** 
 * @file llmultisliderctrl.h
 * @brief LLMultiSliderCtrl base class
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#ifndef LL_MULTI_SLIDERCTRL_H
#define LL_MULTI_SLIDERCTRL_H

#include "llf32uictrl.h"
#include "v4color.h"
#include "llmultislider.h"
#include "lltextbox.h"
#include "llrect.h"


//
// Classes
//
class LLFontGL;
class LLLineEditor;
class LLSlider;


class LLMultiSliderCtrl : public LLF32UICtrl
{
public:
	struct Params : public LLInitParam::Block<Params, LLF32UICtrl::Params>
	{
		Optional<S32>			label_width,
								text_width;
		Optional<bool>			show_text,
								can_edit_text;
		Optional<S32>			decimal_digits;
		Optional<S32>			max_sliders;	
		Optional<bool>			allow_overlap,
								draw_track,
								use_triangle;

		Optional<LLUIColor>		text_color,
								text_disabled_color;

		Optional<CommitCallbackParam>	mouse_down_callback,
										mouse_up_callback;

		Multiple<LLMultiSlider::SliderParams>	sliders;

		Params();
	};

protected:
	LLMultiSliderCtrl(const Params&);
	friend class LLUICtrlFactory;
public:
	virtual ~LLMultiSliderCtrl();

	F32				getSliderValue(const std::string& name) const;
	void			setSliderValue(const std::string& name, F32 v, BOOL from_event = FALSE);

	void	setValue(const LLSD& value ) override;
	LLSD	getValue() const override { return mMultiSlider->getValue(); }
	BOOL	setLabelArg( const std::string& key, const LLStringExplicit& text ) override;

	const std::string& getCurSlider() const					{ return mMultiSlider->getCurSlider(); }
	F32				getCurSliderValue() const				{ return mCurValue; }
	void			setCurSlider(const std::string& name);		
	void			setCurSliderValue(F32 val, BOOL from_event = false) { setSliderValue(mMultiSlider->getCurSlider(), val, from_event); }

	void	setMinValue(const LLSD& min_value) override { setMinValue((F32)min_value.asReal()); }
	void	setMaxValue(const LLSD& max_value) override { setMaxValue((F32)max_value.asReal());  }

	BOOL			isMouseHeldDown();

	void    setEnabled( BOOL b ) override;
	void	clear() override;
	virtual void	setPrecision(S32 precision);
	void			setMinValue(F32 min_value) override {mMultiSlider->setMinValue(min_value);}
	void			setMaxValue(F32 max_value) override {mMultiSlider->setMaxValue(max_value);}
	void			setIncrement(F32 increment) override {mMultiSlider->setIncrement(increment);}

	/// for adding and deleting sliders
	const std::string&	addSlider();
	const std::string&	addSlider(F32 val);
	void			deleteSlider(const std::string& name);
	void			deleteCurSlider()			{ deleteSlider(mMultiSlider->getCurSlider()); }

	F32				getMinValue() const override { return mMultiSlider->getMinValue(); }
	F32				getMaxValue() const override { return mMultiSlider->getMaxValue(); }

	void			setLabel(const std::string& label)				{ if (mLabelBox) mLabelBox->setText(label); }
	void			setLabelColor(const LLColor4& c)			{ mTextEnabledColor = c; }
	void			setDisabledLabelColor(const LLColor4& c)	{ mTextDisabledColor = c; }

	boost::signals2::connection setSliderMouseDownCallback( const commit_signal_t::slot_type& cb );
	boost::signals2::connection setSliderMouseUpCallback( const commit_signal_t::slot_type& cb );

	void	onTabInto() override;

	void	setTentative(BOOL b) override;			// marks value as tentative
	void	onCommit() override;						// mark not tentative, then commit

	void		setControlName(const std::string& control_name, LLView* context) override;
	
	static void		onSliderCommit(LLUICtrl* caller, const LLSD& userdata);
	
	static void		onEditorCommit(LLUICtrl* ctrl, const LLSD& userdata);
	static void		onEditorGainFocus(LLFocusableElement* caller, void *userdata);
	static void		onEditorChangeFocus(LLUICtrl* caller, S32 direction, void *userdata);

private:
	void			updateText();
	void			reportInvalidData();

private:
	const LLFontGL*	mFont;
	BOOL			mShowText;
	BOOL			mCanEditText;

	S32				mPrecision;
	LLTextBox*		mLabelBox;
	S32				mLabelWidth;

	F32				mCurValue;
	LLMultiSlider*	mMultiSlider;
	LLLineEditor*	mEditor;
	LLTextBox*		mTextBox;

	LLUIColor	mTextEnabledColor;
	LLUIColor	mTextDisabledColor;
};

#endif  // LL_MULTI_SLIDERCTRL_H
