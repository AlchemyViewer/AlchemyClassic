/** 
 * @file llcompilequeue.cpp
 * @brief LLCompileQueueData class implementation
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

/**
 *
 * Implementation of the script queue which keeps an array of object
 * UUIDs and manipulates all of the scripts on each of them. 
 *
 */


#include "llviewerprecompiledheaders.h"

#include "llcompilequeue.h"

#include "llagent.h"
#include "llassetuploadqueue.h"
#include "llassetuploadresponders.h"
#include "llchat.h"
#include "llfloaterreg.h"
#include "llviewerwindow.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llresmgr.h"

#include "llbutton.h"
#include "lldir.h"
#include "llnotificationsutil.h"
#include "llviewerstats.h"
#include "llvfile.h"
#include "lluictrlfactory.h"
#include "lltrans.h"

#include "llselectmgr.h"

// *TODO: This should be separated into the script queue, and the floater views of that queue.
// There should only be one floater class that can view any queue type

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

struct LLScriptQueueData
{
	LLUUID mQueueID;
	std::string mScriptName;
	LLUUID mTaskId;
	LLUUID mItemId;
	LLScriptQueueData(const LLUUID& q_id, const std::string& name, const LLUUID& task_id, const LLUUID& item_id) :
		mQueueID(q_id), mScriptName(name), mTaskId(task_id), mItemId(item_id) {}

};

///----------------------------------------------------------------------------
/// Class LLFloaterScriptQueue
///----------------------------------------------------------------------------

// Default constructor
LLFloaterScriptQueue::LLFloaterScriptQueue(const LLSD& key) :
	LLFloater(key),
	mDone(false),
	mMono(false)
{
}

// Destroys the object
LLFloaterScriptQueue::~LLFloaterScriptQueue()
{
}

BOOL LLFloaterScriptQueue::postBuild()
{
	childSetAction("close",onCloseBtn,this);
	getChildView("close")->setEnabled(FALSE);
	setVisible(true);
	return TRUE;
}

// This is the callback method for the viewer object currently being
// worked on.
// NOT static, virtual!
void LLFloaterScriptQueue::inventoryChanged(LLViewerObject* viewer_object,
											 LLInventoryObject::object_list_t* inv,
											 S32,
											 void* q_id)
{
	LL_INFOS() << "LLFloaterScriptQueue::inventoryChanged() for  object "
			<< viewer_object->getID() << LL_ENDL;

	//Remove this listener from the object since its
	//listener callback is now being executed.
	
	//We remove the listener here because the function
	//removeVOInventoryListener removes the listener from a ViewerObject
	//which it internally stores.
	
	//If we call this further down in the function, calls to handleInventory
	//and nextObject may update the interally stored viewer object causing
	//the removal of the incorrect listener from an incorrect object.
	
	//Fixes SL-6119:Recompile scripts fails to complete
	removeVOInventoryListener();

	if (viewer_object && inv && (viewer_object->getID() == mCurrentObjectID) )
	{
		handleInventory(viewer_object, inv);
	}
	else
	{
		// something went wrong...
		// note that we're not working on this one, and move onto the
		// next object in the list.
		LL_WARNS() << "No inventory for " << mCurrentObjectID
				<< LL_ENDL;
		nextObject();
	}
}


// static
void LLFloaterScriptQueue::onCloseBtn(void* user_data)
{
	LLFloaterScriptQueue* self = (LLFloaterScriptQueue*)user_data;
	self->closeFloater();
}

void LLFloaterScriptQueue::addObject(const LLUUID& id)
{
	mObjectIDs.push_back(id);
}

BOOL LLFloaterScriptQueue::start()
{
	std::string buffer;

	LLStringUtil::format_map_t args;
	args["[START]"] = mStartString;
	args["[COUNT]"] = llformat ("%d", mObjectIDs.size());
	buffer = getString ("Starting", args);
	
	getChild<LLScrollListCtrl>("queue output")->addSimpleElement(buffer, ADD_BOTTOM);

	return nextObject();
}

