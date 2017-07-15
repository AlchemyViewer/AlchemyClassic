// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llavatarlist.h
 * @brief Generic avatar list
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

#include "llviewerprecompiledheaders.h"

#include "llavatarlist.h"

// common
#include "lltrans.h"
#include "llcommonutils.h"

// llui
#include "lltextutil.h"

// newview
#include "llagent.h"
#include "llagentdata.h" // for comparator
#include "llavatariconctrl.h"
#include "llavatarnamecache.h"
#include "llcallingcard.h" // for LLAvatarTracker
#include "llcachename.h"
#include "lllistcontextmenu.h"
#include "llrecentpeople.h"
#include "lluuid.h"
#include "llvoiceclient.h"
#include "llviewercontrol.h"	// for gSavedSettings
#include "lltooldraganddrop.h"
#include "llworld.h"

static LLDefaultChildRegistry::Register<LLAvatarList> r("avatar_list");

// Last interaction time update period.
static const F32 LIT_UPDATE_PERIOD = 2;

// Maximum number of avatars that can be added to a list in one pass.
// Used to limit time spent for avatar list update per frame.
static const unsigned ADD_LIMIT = 50;

bool LLAvatarList::contains(const LLUUID& id)
{
	const uuid_vec_t& ids = getIDs();
	return std::find(ids.begin(), ids.end(), id) != ids.end();
}

void LLAvatarList::toggleIcons()
{
	// Save the new value for new items to use.
	mShowIcons = !mShowIcons;
	gSavedSettings.setBOOL(mIconParamName, mShowIcons);
	
	// Show/hide icons for all existing items.
	std::vector<LLPanel*> items;
	getItems(items);
	for (auto item : items)
	{
		static_cast<LLAvatarListItem*>(item)->setAvatarIconVisible(mShowIcons);
	}
}

void LLAvatarList::setSpeakingIndicatorsVisible(bool visible)
{
	// Save the new value for new items to use.
	mShowSpeakingIndicator = visible;
	
	// Show/hide icons for all existing items.
	std::vector<LLPanel*> items;
	getItems(items);
	for (auto item : items)
	{
		static_cast<LLAvatarListItem*>(item)->showSpeakingIndicator(mShowSpeakingIndicator);
	}
}

void LLAvatarList::showPermissions(bool visible)
{
	// Save the value for new items to use.
	mShowPermissions = visible;

	// Enable or disable showing permissions icons for all existing items.
	std::vector<LLPanel*> items;
	getItems(items);
	for (auto item : items)
	{
		static_cast<LLAvatarListItem*>(item)->setShowPermissions(mShowPermissions);
	}
}

static bool findInsensitive(std::string haystack, const std::string& needle_upper)
{
    LLStringUtil::toUpper(haystack);
    return haystack.find(needle_upper) != std::string::npos;
}


//comparators
static const LLAvatarItemNameComparator NAME_COMPARATOR;
static const LLFlatListView::ItemReverseComparator REVERSE_NAME_COMPARATOR(NAME_COMPARATOR);

LLAvatarList::Params::Params()
: ignore_online_status("ignore_online_status", false)
, show_last_interaction_time("show_last_interaction_time", false)
, show_distance("show_distance", false)
, show_info_btn("show_info_btn", true)
, show_profile_btn("show_profile_btn", true)
, show_speaking_indicator("show_speaking_indicator", true)
, show_permissions_granted("show_permissions_granted", false)
{
}

LLAvatarList::LLAvatarList(const Params& p)
:	LLFlatListViewEx(p)
, mIgnoreOnlineStatus(p.ignore_online_status)
, mShowLastInteractionTime(p.show_last_interaction_time)
, mShowDistance(p.show_distance)
, mDirty(true)
, mNeedUpdateNames(false) // to force initial update
, mShowIcons(true)
, mShowInfoBtn(p.show_info_btn)
, mShowProfileBtn(p.show_profile_btn)
, mShowSpeakingIndicator(p.show_speaking_indicator)
, mShowPermissions(p.show_permissions_granted)
, mShowCompleteName(false)
, mLITUpdateTimer(nullptr)
, mContextMenu(nullptr)
{
	setCommitOnSelectionChange(true);

	// Set default sort order.
	setComparator(&NAME_COMPARATOR);

	if (mShowLastInteractionTime || mShowDistance)
	{
		mLITUpdateTimer = new LLTimer();
		mLITUpdateTimer->setTimerExpirySec(0); // zero to force initial update
		mLITUpdateTimer->start();
	}
	
	LLAvatarNameCache::addUseDisplayNamesCallback(boost::bind(&LLAvatarList::handleDisplayNamesOptionChanged, this));

	gSavedSettings.getControl("AlchemyAvatarListNameFormat")->getSignal()->connect(
		boost::bind(&LLAvatarList::handleDisplayNamesOptionChanged, this));
}


