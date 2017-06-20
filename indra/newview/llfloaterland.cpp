// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llfloaterland.cpp
 * @brief "About Land" floater, allowing display and editing of land parcel properties.
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

#include "llviewerprecompiledheaders.h"

#include <sstream>
#include <time.h>

#include "llfloaterland.h"

#include "llavatarnamecache.h"
#include "llfocusmgr.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "message.h"

#include "llagent.h"
#include "llagentaccess.h"
#include "llappviewer.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "llfloateravatarpicker.h"
#include "llfloaterauction.h"
#include "llfloatergroups.h"
#include "llfloaterscriptlimits.h"
#include "llavataractions.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llpanellandmedia.h"
#include "llradiogroup.h"
#include "llresmgr.h"					// getMonetaryString
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llscrolllistcell.h"
#include "llselectmgr.h"
#include "llslurl.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lluiconstants.h"
#include "llviewertexturelist.h"		// LLUIImageList
#include "llviewermessage.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "roles_constants.h"
#include "lltrans.h"
#include "llpanelexperiencelisteditor.h"
#include "llpanelexperiencepicker.h"
#include "llexperiencecache.h"

#include "llgroupactions.h"

const F64 COVENANT_REFRESH_TIME_SEC = 60.0f;

static std::string OWNER_ONLINE 	= "0";
static std::string OWNER_OFFLINE	= "1";
static std::string OWNER_GROUP 		= "2";
static std::string MATURITY 		= "[MATURITY]";

// constants used in callbacks below - syntactic sugar.
static const BOOL BUY_GROUP_LAND = TRUE;
static const BOOL BUY_PERSONAL_LAND = FALSE;
LLPointer<LLParcelSelection> LLPanelLandGeneral::sSelectionForBuyPass = NULL;

// Statics
LLParcelSelectionObserver* LLFloaterLand::sObserver = NULL;
S32 LLFloaterLand::sLastTab = 0;

// Local classes
class LLParcelSelectionObserver : public LLParcelObserver
{
public:
	virtual void changed() { LLFloaterLand::refreshAll(); }
};

// class needed to get full access to textbox inside checkbox, because LLCheckBoxCtrl::setLabel() has string as its argument.
// It was introduced while implementing EXT-4706
class LLCheckBoxWithTBAcess	: public LLCheckBoxCtrl
{
public:
	LLTextBox* getTextBox()
	{
		return mLabel;
	}
};


class LLPanelLandExperiences
	:	public LLPanel
{
public:	
	LLPanelLandExperiences(LLSafeHandle<LLParcelSelection>& parcelp);
	virtual BOOL postBuild();
	void refresh();

	void experienceAdded(const LLUUID& id, U32 xp_type, U32 access_type);
	void experienceRemoved(const LLUUID& id, U32 access_type);
protected:
	LLPanelExperienceListEditor* setupList( const char* control_name, U32 xp_type, U32 access_type );
	void refreshPanel(LLPanelExperienceListEditor* panel, U32 xp_type);

	LLSafeHandle<LLParcelSelection>&	mParcel;


	LLPanelExperienceListEditor* mAllowed;
	LLPanelExperienceListEditor* mBlocked;
};

// inserts maturity info(icon and text) into target textbox 
// names_floater - pointer to floater which contains strings with maturity icons filenames
// str_to_parse is string in format "txt1[MATURITY]txt2" where maturity icon and text will be inserted instead of [MATURITY]
void insert_maturity_into_textbox(LLTextBox* target_textbox, LLFloater* names_floater, std::string str_to_parse);

//---------------------------------------------------------------------------
// LLFloaterLand
//---------------------------------------------------------------------------

void send_parcel_select_objects(S32 parcel_local_id, U32 return_type,
								uuid_list_t* return_ids = NULL)
{
	LLMessageSystem *msg = gMessageSystem;

	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region) return;

	// Since new highlight will be coming in, drop any highlights
	// that exist right now.
	LLSelectMgr::getInstance()->unhighlightAll();

	msg->newMessageFast(_PREHASH_ParcelSelectObjects);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addU32Fast(_PREHASH_ReturnType, return_type);

	// Throw all return ids into the packet.
	// TODO: Check for too many ids.
	if (return_ids)
	{
		uuid_list_t::iterator end = return_ids->end();
		for (uuid_list_t::iterator it = return_ids->begin();
			 it != end;
			 ++it)
		{
			msg->nextBlockFast(_PREHASH_ReturnIDs);
			msg->addUUIDFast(_PREHASH_ReturnID, (*it));
		}
	}
	else
	{
		// Put in a null key so that the message is complete.
		msg->nextBlockFast(_PREHASH_ReturnIDs);
		msg->addUUIDFast(_PREHASH_ReturnID, LLUUID::null);
	}

	msg->sendReliable(region->getHost());
}

LLParcel* LLFloaterLand::getCurrentSelectedParcel()
{
	return mParcel->getParcel();
};

//static
LLPanelLandObjects* LLFloaterLand::getCurrentPanelLandObjects()
{
	LLFloaterLand* land_instance = LLFloaterReg::getTypedInstance<LLFloaterLand>("about_land");
	if(land_instance)
	{
		return land_instance->mPanelObjects;
	}
	else
	{
		return NULL;
	}
}

//static
LLPanelLandCovenant* LLFloaterLand::getCurrentPanelLandCovenant()
{
	LLFloaterLand* land_instance = LLFloaterReg::getTypedInstance<LLFloaterLand>("about_land");
	if(land_instance)
	{
		return land_instance->mPanelCovenant;
	}
	else
	{
		return NULL;
	}
}

// static
void LLFloaterLand::refreshAll()
{
	LLFloaterLand* land_instance = LLFloaterReg::getTypedInstance<LLFloaterLand>("about_land");
	if(land_instance)
	{
		land_instance->refresh();
	}
}

void LLFloaterLand::onOpen(const LLSD& key)
{
	// moved from triggering show instance in llviewermenu.cpp
	
	if (LLViewerParcelMgr::getInstance()->selectionEmpty())
	{
		LLViewerParcelMgr::getInstance()->selectParcelAt(gAgent.getPositionGlobal());
	}
	
	// Done automatically when the selected parcel's properties arrive
	// (and hence we have the local id).
	// LLViewerParcelMgr::getInstance()->sendParcelAccessListRequest(AL_ACCESS | AL_BAN | AL_RENTER);

	mParcel = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();
	
	// Refresh even if not over a region so we don't get an
	// uninitialized dialog. The dialog is 0-region aware.
	refresh();
}

void LLFloaterLand::onVisibilityChanged(const LLSD& visible)
{
	if (!visible.asBoolean())
	{
		// Might have been showing owned objects
		LLSelectMgr::getInstance()->unhighlightAll();

		// Save which panel we had open
		sLastTab = mTabLand->getCurrentPanelIndex();
	}
}


LLFloaterLand::LLFloaterLand(const LLSD& seed)
	: LLFloater(seed)
	, mTabLand(nullptr)
	, mPanelGeneral(nullptr)
	, mPanelObjects(nullptr)
	, mPanelOptions(nullptr)
	, mPanelMedia(nullptr)
	, mPanelAccess(nullptr)
	, mPanelCovenant(nullptr)
	, mPanelExperiences(nullptr)
{
	mFactoryMap["land_general_panel"] = LLCallbackMap(createPanelLandGeneral, this);
	mFactoryMap["land_covenant_panel"] = LLCallbackMap(createPanelLandCovenant, this);
	mFactoryMap["land_objects_panel"] = LLCallbackMap(createPanelLandObjects, this);
	mFactoryMap["land_options_panel"] = LLCallbackMap(createPanelLandOptions, this);
	mFactoryMap["land_media_panel"] =	LLCallbackMap(createPanelLandMedia, this);
	mFactoryMap["land_access_panel"] =	LLCallbackMap(createPanelLandAccess, this);
	mFactoryMap["land_experiences_panel"] =	LLCallbackMap(createPanelLandExperiences, this);

	sObserver = new LLParcelSelectionObserver();
	LLViewerParcelMgr::getInstance()->addObserver( sObserver );
}

BOOL LLFloaterLand::postBuild()
{
	if (!mParcelMsgSignalSlot.connected())
	{
		mParcelMsgSignalSlot = LLViewerParcelMgr::getInstance()->addParcelMsgCallback(
			boost::bind(&LLFloaterLand::selectiveRefresh, this));
	}
	setVisibleCallback(boost::bind(&LLFloaterLand::onVisibilityChanged, this, _2));
	
	LLTabContainer* tab = getChild<LLTabContainer>("landtab");

	mTabLand = (LLTabContainer*) tab;

	if (tab)
	{
		tab->selectTab(sLastTab);
	}

	return TRUE;
}


// virtual
LLFloaterLand::~LLFloaterLand()
{
	if (mParcelMsgSignalSlot.connected())
		mParcelMsgSignalSlot.disconnect();
	
	LLViewerParcelMgr::getInstance()->removeObserver( sObserver );
	delete sObserver;
	sObserver = NULL;
}

// public
void LLFloaterLand::refresh()
{
	mPanelGeneral->refresh();
	mPanelObjects->refresh();
	mPanelOptions->refresh();
	mPanelMedia->refresh();
	mPanelAccess->refresh();
	mPanelCovenant->refresh();
	mPanelExperiences->refresh();
}

// public
void LLFloaterLand::selectiveRefresh()
{
	mPanelAccess->refresh();
	mPanelGeneral->refresh();
}

// static
void* LLFloaterLand::createPanelLandGeneral(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelGeneral = new LLPanelLandGeneral(self->mParcel);
	return self->mPanelGeneral;
}

// static
void* LLFloaterLand::createPanelLandCovenant(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelCovenant = new LLPanelLandCovenant(self->mParcel);
	return self->mPanelCovenant;
}


// static
void* LLFloaterLand::createPanelLandObjects(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelObjects = new LLPanelLandObjects(self->mParcel);
	return self->mPanelObjects;
}

// static
void* LLFloaterLand::createPanelLandOptions(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelOptions = new LLPanelLandOptions(self->mParcel);
	return self->mPanelOptions;
}

// static
void* LLFloaterLand::createPanelLandMedia(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelMedia = new LLPanelLandMedia(self->mParcel);
	return self->mPanelMedia;
}

// static
void* LLFloaterLand::createPanelLandAccess(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelAccess = new LLPanelLandAccess(self->mParcel);
	return self->mPanelAccess;
}

// static
void* LLFloaterLand::createPanelLandExperiences(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelExperiences = new LLPanelLandExperiences(self->mParcel);
	return self->mPanelExperiences;
}


//---------------------------------------------------------------------------
// LLPanelLandGeneral
//---------------------------------------------------------------------------


LLPanelLandGeneral::LLPanelLandGeneral(LLParcelSelectionHandle& parcel)
	: LLPanel()
	, mEditName(nullptr)
	, mEditDesc(nullptr)
	, mTextSalePending(nullptr)
	, mBtnDeedToGroup(nullptr)
	, mBtnSetGroup(nullptr)
	, mTextOwner(nullptr)
	, mBtnProfile(nullptr)
	, mContentRating(nullptr)
	, mLandType(nullptr)
	, mTextGroup(nullptr)
	, mTextClaimDate(nullptr)
	, mTextPriceLabel(nullptr)
	, mTextPrice(nullptr)
	, mCheckDeedToGroup(nullptr)
	, mCheckContributeWithDeed(nullptr)
	, mSaleInfoForSale1(nullptr)
	, mSaleInfoForSale2(nullptr)
	, mSaleInfoForSaleObjects(nullptr)
	, mSaleInfoForSaleNoObjects(nullptr)
	, mSaleInfoNotForSale(nullptr)
	, mBtnSellLand(nullptr)
	, mBtnStopSellLand(nullptr)
	, mTextDwell(nullptr)
	, mBtnBuyLand(nullptr)
	, mBtnScriptLimits(nullptr)
	, mBtnBuyGroupLand(nullptr)
	, mBtnReleaseLand(nullptr)
	, mBtnReclaimLand(nullptr)
	, mBtnBuyPass(nullptr)
	, mBtnStartAuction(nullptr)
	, mParcel(parcel)
{
}