BOOL LLFloaterScriptQueue::isDone() const
{
	return (mCurrentObjectID.isNull() && (mObjectIDs.size() == 0));
}

// go to the next object. If no objects left, it falls out silently
// and waits to be killed by the window being closed.
BOOL LLFloaterScriptQueue::nextObject()
{
	U32 count;
	BOOL successful_start = FALSE;
	do
	{
		count = mObjectIDs.size();
		LL_INFOS() << "LLFloaterScriptQueue::nextObject() - " << count
				<< " objects left to process." << LL_ENDL;
		mCurrentObjectID.setNull();
		if(count > 0)
		{
			successful_start = popNext();
		}
		LL_INFOS() << "LLFloaterScriptQueue::nextObject() "
				<< (successful_start ? "successful" : "unsuccessful")
				<< LL_ENDL; 
	} while((mObjectIDs.size() > 0) && !successful_start);
	if(isDone() && !mDone)
	{
		mDone = true;
		getChild<LLScrollListCtrl>("queue output")->addSimpleElement(getString("Done"), ADD_BOTTOM);
		getChildView("close")->setEnabled(TRUE);
	}
	return successful_start;
}

// returns true if the queue has started, otherwise false.  This
// method pops the top object off of the queue.
BOOL LLFloaterScriptQueue::popNext()
{
	// get the first element off of the container, and attempt to get
	// the inventory.
	BOOL rv = FALSE;
	S32 count = mObjectIDs.size();
	if(mCurrentObjectID.isNull() && (count > 0))
	{
		mCurrentObjectID = mObjectIDs.at(0);
		LL_INFOS() << "LLFloaterScriptQueue::popNext() - mCurrentID: "
				<< mCurrentObjectID << LL_ENDL;
		mObjectIDs.erase(mObjectIDs.begin());
		LLViewerObject* obj = gObjectList.findObject(mCurrentObjectID);
		if(obj)
		{
			LL_INFOS() << "LLFloaterScriptQueue::popNext() requesting inv for "
					<< mCurrentObjectID << LL_ENDL;
			LLUUID* id = new LLUUID(getKey().asUUID());
			registerVOInventoryListener(obj,id);
			requestVOInventory();
			rv = TRUE;
		}
	}
	return rv;
}


///----------------------------------------------------------------------------
/// Class LLFloaterCompileQueue
///----------------------------------------------------------------------------

class LLCompileFloaterUploadQueueSupplier : public LLAssetUploadQueueSupplier
{
public:
	
	LLCompileFloaterUploadQueueSupplier(const LLUUID& queue_id) :
		mQueueId(queue_id)
	{
	}
		
	virtual LLAssetUploadQueue* get() const 
	{
		LLFloaterCompileQueue* queue = LLFloaterReg::findTypedInstance<LLFloaterCompileQueue>("compile_queue", LLSD(mQueueId));
		if(NULL == queue)
		{
			return NULL;
		}
		return queue->getUploadQueue();
	}

	virtual void log(std::string message) const
	{
		LLFloaterCompileQueue* queue = LLFloaterReg::findTypedInstance<LLFloaterCompileQueue>("compile_queue", LLSD(mQueueId));
		if(NULL == queue)
		{
			return;
		}

		queue->getChild<LLScrollListCtrl>("queue output")->addSimpleElement(message, ADD_BOTTOM);
	}
		
private:
	LLUUID mQueueId;
};

LLFloaterCompileQueue::LLFloaterCompileQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("CompileQueueTitle"));
	setStartString(LLTrans::getString("CompileQueueStart"));
														 															 
	mUploadQueue = new LLAssetUploadQueue(new LLCompileFloaterUploadQueueSupplier(key.asUUID()));
}

LLFloaterCompileQueue::~LLFloaterCompileQueue()
{ 
}

