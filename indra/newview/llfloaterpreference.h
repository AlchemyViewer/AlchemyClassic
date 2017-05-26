/** 
 * @file llfloaterpreference.h
 * @brief LLPreferenceCore class definition
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

/*
 * App-wide preferences.  Note that these are not per-user,
 * because we need to load many preferences before we have
 * a login name.
 */

#ifndef LL_LLFLOATERPREFERENCE_H
#define LL_LLFLOATERPREFERENCE_H

#include "llfloater.h"
#include "llavatarpropertiesprocessor.h"
#include "llconversationlog.h"

class LLConversationLogObserver;
class LLPanelPreference;
class LLPanelLCD;
class LLPanelDebug;
class LLMessageSystem;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSD;
class LLTextBox;
struct skin_t;

typedef std::map<std::string, std::string> notifications_map;

typedef enum
	{
		GS_LOW_GRAPHICS,
		GS_MID_GRAPHICS,
		GS_HIGH_GRAPHICS,
		GS_ULTRA_GRAPHICS
		
	} EGraphicsSettings;

// Floater to control preferences (display, audio, bandwidth, general.
class LLFloaterPreference : public LLFloater, public LLAvatarPropertiesObserver, public LLConversationLogObserver
{
public: 
	LLFloaterPreference(const LLSD& key);
	~LLFloaterPreference();

	void apply();
	void cancel();
	/*virtual*/ void draw() override;
	/*virtual*/ BOOL postBuild() override;
	/*virtual*/ void onOpen(const LLSD& key) override;
	/*virtual*/	void onClose(bool app_quitting) override;
	/*virtual*/ void changed() override;
	/*virtual*/ void changed(const LLUUID& session_id, U32 mask) override {};

	// static data update, called from message handler
	static void updateUserInfo(const std::string& visibility, bool im_via_email);

	// refresh all the graphics preferences menus
	static void refreshEnabledGraphics();
	
	// translate user's do not disturb response message according to current locale if message is default, otherwise do nothing
	static void initDoNotDisturbResponse();

	void processProperties( void* pData, EAvatarProcessorType type ) override;
	void processProfileProperties(const LLAvatarData* pAvatarData );
	void storeAvatarProperties( const LLAvatarData* pAvatarData );
	void saveAvatarProperties( void );
	void selectPrivacyPanel();
	void selectChatPanel();

protected:	
	void		onBtnOK(const LLSD& userdata);
	void		onBtnCancel(const LLSD& userdata);

	void		onClickClearCache();			// Clear viewer texture cache, vfs, and VO cache on next startup
	void		onClickBrowserClearCache();		// Clear web history and caches as well as viewer caches above
	void		onLanguageChange();
	void		onNotificationsChange(const std::string& OptionName);
	void		onNameTagOpacityChange(const LLSD& newvalue);

	// set value of "DoNotDisturbResponseChanged" in account settings depending on whether do not disturb response
	// string differs from default after user changes.
	void onDoNotDisturbResponseChanged();
	// if the custom settings box is clicked
	void onChangeCustom();
	void updateMeterText(LLUICtrl* ctrl);
	// callback for defaults
	void setHardwareDefaults();
	// callback for when client turns on shaders
	void onVertexShaderEnable();
	// callback for when client turns on impostors
	void onAvatarImpostorsEnable();

	// callback for commit in the "Single click on land" and "Double click on land" comboboxes.
	void onClickActionChange();
	// updates click/double-click action settings depending on controls values
	void updateClickActionSettings();
	// updates click/double-click action controls depending on values from settings.xml
	void updateClickActionControls();

public:
	// This function squirrels away the current values of the controls so that
	// cancel() can restore them.	
	void saveSettings();

	void setCacheLocation(const LLStringExplicit& location);

	void onClickSetCache();
	void onClickResetCache();
	void onClickSetKey();
	void setKey(KEY key);
	void onClickSetMiddleMouse();
	void onClickSetSounds();
	void onClickEnablePopup();
	void onClickDisablePopup();	
	void resetAllIgnored();
	void setAllIgnored();
	void onClickLogPath();
	bool moveTranscriptsAndLog();
	void enableHistory();
	void setPersonalInfo(const std::string& visibility, bool im_via_email);
	void refreshEnabledState();
	void onCommitWindowedMode();
	void refresh() override;	// Refresh enable/disable
	// if the quality radio buttons are changed
	void onChangeQuality(const LLSD& data);
	
	void refreshUI();

	void onChangeMaturity();
	void onChangeModelFolder();
	void onChangeTextureFolder();
	void onChangeSoundFolder();
	void onChangeAnimationFolder();
	void onClickBlockList();
	void onClickProxySettings();
	void onClickTranslationSettings();
	void onClickPermsDefault();
	void onClickAutoReplace();
	void onClickSpellChecker();
	void onClickRenderExceptions();
	void onClickAdvanced();
	void onClickResetControlDefault(const LLSD& userdata); // <alchemy/>
	void applyUIColor(LLUICtrl* ctrl, const LLSD& param);
	void getUIColor(LLUICtrl* ctrl, const LLSD& param);
	void onLogChatHistorySaved();	
	void buildPopupLists();
	void selectPanel(const LLSD& name);

private:
	void onSoundQualityChange();

