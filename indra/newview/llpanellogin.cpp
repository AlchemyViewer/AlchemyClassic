// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llpanellogin.cpp
 * @brief Login dialog and logo display
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

#include "llpanellogin.h"
#include "lllayoutstack.h"

#include "indra_constants.h"		// for key and mask constants
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llmd5.h"
#include "v4color.h"

#include "llappviewer.h"
#include "llbutton.h"
#include "llcommandhandler.h"		// for secondlife:///app/login/
#include "llcombobox.h"
#include "llviewercontrol.h"
#include "llfloaterpreference.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llsecapi.h"
#include "llstartup.h"
#include "lltextbox.h"
#include "llui.h"
#include "llslurl.h"
#include "llversioninfo.h"
#include "llviewerhelp.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"			// for handle_preferences()
#include "llviewernetwork.h"
#include "llviewerwindow.h"			// to link into child list
#include "llweb.h"
#include "llmediactrl.h"
#include "llrootview.h"

#include "lltrans.h"
#include "llglheaders.h"
#include "llpanelloginlistener.h"

#include <boost/algorithm/string.hpp>

#if LL_WINDOWS
#pragma warning(disable: 4355)      // 'this' used in initializer list
#endif  // LL_WINDOWS

#include "llsdserialize.h"

LLPanelLogin *LLPanelLogin::sInstance = nullptr;
BOOL LLPanelLogin::sCapslockDidNotification = FALSE;

class LLLoginRefreshHandler : public LLCommandHandler
{
public:
	// don't allow from external browsers
	LLLoginRefreshHandler() : LLCommandHandler("login_refresh", UNTRUSTED_BLOCK) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLMediaCtrl* web) override
	{	
		if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
		{
			LLPanelLogin::loadLoginPage();
		}	
		return true;
	}
};

//---------------------------------------------------------------------------
// Public methods
//---------------------------------------------------------------------------
LLPanelLogin::LLPanelLogin(const LLRect &rect,
						 void (*callback)(S32 option, void* user_data),
						 void *cb_data)
:	LLPanel(),
	mLogoImage(),
	mListener(new LLPanelLoginListener(this)),
	mCallback(callback),
	mCallbackData(cb_data),
	mLoginWidgetStack(nullptr)
{
	setBackgroundVisible(FALSE);
	setBackgroundOpaque(TRUE);

	mPasswordModified = FALSE;
	sInstance = this;

	LLView* login_holder = gViewerWindow->getLoginPanelHolder();
	if (login_holder)
	{
		login_holder->addChild(this);
	}

	// Logo
	mLogoImage = LLUI::getUIImage("startup_logo");

	buildFromFile("panel_login.xml");

	LLView::reshape(rect.getWidth(), rect.getHeight());

    mCommitCallbackRegistrar.add("RemoveUser", boost::bind(&LLPanelLogin::onRemoveUser, this, _2));
    mCommitCallbackRegistrar.add("Connect", boost::bind(&LLPanelLogin::onClickConnect, this));

	auto password_edit = getChild<LLLineEditor>("password_edit");
	password_edit->setKeystrokeCallback(onPassKey, this);
	password_edit->setCommitCallback(boost::bind(&LLPanelLogin::onClickConnect, this));

	// change z sort of clickable text to be behind buttons
	sendChildToBack(LLView::getChildView("forgot_password_text"));

	auto location_combo = getChild<LLComboBox>("start_location_combo");
	updateLocationSelectorsVisibility(); // separate so that it can be called from preferences
	location_combo->setFocusLostCallback(boost::bind(&LLPanelLogin::onLocationSLURL, this));
	
	auto server_choice_combo = getChild<LLComboBox>("server_combo");
	server_choice_combo->setCommitCallback(boost::bind(&LLPanelLogin::onSelectServer, this));
	
	refreshGridList();
	mGridListChangedConnection = LLGridManager::getInstance()->addGridListChangedCallback(boost::bind(&LLPanelLogin::refreshGridList, this));

	LLSLURL start_slurl(LLStartUp::getStartSLURL());
	if ( !start_slurl.isSpatial() ) // has a start been established by the command line or NextLoginLocation ? 
	{
		// no, so get the preference setting
		std::string defaultStartLocation = gSavedSettings.getString("LoginLocation");
		LL_INFOS("AppInit")<<"default LoginLocation '"<<defaultStartLocation<<"'"<<LL_ENDL;
		LLSLURL defaultStart(defaultStartLocation);
		if ( defaultStart.isSpatial() )
		{
			LLStartUp::setStartSLURL(defaultStart);
		}
		else
		{
			LL_INFOS("AppInit")<<"no valid LoginLocation, using home"<<LL_ENDL;
			LLSLURL homeStart(LLSLURL::SIM_LOCATION_HOME);
			LLStartUp::setStartSLURL(homeStart);
		}
	}
	else
	{
		onUpdateStartSLURL(start_slurl); // updates grid if needed
	}
	
	childSetAction("connect_btn", onClickConnect, this);

	getChild<LLPanel>("links_login_panel")->setDefaultBtn("connect_btn");

	auto forgot_password_text = getChild<LLTextBox>("forgot_password_text");
	forgot_password_text->setClickedCallback(onClickForgotPassword, nullptr);

	childSetAction("create_new_account_btn", onClickNewAccount, nullptr);

	auto need_help_text = getChild<LLTextBox>("login_help");
	need_help_text->setClickedCallback(onClickHelp, nullptr);
	
	// get the web browser control
	auto web_browser = getChild<LLMediaCtrl>("login_html");
	web_browser->addObserver(this);

	reshapeBrowser();

	loadLoginPage();

	auto username_combo = getChild<LLComboBox>("username_combo");
	username_combo->setTextChangedCallback(boost::bind(&LLPanelLogin::onSelectUser, this));
	username_combo->setCommitCallback(boost::bind(&LLPanelLogin::onSelectUser, this));
	auto username_list = username_combo->getListCtrl();
	username_list->setRemoveCallback(boost::bind(&LLPanelLogin::onRemoveUser, this, _2));
	username_list->setCommitOnSelectionChange(true);
	
	mLoginWidgetStack = getChild<LLLayoutStack>("login_widgets");
}

