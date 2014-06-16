/*
 * @file llnotificationcenter.h
 * @brief Mac OSX Notification Center support
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder.roxley@phoenixviewer.com>
 * ----------------------------------------------------------------------------
 */

#ifndef LL_NOTIFICATIONCENTER_H
#define LL_NOTIFICATIONCENTER_H

#ifndef LL_DARWIN
#error "This file should only be included when building for mac!"
#else

#include <string>

class LLOSXNotificationCenter
{
public:
	static void sendNotification(const std::string& title, const std::string& body);
};

#endif // LL_DARWIN
#endif // LL_NOTIFICATIONCENTER_H
