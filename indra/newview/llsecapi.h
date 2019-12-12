/** 
 * @file llsecapi.h
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

#ifndef LLSECAPI_H
#define LLSECAPI_H

#include "llexception.h"
#include "llpointer.h"
#include "llsdutil.h"

#include <utility>
#include <vector>
#include <ostream>



#define BASIC_SECHANDLER "BASIC_SECHANDLER"

#define CRED_IDENTIFIER_TYPE_ACCOUNT "account"
#define CRED_IDENTIFIER_TYPE_AGENT "agent"
#define CRED_AUTHENTICATOR_TYPE_CLEAR "clear"
#define CRED_AUTHENTICATOR_TYPE_HASH   "hash"


struct LLProtectedDataException: public LLException
{
    LLProtectedDataException(const std::string& msg):
    LLException(msg)
    {
        LL_WARNS("SECAPI") << "Protected Data Error: " << msg << LL_ENDL;
    }
};

/**
 * LLCredential - interface for credentials providing the following functionality:
 * * Persistence of credential information based on grid (for saving username/password)
 * * Serialization to an OGP identifier/authenticator pair
 */
class LLCredential  : public LLThreadSafeRefCount
{
public:
	LLCredential() = default;
    LLCredential(std::string grid) : mGrid(std::move(grid)) {}
	
	virtual ~LLCredential() = default;
	
	virtual void setCredentialData(const LLSD& identifier, const LLSD& authenticator) 
	{ 
		mIdentifier = identifier;
		mAuthenticator = authenticator;
	}
	virtual LLSD getIdentifier() { return mIdentifier; }
	virtual void identifierType(std::string& idType);
	virtual LLSD getAuthenticator() { return mAuthenticator; }
	virtual void authenticatorType(std::string& authType);
	virtual LLSD getLoginParams();
	virtual std::string getGrid() { return mGrid; }
	
	virtual bool hasIdentifier() { return !mIdentifier.isUndefined(); }
	virtual bool hasAuthenticator() { return !mAuthenticator.isUndefined(); }

	virtual void clearAuthenticator() { mAuthenticator = LLSD(); } 
	virtual std::string userID() const { return std::string("unknown");}
	virtual std::string username() const { return std::string("unknown");}

	virtual LLSD asLLSD(bool save_authenticator);
	virtual std::string asString() const { return std::string("unknown");}
	operator std::string() const { return asString(); }
    
    static std::string userIDFromIdentifier(const LLSD& identifier);
    static std::string usernameFromIdentifier(const LLSD& identifier);
    
protected:
	LLSD mIdentifier;
	LLSD mAuthenticator;
	std::string mGrid;
};

std::ostream& operator <<(std::ostream& s, const LLCredential& cred);


// LLSecAPIHandler Class
// Interface handler class for credential and protected storage handlers.
class LLSecAPIHandler : public LLThreadSafeRefCount
{
public:
	LLSecAPIHandler() = default;
	virtual ~LLSecAPIHandler() = default;
	
	// initialize the SecAPIHandler
	virtual void init() {};
	
	// persist data in a protected store
	virtual void setProtectedData(const std::string& data_type,
								  const std::string& data_id,
								  const LLSD& data)=0;
	
	// retrieve protected data
	virtual LLSD getProtectedData(const std::string& data_type,
								  const std::string& data_id)=0;
	
	// delete a protected data item from the store
	virtual void deleteProtectedData(const std::string& data_type,
									 const std::string& data_id)=0;
	
	virtual LLPointer<LLCredential> createCredential(const std::string& grid,
													 const LLSD& identifier, 
													 const LLSD& authenticator)=0;

	virtual LLPointer<LLCredential> loadCredential(const std::string& grid, const std::string& user_id = LLStringUtil::null) = 0;
	virtual LLPointer<LLCredential> loadCredential(const std::string& grid, const LLSD& identifier) = 0;
	
	virtual void saveCredential(LLPointer<LLCredential> cred, bool save_authenticator)=0;
	
	virtual void deleteCredential(const std::string& grid, const LLSD& identifier) = 0;
	virtual void deleteCredential(LLPointer<LLCredential> cred)=0;

	virtual bool getCredentialIdentifierList(const std::string& grid, std::vector<LLSD>& identifiers) = 0;
};

void initializeSecHandler();
void cleanupSecHandler();
				
// retrieve a security api depending on the api type
LLPointer<LLSecAPIHandler> getSecHandler(const std::string& handler_type);

void registerSecHandler(const std::string& handler_type, 
						LLPointer<LLSecAPIHandler>& handler);

extern LLPointer<LLSecAPIHandler> gSecAPIHandler;

#endif // LL_SECAPI_H