BOOL LLPanelLandGeneral::postBuild()
{
	mEditName = getChild<LLLineEditor>("Name");
	mEditName->setCommitCallback(boost::bind(&LLPanelLandGeneral::onCommitAny, this));
	getChild<LLLineEditor>("Name")->setPrevalidate(LLTextValidate::validateASCIIPrintableNoPipe);

	mEditDesc = getChild<LLTextEditor>("Description");
	mEditDesc->setCommitOnFocusLost(TRUE);
	mEditDesc->setCommitCallback(boost::bind(&LLPanelLandGeneral::onCommitAny, this));
	// No prevalidate function - historically the prevalidate function was broken,
	// allowing residents to put in characters like U+2661 WHITE HEART SUIT, so
	// preserve that ability.
	
	mTextSalePending = getChild<LLTextBox>("SalePending");
	mTextOwner = getChild<LLTextBox>("OwnerText");
	
	mContentRating = getChild<LLTextBox>("ContentRatingText");
	mLandType = getChild<LLTextBox>("LandTypeText");
	
	mBtnProfile = getChild<LLButton>("Profile...");
	mBtnProfile->setClickedCallback(boost::bind(&LLPanelLandGeneral::onClickProfile, this));

	
	mTextGroup = getChild<LLTextBox>("GroupText");

	
	mBtnSetGroup = getChild<LLButton>("Set...");
	mBtnSetGroup->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickSetGroup, this));

	
	mCheckDeedToGroup = getChild<LLCheckBoxCtrl>( "check deed");
	mCheckDeedToGroup->setCommitCallback(boost::bind(&LLPanelLandGeneral::onCommitAny, this));

	
	mBtnDeedToGroup = getChild<LLButton>("Deed...");
	mBtnDeedToGroup->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickDeed, this));

	
	mCheckContributeWithDeed = getChild<LLCheckBoxCtrl>( "check contrib");
	mCheckContributeWithDeed->setCommitCallback(boost::bind(&LLPanelLandGeneral::onCommitAny, this));
	
	
	mSaleInfoNotForSale = getChild<LLTextBox>("Not for sale.");
	
	mSaleInfoForSale1 = getChild<LLTextBox>("For Sale: Price L$[PRICE].");

	
	mBtnSellLand = getChild<LLButton>("Sell Land...");
	mBtnSellLand->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickSellLand, this));
	
	mSaleInfoForSale2 = getChild<LLTextBox>("For sale to");
	
	mSaleInfoForSaleObjects = getChild<LLTextBox>("Sell with landowners objects in parcel.");
	
	mSaleInfoForSaleNoObjects = getChild<LLTextBox>("Selling with no objects in parcel.");

	
	mBtnStopSellLand = getChild<LLButton>("Cancel Land Sale");
	mBtnStopSellLand->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickStopSellLand, this));

	
	mTextClaimDate = getChild<LLTextBox>("DateClaimText");

	
	mTextPriceLabel = getChild<LLTextBox>("PriceLabel");
	mTextPrice = getChild<LLTextBox>("PriceText");

	
	mTextDwell = getChild<LLTextBox>("DwellText");
	
	mBtnBuyLand = getChild<LLButton>("Buy Land...");
	mBtnBuyLand->setClickedCallback(onClickBuyLand, (void*)&BUY_PERSONAL_LAND);
	
	mBtnBuyGroupLand = getChild<LLButton>("Buy For Group...");
	mBtnBuyGroupLand->setClickedCallback(onClickBuyLand, (void*)&BUY_GROUP_LAND);


	mBtnBuyPass = getChild<LLButton>("Buy Pass...");
	mBtnBuyPass->setClickedCallback(onClickBuyPass, this);

	mBtnReleaseLand = getChild<LLButton>("Abandon Land...");
	mBtnReleaseLand->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickRelease, this));

	mBtnReclaimLand = getChild<LLButton>("Reclaim Land...");
	mBtnReclaimLand->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickReclaim, this));
	
	mBtnStartAuction = getChild<LLButton>("Linden Sale...");
	mBtnStartAuction->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickStartAuction, this));

	// note: on region change this will not be re checked, should not matter on Agni as
	// 99% of the time all regions will return the same caps. In case of an erroneous setting
	// to enabled the floater will just throw an error when trying to get it's cap
	mBtnScriptLimits = getChild<LLButton>("Scripts...");
	std::string url = gAgent.getRegion() ? gAgent.getRegion()->getCapability("LandResources") : LLStringUtil::null;
	if (!url.empty())
	{
		mBtnScriptLimits->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickScriptLimits, this));
	}
	else
	{
		mBtnScriptLimits->setVisible(false);
	}
	

	return TRUE;
}


// virtual
LLPanelLandGeneral::~LLPanelLandGeneral()
{ }


// public
void LLPanelLandGeneral::refresh()
{
	mEditName->setEnabled(FALSE);
	mEditName->setText(LLStringUtil::null);

	mEditDesc->setEnabled(FALSE);
	mEditDesc->setText(getString("no_selection_text"));

	mTextSalePending->setText(LLStringUtil::null);
	mTextSalePending->setEnabled(FALSE);

	mBtnDeedToGroup->setEnabled(FALSE);
	mBtnSetGroup->setEnabled(FALSE);
	mBtnStartAuction->setEnabled(FALSE);

	mCheckDeedToGroup	->set(FALSE);
	mCheckDeedToGroup	->setEnabled(FALSE);
	mCheckContributeWithDeed->set(FALSE);
	mCheckContributeWithDeed->setEnabled(FALSE);

	mTextOwner->setText(LLStringUtil::null);
	mContentRating->setText(LLStringUtil::null);
	mLandType->setText(LLStringUtil::null);
	mBtnProfile->setLabel(getString("profile_text"));
	mBtnProfile->setEnabled(FALSE);

	mTextClaimDate->setText(LLStringUtil::null);
	mTextGroup->setText(LLStringUtil::null);
	mTextPrice->setText(LLStringUtil::null);

	mSaleInfoForSale1->setVisible(FALSE);
	mSaleInfoForSale2->setVisible(FALSE);
	mSaleInfoForSaleObjects->setVisible(FALSE);
	mSaleInfoForSaleNoObjects->setVisible(FALSE);
	mSaleInfoNotForSale->setVisible(FALSE);
	mBtnSellLand->setVisible(FALSE);
	mBtnStopSellLand->setVisible(FALSE);

	mTextPriceLabel->setText(LLStringUtil::null);
	mTextDwell->setText(LLStringUtil::null);

	mBtnBuyLand->setEnabled(FALSE);
	mBtnScriptLimits->setEnabled(FALSE);
	mBtnBuyGroupLand->setEnabled(FALSE);
	mBtnReleaseLand->setEnabled(FALSE);
	mBtnReclaimLand->setEnabled(FALSE);
	mBtnBuyPass->setEnabled(FALSE);

	if(gDisconnected)
	{
		return;
	}

	mBtnStartAuction->setVisible(gAgent.isGodlike());
	bool region_owner = false;
	LLViewerRegion* regionp = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if(regionp && (regionp->getOwner() == gAgent.getID()))
	{
		region_owner = true;
		mBtnReleaseLand->setVisible(FALSE);
		mBtnReclaimLand->setVisible(TRUE);
	}
	else
	{
		mBtnReleaseLand->setVisible(TRUE);
		mBtnReclaimLand->setVisible(FALSE);
	}
	LLParcel *parcel = mParcel->getParcel();
	if (parcel)
	{
		// something selected, hooray!
		BOOL is_leased = (LLParcel::OS_LEASED == parcel->getOwnershipStatus());
		BOOL region_xfer = FALSE;
		if(regionp
		   && !(regionp->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL)))
		{
			region_xfer = TRUE;
		}
		
		if (regionp)
		{
			insert_maturity_into_textbox(mContentRating, gFloaterView->getParentFloater(this), MATURITY);
			mLandType->setText(regionp->getLocalizedSimProductName());
		}

		// estate owner/manager cannot edit other parts of the parcel
		BOOL estate_manager_sellable = !parcel->getAuctionID()
										&& gAgent.canManageEstate()
										// estate manager/owner can only sell parcels owned by estate owner
										&& regionp
										&& (parcel->getOwnerID() == regionp->getOwner());
		BOOL owner_sellable = region_xfer && !parcel->getAuctionID()
							&& LLViewerParcelMgr::isParcelModifiableByAgent(
										parcel, GP_LAND_SET_SALE_INFO);
		BOOL can_be_sold = owner_sellable || estate_manager_sellable;

		const LLUUID &owner_id = parcel->getOwnerID();
		BOOL is_public = parcel->isPublic();

		// Is it owned?
		if (is_public)
		{
			mTextSalePending->setText(LLStringUtil::null);
			mTextSalePending->setEnabled(FALSE);
			mTextOwner->setText(getString("public_text"));
			mTextOwner->setEnabled(FALSE);
			mBtnProfile->setEnabled(FALSE);
			mTextClaimDate->setText(LLStringUtil::null);
			mTextClaimDate->setEnabled(FALSE);
			mTextGroup->setText(getString("none_text"));
			mTextGroup->setEnabled(FALSE);
			mBtnStartAuction->setEnabled(FALSE);
		}
		else
		{
			if(!is_leased && (owner_id == gAgent.getID()))
			{
				mTextSalePending->setText(getString("need_tier_to_modify"));
				mTextSalePending->setEnabled(TRUE);
			}
			else if(parcel->getAuctionID())
			{
				mTextSalePending->setText(getString("auction_id_text"));
				mTextSalePending->setTextArg("[ID]", llformat("%u", parcel->getAuctionID()));
				mTextSalePending->setEnabled(TRUE);
			}
			else
			{
				// not the owner, or it is leased
				mTextSalePending->setText(LLStringUtil::null);
				mTextSalePending->setEnabled(FALSE);
			}
			//refreshNames();
			mTextOwner->setEnabled(TRUE);

			// We support both group and personal profiles
			mBtnProfile->setEnabled(TRUE);

			if (parcel->getGroupID().isNull())
			{
				// Not group owned, so "Profile"
				mBtnProfile->setLabel(getString("profile_text"));

				mTextGroup->setText(getString("none_text"));
				mTextGroup->setEnabled(FALSE);
			}
			else
			{
				// Group owned, so "Info"
				mBtnProfile->setLabel(getString("info_text"));

				//mTextGroup->setText("HIPPOS!");//parcel->getGroupName());
				mTextGroup->setEnabled(TRUE);
			}

			// Display claim date
			time_t claim_date = parcel->getClaimDate();
			std::string claim_date_str = getString("time_stamp_template");
			LLSD substitution;
			substitution["datetime"] = (S32) claim_date;
			LLStringUtil::format (claim_date_str, substitution);
			mTextClaimDate->setText(claim_date_str);
			mTextClaimDate->setEnabled(is_leased);

			BOOL enable_auction = (gAgent.getGodLevel() >= GOD_LIAISON)
								  && (owner_id == GOVERNOR_LINDEN_ID)
								  && (parcel->getAuctionID() == 0);
			mBtnStartAuction->setEnabled(enable_auction);
		}

		// Display options
		BOOL can_edit_identity = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_CHANGE_IDENTITY);
		mEditName->setEnabled(can_edit_identity);
		mEditDesc->setEnabled(can_edit_identity);

		BOOL can_edit_agent_only = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_NO_POWERS);
		mBtnSetGroup->setEnabled(can_edit_agent_only && !parcel->getIsGroupOwned());

		const LLUUID& group_id = parcel->getGroupID();

		// Can only allow deeding if you own it and it's got a group.
		BOOL enable_deed = (owner_id == gAgent.getID()
							&& group_id.notNull()
							&& gAgent.isInGroup(group_id));
		// You don't need special powers to allow your object to
		// be deeded to the group.
		mCheckDeedToGroup->setEnabled(enable_deed);
		mCheckDeedToGroup->set( parcel->getAllowDeedToGroup() );
		mCheckContributeWithDeed->setEnabled(enable_deed && parcel->getAllowDeedToGroup());
		mCheckContributeWithDeed->set(parcel->getContributeWithDeed());

		// Actually doing the deeding requires you to have GP_LAND_DEED
		// powers in the group.
		BOOL can_deed = gAgent.hasPowerInGroup(group_id, GP_LAND_DEED);
		mBtnDeedToGroup->setEnabled(   parcel->getAllowDeedToGroup()
									&& group_id.notNull()
									&& can_deed
									&& !parcel->getIsGroupOwned()
									);

		mEditName->setText( parcel->getName() );
		mEditDesc->setText( parcel->getDesc() );

		BOOL for_sale = parcel->getForSale();
				
		mBtnSellLand->setVisible(FALSE);
		mBtnStopSellLand->setVisible(FALSE);
		
		// show pricing information
		S32 area;
		S32 claim_price;
		S32 rent_price;
		F32 dwell = DWELL_NAN;
		LLViewerParcelMgr::getInstance()->getDisplayInfo(&area,
								 &claim_price,
								 &rent_price,
								 &for_sale,
								 &dwell);
		// Area
		LLUIString price = getString("area_size_text");
		price.setArg("[AREA]", llformat("%d",area));    
		mTextPriceLabel->setText(getString("area_text"));
		mTextPrice->setText(price.getString());

		if (dwell == DWELL_NAN)
		{
			mTextDwell->setText(LLTrans::getString("LoadingData"));
		}
		else
		{
			mTextDwell->setText(llformat("%.0f", dwell));
		}

		if (for_sale)
		{
			mSaleInfoForSale1->setVisible(TRUE);
			mSaleInfoForSale2->setVisible(TRUE);
			if (parcel->getSellWithObjects())
			{
				mSaleInfoForSaleObjects->setVisible(TRUE);
				mSaleInfoForSaleNoObjects->setVisible(FALSE);
			}
			else
			{
				mSaleInfoForSaleObjects->setVisible(FALSE);
				mSaleInfoForSaleNoObjects->setVisible(TRUE);
			}
			mSaleInfoNotForSale->setVisible(FALSE);

			F32 cost_per_sqm = 0.0f;
			if (area > 0)
			{
				cost_per_sqm = (F32)parcel->getSalePrice() / (F32)area;
			}

			S32 price = parcel->getSalePrice();
			mSaleInfoForSale1->setTextArg("[PRICE]", LLResMgr::getInstance()->getMonetaryString(price));
			mSaleInfoForSale1->setTextArg("[PRICE_PER_SQM]", llformat("%.1f", cost_per_sqm));
			if (can_be_sold)
			{
				mBtnStopSellLand->setVisible(TRUE);
			}
		}
		else
		{
			mSaleInfoForSale1->setVisible(FALSE);
			mSaleInfoForSale2->setVisible(FALSE);
			mSaleInfoForSaleObjects->setVisible(FALSE);
			mSaleInfoForSaleNoObjects->setVisible(FALSE);
			mSaleInfoNotForSale->setVisible(TRUE);
			if (can_be_sold)
			{
				mBtnSellLand->setVisible(TRUE);
			}
		}
		
		refreshNames();

		mBtnBuyLand->setEnabled(
			LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, false));
		mBtnScriptLimits->setEnabled(true);
//			LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, false));
		mBtnBuyGroupLand->setEnabled(
			LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, true));

		if(region_owner)
		{
			mBtnReclaimLand->setEnabled(
				!is_public && (parcel->getOwnerID() != gAgent.getID()));
		}
		else
		{
			BOOL is_owner_release = LLViewerParcelMgr::isParcelOwnedByAgent(parcel, GP_LAND_RELEASE);
			BOOL is_manager_release = (gAgent.canManageEstate() && 
									regionp && 
									(parcel->getOwnerID() != regionp->getOwner()));
			BOOL can_release = is_owner_release || is_manager_release;
			mBtnReleaseLand->setEnabled( can_release );
		}

		BOOL use_pass = parcel->getOwnerID()!= gAgent.getID() && parcel->getParcelFlag(PF_USE_PASS_LIST) && !LLViewerParcelMgr::getInstance()->isCollisionBanned();;
		mBtnBuyPass->setEnabled(use_pass);

	}
}

