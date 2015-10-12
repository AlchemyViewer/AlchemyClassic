/** 
 * @file llfloaterperms.h
 * @brief Asset creation permission preferences.
 * @author Jonathan Yap
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

#ifndef LL_LLFLOATERPERMPREFS_H
#define LL_LLFLOATERPERMPREFS_H

#include "llfloater.h"
#include "llhttpclient.h"

class LLViewerRegion;

class LLFloaterPerms : public LLFloater
{
	friend class LLFloaterReg;
	
public:
	/*virtual*/ BOOL postBuild();

	// Convenience methods to get current permission preference bitfields from saved settings:
	static U32 getEveryonePerms(const std::string& prefix=""); // prefix + "EveryoneCopy"
	static U32 getGroupPerms(const std::string& prefix=""); // prefix + "ShareWithGroup"
	static U32 getNextOwnerPerms(const std::string& prefix=""); // bitfield for prefix + "NextOwner" + "Copy", "Modify", and "Transfer"

private:
	LLFloaterPerms(const LLSD& seed);

};


class LLFloaterPermsDefault : public LLFloater
{
	friend class LLFloaterReg;
public:
	BOOL postBuild() override;
	void ok();
	void cancel();
	void onClickOK();
	void onClickCancel();
	void onCommitCopy(const LLSD& user_data);
	void updateCap();
	static void setCapsReceivedCallback(LLViewerRegion* regionp);

private:
	LLFloaterPermsDefault(const LLSD& seed);
	void reload();

	static const std::array<std::string, 6> sCategoryNames;

	// cached values only for implementing cancel.
	bool mShareWithGroup[6];
	bool mEveryoneCopy[6];
	bool mNextOwnerCopy[6];
	bool mNextOwnerModify[6];
	bool mNextOwnerTransfer[6];
};

class LLFloaterPermsRequester
{
public:
	LLFloaterPermsRequester(const std::string url, const LLSD report, int maxRetries);

	static void init(const std::string url, const LLSD report, int maxRetries);
	static void finalize();
	static LLFloaterPermsRequester* instance();

	void start();
	bool retry();

private:
	int mRetriesCount;
	int mMaxRetries;
	const std::string mUrl;
	const LLSD mReport;
public:
	static LLFloaterPermsRequester* sPermsRequester;
};

class LLFloaterPermsResponder : public LLHTTPClient::Responder
{
public:
	LLFloaterPermsResponder() : LLHTTPClient::Responder() {}
private:
	static	std::string sPreviousReason;

	void httpFailure();
	void httpSuccess();
};

#endif
