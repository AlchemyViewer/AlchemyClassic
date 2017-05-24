// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file llfloatermessagelog.cpp
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
#include "llfloatermessagelog.h"

#include "llcheckboxctrl.h"
#include "llscrolllistctrl.h"
#include "lltexteditor.h"

#include "llagent.h"
#include "lleasymessagereader.h"
#include "llfloatermessagebuilder.h"
#include "llfloaterreg.h"
#include "llmessagetemplate.h"
#include "llnotificationsutil.h"
#include "llviewermenu.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"

#include <boost/tokenizer.hpp>

const std::string DEFAULT_FILTER = "!StartPingCheck !CompletePingCheck !PacketAck !SimulatorViewerTimeMessage !SimStats !AgentUpdate !AgentAnimation !AvatarAnimation !ViewerEffect !CoarseLocationUpdate !LayerData !CameraConstraint !ObjectUpdateCached !RequestMultipleObjects !ObjectUpdate !ObjectUpdateCompressed !ImprovedTerseObjectUpdate !KillObject !ImagePacket !SendXferPacket !ConfirmXferPacket !TransferPacket !SoundTrigger !AttachedSound !PreloadSound";

//TODO: replace all filtering code, esp start/stopApplyingFilter

LLMessageLogFilter::LLMessageLogFilter(const std::string& filter)
{
	set(filter);
}

void LLMessageLogFilter::set(const std::string& filter)
{
	mAsString = filter;
	mPositiveNames.clear();
	mNegativeNames.clear();
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ","");
	tokenizer tokens(filter, sep);
	tokenizer::iterator end = tokens.end();
	for(tokenizer::iterator iter = tokens.begin(); iter != end; ++iter)
	{
		std::string token = (*iter);
		LLStringUtil::trim(token);
		if(token.length())
		{
			LLStringUtil::toLower(token);

			BOOL negative = token.find('!') == 0;
			if(negative)
			{
				token = token.substr(1);
				mNegativeNames.insert(token);
			}
			else
				mPositiveNames.insert(token);
		}
	}
}

LLFloaterMessageLog::LLMessageLogNetMan::LLMessageLogNetMan(LLFloaterMessageLog* parent)
:	LLEventTimer(1.0f)
,	mParent(parent)
{
}

BOOL LLFloaterMessageLog::LLMessageLogNetMan::tick()
{
	if (mParent) mParent->updateGlobalNetList();
	return FALSE;
}

////////////////////////////////
// LLFloaterMessageLog
////////////////////////////////

std::list<LLNetListItem*> LLFloaterMessageLog::sNetListItems;
LogPayloadList LLFloaterMessageLog::sMessageLogEntries = LogPayloadList(4096);
LLMutex* LLFloaterMessageLog::sNetListMutex = nullptr;
LLMutex* LLFloaterMessageLog::sMessageListMutex = nullptr;
LLMutex* LLFloaterMessageLog::sIncompleteHTTPConvoMutex = nullptr;

LLFloaterMessageLog::LLFloaterMessageLog(const LLSD& key)
:	LLFloater(key)
,	mMessagelogScrollListCtrl(nullptr)
,	mMessageLogFilter(DEFAULT_FILTER)
,	mInfoPaneMode(IPANE_NET)
,	mBeautifyMessages(false)
,	mMessagesLogged(0)
,	mEasyMessageReader(new LLEasyMessageReader())
{
	mCommitCallbackRegistrar.add("MessageLog.Filter.Action", boost::bind(&LLFloaterMessageLog::onClickFilterMenu, this, _2));
	if(!sNetListMutex)
		sNetListMutex = new LLMutex();
	if(!sMessageListMutex)
		sMessageListMutex = new LLMutex();
	if(!sIncompleteHTTPConvoMutex)
		sIncompleteHTTPConvoMutex = new LLMutex();

	mFloaterMessageLogItems.reserve(4096);
}

LLFloaterMessageLog::~LLFloaterMessageLog()
{
	stopApplyingFilter();
	clearFloaterMessageItems(true);
	
	LLMessageLog::setCallback(nullptr);

	sNetListMutex->lock();
	sNetListItems.clear();
	sNetListMutex->unlock();
	
	clearMessageLogEntries();
	delete mEasyMessageReader;
	delete sNetListMutex;
	delete sMessageListMutex;
	delete sIncompleteHTTPConvoMutex;
	sNetListMutex = nullptr;
	sMessageListMutex = nullptr;
	sIncompleteHTTPConvoMutex = nullptr;
}