// public
void LLPanelLandGeneral::refreshNames()
{
	LLParcel *parcel = mParcel->getParcel();
	if (!parcel)
	{
		mTextOwner->setText(LLStringUtil::null);
		mTextGroup->setText(LLStringUtil::null);
		return;
	}

	std::string owner;
	if (parcel->getIsGroupOwned())
	{
		owner = getString("group_owned_text");
	}
	else
	{
		// Figure out the owner's name
		owner = LLSLURL("agent", parcel->getOwnerID(), "inspect").getSLURLString();
	}

	if(LLParcel::OS_LEASE_PENDING == parcel->getOwnershipStatus())
	{
		owner += getString("sale_pending_text");
	}
	mTextOwner->setText(owner);

	std::string group;
	if (!parcel->getGroupID().isNull())
	{
		group = LLSLURL("group", parcel->getGroupID(), "inspect").getSLURLString();
	}
	mTextGroup->setText(group);

	if (parcel->getForSale())
	{
		const LLUUID& auth_buyer_id = parcel->getAuthorizedBuyerID();
		if(auth_buyer_id.notNull())
		{
		  std::string name;
		  name = LLSLURL("agent", auth_buyer_id, "inspect").getSLURLString();
		  mSaleInfoForSale2->setTextArg("[BUYER]", name);
		}
		else
		{
			mSaleInfoForSale2->setTextArg("[BUYER]", getString("anyone"));
		}
	}
}


// virtual
void LLPanelLandGeneral::draw()
{
	LLPanel::draw();
}

void LLPanelLandGeneral::onClickSetGroup()
{
	LLFloater* parent_floater = gFloaterView->getParentFloater(this);

	LLFloaterGroupPicker* fg = 	LLFloaterReg::showTypedInstance<LLFloaterGroupPicker>("group_picker", LLSD(gAgent.getID()));
	if (fg)
	{
		fg->setSelectGroupCallback( boost::bind(&LLPanelLandGeneral::setGroup, this, _1 ));
		if (parent_floater)
		{
			LLRect new_rect = gFloaterView->findNeighboringPosition(parent_floater, fg);
			fg->setOrigin(new_rect.mLeft, new_rect.mBottom);
			parent_floater->addDependentFloater(fg);
		}
	}
}

void LLPanelLandGeneral::onClickProfile()
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel) return;

	if (parcel->getIsGroupOwned())
	{
		const LLUUID& group_id = parcel->getGroupID();
		LLGroupActions::show(group_id);
	}
	else
	{
		const LLUUID& avatar_id = parcel->getOwnerID();
		LLAvatarActions::showProfile(avatar_id);
	}
}

// public
void LLPanelLandGeneral::setGroup(const LLUUID& group_id)
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel) return;

	// Set parcel properties and send message
	parcel->setGroupID(group_id);
	//parcel->setGroupName(group_name);
	//mTextGroup->setText(group_name);

	// Send update
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(parcel);

	// Update UI
	refresh();
}

// static
void LLPanelLandGeneral::onClickBuyLand(void* data)
{
	BOOL* for_group = (BOOL*)data;
	LLViewerParcelMgr::getInstance()->startBuyLand(*for_group);
}

void LLPanelLandGeneral::onClickScriptLimits()
{
	LLParcel* parcel = mParcel->getParcel();
	if(parcel != NULL)
	{
		LLFloaterReg::showInstance("script_limits");
	}
}

void LLPanelLandGeneral::onClickDeed()
{
	//LLParcel* parcel = mParcel->getParcel();
	//if (parcel)
	//{
	LLViewerParcelMgr::getInstance()->startDeedLandToGroup();
	//}
}

void LLPanelLandGeneral::onClickRelease()
{
	LLViewerParcelMgr::getInstance()->startReleaseLand();
}

void LLPanelLandGeneral::onClickReclaim()
{
	LL_DEBUGS() << "LLPanelLandGeneral::onClickReclaim()" << LL_ENDL;
	LLViewerParcelMgr::getInstance()->reclaimParcel();
}

// static
BOOL LLPanelLandGeneral::enableBuyPass(void* data)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
	LLParcel* parcel = panelp != NULL ? panelp->mParcel->getParcel() : LLViewerParcelMgr::getInstance()->getParcelSelection()->getParcel();
	return (parcel != NULL) && (parcel->getParcelFlag(PF_USE_PASS_LIST) && !LLViewerParcelMgr::getInstance()->isCollisionBanned());
}


// static
void LLPanelLandGeneral::onClickBuyPass(void* data)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
	LLParcel* parcel = panelp != NULL ? panelp->mParcel->getParcel() : LLViewerParcelMgr::getInstance()->getParcelSelection()->getParcel();

	if (!parcel) return;

	S32 pass_price = parcel->getPassPrice();
	std::string parcel_name = parcel->getName();
	F32 pass_hours = parcel->getPassHours();

	std::string cost, time;
	cost = llformat("%d", pass_price);
	time = llformat("%.2f", pass_hours);

	LLSD args;
	args["COST"] = cost;
	args["PARCEL_NAME"] = parcel_name;
	args["TIME"] = time;
	
	// creating pointer on selection to avoid deselection of parcel until we are done with buying pass (EXT-6464)
	sSelectionForBuyPass = LLViewerParcelMgr::getInstance()->getParcelSelection();
	LLNotificationsUtil::add("LandBuyPass", args, LLSD(), cbBuyPass);
}

void LLPanelLandGeneral::onClickStartAuction()
{
	LLParcel* parcelp = mParcel->getParcel();
	if(parcelp)
	{
		if(parcelp->getForSale())
		{
			LLNotificationsUtil::add("CannotStartAuctionAlreadyForSale");
		}
		else
		{
			//LLFloaterAuction::showInstance();
			LLFloaterReg::showInstance("auction");
		}
	}
}

// static
bool LLPanelLandGeneral::cbBuyPass(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == option)
	{
		// User clicked OK
		LLViewerParcelMgr::getInstance()->buyPass();
	}
	// we are done with buying pass, additional selection is no longer needed
	sSelectionForBuyPass = NULL;
	return false;
}

void LLPanelLandGeneral::onCommitAny()
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	std::string name = mEditName->getText();
	std::string desc = mEditDesc->getText();

	// Valid data from UI

	// Stuff data into selected parcel
	parcel->setName(name);
	parcel->setDesc(desc);

	BOOL allow_deed_to_group= mCheckDeedToGroup->get();
	BOOL contribute_with_deed = mCheckContributeWithDeed->get();

	parcel->setParcelFlag(PF_ALLOW_DEED_TO_GROUP, allow_deed_to_group);
	parcel->setContributeWithDeed(contribute_with_deed);

	// Send update to server
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

	// Might have changed properties, so let's redraw!
	refresh();
}

void LLPanelLandGeneral::onClickSellLand()
{
	LLViewerParcelMgr::getInstance()->startSellLand();
	refresh();
}

void LLPanelLandGeneral::onClickStopSellLand()
{
	LLParcel* parcel = mParcel->getParcel();

	parcel->setParcelFlag(PF_FOR_SALE, FALSE);
	parcel->setSalePrice(0);
	parcel->setAuthorizedBuyerID(LLUUID::null);

	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(parcel);
}

//---------------------------------------------------------------------------
// LLPanelLandObjects
//---------------------------------------------------------------------------
LLPanelLandObjects::LLPanelLandObjects(LLParcelSelectionHandle& parcel)
	:	LLPanel(),

		mParcel(parcel),
		mParcelObjectBonus(NULL),
		mSWTotalObjects(NULL),
		mObjectContribution(NULL),
		mTotalObjects(NULL),
		mOwnerObjects(NULL),
		mBtnShowOwnerObjects(NULL),
		mBtnReturnOwnerObjects(NULL),
		mGroupObjects(NULL),
		mBtnShowGroupObjects(NULL),
		mBtnReturnGroupObjects(NULL),
		mOtherObjects(NULL),
		mBtnShowOtherObjects(NULL),
		mBtnReturnOtherObjects(NULL),
		mSelectedObjects(NULL),
		mCleanOtherObjectsTime(NULL),
		mOtherTime(0),
		mBtnRefresh(NULL),
		mBtnReturnOwnerList(NULL),
		mOwnerList(NULL),
		mFirstReply(TRUE),
		mSelectedCount(0),
		mSelectedIsGroup(FALSE)
{
}



BOOL LLPanelLandObjects::postBuild()
{
	mFirstReply = TRUE;
	mParcelObjectBonus = getChild<LLTextBox>("parcel_object_bonus");
	mSWTotalObjects = getChild<LLTextBox>("objects_available");
	mObjectContribution = getChild<LLTextBox>("object_contrib_text");
	mTotalObjects = getChild<LLTextBox>("total_objects_text");
	mOwnerObjects = getChild<LLTextBox>("owner_objects_text");
	
	mBtnShowOwnerObjects = getChild<LLButton>("ShowOwner");
	mBtnShowOwnerObjects->setCommitCallback(boost::bind(&LLPanelLandObjects::onClickShowOwnerObjects, this));
	
	mBtnReturnOwnerObjects = getChild<LLButton>("ReturnOwner...");
	mBtnReturnOwnerObjects->setCommitCallback(boost::bind(&LLPanelLandObjects::onClickReturnOwnerObjects, this));
	
	mGroupObjects = getChild<LLTextBox>("group_objects_text");
	mBtnShowGroupObjects = getChild<LLButton>("ShowGroup");
	mBtnShowGroupObjects->setCommitCallback(boost::bind(&LLPanelLandObjects::onClickShowGroupObjects, this));
	
	mBtnReturnGroupObjects = getChild<LLButton>("ReturnGroup...");
	mBtnReturnGroupObjects->setCommitCallback(boost::bind(&LLPanelLandObjects::onClickReturnGroupObjects, this));
	
	mOtherObjects = getChild<LLTextBox>("other_objects_text");
	mBtnShowOtherObjects = getChild<LLButton>("ShowOther");
	mBtnShowOtherObjects->setCommitCallback(boost::bind(&LLPanelLandObjects::onClickShowOtherObjects, this));
	
	mBtnReturnOtherObjects = getChild<LLButton>("ReturnOther...");
	mBtnReturnOtherObjects->setCommitCallback(boost::bind(&LLPanelLandObjects::onClickReturnOtherObjects, this));
	
	mSelectedObjects = getChild<LLTextBox>("selected_objects_text");
	mCleanOtherObjectsTime = getChild<LLLineEditor>("clean other time");

	mCleanOtherObjectsTime->setFocusLostCallback(boost::bind(onLostFocus, _1, this));
	mCleanOtherObjectsTime->setCommitCallback(boost::bind(&LLPanelLandObjects::onCommitClean, this));
	getChild<LLLineEditor>("clean other time")->setPrevalidate(LLTextValidate::validateNonNegativeS32);
	
	mBtnRefresh = getChild<LLButton>("Refresh List");
	mBtnRefresh->setCommitCallback(boost::bind(&LLPanelLandObjects::onClickRefresh, this));
	
	mBtnReturnOwnerList = getChild<LLButton>("Return objects...");
	mBtnReturnOwnerList->setCommitCallback(boost::bind(&LLPanelLandObjects::onClickReturnOwnerList, this));

	mIconAvatarOnline = LLUIImageList::getInstance()->getUIImage("icon_avatar_online.tga", 0);
	mIconAvatarOffline = LLUIImageList::getInstance()->getUIImage("icon_avatar_offline.tga", 0);
	mIconGroup = LLUIImageList::getInstance()->getUIImage("icon_group.tga", 0);

	mOwnerList = getChild<LLNameListCtrl>("owner list");
	mOwnerList->sortByColumnIndex(3, FALSE);
	getChild<LLUICtrl>("owner list")->setCommitCallback(boost::bind(&LLPanelLandObjects::onCommitList, this, _1));
	mOwnerList->setDoubleClickCallback(boost::bind(&LLPanelLandObjects::onDoubleClickOwner, this));
	mOwnerList->setContextMenu(LLScrollListCtrl::MENU_AVATAR);

	return TRUE;
}

// virtual
LLPanelLandObjects::~LLPanelLandObjects()
{ }

void LLPanelLandObjects::onDoubleClickOwner()
{
	LLScrollListItem* item = mOwnerList->getFirstSelected();
	if (item)
	{
		LLUUID owner_id = item->getUUID();
		// Look up the selected name, for future dialog box use.
		const LLScrollListCell* cell;
		cell = item->getColumn(1);
		if (!cell)
		{
			return;
		}
		// Is this a group?
		BOOL is_group = cell->getValue().asString() == OWNER_GROUP;
		if (is_group)
		{
			LLGroupActions::show(owner_id);
		}
		else
		{
			LLAvatarActions::showProfile(owner_id);
		}
	}
}

const LLStringExplicit zero_str("0");