void LLPanelLogin::addFavoritesToStartLocation()
{
	// Clear the combo.
	auto combo = getChild<LLComboBox>("start_location_combo");
	if (!combo) return;
	int num_items = combo->getItemCount();
	for (int i = num_items - 1; i > 2; i--)
	{
		combo->remove(i);
	}

	// Load favorites into the combo.
	std::string user_defined_name = getChild<LLComboBox>("username_combo")->getSimple();
	std::replace(user_defined_name.begin(), user_defined_name.end(), '.', ' ');
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "stored_favorites_" + LLGridManager::getInstance()->getGrid() + ".xml");
	std::string old_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "stored_favorites.xml");
	LLSD fav_llsd;
	llifstream file;
	file.open(filename);
	if (!file.is_open())
	{
		file.open(old_filename);
		if (!file.is_open()) return;
	}
	LLSDSerialize::fromXML(fav_llsd, file);

	for (LLSD::map_const_iterator iter = fav_llsd.beginMap();
		iter != fav_llsd.endMap(); ++iter)
	{
		// The account name in stored_favorites.xml has Resident last name even if user has
		// a single word account name, so it can be compared case-insensitive with the
		// user defined "firstname lastname".
		S32 res = LLStringUtil::compareInsensitive(user_defined_name, iter->first);
		if (res != 0)
		{
			LL_DEBUGS() << "Skipping favorites for " << iter->first << LL_ENDL;
			continue;
		}

		combo->addSeparator();
		LL_DEBUGS() << "Loading favorites for " << iter->first << LL_ENDL;
		auto user_llsd = iter->second;
		for (LLSD::array_const_iterator iter1 = user_llsd.beginArray();
			iter1 != user_llsd.endArray(); ++iter1)
		{
			std::string label = (*iter1)["name"].asString();
			std::string value = (*iter1)["slurl"].asString();
			if (!label.empty() && !value.empty())
			{
				combo->add(label, value);
			}
		}
		break;
	}
}

// force the size to be correct (XML doesn't seem to be sufficient to do this)
// (with some padding so the other login screen doesn't show through)
void LLPanelLogin::reshapeBrowser()
{
	auto web_browser = getChild<LLMediaCtrl>("login_html");
	LLRect rect = gViewerWindow->getWindowRectScaled();
	LLRect html_rect;
	html_rect.setCenterAndSize(
		rect.getCenterX() - 2, rect.getCenterY() + 40,
		rect.getWidth() + 6, rect.getHeight() - 78 );
	web_browser->setRect( html_rect );
	web_browser->reshape( html_rect.getWidth(), html_rect.getHeight(), TRUE );
	reshape( rect.getWidth(), rect.getHeight(), 1 );
}