BOOL LLFloaterMessageLog::postBuild()
{
	mMessagelogScrollListCtrl = getChild<LLScrollListCtrl>("message_log");
	mMessagelogScrollListCtrl->setSortCallback(boost::bind(&LLFloaterMessageLog::sortMessageList, this, _1 ,_2, _3));

	getChild<LLUICtrl>("net_list")->setCommitCallback(boost::bind(&LLFloaterMessageLog::onCommitNetList, this, _1));
	getChild<LLUICtrl>("message_log")->setCommitCallback(boost::bind(&LLFloaterMessageLog::onCommitMessageLog, this, _1));
	getChild<LLUICtrl>("filter_apply_btn")->setCommitCallback(boost::bind(&LLFloaterMessageLog::onClickFilterApply, this));
	getChild<LLUICtrl>("filter_edit")->setCommitCallback(boost::bind(&LLFloaterMessageLog::onCommitFilter, this));
	getChild<LLUICtrl>("wrap_net_info")->setCommitCallback(boost::bind(&LLFloaterMessageLog::onCheckWrapNetInfo, this, _2));
	getChild<LLUICtrl>("beautify_messages")->setCommitCallback(boost::bind(&LLFloaterMessageLog::onCheckBeautifyMessages, this, _2));
	getChild<LLUICtrl>("clear_log_btn")->setCommitCallback(boost::bind(&LLFloaterMessageLog::onClickClearLog, this));
	getChild<LLUICtrl>("msg_builder_send_btn")->setCommitCallback(boost::bind(&LLFloaterMessageLog::onClickSendToMessageBuilder, this));
	getChild<LLLineEditor>("filter_edit")->setText(mMessageLogFilter.asString());

	startApplyingFilter(mMessageLogFilter.asString(), TRUE);

	updateGlobalNetList(true);
	mNetListTimer.reset(new LLMessageLogNetMan(this));

	setInfoPaneMode(IPANE_NET);
	wrapInfoPaneText(true);

	return TRUE;
}

void LLFloaterMessageLog::onOpen(const LLSD& key)
{
	LLMessageLog::setCallback(onLog);
}

void LLFloaterMessageLog::onClose(bool app_quiting)
{
	LLMessageLog::setCallback(nullptr);
	if (!app_quiting) onClickClearLog();
}

void LLFloaterMessageLog::clearFloaterMessageItems(bool dying)
{
	if(!dying)
	{
		childSetEnabled("msg_builder_send_btn", false);
		mMessagelogScrollListCtrl->clearRows();
	}

	sIncompleteHTTPConvoMutex->lock();
	mIncompleteHTTPConvos.clear();
	sIncompleteHTTPConvoMutex->unlock();

	mFloaterMessageLogItems.clear();
}

void LLFloaterMessageLog::clearMessageLogEntries()
{
	LLMutexLock lock(sMessageListMutex);

	sMessageLogEntries.clear();
	mMessagesLogged = 0;
}

void LLFloaterMessageLog::updateGlobalNetList(bool starting)
{
	//something tells me things aren't deallocated properly here, but
	//valgrind isn't complaining

	LLMutexLock lock(sNetListMutex);

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
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != sNetListItems.end(); ++iter)
	{
		if(std::find(circuits.begin(), circuits.end(), (*iter)->mCircuitData) == circuits.end())
			(*iter)->mCircuitData = nullptr;
	}
	// Remove net list items that are totally useless now
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != sNetListItems.end();)
	{
		if((*iter)->mCircuitData == nullptr)
		{
			delete *iter;
			iter = sNetListItems.erase(iter);
		}
		else ++iter;
	}

	if(!starting)
	{
		refreshNetList();
		refreshNetInfo(FALSE);
	}
}

LLNetListItem* LLFloaterMessageLog::findNetListItem(LLHost host)
{
	std::list<LLNetListItem*>::iterator end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		if((*iter)->mCircuitData && (*iter)->mCircuitData->getHost() == host)
			return (*iter);
	return nullptr;
}

