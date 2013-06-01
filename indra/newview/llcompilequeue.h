/** 
 * @file llcompilequeue.h
 * @brief LLCompileQueue class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#ifndef LL_LLCOMPILEQUEUE_H
#define LL_LLCOMPILEQUEUE_H

#include "lldarray.h"
#include "llinventory.h"
#include "llviewerobject.h"
#include "llvoinventorylistener.h"
#include "llmap.h"
#include "lluuid.h"

#include "llfloater.h"
#include "llscrolllistctrl.h"

#include "llviewerinventory.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterScriptQueue
//
// This class provides a mechanism of adding objects to a list that
// will go through and execute action for the scripts on each object. The
// objects will be accessed serially and the scripts may be
// manipulated in parallel. For example, selecting two objects each
// with three scripts will result in the first object having all three
// scripts manipulated.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterScriptQueue : public LLFloater, public LLVOInventoryListener
{
public:
	LLFloaterScriptQueue(const LLSD& key);
	virtual ~LLFloaterScriptQueue();

	/*virtual*/ BOOL postBuild();
	
	void setMono(bool mono) { mMono = mono; }
	
	// addObject() accepts an object id.
	void addObject(const LLUUID& id);

	// start() returns TRUE if the queue has started, otherwise FALSE.
	BOOL start();
	
protected:
	// This is the callback method for the viewer object currently
	// being worked on.
	/*virtual*/ void inventoryChanged(LLViewerObject* obj,
								 LLInventoryObject::object_list_t* inv,
								 S32 serial_num,
								 void* queue);
	
	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								LLInventoryObject::object_list_t* inv) = 0;

	static void onCloseBtn(void* user_data);

	// returns true if this is done
	BOOL isDone() const;

	// go to the next object. If no objects left, it falls out
	// silently and waits to be killed by the deleteIfDone() callback.
	BOOL nextObject();
	BOOL popNext();

	void setStartString(const std::string& s) { mStartString = s; }
	
protected:
	// UI
	LLScrollListCtrl* mMessages;
	LLButton* mCloseBtn;

	// Object Queue
	LLDynamicArray<LLUUID> mObjectIDs;
	LLUUID mCurrentObjectID;
	bool mDone;

	std::string mStartString;
	bool mMono;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterCompileQueue
//
// This script queue will recompile each script.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct LLCompileQueueData
{
	LLUUID mQueueID;
	LLUUID mItemId;
	LLCompileQueueData(const LLUUID& q_id, const LLUUID& item_id) :
		mQueueID(q_id), mItemId(item_id) {}
};

class LLAssetUploadQueue;

class LLFloaterCompileQueue : public LLFloaterScriptQueue
{
	friend class LLFloaterReg;
public:
	static void onSaveBytecodeComplete(const LLUUID& asset_id,
									void* user_data,
									S32 status);
									
	// remove any object in mScriptScripts with the matching uuid.
	void removeItemByItemID(const LLUUID& item_id);
	
	LLAssetUploadQueue* getUploadQueue() { return mUploadQueue; }

protected:
	LLFloaterCompileQueue(const LLSD& key);
	virtual ~LLFloaterCompileQueue();
	
	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								LLInventoryObject::object_list_t* inv);

	// This is the callback for when each script arrives
	static void scriptArrived(LLVFS *vfs, const LLUUID& asset_id,
								LLAssetType::EType type,
								void* user_data, S32 status, LLExtStat ext_status);

	static void onSaveTextComplete(const LLUUID& asset_id, void* user_data, S32 status, LLExtStat ext_status);

	static void onSaveBytecodeComplete(const LLUUID& asset_id,
									   void* user_data,
									   S32 status, LLExtStat ext_status);

	// compile the file given and save it out.
	void compile(const std::string& filename, const LLUUID& asset_id);
	
	// remove any object in mScriptScripts with the matching uuid.
	void removeItemByAssetID(const LLUUID& asset_id);

	// save the items indicated by the item id.
	void saveItemByItemID(const LLUUID& item_id);

	// find InventoryItem given item id.
	const LLInventoryItem* findItemByItemID(const LLUUID& item_id) const;
	
protected:
	LLViewerInventoryItem::item_array_t mCurrentScripts;

private:
	LLAssetUploadQueue* mUploadQueue;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterResetQueue
//
// This script queue will reset each script.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterResetQueue : public LLFloaterScriptQueue
{
	friend class LLFloaterReg;
protected:
	LLFloaterResetQueue(const LLSD& key);
	virtual ~LLFloaterResetQueue();
	
	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								LLInventoryObject::object_list_t* inv);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterRunQueue
//
// This script queue will set each script as running.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterRunQueue : public LLFloaterScriptQueue
{
	friend class LLFloaterReg;
protected:
	LLFloaterRunQueue(const LLSD& key);
	virtual ~LLFloaterRunQueue();
	
	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								LLInventoryObject::object_list_t* inv);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterNotRunQueue
//
// This script queue will set each script as not running.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterNotRunQueue : public LLFloaterScriptQueue
{
	friend class LLFloaterReg;
protected:
	LLFloaterNotRunQueue(const LLSD& key);
	virtual ~LLFloaterNotRunQueue();
	
	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								LLInventoryObject::object_list_t* inv);
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFloaterDeleteQueue
//
// This script queue will remove each script.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFloaterDeleteQueue : public LLFloaterScriptQueue
{
	friend class LLFloaterReg;
protected:
	LLFloaterDeleteQueue(const LLSD& key);
	virtual ~LLFloaterDeleteQueue();
	
	// This is called by inventoryChanged
	virtual void handleInventory(LLViewerObject* viewer_obj,
								LLInventoryObject::object_list_t* inv);
};
#endif // LL_LLCOMPILEQUEUE_H