LLPanelLogin::~LLPanelLogin()
{
	if (mGridListChangedConnection.connected())
		mGridListChangedConnection.disconnect();
	
	sInstance = nullptr;

	// Controls having keyboard focus by default
	// must reset it on destroy. (EXT-2748)
	gFocusMgr.setDefaultKeyboardFocus(nullptr);
}

// virtual
void LLPanelLogin::draw()
{
	gGL.pushMatrix();
	{
		F32 image_aspect = 1.333333f;
		F32 view_aspect = (F32)getRect().getWidth() / (F32)getRect().getHeight();
		// stretch image to maintain aspect ratio
		if (image_aspect > view_aspect)
		{
			gGL.translatef(-0.5f * (image_aspect / view_aspect - 1.f) * getRect().getWidth(), 0.f, 0.f);
			gGL.scalef(image_aspect / view_aspect, 1.f, 1.f);
		}

		S32 width = getRect().getWidth();
		S32 height = getRect().getHeight();

		if (mLoginWidgetStack->getVisible())
		{
			// draw a background box in black
			gl_rect_2d( 0, height - 264, width, 264, LLColor4::black );
			// draw the bottom part of the background image
			// just the blue background to the native client UI
			mLogoImage->draw(0, -264, width + 8, mLogoImage->getHeight());
		};
	}
	gGL.popMatrix();

	LLPanel::draw();
}

// virtual
BOOL LLPanelLogin::handleKeyHere(KEY key, MASK mask)
{
	if ( KEY_F1 == key )
	{
		auto vhelp = LLViewerHelp::getInstance();
		vhelp->showTopic(vhelp->f1HelpTopic());
		return TRUE;
	}

	return LLPanel::handleKeyHere(key, mask);
}

// virtual
void LLPanelLogin::setFocus(BOOL b)
{
	if(b != hasFocus())
	{
		if(b)
		{
			LLPanelLogin::giveFocus();
		}
		else
		{
			LLPanel::setFocus(b);
		}
	}
}

// static
void LLPanelLogin::giveFocus()
{
	if( sInstance )
	{
		// Grab focus and move cursor to first blank input field
		std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
		std::string pass = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();

		BOOL have_username = !username.empty();
		BOOL have_pass = !pass.empty();

		LLLineEditor* edit = nullptr;
		LLComboBox* combo = nullptr;
		if (have_username && !have_pass)
		{
			// User saved his name but not his password.  Move
			// focus to password field.
			edit = sInstance->getChild<LLLineEditor>("password_edit");
		}
		else
		{
			// User doesn't have a name, so start there.
			combo = sInstance->getChild<LLComboBox>("username_combo");
		}

		if (edit)
		{
			edit->setFocus(TRUE);
			edit->selectAll();
		}
		else if (combo)
		{
			combo->setFocus(TRUE);
		}
	}
}

// static
void LLPanelLogin::showLoginWidgets()
{
	if (sInstance)
	{
		// *NOTE: Mani - This may or may not be obselete code.
		// It seems to be part of the defunct? reg-in-client project.
		sInstance->getChildView("login_widgets")->setVisible( true);
		auto web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
		sInstance->reshapeBrowser();
		// *TODO: Append all the usual login parameters, like first_login=Y etc.
		std::string splash_screen_url = LLGridManager::getInstance()->getLoginPage();
		web_browser->navigateTo( splash_screen_url, HTTP_CONTENT_TEXT_HTML );
		auto username_combo = sInstance->getChild<LLUICtrl>("username_combo");
		username_combo->setFocus(TRUE);
	}
}

// static
void LLPanelLogin::show(const LLRect &rect,
						void (*callback)(S32 option, void* user_data),
						void* callback_data)
{
    if (!sInstance)
    {
        new LLPanelLogin(rect, callback, callback_data);
    }

	if( !gFocusMgr.getKeyboardFocus() )
	{
		// Grab focus and move cursor to first enabled control
		sInstance->setFocus(TRUE);
	}

	// Make sure that focus always goes here (and use the latest sInstance that was just created)
	gFocusMgr.setDefaultKeyboardFocus(sInstance);
}