void LLAvatarList::handleDisplayNamesOptionChanged()
{
	mNeedUpdateNames = true;
}


LLAvatarList::~LLAvatarList()
{
	delete mLITUpdateTimer;
}

void LLAvatarList::setShowIcons(std::string param_name)
{
	mIconParamName= param_name;
	mShowIcons = gSavedSettings.getBOOL(mIconParamName);
}

std::string LLAvatarList::getAvatarName(LLAvatarName av_name)
{
	return mShowCompleteName? av_name.getCompleteName(false) : av_name.getDisplayName();
}

// virtual
void LLAvatarList::draw()
{
	// *NOTE dzaporozhan
	// Call refresh() after draw() to avoid flickering of avatar list items.

	LLFlatListViewEx::draw();

	if (mNeedUpdateNames)
	{
		updateAvatarNames();
	}

	if (mDirty)
		refresh();

	if (mShowLastInteractionTime && mLITUpdateTimer->hasExpired())
	{
		updateLastInteractionTimes();
		mLITUpdateTimer->setTimerExpirySec(LIT_UPDATE_PERIOD); // restart the timer
	}
	else if (mShowDistance && mLITUpdateTimer->hasExpired())
	{
		updateDistances();
		mLITUpdateTimer->setTimerExpirySec(LIT_UPDATE_PERIOD);
	}
}

//virtual
void LLAvatarList::clear()
{
	getIDs().clear();
	setDirty(true);
	LLFlatListViewEx::clear();
}

void LLAvatarList::setNameFilter(const std::string& filter)
{
	std::string filter_upper = filter;
	LLStringUtil::toUpper(filter_upper);
	if (mNameFilter != filter_upper)
	{
		mNameFilter = filter_upper;

		// update message for empty state here instead of refresh() to avoid blinking when switch
		// between tabs.
		updateNoItemsMessage(filter);
		setDirty();
	}
}

void LLAvatarList::sortByName()
{
	setComparator(&NAME_COMPARATOR);
	sort();
}

void LLAvatarList::setDirty(bool val /*= true*/, bool force_refresh /*= false*/)
{
	mDirty = val;
	if(mDirty && force_refresh)
	{
		refresh();
	}
}

void LLAvatarList::addAvalineItem(const LLUUID& item_id, const LLUUID& session_id, const std::string& item_name)
{
	LL_DEBUGS("Avaline") << "Adding avaline item into the list: " << item_name << "|" << item_id << ", session: " << session_id << LL_ENDL;
	LLAvalineListItem* item = new LLAvalineListItem(/*hide_number=*/false);
	item->setAvatarId(item_id, session_id, true, false);
	item->setName(item_name);
	item->showLastInteractionTime(mShowLastInteractionTime);
	item->showDistance(mShowDistance);
	item->showSpeakingIndicator(mShowSpeakingIndicator);
	item->setOnline(false);

	addItem(item, item_id);
	mIDs.push_back(item_id);
	sort();
}