// public
void LLPanelLandObjects::refresh()
{
	LLParcel *parcel = mParcel->getParcel();

	mBtnShowOwnerObjects->setEnabled(FALSE);
	mBtnShowGroupObjects->setEnabled(FALSE);
	mBtnShowOtherObjects->setEnabled(FALSE);
	mBtnReturnOwnerObjects->setEnabled(FALSE);
	mBtnReturnGroupObjects->setEnabled(FALSE);
	mBtnReturnOtherObjects->setEnabled(FALSE);
	mCleanOtherObjectsTime->setEnabled(FALSE);
	mBtnRefresh->			setEnabled(FALSE);
	mBtnReturnOwnerList->	setEnabled(FALSE);

	mSelectedOwners.clear();
	mOwnerList->deleteAllItems();
	mOwnerList->setEnabled(FALSE);

	if (!parcel || gDisconnected)
	{
		mSWTotalObjects->setTextArg("[COUNT]", zero_str);
		mSWTotalObjects->setTextArg("[TOTAL]", zero_str);
		mSWTotalObjects->setTextArg("[AVAILABLE]", zero_str);
		mObjectContribution->setTextArg("[COUNT]", zero_str);
		mTotalObjects->setTextArg("[COUNT]", zero_str);
		mOwnerObjects->setTextArg("[COUNT]", zero_str);
		mGroupObjects->setTextArg("[COUNT]", zero_str);
		mOtherObjects->setTextArg("[COUNT]", zero_str);
		mSelectedObjects->setTextArg("[COUNT]", zero_str);
	}
	else
	{
		S32 sw_max = parcel->getSimWideMaxPrimCapacity();
		S32 sw_total = parcel->getSimWidePrimCount();
		S32 max = ll_round(parcel->getMaxPrimCapacity() * parcel->getParcelPrimBonus());
		S32 total = parcel->getPrimCount();
		S32 owned = parcel->getOwnerPrimCount();
		S32 group = parcel->getGroupPrimCount();
		S32 other = parcel->getOtherPrimCount();
		S32 selected = parcel->getSelectedPrimCount();
		F32 parcel_object_bonus = parcel->getParcelPrimBonus();
		mOtherTime = parcel->getCleanOtherTime();

		// Can't have more than region max tasks, regardless of parcel
		// object bonus factor.
		LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
		if (region)
		{
			S32 max_tasks_per_region = (S32)region->getMaxTasks();
			sw_max = llmin(sw_max, max_tasks_per_region);
			max = llmin(max, max_tasks_per_region);
		}

		if (parcel_object_bonus != 1.0f)
		{
			mParcelObjectBonus->setVisible(TRUE);
			mParcelObjectBonus->setTextArg("[BONUS]", llformat("%.2f", parcel_object_bonus));
		}
		else
		{
			mParcelObjectBonus->setVisible(FALSE);
		}

		if (sw_total > sw_max)
		{
			mSWTotalObjects->setText(getString("objects_deleted_text"));
			mSWTotalObjects->setTextArg("[DELETED]", llformat("%d", sw_total - sw_max));
		}
		else
		{
			mSWTotalObjects->setText(getString("objects_available_text"));
			mSWTotalObjects->setTextArg("[AVAILABLE]", llformat("%d", sw_max - sw_total));
		}
		mSWTotalObjects->setTextArg("[COUNT]", llformat("%d", sw_total));
		mSWTotalObjects->setTextArg("[MAX]", llformat("%d", sw_max));

		mObjectContribution->setTextArg("[COUNT]", llformat("%d", max));
		mTotalObjects->setTextArg("[COUNT]", llformat("%d", total));
		mOwnerObjects->setTextArg("[COUNT]", llformat("%d", owned));
		mGroupObjects->setTextArg("[COUNT]", llformat("%d", group));
		mOtherObjects->setTextArg("[COUNT]", llformat("%d", other));
		mSelectedObjects->setTextArg("[COUNT]", llformat("%d", selected));
		mCleanOtherObjectsTime->setText(llformat("%d", mOtherTime));

		BOOL can_return_owned = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_GROUP_OWNED);
		BOOL can_return_group_set = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_GROUP_SET);
		BOOL can_return_other = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_NON_GROUP);

		if (can_return_owned || can_return_group_set || can_return_other)
		{
			if (owned && can_return_owned)
			{
				mBtnShowOwnerObjects->setEnabled(TRUE);
				mBtnReturnOwnerObjects->setEnabled(TRUE);
			}
			if (group && can_return_group_set)
			{
				mBtnShowGroupObjects->setEnabled(TRUE);
				mBtnReturnGroupObjects->setEnabled(TRUE);
			}
			if (other && can_return_other)
			{
				mBtnShowOtherObjects->setEnabled(TRUE);
				mBtnReturnOtherObjects->setEnabled(TRUE);
			}

			mCleanOtherObjectsTime->setEnabled(TRUE);
			mBtnRefresh->setEnabled(TRUE);
		}
	}
}

// virtual
void LLPanelLandObjects::draw()
{
	LLPanel::draw();
}

void send_other_clean_time_message(S32 parcel_local_id, S32 other_clean_time)
{
	LLMessageSystem *msg = gMessageSystem;

	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region) return;

	msg->newMessageFast(_PREHASH_ParcelSetOtherCleanTime);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addS32Fast(_PREHASH_OtherCleanTime, other_clean_time);

	msg->sendReliable(region->getHost());
}

void send_return_objects_message(S32 parcel_local_id, S32 return_type, 
								 uuid_list_t* owner_ids = NULL)
{
	LLMessageSystem *msg = gMessageSystem;

	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region) return;

	msg->newMessageFast(_PREHASH_ParcelReturnObjects);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addU32Fast(_PREHASH_ReturnType, (U32) return_type);

	// Dummy task id, not used
	msg->nextBlock("TaskIDs");
	msg->addUUID("TaskID", LLUUID::null);

	// Throw all return ids into the packet.
	// TODO: Check for too many ids.
	if (owner_ids)
	{
		uuid_list_t::iterator end = owner_ids->end();
		for (uuid_list_t::iterator it = owner_ids->begin();
			 it != end;
			 ++it)
		{
			msg->nextBlockFast(_PREHASH_OwnerIDs);
			msg->addUUIDFast(_PREHASH_OwnerID, (*it));
		}
	}
	else
	{
		msg->nextBlockFast(_PREHASH_OwnerIDs);
		msg->addUUIDFast(_PREHASH_OwnerID, LLUUID::null);
	}

	msg->sendReliable(region->getHost());
}

bool LLPanelLandObjects::callbackReturnOwnerObjects(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLParcel *parcel = mParcel->getParcel();
	if (0 == option)
	{
		if (parcel)
		{
			LLUUID owner_id = parcel->getOwnerID();	
			LLSD args;
			if (owner_id == gAgentID)
			{
				LLNotificationsUtil::add("OwnedObjectsReturned");
			}
			else
			{
				args["NAME"] = LLSLURL("agent", owner_id, "completename").getSLURLString();
				LLNotificationsUtil::add("OtherObjectsReturned", args);
			}
			send_return_objects_message(parcel->getLocalID(), RT_OWNER);
		}
	}

	LLSelectMgr::getInstance()->unhighlightAll();
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
	refresh();
	return false;
}

bool LLPanelLandObjects::callbackReturnGroupObjects(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLParcel *parcel = mParcel->getParcel();
	if (0 == option)
	{
		if (parcel)
		{
			std::string group_name;
			gCacheName->getGroupName(parcel->getGroupID(), group_name);
			LLSD args;
			args["GROUPNAME"] = group_name;
			LLNotificationsUtil::add("GroupObjectsReturned", args);
			send_return_objects_message(parcel->getLocalID(), RT_GROUP);
		}
	}
	LLSelectMgr::getInstance()->unhighlightAll();
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
	refresh();
	return false;
}

bool LLPanelLandObjects::callbackReturnOtherObjects(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLParcel *parcel = mParcel->getParcel();
	if (0 == option)
	{
		if (parcel)
		{
			LLNotificationsUtil::add("UnOwnedObjectsReturned");
			send_return_objects_message(parcel->getLocalID(), RT_OTHER);
		}
	}
	LLSelectMgr::getInstance()->unhighlightAll();
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
	refresh();
	return false;
}

bool LLPanelLandObjects::callbackReturnOwnerList(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLParcel *parcel = mParcel->getParcel();
	if (0 == option)
	{
		if (parcel)
		{
			// Make sure we have something selected.
			uuid_list_t::iterator selected = mSelectedOwners.begin();
			if (selected != mSelectedOwners.end())
			{
				LLSD args;
				if (mSelectedIsGroup)
				{
					args["GROUPNAME"] = mSelectedName;
					LLNotificationsUtil::add("GroupObjectsReturned", args);
				}
				else
				{
					args["NAME"] = mSelectedName;
					LLNotificationsUtil::add("OtherObjectsReturned2", args);
				}

				send_return_objects_message(parcel->getLocalID(), RT_LIST, &(mSelectedOwners));
			}
		}
	}
	LLSelectMgr::getInstance()->unhighlightAll();
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
	refresh();
	return false;
}

void LLPanelLandObjects::onClickReturnOwnerList()
{
	LLParcel* parcelp = mParcel->getParcel();
	if (!parcelp) return;

	// Make sure we have something selected.
	if (mSelectedOwners.empty())
	{
		return;
	}
	//uuid_list_t::iterator selected_itr = self->mSelectedOwners.begin();
	//if (selected_itr == self->mSelectedOwners.end()) return;

	send_parcel_select_objects(parcelp->getLocalID(), RT_LIST, &mSelectedOwners);

	LLSD args;
	args["NAME"] = mSelectedName;
	args["N"] = std::to_string(mSelectedCount);
	if (mSelectedIsGroup)
	{
		LLNotificationsUtil::add("ReturnObjectsDeededToGroup", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOwnerList, this, _1, _2));
	}
	else 
	{
		LLNotificationsUtil::add("ReturnObjectsOwnedByUser", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOwnerList, this, _1, _2));
	}
}

void LLPanelLandObjects::onClickRefresh()
{
	LLMessageSystem *msg = gMessageSystem;

	LLParcel* parcel = mParcel->getParcel();
	if (!parcel) return;

	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region) return;

	mBtnRefresh->setEnabled(false);

	// ready the list for results
	mOwnerList->deleteAllItems();
	mOwnerList->setCommentText(LLTrans::getString("Searching"));
	mOwnerList->setEnabled(FALSE);
	mFirstReply = TRUE;

	// send the message
	msg->newMessageFast(_PREHASH_ParcelObjectOwnersRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel->getLocalID());

	msg->sendReliable(region->getHost());
}

// static
void LLPanelLandObjects::processParcelObjectOwnersReply(LLMessageSystem *msg, void **)
{
	LLPanelLandObjects* self = LLFloaterLand::getCurrentPanelLandObjects();

	if (!self)
	{
		LL_WARNS() << "Received message for nonexistent LLPanelLandObject"
				<< LL_ENDL;
		return;
	}
	
	const LLFontGL* FONT = LLFontGL::getFontSansSerif();

	// Extract all of the owners.
	S32 rows = msg->getNumberOfBlocksFast(_PREHASH_Data);
	//uuid_list_t return_ids;
	LLUUID	owner_id;
	BOOL	is_group_owned;
	S32		object_count;
	U32		most_recent_time = 0;
	BOOL	is_online;
	std::string object_count_str;
	//BOOL b_need_refresh = FALSE;

	// If we were waiting for the first reply, clear the "Searching..." text.
	if (self->mFirstReply)
	{
		self->mOwnerList->deleteAllItems();
		self->mFirstReply = FALSE;
	}

	for(S32 i = 0; i < rows; ++i)
	{
		msg->getUUIDFast(_PREHASH_Data, _PREHASH_OwnerID,		owner_id,		i);
		msg->getBOOLFast(_PREHASH_Data, _PREHASH_IsGroupOwned,	is_group_owned,	i);
		msg->getS32Fast (_PREHASH_Data, _PREHASH_Count,			object_count,	i);
		msg->getBOOLFast(_PREHASH_Data, _PREHASH_OnlineStatus,	is_online,		i);
		if(msg->has("DataExtended"))
		{
			msg->getU32("DataExtended", "TimeStamp", most_recent_time, i);
		}

		if (owner_id.isNull())
		{
			continue;
		}

		LLNameListCtrl::NameItem item_params;
		item_params.value = owner_id;
		item_params.target = is_group_owned ? LLNameListCtrl::GROUP : LLNameListCtrl::INDIVIDUAL;

		if (is_group_owned)
		{
			item_params.columns.add().type("icon").value(self->mIconGroup->getName()).column("type");
			item_params.columns.add().value(OWNER_GROUP).font(FONT).column("online_status");
		}
		else if (is_online)
		{
			item_params.columns.add().type("icon").value(self->mIconAvatarOnline->getName()).column("type");
			item_params.columns.add().value(OWNER_ONLINE).font(FONT).column("online_status");
		}
		else  // offline
		{
			item_params.columns.add().type("icon").value(self->mIconAvatarOffline->getName()).column("type");
			item_params.columns.add().value(OWNER_OFFLINE).font(FONT).column("online_status");
		}

		// Placeholder for name.
		LLAvatarName av_name;
		LLAvatarNameCache::get(owner_id, &av_name);
		item_params.columns.add().value(av_name.getCompleteName()).font(FONT).column("name");

		object_count_str = llformat("%d", object_count);
		item_params.columns.add().value(object_count_str).font(FONT).column("count");
		item_params.columns.add().value(LLDate((time_t)most_recent_time)).font(FONT).column("mostrecent").type("date");

		self->mOwnerList->addNameItemRow(item_params);
		LL_DEBUGS() << "object owner " << owner_id << " (" << (is_group_owned ? "group" : "agent")
				<< ") owns " << object_count << " objects." << LL_ENDL;
	}

	// check for no results
	if (0 == self->mOwnerList->getItemCount())
	{
		self->mOwnerList->setCommentText(LLTrans::getString("NoneFound"));
	}
	else
	{
		self->mOwnerList->setEnabled(TRUE);
	}

	self->mBtnRefresh->setEnabled(true);
}