// static
void LLPanelLogin::selectUser(LLPointer<LLCredential> cred, BOOL remember)
{
	if (!sInstance)
	{
		LL_WARNS() << "Attempted selectUser with no login view shown" << LL_ENDL;
		return;
	}
	LL_INFOS("Credentials") << "Setting login fields to " << *cred << LL_ENDL;

	auto pUserCombo = sInstance->getChild<LLComboBox>("username_combo");

	if (cred->hasIdentifier())
	{
		pUserCombo->setTextEntry(cred->username());
	}

	if (pUserCombo->getCurrentIndex() == -1)
	{
		sInstance->getChild<LLUICtrl>("password_edit")->setValue(LLStringUtil::null);
	}

	sInstance->getChild<LLUICtrl>("remember_check")->setValue(remember);
}

LLSD LLPanelLogin::getIdentifier()
{
	if (!sInstance)
	{
		LL_WARNS() << "Attempted getIdentifier with no login view shown" << LL_ENDL;
		return LLSD();
	}

	auto identifier = LLSD::emptyMap();

	std::string username = sInstance->getChild<LLComboBox>("username_combo")->getSimple();
	LLStringUtil::trim(username);

	// determine if the username is a first/last form or not.
	size_t separator_index = username.find_first_of(' ');
	if (separator_index == username.npos && !LLGridManager::getInstance()->isSystemGrid())
	{
		// single username, so this is a 'clear' identifier
		identifier["type"] = CRED_IDENTIFIER_TYPE_ACCOUNT;
		identifier["account_name"] = username;
	}
	else
	{
		// Be lenient in terms of what separators we allow for two-word names
		// and allow legacy users to login with firstname.lastname
		separator_index = username.find_first_of(" ._");
		std::string first = username.substr(0, separator_index);
		std::string last;
		if (separator_index != username.npos)
		{
			last = username.substr(separator_index+1, username.npos);
			LLStringUtil::trim(last);
		}
		else
		{
			// ...on Linden grids, single username users as considered to have
			// last name "Resident"
			// *TODO: Make login.cgi support "account_name" like above
			last = "Resident";
		}
		
		if (last.find_first_of(' ') == last.npos)
		{
			// traditional firstname / lastname
			identifier["type"] = CRED_IDENTIFIER_TYPE_AGENT;
			identifier["first_name"] = first;
			identifier["last_name"] = last;
		}
	}

	return identifier;
}

// static
void LLPanelLogin::getFields(LLPointer<LLCredential>& credential, BOOL& remember)
{
	if (!sInstance)
	{
		LL_WARNS() << "Attempted getFields with no login view shown" << LL_ENDL;
		return;
	}

	auto authenticator = LLSD::emptyMap();
	std::string password = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();

	auto identifier = getIdentifier();
	if (identifier.has("type"))
	{
		if (sInstance->mPasswordModified)
		{
			if (CRED_IDENTIFIER_TYPE_ACCOUNT == identifier["type"])
			{
				// single username, so this is a 'clear' identifier
				authenticator = LLSD::emptyMap();
				// password is plaintext
				authenticator["type"] = CRED_AUTHENTICATOR_TYPE_CLEAR;
				authenticator["secret"] = password;
			}
			else if (CRED_IDENTIFIER_TYPE_AGENT == identifier["type"])
			{
				authenticator = LLSD::emptyMap();
				authenticator["type"] = CRED_AUTHENTICATOR_TYPE_HASH;
				authenticator["algorithm"] = "md5";
				LLMD5 pass(reinterpret_cast<const U8*>(password.c_str()));
				char md5pass[33];               /* Flawfinder: ignore */
				pass.hex_digest(md5pass);
				authenticator["secret"] = md5pass;
			}
		}
		else
		{
			credential = gSecAPIHandler->loadCredential(LLGridManager::getInstance()->getGrid(), identifier);
			if (credential.notNull())
			{
				authenticator = credential->getAuthenticator();
			}
		}
	}

	credential = gSecAPIHandler->createCredential(LLGridManager::getInstance()->getGrid(), identifier, authenticator);
	remember = sInstance->getChild<LLUICtrl>("remember_check")->getValue();
}

