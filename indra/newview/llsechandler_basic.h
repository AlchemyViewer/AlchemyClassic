/** 
 * @file llsechandler_basic.h
 * @brief Security API for services such as certificate handling
 * secure local storage, etc.
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#ifndef LLSECHANDLER_BASIC
#define LLSECHANDLER_BASIC

#include "llsecapi.h"

// LLSecAPIBasicCredential class
class LLSecAPIBasicCredential : public LLCredential
{
public:
	LLSecAPIBasicCredential(const std::string& grid) : LLCredential(grid) {} 
	virtual ~LLSecAPIBasicCredential() = default;

	// return a value representing the user id, (could be guid, name, whatever)
	std::string userID() const override;
	std::string username() const override;
	
	// printible string identifying the credential.
	std::string asString() const override;
};

// LLSecAPIBasicHandler Class
// Interface handler class for the various security storage handlers.
class LLSecAPIBasicHandler : public LLSecAPIHandler
{
public:
	
	LLSecAPIBasicHandler(const std::string& protected_data_filename,
						 const std::string& legacy_password_path);
	LLSecAPIBasicHandler();
	
	void init() override;
	
	virtual ~LLSecAPIBasicHandler();

	
	// persist data in a protected store
	void setProtectedData(const std::string& data_type,
								  const std::string& data_id,
								  const LLSD& data) override;
	
	// retrieve protected data
	LLSD getProtectedData(const std::string& data_type,
								  const std::string& data_id) override;
	
	// delete a protected data item from the store
	void deleteProtectedData(const std::string& data_type,
									 const std::string& data_id) override;
	
	// credential management routines

	LLPointer<LLCredential> createCredential(const std::string& grid,
													 const LLSD& identifier, 
													 const LLSD& authenticator) override;

	LLPointer<LLCredential> loadCredential(const std::string& grid, const std::string& user_id) override;
	LLPointer<LLCredential> loadCredential(const std::string& grid, const LLSD& identifier) override;

	void saveCredential(LLPointer<LLCredential> cred, bool save_authenticator) override;

	void deleteCredential(const std::string& grid, const LLSD& identifier) override;
	void deleteCredential(LLPointer<LLCredential> cred) override;

	bool getCredentialIdentifierList(const std::string& grid, std::vector<LLSD>& identifiers) override;

protected:
	void _readProtectedData();
	void _writeProtectedData();
	std::string _legacyLoadPassword();

	std::string mProtectedDataFilename;
	LLSD mProtectedDataMap;
	
	std::string mLegacyPasswordPath;
};

#endif // LLSECHANDLER_BASIC



