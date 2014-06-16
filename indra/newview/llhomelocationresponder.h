/** 
 * @file llhomelocationresponder.h
 * @author Meadhbh Hamrick
 * @brief Processes responses to the HomeLocation CapReq
 *
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
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
 
 /* Macro Definitions */
#ifndef LL_LLHOMELOCATIONRESPONDER_H
#define LL_LLHOMELOCATIONRESPONDER_H

/* File Inclusions */
#include "llhttpclient.h"

/* Typedef, Enum, Class, Struct, etc. */
class LLHomeLocationResponder : public LLHTTPClient::Responder
{
	LOG_CLASS(LLHomeLocationResponder);
private:
	/* virtual */ void httpSuccess();
	/* virtual */ void httpFailure();
};

#endif