// static
BOOL LLPanelLogin::areCredentialFieldsDirty()
{
	if (!sInstance)
	{
		LL_WARNS() << "Attempted getServer with no login view shown" << LL_ENDL;
	}
	else
	{
		auto combo = sInstance->getChild<LLComboBox>("username_combo");
		if(combo && combo->isDirty())
		{
			return true;
		}
		auto ctrl = sInstance->getChild<LLLineEditor>("password_edit");
		if(ctrl && ctrl->isDirty()) 
		{
			return true;
		}
	}
	return false;	
}


// static
void LLPanelLogin::updateLocationSelectorsVisibility()
{
	if (sInstance) 
	{
		BOOL show_start = gSavedSettings.getBOOL("ShowStartLocation");
		sInstance->getChild<LLLayoutPanel>("start_location_panel")->setVisible(show_start);

		BOOL show_server = gSavedSettings.getBOOL("ForceShowGrid");
		sInstance->getChild<LLLayoutPanel>("grid_panel")->setVisible(show_server);
	}	
}

// static - called from LLStartUp::setStartSLURL
void LLPanelLogin::onUpdateStartSLURL(const LLSLURL& new_start_slurl)
{
	if (!sInstance) return;

	LL_DEBUGS("AppInit")<<new_start_slurl.asString()<<LL_ENDL;

	auto location_combo = sInstance->getChild<LLComboBox>("start_location_combo");
	/*
	 * Determine whether or not the new_start_slurl modifies the grid.
	 *
	 * Note that some forms that could be in the slurl are grid-agnostic.,
	 * such as "home".  Other forms, such as
	 * https://grid.example.com/region/Party%20Town/20/30/5 
	 * specify a particular grid; in those cases we want to change the grid
	 * and the grid selector to match the new value.
	 */
	enum LLSLURL::SLURL_TYPE new_slurl_type = new_start_slurl.getType();
	switch ( new_slurl_type )
	{
	case LLSLURL::LOCATION:
	  {
		std::string slurl_grid = LLGridManager::getInstance()->getGrid(new_start_slurl.getGrid());
		if ( ! slurl_grid.empty() ) // is that a valid grid?
		{
			if ( slurl_grid != LLGridManager::getInstance()->getGrid() ) // new grid?
			{
				// the slurl changes the grid, so update everything to match
				LLGridManager::getInstance()->setGridChoice(slurl_grid);

				// update the grid selector to match the slurl
				LLComboBox* server_combo = sInstance->getChild<LLComboBox>("server_combo");
				std::string server_label(LLGridManager::getInstance()->getGridLabel(slurl_grid));
				server_combo->setSimple(server_label);

				updateServer(); // to change the links and splash screen
			}
			location_combo->setTextEntry(new_start_slurl.getLocationString());
		}
		else
		{
			// the grid specified by the slurl is not known
			LLNotificationsUtil::add("InvalidLocationSLURL");
			LL_WARNS("AppInit")<<"invalid LoginLocation:"<<new_start_slurl.asString()<<LL_ENDL;
			location_combo->setTextEntry(LLStringUtil::null);
		}
	  }
 	break;

	case LLSLURL::HOME_LOCATION:
		location_combo->setCurrentByIndex(1); // home location
		break;
		
	case LLSLURL::LAST_LOCATION:
		location_combo->setCurrentByIndex(0); // last location
		break;

	default:
		LL_WARNS("AppInit")<<"invalid login slurl, using home"<<LL_ENDL;
		location_combo->setCurrentByIndex(1); // home location
		break;
	}
}

void LLPanelLogin::setLocation(const LLSLURL& slurl)
{
	LL_DEBUGS("AppInit")<<"setting Location "<<slurl.asString()<<LL_ENDL;
	LLStartUp::setStartSLURL(slurl); // calls onUpdateStartSLURL, above
}

// static
void LLPanelLogin::closePanel()
{
	if (sInstance)
	{
		sInstance->getParent()->removeChild( sInstance );

		delete sInstance;
		sInstance = nullptr;
	}
}

