/** 
 * @file llsechandler_basic.cpp
 * @brief Security API for services such as certificate handling
 * secure local storage, etc.
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
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


#include "llviewerprecompiledheaders.h"
#include "llsecapi.h"
#include "llsechandler_basic.h"

#include "llsdserialize.h"
#include "llxorcipher.h"
#include "llfile.h"
#include "lldir.h"
#include "llviewercontrol.h"
#include "llexception.h"
#include "stringize.h"
#include "llappviewer.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>
#include <iomanip>
#include <ctime>

// compat
#define COMPAT_STORE_SALT_SIZE 16

// 256 bits of salt data...
#define STORE_SALT_SIZE 32 
#define BUFFER_READ_SIZE 256


// LLSecAPIBasicHandler Class
// Interface handler class for the various security storage handlers.

// We read the file on construction, and write it on destruction.  This
// means multiple processes cannot modify the datastore.
LLSecAPIBasicHandler::LLSecAPIBasicHandler(const std::string& protected_data_file,
										   const std::string& legacy_password_path)
{
	mProtectedDataFilename = protected_data_file;
	mProtectedDataMap = LLSD::emptyMap();
	mLegacyPasswordPath = legacy_password_path;

}

LLSecAPIBasicHandler::LLSecAPIBasicHandler()
{
}


void LLSecAPIBasicHandler::init()
{
	mProtectedDataMap = LLSD::emptyMap();
	if (mProtectedDataFilename.length() == 0)
	{
		mProtectedDataFilename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
															"bin_conf.dat");
        mLegacyPasswordPath = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "password.dat");
	}
	_readProtectedData(); // initialize mProtectedDataMap
						  // may throw LLProtectedDataException if saved datamap is not decryptable
}
LLSecAPIBasicHandler::~LLSecAPIBasicHandler()
{
	_writeProtectedData();
}

// compat_rc4 reads old rc4 encrypted files
void compat_rc4(llifstream &protected_data_stream, std::string &decrypted_data)
{
	U8 salt[COMPAT_STORE_SALT_SIZE];
	U8 buffer[BUFFER_READ_SIZE];
	U8 decrypted_buffer[BUFFER_READ_SIZE];
	int decrypted_length;
	U8 unique_id[32];
	U32 id_len = LLAppViewer::instance()->getMachineID().getUniqueID(unique_id, sizeof(unique_id));
	LLXORCipher cipher(unique_id, id_len);

	// read in the salt and key
	protected_data_stream.read((char *)salt, COMPAT_STORE_SALT_SIZE);
	if (protected_data_stream.gcount() < COMPAT_STORE_SALT_SIZE)
	{
		throw LLProtectedDataException("Config file too short.");
	}

	cipher.decrypt(salt, COMPAT_STORE_SALT_SIZE);

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	EVP_CipherInit_ex(ctx, EVP_rc4(), NULL, salt, NULL, 0); // 0 is decrypt

	while (protected_data_stream.good()) {
		// read data as a block:
		protected_data_stream.read((char *)buffer, BUFFER_READ_SIZE);

		EVP_CipherUpdate(ctx, decrypted_buffer, &decrypted_length,
			buffer, protected_data_stream.gcount());
		decrypted_data.append((const char *)decrypted_buffer, decrypted_length);
	}

	EVP_CipherFinal(ctx, decrypted_buffer, &decrypted_length);
	decrypted_data.append((const char *)decrypted_buffer, decrypted_length);

	EVP_CIPHER_CTX_free(ctx);
}

void LLSecAPIBasicHandler::_readProtectedData()
{	
	// attempt to load the file into our map
	LLPointer<LLSDParser> parser = new LLSDXMLParser();
	llifstream protected_data_stream(mProtectedDataFilename.c_str(), 
									llifstream::binary);

	if (!protected_data_stream.fail()) {
		U8 salt[STORE_SALT_SIZE];
		U8 buffer[BUFFER_READ_SIZE];
		U8 decrypted_buffer[BUFFER_READ_SIZE];
		int decrypted_length;	
		U8 unique_id[32];
        U32 id_len = LLAppViewer::instance()->getMachineID().getUniqueID(unique_id, sizeof(unique_id));
		LLXORCipher cipher(unique_id, id_len);

		// read in the salt and key
		protected_data_stream.read((char *)salt, STORE_SALT_SIZE);
		if (protected_data_stream.gcount() < STORE_SALT_SIZE)
		{
			LLTHROW(LLProtectedDataException("Config file too short."));
		}

		cipher.decrypt(salt, STORE_SALT_SIZE);		

		// totally lame.  As we're not using the OS level protected data, we need to
		// at least obfuscate the data.  We do this by using a salt stored at the head of the file
		// to encrypt the data, therefore obfuscating it from someone using simple existing tools.
		// We do include the MAC address as part of the obfuscation, which would require an
		// attacker to get the MAC address as well as the protected store, which improves things
		// somewhat.  It would be better to use the password, but as this store
		// will be used to store the SL password when the user decides to have SL remember it, 
		// so we can't use that.  OS-dependent store implementations will use the OS password/storage 
		// mechanisms and are considered to be more secure.
		// We've a strong intent to move to OS dependent protected data stores.
		

		// read in the rest of the file.
		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		EVP_CipherInit_ex(ctx, EVP_chacha20(), NULL, salt, NULL, 0); // 0 is decrypt

		std::string decrypted_data;
		while(protected_data_stream.good()) {
			// read data as a block:
			protected_data_stream.read((char *)buffer, BUFFER_READ_SIZE);
			
			EVP_CipherUpdate(ctx, decrypted_buffer, &decrypted_length,
							  buffer, protected_data_stream.gcount());
			decrypted_data.append((const char *)decrypted_buffer, decrypted_length);
		}
		
		EVP_CipherFinal(ctx, decrypted_buffer, &decrypted_length);
		decrypted_data.append((const char *)decrypted_buffer, decrypted_length);

		EVP_CIPHER_CTX_free(ctx);
		std::istringstream parse_stream(decrypted_data);
		if (parser->parse(parse_stream, mProtectedDataMap, 
						  LLSDSerialize::SIZE_UNLIMITED) == LLSDParser::PARSE_FAILURE)
		{
			// clear and reset to try compat
			parser->reset();
			decrypted_data.clear();
			protected_data_stream.clear();
			protected_data_stream.seekg(0, std::ios::beg);
			compat_rc4(protected_data_stream, decrypted_data);

			std::istringstream compat_parse_stream(decrypted_data);
			if (parser->parse(compat_parse_stream, mProtectedDataMap,
				LLSDSerialize::SIZE_UNLIMITED) == LLSDParser::PARSE_FAILURE)
			{
				// everything failed abort
				LLTHROW(LLProtectedDataException("Config file cannot be decrypted."));
			}
		}
	}
}

void LLSecAPIBasicHandler::_writeProtectedData()
{	
	std::ostringstream formatted_data_ostream;
	U8 salt[STORE_SALT_SIZE];
	U8 buffer[BUFFER_READ_SIZE];
	U8 encrypted_buffer[BUFFER_READ_SIZE];

	if(mProtectedDataMap.isUndefined())
	{
		LLFile::remove(mProtectedDataFilename);
		return;
	}

	// create a string with the formatted data.
	LLSDSerialize::toXML(mProtectedDataMap, formatted_data_ostream);
	std::istringstream formatted_data_istream(formatted_data_ostream.str());
	// generate the seed
	RAND_bytes(salt, STORE_SALT_SIZE);

	// write to a temp file so we don't clobber the initial file if there is
	// an error.
	std::string tmp_filename = mProtectedDataFilename + ".tmp";
	
	llofstream protected_data_stream(tmp_filename.c_str(), 
                                     std::ios_base::binary);
	try
	{
		
		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		EVP_CipherInit_ex(ctx, EVP_chacha20(), NULL, salt, NULL, 1); // 1 is encrypt
		U8 unique_id[32];
        U32 id_len = LLAppViewer::instance()->getMachineID().getUniqueID(unique_id, sizeof(unique_id));
		LLXORCipher cipher(unique_id, id_len);
		cipher.encrypt(salt, STORE_SALT_SIZE);
		protected_data_stream.write((const char *)salt, STORE_SALT_SIZE);

		int encrypted_length;
		while (formatted_data_istream.good())
		{
			formatted_data_istream.read((char *)buffer, BUFFER_READ_SIZE);
			if(formatted_data_istream.gcount() == 0)
			{
				break;
			}
			EVP_CipherUpdate(ctx, encrypted_buffer, &encrypted_length,
						  buffer, formatted_data_istream.gcount());
			protected_data_stream.write((const char *)encrypted_buffer, encrypted_length);
		}

		EVP_CipherFinal(ctx, encrypted_buffer, &encrypted_length);
		protected_data_stream.write((const char *)encrypted_buffer, encrypted_length);
		
		EVP_CIPHER_CTX_free(ctx);

		protected_data_stream.close();
	}
	catch (...)
	{
		LOG_UNHANDLED_EXCEPTION("LLProtectedDataException(Error writing Protected Data Store)");
		// it's good practice to clean up any secure information on error
		// (even though this file isn't really secure.  Perhaps in the future
		// it may be, however.
		LLFile::remove(tmp_filename);

		// EXP-1825 crash in LLSecAPIBasicHandler::_writeProtectedData()
		// Decided throwing an exception here was overkill until we figure out why this happens
		//LLTHROW(LLProtectedDataException("Error writing Protected Data Store"));
	}

	try
	{
		// move the temporary file to the specified file location.
		if(((	(LLFile::isfile(mProtectedDataFilename) != 0)
			 && (LLFile::remove(mProtectedDataFilename) != 0)))
		   || (LLFile::rename(tmp_filename, mProtectedDataFilename)))
		{
			LL_WARNS() << "LLProtectedDataException(Could not overwrite protected data store)" << LL_ENDL;
			LLFile::remove(tmp_filename);

			// EXP-1825 crash in LLSecAPIBasicHandler::_writeProtectedData()
			// Decided throwing an exception here was overkill until we figure out why this happens
			//LLTHROW(LLProtectedDataException("Could not overwrite protected data store"));
		}
	}
	catch (...)
	{
		LOG_UNHANDLED_EXCEPTION(STRINGIZE("renaming '" << tmp_filename << "' to '"
										  << mProtectedDataFilename << "'"));
		// it's good practice to clean up any secure information on error
		// (even though this file isn't really secure.  Perhaps in the future
		// it may be, however).
		LLFile::remove(tmp_filename);

		//crash in LLSecAPIBasicHandler::_writeProtectedData()
		// Decided throwing an exception here was overkill until we figure out why this happens
		//LLTHROW(LLProtectedDataException("Error writing Protected Data Store"));
	}
}

		
// retrieve protected data
LLSD LLSecAPIBasicHandler::getProtectedData(const std::string& data_type,
											const std::string& data_id)
{

	if (mProtectedDataMap.has(data_type) && 
		mProtectedDataMap[data_type].isMap() && 
		mProtectedDataMap[data_type].has(data_id))
	{
		return mProtectedDataMap[data_type][data_id];
	}
																				
	return LLSD();
}

void LLSecAPIBasicHandler::deleteProtectedData(const std::string& data_type,
											   const std::string& data_id)
{
	if (mProtectedDataMap.has(data_type) &&
		mProtectedDataMap[data_type].isMap() &&
		mProtectedDataMap[data_type].has(data_id))
		{
			mProtectedDataMap[data_type].erase(data_id);
		}
}


//
// persist data in a protected store
//
void LLSecAPIBasicHandler::setProtectedData(const std::string& data_type,
											const std::string& data_id,
											const LLSD& data)
{
	if (!mProtectedDataMap.has(data_type) || !mProtectedDataMap[data_type].isMap()) {
		mProtectedDataMap[data_type] = LLSD::emptyMap();
	}
	
	mProtectedDataMap[data_type][data_id] = data; 
}

//
// Create a credential object from an identifier and authenticator.  credentials are
// per grid.
LLPointer<LLCredential> LLSecAPIBasicHandler::createCredential(const std::string& grid,
															   const LLSD& identifier, 
															   const LLSD& authenticator)
{
	LLPointer<LLSecAPIBasicCredential> result = new LLSecAPIBasicCredential(grid);
	result->setCredentialData(identifier, authenticator);
	return result;
}

// Load a credential from the credential store, given the grid
LLPointer<LLCredential> LLSecAPIBasicHandler::loadCredential(const std::string& grid, const std::string& user_id)
{
	const LLSD sdCredentials = getProtectedData("credentials", grid);
	LLPointer<LLSecAPIBasicCredential> result = new LLSecAPIBasicCredential(grid);
	if (sdCredentials.isArray())
	{
		for (LLSD::array_const_iterator itCred = sdCredentials.beginArray(); itCred != sdCredentials.endArray(); ++itCred)
		{
			const LLSD& sdCredential = *itCred;
			if ( (sdCredential.isMap()) && (sdCredential.has("identifier")) )
			{
				const LLSD& sdIdentifier = sdCredential["identifier"];
				if ( (user_id.empty()) || (LLSecAPIBasicCredential::userIDFromIdentifier(sdIdentifier) == user_id) )
				{
					LLSD sdAuthenticator;
					if (sdCredential.has("authenticator"))
						sdAuthenticator = sdCredential["authenticator"];
					result->setCredentialData(sdIdentifier, sdAuthenticator);
					break;
				}
			}
		}
	}
	return result;
}

LLPointer<LLCredential> LLSecAPIBasicHandler::loadCredential(const std::string& grid, const LLSD& identifier)
{
	return loadCredential(grid, LLSecAPIBasicCredential::userIDFromIdentifier(identifier));
}

// Save the credential to the credential store.  Save the authenticator also if requested.
// That feature is used to implement the 'remember password' functionality.
void LLSecAPIBasicHandler::saveCredential(LLPointer<LLCredential> cred, bool save_authenticator)
{
	LLSD sdCredentials = getProtectedData("credentials", cred->getGrid());
	if (!sdCredentials.isArray())
	{
		sdCredentials = LLSD::emptyArray();
	}

	// Try and update the existing credential first if one exists
	bool fFound = false;
	for (LLSD::array_iterator itCred = sdCredentials.beginArray(); itCred != sdCredentials.endArray(); ++itCred)
	{
		LLSD& sdCredential = *itCred;
		if ( (sdCredential.has("identifier")) && (LLSecAPIBasicCredential::userIDFromIdentifier(sdCredential["identifier"]) == cred->userID()) )
		{
			fFound = true;
			sdCredential = cred->asLLSD(save_authenticator);
			break;
		}
	}

	// No existing stored credential found, add a new one
	if (!fFound)
	{
		sdCredentials.append(cred->asLLSD(save_authenticator));
	}

	LL_DEBUGS("SECAPI") << "Saving Credential " << cred->getGrid() << ":" << cred->userID() << " " << save_authenticator << LL_ENDL;
	setProtectedData("credentials", cred->getGrid(), sdCredentials);
	_writeProtectedData();
}

// Remove a credential from the credential store.
void LLSecAPIBasicHandler::deleteCredential(const std::string& grid, const LLSD& identifier)
{
	const std::string strUserId = LLSecAPIBasicCredential::userIDFromIdentifier(identifier);

	LLSD sdCredentials = getProtectedData("credentials", grid);
	if (sdCredentials.isArray())
	{
		for (LLSD::array_const_iterator itCred = sdCredentials.beginArray(); itCred != sdCredentials.endArray(); ++itCred)
		{
			const LLSD& sdCredential = *itCred;
			if ( (sdCredential.has("identifier")) && (LLSecAPIBasicCredential::userIDFromIdentifier(sdCredential["identifier"]) == strUserId) )
			{
				sdCredentials.erase(itCred - sdCredentials.beginArray());
				break;
			}
		}

		if (sdCredentials.size() > 0)
			setProtectedData("credentials", grid, sdCredentials);
		else
			deleteProtectedData("credentials", grid);
	}
	_writeProtectedData();
}

void LLSecAPIBasicHandler::deleteCredential(LLPointer<LLCredential> cred)
{
	deleteCredential(cred->getGrid(), cred->getIdentifier());
	cred->setCredentialData(LLSD(), LLSD());
}

bool LLSecAPIBasicHandler::getCredentialIdentifierList(const std::string& grid, std::vector<LLSD>& identifiers)
{
	identifiers.clear();

	const LLSD sdCredentials = getProtectedData("credentials", grid);
	if (sdCredentials.isArray())
	{
		for (LLSD::array_const_iterator itCred = sdCredentials.beginArray(); itCred != sdCredentials.endArray(); ++itCred)
		{
			const LLSD& sdCredential = *itCred;
			if ( (sdCredential.isMap()) && (sdCredential.has("identifier")) )
				identifiers.push_back(sdCredential["identifier"]);
		}
	}

	return !identifiers.empty();
}

// load the legacy hash for agni, and decrypt it given the 
// mac address
std::string LLSecAPIBasicHandler::_legacyLoadPassword()
{
	const S32 HASHED_LENGTH = 32;	
	std::vector<U8> buffer(HASHED_LENGTH);
	llifstream password_file(mLegacyPasswordPath.c_str(), llifstream::binary);
	
	if(password_file.fail())
	{
		return std::string("");
	}
	
	password_file.read((char*)&buffer[0], buffer.size());
	if(password_file.gcount() != (S32)buffer.size())
	{
		return std::string("");
	}
	
	// Decipher with MAC address
	U8 unique_id[32];
    U32 id_len = LLAppViewer::instance()->getMachineID().getUniqueID(unique_id, sizeof(unique_id));
	LLXORCipher cipher(unique_id, id_len);
	cipher.decrypt(&buffer[0], buffer.size());
	
	return std::string(reinterpret_cast<const char*>(&buffer[0]), buffer.size());
}

std::string LLSecAPIBasicCredential::userID() const
{
	return userIDFromIdentifier(mIdentifier);
}

std::string LLSecAPIBasicCredential::username() const
{
	return usernameFromIdentifier(mIdentifier);
}

// return a printable user identifier
std::string LLSecAPIBasicCredential::asString() const
{
	if (!mIdentifier.isMap())
	{
		return mGrid + ":(null)";
	}
	if (mIdentifier["type"].asString() == "agent")
	{
		return mGrid + ":" + mIdentifier["first_name"].asString() + " " + mIdentifier["last_name"].asString();
	}
	if (mIdentifier["type"].asString() == "account")
	{
		return mGrid + ":" + mIdentifier["account_name"].asString();
	}

	return mGrid + ":(unknown type)";
}
