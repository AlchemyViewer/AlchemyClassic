/** 
 * @file llpanelpicks.h
 * @brief LLPanelPicks and related class definitions
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

#ifndef LL_LLPANELPICKS_H
#define LL_LLPANELPICKS_H

#include "llpanel.h"
#include "llpanelavatar.h"
#include "llclassifieditem.h"

class LLAccordionCtrlTab;
class LLPanelProfile;
class LLMessageSystem;
class LLVector3d;
class LLPanelProfileTab;
class LLAgent;
class LLMenuGL;
class LLPickItem;
class LLClassifiedItem;
class LLFlatListView;
class LLPanelPickInfo;
class LLPanelPickEdit;
class LLToggleableMenu;
class LLPanelClassifiedInfo;
class LLPanelClassifiedEdit;

class LLPanelPicks 
	: public LLPanelProfileTab
{
public:
	LLPanelPicks();
	~LLPanelPicks();

	static void* create(void* data);

	BOOL postBuild() override;
	void onOpen(const LLSD& key) override;
	void onClosePanel() override;
	void processProperties(void* data, EAvatarProcessorType type) override;
	void updateData() override;

	// returns the selected pick item
	LLPickItem* getSelectedPickItem() const;
	LLClassifiedItem* getSelectedClassifiedItem() const;
	LLClassifiedItem* findClassifiedById(const LLUUID& classified_id) const;

	//*NOTE top down approch when panel toggling is done only by 
	// parent panels failed to work (picks related code was in my profile panel)
	void setProfilePanel(LLPanelProfile* profile_panel);

	void createNewPick();
	void createNewClassified();

protected:
	void updateButtons() override;

private:
	void onClickDelete();
	void onClickTeleport();
	void onClickMap();

	void onPlusMenuItemClicked(const LLSD& param);
	bool isActionEnabled(const LLSD& userdata) const;

	bool isClassifiedPublished(LLClassifiedItem* c_item);

	void onListCommit(const LLFlatListView* f_list);
	void onAccordionStateChanged(const LLAccordionCtrlTab* acc_tab);

	//------------------------------------------------
	// Callbacks which require panel toggling
	//------------------------------------------------
	void onClickPlusBtn();
	void onClickInfo();
	void onPanelPickClose(LLPanel* panel);
	void onPanelPickSave(LLPanel* panel);
	void onPanelClassifiedSave(LLPanelClassifiedEdit* panel);
	void onPanelClassifiedClose(LLPanelClassifiedInfo* panel);
	void openPickEdit(const LLSD& params);
	void onPanelPickEdit();
	void onPanelClassifiedEdit();
	void editClassified(const LLUUID&  classified_id);
	void onClickMenuEdit();

	bool onEnableMenuItem(const LLSD& user_data);

	void openPickInfo();
	void openClassifiedInfo();
	void openClassifiedInfo(const LLSD& params);
	void openClassifiedEdit(const LLSD& params);
	friend class LLPanelProfile;

	void showAccordion(const std::string& name, bool show);

	void buildPickPanel();

	bool callbackDeletePick(const LLSD& notification, const LLSD& response);
	bool callbackDeleteClassified(const LLSD& notification, const LLSD& response);
	bool callbackTeleport(const LLSD& notification, const LLSD& response);


	virtual void onDoubleClickPickItem(LLUICtrl* item);
	virtual void onDoubleClickClassifiedItem(LLUICtrl* item);
	virtual void onRightMouseUpItem(LLUICtrl* item, S32 x, S32 y, MASK mask);

	LLPanelProfile* getProfilePanel() const;

	void createPickInfoPanel();
	void createPickEditPanel();
	void createClassifiedInfoPanel();
	void createClassifiedEditPanel(LLPanelClassifiedEdit** panel);

	LLHandle<LLView> mPopupMenuHandle;
	LLPanelProfile* mProfilePanel;
	LLPanelPickInfo* mPickPanel;
	LLFlatListView* mPicksList;
	LLFlatListView* mClassifiedsList;
	LLPanelPickInfo* mPanelPickInfo;
	LLPanelClassifiedInfo* mPanelClassifiedInfo;
	LLPanelPickEdit* mPanelPickEdit;
	LLHandle<LLToggleableMenu> mPlusMenuHandle;
	LLUICtrl* mNoItemsLabel;

	// This map is needed for newly created classifieds. The purpose of panel is to
	// sit in this map and listen to LLPanelClassifiedEdit::processProperties callback.
	panel_classified_edit_map_t mEditClassifiedPanels;

	LLAccordionCtrlTab* mPicksAccTab;
	LLAccordionCtrlTab* mClassifiedsAccTab;

	//true if picks list is empty after processing picks
	bool mNoPicks;
	//true if classifieds list is empty after processing classifieds
	bool mNoClassifieds;
};

#endif // LL_LLPANELPICKS_H
