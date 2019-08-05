include(Linking)
include(Prebuilt)

set(APR_FIND_QUIETLY ON)
set(APR_FIND_REQUIRED ON)

set(APRUTIL_FIND_QUIETLY ON)
set(APRUTIL_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindAPR)
else (USESYSTEMLIBS)
  use_prebuilt_binary(apr_suite)
  if (WINDOWS)
    set(APR_LIBRARIES 
      debug ${ARCH_PREBUILT_DIRS_DEBUG}/apr-1.lib
      optimized ${ARCH_PREBUILT_DIRS_RELEASE}/apr-1.lib
      )
    set(APRICONV_LIBRARIES 
      debug ${ARCH_PREBUILT_DIRS_DEBUG}/apriconv-1.lib
      optimized ${ARCH_PREBUILT_DIRS_RELEASE}/apriconv-1.lib
      )
    set(APRUTIL_LIBRARIES 
      debug ${ARCH_PREBUILT_DIRS_DEBUG}/aprutil-1.lib ${APRICONV_LIBRARIES}
      optimized ${ARCH_PREBUILT_DIRS_RELEASE}/aprutil-1.lib ${APRICONV_LIBRARIES}
      )
    list(APPEND APR_LIBRARIES Rpcrt4)
  else ()
    set(APR_LIBRARIES apr-1)
    set(APRUTIL_LIBRARIES aprutil-1)
    set(APRICONV_LIBRARIES iconv)
  endif ()

  set(APR_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/apr-1)

  if (LINUX)
    list(APPEND APRUTIL_LIBRARIES rt)
  endif (LINUX)
endif (USESYSTEMLIBS)
