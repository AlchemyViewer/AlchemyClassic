/** 
* @file   llfloaterpathfindingcharacters.h
* @brief  "Pathfinding characters" floater, allowing for identification of pathfinding characters and their cpu usage.
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
#ifndef LL_LLFLOATERPATHFINDINGCHARACTERS_H
#define LL_LLFLOATERPATHFINDINGCHARACTERS_H

#include "llfloaterpathfindingobjects.h"
#include "llhandle.h"
#include "llpathfindingobjectlist.h"
#include "lluuid.h"
#include "v4color.h"

class LLCheckBoxCtrl;
class LLPathfindingCharacter;
class LLQuaternion;
class LLSD;
class LLVector3;

class LLFloaterPathfindingCharacters : public LLFloaterPathfindingObjects
{
public:
	void                                    onClose(bool pIsAppQuitting) override;

	static void                                     openCharactersWithSelectedObjects();
	static LLHandle<LLFloaterPathfindingCharacters> getInstanceHandle();

protected:
	friend class LLFloaterReg;

	LLFloaterPathfindingCharacters(const LLSD& pSeed);
	virtual ~LLFloaterPathfindingCharacters();

	BOOL                       postBuild() override;

	void                       requestGetObjects() override;

	void                       buildObjectsScrollList(const LLPathfindingObjectListPtr pObjectListPtr) override;

	void                       updateControlsOnScrollListChange() override;

	S32                        getNameColumnIndex() const override;
	S32                        getOwnerNameColumnIndex() const override;
	std::string                getOwnerName(const LLPathfindingObject *pObject) const override;
	const LLColor4             &getBeaconColor() const override;

	LLPathfindingObjectListPtr getEmptyObjectList() const override;

private:
	LLSD buildCharacterScrollListItemData(const LLPathfindingCharacter *pCharacterPtr) const;

	LLUUID                                           mSelectedCharacterId;

	LLColor4                                         mBeaconColor;

	LLRootHandle<LLFloaterPathfindingCharacters>     mSelfHandle;
	static LLHandle<LLFloaterPathfindingCharacters>  sInstanceHandle;
};

#endif // LL_LLFLOATERPATHFINDINGCHARACTERS_H