LLNetListItem* LLFloaterMessageLog::findNetListItem(LLUUID id)
{
	std::list<LLNetListItem*>::iterator end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		if((*iter)->mID == id)
			return (*iter);
	return nullptr;
}

void LLFloaterMessageLog::refreshNetList()
{
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("net_list");

	// Update names of net list items
	for (LLNetListItem* itemp : sNetListItems)
	{
		if(itemp->mAutoName && itemp->mCircuitData)
		{
			LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(itemp->mCircuitData->getHost());
			if(regionp)
			{
				std::string name = regionp->getName();
				if(name.empty())
					name = llformat("%s (awaiting region name)", itemp->mCircuitData->getHost().getString().c_str());
				itemp->mName = name;
				itemp->mPreviousRegionName = name;
				itemp->mHandle = regionp->getHandle();
			}
			else
			{
				itemp->mName = itemp->mCircuitData->getHost().getString();
				if(!itemp->mPreviousRegionName.empty())
					itemp->mName.append(llformat(" (was %s)", itemp->mPreviousRegionName.c_str()));
			}
		}
		else
		{
			// an item just for an event queue, not handled yet
			itemp->mName = "Something else";
		}
	}
	// Rebuild scroll list from scratch
	const LLUUID& selected_id = scrollp->getFirstSelected() ? scrollp->getFirstSelected()->getUUID() : LLUUID::null;
	const S32 scroll_pos = scrollp->getScrollPos();
	scrollp->clearRows();
	for (LLNetListItem* itemp : sNetListItems)
	{
		std::string text_suffix;
		bool has_live_circuit = false;
		if (itemp->mCircuitData)
		{
			has_live_circuit = itemp->mCircuitData->isAlive();
			if (itemp->mCircuitData->getHost() == gAgent.getRegionHost())
			{
				text_suffix = " (main)";
			}
		}

		LLSD element;
		element["id"] = itemp->mID;
		LLSD& text_column = element["columns"][0];
		text_column["column"] = "text";
		text_column["value"] = itemp->mName + text_suffix;
		for(S32 i = 0; i < 2; ++i)
		{
			LLSD& icon_column = element["columns"][i + 1];
			icon_column["column"] = llformat("icon%d", i);
			icon_column["type"] = "icon";
			icon_column["value"] = "";
		}
		LLScrollListItem* scroll_itemp = scrollp->addElement(element);
		if(has_live_circuit)
		{
			LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(1);
			icon->setValue("Stop_Off");
			icon->setColor(LLColor4(1.0f,0.0f,0.0f,0.7f));
			icon->setClickCallback(onClickCloseCircuit, itemp);
		}
		else
		{
			LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(1);
			icon->setValue("Stop_Off");
			icon->setColor(LLColor4(1.0f,1.0f,1.0f,0.5f));
			icon->setClickCallback(nullptr, nullptr);
		}
		// Event queue isn't even supported yet... FIXME
		LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(2);
		icon->setValue("Stop_Off");
		icon->setColor(LLColor4(0.1f,0.1f,0.1f,0.7f));
		icon->setClickCallback(nullptr, nullptr);
	}
	if(selected_id.notNull())
		scrollp->selectByID(selected_id);
	if(scroll_pos < scrollp->getItemCount())
		scrollp->setScrollPos(scroll_pos);
}