// static
void LLPanelLandObjects::onCommitList(LLUICtrl* ctrl)
{
	if (mOwnerList->getCanSelect() == FALSE)
	{
		return;
	}
	LLScrollListItem *item = mOwnerList->getFirstSelected();
	if (item)
	{
		// Look up the selected name, for future dialog box use.
		const LLScrollListCell* cell;
		cell = item->getColumn(1);
		if (!cell)
		{
			return;
		}
		// Is this a group?
		mSelectedIsGroup = cell->getValue().asString() == OWNER_GROUP;
		cell = item->getColumn(2);
		mSelectedName = cell->getValue().asString();
		cell = item->getColumn(3);
		mSelectedCount = std::stoi(cell->getValue().asString());

		// Set the selection, and enable the return button.
		mSelectedOwners.clear();
		mSelectedOwners.insert(item->getUUID());
		mBtnReturnOwnerList->setEnabled(TRUE);

		// Highlight this user's objects
		clickShowCore(this, RT_LIST, &mSelectedOwners);
	}
}

// static
void LLPanelLandObjects::clickShowCore(LLPanelLandObjects* self, S32 return_type, uuid_list_t* list)
{
	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), return_type, list);
}

void LLPanelLandObjects::onClickShowOwnerObjects()
{
	clickShowCore(this, RT_OWNER);
}

void LLPanelLandObjects::onClickShowGroupObjects()
{
	clickShowCore(this, (RT_GROUP));
}

void LLPanelLandObjects::onClickShowOtherObjects()
{
	clickShowCore(this, RT_OTHER);
}

void LLPanelLandObjects::onClickReturnOwnerObjects()
{
	S32 owned = 0;

	LLParcel* parcel = mParcel->getParcel();
	if (!parcel) return;

	owned = parcel->getOwnerPrimCount();

	send_parcel_select_objects(parcel->getLocalID(), RT_OWNER);

	LLUUID owner_id = parcel->getOwnerID();
	
	LLSD args;
	args["N"] = std::to_string(owned);

	if (owner_id == gAgent.getID())
	{
		LLNotificationsUtil::add("ReturnObjectsOwnedBySelf", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOwnerObjects, this, _1, _2));
	}
	else
	{
		args["NAME"] = LLSLURL("agent", owner_id, "completename").getSLURLString();
		LLNotificationsUtil::add("ReturnObjectsOwnedByUser", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOwnerObjects, this, _1, _2));
	}
}

void LLPanelLandObjects::onClickReturnGroupObjects()
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), RT_GROUP);

	std::string group_name;
	gCacheName->getGroupName(parcel->getGroupID(), group_name);
	
	LLSD args;
	args["NAME"] = group_name;
	args["N"] = llformat("%d", parcel->getGroupPrimCount());

	// create and show confirmation textbox
	LLNotificationsUtil::add("ReturnObjectsDeededToGroup", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnGroupObjects, this, _1, _2));
}

void LLPanelLandObjects::onClickReturnOtherObjects()
{
	S32 other = 0;

	LLParcel* parcel = mParcel->getParcel();
	if (!parcel) return;
	
	other = parcel->getOtherPrimCount();

	send_parcel_select_objects(parcel->getLocalID(), RT_OTHER);

	LLSD args;
	args["N"] = llformat("%d", other);
	
	if (parcel->getIsGroupOwned())
	{
		std::string group_name;
		gCacheName->getGroupName(parcel->getGroupID(), group_name);
		args["NAME"] = group_name;

		LLNotificationsUtil::add("ReturnObjectsNotOwnedByGroup", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects, this, _1, _2));
	}
	else
	{
		LLUUID owner_id = parcel->getOwnerID();

		if (owner_id == gAgent.getID())
		{
			LLNotificationsUtil::add("ReturnObjectsNotOwnedBySelf", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects, this, _1, _2));
		}
		else
		{
			args["NAME"] = LLSLURL("agent", owner_id, "completename").getSLURLString();
			LLNotificationsUtil::add("ReturnObjectsNotOwnedByUser", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects, this, _1, _2));
		}
	}
}

// static
void LLPanelLandObjects::onLostFocus(LLFocusableElement* caller, void* user_data)
{
	LLPanelLandObjects *lop = static_cast<LLPanelLandObjects*>(user_data);
	if (lop) lop->onCommitClean();
}

void LLPanelLandObjects::onCommitClean()
{
	LLParcel* parcel = mParcel->getParcel();
	if (parcel)
	{
		S32 return_time = std::stoi(mCleanOtherObjectsTime->getText());
		// Only send return time if it has changed
		if (return_time != mOtherTime)
		{
			mOtherTime = return_time;

		parcel->setCleanOtherTime(mOtherTime);
		send_other_clean_time_message(parcel->getLocalID(), mOtherTime);
	}
}
}


//---------------------------------------------------------------------------
// LLPanelLandOptions
//---------------------------------------------------------------------------

LLPanelLandOptions::LLPanelLandOptions(LLParcelSelectionHandle& parcel)
:	LLPanel(),
	mCheckEditObjects(NULL),
	mCheckEditGroupObjects(NULL),
	mCheckAllObjectEntry(NULL),
	mCheckGroupObjectEntry(NULL),
	mCheckEditLand(NULL),
	mCheckSafe(NULL),
	mCheckFly(NULL),
	mCheckGroupScripts(NULL),
	mCheckOtherScripts(NULL),
	mCheckShowDirectory(NULL),
	mCategoryCombo(NULL),
	mLandingTypeCombo(NULL),
	mSnapshotCtrl(NULL),
	mLocationText(NULL),
	mSetBtn(NULL),
	mClearBtn(NULL),
	mMatureCtrl(NULL),
	mPushRestrictionCtrl(NULL),
	mSeeAvatarsCtrl(NULL),
	mParcel(parcel)
{
}


BOOL LLPanelLandOptions::postBuild()
{
	mCheckEditObjects = getChild<LLCheckBoxCtrl>( "edit objects check");
	mCheckEditObjects->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));
	
	mCheckEditGroupObjects = getChild<LLCheckBoxCtrl>( "edit group objects check");
	mCheckEditGroupObjects->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	mCheckAllObjectEntry = getChild<LLCheckBoxCtrl>( "all object entry check");
	mCheckAllObjectEntry->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	mCheckGroupObjectEntry = getChild<LLCheckBoxCtrl>( "group object entry check");
	mCheckGroupObjectEntry->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));
	
	mCheckEditLand = getChild<LLCheckBoxCtrl>( "edit land check");
	mCheckEditLand->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));
	
	mCheckGroupScripts = getChild<LLCheckBoxCtrl>( "check group scripts");
	mCheckGroupScripts->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	
	mCheckFly = getChild<LLCheckBoxCtrl>( "check fly");
	mCheckFly->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	
	mCheckOtherScripts = getChild<LLCheckBoxCtrl>( "check other scripts");
	mCheckOtherScripts->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	
	mCheckSafe = getChild<LLCheckBoxCtrl>( "check safe");
	mCheckSafe->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	
	mPushRestrictionCtrl = getChild<LLCheckBoxCtrl>( "PushRestrictCheck");
	mPushRestrictionCtrl->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	mSeeAvatarsCtrl = getChild<LLCheckBoxCtrl>( "SeeAvatarsCheck");
	mSeeAvatarsCtrl->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	mCheckShowDirectory = getChild<LLCheckBoxCtrl>( "ShowDirectoryCheck");
	mCheckShowDirectory->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	
	mCategoryCombo = getChild<LLComboBox>( "land category");
	mCategoryCombo->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));
	

	mMatureCtrl = getChild<LLCheckBoxCtrl>( "MatureCheck");
	mMatureCtrl->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));
	
	if (gAgent.wantsPGOnly())
	{
		// Disable these buttons if they are PG (Teen) users
		mMatureCtrl->setVisible(FALSE);
		mMatureCtrl->setEnabled(FALSE);
	}
	
	
	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	if (mSnapshotCtrl)
	{
		mSnapshotCtrl->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));
		mSnapshotCtrl->setAllowNoTexture ( TRUE );
		mSnapshotCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
		mSnapshotCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
		mSnapshotCtrl->setNonImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
	}
	else
	{
		LL_WARNS() << "LLUICtrlFactory::getTexturePickerByName() returned NULL for 'snapshot_ctrl'" << LL_ENDL;
	}


	mLocationText = getChild<LLTextBox>("landing_point");

	mSetBtn = getChild<LLButton>("Set");
	mSetBtn->setCommitCallback(boost::bind(&LLPanelLandOptions::onClickSet, this));

	
	mClearBtn = getChild<LLButton>("Clear");
	mClearBtn->setCommitCallback(boost::bind(&LLPanelLandOptions::onClickClear, this));


	mLandingTypeCombo = getChild<LLComboBox>( "landing type");
	mLandingTypeCombo->setCommitCallback(boost::bind(&LLPanelLandOptions::onCommitAny, this));

	return TRUE;
}


// virtual
LLPanelLandOptions::~LLPanelLandOptions()
{ }


// virtual
void LLPanelLandOptions::refresh()
{
	refreshSearch();

	LLParcel *parcel = mParcel->getParcel();
	if (!parcel || gDisconnected)
	{
		mCheckEditObjects	->set(FALSE);
		mCheckEditObjects	->setEnabled(FALSE);

		mCheckEditGroupObjects	->set(FALSE);
		mCheckEditGroupObjects	->setEnabled(FALSE);

		mCheckAllObjectEntry	->set(FALSE);
		mCheckAllObjectEntry	->setEnabled(FALSE);

		mCheckGroupObjectEntry	->set(FALSE);
		mCheckGroupObjectEntry	->setEnabled(FALSE);
		
		mCheckEditLand		->set(FALSE);
		mCheckEditLand		->setEnabled(FALSE);

		mCheckSafe			->set(FALSE);
		mCheckSafe			->setEnabled(FALSE);

		mCheckFly			->set(FALSE);
		mCheckFly			->setEnabled(FALSE);

		mCheckGroupScripts	->set(FALSE);
		mCheckGroupScripts	->setEnabled(FALSE);

		mCheckOtherScripts	->set(FALSE);
		mCheckOtherScripts	->setEnabled(FALSE);

		mPushRestrictionCtrl->set(FALSE);
		mPushRestrictionCtrl->setEnabled(FALSE);

		mSeeAvatarsCtrl->set(TRUE);
		mSeeAvatarsCtrl->setEnabled(FALSE);

		mLandingTypeCombo->setCurrentByIndex(0);
		mLandingTypeCombo->setEnabled(FALSE);

		mSnapshotCtrl->setImageAssetID(LLUUID::null);
		mSnapshotCtrl->setEnabled(FALSE);

		mLocationText->setTextArg("[LANDING]", getString("landing_point_none"));
		mSetBtn->setEnabled(FALSE);
		mClearBtn->setEnabled(FALSE);

		mMatureCtrl->setEnabled(FALSE);
	}
	else
	{
		// something selected, hooray!

		// Display options
		BOOL can_change_options = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_OPTIONS);
		mCheckEditObjects	->set( parcel->getAllowModify() );
		mCheckEditObjects	->setEnabled( can_change_options );
		
		mCheckEditGroupObjects	->set( parcel->getAllowGroupModify() ||  parcel->getAllowModify());
		mCheckEditGroupObjects	->setEnabled( can_change_options && !parcel->getAllowModify() );		// If others edit is enabled, then this is explicitly enabled.

		mCheckAllObjectEntry	->set( parcel->getAllowAllObjectEntry() );
		mCheckAllObjectEntry	->setEnabled( can_change_options );

		mCheckGroupObjectEntry	->set( parcel->getAllowGroupObjectEntry() ||  parcel->getAllowAllObjectEntry());
		mCheckGroupObjectEntry	->setEnabled( can_change_options && !parcel->getAllowAllObjectEntry() );
		
		BOOL can_change_terraform = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_EDIT);
		mCheckEditLand		->set( parcel->getAllowTerraform() );
		mCheckEditLand		->setEnabled( can_change_terraform );
		
		mCheckSafe			->set( !parcel->getAllowDamage() );
		mCheckSafe			->setEnabled( can_change_options );

		mCheckFly			->set( parcel->getAllowFly() );
		mCheckFly			->setEnabled( can_change_options );

		mCheckGroupScripts	->set( parcel->getAllowGroupScripts() || parcel->getAllowOtherScripts());
		mCheckGroupScripts	->setEnabled( can_change_options && !parcel->getAllowOtherScripts());

		mCheckOtherScripts	->set( parcel->getAllowOtherScripts() );
		mCheckOtherScripts	->setEnabled( can_change_options );

		mPushRestrictionCtrl->set( parcel->getRestrictPushObject() );
		if(parcel->getRegionPushOverride())
		{
			mPushRestrictionCtrl->setLabel(getString("push_restrict_region_text"));
			mPushRestrictionCtrl->setEnabled(false);
			mPushRestrictionCtrl->set(TRUE);
		}
		else
		{
			mPushRestrictionCtrl->setLabel(getString("push_restrict_text"));
			mPushRestrictionCtrl->setEnabled(can_change_options);
		}

		mSeeAvatarsCtrl->set(parcel->getSeeAVs());
		mSeeAvatarsCtrl->setEnabled(can_change_options && parcel->getHaveNewParcelLimitData());

		BOOL can_change_landing_point = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, 
														GP_LAND_SET_LANDING_POINT);
		mLandingTypeCombo->setCurrentByIndex((S32)parcel->getLandingType());
		mLandingTypeCombo->setEnabled( can_change_landing_point );

		bool can_change_identity =
				LLViewerParcelMgr::isParcelModifiableByAgent(
					parcel, GP_LAND_CHANGE_IDENTITY);
		mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
		mSnapshotCtrl->setEnabled( can_change_identity );

		// find out where we're looking and convert that to an angle in degrees on a regular compass (not the internal representation)
		LLVector3 user_look_at = parcel->getUserLookAt();
		U32 user_look_at_angle = ( (U32)( ( atan2(user_look_at[1], -user_look_at[0]) + F_PI * 2 ) * RAD_TO_DEG + 0.5) - 90) % 360;

		LLVector3 pos = parcel->getUserLocation();
		if (pos.isExactlyZero())
		{
			mLocationText->setTextArg("[LANDING]", getString("landing_point_none"));
		}
		else
		{
			mLocationText->setTextArg("[LANDING]",llformat("%d, %d, %d (%d\xC2\xB0)",
														   ll_round(pos.mV[VX]),
														   ll_round(pos.mV[VY]),
		   												   ll_round(pos.mV[VZ]),
														   user_look_at_angle));
		}

		mSetBtn->setEnabled( can_change_landing_point );
		mClearBtn->setEnabled( can_change_landing_point );

		if (gAgent.wantsPGOnly())
		{
			// Disable these buttons if they are PG (Teen) users
			mMatureCtrl->setVisible(FALSE);
			mMatureCtrl->setEnabled(FALSE);
		}
		else
		{
			// not teen so fill in the data for the maturity control
			mMatureCtrl->setVisible(TRUE);
			LLStyle::Params style;
			style.image(LLUI::getUIImage(gFloaterView->getParentFloater(this)->getString("maturity_icon_moderate")));
			LLCheckBoxWithTBAcess* fullaccess_mature_ctrl = (LLCheckBoxWithTBAcess*)mMatureCtrl;
			fullaccess_mature_ctrl->getTextBox()->setText(LLStringExplicit(""));
			fullaccess_mature_ctrl->getTextBox()->appendImageSegment(style);
			fullaccess_mature_ctrl->getTextBox()->appendText(getString("mature_check_mature"), false);
			fullaccess_mature_ctrl->setToolTip(getString("mature_check_mature_tooltip"));
			fullaccess_mature_ctrl->reshape(fullaccess_mature_ctrl->getRect().getWidth(), fullaccess_mature_ctrl->getRect().getHeight(), FALSE);
			
			// they can see the checkbox, but its disposition depends on the 
			// state of the region
			LLViewerRegion* regionp = LLViewerParcelMgr::getInstance()->getSelectionRegion();
			if (regionp)
			{
				if (regionp->getSimAccess() == SIM_ACCESS_PG)
				{
					mMatureCtrl->setEnabled(FALSE);
					mMatureCtrl->set(FALSE);
				}
				else if (regionp->getSimAccess() == SIM_ACCESS_MATURE)
				{
					mMatureCtrl->setEnabled(can_change_identity);
					mMatureCtrl->set(parcel->getMaturePublish());
				}
				else if (regionp->getSimAccess() == SIM_ACCESS_ADULT)
				{
					mMatureCtrl->setEnabled(FALSE);
					mMatureCtrl->set(TRUE);
					mMatureCtrl->setLabel(getString("mature_check_adult"));
					mMatureCtrl->setToolTip(getString("mature_check_adult_tooltip"));
				}
			}
		}
	}
}

