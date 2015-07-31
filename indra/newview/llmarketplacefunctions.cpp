/** 
 * @file llmarketplacefunctions.cpp
 * @brief Implementation of assorted functions related to the marketplace
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llmarketplacefunctions.h"

#include "llagent.h"
#include "llbufferstream.h"
#include "llhttpclient.h"
#include "llinventoryfunctions.h"
#include "llinventoryobserver.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "lltimer.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermedia.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"

#include "reader.h" // JSON
#include "writer.h" // JSON

//
// Helpers
//

static std::string getMarketplaceDomain()
{
	std::string domain = "secondlife.com";
	
	if (!LLGridManager::getInstance()->isInProductionGrid())
	{
		const std::string& grid_id = LLGridManager::getInstance()->getGridId();
		const std::string& grid_id_lower = utf8str_tolower(grid_id);
		
		if (grid_id_lower == "damballah")
		{
			domain = "secondlife-staging.com";
		}
		else
		{
			domain = llformat("%s.lindenlab.com", grid_id_lower.c_str());
		}
	}
	
	return domain;
}

static std::string getMarketplaceURL(const std::string& urlStringName)
{
	LLStringUtil::format_map_t domain_arg;
	domain_arg["[MARKETPLACE_DOMAIN_NAME]"] = getMarketplaceDomain();
	
	std::string marketplace_url = LLTrans::getString(urlStringName, domain_arg);
	
	return marketplace_url;
}

LLSD getMarketplaceStringSubstitutions()
{
	std::string marketplace_url = getMarketplaceURL("MarketplaceURL");
	std::string marketplace_url_create = getMarketplaceURL("MarketplaceURL_CreateStore");
	std::string marketplace_url_dashboard = getMarketplaceURL("MarketplaceURL_Dashboard");
	std::string marketplace_url_imports = getMarketplaceURL("MarketplaceURL_Imports");
	std::string marketplace_url_info = getMarketplaceURL("MarketplaceURL_LearnMore");
	
	LLSD marketplace_sub_map;

	marketplace_sub_map["[MARKETPLACE_URL]"] = marketplace_url;
	marketplace_sub_map["[MARKETPLACE_CREATE_STORE_URL]"] = marketplace_url_create;
	marketplace_sub_map["[MARKETPLACE_LEARN_MORE_URL]"] = marketplace_url_info;
	marketplace_sub_map["[MARKETPLACE_DASHBOARD_URL]"] = marketplace_url_dashboard;
	marketplace_sub_map["[MARKETPLACE_IMPORTS_URL]"] = marketplace_url_imports;
	
	return marketplace_sub_map;
}

// Get the version folder: if there is only one subfolder, we will use it as a version folder
LLUUID getVersionFolderIfUnique(const LLUUID& folder_id)
{
    LLUUID version_id = LLUUID::null;
	LLInventoryModel::cat_array_t* categories;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(folder_id, categories, items);
    if (categories->size() == 1)
    {
        version_id = categories->begin()->get()->getUUID();
    }
    else
    {
        LLNotificationsUtil::add("AlertMerchantListingActivateRequired");
    }
    return version_id;
}

///////////////////////////////////////////////////////////////////////////////
// SLM Responders
void log_SLM_warning(const std::string& request, U32 status, const std::string& reason, const std::string& code, const std::string& description)
{
    LL_WARNS("SLM") << "SLM API : Responder to " << request << ". status : " << status << ", reason : " << reason << ", code : " << code << ", description : " << description << LL_ENDL;
    if ((status == 422) && (description == "[\"You must have an English description to list the product\", \"You must choose a category for your product before it can be listed\", \"Listing could not change state.\", \"Price can't be blank\"]"))
    {
        // Unprocessable Entity : Special case that error as it is a frequent answer when trying to list an incomplete listing
        LLNotificationsUtil::add("MerchantUnprocessableEntity");
    }
    else
    {
        // Prompt the user with the warning (so they know why things are failing)
        LLSD subs;
        subs["[ERROR_REASON]"] = reason;
        // We do show long descriptions in the alert (unlikely to be readable). The description string will be in the log though.
        subs["[ERROR_DESCRIPTION]"] = (description.length() <= 512 ? description : "");
        LLNotificationsUtil::add("MerchantTransactionFailed", subs);
    }
}
void log_SLM_infos(const std::string& request, U32 status, const std::string& body)
{
    if (gSavedSettings.getBOOL("MarketplaceListingsLogging"))
    {
        LL_INFOS("SLM") << "SLM API : Responder to " << request << ". status : " << status << ", body or description : " << body << LL_ENDL;
    }
}
void log_SLM_infos(const std::string& request, const std::string& url, const std::string& body)
{
    if (gSavedSettings.getBOOL("MarketplaceListingsLogging"))
    {
		LL_INFOS("SLM") << "SLM API : Sending " << request << ". url : " << url << ", body : " << body << LL_ENDL;
    }
}

class LLSLMGetMerchantResponder : public LLHTTPClient::Responder
{
	LOG_CLASS(LLSLMGetMerchantResponder);
public:
	
    LLSLMGetMerchantResponder() {}

protected:
    virtual void httpFailure()
    {
        if (HTTP_NOT_FOUND == getStatus())
        {
            log_SLM_infos("Get /merchant", getStatus(), "User is not a merchant");
            LLMarketplaceData::instance().setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_MERCHANT);
        }
        else if (HTTP_SERVICE_UNAVAILABLE == getStatus())
        {
            log_SLM_infos("Get /merchant", getStatus(), "Merchant is not migrated");
            LLMarketplaceData::instance().setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_MIGRATED_MERCHANT);
        }
		else
		{
            log_SLM_warning("Get /merchant", getStatus(), getReason(), getContent().get("error_code"), getContent().get("error_description"));
            LLMarketplaceData::instance().setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE);
		}
    }
    
    virtual void httpSuccess()
    {
        log_SLM_infos("Get /merchant", getStatus(), "User is a merchant");
        LLMarketplaceData::instance().setSLMStatus(MarketplaceStatusCodes::MARKET_PLACE_MERCHANT);
    }
    
};

class LLSLMGetListingsResponder : public LLHTTPClient::Responder
{
	LOG_CLASS(LLSLMGetListingsResponder);
public:
	
    LLSLMGetListingsResponder(const LLUUID& folder_id)
    {
        mExpectedFolderId = folder_id;
    }
    
    virtual void completedRaw(const LLChannelDescriptors& channels,
                              const LLIOPipe::buffer_ptr_t& buffer)
    {
        LLMarketplaceData::instance().setUpdating(mExpectedFolderId,false);
        
        LLBufferStream istr(channels, buffer.get());
        std::stringstream strstrm;
        strstrm << istr.rdbuf();
        const std::string body = strstrm.str();

		if (!isGoodStatus())
		{
            log_SLM_warning("Get /listings", getStatus(), getReason(), "", body);
            LLMarketplaceData::instance().setSLMDataFetched(MarketplaceFetchCodes::MARKET_FETCH_FAILED);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
		}

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(body,root))
        {
            log_SLM_warning("Get /listings", getStatus(), "Json parsing failed", reader.getFormatedErrorMessages(), body);
            LLMarketplaceData::instance().setSLMDataFetched(MarketplaceFetchCodes::MARKET_FETCH_FAILED);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
        }
        
        log_SLM_infos("Get /listings", getStatus(), body);
        
        // Extract the info from the Json string
        Json::ValueIterator it = root["listings"].begin();
        
        while (it != root["listings"].end())
        {
            Json::Value listing = *it;
            
            int listing_id = listing["id"].asInt();
            bool is_listed = listing["is_listed"].asBool();
            std::string edit_url = listing["edit_url"].asString();
            std::string folder_uuid_string = listing["inventory_info"]["listing_folder_id"].asString();
            std::string version_uuid_string = listing["inventory_info"]["version_folder_id"].asString();
            int count = listing["inventory_info"]["count_on_hand"].asInt();
            
            LLUUID folder_id(folder_uuid_string);
            LLUUID version_id(version_uuid_string);
            if (folder_id.notNull())
            {
                LLMarketplaceData::instance().addListing(folder_id,listing_id,version_id,is_listed,edit_url,count);
            }
            it++;
        }
        
        // Update all folders under the root
        LLMarketplaceData::instance().setSLMDataFetched(MarketplaceFetchCodes::MARKET_FETCH_DONE);
        update_marketplace_category(mExpectedFolderId, false);
        gInventory.notifyObservers();        
    }
private:
    LLUUID mExpectedFolderId;
};

class LLSLMCreateListingsResponder : public LLHTTPClient::Responder
{
	LOG_CLASS(LLSLMCreateListingsResponder);
public:
	
    LLSLMCreateListingsResponder(const LLUUID& folder_id)
    {
        mExpectedFolderId = folder_id;
    }
    
    virtual void completedRaw(const LLChannelDescriptors& channels,
                              const LLIOPipe::buffer_ptr_t& buffer)
    {
        LLMarketplaceData::instance().setUpdating(mExpectedFolderId,false);
        
        LLBufferStream istr(channels, buffer.get());
        std::stringstream strstrm;
        strstrm << istr.rdbuf();
        const std::string body = strstrm.str();
        
		if (!isGoodStatus())
		{
            log_SLM_warning("Post /listings", getStatus(), getReason(), "", body);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
		}
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(body,root))
        {
            log_SLM_warning("Post /listings", getStatus(), "Json parsing failed", reader.getFormatedErrorMessages(), body);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
        }

        log_SLM_infos("Post /listings", getStatus(), body);

        // Extract the info from the Json string
        Json::ValueIterator it = root["listings"].begin();
        
        while (it != root["listings"].end())
        {
            Json::Value listing = *it;
            
            int listing_id = listing["id"].asInt();
            bool is_listed = listing["is_listed"].asBool();
            std::string edit_url = listing["edit_url"].asString();
            std::string folder_uuid_string = listing["inventory_info"]["listing_folder_id"].asString();
            std::string version_uuid_string = listing["inventory_info"]["version_folder_id"].asString();
            int count = listing["inventory_info"]["count_on_hand"].asInt();
            
            LLUUID folder_id(folder_uuid_string);
            LLUUID version_id(version_uuid_string);
            LLMarketplaceData::instance().addListing(folder_id,listing_id,version_id,is_listed,edit_url,count);
            update_marketplace_category(folder_id, false);
            gInventory.notifyObservers();
            it++;
        }
    }
private:
    LLUUID mExpectedFolderId;
};

class LLSLMGetListingResponder : public LLHTTPClient::Responder
{
	LOG_CLASS(LLSLMGetListingResponder);
public:
	
    LLSLMGetListingResponder(const LLUUID& folder_id)
    {
        mExpectedFolderId = folder_id;
    }
    
    virtual void completedRaw(const LLChannelDescriptors& channels,
                              const LLIOPipe::buffer_ptr_t& buffer)
    {
        LLMarketplaceData::instance().setUpdating(mExpectedFolderId,false);

        LLBufferStream istr(channels, buffer.get());
        std::stringstream strstrm;
        strstrm << istr.rdbuf();
        const std::string body = strstrm.str();
        
		if (!isGoodStatus())
		{
            if (getStatus() == 404)
            {
                // That listing does not exist -> delete its record from the local SLM data store
                LLMarketplaceData::instance().deleteListing(mExpectedFolderId, false);
            }
            else
            {
                log_SLM_warning("Get /listing", getStatus(), getReason(), "", body);
            }
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
		}
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(body,root))
        {
            log_SLM_warning("Get /listing", getStatus(), "Json parsing failed", reader.getFormatedErrorMessages(), body);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
        }
        
        log_SLM_infos("Get /listing", getStatus(), body);
        
        // Extract the info from the Json string
        Json::ValueIterator it = root["listings"].begin();
        
        while (it != root["listings"].end())
        {
            Json::Value listing = *it;
            
            int listing_id = listing["id"].asInt();
            bool is_listed = listing["is_listed"].asBool();
            std::string edit_url = listing["edit_url"].asString();
            std::string folder_uuid_string = listing["inventory_info"]["listing_folder_id"].asString();
            std::string version_uuid_string = listing["inventory_info"]["version_folder_id"].asString();
            int count = listing["inventory_info"]["count_on_hand"].asInt();
            
            LLUUID folder_id(folder_uuid_string);
            LLUUID version_id(version_uuid_string);
            
            // Update that listing
            LLMarketplaceData::instance().setListingID(folder_id, listing_id, false);
            LLMarketplaceData::instance().setVersionFolderID(folder_id, version_id, false);
            LLMarketplaceData::instance().setActivationState(folder_id, is_listed, false);
            LLMarketplaceData::instance().setListingURL(folder_id, edit_url, false);
            LLMarketplaceData::instance().setCountOnHand(folder_id,count,false);
            update_marketplace_category(folder_id, false);
            gInventory.notifyObservers();
            
            it++;
        }
    }
private:
    LLUUID mExpectedFolderId;
};

class LLSLMUpdateListingsResponder : public LLHTTPClient::Responder
{
	LOG_CLASS(LLSLMUpdateListingsResponder);
public:
	
    LLSLMUpdateListingsResponder(const LLUUID& folder_id, bool expected_listed_state, const LLUUID& expected_version_id)
    {
        mExpectedFolderId = folder_id;
        mExpectedListedState = expected_listed_state;
        mExpectedVersionUUID = expected_version_id;
    }
    
    virtual void completedRaw(const LLChannelDescriptors& channels,
                              const LLIOPipe::buffer_ptr_t& buffer)
    {
        LLMarketplaceData::instance().setUpdating(mExpectedFolderId,false);

        LLBufferStream istr(channels, buffer.get());
        std::stringstream strstrm;
        strstrm << istr.rdbuf();
        const std::string body = strstrm.str();
        
		if (!isGoodStatus())
		{
            log_SLM_warning("Put /listing", getStatus(), getReason(), "", body);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
		}
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(body,root))
        {
            log_SLM_warning("Put /listing", getStatus(), "Json parsing failed", reader.getFormatedErrorMessages(), body);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
        }
        
        log_SLM_infos("Put /listing", getStatus(), body);

        // Extract the info from the Json string
        Json::ValueIterator it = root["listings"].begin();
        
        while (it != root["listings"].end())
        {
            Json::Value listing = *it;
            
            int listing_id = listing["id"].asInt();
            bool is_listed = listing["is_listed"].asBool();
            std::string edit_url = listing["edit_url"].asString();
            std::string folder_uuid_string = listing["inventory_info"]["listing_folder_id"].asString();
            std::string version_uuid_string = listing["inventory_info"]["version_folder_id"].asString();
            int count = listing["inventory_info"]["count_on_hand"].asInt();
            
            LLUUID folder_id(folder_uuid_string);
            LLUUID version_id(version_uuid_string);
            
            // Update that listing
            LLMarketplaceData::instance().setListingID(folder_id, listing_id, false);
            LLMarketplaceData::instance().setVersionFolderID(folder_id, version_id, false);
            LLMarketplaceData::instance().setActivationState(folder_id, is_listed, false);
            LLMarketplaceData::instance().setListingURL(folder_id, edit_url, false);
            LLMarketplaceData::instance().setCountOnHand(folder_id,count,false);
            update_marketplace_category(folder_id, false);
            gInventory.notifyObservers();
            
            // Show a notification alert if what we got is not what we expected
            // (this actually doesn't result in an error status from the SLM API protocol)
            if ((mExpectedListedState != is_listed) || (mExpectedVersionUUID != version_id))
            {
                LLSD subs;
                subs["[URL]"] = edit_url;
                LLNotificationsUtil::add("AlertMerchantListingNotUpdated", subs);
            }
            
            it++;
        }
    }
private:
    LLUUID mExpectedFolderId;
    bool mExpectedListedState;
    LLUUID mExpectedVersionUUID;
};

class LLSLMAssociateListingsResponder : public LLHTTPClient::Responder
{
	LOG_CLASS(LLSLMAssociateListingsResponder);
public:
	
    LLSLMAssociateListingsResponder(const LLUUID& folder_id, const LLUUID& source_folder_id)
    {
        mExpectedFolderId = folder_id;
        mSourceFolderId = source_folder_id;
    }
    
    virtual void completedRaw(const LLChannelDescriptors& channels,
                              const LLIOPipe::buffer_ptr_t& buffer)
    {
        LLMarketplaceData::instance().setUpdating(mExpectedFolderId,false);
        LLMarketplaceData::instance().setUpdating(mSourceFolderId,false);
        
        LLBufferStream istr(channels, buffer.get());
        std::stringstream strstrm;
        strstrm << istr.rdbuf();
        const std::string body = strstrm.str();
        
		if (!isGoodStatus())
		{
            log_SLM_warning("Put /associate_inventory", getStatus(), getReason(), "", body);
            update_marketplace_category(mExpectedFolderId, false);
            update_marketplace_category(mSourceFolderId, false);
            gInventory.notifyObservers();
            return;
		}
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(body,root))
        {
            log_SLM_warning("Put /associate_inventory", getStatus(), "Json parsing failed", reader.getFormatedErrorMessages(), body);
            update_marketplace_category(mExpectedFolderId, false);
            update_marketplace_category(mSourceFolderId, false);
            gInventory.notifyObservers();
            return;
        }
        
        log_SLM_infos("Put /associate_inventory", getStatus(), body);
        
        // Extract the info from the Json string
        Json::ValueIterator it = root["listings"].begin();
        
        while (it != root["listings"].end())
        {
            Json::Value listing = *it;
            
            int listing_id = listing["id"].asInt();
            bool is_listed = listing["is_listed"].asBool();
            std::string edit_url = listing["edit_url"].asString();
            std::string folder_uuid_string = listing["inventory_info"]["listing_folder_id"].asString();
            std::string version_uuid_string = listing["inventory_info"]["version_folder_id"].asString();
            int count = listing["inventory_info"]["count_on_hand"].asInt();
           
            LLUUID folder_id(folder_uuid_string);
            LLUUID version_id(version_uuid_string);
            
            // Check that the listing ID is not already associated to some other record
            LLUUID old_listing = LLMarketplaceData::instance().getListingFolder(listing_id);
            if (old_listing.notNull())
            {
                // If it is already used, unlist the old record (we can't have 2 listings with the same listing ID)
                LLMarketplaceData::instance().deleteListing(old_listing);
            }
            
            // Add the new association
            LLMarketplaceData::instance().addListing(folder_id,listing_id,version_id,is_listed,edit_url,count);
            update_marketplace_category(folder_id, false);
            gInventory.notifyObservers();
            
            // The stock count needs to be updated with the new local count now
            LLMarketplaceData::instance().updateCountOnHand(folder_id,1);

            it++;
        }
        
        // Always update the source folder so its widget updates
        update_marketplace_category(mSourceFolderId, false);
    }
private:
    LLUUID mExpectedFolderId;   // This is the folder now associated with the id.
    LLUUID mSourceFolderId;     // This is the folder initially associated with the id. Can be LLUUI::null
};

class LLSLMDeleteListingsResponder : public LLHTTPClient::Responder
{
	LOG_CLASS(LLSLMDeleteListingsResponder);
public:
	
    LLSLMDeleteListingsResponder(const LLUUID& folder_id)
    {
        mExpectedFolderId = folder_id;
    }
    
    virtual void completedRaw(const LLChannelDescriptors& channels,
                              const LLIOPipe::buffer_ptr_t& buffer)
    {
        LLMarketplaceData::instance().setUpdating(mExpectedFolderId,false);
        
        LLBufferStream istr(channels, buffer.get());
        std::stringstream strstrm;
        strstrm << istr.rdbuf();
        const std::string body = strstrm.str();
        
		if (!isGoodStatus())
		{
            log_SLM_warning("Delete /listing", getStatus(), getReason(), "", body);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
		}
        
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(body,root))
        {
            log_SLM_warning("Delete /listing", getStatus(), "Json parsing failed", reader.getFormatedErrorMessages(), body);
            update_marketplace_category(mExpectedFolderId, false);
            gInventory.notifyObservers();
            return;
        }
        
        log_SLM_infos("Delete /listing", getStatus(), body);
        
        // Extract the info from the Json string
        Json::ValueIterator it = root["listings"].begin();
        
        while (it != root["listings"].end())
        {
            Json::Value listing = *it;
            
            int listing_id = listing["id"].asInt();
            LLUUID folder_id = LLMarketplaceData::instance().getListingFolder(listing_id);
            LLMarketplaceData::instance().deleteListing(folder_id);
            
            it++;
        }
    }
private:
    LLUUID mExpectedFolderId;
};

// SLM Responders End
///////////////////////////////////////////////////////////////////////////////

namespace LLMarketplaceImport
{
	// Basic interface for this namespace

	bool hasSessionCookie();
	bool inProgress();
	bool resultPending();
	S32 getResultStatus();
	const LLSD& getResults();

	bool establishMarketplaceSessionCookie();
	bool pollStatus();
	bool triggerImport();
	
	// Internal state variables

	static std::string sMarketplaceCookie = "";
	static LLSD sImportId = LLSD::emptyMap();
	static bool sImportInProgress = false;
	static bool sImportPostPending = false;
	static bool sImportGetPending = false;
	static S32 sImportResultStatus = 0;
	static LLSD sImportResults = LLSD::emptyMap();

	static LLTimer slmGetTimer;
	static LLTimer slmPostTimer;

	// Responders
	
	class LLImportPostResponder : public LLHTTPClient::Responder
	{
		LOG_CLASS(LLImportPostResponder);
	public:
		LLImportPostResponder() : LLCurl::Responder() {}

	protected:
		/* virtual */ void httpCompleted()
		{
			slmPostTimer.stop();

			if (gSavedSettings.getBOOL("InventoryOutboxLogging"))
			{
				LL_INFOS() << " SLM [timer:" << slmPostTimer.getElapsedTimeF32() << "] "
						   << dumpResponse() << LL_ENDL;
			}

			S32 status = getStatus();
			if ((status == MarketplaceErrorCodes::IMPORT_REDIRECT) ||
				(status == MarketplaceErrorCodes::IMPORT_AUTHENTICATION_ERROR) ||
				// MAINT-2301 : we determined we can safely ignore that error in that context
				(status == MarketplaceErrorCodes::IMPORT_JOB_TIMEOUT))
			{
				if (gSavedSettings.getBOOL("InventoryOutboxLogging"))
				{
					LL_INFOS() << " SLM POST : Ignoring time out status and treating it as success" << LL_ENDL;
				}
				status = MarketplaceErrorCodes::IMPORT_DONE;
			}
			
			if (status >= MarketplaceErrorCodes::IMPORT_BAD_REQUEST)
			{
				if (gSavedSettings.getBOOL("InventoryOutboxLogging"))
				{
					LL_INFOS() << " SLM POST : Clearing marketplace cookie due to client or server error" << LL_ENDL;
				}
				sMarketplaceCookie.clear();
			}

			sImportInProgress = (status == MarketplaceErrorCodes::IMPORT_DONE);
			sImportPostPending = false;
			sImportResultStatus = status;
			sImportId = getContent();
		}
	};
	
	class LLImportGetResponder : public LLHTTPClient::Responder
	{
		LOG_CLASS(LLImportGetResponder);
	public:
		LLImportGetResponder() : LLCurl::Responder() {}
		
	protected:
		/* virtual */ void httpCompleted()
		{
			const std::string& set_cookie_string = getResponseHeader(HTTP_IN_HEADER_SET_COOKIE);
			
			if (!set_cookie_string.empty())
			{
				sMarketplaceCookie = set_cookie_string;
			}

			slmGetTimer.stop();

			if (gSavedSettings.getBOOL("InventoryOutboxLogging"))
			{
				LL_INFOS() << " SLM [timer:" << slmGetTimer.getElapsedTimeF32() << "] "
						   << dumpResponse() << LL_ENDL;
			}
			
            // MAINT-2452 : Do not clear the cookie on IMPORT_DONE_WITH_ERRORS : Happens when trying to import objects with wrong permissions
            // ACME-1221 : Do not clear the cookie on IMPORT_NOT_FOUND : Happens for newly created Merchant accounts that are initally empty
			S32 status = getStatus();
			if ((status >= MarketplaceErrorCodes::IMPORT_BAD_REQUEST) &&
                (status != MarketplaceErrorCodes::IMPORT_DONE_WITH_ERRORS) &&
                (status != MarketplaceErrorCodes::IMPORT_NOT_FOUND))
			{
				if (gSavedSettings.getBOOL("InventoryOutboxLogging"))
				{
					LL_INFOS() << " SLM GET clearing marketplace cookie due to client or server error" << LL_ENDL;
				}
				sMarketplaceCookie.clear();
			}
            else if (gSavedSettings.getBOOL("InventoryOutboxLogging") && (status >= MarketplaceErrorCodes::IMPORT_BAD_REQUEST))
            {
                LL_INFOS() << " SLM GET : Got error status = " << status << ", but marketplace cookie not cleared." << LL_ENDL;
            }

			sImportInProgress = (status == MarketplaceErrorCodes::IMPORT_PROCESSING);
			sImportGetPending = false;
			sImportResultStatus = status;
			sImportResults = getContent();
		}
	};

	// Basic API

	bool hasSessionCookie()
	{
		return !sMarketplaceCookie.empty();
	}
	
	bool inProgress()
	{
		return sImportInProgress;
	}
	
	bool resultPending()
	{
		return (sImportPostPending || sImportGetPending);
	}
	
	S32 getResultStatus()
	{
        return sImportResultStatus;
	}
	
	const LLSD& getResults()
	{
		return sImportResults;
	}
	
	static std::string getInventoryImportURL()
	{
		std::string url = getMarketplaceURL("MarketplaceURL");
		
		url += "api/1/";
		url += gAgent.getID().getString();
		url += "/inventory/import/";
		
		return url;
	}
	
	bool establishMarketplaceSessionCookie()
	{
		if (hasSessionCookie())
		{
			return false;
		}

		sImportInProgress = true;
		sImportGetPending = true;
		
		std::string url = getInventoryImportURL();
		
		if (gSavedSettings.getBOOL("InventoryOutboxLogging"))
		{
            LL_INFOS() << " SLM GET: establishMarketplaceSessionCookie, LLHTTPClient::get, url = " << url << LL_ENDL;
            LLSD headers = LLViewerMedia::getHeaders();
            std::stringstream str;
            LLSDSerialize::toPrettyXML(headers, str);
            LL_INFOS() << " SLM GET: headers " << LL_ENDL;
            LL_INFOS() << str.str() << LL_ENDL;
		}

		slmGetTimer.start();
		LLHTTPClient::get(url, new LLImportGetResponder(), LLViewerMedia::getHeaders());
		
		return true;
	}
	
	bool pollStatus()
	{
		if (!hasSessionCookie())
		{
			return false;
		}
		
		sImportGetPending = true;

		std::string url = getInventoryImportURL();

		url += sImportId.asString();

		// Make the headers for the post
		LLSD headers = LLSD::emptyMap();
		headers[HTTP_OUT_HEADER_ACCEPT] = "*/*";
		headers[HTTP_OUT_HEADER_COOKIE] = sMarketplaceCookie;
		// *TODO: Why are we setting Content-Type for a GET request?
		headers[HTTP_OUT_HEADER_CONTENT_TYPE] = HTTP_CONTENT_LLSD_XML;
		headers[HTTP_OUT_HEADER_USER_AGENT] = LLViewerMedia::getCurrentUserAgent();
		
		if (gSavedSettings.getBOOL("InventoryOutboxLogging"))
		{
            LL_INFOS() << " SLM GET: pollStatus, LLHTTPClient::get, url = " << url << LL_ENDL;
            std::stringstream str;
            LLSDSerialize::toPrettyXML(headers, str);
            LL_INFOS() << " SLM GET: headers " << LL_ENDL;
            LL_INFOS() << str.str() << LL_ENDL;
		}

		slmGetTimer.start();
		LLHTTPClient::get(url, new LLImportGetResponder(), headers);
		
		return true;
	}
	
	bool triggerImport()
	{
		if (!hasSessionCookie())
		{
			return false;
		}

		sImportId = LLSD::emptyMap();
		sImportInProgress = true;
		sImportPostPending = true;
		sImportResultStatus = MarketplaceErrorCodes::IMPORT_PROCESSING;
		sImportResults = LLSD::emptyMap();

		std::string url = getInventoryImportURL();
		
		// Make the headers for the post
		LLSD headers = LLSD::emptyMap();
		headers[HTTP_OUT_HEADER_ACCEPT] = "*/*";
		headers[HTTP_OUT_HEADER_CONNECTION] = "Keep-Alive";
		headers[HTTP_OUT_HEADER_COOKIE] = sMarketplaceCookie;
		headers[HTTP_OUT_HEADER_CONTENT_TYPE] = HTTP_CONTENT_XML;
		headers[HTTP_OUT_HEADER_USER_AGENT] = LLViewerMedia::getCurrentUserAgent();
		
		if (gSavedSettings.getBOOL("InventoryOutboxLogging"))
		{
            LL_INFOS() << " SLM POST: triggerImport, LLHTTPClient::post, url = " << url << LL_ENDL;
            std::stringstream str;
            LLSDSerialize::toPrettyXML(headers, str);
            LL_INFOS() << " SLM POST: headers " << LL_ENDL;
            LL_INFOS() << str.str() << LL_ENDL;
		}

		slmPostTimer.start();
        LLHTTPClient::post(url, LLSD(), new LLImportPostResponder(), headers);
		
		return true;
	}
}