//////////////////////////////////////////////////////////////////////////
// PROTECTED SECTION
//////////////////////////////////////////////////////////////////////////
void LLAvatarList::refresh()
{
	bool have_names			= TRUE;
	bool add_limit_exceeded	= false;
	bool modified			= false;
	bool have_filter		= !mNameFilter.empty();

	// Save selection.	
	uuid_vec_t selected_ids;
	getSelectedUUIDs(selected_ids);
	LLUUID current_id = getSelectedUUID();

	// Determine what to add and what to remove.
	uuid_vec_t added, removed;
	LLAvatarList::computeDifference(getIDs(), added, removed);

	// Handle added items.
	unsigned nadded = 0;
	const std::string waiting_str = LLTrans::getString("AvatarNameWaiting");

	for (const auto& buddy_id : added)
	{
		LLAvatarName av_name;
		have_names &= LLAvatarNameCache::get(buddy_id, &av_name);

		if (!have_filter || findInsensitive(getAvatarName(av_name), mNameFilter))
		{
			if (nadded >= ADD_LIMIT)
			{
				add_limit_exceeded = true;
				break;
			}
			else
			{
				// *NOTE: If you change the UI to show a different string,
				// be sure to change the filter code below.
				if (LLRecentPeople::instance().isAvalineCaller(buddy_id))
				{
					const LLSD& call_data = LLRecentPeople::instance().getData(buddy_id);
					addAvalineItem(buddy_id, call_data["session_id"].asUUID(), call_data["call_number"].asString());
				}
				else
				{
					std::string display_name = getAvatarName(av_name);
					addNewItem(buddy_id, 
						display_name.empty() ? waiting_str : display_name, 
						LLAvatarTracker::instance().isBuddyOnline(buddy_id));
				}
				
				modified = true;
				nadded++;
			}
		}
	}

	// Handle removed items.
	for (const auto& item : removed)
	{
		removeItemByUUID(item);
		modified = true;
	}

	// Handle filter.
	if (have_filter)
	{
		std::vector<LLSD> cur_values;
		getValues(cur_values);

		for (const auto& value : cur_values)
		{
			const LLUUID& buddy_id = value.asUUID();
			LLAvatarName av_name;
			have_names &= LLAvatarNameCache::get(buddy_id, &av_name);
			if (!findInsensitive(getAvatarName(av_name), mNameFilter))
			{
				removeItemByUUID(buddy_id);
				modified = true;
			}
		}
	}

	// Changed item in place, need to request sort and update columns
	// because we might have changed data in a column on which the user
	// has already sorted. JC
	sort();

	// re-select items
	//	selectMultiple(selected_ids); // TODO: implement in LLFlatListView if need
	selectItemByUUID(current_id);

	// If the name filter is specified and the names are incomplete,
	// we need to re-update when the names are complete so that
	// the filter can be applied correctly.
	//
	// Otherwise, if we have no filter then no need to update again
	// because the items will update their names.
	bool dirty = add_limit_exceeded || (have_filter && !have_names);
	setDirty(dirty);

	// Refreshed all items.
	if(!dirty)
	{
		// Highlight items matching the filter.
		std::vector<LLPanel*> items;
		getItems(items);
		for (const auto& itemp : items)
		{
			static_cast<LLAvatarListItem*>(itemp)->setHighlight(mNameFilter);
		}

		// Send refresh_complete signal.
		mRefreshCompleteSignal(this, LLSD((S32)size(false)));
	}

	// Commit if we've added/removed items.
	if (modified)
		onCommit();
}

void LLAvatarList::updateAvatarNames()
{
	std::vector<LLPanel*> items;
	getItems(items);

	for (const auto& itemp : items)
	{
		LLAvatarListItem* item = static_cast<LLAvatarListItem*>(itemp);
		item->setShowCompleteName(mShowCompleteName);
		item->updateAvatarName();
	}
	mNeedUpdateNames = false;
}


bool LLAvatarList::filterHasMatches()
{
	uuid_vec_t values = getIDs();

	for (const auto& buddy_id : values)
	{
		LLAvatarName av_name;
		bool have_name = LLAvatarNameCache::get(buddy_id, &av_name);

		// If name has not been loaded yet we consider it as a match.
		// When the name will be loaded the filter will be applied again(in refresh()).

		if (have_name && !findInsensitive(getAvatarName(av_name), mNameFilter))
		{
			continue;
		}

		return true;
	}
	return false;
}

boost::signals2::connection LLAvatarList::setRefreshCompleteCallback(const commit_signal_t::slot_type& cb)
{
	return mRefreshCompleteSignal.connect(cb);
}

boost::signals2::connection LLAvatarList::setItemDoubleClickCallback(const mouse_signal_t::slot_type& cb)
{
	return mItemDoubleClickSignal.connect(cb);
}

//virtual
S32 LLAvatarList::notifyParent(const LLSD& info)
{
	if (info.has("sort") && &NAME_COMPARATOR == mItemComparator)
	{
		sort();
		return 1;
	}
	return LLFlatListViewEx::notifyParent(info);
}

void LLAvatarList::addNewItem(const LLUUID& id, const std::string& name, BOOL is_online, EAddPosition pos)
{
	LLAvatarListItem* item = new LLAvatarListItem();
	item->setShowCompleteName(mShowCompleteName);
	// This sets the name as a side effect
	item->setAvatarId(id, mSessionID, mIgnoreOnlineStatus);
	item->setOnline(mIgnoreOnlineStatus ? true : is_online);
	item->showLastInteractionTime(mShowLastInteractionTime);
	item->showDistance(mShowDistance);

	item->setAvatarIconVisible(mShowIcons);
	item->setShowInfoBtn(mShowInfoBtn);
	item->setShowProfileBtn(mShowProfileBtn);
	item->showSpeakingIndicator(mShowSpeakingIndicator);
	item->setShowPermissions(mShowPermissions);


	item->setDoubleClickCallback(boost::bind(&LLAvatarList::onItemDoubleClicked, this, _1, _2, _3, _4));

	addItem(item, id, pos);
}

