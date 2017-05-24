/** 
 * @file llradiogroup.h
 * @brief LLRadioGroup base class
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

#ifndef LL_LLRADIOGROUP_H
#define LL_LLRADIOGROUP_H

#include "lluictrl.h"
#include "llcheckboxctrl.h"
#include "llctrlselectioninterface.h"

/*
 * An invisible view containing multiple mutually exclusive toggling 
 * buttons (usually radio buttons).  Automatically handles the mutex
 * condition by highlighting only one button at a time.
 */
class LLRadioGroup
:	public LLUICtrl, public LLCtrlSelectionInterface
{
public:

	struct ItemParams : public LLInitParam::Block<ItemParams, LLCheckBoxCtrl::Params>
	{
		Optional<LLSD>	value;
		ItemParams();
	};

	struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		Optional<bool>						allow_deselect;
		Multiple<ItemParams, AtLeast<1> >	items;
		Params();
	};

protected:
	LLRadioGroup(const Params&);
	friend class LLUICtrlFactory;

public:

	/*virtual*/ void initFromParams(const Params&);

	virtual ~LLRadioGroup();

	BOOL postBuild() override;

	BOOL handleKeyHere(KEY key, MASK mask) override;

	void setIndexEnabled(S32 index, BOOL enabled);
	// return the index value of the selected item
	S32 getSelectedIndex() const { return mSelectedIndex; }
	// set the index value programatically
	BOOL setSelectedIndex(S32 index, BOOL from_event = FALSE);
	// foxus child by index if it can get focus
	void focusSelectedRadioBtn();

	// Accept and retrieve strings of the radio group control names
	void	setValue(const LLSD& value ) override;
	LLSD	getValue() const override;

	// Update the control as needed.  Userdata must be a pointer to the button.
	void onClickButton(LLUICtrl* clicked_radio);
	
	//========================================================================
	LLCtrlSelectionInterface* getSelectionInterface() override { return (LLCtrlSelectionInterface*)this; };

	// LLCtrlSelectionInterface functions
	/*virtual*/ S32		getItemCount() const override { return mRadioButtons.size(); }
	/*virtual*/ BOOL	getCanSelect() const override { return TRUE; }
	/*virtual*/ BOOL	selectFirstItem() override { return setSelectedIndex(0); }
	/*virtual*/ BOOL	selectNthItem( S32 index ) override { return setSelectedIndex(index); }
	/*virtual*/ BOOL	selectItemRange( S32 first, S32 last ) override { return setSelectedIndex(first); }
	/*virtual*/ S32		getFirstSelectedIndex() const override { return getSelectedIndex(); }
	/*virtual*/ BOOL	setCurrentByID( const LLUUID& id ) override;
	/*virtual*/ LLUUID	getCurrentID() const override;				// LLUUID::null if no items in menu
	/*virtual*/ BOOL	setSelectedByValue(const LLSD& value, BOOL selected) override;
	/*virtual*/ LLSD	getSelectedValue() override;
	/*virtual*/ BOOL	isSelected(const LLSD& value) const override;
	/*virtual*/ BOOL	operateOnSelection(EOperation op) override;
	/*virtual*/ BOOL	operateOnAll(EOperation op) override;

private:
	const LLFontGL*		mFont;
	S32					mSelectedIndex;

	typedef std::vector<class LLRadioCtrl*> button_list_t;
	button_list_t		mRadioButtons;

	bool				mAllowDeselect;	// user can click on an already selected option to deselect it
};

#endif