void LLFloaterMessageLog::refreshNetInfo(BOOL force)
{
	if(mInfoPaneMode != IPANE_NET) return;
	const LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("net_list");
	const LLScrollListItem* selected_itemp = scrollp->getFirstSelected();
	if(selected_itemp)
	{
		const LLTextEditor* net_info = getChild<LLTextEditor>("net_info");
		if(!force && (net_info->hasSelection() || net_info->hasFocus())) return;
		const LLNetListItem* itemp = findNetListItem(selected_itemp->getUUID());
		if(itemp)
		{
			std::string info(llformat("%s, %d\n--------------------------------\n\n", itemp->mName.c_str(), itemp->mHandle));
			if(itemp->mCircuitData)
			{
				LLCircuitData* cdp = itemp->mCircuitData;
				info.append("Circuit\n--------------------------------\n");
				info.append(llformat(" * Host: %s\n", cdp->getHost().getString().c_str()));
				S32 seconds = (S32)cdp->getAgeInSeconds();
				S32 minutes = seconds / 60;
				seconds = seconds % 60;
				S32 hours = minutes / 60;
				minutes = minutes % 60;
				info.append(llformat(" * Age: %dh %dm %ds\n", hours, minutes, seconds));
				info.append(llformat(" * Alive: %s\n", cdp->isAlive() ? "yes" : "no"));
				info.append(llformat(" * Blocked: %s\n", cdp->isBlocked() ? "yes" : "no"));
				info.append(llformat(" * Allow timeout: %s\n", cdp->getAllowTimeout() ? "yes" : "no"));
				info.append(llformat(" * Trusted: %s\n", cdp->getTrusted() ? "yes" : "no"));
				info.append(llformat(" * Ping delay: %d\n", cdp->getPingDelay().value()));
				info.append(llformat(" * Packets out: %d\n", cdp->getPacketsOut()));
				info.append(llformat(" * Bytes out: %d\n", cdp->getBytesOut().value()));
				info.append(llformat(" * Packets in: %d\n", cdp->getPacketsIn()));
				info.append(llformat(" * Bytes in: %d\n", cdp->getBytesIn().value()));
				info.append(llformat(" * Endpoint ID: %s\n", cdp->getLocalEndPointID().asString().c_str()));
				info.append(llformat(" * Remote ID: %s\n", cdp->getRemoteID().asString().c_str()));
				info.append(llformat(" * Remote session ID: %s\n", cdp->getRemoteSessionID().asString().c_str()));
				info.append("\n");
			}

			getChild<LLTextBase>("net_info")->setText(info);
		}
		else
			getChild<LLTextBase>("net_info")->setText(LLStringUtil::null);
	}
	else
		getChild<LLTextBase>("net_info")->setText(LLStringUtil::null);
}

void LLFloaterMessageLog::setInfoPaneMode(EInfoPaneMode mode)
{
	mInfoPaneMode = mode;
	if(mode == IPANE_NET)
		refreshNetInfo(TRUE);

	//we hide the regular net_info editor and show two panes for http log mode
	getChild<LLView>("net_info")->setVisible(mode != IPANE_HTTP_LOG);
	getChild<LLView>("conv_stack")->setVisible(mode == IPANE_HTTP_LOG);
	getChild<LLView>("msg_builder_send_btn")->setEnabled(mode != IPANE_NET);
}

// static
void LLFloaterMessageLog::onLog(LogPayload& entry)
{
	LLFloaterMessageLog* floaterp = LLFloaterReg::findTypedInstance<LLFloaterMessageLog>("message_log");
	if (!floaterp)
	{
		return;
	}
	if (entry->mType != LLMessageLogEntry::HTTP_RESPONSE)
	{
		sMessageListMutex->lock();

		++floaterp->mMessagesLogged;
		sMessageLogEntries.push_back(entry);

		sMessageListMutex->unlock();
		floaterp->conditionalLog(entry);
	}
	//this is a response, try to add it to the relevant request
	else
	{
		floaterp->pairHTTPResponse(entry);
	}
}