// virtual
BOOL LLAvatarList::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLUICtrl::handleRightMouseDown(x, y, mask);
	if ( mContextMenu && !isAvalineItemSelected())
	{
		uuid_vec_t selected_uuids;
		getSelectedUUIDs(selected_uuids);
		mContextMenu->show(this, selected_uuids, x, y);
	}
	return handled;
}

BOOL LLAvatarList::handleMouseDown(S32 x, S32 y, MASK mask)
{
	gFocusMgr.setMouseCapture(this);

	S32 screen_x;
	S32 screen_y;
	localPointToScreen(x, y, &screen_x, &screen_y);
	LLToolDragAndDrop::getInstance()->setDragStart(screen_x, screen_y);

	return LLFlatListViewEx::handleMouseDown(x, y, mask);
}

BOOL LLAvatarList::handleMouseUp( S32 x, S32 y, MASK mask )
{
	if(hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(nullptr);
	}

	return LLFlatListViewEx::handleMouseUp(x, y, mask);
}

BOOL LLAvatarList::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = hasMouseCapture();
	if(handled)
	{
		S32 screen_x;
		S32 screen_y;
		localPointToScreen(x, y, &screen_x, &screen_y);

		if(LLToolDragAndDrop::getInstance()->isOverThreshold(screen_x, screen_y))
		{
			// First, create the global drag and drop object
			std::vector<EDragAndDropType> types;
			uuid_vec_t cargo_ids;
			getSelectedUUIDs(cargo_ids);
			types.resize(cargo_ids.size(), DAD_PERSON);
			LLToolDragAndDrop::ESource src = LLToolDragAndDrop::SOURCE_PEOPLE;
			LLToolDragAndDrop::getInstance()->beginMultiDrag(types, cargo_ids, src);
		}
	}

	if(!handled)
	{
		handled = LLFlatListViewEx::handleHover(x, y, mask);
	}

	return handled;
}

bool LLAvatarList::isAvalineItemSelected() const
{
	std::vector<LLPanel*> selected_items;
	getSelectedItems(selected_items);

	for (const auto& selected_item : selected_items)
	{
		if (dynamic_cast<LLAvalineListItem*>(selected_item))
			return true;
	}

	return false;
}

void LLAvatarList::setVisible(BOOL visible)
{
	if ( visible == FALSE && mContextMenu )
	{
		mContextMenu->hide();
	}
	LLFlatListViewEx::setVisible(visible);
}

void LLAvatarList::computeDifference(
	const uuid_vec_t& vnew_unsorted,
	uuid_vec_t& vadded,
	uuid_vec_t& vremoved) const
{
	uuid_vec_t vcur;

	// Convert LLSDs to LLUUIDs.
	{
		std::vector<LLSD> vcur_values;
		getValues(vcur_values);

		for (size_t i=0; i<vcur_values.size(); i++)
			vcur.push_back(vcur_values[i].asUUID());
	}

	LLCommonUtils::computeDifference(vnew_unsorted, vcur, vadded, vremoved);
}

// Refresh shown time of our last interaction with all listed avatars.
void LLAvatarList::updateLastInteractionTimes()
{
	S32 now = (S32) LLDate::now().secondsSinceEpoch();
	std::vector<LLPanel*> items;
	getItems(items);

	for(auto itemp : items)
	{
		// *TODO: error handling
		LLAvatarListItem* item = static_cast<LLAvatarListItem*>(itemp);
		S32 secs_since = now - (S32) LLRecentPeople::instance().getDate(item->getAvatarId()).secondsSinceEpoch();
		if (secs_since >= 0)
			item->setLastInteractionTime(secs_since);
	}
}