void LLFloaterCompileQueue::handleInventory(LLViewerObject *viewer_object,
											LLInventoryObject::object_list_t* inv)
{
	// find all of the lsl, leaving off duplicates. We'll remove
	// all matching asset uuids on compilation success.

	typedef std::multimap<LLUUID, LLPointer<LLInventoryItem> > uuid_item_map;
	uuid_item_map asset_item_map;

	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		if((*it)->getType() == LLAssetType::AT_LSL_TEXT)
		{
			LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
			// Check permissions before allowing the user to retrieve data.
			if (item->getPermissions().allowModifyBy(gAgent.getID(), gAgent.getGroupID())  &&
				item->getPermissions().allowCopyBy(gAgent.getID(), gAgent.getGroupID()) )
			{
				LLPointer<LLViewerInventoryItem> script = new LLViewerInventoryItem(item);
				mCurrentScripts.push_back(script);
				asset_item_map.insert(std::make_pair(item->getAssetUUID(), item));
			}
		}
	}

	if (asset_item_map.empty())
	{
		// There are no scripts in this object.  move on.
		nextObject();
	}
	else
	{
		// request all of the assets.
		uuid_item_map::iterator iter;
		for(iter = asset_item_map.begin(); iter != asset_item_map.end(); iter++)
		{
			LLInventoryItem *itemp = iter->second;
			LLScriptQueueData* datap = new LLScriptQueueData(getKey().asUUID(),
												 itemp->getName(),
												 viewer_object->getID(),
												 itemp->getUUID());

			//LL_INFOS() << "ITEM NAME 2: " << names.get(i) << LL_ENDL;
			gAssetStorage->getInvItemAsset(viewer_object->getRegion()->getHost(),
				gAgent.getID(),
				gAgent.getSessionID(),
				itemp->getPermissions().getOwner(),
				viewer_object->getID(),
				itemp->getUUID(),
				itemp->getAssetUUID(),
				itemp->getType(),
				LLFloaterCompileQueue::scriptArrived,
				(void*)datap);
		}
	}
}

// This is the callback for when each script arrives
// static
void LLFloaterCompileQueue::scriptArrived(LLVFS *vfs, const LLUUID& asset_id,
										  LLAssetType::EType type,
										  void* user_data, S32 status, LLExtStat ext_status)
{
	LL_INFOS() << "LLFloaterCompileQueue::scriptArrived()" << LL_ENDL;
	LLScriptQueueData* data = (LLScriptQueueData*)user_data;
	if(!data)
	{
		return;
	}
	LLFloaterCompileQueue* queue = LLFloaterReg::findTypedInstance<LLFloaterCompileQueue>("compile_queue", data->mQueueID);
	
	std::string buffer;
	if(queue && (0 == status))
	{
		//LL_INFOS() << "ITEM NAME 3: " << data->mScriptName << LL_ENDL;

		// Dump this into a file on the local disk so we can compile it.
		std::string filename;
		LLVFile file(vfs, asset_id, type);
		std::string uuid_str;
		asset_id.toString(uuid_str);
		filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,uuid_str) + llformat(".%s",LLAssetType::lookup(type));
		
		const bool is_running = true;
		LLViewerObject* object = gObjectList.findObject(data->mTaskId);
		if (object)
		{
			std::string url = object->getRegion()->getCapability("UpdateScriptTask");
			if(!url.empty())
			{
				// Read script source in to buffer.
				U32 script_size = file.getSize();
				U8* script_data = new U8[script_size];
				file.read(script_data, script_size);

				queue->mUploadQueue->queue(filename, data->mTaskId, 
										   data->mItemId, is_running, queue->mMono, queue->getKey().asUUID(),
										   script_data, script_size, data->mScriptName);
			}
			else
			{
				buffer = LLTrans::getString("CompileQueueServiceUnavailable") + (": ") + data->mScriptName;
			}
		}
	}
	else
	{
		if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status )
		{
			LLSD args;
			args["MESSAGE"] = LLTrans::getString("CompileQueueScriptNotFound");
			LLNotificationsUtil::add("SystemMessage", args);
			
			buffer = LLTrans::getString("CompileQueueProblemDownloading") + (": ") + data->mScriptName;
		}
		else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
		{
			LLSD args;
			args["MESSAGE"] = LLTrans::getString("CompileQueueInsufficientPermDownload");
			LLNotificationsUtil::add("SystemMessage", args);

			buffer = LLTrans::getString("CompileQueueInsufficientPermFor") + (": ") + data->mScriptName;
		}
		else
		{
			buffer = LLTrans::getString("CompileQueueUnknownFailure") + (" ") + data->mScriptName;
		}

		LL_WARNS() << "Problem downloading script asset." << LL_ENDL;
		if(queue) queue->removeItemByItemID(data->mItemId);
	}
	if(queue && (buffer.size() > 0)) 
	{
		queue->getChild<LLScrollListCtrl>("queue output")->addSimpleElement(buffer, ADD_BOTTOM);
	}
	delete data;
}


