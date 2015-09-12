/**
 * @file lleasymessagesender.cpp
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llfloatermessagebuilder.h"
#include "lluictrlfactory.h"
#include "llmessagetemplate.h"
#include "llagent.h"
#include "llviewerregion.h" // getHandle
#include "llcombobox.h"
#include "llselectmgr.h" // fill in stuff about selected object
#include "llparcel.h"
#include "llviewerparcelmgr.h" // same for parcel
#include "llscrolllistctrl.h"
#include "llworld.h"
#include "lltemplatemessagebuilder.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "lleasymessagesender.h"
#include "lltextbase.h"

////////////////////////////////
// LLNetListItem
////////////////////////////////
LLNetListItem::LLNetListItem(LLUUID id)
:	mID(id),
	mAutoName(TRUE),
	mName("No name"),
	mPreviousRegionName(""),
	mCircuitData(NULL),
	mHandle(0)
{
}

////////////////////////////////
// LLFloaterMessageBuilder
////////////////////////////////
std::list<LLNetListItem*> LLFloaterMessageBuilder::sNetListItems;

LLFloaterMessageBuilder::LLFloaterMessageBuilder(const LLSD& key)
:	LLFloater(key),
	LLEventTimer(1.0f)
//	mNetInfoMode(NI_NET)
{
}

void LLFloaterMessageBuilder::show(const std::string& initial_text)
{
	LLFloaterReg::showInstance("message_builder", initial_text.length() > 0
							   ? LLSD().with("initial_text", initial_text) : LLSD());
}

void LLFloaterMessageBuilder::onOpen(const LLSD& key)
{
	if(key.has("initial_text"))
	{
		mInitialText = key["initial_text"].asString();
		getChild<LLTextBase>("message_edit")->setText(mInitialText);
	}
}
BOOL LLFloaterMessageBuilder::tick()
{
	refreshNetList();
	return FALSE;
}
LLNetListItem* LLFloaterMessageBuilder::findNetListItem(LLHost host)
{
	std::list<LLNetListItem*>::iterator end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		if((*iter)->mCircuitData && (*iter)->mCircuitData->getHost() == host)
			return (*iter);
	return NULL;
}
LLNetListItem* LLFloaterMessageBuilder::findNetListItem(LLUUID id)
{
	std::list<LLNetListItem*>::iterator end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		if((*iter)->mID == id)
			return (*iter);
	return NULL;
}
void LLFloaterMessageBuilder::refreshNetList()
{
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("net_list");
	// Update circuit data of net list items
	std::vector<LLCircuitData*> circuits = gMessageSystem->getCircuit()->getCircuitDataList();
	std::vector<LLCircuitData*>::iterator circuits_end = circuits.end();
	for(std::vector<LLCircuitData*>::iterator iter = circuits.begin(); iter != circuits_end; ++iter)
	{
		LLNetListItem* itemp = findNetListItem((*iter)->getHost());
		if(!itemp)
		{
			LLUUID id; id.generate();
			itemp = new LLNetListItem(id);
			sNetListItems.push_back(itemp);
		}
		itemp->mCircuitData = (*iter);
	}
	// Clear circuit data of items whose circuits are gone
	std::list<LLNetListItem*>::iterator items_end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != items_end; ++iter)
	{
		if(std::find(circuits.begin(), circuits.end(), (*iter)->mCircuitData) == circuits.end())
			(*iter)->mCircuitData = NULL;
	}
	// Remove net list items that are totally useless now
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != sNetListItems.end();)
	{
		if((*iter)->mCircuitData == NULL)
			iter = sNetListItems.erase(iter);
		else ++iter;
	}
	// Update names of net list items
	items_end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != items_end; ++iter)
	{
		LLNetListItem* itemp = (*iter);
		if(itemp->mAutoName)
		{
			if(itemp->mCircuitData)
			{
				LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(itemp->mCircuitData->getHost());
				if(regionp)
				{
					std::string name = regionp->getName();
					if(name == "") name = llformat("%s (awaiting region name)", itemp->mCircuitData->getHost().getString().c_str());
					itemp->mName = name;
					itemp->mPreviousRegionName = name;
				}
				else
				{
					itemp->mName = itemp->mCircuitData->getHost().getString();
					if(itemp->mPreviousRegionName != "")
						itemp->mName.append(llformat(" (was %s)", itemp->mPreviousRegionName.c_str()));
				}
			}
			else
			{
				// an item just for an event queue, not handled yet
				itemp->mName = "Something else";
			}
		}
	}
	// Rebuild scroll list from scratch
	LLUUID selected_id = scrollp->getFirstSelected() ? scrollp->getFirstSelected()->getUUID() : LLUUID::null;
	S32 scroll_pos = scrollp->getScrollPos();
	scrollp->clearRows();
	for (LLNetListItem* itemp : sNetListItems)
	{
		LLSD element;
		element["id"] = itemp->mID;
		LLSD& text_column = element["columns"][0];
		text_column["column"] = "text";
		text_column["value"] = itemp->mName + (itemp->mCircuitData->getHost() == gAgent.getRegionHost() ? " (main)" : "");

		LLSD& state_column = element["columns"][ 1];
		state_column["column"] = "state";
		state_column["value"] = "";

		LLScrollListItem* scroll_itemp = scrollp->addElement(element);
		BOOL has_live_circuit = itemp->mCircuitData && itemp->mCircuitData->isAlive();

		LLScrollListText* state = (LLScrollListText*)scroll_itemp->getColumn(1);

		if(has_live_circuit)
			state->setText(LLStringExplicit("Alive"));
		else
			state->setText(LLStringExplicit("Dead"));
	}
	if(selected_id.notNull())
		scrollp->selectByID(selected_id);
	if(scroll_pos < scrollp->getItemCount())
		scrollp->setScrollPos(scroll_pos);
}
BOOL LLFloaterMessageBuilder::postBuild()
{
	getChild<LLTextBase>("message_edit")->setText(mInitialText);
	getChild<LLUICtrl>("send_btn")->setCommitCallback(boost::bind(&LLFloaterMessageBuilder::onClickSend, this));
	std::vector<std::string> names;
	LLComboBox* combo;
	LLMessageSystem::message_template_name_map_t::iterator temp_end = gMessageSystem->mMessageTemplates.end();
	LLMessageSystem::message_template_name_map_t::iterator temp_iter;
	std::vector<std::string>::iterator names_end;
	std::vector<std::string>::iterator names_iter;
	for(temp_iter = gMessageSystem->mMessageTemplates.begin(); temp_iter != temp_end; ++temp_iter)
		if((*temp_iter).second->getTrust() == MT_NOTRUST)
			names.push_back((*temp_iter).second->mName);
	std::sort(names.begin(), names.end());
	combo = getChild<LLComboBox>("untrusted_message_combo");
	names_end = names.end();
	for(names_iter = names.begin(); names_iter != names_end; ++names_iter)
		combo->add((*names_iter));
	names.clear();
	for(temp_iter = gMessageSystem->mMessageTemplates.begin(); temp_iter != temp_end; ++temp_iter)
		if((*temp_iter).second->getTrust() == MT_TRUST)
			names.push_back((*temp_iter).second->mName);
	std::sort(names.begin(), names.end());
	combo = getChild<LLComboBox>("trusted_message_combo");
	names_end = names.end();
	for(names_iter = names.begin(); names_iter != names_end; ++names_iter)
		combo->add((*names_iter));
	getChild<LLUICtrl>("untrusted_message_combo")->setCommitCallback(boost::bind(&LLFloaterMessageBuilder::onCommitPacketCombo, this, _1));
	getChild<LLUICtrl>("trusted_message_combo")->setCommitCallback(boost::bind(&LLFloaterMessageBuilder::onCommitPacketCombo, this, _1));
	return TRUE;
}

void LLFloaterMessageBuilder::onCommitPacketCombo(LLUICtrl* ctrl)
{
	LLViewerObject* selected_objectp = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
	LLParcel* agent_parcelp = LLViewerParcelMgr::getInstance()->getAgentParcel();
	std::string message = ctrl->getValue();
	std::map<const char *, LLMessageTemplate*>::iterator template_iter;
	template_iter = gMessageSystem->mMessageTemplates.find(LLMessageStringTable::getInstance()->getString(message.c_str()));
	if(template_iter == gMessageSystem->mMessageTemplates.end())
	{
		getChild<LLTextBase>("message_edit")->setText(LLStringUtil::null);
		return;
	}
	std::string text(llformat((*template_iter).second->getTrust() == MT_NOTRUST
							  ? "out %s\n\n" : "in %s\n\n", message.c_str()));
	LLMessageTemplate* temp = (*template_iter).second;
	LLMessageTemplate::message_block_map_t::iterator blocks_end = temp->mMemberBlocks.end();
	for (LLMessageTemplate::message_block_map_t::iterator blocks_iter = temp->mMemberBlocks.begin();
		 blocks_iter != blocks_end; ++blocks_iter)
	{
		LLMessageBlock* block = (*blocks_iter);
		const char* block_name = block->mName;
		std::string block_name_string = std::string(block_name);
		S32 num_blocks = 1;
		if(block->mType == MBT_MULTIPLE)
			num_blocks = block->mNumber;
		else if(("ObjectLink" == message && "ObjectData" == block_name_string))
			num_blocks = 2;
		for(S32 i = 0; i < num_blocks; i++)
		{
			text.append(llformat("[%s]\n", block_name));
			LLMessageBlock::message_variable_map_t::iterator var_end = block->mMemberVariables.end();
			for (LLMessageBlock::message_variable_map_t::iterator var_iter = block->mMemberVariables.begin();
				 var_iter != var_end; ++var_iter)
			{
				LLMessageVariable* variable = (*var_iter);
				const char* var_name = variable->getName();
				std::string var_name_string = std::string(var_name);
				text.append(llformat("    %s = ", var_name));
				std::string value("");
				S32 size = variable->getSize();
				switch(variable->getType())
				{
				case MVT_U8:
				case MVT_U16:
				case MVT_U32:
				case MVT_U64:
				case MVT_S8:
				case MVT_S16:
				case MVT_S32:
				case MVT_IP_ADDR:
				case MVT_IP_PORT:
					if("RegionHandle" == var_name_string || "Handle" == var_name_string)
						value = "$RegionHandle";
					else if("CircuitCode" == var_name_string || "ViewerCircuitCode" == var_name_string
						|| ("Code" == var_name_string && "CircuitCode" == block_name_string) )
					{
						value = "$CircuitCode";
					}
					else if(selected_objectp &&
							("ObjectLocalID" == var_name_string || "TaskLocalID" == var_name_string || ("LocalID" == var_name_string && ("ObjectData" == block_name_string || "UpdateData" == block_name_string || "InventoryData" == block_name_string))))
					{
						std::stringstream temp_stream;
						temp_stream << selected_objectp->getLocalID();
						value = temp_stream.str();
					}
					else if(agent_parcelp && "LocalID" == var_name_string
							&& ("ParcelData" == block_name_string || message.find("Parcel") != message.npos))
					{
						std::stringstream temp_stream;
						temp_stream << agent_parcelp->getLocalID();
						value = temp_stream.str();
					}
					else if("PCode" == var_name_string)
						value = "9";
					else if("PathCurve" == var_name_string)
						value = "16";
					else if("ProfileCurve" == var_name_string)
						value = "1";
					else if("PathScaleX" == var_name_string || "PathScaleY" == var_name_string)
						value = "100";
					else if("BypassRaycast" == var_name_string)
						value = "1";
					else
						value = "0";
					break;
				case MVT_F32:
				case MVT_F64:
					value = "0.0";
					break;
				case MVT_LLVector3:
				case MVT_LLVector3d:
				case MVT_LLQuaternion:
					if("Position" == var_name_string || "RayStart" == var_name_string || "RayEnd" == var_name_string)
						value = "$Position";
					else if("Scale" == var_name_string)
						value = "<0.5, 0.5, 0.5>";
					else
						value = "<0, 0, 0>";
					break;
				case MVT_LLVector4:
					value = "<0, 0, 0, 0>";
					break;
				case MVT_LLUUID:
					if("AgentID" == var_name_string)
						value = "$AgentID";
					else if("SessionID" == var_name_string)
						value = "$SessionID";
					else if("ObjectID" == var_name_string && selected_objectp)
						value = selected_objectp->getID().asString();
					else if("ParcelID" == var_name_string && agent_parcelp)
						value = agent_parcelp->getID().asString();
					else
						value = "00000000-0000-0000-0000-000000000000";
					break;
				case MVT_BOOL:
					value = "false";
					break;
				case MVT_VARIABLE:
					value = "Hello, world!";
					break;
				case MVT_FIXED:
					for(S32 si = 0; si < size; si++)
						value.append("a");
					break;
				default:
					value = "";
					break;
				}
				text.append(llformat("%s\n", value.c_str()));
			}
		}
	}
	text = text.substr(0, text.length() - 1);
	getChild<LLTextBase>("message_edit")->setText(text);
}

void LLFloaterMessageBuilder::onClickSend()
{
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("net_list");
	LLScrollListItem* selected_itemp = scrollp->getFirstSelected();

	LLHost end_point;

	//if a specific circuit is selected, send it to that, otherwise send it to the current sim
	if(selected_itemp)
	{
		LLNetListItem* itemp = findNetListItem(selected_itemp->getUUID());
		LLScrollListText* textColumn = (LLScrollListText*)selected_itemp->getColumn(1);

		//why would you send data through a dead circuit?
		if(textColumn->getValue().asString() == "Dead")
		{
			LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", "No sending messages through dead circuits!"));
			return;
		}

		end_point = itemp->mCircuitData->getHost();
	}
	else
		end_point = gAgent.getRegionHost();

	mMessageSender.sendMessage(end_point, getChild<LLTextBase>("message_edit")->getText());
}

BOOL LLFloaterMessageBuilder::handleKeyHere(KEY key, MASK mask)
{
	if(key == KEY_RETURN && (mask & MASK_CONTROL))
	{
		onClickSend();
		return TRUE;
	}
	if(key == KEY_ESCAPE)
	{
		releaseFocus();
		return TRUE;
	}
	return FALSE;
}