// virtual
void LLPanelLandOptions::draw()
{
	LLPanel::draw();
}


// private
void LLPanelLandOptions::refreshSearch()
{
	LLParcel *parcel = mParcel->getParcel();
	if (!parcel || gDisconnected)
	{
		mCheckShowDirectory->set(FALSE);
		mCheckShowDirectory->setEnabled(FALSE);

		const std::string& none_string = LLParcel::getCategoryString(LLParcel::C_NONE);
		mCategoryCombo->setValue(none_string);
		mCategoryCombo->setEnabled(FALSE);
		return;
	}

	LLViewerRegion* region =
			LLViewerParcelMgr::getInstance()->getSelectionRegion();

	bool can_change =
			LLViewerParcelMgr::isParcelModifiableByAgent(
				parcel, GP_LAND_FIND_PLACES)
			&& region
			&& !(region->getRegionFlag(REGION_FLAGS_BLOCK_PARCEL_SEARCH));

	// There is a bug with this panel whereby the Show Directory bit can be 
	// slammed off by the Region based on an override.  Since this data is cached
	// locally the change will not reflect in the panel, which could cause confusion
	// A workaround for this is to flip the bit off in the locally cached version
	// when we detect a mismatch case.
	if(!can_change && parcel->getParcelFlag(PF_SHOW_DIRECTORY))
	{
		parcel->setParcelFlag(PF_SHOW_DIRECTORY, FALSE);
	}
	BOOL show_directory = parcel->getParcelFlag(PF_SHOW_DIRECTORY);
	mCheckShowDirectory	->set(show_directory);

	// Set by string in case the order in UI doesn't match the order by index.
	LLParcel::ECategory cat = parcel->getCategory();
	const std::string& category_string = LLParcel::getCategoryString(cat);
	mCategoryCombo->setValue(category_string);

	std::string tooltip;
	bool enable_show_directory = false;
	// Parcels <= 128 square meters cannot be listed in search, in an
	// effort to reduce search spam from small parcels.  See also
	// the search crawler "grid-crawl.py" in secondlife.com/doc/app/search/ JC
	const S32 MIN_PARCEL_AREA_FOR_SEARCH = 128;
	bool large_enough = parcel->getArea() > MIN_PARCEL_AREA_FOR_SEARCH;
	if (large_enough)
	{
		if (can_change)
		{
			tooltip = getString("search_enabled_tooltip");
			enable_show_directory = true;
		}
		else
		{
			tooltip = getString("search_disabled_permissions_tooltip");
			enable_show_directory = false;
		}
	}
	else
	{
		// not large enough to include in search
		if (can_change)
		{
			if (show_directory)
			{
				// parcels that are too small, but are still in search for
				// legacy reasons, need to have the check box enabled so
				// the owner can delist the parcel. JC
				tooltip = getString("search_enabled_tooltip");
				enable_show_directory = true;
			}
			else
			{
				tooltip = getString("search_disabled_small_tooltip");
				enable_show_directory = false;
			}
		}
		else
		{
			// both too small and don't have permission, so just
			// show the permissions as the reason (which is probably
			// the more common case) JC
			tooltip = getString("search_disabled_permissions_tooltip");
			enable_show_directory = false;
		}
	}
	mCheckShowDirectory->setToolTip(tooltip);
	mCategoryCombo->setToolTip(tooltip);
	mCheckShowDirectory->setEnabled(enable_show_directory);
	mCategoryCombo->setEnabled(enable_show_directory);
}


void LLPanelLandOptions::onCommitAny()
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	BOOL create_objects		= mCheckEditObjects->get();
	BOOL create_group_objects	= mCheckEditGroupObjects->get() || mCheckEditObjects->get();
	BOOL all_object_entry		= mCheckAllObjectEntry->get();
	BOOL group_object_entry	= mCheckGroupObjectEntry->get() || mCheckAllObjectEntry->get();
	BOOL allow_terraform	= mCheckEditLand->get();
	BOOL allow_damage		= !mCheckSafe->get();
	BOOL allow_fly			= mCheckFly->get();
	BOOL allow_landmark		= TRUE; // cannot restrict landmark creation
	BOOL allow_other_scripts	= mCheckOtherScripts->get();
	BOOL allow_group_scripts	= mCheckGroupScripts->get() || allow_other_scripts;
	BOOL allow_publish		= FALSE;
	BOOL mature_publish		= mMatureCtrl->get();
	BOOL push_restriction	= mPushRestrictionCtrl->get();
	BOOL see_avs			= mSeeAvatarsCtrl->get();
	BOOL show_directory		= mCheckShowDirectory->get();
	// we have to get the index from a lookup, not from the position in the dropdown!
	S32  category_index		= LLParcel::getCategoryFromString(mCategoryCombo->getSelectedValue());
	S32  landing_type_index	= mLandingTypeCombo->getCurrentIndex();
	LLUUID snapshot_id		= mSnapshotCtrl->getImageAssetID();
	LLViewerRegion* region;
	region = LLViewerParcelMgr::getInstance()->getSelectionRegion();

	if (region && region->getAllowDamage())
	{	// Damage is allowed on the region - server will always allow scripts
		if ( (!allow_other_scripts && parcel->getParcelFlag(PF_ALLOW_OTHER_SCRIPTS)) ||
			 (!allow_group_scripts && parcel->getParcelFlag(PF_ALLOW_GROUP_SCRIPTS)) )
		{	// Don't allow turning off "Run Scripts" if damage is allowed in the region
			mCheckOtherScripts->set(parcel->getParcelFlag(PF_ALLOW_OTHER_SCRIPTS));	// Restore UI to actual settings
			mCheckGroupScripts->set(parcel->getParcelFlag(PF_ALLOW_GROUP_SCRIPTS));
			LLNotificationsUtil::add("UnableToDisableOutsideScripts");
			return;
		}
	}

	// Push data into current parcel
	parcel->setParcelFlag(PF_CREATE_OBJECTS, create_objects);
	parcel->setParcelFlag(PF_CREATE_GROUP_OBJECTS, create_group_objects);
	parcel->setParcelFlag(PF_ALLOW_ALL_OBJECT_ENTRY, all_object_entry);
	parcel->setParcelFlag(PF_ALLOW_GROUP_OBJECT_ENTRY, group_object_entry);
	parcel->setParcelFlag(PF_ALLOW_TERRAFORM, allow_terraform);
	parcel->setParcelFlag(PF_ALLOW_DAMAGE, allow_damage);
	parcel->setParcelFlag(PF_ALLOW_FLY, allow_fly);
	parcel->setParcelFlag(PF_ALLOW_LANDMARK, allow_landmark);
	parcel->setParcelFlag(PF_ALLOW_GROUP_SCRIPTS, allow_group_scripts);
	parcel->setParcelFlag(PF_ALLOW_OTHER_SCRIPTS, allow_other_scripts);
	parcel->setParcelFlag(PF_SHOW_DIRECTORY, show_directory);
	parcel->setParcelFlag(PF_ALLOW_PUBLISH, allow_publish);
	parcel->setParcelFlag(PF_MATURE_PUBLISH, mature_publish);
	parcel->setParcelFlag(PF_RESTRICT_PUSHOBJECT, push_restriction);
	parcel->setCategory((LLParcel::ECategory)category_index);
	parcel->setLandingType((LLParcel::ELandingType)landing_type_index);
	parcel->setSnapshotID(snapshot_id);
	parcel->setSeeAVs(see_avs);

	// Send current parcel data upstream to server
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

	// Might have changed properties, so let's redraw!
	refresh();
}

void LLPanelLandOptions::onClickSet()
{
	LLParcel* selected_parcel = mParcel->getParcel();
	if (!selected_parcel) return;

	LLParcel* agent_parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!agent_parcel) return;

	if (agent_parcel->getLocalID() != selected_parcel->getLocalID())
	{
		LLNotificationsUtil::add("MustBeInParcel");
		return;
	}

	LLVector3 pos_region = gAgent.getPositionAgent();
	selected_parcel->setUserLocation(pos_region);
	selected_parcel->setUserLookAt(gAgent.getFrameAgent().getAtAxis());

	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(selected_parcel);

	refresh();
}

void LLPanelLandOptions::onClickClear()
{
	LLParcel* selected_parcel = mParcel->getParcel();
	if (!selected_parcel) return;

	// yes, this magic number of 0,0,0 means that it is clear
	LLVector3 zero_vec(0.f, 0.f, 0.f);
	selected_parcel->setUserLocation(zero_vec);
	selected_parcel->setUserLookAt(zero_vec);

	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(selected_parcel);

	refresh();
}


//---------------------------------------------------------------------------
// LLPanelLandAccess
//---------------------------------------------------------------------------

LLPanelLandAccess::LLPanelLandAccess(LLParcelSelectionHandle& parcel)
	: LLPanel()
	, mListAccess(nullptr)
	, mListBanned(nullptr)
	, mParcel(parcel)
{
}


BOOL LLPanelLandAccess::postBuild()
{
	getChild<LLUICtrl>("public_access")->setCommitCallback(boost::bind(&LLPanelLandAccess::onCommitPublicAccess, this, _1));
	getChild<LLUICtrl>("limit_payment")->setCommitCallback(boost::bind(&LLPanelLandAccess::onCommitAny, this));
	getChild<LLUICtrl>("limit_age_verified")->setCommitCallback(boost::bind(&LLPanelLandAccess::onCommitAny, this));
	getChild<LLUICtrl>("GroupCheck")->setCommitCallback(boost::bind(&LLPanelLandAccess::onCommitGroupCheck, this, _1));
	getChild<LLUICtrl>("PassCheck")->setCommitCallback(boost::bind(&LLPanelLandAccess::onCommitAny, this));
	getChild<LLUICtrl>("pass_combo")->setCommitCallback(boost::bind(&LLPanelLandAccess::onCommitAny, this));
	getChild<LLUICtrl>("PriceSpin")->setCommitCallback(boost::bind(&LLPanelLandAccess::onCommitAny, this));
	getChild<LLUICtrl>("HoursSpin")->setCommitCallback(boost::bind(&LLPanelLandAccess::onCommitAny, this));

	childSetAction("add_allowed", boost::bind(&LLPanelLandAccess::onClickAddAccess, this));
	getChild<LLUICtrl>("remove_allowed")->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickRemoveAccess, this));
	childSetAction("add_banned", boost::bind(&LLPanelLandAccess::onClickAddBanned, this));
	getChild<LLUICtrl>("remove_banned")->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickRemoveBanned, this));
	
	mListAccess = getChild<LLNameListCtrl>("AccessList");
	if (mListAccess)
	{
		mListAccess->sortByColumnIndex(0, TRUE); // ascending
		mListAccess->setContextMenu(LLScrollListCtrl::MENU_AVATAR);
	}

	mListBanned = getChild<LLNameListCtrl>("BannedList");
	if (mListBanned)
	{
		mListBanned->sortByColumnIndex(0, TRUE); // ascending
		mListBanned->setContextMenu(LLScrollListCtrl::MENU_AVATAR);
	}

	return TRUE;
}


LLPanelLandAccess::~LLPanelLandAccess()
{ 
}