// static
void LLPanelLogin::setAlwaysRefresh(bool refresh)
{
	if (sInstance && LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
	{
		auto web_browser = sInstance->getChild<LLMediaCtrl>("login_html");

		if (web_browser)
		{
			web_browser->setAlwaysRefresh(refresh);
		}
	}
}



void LLPanelLogin::loadLoginPage()
{
	if (!sInstance) return;

	LLURI login_page = LLURI(LLGridManager::getInstance()->getLoginPage());
	LLSD params(login_page.queryMap());

	LL_DEBUGS("AppInit") << "login_page: " << login_page << LL_ENDL;

	// Language
	params["lang"] = LLUI::getLanguage();
	if (gSavedSettings.getBOOL("FirstLoginThisInstall"))
	{
		params["firstlogin"] = "TRUE"; // not bool: server expects string TRUE
	}
	params["version"] = llformat("%s (%d)",
								 LLVersionInfo::getShortVersion().c_str(),
								 LLVersionInfo::getBuild());
	params["channel"] = LLVersionInfo::getChannel();
	params["grid"] = LLGridManager::getInstance()->getGridId();
	params["os"] = LLAppViewer::instance()->getOSInfo().getOSStringSimple();
	params["sourceid"] = gSavedSettings.getString("sourceid");

	// Make an LLURI with this augmented info
	LLURI login_uri(LLURI::buildHTTP(login_page.scheme(),
									 login_page.authority(),
									 login_page.path(),
									 params));

	gViewerWindow->setMenuBackgroundColor(false);

	auto web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
	if (web_browser->getCurrentNavUrl() != login_uri.asString())
	{
		LL_DEBUGS("AppInit") << "loading:    " << login_uri << LL_ENDL;
		web_browser->navigateTo( login_uri.asString(), HTTP_CONTENT_TEXT_HTML );
	}
}

void LLPanelLogin::handleMediaEvent(LLPluginClassMedia* /*self*/, EMediaEvent event)
{
}

//---------------------------------------------------------------------------
// Protected methods
//---------------------------------------------------------------------------
// static
void LLPanelLogin::onClickConnect(void *)
{
	if (sInstance && sInstance->mCallback)
	{
		// JC - Make sure the fields all get committed.
		sInstance->setFocus(FALSE);

		auto combo = sInstance->getChild<LLComboBox>("server_combo");
		auto combo_val = combo->getSelectedValue();

		// the grid definitions may come from a user-supplied grids.xml, so they may not be good
		LL_DEBUGS("AppInit") << "grid " << combo_val.asString() << LL_ENDL;

        const std::string username = sInstance->getChild<LLComboBox>("username_combo")->getSimple();
        std::string password = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();
        
		//LLGridManager::getInstance()->setGridChoice(combo_val.asString());

		if (username.empty())
		{
			// user must type in something into the username field
			LLNotificationsUtil::add("MustHaveAccountToLogIn");
		}
		else if (password.empty())
		{
			LLNotificationsUtil::add("MustEnterPasswordToLogIn");
		}
		else if (password.length() > 16 && LLGridManager::getInstance()->isInSecondlife())
		{
			LLNotificationsUtil::add("SecondLifePasswordTooLong");
			password.erase(password.begin() + 16, password.end());
		}
		else
		{
			string_vec_t login_uris;
			LLGridManager::getInstance()->getLoginURIs(login_uris);
			if (std::none_of(login_uris.begin(), login_uris.end(),
				[](auto& uri) -> bool { return boost::algorithm::starts_with(uri, "https://"); }
			))
			{
				//LLSD payload;
				//payload["username"] = username;
				//payload["password"] = password;
				LLNotificationsUtil::add("InsecureLogin",
					LLSD().with("GRID", LLGridManager::getInstance()->getGridLabel()), LLSD(), &connectCallback);
				return;
			}
			connect();
		}
	}
}

//static 
void LLPanelLogin::connectCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option != 0)  return;

	connect();
}

// static
void LLPanelLogin::connect()
{
	if (sInstance && sInstance->mCallback)
	{
		LLPointer<LLCredential> cred;
		BOOL remember;
		getFields(cred, remember);
		std::string identifier_type;
		cred->identifierType(identifier_type);
		LLSD allowed_credential_types;
		LLGridManager::getInstance()->getLoginIdentifierTypes(allowed_credential_types);

		// check the typed in credential type against the credential types expected by the server.
		for (LLSD::array_iterator i = allowed_credential_types.beginArray();
			i != allowed_credential_types.endArray();
			++i)
		{

			if (i->asString() == identifier_type)
			{
				// yay correct credential type
				sInstance->mCallback(0, sInstance->mCallbackData);
				return;
			}
		}

		// Right now, maingrid is the only thing that is picky about
		// credential format, as it doesn't yet allow account (single username)
		// format creds.  - Rox.  James, we wanna fix the message when we change
		// this.
		LLNotificationsUtil::add("InvalidCredentialFormat");
	}
}

