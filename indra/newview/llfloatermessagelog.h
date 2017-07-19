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

#ifndef LL_LLFLOATERMESSAGELOG_H
#define LL_LLFLOATERMESSAGELOG_H

#include "llfloater.h"
#include "llmessagelog.h"
#include "lleventtimer.h"

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/circular_buffer.hpp>

struct LLNetListItem;
class LLScrollListCtrl;
class LLEasyMessageLogEntry;
class LLEasyMessageReader;
class LLEasyMessageLogEntry;
class LLFloaterMessageLog;

typedef boost::circular_buffer<LogPayload> LogPayloadList;
typedef std::shared_ptr<LLEasyMessageLogEntry> FloaterMessageItem;
typedef std::vector<FloaterMessageItem> FloaterMessageList;
typedef boost::container::flat_map<U64, FloaterMessageItem> HTTPConvoMap;

class LLMessageLogFilter
{
public:
	LLMessageLogFilter() {}
	~LLMessageLogFilter() {}
	LLMessageLogFilter(const std::string& filter);
	void set(const std::string& filter);
	bool empty() { return mPositiveNames.empty() && mNegativeNames.empty(); }

	std::string asString() {return mAsString;}

	//these should probably be unordered_sets
	boost::container::flat_set<std::string> mPositiveNames;
	boost::container::flat_set<std::string> mNegativeNames;

protected:
	std::string mAsString;
};

class LLFloaterMessageLog : public LLFloater
{
public:
	LLFloaterMessageLog(const LLSD& key);
	~LLFloaterMessageLog();
	static void onLog(LogPayload& entry);

	void onOpen(const LLSD& key) override;
	void onClose(bool app_quitting) override;

protected:
	BOOL postBuild() override;
	void updateGlobalNetList(bool starting = false);
	static LLNetListItem* findNetListItem(LLHost host);
	static LLNetListItem* findNetListItem(LLUUID id);

	void refreshNetList();
	void refreshNetInfo(BOOL force);

	//the textbox(es) in the lower half of the floater can
	//display two types of information, information about
	//the circuit, or information about the selected message.

	//depending on which mode is set, certain UI elements may
	//be enabled or disabled.
	enum EInfoPaneMode { IPANE_NET, IPANE_TEMPLATE_LOG, IPANE_HTTP_LOG };
	void setInfoPaneMode(EInfoPaneMode mode);
	void wrapInfoPaneText(bool wrap);

	void conditionalLog(LogPayload entry);
	void pairHTTPResponse(LogPayload entry);

	void showSelectedMessage();
	void showMessage(FloaterMessageItem item);
	bool getBeautifyMessages() { return mBeautifyMessages; }

	void onCommitNetList(LLUICtrl* ctrl);
	void onCommitMessageLog(LLUICtrl* ctrl);
	void onClickClearLog();
	void onCommitFilter();
	void onClickFilterMenu(const LLSD& user_data);
	void onClickFilterApply();
	void onClickSendToMessageBuilder();
	void onCheckWrapNetInfo(const LLSD& value);
	void onCheckBeautifyMessages(const LLSD& value);
	static BOOL onClickCloseCircuit(void* user_data);
	static void onConfirmCloseCircuit(const LLSD& notification, const LLSD& response);
	static void onConfirmRemoveRegion(const LLSD& notification, const LLSD& response);

	LLScrollListCtrl* mMessagelogScrollListCtrl;

public:
	void startApplyingFilter(const std::string& filter, BOOL force);

protected:
	void stopApplyingFilter(bool quitting = false);
	void updateFilterStatus();

	LLMessageLogFilter mMessageLogFilter;

	EInfoPaneMode mInfoPaneMode;

	bool mBeautifyMessages;

public:
	static std::list<LLNetListItem*> sNetListItems;

	static LogPayloadList sMessageLogEntries;
	FloaterMessageList mFloaterMessageLogItems;

	static LLMutex* sNetListMutex;
	static LLMutex* sMessageListMutex;
	static LLMutex* sIncompleteHTTPConvoMutex;

protected:
	HTTPConvoMap mIncompleteHTTPConvos;

	U32 mMessagesLogged;

	LLEasyMessageReader* mEasyMessageReader;

	S32 sortMessageList(S32,const LLScrollListItem*,const LLScrollListItem*);

	void clearMessageLogEntries();
	void clearFloaterMessageItems(bool dying = false);

	class LLMessageLogNetMan : public LLEventTimer
	{
	public:
		LLMessageLogNetMan(LLFloaterMessageLog* parent);
		~LLMessageLogNetMan() {}
		
	private:
		BOOL tick() override;
		LLFloaterMessageLog* mParent;
	};
	
	boost::scoped_ptr<LLMessageLogNetMan> mNetListTimer;
	
	//this needs to be able to look through the list of raw messages
	//to be able to create floater message items on a timer.
	friend class LLMessageLogFilterApply;
};

#endif // LL_LLFLOATERMESSAGELOG_H
