// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
* @file llfloaterpathfindingcharacters.cpp
* @brief "Pathfinding characters" floater, allowing for identification of pathfinding characters and their cpu usage.
* @author Stinson@lindenlab.com
*
* $LicenseInfo:firstyear=2012&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2012, Linden Research, Inc.
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

#include "llfloaterpathfindingcharacters.h"

#include "llcheckboxctrl.h"
#include "llfloaterreg.h"
#include "llfloaterpathfindingobjects.h"
#include "llhandle.h"
#include "llpathfindingcharacter.h"
#include "llpathfindingcharacterlist.h"
#include "llpathfindingmanager.h"
#include "llpathfindingobject.h"
#include "llpathfindingobjectlist.h"
#include "llpathinglib.h"
#include "llquaternion.h"
#include "llsd.h"
#include "lluicolortable.h"
#include "lluuid.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "pipeline.h"
#include "v3math.h"
#include "v4color.h"

LLHandle<LLFloaterPathfindingCharacters> LLFloaterPathfindingCharacters::sInstanceHandle;

//---------------------------------------------------------------------------
// LLFloaterPathfindingCharacters
//---------------------------------------------------------------------------

void LLFloaterPathfindingCharacters::onClose(bool pIsAppQuitting)
{
	LLFloaterPathfindingObjects::onClose( pIsAppQuitting );
}

void LLFloaterPathfindingCharacters::openCharactersWithSelectedObjects()
{
	LLFloaterPathfindingCharacters *charactersFloater = LLFloaterReg::getTypedInstance<LLFloaterPathfindingCharacters>("pathfinding_characters");
	charactersFloater->showFloaterWithSelectionObjects();
}

LLHandle<LLFloaterPathfindingCharacters> LLFloaterPathfindingCharacters::getInstanceHandle()
{
	if ( sInstanceHandle.isDead() )
	{
		LLFloaterPathfindingCharacters *floaterInstance = LLFloaterReg::findTypedInstance<LLFloaterPathfindingCharacters>("pathfinding_characters");
		if (floaterInstance != nullptr)
		{
			sInstanceHandle = floaterInstance->mSelfHandle;
		}
	}

	return sInstanceHandle;
}

LLFloaterPathfindingCharacters::LLFloaterPathfindingCharacters(const LLSD& pSeed)
	: LLFloaterPathfindingObjects(pSeed),
	mSelectedCharacterId(),
	mBeaconColor(),
	mSelfHandle()
{
	mSelfHandle.bind(this);
}

LLFloaterPathfindingCharacters::~LLFloaterPathfindingCharacters()
{
}

BOOL LLFloaterPathfindingCharacters::postBuild()
{
	mBeaconColor = LLUIColorTable::getInstance()->getColor("PathfindingCharacterBeaconColor");

	return LLFloaterPathfindingObjects::postBuild();
}

void LLFloaterPathfindingCharacters::requestGetObjects()
{
	LLPathfindingManager::getInstance()->requestGetCharacters(getNewRequestId(), boost::bind(&LLFloaterPathfindingCharacters::handleNewObjectList, this, _1, _2, _3));
}

void LLFloaterPathfindingCharacters::buildObjectsScrollList(const LLPathfindingObjectListPtr pObjectListPtr)
{
	llassert(pObjectListPtr != NULL);
	llassert(!pObjectListPtr->isEmpty());

	for (LLPathfindingObjectList::const_iterator objectIter = pObjectListPtr->begin();	objectIter != pObjectListPtr->end(); ++objectIter)
	{
		const LLPathfindingObjectPtr objectPtr = objectIter->second;
		const LLPathfindingCharacter *characterPtr = dynamic_cast<const LLPathfindingCharacter *>(objectPtr.get());
		llassert(characterPtr != NULL);

		LLSD scrollListItemData = buildCharacterScrollListItemData(characterPtr);
		addObjectToScrollList(objectPtr, scrollListItemData);
	}
}

void LLFloaterPathfindingCharacters::updateControlsOnScrollListChange()
{
	LLFloaterPathfindingObjects::updateControlsOnScrollListChange();
}

S32 LLFloaterPathfindingCharacters::getNameColumnIndex() const
{
	return 0;
}

S32 LLFloaterPathfindingCharacters::getOwnerNameColumnIndex() const
{
	return 2;
}

std::string LLFloaterPathfindingCharacters::getOwnerName(const LLPathfindingObject *pObject) const
{
	return (pObject->hasOwner()
		? (pObject->hasOwnerName()
		? (pObject->isGroupOwned()
		? (pObject->getOwnerName() + " " + getString("character_owner_group"))
		: pObject->getOwnerName())
		: getString("character_owner_loading"))
		: getString("character_owner_unknown"));
}

const LLColor4 &LLFloaterPathfindingCharacters::getBeaconColor() const
{
	return mBeaconColor;
}

LLPathfindingObjectListPtr LLFloaterPathfindingCharacters::getEmptyObjectList() const
{
	LLPathfindingObjectListPtr objectListPtr(new LLPathfindingCharacterList());
	return objectListPtr;
}

LLSD LLFloaterPathfindingCharacters::buildCharacterScrollListItemData(const LLPathfindingCharacter *pCharacterPtr) const
{
	LLSD columns = LLSD::emptyArray();

	columns[0]["column"] = "name";
	columns[0]["value"] = pCharacterPtr->getName();

	columns[1]["column"] = "description";
	columns[1]["value"] = pCharacterPtr->getDescription();

	columns[2]["column"] = "owner";
	columns[2]["value"] = getOwnerName(pCharacterPtr);

	S32 cpuTime = ll_round(pCharacterPtr->getCPUTime());
	std::string cpuTimeString = llformat("%d", cpuTime);
	LLStringUtil::format_map_t string_args;
	string_args["[CPU_TIME]"] = cpuTimeString;

	columns[3]["column"] = "cpu_time";
	columns[3]["value"] = getString("character_cpu_time", string_args);

	columns[4]["column"] = "altitude";
	columns[4]["value"] = llformat("%1.0f m", pCharacterPtr->getLocation()[2]);

	return columns;
}