///----------------------------------------------------------------------------
/// Class LLFloaterResetQueue
///----------------------------------------------------------------------------

LLFloaterResetQueue::LLFloaterResetQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("ResetQueueTitle"));
	setStartString(LLTrans::getString("ResetQueueStart"));
}

LLFloaterResetQueue::~LLFloaterResetQueue()
{ 
}

void LLFloaterResetQueue::handleInventory(LLViewerObject* viewer_obj,
										  LLInventoryObject::object_list_t* inv)
{
	// find all of the lsl, leaving off duplicates. We'll remove
	// all matching asset uuids on compilation success.

	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		if((*it)->getType() == LLAssetType::AT_LSL_TEXT)
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
				std::string buffer;
				buffer = getString("Resetting") + (": ") + item->getName();
				getChild<LLScrollListCtrl>("queue output")->addSimpleElement(buffer, ADD_BOTTOM);
				LLMessageSystem* msg = gMessageSystem;
				msg->newMessageFast(_PREHASH_ScriptReset);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_Script);
				msg->addUUIDFast(_PREHASH_ObjectID, viewer_obj->getID());
				msg->addUUIDFast(_PREHASH_ItemID, (*it)->getUUID());
				msg->sendReliable(object->getRegion()->getHost());
			}
		}
	}

	nextObject();	
}

///----------------------------------------------------------------------------
/// Class LLFloaterRunQueue
///----------------------------------------------------------------------------

LLFloaterRunQueue::LLFloaterRunQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("RunQueueTitle"));
	setStartString(LLTrans::getString("RunQueueStart"));
}

LLFloaterRunQueue::~LLFloaterRunQueue()
{ 
}

void LLFloaterRunQueue::handleInventory(LLViewerObject* viewer_obj,
										  LLInventoryObject::object_list_t* inv)
{
	// find all of the lsl, leaving off duplicates. We'll remove
	// all matching asset uuids on compilation success.
	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		if((*it)->getType() == LLAssetType::AT_LSL_TEXT)
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
				LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");
				std::string buffer;
				buffer = getString("Running") + (": ") + item->getName();
				list->addSimpleElement(buffer, ADD_BOTTOM);

				LLMessageSystem* msg = gMessageSystem;
				msg->newMessageFast(_PREHASH_SetScriptRunning);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_Script);
				msg->addUUIDFast(_PREHASH_ObjectID, viewer_obj->getID());
				msg->addUUIDFast(_PREHASH_ItemID, (*it)->getUUID());
				msg->addBOOLFast(_PREHASH_Running, TRUE);
				msg->sendReliable(object->getRegion()->getHost());
			}
		}
	}

	nextObject();	
}

///----------------------------------------------------------------------------
/// Class LLFloaterNotRunQueue
///----------------------------------------------------------------------------