void LLAvatarList::updateDistances()
{
	std::vector<LLPanel*> items;
	getItems(items);
	
	static LLCachedControl<F32> near_me_range(gSavedSettings, "NearMeRange");
	LLWorld::pos_map_t positions;
	LLWorld::getInstance()->getAvatars(&positions, gAgent.getPositionGlobal(), near_me_range);

	for (auto itemp : items)
	{
		LLAvatarListItem* item = static_cast<LLAvatarListItem*>(itemp);
		if (item->getAvatarId() == gAgentID) continue;
		
		LLWorld::pos_map_t::iterator iter = positions.find(item->getAvatarId());
		if (iter != positions.end())
			item->setDistance((iter->second - gAgent.getPositionGlobal()).magVec());
		else
			item->setDistance(0.f);
	}
}

void LLAvatarList::onItemDoubleClicked(LLUICtrl* ctrl, S32 x, S32 y, MASK mask)
{
	mItemDoubleClickSignal(ctrl, x, y, mask);
}

bool LLAvatarItemComparator::compare(const LLPanel* item1, const LLPanel* item2) const
{
	const LLAvatarListItem* avatar_item1 = dynamic_cast<const LLAvatarListItem*>(item1);
	const LLAvatarListItem* avatar_item2 = dynamic_cast<const LLAvatarListItem*>(item2);
	
	if (!avatar_item1 || !avatar_item2)
	{
		LL_ERRS() << "item1 and item2 cannot be null" << LL_ENDL;
		return true;
	}

	return doCompare(avatar_item1, avatar_item2);
}

bool LLAvatarItemNameComparator::doCompare(const LLAvatarListItem* avatar_item1, const LLAvatarListItem* avatar_item2) const
{
	std::string name1 = avatar_item1->getAvatarName();
	std::string name2 = avatar_item2->getAvatarName();

	LLStringUtil::toUpper(name1);
	LLStringUtil::toUpper(name2);

	return name1 < name2;
}
bool LLAvatarItemAgentOnTopComparator::doCompare(const LLAvatarListItem* avatar_item1, const LLAvatarListItem* avatar_item2) const
{
	//keep agent on top, if first is agent, 
	//then we need to return true to elevate this id, otherwise false.
	if(avatar_item1->getAvatarId() == gAgentID)
	{
		return true;
	}
	else if (avatar_item2->getAvatarId() == gAgentID)
	{
		return false;
	}
	return LLAvatarItemNameComparator::doCompare(avatar_item1,avatar_item2);
}

/************************************************************************/
/*             class LLAvalineListItem                                  */
/************************************************************************/
LLAvalineListItem::LLAvalineListItem(bool hide_number/* = true*/) : LLAvatarListItem(false)
, mIsHideNumber(hide_number)
{
	// should not use buildPanel from the base class to ensure LLAvalineListItem::postBuild is called.
	buildFromFile( "panel_avatar_list_item.xml");
}

BOOL LLAvalineListItem::postBuild()
{
	BOOL rv = LLAvatarListItem::postBuild();

	if (rv)
	{
		setOnline(true);
		showLastInteractionTime(false);
		showDistance(false);
		setShowProfileBtn(false);
		setShowInfoBtn(false);
		mAvatarIcon->setValue("Avaline_Icon");
		mAvatarIcon->setToolTip(std::string(""));
	}
	return rv;
}

// to work correctly this method should be called AFTER setAvatarId for avaline callers with hidden phone number
void LLAvalineListItem::setName(const std::string& name)
{
	if (mIsHideNumber)
	{
		static U32 order = 0;
		typedef std::map<LLUUID, U32> avaline_callers_nums_t;
		static avaline_callers_nums_t mAvalineCallersNums;

		llassert(getAvatarId() != LLUUID::null);

		const LLUUID &uuid = getAvatarId();

		if (mAvalineCallersNums.find(uuid) == mAvalineCallersNums.end())
		{
			mAvalineCallersNums[uuid] = ++order;
			LL_DEBUGS("Avaline") << "Set name for new avaline caller: " << uuid << ", order: " << order << LL_ENDL;
		}
		LLStringUtil::format_map_t args;
		args["[ORDER]"] = llformat("%u", mAvalineCallersNums[uuid]);
		std::string hidden_name = LLTrans::getString("AvalineCaller", args);

		LL_DEBUGS("Avaline") << "Avaline caller: " << uuid << ", name: " << hidden_name << LL_ENDL;
		LLAvatarListItem::setAvatarName(hidden_name);
		LLAvatarListItem::setAvatarToolTip(hidden_name);
	}
	else
	{
		const std::string& formatted_phone = LLTextUtil::formatPhoneNumber(name);
		LLAvatarListItem::setAvatarName(formatted_phone);
		LLAvatarListItem::setAvatarToolTip(formatted_phone);
	}
}