//
// Interface class
//

static const F32 MARKET_IMPORTER_UPDATE_FREQUENCY = 1.0f;

//static
void LLMarketplaceInventoryImporter::update()
{
	if (instanceExists())
	{
		static LLTimer update_timer;
		if (update_timer.hasExpired())
		{
			LLMarketplaceInventoryImporter::instance().updateImport();
			update_timer.setTimerExpirySec(MARKET_IMPORTER_UPDATE_FREQUENCY);
		}
	}
}

LLMarketplaceInventoryImporter::LLMarketplaceInventoryImporter()
	: mAutoTriggerImport(false)
	, mImportInProgress(false)
	, mInitialized(false)
	, mMarketPlaceStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED)
	, mErrorInitSignal(NULL)
	, mStatusChangedSignal(NULL)
	, mStatusReportSignal(NULL)
{
}

boost::signals2::connection LLMarketplaceInventoryImporter::setInitializationErrorCallback(const status_report_signal_t::slot_type& cb)
{
	if (mErrorInitSignal == NULL)
	{
		mErrorInitSignal = new status_report_signal_t();
	}
	
	return mErrorInitSignal->connect(cb);
}

boost::signals2::connection LLMarketplaceInventoryImporter::setStatusChangedCallback(const status_changed_signal_t::slot_type& cb)
{
	if (mStatusChangedSignal == NULL)
	{
		mStatusChangedSignal = new status_changed_signal_t();
	}

	return mStatusChangedSignal->connect(cb);
}

