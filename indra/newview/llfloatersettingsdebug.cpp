// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llfloatersettingsdebug.cpp
 * @brief floater for debugging internal viewer settings
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

#include "llviewerprecompiledheaders.h"
#include "llfloatersettingsdebug.h"
#include "llfloater.h"
#include "llcombobox.h"
#include "llspinctrl.h"
#include "llcolorswatch.h"
#include "llviewercontrol.h"
#include "lltexteditor.h"


LLFloaterSettingsDebug::LLFloaterSettingsDebug(const LLSD& key) 
:	LLFloater(key.asString().empty() ? LLSD("all") : key) // <alchemy/> - So apparantly just opening this doesn't pass in a key *yes bb yes*
,	mComboSettings(nullptr)
,	mValSpinner1(nullptr)
,	mValSpinner2(nullptr)
,	mValSpinner3(nullptr)
,	mValSpinner4(nullptr)
,	mValColor(nullptr)
,	mValBool(nullptr)
,	mValText(nullptr)
,	mComment(nullptr)
{
	mCommitCallbackRegistrar.add("SettingSelect",	boost::bind(&LLFloaterSettingsDebug::onSettingSelect, this,_1));
	mCommitCallbackRegistrar.add("CommitSettings",	boost::bind(&LLFloaterSettingsDebug::onCommitSettings, this));
	mCommitCallbackRegistrar.add("ClickDefault",	boost::bind(&LLFloaterSettingsDebug::onClickDefault, this));
}

LLFloaterSettingsDebug::~LLFloaterSettingsDebug()
{}

BOOL LLFloaterSettingsDebug::postBuild()
{
	mComboSettings = getChild<LLComboBox>("settings_combo");

	struct f : public LLControlGroup::ApplyFunctor
	{
		LLComboBox* combo;
		f(LLComboBox* c) : combo(c) {}

		void apply(const std::string& name, LLControlVariable* control) override
		{
			if (!control->isHiddenFromSettingsEditor())
			{
				combo->add(name, (void*)control);
			}
		}
	} func(mComboSettings);

	std::string key = getKey().asString();
	if (key == "all" || key == "base")
	{
		gSavedSettings.applyToAll(&func);
	}
	if (key == "all" || key == "account")
	{
		gSavedPerAccountSettings.applyToAll(&func);
	}

	mComboSettings->sortByName();
	mComboSettings->updateSelection();
	mComment = getChild<LLTextEditor>("comment_text");
	mValSpinner1 = getChild<LLSpinCtrl>("val_spinner_1");
	mValSpinner2 = getChild<LLSpinCtrl>("val_spinner_2");
	mValSpinner3 = getChild<LLSpinCtrl>("val_spinner_3");
	mValSpinner4 = getChild<LLSpinCtrl>("val_spinner_4");
	mValColor = getChild<LLColorSwatchCtrl>("val_color_swatch");
	mValBool = getChild<LLUICtrl>("boolean_combo");
	mValText = getChild<LLUICtrl>("val_text");
	return TRUE;
}

void LLFloaterSettingsDebug::draw()
{
	LLControlVariable* controlp = static_cast<LLControlVariable*>(mComboSettings->getCurrentUserdata());
	updateControl(controlp);

	LLFloater::draw();
}

//static 
void LLFloaterSettingsDebug::onSettingSelect(LLUICtrl* ctrl)
{
	LLComboBox* combo_box = (LLComboBox*)ctrl;
	LLControlVariable* controlp = (LLControlVariable*)combo_box->getCurrentUserdata();

	updateControl(controlp);
}

