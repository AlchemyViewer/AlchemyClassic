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
    debug libcurld.lib
    optimized libcurl.lib)
  elseif (LINUX)
    use_prebuilt_binary(libidn)
    set(CURL_LIBRARIES curl idn)
  else (DARWIN)
    use_prebuilt_binary(libidn)
    set(CURL_LIBRARIES curl idn iconv)
  endif ()
  set(CURL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
endif (USESYSTEMLIBS)
