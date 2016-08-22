/** 
 * @file llfloaterinventory.cpp
 * @brief Implementation of the inventory view and associated stuff.
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

#include "llfloaterinventory.h"

#include "llagentcamera.h"
//#include "llfirstuse.h"
#include "llfloaterreg.h"
#include "llinventorymodel.h"
#include "llpanelmaininventory.h"
#include "llresmgr.h"
#include "llviewerfoldertype.h"
#include "lltransientfloatermgr.h"

///----------------------------------------------------------------------------
/// LLFloaterInventory
///----------------------------------------------------------------------------

LLFloaterInventory::LLFloaterInventory(const LLSD& key)
	: LLFloater(key)
	, mPanelMainInventory(nullptr)
{
	LLTransientFloaterMgr::getInstance()->addControlView(this);
}

LLFloaterInventory::~LLFloaterInventory()
{
	LLTransientFloaterMgr::getInstance()->removeControlView(this);
}

BOOL LLFloaterInventory::postBuild()
{
	mPanelMainInventory = findChild<LLPanelMainInventory>("Inventory Panel");
	return TRUE;
}

LLInventoryPanel* LLFloaterInventory::getPanel()
{
	if (mPanelMainInventory)
		return mPanelMainInventory->getPanel();
	return NULL;
}

// static
LLFloaterInventory* LLFloaterInventory::showAgentInventory()
{
	// Hack to generate semi-unique key for each inventory floater.
	static S32 instance_num = 0;
	instance_num = (instance_num + 1) % S32_MAX;

	LLFloaterInventory* iv = NULL;
	if (!gAgentCamera.cameraMouselook())
	{
		iv = LLFloaterReg::showTypedInstance<LLFloaterInventory>("inventory", LLSD(instance_num));
	}
	return iv;
}

// static
void LLFloaterInventory::cleanup()
{
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("inventory");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin(); iter != inst_list.end();)
	{
		LLFloaterInventory* iv = dynamic_cast<LLFloaterInventory*>(*iter++);
		if (iv)
		{
			iv->destroy();
		}
	}
}

void LLFloaterInventory::onOpen(const LLSD& key)
{
	//LLFirstUse::useInventory();
}

void LLFloaterInventory::onClose(bool app_quitting)
{
	LLFloater::onClose(app_quitting);
	if (mKey.asInteger() > 1)
	{
		destroy();
	}
}