boost::signals2::connection LLMarketplaceInventoryImporter::setStatusReportCallback(const status_report_signal_t::slot_type& cb)
{
	if (mStatusReportSignal == NULL)
	{
		mStatusReportSignal = new status_report_signal_t();
	}

	return mStatusReportSignal->connect(cb);
}

void LLMarketplaceInventoryImporter::initialize()
{
    if (mInitialized)
    {
        return;
    }

    if (!LLMarketplaceImport::hasSessionCookie())
    {
        mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_INITIALIZING;
        LLMarketplaceImport::establishMarketplaceSessionCookie();
    }
    else
    {
        mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_MERCHANT;
    }
}

void LLMarketplaceInventoryImporter::reinitializeAndTriggerImport()
{
	mInitialized = false;
	mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED;
	initialize();
	mAutoTriggerImport = true;
}

bool LLMarketplaceInventoryImporter::triggerImport()
{
	const bool import_triggered = LLMarketplaceImport::triggerImport();
	
	if (!import_triggered)
	{
		reinitializeAndTriggerImport();
	}
	
	return import_triggered;
}

void LLMarketplaceInventoryImporter::updateImport()
{
	const bool in_progress = LLMarketplaceImport::inProgress();
	
	if (in_progress && !LLMarketplaceImport::resultPending())
	{
		const bool polling_status = LLMarketplaceImport::pollStatus();
		
		if (!polling_status)
		{
			reinitializeAndTriggerImport();
		}
	}	
	
	if (mImportInProgress != in_progress)
	{
		mImportInProgress = in_progress;

		// If we are no longer in progress
		if (!mImportInProgress)
		{
            // Look for results success
            mInitialized = LLMarketplaceImport::hasSessionCookie();

            // Report results
            if (mStatusReportSignal)
            {
                (*mStatusReportSignal)(LLMarketplaceImport::getResultStatus(), LLMarketplaceImport::getResults());
            }
            
            if (mInitialized)
            {
                mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_MERCHANT;
                // Follow up with auto trigger of import
                if (mAutoTriggerImport)
                {
                    mAutoTriggerImport = false;
                    mImportInProgress = triggerImport();
                }
            }
            else
            {
                U32 status = LLMarketplaceImport::getResultStatus();
                if ((status == MarketplaceErrorCodes::IMPORT_FORBIDDEN) ||
                    (status == MarketplaceErrorCodes::IMPORT_AUTHENTICATION_ERROR))
                {
                    mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_NOT_MERCHANT;
                }
                else if (status == MarketplaceErrorCodes::IMPORT_SERVER_API_DISABLED)
                {
                    mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_MIGRATED_MERCHANT;
                }
                else
                {
                    mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE;
                }
                if (mErrorInitSignal && (mMarketPlaceStatus == MarketplaceStatusCodes::MARKET_PLACE_CONNECTION_FAILURE))
                {
                    (*mErrorInitSignal)(LLMarketplaceImport::getResultStatus(), LLMarketplaceImport::getResults());
                }
            }
		}
	}
    
    // Make sure we trigger the status change with the final state (in case of auto trigger after initialize)
    if (mStatusChangedSignal)
    {
        (*mStatusChangedSignal)(mImportInProgress);
    }
}