LLFloaterNotRunQueue::LLFloaterNotRunQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("NotRunQueueTitle"));
	setStartString(LLTrans::getString("NotRunQueueStart"));
}

LLFloaterNotRunQueue::~LLFloaterNotRunQueue()
{ 
}

void LLFloaterCompileQueue::removeItemByItemID(const LLUUID& asset_id)
{
	LL_INFOS() << "LLFloaterCompileQueue::removeItemByAssetID()" << LL_ENDL;
	for(S32 i = 0; i < mCurrentScripts.size(); )
	{
		if(asset_id == mCurrentScripts.at(i)->getUUID())
		{
			LLViewerInventoryItem::item_array_t::iterator iter = mCurrentScripts.begin() + i;
			vector_replace_with_last(mCurrentScripts, iter);
		}
		else
		{
			++i;
		}
	}
	if(mCurrentScripts.empty())
	{
		nextObject();
	}
}

void LLFloaterNotRunQueue::handleInventory(LLViewerObject* viewer_obj,
										  LLInventoryObject::object_list_t* inv)
{
	// find all of the lsl, leaving off duplicates. We'll remove
	// all matching asset uuids on compilation success.
	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		if((*it)->getType() == LLAssetType::AT_LSL_TEXT)
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
				LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");
				std::string buffer;
				buffer = getString("NotRunning") + (": ") +item->getName();
				list->addSimpleElement(buffer, ADD_BOTTOM);
	
				LLMessageSystem* msg = gMessageSystem;
				msg->newMessageFast(_PREHASH_SetScriptRunning);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_Script);
				msg->addUUIDFast(_PREHASH_ObjectID, viewer_obj->getID());
				msg->addUUIDFast(_PREHASH_ItemID, (*it)->getUUID());
				msg->addBOOLFast(_PREHASH_Running, FALSE);
				msg->sendReliable(object->getRegion()->getHost());
			}
		}
	}

	nextObject();	
}

///----------------------------------------------------------------------------
/// Class LLFloaterDeleteQueue
///----------------------------------------------------------------------------

LLFloaterDeleteQueue::LLFloaterDeleteQueue(const LLSD& key)
  : LLFloaterScriptQueue(key)
{
	setTitle(LLTrans::getString("DeleteQueueTitle"));
	setStartString(LLTrans::getString("DeleteQueueStart"));
}

LLFloaterDeleteQueue::~LLFloaterDeleteQueue()
{ 
}

void LLFloaterDeleteQueue::handleInventory(LLViewerObject* viewer_obj,
										  LLInventoryObject::object_list_t* inv)
{
	if (viewer_obj && inv)
	{
		LLViewerObject* objectp = gObjectList.findObject(viewer_obj->getID());
		const std::string& delstring = getString("Deleting");
		LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");
		if (objectp)
		{
			const LLInventoryObject::object_list_t::const_iterator it_end = inv->end();
			for (LLInventoryObject::object_list_t::const_iterator it = inv->begin(); it != it_end; ++it)
			{
				const LLInventoryObject* item = static_cast<LLInventoryObject*>(*it);
				if (item && item->getType() == LLAssetType::AT_LSL_TEXT)
				{
					list->addSimpleElement(delstring + item->getName(), ADD_BOTTOM);

					LLMessageSystem* msg = gMessageSystem;
					msg->newMessageFast(_PREHASH_RemoveTaskInventory);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					msg->nextBlockFast(_PREHASH_InventoryData);
					msg->addU32Fast(_PREHASH_LocalID, objectp->getLocalID());
					msg->addUUIDFast(_PREHASH_ItemID, item->getUUID());
					msg->sendReliable(objectp->getRegion()->getHost());
				}
			}
			objectp->dirtyInventory();
		}
	}
	nextObject();
}

///----------------------------------------------------------------------------
/// Local function definitions
///----------------------------------------------------------------------------