void LLFloaterMessageLog::conditionalLog(LogPayload entry)
{
	if(!mMessageLogFilter.empty())
		getChild<LLTextBase>("log_status_text")->setText(llformat("Showing %d messages of %d",
																  mFloaterMessageLogItems.size(), mMessagesLogged));
	else
		getChild<LLTextBase>("log_status_text")->setText(llformat("Showing %d messages", mMessagesLogged));

	FloaterMessageItem item = std::make_shared<FloaterMessageItem::element_type>(entry, mEasyMessageReader);

	bool have_positive = false;

	for(const std::string& msg_name : item->mNames)
	{
		std::string find_name = msg_name;
		LLStringUtil::toLower(find_name);

		//keep the message if we allowed its name so long as one of its other names hasn't been blacklisted
		if(!have_positive && !mMessageLogFilter.mPositiveNames.empty())
		{
			if(std::find(mMessageLogFilter.mPositiveNames.begin(), mMessageLogFilter.mPositiveNames.end(), find_name) != mMessageLogFilter.mPositiveNames.end())
				have_positive = true;
		}
		if(!mMessageLogFilter.mNegativeNames.empty())
		{
			if(std::find(mMessageLogFilter.mNegativeNames.begin(), mMessageLogFilter.mNegativeNames.end(), find_name) != mMessageLogFilter.mNegativeNames.end())
			{
				return;
			}
		}
		//we don't have any negative filters and we have a positive match
		else if(have_positive)
			break;
	}

	//we had a positive filter but no positive matches
	if(!mMessageLogFilter.mPositiveNames.empty() && !have_positive)
	{
		return;
	}

	mFloaterMessageLogItems.push_back(item); // moved from beginning...

	if((*item)()->mType == LLMessageLogEntry::HTTP_REQUEST)
	{
		LLMutexLock lock(sIncompleteHTTPConvoMutex);
		mIncompleteHTTPConvos.insert(HTTPConvoMap::value_type((*item)()->mRequestID, item));
	}

	std::string net_name;
	if(item->mRegionHosts.size() > 0)
	{
		//LLHost find_host = outgoing ? item->mToHost : item->mFromHost;
		//net_name = find_host.getIPandPort();
		for (const auto& host : item->mRegionHosts)
		{
			std::string region_name = LLStringUtil::null;
			for (LLNetListItem* item : sNetListItems)
			{
				if (host == item->mCircuitData->getHost())
				{
					region_name += item->mName;
					break;
				}
			}
			if(region_name.empty())
				region_name = host.getIPandPort();
			if(!net_name.empty())
				net_name += ", ";
			net_name += region_name;
		}
	}
	else
	{
		// This is neither a region CAP nor an LLUDP message.
		net_name = "\?\?\?";
	}

	//add the message to the messagelog scroller
	LLSD element;
	element["id"] = item->mID;
	LLSD& sequence_column = element["columns"][0];
	sequence_column["column"] = "sequence";
	sequence_column["value"] = llformat("%u", item->mSequenceID);

	LLSD& type_column = element["columns"][1];
	type_column["column"] = "type";
	switch((*item)()->mType)
	{
	case LLMessageLogEntry::TEMPLATE:
		type_column["value"] = "UDP";
		break;
	case LLMessageLogEntry::HTTP_REQUEST:
		type_column["value"] = "HTTP";
		break;
	default:
		type_column["value"] = "\?\?\?";
	}

	LLSD& direction_column = element["columns"][2];
	direction_column["column"] = "direction";
	if((*item)()->mType == LLMessageLogEntry::TEMPLATE)
		direction_column["value"] = item->isOutgoing() ? "to" : "from";
	else if((*item)()->mType == LLMessageLogEntry::HTTP_REQUEST)
		direction_column["value"] = "both";

	LLSD& net_column = element["columns"][3];
	net_column["column"] = "net";
	net_column["value"] = net_name;

	LLSD& name_column = element["columns"][4];
	name_column["column"] = "name";
	name_column["value"] = item->getName();

	LLSD& summary_column = element["columns"][5];
	summary_column["column"] = "summary";
	summary_column["value"] = item->mSummary;

	S32 scroll_pos = mMessagelogScrollListCtrl->getScrollPos();
	//llinfos << "msglog scrollpos: " << scroll_pos << " // getLinesPerPage: " << scrollp->getLinesPerPage() << " // item count: " << scrollp->getItemCount() <<  llendl;

	mMessagelogScrollListCtrl->addElement(element, ADD_BOTTOM);

	if(scroll_pos > mMessagelogScrollListCtrl->getItemCount() - mMessagelogScrollListCtrl->getLinesPerPage() - 4)
		mMessagelogScrollListCtrl->setScrollPos(mMessagelogScrollListCtrl->getItemCount());
}

