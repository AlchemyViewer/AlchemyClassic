/**
 * @file llnotificationofferhandler.cpp
 * @brief Notification Handler Class for Simple Notifications and Notification Tips
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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


#include "llviewerprecompiledheaders.h" // must be first include

#include "llnotificationhandler.h"
#include "lltoastnotifypanel.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llnotificationmanager.h"
#include "llnotifications.h"
#include "llscriptfloater.h"
#include "llimview.h"
#include "llnotificationsutil.h"
// [RLVa:KB] - Checked: 2013-05-09 (RLVa-1.4.9)
#include "rlvactions.h"
// [/RLVa:KB]

using namespace LLNotificationsUI;

//--------------------------------------------------------------------------
LLOfferHandler::LLOfferHandler()
:	LLCommunicationNotificationHandler("Offer", "offer")
{
	// Getting a Channel for our notifications
	LLScreenChannel* channel = LLChannelManager::getInstance()->createNotificationChannel();
	if(channel)
	{
		channel->setControlHovering(true);
		mChannel = channel->getHandle();
	}
}

//--------------------------------------------------------------------------
LLOfferHandler::~LLOfferHandler()
{
}

//--------------------------------------------------------------------------
void LLOfferHandler::initChannel()
{
	S32 channel_right_bound = gViewerWindow->getWorldViewRectScaled().mRight - gSavedSettings.getS32("NotificationChannelRightMargin");
	S32 channel_width = gSavedSettings.getS32("NotifyBoxWidth");
	mChannel.get()->init(channel_right_bound - channel_width, channel_right_bound);
}

//--------------------------------------------------------------------------
bool LLOfferHandler::processNotification(const LLNotificationPtr& notification)
{
	if(mChannel.isDead())
	{
		return false;
	}

	// arrange a channel on a screen
	if(!mChannel.get()->getVisible())
	{
		initChannel();
	}


	if( notification->getPayload().has("give_inventory_notification")
		&& notification->getPayload()["give_inventory_notification"].asBoolean() == false)
	{
		// This is an original inventory offer, so add a script floater
		LLScriptFloaterManager::instance().onAddNotification(notification->getID());
	}
	else
	{
		bool add_notif_to_im = notification->canLogToIM() && notification->hasFormElements();

		if (add_notif_to_im)
		{
			const std::string name = LLHandlerUtil::getSubstitutionName(notification);

			LLUUID from_id = notification->getPayload()["from_id"];

			if (!notification->isDND())
			{
				//Will not play a notification sound for inventory and teleport offer based upon chat preference
				bool playSound = (notification->getName() == "UserGiveItem"
								  && gSavedSettings.getBOOL("PlaySoundInventoryOffer"))
								 || ((notification->getName() == "TeleportOffered"
								     || notification->getName() == "TeleportOffered_MaturityExceeded"
								     || notification->getName() == "TeleportOffered_MaturityBlocked")
								    && gSavedSettings.getBOOL("PlaySoundTeleportOffer"));

				if (playSound)
				{
					notification->playSound();
				}
			}

// [RLVa:KB] - Checked: 2013-05-09 (RLVa-1.4.9)
			// Don't spawn an IM session for non-chat related events
			if (RlvActions::canStartIM(from_id))
			{
// [/RLVa:KB]
				LLHandlerUtil::spawnIMSession(name, from_id);
				LLHandlerUtil::addNotifPanelToIM(notification);
// [RLVa:KB] - Checked: 2013-05-09 (RLVa-1.4.9)
			}
			else
			{
				// Since we didn't add this notification to an IM session we want it to get routed to the notification syswell
				add_notif_to_im = false;
			}
// [/RLVa:KB]
		}

		if (!notification->canShowToast())
		{
			LLNotificationsUtil::cancel(notification);
		}
		else if(!notification->canLogToIM() || !LLHandlerUtil::isIMFloaterOpened(notification))
		{
			LLToastNotifyPanel* notify_box = new LLToastNotifyPanel(notification);
			LLToast::Params p;
			p.notif_id = notification->getID();
			p.notification = notification;
			p.panel = notify_box;
			// we not save offer notifications to the syswell floater that should be added to the IM floater
			p.can_be_stored = !add_notif_to_im;
			p.force_show = notification->getOfferFromAgent();
			p.can_fade = notification->canFadeToast();

			LLScreenChannel* channel = dynamic_cast<LLScreenChannel*>(mChannel.get());
			if(channel)
				channel->addToast(p);

		}

		if (notification->canLogToIM())
		{
			// log only to file if notif panel can be embedded to IM and IM is opened
			bool file_only = add_notif_to_im && LLHandlerUtil::isIMFloaterOpened(notification);
			LLHandlerUtil::logToIMP2P(notification, file_only);
		}
	}

	return false;
}

/*virtual*/ void LLOfferHandler::onChange(LLNotificationPtr p)
{
	LLToastNotifyPanel* panelp = LLToastNotifyPanel::getInstance(p->getID());
	if (panelp)
	{
		//
		// HACK: if we're dealing with a notification embedded in IM, update it
		// otherwise remove its toast
		//
		if (dynamic_cast<LLIMToastNotifyPanel*>(panelp))
		{
			panelp->updateNotification();
		}
		else
		{
			// if notification has changed, hide it
			mChannel.get()->removeToastByNotificationID(p->getID());
		}
	}
}


/*virtual*/ void LLOfferHandler::onDelete(LLNotificationPtr notification)
{
	if( notification->getPayload().has("give_inventory_notification")
		&& !notification->getPayload()["give_inventory_notification"] )
	{
		// Remove original inventory offer script floater
		LLScriptFloaterManager::instance().onRemoveNotification(notification->getID());
	}
	else
	{
//		if (notification->canLogToIM() 
//			&& notification->hasFormElements()
//			&& !LLHandlerUtil::isIMFloaterOpened(notification))
// [SL:KB] - Patch: UI-Notifications | Checked: 2013-05-09 (Catznip-3.5)
		// The above test won't necessarily tell us whether the notification went into an IM or to the notification syswell
		//   -> the one and only time we need to decrease the unread IM count is when we've clicked any of the buttons on the *toast*
		//   -> since LLIMFloater::updateMessages() hides the toast when we open the IM (which resets the unread count to 0) we should 
		//      *only* decrease the unread IM count if there's a visible toast since the unread count will be at 0 otherwise anyway
		LLScreenChannel* pChannel = dynamic_cast<LLScreenChannel*>(mChannel.get());
		LLToast* pToast = (pChannel) ? pChannel->getToastByNotificationID(notification->getID()) : NULL;
		if ( (pToast) && (!pToast->getCanBeStored()) )
// [/SL:KB]
		{
			LLHandlerUtil::decIMMesageCounter(notification);
		}
		mChannel.get()->removeToastByNotificationID(notification->getID());
	}
}