// static
void LLPanelLogin::onClickNewAccount(void*)
{
	if (sInstance)
	{
		LLWeb::loadURLExternal(LLGridManager::getInstance()->getCreateAccountURL());
	}
}


// static
void LLPanelLogin::onClickVersion(void*)
{
	LLFloaterReg::showInstance("sl_about"); 
}

//static
void LLPanelLogin::onClickForgotPassword(void*)
{
	if (sInstance)
	{
		LLWeb::loadURLExternal(LLGridManager::getInstance()->getForgotPasswordURL());
	}
}

//static
void LLPanelLogin::onClickHelp(void*)
{
	if (sInstance)
	{
		LLViewerHelp* vhelp = LLViewerHelp::getInstance();
		vhelp->showTopic(vhelp->preLoginTopic());
	}
}

// static
void LLPanelLogin::onPassKey(LLLineEditor* caller, void* user_data)
{
	auto self = static_cast<LLPanelLogin *>(user_data);
	self->mPasswordModified = TRUE;
	if (gKeyboard->getKeyDown(KEY_CAPSLOCK) && sCapslockDidNotification == FALSE)
	{
		// *TODO: use another way to notify user about enabled caps lock, see EXT-6858
		sCapslockDidNotification = TRUE;
	}
}


void LLPanelLogin::updateServer()
{
	if (sInstance)
	{
        // Retrieve grid credentials if we got 'em.
		auto user_combo = sInstance->getChild<LLComboBox>("username_combo");
        user_combo->clearRows();
        user_combo->clear();
        sInstance->getChild<LLUICtrl>("password_edit")->setValue(LLStringUtil::null);

        std::vector<LLSD> lIdentifiers;
        if (gSecAPIHandler->getCredentialIdentifierList(LLGridManager::getInstance()->getGrid(), lIdentifiers))
        {
            for (auto itId = lIdentifiers.cbegin(); itId != lIdentifiers.cend(); ++itId)
            {
                const auto& sdIdentifier = *itId;
                user_combo->addRemovable(LLCredential::usernameFromIdentifier(sdIdentifier), LLSD(sdIdentifier));
            }
            user_combo->sortByName();
            user_combo->selectFirstItem();
        }

        // Want to vanish not only create_new_account_btn, but also the
        // title text over it, so turn on/off the whole layout_panel element.
        sInstance->getChild<LLLayoutPanel>("links")->setVisible(!LLGridManager::getInstance()->getCreateAccountURL().empty());
        sInstance->getChildView("forgot_password_text")->setVisible(!LLGridManager::getInstance()->getForgotPasswordURL().empty());

        // grid changed so show new splash screen (possibly)
        loadLoginPage();
	}
}

void LLPanelLogin::onSelectServer()
{
	// The user twiddled with the grid choice ui.
	// apply the selection to the grid setting.
	LLPointer<LLCredential> credential;

	auto server_combo = getChild<LLComboBox>("server_combo");
	auto server_combo_val = server_combo->getSelectedValue();
	LL_INFOS("AppInit") << "grid "<<server_combo_val.asString()<< LL_ENDL;
	LLGridManager::getInstance()->setGridChoice(server_combo_val.asString());
	addFavoritesToStartLocation();
	
	/*
	 * Determine whether or not the value in the start_location_combo makes sense
	 * with the new grid value.
	 *
	 * Note that some forms that could be in the location combo are grid-agnostic,
	 * such as "MyRegion/128/128/0".  There could be regions with that name on any
	 * number of grids, so leave them alone.  Other forms, such as
	 * https://grid.example.com/region/Party%20Town/20/30/5 specify a particular
	 * grid; in those cases we want to clear the location.
	 */
	auto location_combo = getChild<LLComboBox>("start_location_combo");
	S32 index = location_combo->getCurrentIndex();
	switch (index)
	{
	case 0: // last location
	case 1: // home location
		// do nothing - these are grid-agnostic locations
		break;
		
	default:
		{
			std::string location = location_combo->getValue().asString();
			LLSLURL slurl(location); // generata a slurl from the location combo contents
			if (   slurl.getType() == LLSLURL::LOCATION
				&& slurl.getGrid() != LLGridManager::getInstance()->getGrid()
				)
			{
				// the grid specified by the location is not this one, so clear the combo
				location_combo->setCurrentByIndex(0); // last location on the new grid
				location_combo->setTextEntry(LLStringUtil::null);
			}
		}			
		break;
	}

	updateServer();
}

