# -*- cmake -*-
include(Prebuilt)

set(CURL_FIND_QUIETLY ON)
set(CURL_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindCURL)
else (USESYSTEMLIBS)
  use_prebuilt_binary(curl)
  if (WINDOWS)
    set(CURL_LIBRARIES 
    debug libcurl_a_debug.lib
    optimized libcurl_a.lib)
  elseif (LINUX)
    set(CURL_LIBRARIES curl)
  else (DARWIN)
    set(CURL_LIBRARIES curl iconv)
  endif ()
  set(CURL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
endif (USESYSTEMLIBS)