void LLFloaterSettingsDebug::onCommitSettings()
{
	LLControlVariable* controlp = static_cast<LLControlVariable*>(mComboSettings->getCurrentUserdata());

	if (!controlp)
	{
		return;
	}

	LLVector3 vector;
	LLVector3d vectord;
	LLRect rect;
	LLColor4 col4;
	LLColor3 col3;
	LLColor4U col4U;
	LLColor4 color_with_alpha;

	switch(controlp->type())
	{		
	  case TYPE_U32:
		controlp->set(mValSpinner1->getValue());
		break;
	  case TYPE_S32:
		controlp->set(mValSpinner1->getValue());
		break;
	  case TYPE_F32:
		controlp->set(LLSD(mValSpinner1->getValue().asReal()));
		break;
	  case TYPE_BOOLEAN:
		controlp->set(mValBool->getValue());
		break;
	  case TYPE_STRING:
		controlp->set(LLSD(mValText->getValue().asString()));
		break;
	  case TYPE_VEC3:
		vector.mV[VX] = (F32)mValSpinner1->getValue().asReal();
		vector.mV[VY] = (F32)mValSpinner2->getValue().asReal();
		vector.mV[VZ] = (F32)mValSpinner3->getValue().asReal();
		controlp->set(vector.getValue());
		break;
	  case TYPE_VEC3D:
		vectord.mdV[VX] = mValSpinner1->getValue().asReal();
		vectord.mdV[VY] = mValSpinner2->getValue().asReal();
		vectord.mdV[VZ] = mValSpinner3->getValue().asReal();
		controlp->set(vectord.getValue());
		break;
	  case TYPE_RECT:
		rect.mLeft = mValSpinner1->getValue().asInteger();
		rect.mRight = mValSpinner2->getValue().asInteger();
		rect.mBottom = mValSpinner3->getValue().asInteger();
		rect.mTop = mValSpinner4->getValue().asInteger();
		controlp->set(rect.getValue());
		break;
	  case TYPE_COL4:
		col3.setValue(mValColor->getValue());
		col4 = LLColor4(col3, (F32)mValSpinner4->getValue().asReal());
		controlp->set(col4.getValue());
		break;
	  case TYPE_COL3:
		controlp->set(mValColor->getValue());
		break;
	  default:
		break;
	}
}

// static
void LLFloaterSettingsDebug::onClickDefault()
{
	LLControlVariable* controlp = static_cast<LLControlVariable*>(mComboSettings->getCurrentUserdata());

	if (controlp)
	{
		controlp->resetToDefault(true);
		updateControl(controlp);
	}
}

