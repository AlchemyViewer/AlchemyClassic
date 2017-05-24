// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 *
 * Copyright (c) 2010, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#include "llviewerprecompiledheaders.h"

#include "llfloatersearchreplace.h"

#include "llcheckboxctrl.h"
#include "llmultifloater.h"
#include "lltexteditor.h"

LLFloaterSearchReplace::LLFloaterSearchReplace(const LLSD& sdKey)
	: LLFloater(sdKey), mEditor(nullptr)
{
}

LLFloaterSearchReplace::~LLFloaterSearchReplace()
{
}

//static 
void LLFloaterSearchReplace::show(LLTextEditor* pEditor)
{
	LLFloaterSearchReplace* pSelf = LLFloaterReg::getTypedInstance<LLFloaterSearchReplace>("search_replace");
	if (!pSelf)
		return;

	pSelf->mEditor = pEditor;
	if (pEditor)
	{
		LLFloater *pDependeeNew = nullptr, *pDependeeOld = pSelf->getDependee();
		LLView* pView = pEditor->getParent();
		while (pView)
		{
			pDependeeNew = dynamic_cast<LLFloater*>(pView);
			if (pDependeeNew)
			{
				if (pDependeeNew != pDependeeOld)
				{
					if (pDependeeOld)
						pDependeeOld->removeDependentFloater(pSelf);

					if (!pDependeeNew->getHost())
						pDependeeNew->addDependentFloater(pSelf);
					else
						pDependeeNew->getHost()->addDependentFloater(pSelf);
				}
				break;
			}
			pView = pView->getParent();
		}

		pSelf->getChildView("replace_text")->setEnabled(!pEditor->getReadOnly());
		pSelf->getChildView("replace_btn")->setEnabled(!pEditor->getReadOnly());
		pSelf->getChildView("replace_all_btn")->setEnabled(!pEditor->getReadOnly());

		pSelf->openFloater();
	}
}

BOOL LLFloaterSearchReplace::postBuild()
{
	childSetAction("search_btn", boost::bind(&LLFloaterSearchReplace::onBtnSearch, this));
	childSetAction("replace_btn", boost::bind(&LLFloaterSearchReplace::onBtnReplace, this));
	childSetAction("replace_all_btn", boost::bind(&LLFloaterSearchReplace::onBtnReplaceAll, this));

	setDefaultBtn("search_btn");

	return TRUE;
}

void LLFloaterSearchReplace::onBtnSearch()
{
	/*const*/ LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
	/*const*/ LLCheckBoxCtrl* prevChk = getChild<LLCheckBoxCtrl>("find_previous");
	mEditor->selectNext(getChild<LLUICtrl>("search_text")->getValue().asString(), caseChk->get(), TRUE, prevChk->get());
}

void LLFloaterSearchReplace::onBtnReplace()
{
	/*const*/ LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
	/*const*/ LLCheckBoxCtrl* prevChk = getChild<LLCheckBoxCtrl>("find_previous");
	mEditor->replaceText(
		getChild<LLUICtrl>("search_text")->getValue().asString(), getChild<LLUICtrl>("replace_text")->getValue().asString(), caseChk->get(), TRUE, prevChk->get());
}

void LLFloaterSearchReplace::onBtnReplaceAll()
{
	/*const*/ LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
	mEditor->replaceTextAll(getChild<LLUICtrl>("search_text")->getValue().asString(), getChild<LLUICtrl>("replace_text")->getValue().asString(), caseChk->get());
}

bool LLFloaterSearchReplace::hasAccelerators() const
{
	const LLView* pView = (LLView*)mEditor;
	while (pView)
	{
		if (pView->hasAccelerators())
			return true;
		pView = pView->getParent();
	}
	return false;
}

BOOL LLFloaterSearchReplace::handleKeyHere(KEY key, MASK mask)
{
	// Pass this on to the editor we're operating on (or any view up along its hierarchy) if we don't handle the key ourselves 
	// (allows Ctrl-F to work when the floater itself has focus - see changeset 0c8947e5f433)
	if (!LLFloater::handleKeyHere(key, mask))
	{
		LLView* pView = (LLView*)mEditor;
		while (pView)
		{
			if ( (pView->hasAccelerators()) && (pView->handleKeyHere(key, mask)) )
				return TRUE;
			pView = pView->getParent();
		}
	}
	return FALSE;
}