void LLPanelLandAccess::refresh()
{
	LLFloater* parent_floater = gFloaterView->getParentFloater(this);
	LLParcel *parcel = mParcel->getParcel();
		
	// Display options
	if (parcel && !gDisconnected)
	{
		BOOL use_access_list = parcel->getParcelFlag(PF_USE_ACCESS_LIST);
		BOOL use_group = parcel->getParcelFlag(PF_USE_ACCESS_GROUP);
		BOOL public_access = !use_access_list;
		
        if (parcel->getRegionAllowAccessOverride())
        {
            getChild<LLUICtrl>("public_access")->setValue(public_access);
            getChild<LLUICtrl>("GroupCheck")->setValue(use_group);
        }
        else
        {
            getChild<LLUICtrl>("public_access")->setValue(TRUE);
            getChild<LLUICtrl>("GroupCheck")->setValue(FALSE);
        }
		std::string group_name;
		gCacheName->getGroupName(parcel->getGroupID(), group_name);
		getChild<LLUICtrl>("GroupCheck")->setLabelArg("[GROUP]", group_name );
		
		// Allow list
		if (mListAccess)
		{
			// Clear the sort order so we don't re-sort on every add.
			mListAccess->clearSortOrder();
			mListAccess->deleteAllItems();
			S32 count = parcel->mAccessList.size();
			getChild<LLUICtrl>("AllowedText")->setTextArg("[COUNT]", llformat("%d",count));
			getChild<LLUICtrl>("AllowedText")->setTextArg("[MAX]", llformat("%d",PARCEL_MAX_ACCESS_LIST));

			getChild<LLUICtrl>("AccessList")->setToolTipArg(LLStringExplicit("[LISTED]"), llformat("%d",count));
			getChild<LLUICtrl>("AccessList")->setToolTipArg(LLStringExplicit("[MAX]"), llformat("%d",PARCEL_MAX_ACCESS_LIST));

			for (LLAccessEntry::map::const_iterator cit = parcel->mAccessList.begin();
				 cit != parcel->mAccessList.end(); ++cit)
			{
				const LLAccessEntry& entry = (*cit).second;
				std::string suffix;
				if (entry.mTime != 0)
				{
					LLStringUtil::format_map_t args;
					S32 now = time(NULL);
					S32 seconds = entry.mTime - now;
					if (seconds < 0) seconds = 0;
					suffix.assign(" (");
					if (seconds >= 120)
					{
						args["[MINUTES]"] = llformat("%d", (seconds/60));
						std::string buf = parent_floater->getString ("Minutes", args);
						suffix.append(buf);
					}
					else if (seconds >= 60)
					{
						suffix.append("1 " + parent_floater->getString("Minute"));
					}
					else
					{
						args["[SECONDS]"] = llformat("%d", seconds);
						std::string buf = parent_floater->getString ("Seconds", args);
						suffix.append(buf);
					}
					suffix.append(" " + parent_floater->getString("Remaining") + ")");
				}
				mListAccess->addNameItem(entry.mID, ADD_DEFAULT, TRUE, suffix);
			}
			mListAccess->sortByName(TRUE);
		}
		
		// Ban List
		if(mListBanned)
		{
			// Clear the sort order so we don't re-sort on every add.
			mListBanned->clearSortOrder();
			mListBanned->deleteAllItems();
			S32 count = parcel->mBanList.size();
			getChild<LLUICtrl>("BanCheck")->setTextArg("[COUNT]", llformat("%d",count));
			getChild<LLUICtrl>("BanCheck")->setTextArg("[MAX]", llformat("%d",PARCEL_MAX_ACCESS_LIST));

			getChild<LLUICtrl>("BannedList")->setToolTipArg(LLStringExplicit("[LISTED]"), llformat("%d",count));
			getChild<LLUICtrl>("BannedList")->setToolTipArg(LLStringExplicit("[MAX]"), llformat("%d",PARCEL_MAX_ACCESS_LIST));

			for (LLAccessEntry::map::const_iterator cit = parcel->mBanList.begin();
				 cit != parcel->mBanList.end(); ++cit)
			{
				const LLAccessEntry& entry = (*cit).second;
				std::string suffix;
				if (entry.mTime != 0)
				{
					LLStringUtil::format_map_t args;
					S32 now = time(NULL);
					S32 seconds = entry.mTime - now;
					if (seconds < 0) seconds = 0;
					suffix.assign(" (");
					if (seconds >= 120)
					{
						args["[MINUTES]"] = llformat("%d", (seconds/60));
						std::string buf = parent_floater->getString ("Minutes", args);
						suffix.append(buf);
					}
					else if (seconds >= 60)
					{
						suffix.append("1 " + parent_floater->getString("Minute"));
					}
					else
					{
						args["[SECONDS]"] = llformat("%d", seconds);
						std::string buf = parent_floater->getString ("Seconds", args);
						suffix.append(buf);
					}
					suffix.append(" " + parent_floater->getString("Remaining") + ")");
				}
				mListBanned->addNameItem(entry.mID, ADD_DEFAULT, TRUE, suffix);
			}
			mListBanned->sortByName(TRUE);
		}

		if(parcel->getRegionDenyAnonymousOverride())
		{
			getChild<LLUICtrl>("limit_payment")->setValue(TRUE);
			getChild<LLUICtrl>("limit_payment")->setLabelArg("[ESTATE_PAYMENT_LIMIT]", getString("access_estate_defined") );
		}
		else
		{
			getChild<LLUICtrl>("limit_payment")->setValue((parcel->getParcelFlag(PF_DENY_ANONYMOUS)));
			getChild<LLUICtrl>("limit_payment")->setLabelArg("[ESTATE_PAYMENT_LIMIT]", std::string() );
		}
		if(parcel->getRegionDenyAgeUnverifiedOverride())
		{
			getChild<LLUICtrl>("limit_age_verified")->setValue(TRUE);
			getChild<LLUICtrl>("limit_age_verified")->setLabelArg("[ESTATE_AGE_LIMIT]", getString("access_estate_defined") );
		}
		else
		{
			getChild<LLUICtrl>("limit_age_verified")->setValue((parcel->getParcelFlag(PF_DENY_AGEUNVERIFIED)));
			getChild<LLUICtrl>("limit_age_verified")->setLabelArg("[ESTATE_AGE_LIMIT]", std::string() );
		}
		
		BOOL use_pass = parcel->getParcelFlag(PF_USE_PASS_LIST);
		getChild<LLUICtrl>("PassCheck")->setValue(use_pass);
		LLCtrlSelectionInterface* passcombo = childGetSelectionInterface("pass_combo");
		if (passcombo)
		{
			if (public_access || !use_pass)
			{
				passcombo->selectByValue("anyone");
			}
		}
		
		S32 pass_price = parcel->getPassPrice();
		getChild<LLUICtrl>("PriceSpin")->setValue((F32)pass_price );

		F32 pass_hours = parcel->getPassHours();
		getChild<LLUICtrl>("HoursSpin")->setValue(pass_hours );
	}
	else
	{
		getChild<LLUICtrl>("public_access")->setValue(FALSE);
		getChild<LLUICtrl>("limit_payment")->setValue(FALSE);
		getChild<LLUICtrl>("limit_age_verified")->setValue(FALSE);
		getChild<LLUICtrl>("GroupCheck")->setValue(FALSE);
		getChild<LLUICtrl>("GroupCheck")->setLabelArg("[GROUP]", LLStringUtil::null );
		getChild<LLUICtrl>("PassCheck")->setValue(FALSE);
		getChild<LLUICtrl>("PriceSpin")->setValue((F32)PARCEL_PASS_PRICE_DEFAULT);
		getChild<LLUICtrl>("HoursSpin")->setValue(PARCEL_PASS_HOURS_DEFAULT );
		getChild<LLUICtrl>("AccessList")->setToolTipArg(LLStringExplicit("[LISTED]"), zero_str);
		getChild<LLUICtrl>("AccessList")->setToolTipArg(LLStringExplicit("[MAX]"), zero_str);
		getChild<LLUICtrl>("BannedList")->setToolTipArg(LLStringExplicit("[LISTED]"), zero_str);
		getChild<LLUICtrl>("BannedList")->setToolTipArg(LLStringExplicit("[MAX]"), zero_str);
	}	
}

void LLPanelLandAccess::refresh_ui()
{
	getChildView("public_access")->setEnabled(FALSE);
	getChildView("limit_payment")->setEnabled(FALSE);
	getChildView("limit_age_verified")->setEnabled(FALSE);
	getChildView("GroupCheck")->setEnabled(FALSE);
	getChildView("PassCheck")->setEnabled(FALSE);
	getChildView("pass_combo")->setEnabled(FALSE);
	getChildView("PriceSpin")->setEnabled(FALSE);
	getChildView("HoursSpin")->setEnabled(FALSE);
	getChildView("AccessList")->setEnabled(FALSE);
	getChildView("BannedList")->setEnabled(FALSE);
	getChildView("add_allowed")->setEnabled(FALSE);
	getChildView("remove_allowed")->setEnabled(FALSE);
	getChildView("add_banned")->setEnabled(FALSE);
	getChildView("remove_banned")->setEnabled(FALSE);
	
	LLParcel *parcel = mParcel->getParcel();
	if (parcel && !gDisconnected)
	{
        BOOL can_manage_allowed = false;
		BOOL can_manage_banned = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_MANAGE_BANNED);
	
        if (parcel->getRegionAllowAccessOverride())
        {   // Estate owner may have disabled allowing the parcel owner from managing access.
            can_manage_allowed = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_MANAGE_ALLOWED);
        }

		getChildView("public_access")->setEnabled(can_manage_allowed);
		BOOL public_access = getChild<LLUICtrl>("public_access")->getValue().asBoolean();
		if (public_access)
		{
			bool override = false;
			if(parcel->getRegionDenyAnonymousOverride())
			{
				override = true;
				getChildView("limit_payment")->setEnabled(FALSE);
			}
			else
			{
				getChildView("limit_payment")->setEnabled(can_manage_allowed);
			}
			if(parcel->getRegionDenyAgeUnverifiedOverride())
			{
				override = true;
				getChildView("limit_age_verified")->setEnabled(FALSE);
			}
			else
			{
				getChildView("limit_age_verified")->setEnabled(can_manage_allowed);
			}
			if (override)
			{
				getChildView("Only Allow")->setToolTip(getString("estate_override"));
			}
			else
			{
				getChildView("Only Allow")->setToolTip(std::string());
			}
			getChildView("PassCheck")->setEnabled(FALSE);
			getChildView("pass_combo")->setEnabled(FALSE);
			getChildView("AccessList")->setEnabled(FALSE);
		}
		else
		{
			getChildView("limit_payment")->setEnabled(FALSE);
			getChildView("limit_age_verified")->setEnabled(FALSE);


			BOOL sell_passes = getChild<LLUICtrl>("PassCheck")->getValue().asBoolean();
			getChildView("PassCheck")->setEnabled(can_manage_allowed);
			if (sell_passes)
			{
				getChildView("pass_combo")->setEnabled(can_manage_allowed);
				getChildView("PriceSpin")->setEnabled(can_manage_allowed);
				getChildView("HoursSpin")->setEnabled(can_manage_allowed);
			}
		}
		std::string group_name;
		if (gCacheName->getGroupName(parcel->getGroupID(), group_name))
		{
			bool can_allow_groups = !public_access || (public_access && (getChild<LLUICtrl>("limit_payment")->getValue().asBoolean() ^ getChild<LLUICtrl>("limit_age_verified")->getValue().asBoolean()));
			getChildView("GroupCheck")->setEnabled(can_manage_allowed && can_allow_groups);
		}
		getChildView("AccessList")->setEnabled(can_manage_allowed);
		S32 allowed_list_count = parcel->mAccessList.size();
		getChildView("add_allowed")->setEnabled(can_manage_allowed && allowed_list_count < PARCEL_MAX_ACCESS_LIST);
		BOOL has_selected = (mListAccess && mListAccess->getSelectionInterface()->getFirstSelectedIndex() >= 0);
		getChildView("remove_allowed")->setEnabled(can_manage_allowed && has_selected);
		
		getChildView("BannedList")->setEnabled(can_manage_banned);
		S32 banned_list_count = parcel->mBanList.size();
		getChildView("add_banned")->setEnabled(can_manage_banned && banned_list_count < PARCEL_MAX_ACCESS_LIST);
		has_selected = (mListBanned && mListBanned->getSelectionInterface()->getFirstSelectedIndex() >= 0);
		getChildView("remove_banned")->setEnabled(can_manage_banned && has_selected);
	}
}
		

// public
void LLPanelLandAccess::refreshNames()
{
	LLParcel* parcel = mParcel->getParcel();
	std::string group_name;
	if(parcel)
	{
		gCacheName->getGroupName(parcel->getGroupID(), group_name);
	}
	getChild<LLUICtrl>("GroupCheck")->setLabelArg("[GROUP]", group_name);
}


// virtual
void LLPanelLandAccess::draw()
{
	refresh_ui();
	refreshNames();
	LLPanel::draw();
}

void LLPanelLandAccess::onCommitPublicAccess(LLUICtrl *ctrl)
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	onCommitAny();
}

void LLPanelLandAccess::onCommitGroupCheck(LLUICtrl *ctrl)
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	BOOL use_pass_list = !getChild<LLUICtrl>("public_access")->getValue().asBoolean();
	BOOL use_access_group = getChild<LLUICtrl>("GroupCheck")->getValue().asBoolean();
	LLCtrlSelectionInterface* passcombo = childGetSelectionInterface("pass_combo");
	if (passcombo && use_access_group && use_pass_list)
	{
		if (passcombo->getSelectedValue().asString() == "group")
		{
			passcombo->selectByValue("anyone");
		}
	}

	onCommitAny();
}