void LLFloaterMessageLog::pairHTTPResponse(LogPayload entry)
{
	LLMutexLock lock(sIncompleteHTTPConvoMutex);
	HTTPConvoMap::iterator iter = mIncompleteHTTPConvos.find(entry->mRequestID);

	if(iter != mIncompleteHTTPConvos.end())
	{
		iter->second->setResponseMessage(entry);

		//if this message was already selected in the message log,
		//redisplay it to show the response as well.
		const LLScrollListItem* itemp = mMessagelogScrollListCtrl->getFirstSelected();

		if(itemp && itemp->getUUID() == iter->second->mID)
		{
			showMessage(iter->second);
		}
		mIncompleteHTTPConvos.erase(iter);
	}
}

void LLFloaterMessageLog::onCommitNetList(LLUICtrl* ctrl)
{
	setInfoPaneMode(IPANE_NET);
	refreshNetInfo(TRUE);
}

void LLFloaterMessageLog::onCommitMessageLog(LLUICtrl* ctrl)
{
	showSelectedMessage();
}

void LLFloaterMessageLog::showSelectedMessage()
{
	const LLScrollListItem* selected_itemp = mMessagelogScrollListCtrl->getFirstSelected();
	if (!selected_itemp) return;
	const LLUUID& id = selected_itemp->getUUID();
	for (FloaterMessageItem entryp : mFloaterMessageLogItems)
	{
		if(entryp->mID == id)
		{
			showMessage(entryp);
			break;
		}
	}
}

void LLFloaterMessageLog::showMessage(FloaterMessageItem item)
{
	switch ((*item)()->mType)
	{
		case LLMessageLogEntry::TEMPLATE:
			setInfoPaneMode(IPANE_TEMPLATE_LOG);
			getChild<LLTextBase>("net_info")->setText(item->getFull(mBeautifyMessages));
			break;
		case LLMessageLogEntry::HTTP_REQUEST:
			setInfoPaneMode(IPANE_HTTP_LOG);
			getChild<LLTextBase>("conv_request")->setText(item->getFull(mBeautifyMessages));
			getChild<LLTextBase>("conv_response")->setText(item->getResponseFull(mBeautifyMessages));
			break;
		case LLMessageLogEntry::HTTP_RESPONSE:
		default:
			break;
	}
}

// static
BOOL LLFloaterMessageLog::onClickCloseCircuit(void* user_data)
{
	LLNetListItem* itemp = static_cast<LLNetListItem*>(user_data);
	LLCircuitData* cdp = static_cast<LLCircuitData*>(itemp->mCircuitData);
	if(!cdp) return FALSE;
	LLHost myhost = cdp->getHost();
	LLSD args;
	args["MESSAGE"] = "This will delete local circuit data.\nDo you want to tell the remote host to close the circuit too?";
	LLSD payload;
	payload["circuittoclose"] = myhost.getString();
	LLNotificationsUtil::add("GenericAlertYesCancel", args, payload, onConfirmCloseCircuit);
	return TRUE;
}
																  
// static
void LLFloaterMessageLog::onConfirmCloseCircuit(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	LLCircuitData* cdp = gMessageSystem->mCircuitInfo.findCircuit(LLHost(notification["payload"]["circuittoclose"].asString()));
	if(!cdp) return;
	LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(cdp->getHost());
	switch(option)
	{
	case 0: // yes
		gMessageSystem->newMessageFast(_PREHASH_CloseCircuit);
		gMessageSystem->sendReliable(cdp->getHost());
		break;
	case 2: // cancel
		return;
	case 1: // no
	default:
		break;
	}
	if(gMessageSystem->findCircuitCode(cdp->getHost()))
		gMessageSystem->disableCircuit(cdp->getHost());
	else
		gMessageSystem->getCircuit()->removeCircuitData(cdp->getHost());
	if(regionp)
	{
		LLHost myhost = regionp->getHost();
		LLSD args;
		args["MESSAGE"] = "That host had a region associated with it.\nDo you want to clean that up?";
		LLSD payload;
		payload["regionhost"] = myhost.getString();
		LLNotificationsUtil::add("GenericAlertYesCancel", args, payload, onConfirmRemoveRegion);
	}
}
																  
// static
void LLFloaterMessageLog::onConfirmRemoveRegion(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(option == 0) // yes
		LLWorld::getInstance()->removeRegion(LLHost(notification["payload"]["regionhost"].asString()));
}

void LLFloaterMessageLog::onClickFilterApply()
{
	startApplyingFilter(childGetValue("filter_edit"), TRUE);
}
																  
