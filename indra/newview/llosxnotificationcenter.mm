/*
 * @file llnotificationcenter.mm
 * @brief Mac OSX Notification Center support
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder.roxley@phoenixviewer.com>
 * ----------------------------------------------------------------------------
 */

#ifndef LL_DARWIN
#error "This file should only be included when building on mac!"
#else

#import "llosxnotificationcenter.h"
#import <Cocoa/Cocoa.h>

//static
void LLOSXNotificationCenter::sendNotification(const std::string& title, const std::string& body)
{
	if (NSAppKitVersionNumber <= NSAppKitVersionNumber10_7_2) return;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSUserNotification *notification = [[NSUserNotification alloc] init];
	notification.title = [NSString stringWithUTF8String:title.c_str()];
	notification.informativeText = [NSString stringWithUTF8String:body.c_str()];
	notification.soundName = NSUserNotificationDefaultSoundName;
	
	[[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
	[notification release];
	[pool release];
}

#endif // LL_DARWIN
