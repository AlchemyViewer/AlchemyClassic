/** 
 * @file llvfs_objc.cpp
 * @brief Cocoa implementation of directory utilities for Mac OS X
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

#ifndef LL_DARWIN
#  error "This file should only be included when building on mac!"
#else

#import "llvfs-objc.h"
#import <Cocoa/Cocoa.h>
#import <string>

namespace LLDarwin {

std::string getSystemTempFolder()
{
    NSString * tempDir = NSTemporaryDirectory();
    if (tempDir == nil)
        tempDir = @"/tmp";
    std::string result([tempDir UTF8String]);
    
    return result;
}

//findSystemDirectory scoped exclusively to this file. 
std::string findSystemDirectory(NSSearchPathDirectory searchPathDirectory,
								NSSearchPathDomainMask domainMask)
{
	std::string result = "";
    NSString *path = nil;
    
    // Search for the path
    NSArray* paths = NSSearchPathForDirectoriesInDomains(searchPathDirectory,
                                                         domainMask,
                                                         YES);
    if ([paths count])
    {
        path = [paths objectAtIndex:0];
        //HACK:  Always attempt to create directory, ignore errors.
        NSError *error = nil;

        [[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:&error];

        
		result = [path UTF8String];
    }
    return result;
}

std::string getSystemExecutableFolder()
{
    NSString *bundlePath = [[NSBundle mainBundle] executablePath];
    std::string result([bundlePath UTF8String]);

    return result;
}

std::string getSystemResourceFolder()
{
    NSString *bundlePath = [[NSBundle mainBundle] resourcePath];
    std::string result([bundlePath UTF8String]);
    
    return result;
}

std::string getSystemCacheFolder()
{
    return findSystemDirectory(NSCachesDirectory,
							   NSUserDomainMask);
}

std::string getSystemApplicationSupportFolder()
{
    return findSystemDirectory(NSApplicationSupportDirectory,
							   NSUserDomainMask);
}

} // namespace LLDarwin

#endif // LL_DARWIN