//
// Direct Delivery : Marketplace tuples and data
//
class LLMarketplaceInventoryObserver : public LLInventoryObserver
{
public:
	LLMarketplaceInventoryObserver() {}
	virtual ~LLMarketplaceInventoryObserver() {}
	virtual void changed(U32 mask);
};

void LLMarketplaceInventoryObserver::changed(U32 mask)
{
    // When things are added to the marketplace, we might need to re-validate and fix the containing listings
	if (mask & LLInventoryObserver::ADD)
	{
        const std::set<LLUUID>& changed_items = gInventory.getChangedIDs();
        
        std::set<LLUUID>::const_iterator id_it = changed_items.begin();
        std::set<LLUUID>::const_iterator id_end = changed_items.end();
        // First, count the number of items in this list...
        S32 count = 0;
        for (;id_it != id_end; ++id_it)
        {
            LLInventoryObject* obj = gInventory.getObject(*id_it);
            if (obj && (LLAssetType::AT_CATEGORY != obj->getType()))
            {
                count++;
            }
        }
        // Then, decrement the folders of that amount
        // Note that of all of those, only one folder will be a listing folder (if at all).
        // The other will be ignored by the decrement method.
        id_it = changed_items.begin();
        for (;id_it != id_end; ++id_it)
        {
            LLInventoryObject* obj = gInventory.getObject(*id_it);
            if (obj && (LLAssetType::AT_CATEGORY == obj->getType()))
            {
                LLMarketplaceData::instance().decrementValidationWaiting(obj->getUUID(),count);
            }
        }
	}
    
    // When things are changed in the inventory, this can trigger a host of changes in the marketplace listings folder:
    // * stock counts changing : no copy items coming in and out will change the stock count on folders
    // * version and listing folders : moving those might invalidate the marketplace data itself
    // Since we should cannot raise inventory change while the observer is called (the list will be cleared
    // once observers are called) we need to raise a flag in the inventory to signal that things have been dirtied.
    
	if (mask & (LLInventoryObserver::INTERNAL | LLInventoryObserver::STRUCTURE))
	{
        const std::set<LLUUID>& changed_items = gInventory.getChangedIDs();
    
        std::set<LLUUID>::const_iterator id_it = changed_items.begin();
        std::set<LLUUID>::const_iterator id_end = changed_items.end();
        for (;id_it != id_end; ++id_it)
        {
            LLInventoryObject* obj = gInventory.getObject(*id_it);
            if (obj)
            {
                if (LLAssetType::AT_CATEGORY == obj->getType())
                {
                    // If it's a folder known to the marketplace, let's check it's in proper shape
                    if (LLMarketplaceData::instance().isListed(*id_it) || LLMarketplaceData::instance().isVersionFolder(*id_it))
                    {
                        LLInventoryCategory* cat = (LLInventoryCategory*)(obj);
                        validate_marketplacelistings(cat);
                    }
                }
                else
                {
                    // If it's not a category, it's an item...
                    LLInventoryItem* item = (LLInventoryItem*)(obj);
                    // If it's a no copy item, we may need to update the label count of marketplace listings
                    if (!item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID(), gAgent.getGroupID()))
                    {
                        LLMarketplaceData::instance().setDirtyCount();
                    }
                }
            }
        }
	}
}

