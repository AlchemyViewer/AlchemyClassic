#ifndef LL_LLCOMMONPRECOMPILED_H
#define LL_LLCOMMONPRECOMPILED_H

#include "linden_common.h"

#include "llwin32headerslean.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <shared_mutex>
#include <typeinfo>
#include <type_traits>

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
#include <boost/scoped_ptr.hpp>
#include <boost/static_assert.hpp>
#include <boost/signals2.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/visit_each.hpp>
#include <boost/ref.hpp>            // reference_wrapper
#include <boost/static_assert.hpp>

#endif // LL_LLCOMMONPRECOMPILED_H