/**
 * @file llcommonprecompiled.h
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

#ifndef LL_LLCOMMONPRECOMPILED_H
#define LL_LLCOMMONPRECOMPILED_H

#include "linden_common.h"

#ifdef LL_WINDOWS
#  include "llwin32headerslean.h"
#endif // LL_WINDOWS

#include <algorithm>
#include <atomic>
#include <array>
#ifdef LL_WINDOWS
#include <charconv> // MSVC is the only impl with complete charconv support currently
#endif
#include <condition_variable>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <shared_mutex>
#include <typeinfo>
#include <type_traits>
#include <utility>

#include <apr_pools.h>
#include <apr_dso.h>
#include <apr_file_io.h>
#include <apr_network_io.h>
#include <apr_poll.h>
#include <apr_pools.h>
#include <apr_portable.h>
#include <apr_queue.h>
#include <apr_shm.h>
#include <apr_signal.h>
#include <apr_thread_proc.h>

#include <absl/container/node_hash_map.h>
#include <absl/container/flat_hash_map.h>
#include <fmt/format.h>
#include <boost/bind.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/dcoroutine/coroutine.hpp>
#include <boost/dcoroutine/future.hpp>
#include <boost/function.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/regex.hpp>
#include <boost/static_assert.hpp>
#include <boost/signals2.hpp>
#include <boost/visit_each.hpp>
#include <boost/ref.hpp>            // reference_wrapper
#include <boost/static_assert.hpp>

#endif // LL_LLCOMMONPRECOMPILED_H