// Tuple == Item
LLMarketplaceTuple::LLMarketplaceTuple() :
    mListingFolderId(),
    mListingId(0),
    mVersionFolderId(),
    mIsActive(false),
    mEditURL("")
{
}

LLMarketplaceTuple::LLMarketplaceTuple(const LLUUID& folder_id) :
    mListingFolderId(folder_id),
    mListingId(0),
    mVersionFolderId(),
    mIsActive(false),
    mEditURL("")
{
}

LLMarketplaceTuple::LLMarketplaceTuple(const LLUUID& folder_id, S32 listing_id, const LLUUID& version_id, bool is_listed) :
    mListingFolderId(folder_id),
    mListingId(listing_id),
    mVersionFolderId(version_id),
    mIsActive(is_listed),
    mEditURL("")
{
}


// Data map
LLMarketplaceData::LLMarketplaceData() : 
 mMarketPlaceStatus(MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED),
 mMarketPlaceDataFetched(MarketplaceFetchCodes::MARKET_FETCH_NOT_DONE),
 mStatusUpdatedSignal(NULL),
 mDataFetchedSignal(NULL),
 mDirtyCount(false)
{
    mInventoryObserver = new LLMarketplaceInventoryObserver;
    gInventory.addObserver(mInventoryObserver);
}