void LLPanelLandAccess::onCommitAny()
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	BOOL public_access = getChild<LLUICtrl>("public_access")->getValue().asBoolean();
	BOOL use_access_group = getChild<LLUICtrl>("GroupCheck")->getValue().asBoolean();
	if (use_access_group)
	{
		std::string group_name;
		if (!gCacheName->getGroupName(parcel->getGroupID(), group_name))
		{
			use_access_group = FALSE;
		}
	}
		
	BOOL limit_payment = FALSE, limit_age_verified = FALSE;
	BOOL use_access_list = FALSE;
	BOOL use_pass_list = FALSE;
	
	if (public_access)
	{
		use_access_list = FALSE;
		limit_payment = getChild<LLUICtrl>("limit_payment")->getValue().asBoolean();
		limit_age_verified = getChild<LLUICtrl>("limit_age_verified")->getValue().asBoolean();
	}
	else
	{
		use_access_list = TRUE;
		use_pass_list = getChild<LLUICtrl>("PassCheck")->getValue().asBoolean();
		LLCtrlSelectionInterface* passcombo = childGetSelectionInterface("pass_combo");
		if (passcombo)
		{
			if (use_access_group && use_pass_list)
			{
				if (passcombo->getSelectedValue().asString() == "group")
				{
					use_access_group = FALSE;
				}
			}
		}
	}

	S32 pass_price = llfloor((F32)getChild<LLUICtrl>("PriceSpin")->getValue().asReal());
	F32 pass_hours = (F32)getChild<LLUICtrl>("HoursSpin")->getValue().asReal();

	// Push data into current parcel
	parcel->setParcelFlag(PF_USE_ACCESS_GROUP,	use_access_group);
	parcel->setParcelFlag(PF_USE_ACCESS_LIST,	use_access_list);
	parcel->setParcelFlag(PF_USE_PASS_LIST,		use_pass_list);
	parcel->setParcelFlag(PF_USE_BAN_LIST,		TRUE);
	parcel->setParcelFlag(PF_DENY_ANONYMOUS, 	limit_payment);
	parcel->setParcelFlag(PF_DENY_AGEUNVERIFIED, limit_age_verified);

	parcel->setPassPrice( pass_price );
	parcel->setPassHours( pass_hours );

	// Send current parcel data upstream to server
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

	// Might have changed properties, so let's redraw!
	refresh();
}

void LLPanelLandAccess::onClickAddAccess()
{
    LLView * button = findChild<LLButton>("add_allowed");
    LLFloater * root_floater = gFloaterView->getParentFloater(this);
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(
		boost::bind(&LLPanelLandAccess::callbackAvatarCBAccess, this, _1), FALSE, FALSE, FALSE, root_floater->getName(), button);
	if (picker)
	{
		root_floater->addDependentFloater(picker);
	}
}

void LLPanelLandAccess::callbackAvatarCBAccess(const uuid_vec_t& ids)
{
	if (!ids.empty())
	{
		LLUUID id = ids[0];
		LLParcel* parcel = mParcel->getParcel();
		if (parcel && parcel->addToAccessList(id, 0))
		{
			U32 lists_to_update = AL_ACCESS;
			// agent was successfully added to access list
			// but we also need to check ban list to ensure that agent will not be in two lists simultaneously
			if(parcel->removeFromBanList(id))
			{
				lists_to_update |= AL_BAN;
			}
			LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(lists_to_update);
			refresh();
		}
	}
}

// static
void LLPanelLandAccess::onClickRemoveAccess()
{
	if (mListAccess)
	{
		LLParcel* parcel = mParcel->getParcel();
		if (parcel)
		{
			std::vector<LLScrollListItem*> names = mListAccess->getAllSelected();
			for (std::vector<LLScrollListItem*>::iterator iter = names.begin();
				 iter != names.end(); )
			{
				LLScrollListItem* item = *iter++;
				const LLUUID& agent_id = item->getUUID();
				parcel->removeFromAccessList(agent_id);
			}
			LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(AL_ACCESS);
			refresh();
		}
	}
}

// static
void LLPanelLandAccess::onClickAddBanned()
{
    LLView * button = findChild<LLButton>("add_banned");
    LLFloater * root_floater = gFloaterView->getParentFloater(this);
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(
		boost::bind(&LLPanelLandAccess::callbackAvatarCBBanned, this, _1), FALSE, FALSE, FALSE, root_floater->getName(), button);
	if (picker)
	{
		root_floater->addDependentFloater(picker);
	}
}

// static
void LLPanelLandAccess::callbackAvatarCBBanned(const uuid_vec_t& ids)
{
	if (!ids.empty())
	{
		LLUUID id = ids[0];
		LLParcel* parcel = mParcel->getParcel();
		if (parcel && parcel->addToBanList(id, 0))
		{
			U32 lists_to_update = AL_BAN;
			// agent was successfully added to ban list
			// but we also need to check access list to ensure that agent will not be in two lists simultaneously
			if (parcel->removeFromAccessList(id))
			{
				lists_to_update |= AL_ACCESS;
			}
			LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(lists_to_update);
			refresh();
		}
	}
}

void LLPanelLandAccess::onClickRemoveBanned()
{
	if (mListBanned)
	{
		LLParcel* parcel = mParcel->getParcel();
		if (parcel)
		{
			std::vector<LLScrollListItem*> names = mListBanned->getAllSelected();
			for (std::vector<LLScrollListItem*>::iterator iter = names.begin();
				 iter != names.end(); )
			{
				LLScrollListItem* item = *iter++;
				const LLUUID& agent_id = item->getUUID();
				parcel->removeFromBanList(agent_id);
			}
			LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(AL_BAN);
			refresh();
		}
	}
}

//---------------------------------------------------------------------------
// LLPanelLandCovenant
//---------------------------------------------------------------------------
LLPanelLandCovenant::LLPanelLandCovenant(LLParcelSelectionHandle& parcel)
	: LLPanel(),
	  mParcel(parcel),
	  mNextUpdateTime(0)
{
}

LLPanelLandCovenant::~LLPanelLandCovenant()
{
}

BOOL LLPanelLandCovenant::postBuild()
{
	mLastRegionID = LLUUID::null;
	mNextUpdateTime = 0;

	return TRUE;
}

// virtual
void LLPanelLandCovenant::refresh()
{
	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if(!region || gDisconnected) return;
		
	LLTextBox* region_name = getChild<LLTextBox>("region_name_text");
	if (region_name)
	{
		region_name->setText(region->getName());
	}

	LLTextBox* region_landtype = getChild<LLTextBox>("region_landtype_text");
	region_landtype->setText(region->getLocalizedSimProductName());
	
	LLTextBox* region_maturity = getChild<LLTextBox>("region_maturity_text");
	if (region_maturity)
	{
		insert_maturity_into_textbox(region_maturity, gFloaterView->getParentFloater(this), MATURITY);
	}
	
	LLTextBox* resellable_clause = getChild<LLTextBox>("resellable_clause");
	if (resellable_clause)
	{
		if (region->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL))
		{
			resellable_clause->setText(getString("can_not_resell"));
		}
		else
		{
			resellable_clause->setText(getString("can_resell"));
		}
	}
	
	LLTextBox* changeable_clause = getChild<LLTextBox>("changeable_clause");
	if (changeable_clause)
	{
		if (region->getRegionFlag(REGION_FLAGS_ALLOW_PARCEL_CHANGES))
		{
			changeable_clause->setText(getString("can_change"));
		}
		else
		{
			changeable_clause->setText(getString("can_not_change"));
		}
	}

	if (mLastRegionID != region->getRegionID()
		|| mNextUpdateTime < LLTimer::getElapsedSeconds())
	{
		// Request Covenant Info
		// Note: LLPanelLandCovenant doesn't change Covenant's content and any
		// changes made by Estate floater should be requested by Estate floater
		LLMessageSystem *msg = gMessageSystem;
		msg->newMessage("EstateCovenantRequest");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
		msg->sendReliable(region->getHost());

		mLastRegionID = region->getRegionID();
		mNextUpdateTime = LLTimer::getElapsedSeconds() + COVENANT_REFRESH_TIME_SEC;
	}
}

// static
void LLPanelLandCovenant::updateCovenantText(const std::string &string)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		LLViewerTextEditor* editor = self->getChild<LLViewerTextEditor>("covenant_editor");
		editor->setText(string);
	}
}

// static
void LLPanelLandCovenant::updateEstateName(const std::string& name)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		LLTextBox* editor = self->getChild<LLTextBox>("estate_name_text");
		if (editor) editor->setText(name);
	}
}

// static
void LLPanelLandCovenant::updateLastModified(const std::string& text)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		LLTextBox* editor = self->getChild<LLTextBox>("covenant_timestamp_text");
		if (editor) editor->setText(text);
	}
}

// static
void LLPanelLandCovenant::updateEstateOwnerName(const std::string& name)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		LLTextBox* editor = self->getChild<LLTextBox>("estate_owner_text");
		if (editor) editor->setText(name);
	}
}

// inserts maturity info(icon and text) into target textbox 
// names_floater - pointer to floater which contains strings with maturity icons filenames
// str_to_parse is string in format "txt1[MATURITY]txt2" where maturity icon and text will be inserted instead of [MATURITY]
void insert_maturity_into_textbox(LLTextBox* target_textbox, LLFloater* names_floater, std::string str_to_parse)
{
	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region)
		return;

	LLStyle::Params style;

	U8 sim_access = region->getSimAccess();

	switch(sim_access)
	{
	case SIM_ACCESS_PG:
		style.image(LLUI::getUIImage(names_floater->getString("maturity_icon_general")));
		break;

	case SIM_ACCESS_ADULT:
		style.image(LLUI::getUIImage(names_floater->getString("maturity_icon_adult")));
		break;

	case SIM_ACCESS_MATURE:
		style.image(LLUI::getUIImage(names_floater->getString("maturity_icon_moderate")));
		break;

	default:
		break;
	}

	size_t maturity_pos = str_to_parse.find(MATURITY);
	
	if (maturity_pos == std::string::npos)
	{
		return;
	}

	std::string text_before_rating = str_to_parse.substr(0, maturity_pos);
	std::string text_after_rating = str_to_parse.substr(maturity_pos + MATURITY.length());

	target_textbox->setText(text_before_rating);

	target_textbox->appendImageSegment(style);

	target_textbox->appendText(LLViewerParcelMgr::getInstance()->getSelectionRegion()->getSimAccessString(), false);
	target_textbox->appendText(text_after_rating, false);
}

LLPanelLandExperiences::LLPanelLandExperiences( LLSafeHandle<LLParcelSelection>& parcelp ) 
	: mParcel(parcelp),
	  mAllowed(NULL),
	  mBlocked(NULL)
{

}


BOOL LLPanelLandExperiences::postBuild()
{
	mAllowed = setupList("panel_allowed", EXPERIENCE_KEY_TYPE_ALLOWED, AL_ALLOW_EXPERIENCE);
	mBlocked = setupList("panel_blocked", EXPERIENCE_KEY_TYPE_BLOCKED, AL_BLOCK_EXPERIENCE);

	// only non-grid-wide experiences
	mAllowed->addFilter(boost::bind(LLPanelExperiencePicker::FilterWithProperty, _1, LLExperienceCache::PROPERTY_GRID));

	// no privileged ones
	mBlocked->addFilter(boost::bind(LLPanelExperiencePicker::FilterWithoutProperties, _1, LLExperienceCache::PROPERTY_PRIVILEGED|LLExperienceCache::PROPERTY_GRID));

	getChild<LLLayoutPanel>("trusted_layout_panel")->setVisible(FALSE);
	getChild<LLTextBox>("experiences_help_text")->setVisible(FALSE);
	getChild<LLTextBox>("allowed_text_help")->setText(getString("allowed_parcel_text"));
	getChild<LLTextBox>("blocked_text_help")->setText(getString("blocked_parcel_text"));
	
	return LLPanel::postBuild();
}

LLPanelExperienceListEditor* LLPanelLandExperiences::setupList( const char* control_name, U32 xp_type, U32 access_type )
{
	LLPanelExperienceListEditor* child = findChild<LLPanelExperienceListEditor>(control_name);
	if(child)
	{
		child->getChild<LLTextBox>("text_name")->setText(child->getString(control_name));
		child->setMaxExperienceIDs(PARCEL_MAX_EXPERIENCE_LIST);
		child->setAddedCallback(boost::bind(&LLPanelLandExperiences::experienceAdded, this, _1, xp_type, access_type));
		child->setRemovedCallback(boost::bind(&LLPanelLandExperiences::experienceRemoved, this, _1, access_type));
	}

	return child;
}

void LLPanelLandExperiences::experienceAdded( const LLUUID& id, U32 xp_type, U32 access_type )
{
	LLParcel* parcel = mParcel->getParcel();
	if (parcel)
	{			
		parcel->setExperienceKeyType(id, xp_type);
		LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(access_type);
		refresh();
	}
}

void LLPanelLandExperiences::experienceRemoved( const LLUUID& id, U32 access_type )
{
	LLParcel* parcel = mParcel->getParcel();
	if (parcel)
	{			
		parcel->setExperienceKeyType(id, EXPERIENCE_KEY_TYPE_NONE);
		LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(access_type);
		refresh();
	}
}

void LLPanelLandExperiences::refreshPanel(LLPanelExperienceListEditor* panel, U32 xp_type)
{
	LLParcel *parcel = mParcel->getParcel();

	// Display options
	if (panel == NULL)
	{
		return;
	}
	if (!parcel || gDisconnected)
	{
		// disable the panel
		panel->setEnabled(FALSE);
		panel->setExperienceIds(LLSD::emptyArray());
	}
	else
	{
		// enable the panel
		panel->setEnabled(TRUE);
		LLAccessEntry::map entries = parcel->getExperienceKeysByType(xp_type);
		LLAccessEntry::map::iterator it = entries.begin();
		LLSD ids = LLSD::emptyArray();
		for (/**/; it != entries.end(); ++it)
		{
			ids.append(it->second.mID);
		}
		panel->setExperienceIds(ids);
		panel->setReadonly(!LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_OPTIONS));
		panel->refreshExperienceCounter();
	}
}

void LLPanelLandExperiences::refresh()
{
	refreshPanel(mAllowed, EXPERIENCE_KEY_TYPE_ALLOWED);
	refreshPanel(mBlocked, EXPERIENCE_KEY_TYPE_BLOCKED);
}
