/**
 * @file lluiprecompiled.h
 * @brief Includes common headers
 *
 * $LicenseInfo:firstyear=2019&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2019, Alchemy Viewer Project, Inc.
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

#ifndef LL_LLUIPRECOMPILED_H
#define LL_LLUIPRECOMPILED_H

#include "linden_common.h"

#include <functional>
#include <map>
#include <limits>
#include <list>
#include <memory>
#include <queue>
#include <vector>

#include "absl/container/node_hash_map.h"
#include <absl/container/flat_hash_map.h>

#include "llhandle.h"
#include "llfasttimer.h"
#include "llframetimer.h"
#include "llheteromap.h"
#include "llinitparam.h"
#include "llinstancetracker.h"
#include "llpointer.h"
#include "llregistry.h"
#include "llsd.h"
#include "llsingleton.h"
#include "llstl.h"
#include "llstring.h"
#include "lltreeiterators.h"

#include "llcoord.h"
#include "llrect.h"
#include "llquaternion.h"
#include "v2math.h"
#include "v4color.h"
#include "m3math.h"

#include "lldir.h"

#include "llgl.h"
#include "llfontgl.h"
#include "llglslshader.h"
#include "llrender2dutils.h"

#include "llcontrol.h"
#include "llxmlnode.h"

#endif // LL_LLUIPRECOMPILED_H