void LLFloaterMessageLog::startApplyingFilter(const std::string& filter, BOOL force)
{
	LLMessageLogFilter new_filter(filter);
	if (force
	    || (new_filter.mNegativeNames != mMessageLogFilter.mNegativeNames)
		|| (new_filter.mPositiveNames != mMessageLogFilter.mPositiveNames))
	{
		stopApplyingFilter();
		mMessageLogFilter = new_filter;

		clearFloaterMessageItems();

		sMessageListMutex->lock();
		mMessagesLogged = sMessageLogEntries.size();
		for (auto& entry : sMessageLogEntries)
			conditionalLog(entry);
		sMessageListMutex->unlock();
	}
}
																  
void LLFloaterMessageLog::stopApplyingFilter(bool quitting)
{
	if(!mMessageLogFilter.empty())
	{
		if(!quitting)
		{
			getChild<LLTextBase>("log_status_text")->setText(llformat("Showing %d messages from %d", mFloaterMessageLogItems.size(), mMessagesLogged));
		}
	}
}
																  
void LLFloaterMessageLog::updateFilterStatus()
{
	if (!mMessageLogFilter.empty()) return;

	//const S32 progress = mMessageLogFilterApply->getProgress();
	//const size_t packets = sMessageLogEntries.size();
	//const size_t matches = mFloaterMessageLogItems.size();
	//const std::string& text = llformat("Filtering ( %d / %d ), %d matches ...", progress, packets, matches);
	//getChild<LLTextBase>("log_status_text")->setText(text);
}

void LLFloaterMessageLog::onCommitFilter()
{
	startApplyingFilter(childGetValue("filter_edit"), FALSE);
}

void LLFloaterMessageLog::onClickClearLog()
{
	stopApplyingFilter();
	mMessagelogScrollListCtrl->clearRows();
	setInfoPaneMode(IPANE_NET);
	clearMessageLogEntries();
	clearFloaterMessageItems();
}

void LLFloaterMessageLog::onClickFilterMenu(const LLSD& user_data)
{
	getChild<LLLineEditor>("filter_edit")->setText(user_data.asString());
	startApplyingFilter(user_data.asString(), FALSE);
}

void LLFloaterMessageLog::onClickSendToMessageBuilder()
{
	const LLScrollListItem* selected_itemp = mMessagelogScrollListCtrl->getFirstSelected();

	if (!selected_itemp) return;

	const LLUUID& id = selected_itemp->getUUID();
	for(FloaterMessageItem entry : mFloaterMessageLogItems)
	{
		if(entry->mID == id)
		{
			const std::string& message_text = entry->getFull(getBeautifyMessages());
			LLFloaterMessageBuilder::show(message_text);
			break;
		}
	}
}

void LLFloaterMessageLog::onCheckWrapNetInfo(const LLSD& value)
{
	wrapInfoPaneText(value.asBoolean());
}

void LLFloaterMessageLog::onCheckBeautifyMessages(const LLSD& value)
{
	mBeautifyMessages = value.asBoolean();

	//if we already had a message selected, we need to set the full
	//text of the message again
	showSelectedMessage();
}

void LLFloaterMessageLog::wrapInfoPaneText(bool wrap)
{
	getChild<LLTextEditor>("net_info")->setWordWrap(wrap);
	getChild<LLTextEditor>("conv_request")->setWordWrap(wrap);
	getChild<LLTextEditor>("conv_response")->setWordWrap(wrap);
}

S32 LLFloaterMessageLog::sortMessageList(S32 col_idx, const LLScrollListItem* i1, const LLScrollListItem* i2)
{
	const LLScrollListCell *cell1 = i1->getColumn(col_idx);
	const LLScrollListCell *cell2 = i2->getColumn(col_idx);

	//do a numeric sort on the sequence num
	if(col_idx == 0)
	{
		S32 cell1_i = cell1->getValue().asInteger();
		S32 cell2_i = cell2->getValue().asInteger();

		if(cell1_i == cell2_i)
			return 0;
		if(cell1_i > cell2_i)
			return 1;

		return -1;
	}

	return LLStringUtil::compareDict(cell1->getValue().asString(), cell2->getValue().asString());
}