LLMarketplaceData::~LLMarketplaceData()
{
	gInventory.removeObserver(mInventoryObserver);
}

void LLMarketplaceData::initializeSLM(const status_updated_signal_t::slot_type& cb)
{
	if (mStatusUpdatedSignal == NULL)
	{
		mStatusUpdatedSignal = new status_updated_signal_t();
	}
	mStatusUpdatedSignal->connect(cb);
    
    if (mMarketPlaceStatus != MarketplaceStatusCodes::MARKET_PLACE_NOT_INITIALIZED)
    {
        // If already initialized, just confirm the status so the callback gets called
        setSLMStatus(mMarketPlaceStatus);
    }
    else
    {
        // Initiate SLM connection and set responder
        std::string url = getSLMConnectURL("/merchant");
        if (url != "")
        {
            mMarketPlaceStatus = MarketplaceStatusCodes::MARKET_PLACE_INITIALIZING;
            log_SLM_infos("LLHTTPClient::get", url, "");
            LLHTTPClient::get(url, new LLSLMGetMerchantResponder(), LLSD());
        }
    }
}

void LLMarketplaceData::setDataFetchedSignal(const status_updated_signal_t::slot_type& cb)
{
	if (mDataFetchedSignal == NULL)
	{
		mDataFetchedSignal = new status_updated_signal_t();
	}
	mDataFetchedSignal->connect(cb);
}

// Get/Post/Put requests to the SLM Server using the SLM API
void LLMarketplaceData::getSLMListings()
{
	LLSD headers = LLSD::emptyMap();
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";
    
	// Send request
    std::string url = getSLMConnectURL("/listings");
    log_SLM_infos("LLHTTPClient::get", url, "");
	const LLUUID marketplacelistings_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS, false);
    setUpdating(marketplacelistings_id,true);
	LLHTTPClient::get(url, new LLSLMGetListingsResponder(marketplacelistings_id), headers);
}

void LLMarketplaceData::getSLMListing(S32 listing_id)
{
	LLSD headers = LLSD::emptyMap();
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";
    
	// Send request
    std::string url = getSLMConnectURL("/listing/") + llformat("%d",listing_id);
    log_SLM_infos("LLHTTPClient::get", url, "");
    LLUUID folder_id = LLMarketplaceData::instance().getListingFolder(listing_id);
    setUpdating(folder_id,true);
	LLHTTPClient::get(url, new LLSLMGetListingResponder(folder_id), headers);
}

void LLMarketplaceData::createSLMListing(const LLUUID& folder_id, const LLUUID& version_id, S32 count)
{
	LLSD headers = LLSD::emptyMap();
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";
    
    // Build the json message
    Json::Value root;
    Json::FastWriter writer;
    
    LLViewerInventoryCategory* category = gInventory.getCategory(folder_id);
    root["listing"]["name"] = category->getName();
    root["listing"]["inventory_info"]["listing_folder_id"] = folder_id.asString();
    root["listing"]["inventory_info"]["version_folder_id"] = version_id.asString();
    root["listing"]["inventory_info"]["count_on_hand"] = count;
    
    std::string json_str = writer.write(root);
    
	// postRaw() takes ownership of the buffer and releases it later.
	size_t size = json_str.size();
	U8 *data = new U8[size];
	memcpy(data, (U8*)(json_str.c_str()), size);
    
	// Send request
    std::string url = getSLMConnectURL("/listings");
    log_SLM_infos("LLHTTPClient::postRaw", url, json_str);
    setUpdating(folder_id,true);
	LLHTTPClient::postRaw(url, data, size, new LLSLMCreateListingsResponder(folder_id), headers);
}

void LLMarketplaceData::updateSLMListing(const LLUUID& folder_id, S32 listing_id, const LLUUID& version_id, bool is_listed, S32 count)
{
	LLSD headers = LLSD::emptyMap();
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";

    Json::Value root;
    Json::FastWriter writer;

    // Note : auto unlist if the count is 0 (out of stock)
    if (is_listed && (count == 0))
    {
        is_listed = false;
        LLNotificationsUtil::add("AlertMerchantStockFolderEmpty");
    }
    
    // Note : we're assuming that sending unchanged info won't break anything server side...
    root["listing"]["id"] = listing_id;
    root["listing"]["is_listed"] = is_listed;
    root["listing"]["inventory_info"]["listing_folder_id"] = folder_id.asString();
    root["listing"]["inventory_info"]["version_folder_id"] = version_id.asString();
    root["listing"]["inventory_info"]["count_on_hand"] = count;
    
    std::string json_str = writer.write(root);
    
	// postRaw() takes ownership of the buffer and releases it later.
	size_t size = json_str.size();
	U8 *data = new U8[size];
	memcpy(data, (U8*)(json_str.c_str()), size);
    
	// Send request
    std::string url = getSLMConnectURL("/listing/") + llformat("%d",listing_id);
    log_SLM_infos("LLHTTPClient::putRaw", url, json_str);
    setUpdating(folder_id,true);
	LLHTTPClient::putRaw(url, data, size, new LLSLMUpdateListingsResponder(folder_id, is_listed, version_id), headers);
}

void LLMarketplaceData::associateSLMListing(const LLUUID& folder_id, S32 listing_id, const LLUUID& version_id, const LLUUID& source_folder_id)
{
	LLSD headers = LLSD::emptyMap();
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";
    
    Json::Value root;
    Json::FastWriter writer;
    
    // Note : we're assuming that sending unchanged info won't break anything server side...
    root["listing"]["id"] = listing_id;
    root["listing"]["inventory_info"]["listing_folder_id"] = folder_id.asString();
    root["listing"]["inventory_info"]["version_folder_id"] = version_id.asString();
    
    std::string json_str = writer.write(root);
    
	// postRaw() takes ownership of the buffer and releases it later.
	size_t size = json_str.size();
	U8 *data = new U8[size];
	memcpy(data, (U8*)(json_str.c_str()), size);
    
	// Send request
    std::string url = getSLMConnectURL("/associate_inventory/") + llformat("%d",listing_id);
    log_SLM_infos("LLHTTPClient::putRaw", url, json_str);
    setUpdating(folder_id,true);
    setUpdating(source_folder_id,true);
	LLHTTPClient::putRaw(url, data, size, new LLSLMAssociateListingsResponder(folder_id,source_folder_id), headers);
}

void LLMarketplaceData::deleteSLMListing(S32 listing_id)
{
	LLSD headers = LLSD::emptyMap();
	headers["Accept"] = "application/json";
	headers["Content-Type"] = "application/json";
        
	// Send request
    std::string url = getSLMConnectURL("/listing/") + llformat("%d",listing_id);
    log_SLM_infos("LLHTTPClient::del", url, "");
    LLUUID folder_id = LLMarketplaceData::instance().getListingFolder(listing_id);
    setUpdating(folder_id,true);
	LLHTTPClient::del(url, new LLSLMDeleteListingsResponder(folder_id), headers);
}

std::string LLMarketplaceData::getSLMConnectURL(const std::string& route)
{
    std::string url("");
    LLViewerRegion *regionp = gAgent.getRegion();
    if (regionp)
    {
        // Get DirectDelivery cap
        url = regionp->getCapability("DirectDelivery");
        if (url != "")
        {
            url += route;
        }
    }
	return url;
}

void LLMarketplaceData::setSLMStatus(U32 status)
{
    mMarketPlaceStatus = status;
    if (mStatusUpdatedSignal)
    {
        (*mStatusUpdatedSignal)();
    }
}

