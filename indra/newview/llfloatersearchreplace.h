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

#ifndef LL_FLOATERSEARCHREPLACE_H
#define LL_FLOATERSEARCHREPLACE_H

#include "llfloater.h"
#include "llfloaterreg.h"

class LLTextEditor;

class LLFloaterSearchReplace : public LLFloater
{
public:
	LLFloaterSearchReplace(const LLSD& sdKey);
	~LLFloaterSearchReplace();

	/*virtual*/ bool hasAccelerators() const override;
	/*virtual*/ BOOL handleKeyHere(KEY key, MASK mask) override;
	/*virtual*/	BOOL postBuild() override;

public:
	LLTextEditor* getEditor() { return mEditor; }

	static void show(LLTextEditor* pEditor);

protected:
	void onBtnSearch();
	void onBtnReplace();
	void onBtnReplaceAll();

private:
	LLTextEditor* mEditor;
};

#endif // LL_FLOATERSEARCHREPLACE_H