	void onDeleteTranscripts();
	void onDeleteTranscriptsResponse(const LLSD& notification, const LLSD& response);
	void updateDeleteTranscriptsButton();
	void updateMaxComplexity();
	
	void refreshGridList();
	void onClickAddGrid();
	void onClickRemoveGrid();
	void onClickRefreshGrid();
	void onClickDebugGrid();
	void onSelectGrid(const LLSD& data);
	bool handleRemoveGridCB(const LLSD& notification, const LLSD& response);
	
	void loadUserSkins();
	void reloadSkinList();
	void onAddSkin();
	void onRemoveSkin();
	void callbackRemoveSkin(const LLSD& notification, const LLSD& response);
	void onApplySkin();
	void callbackApplySkin(const LLSD& notification, const LLSD& response);
	void onSelectSkin(const LLSD& data);
	void refreshSkinInfo(const skin_t& skin);

	static std::string sSkin;
	notifications_map mNotificationOptions;
	bool mClickActionDirty; ///< Set to true when the click/double-click options get changed by user.
	bool mGotPersonalInfo;
	bool mOriginalIMViaEmail;
	bool mLanguageChanged;
	bool mSoundQualityChanged;
	bool mAvatarDataInitialized;
	std::string mPriorInstantMessageLogPath;
	
	bool mOriginalHideOnlineStatus;
	std::string mDirectoryVisibility;
	
	LLAvatarData mAvatarProperties;

	typedef std::map<std::string, skin_t> skinmap_t;
	skinmap_t mUserSkins;
	
	boost::signals2::connection mGridListChangedConnection;

	LOG_CLASS(LLFloaterPreference);
};

class LLPanelPreference : public LLPanel
{
public:
	LLPanelPreference();
	/*virtual*/ BOOL postBuild() override;
	
	virtual ~LLPanelPreference();

	virtual void apply();
	virtual void cancel();
	void setControlFalse(const LLSD& user_data);
	virtual void setHardwareDefaults();

	// Disables "Allow Media to auto play" check box only when both
	// "Streaming Music" and "Media" are unchecked. Otherwise enables it.
	void updateMediaAutoPlayCheckbox(LLUICtrl* ctrl);

	// This function squirrels away the current values of the controls so that
	// cancel() can restore them.
	virtual void saveSettings();

	class Updater;

protected:
	typedef std::map<LLControlVariable*, LLSD> control_values_map_t;
	control_values_map_t mSavedValues;

private:
	//for "Only friends and groups can call or IM me"
	static void showFriendsOnlyWarning(LLUICtrl*, const LLSD&);
	//for "Show my Favorite Landmarks at Login"
	static void showFavoritesOnLoginWarning(LLUICtrl* checkbox, const LLSD& value);

	typedef std::map<std::string, LLColor4> string_color_map_t;
	string_color_map_t mSavedColors;

	Updater* mBandWidthUpdater;
	LOG_CLASS(LLPanelPreference);
};

class LLPanelPreferenceGraphics : public LLPanelPreference
{
public:
	BOOL postBuild() override;
	void draw() override;
	void cancel() override;
	void saveSettings() override;
	void resetDirtyChilds();
	void setHardwareDefaults() override;

protected:
	LOG_CLASS(LLPanelPreferenceGraphics);
};

class LLFloaterPreferenceGraphicsAdvanced : public LLFloater
{
  public: 
	LLFloaterPreferenceGraphicsAdvanced(const LLSD& key);
	~LLFloaterPreferenceGraphicsAdvanced();
	void onOpen(const LLSD& key) override;
	void onClickCloseBtn(bool app_quitting) override;
	void disableUnavailableSettings();
	void refreshEnabledGraphics();
	void refreshEnabledState();
	void updateSliderText(LLSliderCtrl* ctrl, LLTextBox* text_box);
	void updateMaxNonImpostors();
	void setMaxNonImpostorsText(U32 value, LLTextBox* text_box);
	void updateMaxComplexity();
	void setMaxComplexityText(U32 value, LLTextBox* text_box);
	static void setIndirectControls();
	static void setIndirectMaxNonImpostors();
	static void setIndirectMaxArc();
	void refresh() override;
	// callback for when client turns on shaders
	void onVertexShaderEnable();
	LOG_CLASS(LLFloaterPreferenceGraphicsAdvanced);
};

class LLAvatarComplexityControls
{
  public: 
	static void updateMax(LLSliderCtrl* slider, LLTextBox* value_label);
	static void setText(U32 value, LLTextBox* text_box);
	static void setIndirectControls();
	static void setIndirectMaxNonImpostors();
	static void setIndirectMaxArc();
	LOG_CLASS(LLAvatarComplexityControls);
};




#endif  // LL_LLPREFERENCEFLOATER_H
