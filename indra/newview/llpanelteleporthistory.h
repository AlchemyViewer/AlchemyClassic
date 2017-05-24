/** 
 * @file llpanelteleporthistory.h
 * @brief Teleport history represented by a scrolling list
 * class definition
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

#ifndef LL_LLPANELTELEPORTHISTORY_H
#define LL_LLPANELTELEPORTHISTORY_H

#include "llpanelplacestab.h"
#include "llteleporthistory.h"

class LLTeleportHistoryStorage;
class LLAccordionCtrl;
class LLAccordionCtrlTab;
class LLFlatListView;
class LLMenuButton;

class LLTeleportHistoryPanel : public LLPanelPlacesTab
{
public:
	// *TODO: derive from LLListContextMenu?
	class ContextMenu
	{
	public:
		ContextMenu();
		~ContextMenu();
		void show(LLView* spawning_view, S32 index, S32 x, S32 y);

	private:
		LLHandle<LLContextMenu> createMenu();
		void onTeleport();
		void onInfo();
		void onCopyToClipboard();

		static void gotSLURLCallback(const std::string& slurl);

		LLHandle<LLContextMenu> mMenuHandle;
		S32 mIndex;
	};

	LLTeleportHistoryPanel();
	virtual ~LLTeleportHistoryPanel();

	/*virtual*/ BOOL postBuild() override;
	/*virtual*/ void draw() override;

	/*virtual*/ void onSearchEdit(const std::string& string) override;
	/*virtual*/ void onShowOnMap() override;
	/*virtual*/ void onShowProfile() override;
	/*virtual*/ void onTeleport() override;
	///*virtual*/ void onCopySLURL();
	/*virtual*/ void updateVerbs() override;
	/*virtual*/ bool isSingleItemSelected() override;

private:

	void onDoubleClickItem();
	void onReturnKeyPressed();
	void onAccordionTabRightClick(LLView *view, S32 x, S32 y, MASK mask);
	void onAccordionTabOpen(LLAccordionCtrlTab *tab);
	void onAccordionTabClose(LLAccordionCtrlTab *tab);
	void onExpandAllFolders();
	void onCollapseAllFolders();
	void onClearTeleportHistory();
	bool onClearTeleportHistoryDialog(const LLSD& notification, const LLSD& response);

	void refresh() override;
	void getNextTab(const LLDate& item_date, S32& curr_tab, LLDate& tab_date);
	void onTeleportHistoryChange(S32 removed_index);
	void replaceItem(S32 removed_index);
	void showTeleportHistory();
	void handleItemSelect(LLFlatListView* );
	LLFlatListView* getFlatListViewFromTab(LLAccordionCtrlTab *);
	bool isActionEnabled(const LLSD& userdata) const;

	void setAccordionCollapsedByUser(LLUICtrl* acc_tab, bool collapsed);
	bool isAccordionCollapsedByUser(LLUICtrl* acc_tab);
	void onAccordionExpand(LLUICtrl* ctrl, const LLSD& param);

	static void confirmTeleport(S32 hist_idx);
	static bool onTeleportConfirmation(const LLSD& notification, const LLSD& response, S32 hist_idx);

	LLTeleportHistoryStorage*	mTeleportHistory;
	LLAccordionCtrl*		mHistoryAccordion;

	LLFlatListView*			mLastSelectedFlatlList;
	S32				mLastSelectedItemIndex;
	bool				mDirty;
	S32				mCurrentItem;

	typedef std::vector<LLAccordionCtrlTab*> item_containers_t;
	item_containers_t mItemContainers;

	ContextMenu mContextMenu;
	LLHandle<LLContextMenu>	mAccordionTabMenuHandle;
	LLHandle<LLView>		mGearMenuHandle;
	LLMenuButton*			mMenuGearButton;

	boost::signals2::connection mTeleportHistoryChangedConnection;
};



#endif //LL_LLPANELTELEPORTHISTORY_H