void LLPanelLogin::onLocationSLURL()
{
	auto location_combo = getChild<LLComboBox>("start_location_combo");
	std::string location = location_combo->getValue().asString();
	LL_DEBUGS("AppInit") << location << LL_ENDL;

	LLStartUp::setStartSLURL(location); // calls onUpdateStartSLURL, above 
}

void LLPanelLogin::onSelectUser()
{
	auto password_ctrl = sInstance->getChild<LLLineEditor>("password_edit");

	auto userCred = gSecAPIHandler->loadCredential(LLGridManager::getInstance()->getGrid(), getIdentifier());
	if (userCred.notNull())
	{
		// If the password exists in the credential, set the password field with a filler to get some stars
		const auto authenticator = userCred->getAuthenticator();
		if ( authenticator.isMap() && authenticator.has("secret") && authenticator["secret"].asString().size() > 0 )
		{
			// This is a MD5 hex digest of a password.
			// We don't actually use the password input field, 
			// fill it with MAX_PASSWORD characters so we get a 
			// nice row of asterixes.
			password_ctrl->setValue(LLStringExplicit("123456789!123456"));
		}
		else
		{
			password_ctrl->setValue(LLStringUtil::null);		
		}
	}

	addFavoritesToStartLocation();
}

bool LLPanelLogin::onRemoveUser(const LLSD& value)
{
	// Cancel the removal and ask the user for confirmation
	sInstance->getChild<LLComboBox>("username_combo")->hideList();
	LLNotificationsUtil::add("RemoveLoginCredential", 
		LLSD().with("NAME", LLCredential::usernameFromIdentifier(value)),
		value, 
		boost::bind(&LLPanelLogin::onRemoveUserResponse, this, _1, _2));
	return false;
}

void LLPanelLogin::onRemoveUserResponse(const LLSD& notification, const LLSD& response)
{
	if (LLNotificationsUtil::getSelectedOption(notification, response) == 0) // OK
	{
		const LLSD& identifier = notification["payload"];
		const std::string username = LLCredential::usernameFromIdentifier(identifier);

		auto user_combo = sInstance->getChild<LLComboBox>("username_combo");

		auto item = user_combo->getListCtrl()->getItemByLabel(username);
		if (item)
		{
			S32 idxCur = user_combo->getListCtrl()->getItemIndex(item);
			user_combo->remove(idxCur);
			if (user_combo->getItemCount() > 0)
			{
				user_combo->selectNthItem(llmin(idxCur, user_combo->getItemCount() - 1));
			}
			else
			{
				user_combo->clear();
				sInstance->getChild<LLUICtrl>("password_edit")->setValue(LLStringUtil::null);
			}
		}

		gSecAPIHandler->deleteCredential(LLGridManager::getInstance()->getGrid(), identifier);
	}
}

void LLPanelLogin::refreshGridList()
{
	auto server_choice_combo = getChild<LLComboBox>("server_combo");
	server_choice_combo->removeall();
	
	const std::string& current_grid = LLGridManager::getInstance()->getGrid();
	auto known_grids = LLGridManager::getInstance()->getKnownGrids();
	for (auto grid_choice = known_grids.begin();
		 grid_choice != known_grids.end(); ++grid_choice)
	{
		if (!grid_choice->first.empty() && current_grid != grid_choice->first)
		{
			server_choice_combo->add(grid_choice->second, grid_choice->first);
		}
	}
	server_choice_combo->sortByName();
	server_choice_combo->addSeparator(ADD_TOP);
	server_choice_combo->add(LLGridManager::getInstance()->getGridLabel(), current_grid, ADD_TOP);
	server_choice_combo->selectFirstItem();
	updateServer();
}