// we've switched controls, or doing per-frame update, so update spinners, etc.
void LLFloaterSettingsDebug::updateControl(LLControlVariable* controlp)
{
	if (!mValSpinner1 || !mValSpinner2 || !mValSpinner3 || !mValSpinner4 || !mValColor)
	{
		LL_WARNS() << "Could not find all desired controls by name"
			<< LL_ENDL;
		return;
	}

	mValSpinner1->setVisible(FALSE);
	mValSpinner2->setVisible(FALSE);
	mValSpinner3->setVisible(FALSE);
	mValSpinner4->setVisible(FALSE);
	mValColor->setVisible(FALSE);
	mValText->setVisible( FALSE);
	if (!mComment->hasFocus()) // <alchemy/>
	{
		mComment->setText(LLStringUtil::null);
	}

	if (controlp)
	{
		eControlType type = controlp->type();

		//hide combo box only for non booleans, otherwise this will result in the combo box closing every frame
		mValBool->setVisible( type == TYPE_BOOLEAN);
		

		if (!mComment->hasFocus()) // <alchemy/>
		{
			mComment->setText(controlp->getComment());
		}
		mValSpinner1->setMaxValue(F32_MAX);
		mValSpinner2->setMaxValue(F32_MAX);
		mValSpinner3->setMaxValue(F32_MAX);
		mValSpinner4->setMaxValue(F32_MAX);
		mValSpinner1->setMinValue(-F32_MAX);
		mValSpinner2->setMinValue(-F32_MAX);
		mValSpinner3->setMinValue(-F32_MAX);
		mValSpinner4->setMinValue(-F32_MAX);
		if (!mValSpinner1->hasFocus())
		{
			mValSpinner1->setIncrement(0.1f);
		}
		if (!mValSpinner2->hasFocus())
		{
			mValSpinner2->setIncrement(0.1f);
		}
		if (!mValSpinner3->hasFocus())
		{
			mValSpinner3->setIncrement(0.1f);
		}
		if (!mValSpinner4->hasFocus())
		{
			mValSpinner4->setIncrement(0.1f);
		}

		LLSD sd = controlp->get();
		switch(type)
		{
		  case TYPE_U32:
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("value")); // Debug, don't translate
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setValue(sd);
				mValSpinner1->setMinValue((F32)U32_MIN);
				mValSpinner1->setMaxValue((F32)U32_MAX);
				mValSpinner1->setIncrement(1.f);
				mValSpinner1->setPrecision(0);
			}
			break;
		  case TYPE_S32:
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("value")); // Debug, don't translate
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setValue(sd);
				mValSpinner1->setMinValue((F32)S32_MIN);
				mValSpinner1->setMaxValue((F32)S32_MAX);
				mValSpinner1->setIncrement(1.f);
				mValSpinner1->setPrecision(0);
			}
			break;
		  case TYPE_F32:
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("value")); // Debug, don't translate
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setPrecision(3);
				mValSpinner1->setValue(sd);
			}
			break;
		  case TYPE_BOOLEAN:
			if (!mValBool->hasFocus())
			{
				mValBool->setValue(LLSD(sd.asBoolean() ? "true" : LLStringUtil::null));
			}
			break;
		  case TYPE_STRING:
			mValText->setVisible(TRUE);
			if (!mValText->hasFocus())
			{
				mValText->setValue(sd);
			}
			break;
		  case TYPE_VEC3:
		  {
			LLVector3 v;
			v.setValue(sd);
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("X"));
			mValSpinner2->setVisible(TRUE);
			mValSpinner2->setLabel(LLStringExplicit("Y"));
			mValSpinner3->setVisible(TRUE);
			mValSpinner3->setLabel(LLStringExplicit("Z"));
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setPrecision(3);
				mValSpinner1->setValue(v[VX]);
			}
			if (!mValSpinner2->hasFocus())
			{
				mValSpinner2->setPrecision(3);
				mValSpinner2->setValue(v[VY]);
			}
			if (!mValSpinner3->hasFocus())
			{
				mValSpinner3->setPrecision(3);
				mValSpinner3->setValue(v[VZ]);
			}
			break;
		  }
		  case TYPE_VEC3D:
		  {
			LLVector3d v;
			v.setValue(sd);
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("X"));
			mValSpinner2->setVisible(TRUE);
			mValSpinner2->setLabel(LLStringExplicit("Y"));
			mValSpinner3->setVisible(TRUE);
			mValSpinner3->setLabel(LLStringExplicit("Z"));
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setPrecision(3);
				mValSpinner1->setValue(v[VX]);
			}
			if (!mValSpinner2->hasFocus())
			{
				mValSpinner2->setPrecision(3);
				mValSpinner2->setValue(v[VY]);
			}
			if (!mValSpinner3->hasFocus())
			{
				mValSpinner3->setPrecision(3);
				mValSpinner3->setValue(v[VZ]);
			}
			break;
		  }
		  case TYPE_RECT:
		  {
			LLRect r;
			r.setValue(sd);
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("Left"));
			mValSpinner2->setVisible(TRUE);
			mValSpinner2->setLabel(LLStringExplicit("Right"));
			mValSpinner3->setVisible(TRUE);
			mValSpinner3->setLabel(LLStringExplicit("Bottom"));
			mValSpinner4->setVisible(TRUE);
			mValSpinner4->setLabel(LLStringExplicit("Top"));
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setPrecision(0);
				mValSpinner1->setValue(r.mLeft);
			}
			if (!mValSpinner2->hasFocus())
			{
				mValSpinner2->setPrecision(0);
				mValSpinner2->setValue(r.mRight);
			}
			if (!mValSpinner3->hasFocus())
			{
				mValSpinner3->setPrecision(0);
				mValSpinner3->setValue(r.mBottom);
			}
			if (!mValSpinner4->hasFocus())
			{
				mValSpinner4->setPrecision(0);
				mValSpinner4->setValue(r.mTop);
			}

			mValSpinner1->setMinValue((F32)S32_MIN);
			mValSpinner1->setMaxValue((F32)S32_MAX);
			mValSpinner1->setIncrement(1.f);

			mValSpinner2->setMinValue((F32)S32_MIN);
			mValSpinner2->setMaxValue((F32)S32_MAX);
			mValSpinner2->setIncrement(1.f);

			mValSpinner3->setMinValue((F32)S32_MIN);
			mValSpinner3->setMaxValue((F32)S32_MAX);
			mValSpinner3->setIncrement(1.f);

			mValSpinner4->setMinValue((F32)S32_MIN);
			mValSpinner4->setMaxValue((F32)S32_MAX);
			mValSpinner4->setIncrement(1.f);
			break;
		  }
		  case TYPE_COL4:
		  {
			LLColor4 clr;
			clr.setValue(sd);
			mValColor->setVisible(TRUE);
			// only set if changed so color picker doesn't update
			if(clr != LLColor4(mValColor->getValue()))
			{
				mValColor->set(LLColor4(sd), TRUE, FALSE);
			}
			mValSpinner4->setVisible(TRUE);
			mValSpinner4->setLabel(LLStringExplicit("Alpha"));
			if (!mValSpinner4->hasFocus())
			{
				mValSpinner4->setPrecision(3);
				mValSpinner4->setMinValue(0.0);
				mValSpinner4->setMaxValue(1.f);
				mValSpinner4->setValue(clr.mV[VALPHA]);
			}
			break;
		  }
		  case TYPE_COL3:
		  {
			LLColor3 clr;
			clr.setValue(sd);
			mValColor->setVisible(TRUE);
			mValColor->setValue(sd);
			break;
		  }
		  default:
			mComment->setText(LLStringExplicit("unknown"));
			break;
		}
	}

}
