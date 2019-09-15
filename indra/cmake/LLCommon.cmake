# -*- cmake -*-

include(APR)
include(Boost)
include(EXPAT)
include(Linking)
include(ZLIB)
include(GooglePerfTools)

set(LLCOMMON_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/llcommon
    ${CMAKE_BINARY_DIR}/llcommon/generated
    ${APRUTIL_INCLUDE_DIR}
    ${APR_INCLUDE_DIR}
    )
set(LLCOMMON_SYSTEM_INCLUDE_DIRS
    ${Boost_INCLUDE_DIRS}
    )

# In order to support using ld.gold on linux, we need to explicitely
# specify all libraries that llcommon uses.
# llcommon uses `clock_gettime' which is provided by librt on linux.
set(LLCOMMON_LIBRARIES llcommon 
    ${BOOST_COROUTINE_LIBRARY} 
    ${BOOST_CONTEXT_LIBRARY} 
    ${BOOST_THREAD_LIBRARY} 
    ${BOOST_SYSTEM_LIBRARY}
    absl::flat_hash_map
    absl::node_hash_map
    fmt::fmt
    ${RT_LIBRARY}
    )

add_definitions(${TCMALLOC_FLAG})