void LLMarketplaceData::setSLMDataFetched(U32 status)
{
    mMarketPlaceDataFetched = status;
    if (mDataFetchedSignal)
    {
        (*mDataFetchedSignal)();
    }
}

// Creation / Deletion / Update
// Methods publicly called
bool LLMarketplaceData::createListing(const LLUUID& folder_id)
{
    if (isListed(folder_id))
    {
        // Listing already exists -> exit with error
        return false;
    }
    
    // Get the version folder: if there is only one subfolder, we will set it as a version folder immediately
    S32 count = -1;
    LLUUID version_id = getVersionFolderIfUnique(folder_id);
    if (version_id.notNull())
    {
        count = compute_stock_count(version_id, true);
    }
    
    // Validate the count on hand
    if (count == COMPUTE_STOCK_NOT_EVALUATED)
    {
        // If the count on hand cannot be evaluated, we will consider it empty (out of stock) at creation time
        // It will get reevaluated and updated once the items are fetched
        count = 0;
    }

    // Post the listing creation request to SLM
    createSLMListing(folder_id, version_id, count);
    
    return true;
}

bool LLMarketplaceData::clearListing(const LLUUID& folder_id, S32 depth)
{
    if (folder_id.isNull())
    {
        // Folder doesn't exists -> exit with error
        return false;
    }

    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(folder_id);
        
    }
    // Folder id can be the root of the listing or not so we need to retrieve the root first
    LLUUID listing_uuid = (isListed(folder_id) ? folder_id : nested_parent_id(folder_id, depth));
    S32 listing_id = getListingID(listing_uuid);
   
    if (listing_id == 0)
    {
        // Listing doesn't exists -> exit with error
        return false;
    }
    
    // Update the SLM Server so that this listing is deleted (actually, archived...)
    deleteSLMListing(listing_id);
    
    return true;
}

bool LLMarketplaceData::getListing(const LLUUID& folder_id, S32 depth)
{
    if (folder_id.isNull())
    {
        // Folder doesn't exists -> exit with error
        return false;
    }
    
    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(folder_id);
        
    }
    // Folder id can be the root of the listing or not so we need to retrieve the root first
    LLUUID listing_uuid = (isListed(folder_id) ? folder_id : nested_parent_id(folder_id, depth));
    S32 listing_id = getListingID(listing_uuid);
    
    if (listing_id == 0)
    {
        // Listing doesn't exists -> exit with error
        return false;
    }
    
    // Get listing data from SLM
    getSLMListing(listing_id);
    
    return true;
}

bool LLMarketplaceData::getListing(S32 listing_id)
{
    if (listing_id == 0)
    {
        return false;
    }
    
    // Get listing data from SLM
    getSLMListing(listing_id);
    return true;
}

bool LLMarketplaceData::activateListing(const LLUUID& folder_id, bool activate, S32 depth)
{
    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(folder_id);
        
    }
    // Folder id can be the root of the listing or not so we need to retrieve the root first
    LLUUID listing_uuid = nested_parent_id(folder_id, depth);
    S32 listing_id = getListingID(listing_uuid);
    if (listing_id == 0)
    {
        // Listing doesn't exists -> exit with error
        return false;
    }
    
    if (getActivationState(listing_uuid) == activate)
    {
        // If activation state is unchanged, no point spamming SLM with an update
        return true;
    }
    
    LLUUID version_uuid = getVersionFolder(listing_uuid);
    
    // Also update the count on hand
    S32 count = compute_stock_count(folder_id);
    if (count == COMPUTE_STOCK_NOT_EVALUATED)
    {
        // If the count on hand cannot be evaluated locally, we should not change that SLM value
        // We are assuming that this issue is local and should not modify server side values
        count = getCountOnHand(listing_uuid);
    }
    
    // Post the listing update request to SLM
    updateSLMListing(listing_uuid, listing_id, version_uuid, activate, count);
    
    return true;
}

bool LLMarketplaceData::setVersionFolder(const LLUUID& folder_id, const LLUUID& version_id, S32 depth)
{
    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(folder_id);
        
    }
    // Folder id can be the root of the listing or not so we need to retrieve the root first
    LLUUID listing_uuid = nested_parent_id(folder_id, depth);
    S32 listing_id = getListingID(listing_uuid);
    if (listing_id == 0)
    {
        // Listing doesn't exists -> exit with error
        return false;
    }
    
    if (getVersionFolder(listing_uuid) == version_id)
    {
        // If version folder is unchanged, no point spamming SLM with an update
        return true;
    }
    
    // Note: if the version_id is cleared, we need to unlist the listing, otherwise, state unchanged
    bool is_listed = (version_id.isNull() ? false : getActivationState(listing_uuid));
    
    // Also update the count on hand
    S32 count = compute_stock_count(version_id);
    if (count == COMPUTE_STOCK_NOT_EVALUATED)
    {
        // If the count on hand cannot be evaluated, we will consider it empty (out of stock) when resetting the version folder
        // It will get reevaluated and updated once the items are fetched
        count = 0;
    }
    
    // Post the listing update request to SLM
    updateSLMListing(listing_uuid, listing_id, version_id, is_listed, count);
    
    return true;
}

bool LLMarketplaceData::updateCountOnHand(const LLUUID& folder_id, S32 depth)
{
    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(folder_id);
        
    }
    // Folder id can be the root of the listing or not so we need to retrieve the root first
    LLUUID listing_uuid = nested_parent_id(folder_id, depth);
    S32 listing_id = getListingID(listing_uuid);
    if (listing_id == 0)
    {
        // Listing doesn't exists -> exit with error
        return false;
    }
    
    // Compute the new count on hand
    S32 count = compute_stock_count(folder_id);

    if (count == getCountOnHand(listing_uuid))
    {
        // If count on hand is unchanged, no point spamming SLM with an update
        return true;
    }
    else if (count == COMPUTE_STOCK_NOT_EVALUATED)
    {
        // If local count on hand is not known at that point, do *not* force an update to SLM
        return false;
    }
    
    // Get the unchanged values
    bool is_listed = getActivationState(listing_uuid);
    LLUUID version_uuid = getVersionFolder(listing_uuid);

    
    // Post the listing update request to SLM
    updateSLMListing(listing_uuid, listing_id, version_uuid, is_listed, count);
    
    // Force the local value as it prevents spamming (count update may occur in burst when restocking)
    // Note that if SLM has a good reason to return a different value, it'll be updated by the responder
    setCountOnHand(listing_uuid, count, false);

    return true;
}

bool LLMarketplaceData::associateListing(const LLUUID& folder_id, const LLUUID& source_folder_id, S32 listing_id)
{
    if (isListed(folder_id))
    {
        // Listing already exists -> exit with error
        return false;
    }
    
    // Get the version folder: if there is only one subfolder, we will set it as a version folder immediately
    LLUUID version_id = getVersionFolderIfUnique(folder_id);
    
    // Post the listing associate request to SLM
    associateSLMListing(folder_id, listing_id, version_id, source_folder_id);
     
    return true;
}

// Methods privately called or called by SLM responders to perform changes
bool LLMarketplaceData::addListing(const LLUUID& folder_id, S32 listing_id, const LLUUID& version_id, bool is_listed,  const std::string& edit_url, S32 count)
{
	mMarketplaceItems[folder_id] = LLMarketplaceTuple(folder_id, listing_id, version_id, is_listed);
    mMarketplaceItems[folder_id].mEditURL = edit_url;
    mMarketplaceItems[folder_id].mCountOnHand = count;
    if (version_id.notNull())
    {
        mVersionFolders[version_id] = folder_id;
    }
    return true;
}

