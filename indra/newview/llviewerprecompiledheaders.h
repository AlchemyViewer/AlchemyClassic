/** 
 * @file llviewerprecompiledheaders.h
 * @brief precompiled headers for newview project
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
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


#ifndef LL_LLVIEWERPRECOMPILEDHEADERS_H
#define LL_LLVIEWERPRECOMPILEDHEADERS_H

// This file MUST be the first one included by each .cpp file
// in viewer.
// It is used to precompile headers for improved build speed.

#include "linden_common.h"

#include "llwin32headerslean.h"

#include <algorithm>
#include <atomic>
#if LL_WINDOWS
#include <charconv>
#endif
#include <condition_variable>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

#include <absl/container/node_hash_set.h>
#include <absl/container/node_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/signals2.hpp>
#include <boost/shared_ptr.hpp>

// Library headers from llcommon project:
#include "indra_constants.h"
#include "llallocator.h"
#include "llapp.h"
#include "llapr.h"
#include "llatomic.h"
#include "llcriticaldamp.h"
#include "llcoros.h"
#include "lldate.h"
#include "lldefs.h"
#include "lldepthstack.h"
#include "llerror.h"
#include "llexception.h"
#include "llfasttimer.h"
#include "llfile.h"
#include "llformat.h"
#include "llframetimer.h"
#include "llhandle.h"
#include "llinitparam.h"
#include "llinstancetracker.h"
#include "llmemory.h"
#include "llmutex.h"
#include "llpointer.h"
#include "llprocessor.h"
#include "llqueuedthread.h"
#include "llrefcount.h"
#include "llregistry.h"
#include "llsafehandle.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llsingleton.h"
#include "llsortedvector.h"
#include "llstl.h"
#include "llstrider.h"
#include "llstring.h"
#include "llsys.h"
#include "llthread.h"
#include "lltimer.h"
#include "lltrace.h"
#include "lluuid.h"
#include "llworkerthread.h"
#include "stdtypes.h"

// Library includes from llmath project
#include "llmath.h"
#include "llbboxlocal.h"
#include "llcamera.h"
#include "llcoord.h"
#include "llcoordframe.h"
#include "llcrc.h"
#include "llplane.h"
#include "llquantize.h"
#include "llrand.h"
#include "llrect.h"
#include "llvolume.h"
#include "m3math.h"
#include "m4math.h"
#include "llquaternion.h"
#include "v2math.h"
#include "v3color.h"
#include "v3dmath.h"
#include "v3math.h"
#include "v4color.h"
#include "v4coloru.h"
#include "v4math.h"
#include "xform.h"
#include "llmatrix4a.h"

// Library includes from llvfs
#include "lldir.h"
#include "llvfs.h"

// Library includes from llmessage project
#include "llcachename.h"
#include "llcorehttputil.h"
#include "message.h"

// Library includes from llrender project
#include "llglheaders.h"
#include "llglslshader.h"
#include "llfontgl.h"
#include "llgl.h"
#include "llimagegl.h"
#include "llrender.h"
#include "llvertexbuffer.h"

// Library includes from llxml project
#include "llcontrol.h"

// Library includes from llui project
#include "llui.h"
#include "llview.h"
#include "lluictrl.h"
#include "llpanel.h"
#include "llfloater.h"

#endif