bool LLMarketplaceData::deleteListing(const LLUUID& folder_id, bool update)
{
    LLUUID version_folder = getVersionFolder(folder_id);
    
	if (mMarketplaceItems.erase(folder_id) != 1)
    {
        return false;
    }
    mVersionFolders.erase(version_folder);
    
    if (update)
    {
        update_marketplace_category(folder_id, false);
        gInventory.notifyObservers();
    }
    return true;
}

bool LLMarketplaceData::deleteListing(S32 listing_id, bool update)
{
    if (listing_id == 0)
    {
        return false;
    }
    
    LLUUID folder_id = getListingFolder(listing_id);
    return deleteListing(folder_id, update);
}

// Accessors
bool LLMarketplaceData::getActivationState(const LLUUID& folder_id)
{
    // Listing folder case
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    if (it != mMarketplaceItems.end())
    {
        return (it->second).mIsActive;
    }
    // Version folder case
    version_folders_list_t::iterator it_version = mVersionFolders.find(folder_id);
    if (it_version != mVersionFolders.end())
    {
        marketplace_items_list_t::iterator it = mMarketplaceItems.find(it_version->second);
        if (it != mMarketplaceItems.end())
        {
            return (it->second).mIsActive;
        }
    }
    return false;
}

S32 LLMarketplaceData::getListingID(const LLUUID& folder_id)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    return (it == mMarketplaceItems.end() ? 0 : (it->second).mListingId);
}

S32 LLMarketplaceData::getCountOnHand(const LLUUID& folder_id)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    return (it == mMarketplaceItems.end() ? -1 : (it->second).mCountOnHand);
}

LLUUID LLMarketplaceData::getVersionFolder(const LLUUID& folder_id)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    return (it == mMarketplaceItems.end() ? LLUUID::null : (it->second).mVersionFolderId);
}

// Reverse lookup : find the listing folder id from the listing id
LLUUID LLMarketplaceData::getListingFolder(S32 listing_id)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.begin();
    while (it != mMarketplaceItems.end())
    {
        if ((it->second).mListingId == listing_id)
        {
            return (it->second).mListingFolderId;
        }
        it++;
    }
    return LLUUID::null;
}

std::string LLMarketplaceData::getListingURL(const LLUUID& folder_id, S32 depth)
{
    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(folder_id);
        
    }

    LLUUID listing_uuid = nested_parent_id(folder_id, depth);
    
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(listing_uuid);
    return (it == mMarketplaceItems.end() ? "" : (it->second).mEditURL);
}

bool LLMarketplaceData::isListed(const LLUUID& folder_id)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    return (it != mMarketplaceItems.end());
}

bool LLMarketplaceData::isListedAndActive(const LLUUID& folder_id)
{
    return (isListed(folder_id) && getActivationState(folder_id));
}

bool LLMarketplaceData::isVersionFolder(const LLUUID& folder_id)
{
    version_folders_list_t::iterator it = mVersionFolders.find(folder_id);
    return (it != mVersionFolders.end());
}

bool LLMarketplaceData::isInActiveFolder(const LLUUID& obj_id, S32 depth)
{
    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(obj_id);
        
    }

    LLUUID listing_uuid = nested_parent_id(obj_id, depth);
    bool active = getActivationState(listing_uuid);
    LLUUID version_uuid = getVersionFolder(listing_uuid);
    return (active && ((obj_id == version_uuid) || gInventory.isObjectDescendentOf(obj_id, version_uuid)));
}

LLUUID LLMarketplaceData::getActiveFolder(const LLUUID& obj_id, S32 depth)
{
    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(obj_id);
        
    }
    
    LLUUID listing_uuid = nested_parent_id(obj_id, depth);
    return (getActivationState(listing_uuid) ? getVersionFolder(listing_uuid) : LLUUID::null);
}

bool LLMarketplaceData::isUpdating(const LLUUID& folder_id, S32 depth)
{
    // Evaluate the depth if it wasn't passed as a parameter
    if (depth < 0)
    {
        depth = depth_nesting_in_marketplace(folder_id);
    }
    if ((depth <= 0) || (depth > 2))
    {
        // Only listing and version folders though are concerned by that status
        return false;
    }
    else
    {
        const LLUUID marketplace_listings_uuid = gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS, false);
        std::set<LLUUID>::iterator it = mPendingUpdateSet.find(marketplace_listings_uuid);
        if (it != mPendingUpdateSet.end())
        {
            // If we're waiting for data for the marketplace listings root, we are in the updating process for all
            return true;
        }
        else
        {
            // Check if the listing folder is waiting or data
            LLUUID listing_uuid = nested_parent_id(folder_id, depth);
            it = mPendingUpdateSet.find(listing_uuid);
            return (it != mPendingUpdateSet.end());
        }
    }
}

void LLMarketplaceData::setUpdating(const LLUUID& folder_id, bool isUpdating)
{
    std::set<LLUUID>::iterator it = mPendingUpdateSet.find(folder_id);
    if (it != mPendingUpdateSet.end())
    {
        mPendingUpdateSet.erase(it);
    }
    if (isUpdating)
    {
        mPendingUpdateSet.insert(folder_id);
    }
}

void LLMarketplaceData::setValidationWaiting(const LLUUID& folder_id, S32 count)
{
    mValidationWaitingList[folder_id] = count;
}

void LLMarketplaceData::decrementValidationWaiting(const LLUUID& folder_id, S32 count)
{
    waiting_list_t::iterator found = mValidationWaitingList.find(folder_id);
    if (found != mValidationWaitingList.end())
    {
        found->second -= count;
        if (found->second <= 0)
        {
            mValidationWaitingList.erase(found);
            LLInventoryCategory *cat = gInventory.getCategory(folder_id);
            validate_marketplacelistings(cat);
            update_marketplace_category(folder_id);
            gInventory.notifyObservers();
        }
    }
}

// Private Modifiers
bool LLMarketplaceData::setListingID(const LLUUID& folder_id, S32 listing_id, bool update)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    if (it == mMarketplaceItems.end())
    {
        return false;
    }
    
    (it->second).mListingId = listing_id;
    
    if (update)
    {
        update_marketplace_category(folder_id, false);
        gInventory.notifyObservers();
    }
    return true;
}

bool LLMarketplaceData::setCountOnHand(const LLUUID& folder_id, S32 count, bool update)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    if (it == mMarketplaceItems.end())
    {
        return false;
    }
    
    (it->second).mCountOnHand = count;

    return true;
}

bool LLMarketplaceData::setVersionFolderID(const LLUUID& folder_id, const LLUUID& version_id, bool update)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    if (it == mMarketplaceItems.end())
    {
        return false;
    }
    
    LLUUID old_version_id = (it->second).mVersionFolderId;
    if (old_version_id == version_id)
    {
        return false;
    }
    
    (it->second).mVersionFolderId = version_id;
    mVersionFolders.erase(old_version_id);
    if (version_id.notNull())
    {
        mVersionFolders[version_id] = folder_id;
    }
    
    if (update)
    {
        update_marketplace_category(old_version_id, false);
        update_marketplace_category(version_id, false);
        gInventory.notifyObservers();
    }
    return true;
}

bool LLMarketplaceData::setActivationState(const LLUUID& folder_id, bool activate, bool update)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    if (it == mMarketplaceItems.end())
    {
        return false;
    }

    (it->second).mIsActive = activate;
    
    if (update)
    {
        update_marketplace_category((it->second).mListingFolderId, false);
        gInventory.notifyObservers();
    }
    return true;
}

bool LLMarketplaceData::setListingURL(const LLUUID& folder_id, const std::string& edit_url, bool update)
{
    marketplace_items_list_t::iterator it = mMarketplaceItems.find(folder_id);
    if (it == mMarketplaceItems.end())
    {
        return false;
    }
    
    (it->second).mEditURL = edit_url;
    return true;
}



