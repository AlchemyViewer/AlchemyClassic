# -*- cmake -*-

include(Prebuilt)

set(JSONCPP_FIND_QUIETLY ON)
set(JSONCPP_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindJsonCpp)
else (USESYSTEMLIBS)
  use_prebuilt_binary(jsoncpp)
  if (WINDOWS)
    if(MSVC10)
      set(JSONCPP_LIBRARIES 
        debug json_vc100debug_libmt.lib
        optimized json_vc100_libmt)
    elseif(MSVC12)
      set(JSONCPP_LIBRARIES 
        debug json_vc120debug_libmt.lib
        optimized json_vc120_libmt)
    endif(MSVC10)
  elseif (DARWIN)
    set(JSONCPP_LIBRARIES libjson_darwin_libmt.a)
  elseif (LINUX)
    set(JSONCPP_LIBRARIES libjson_linux-gcc-4.1.3_libmt.a)
  endif (WINDOWS)
  set(JSONCPP_INCLUDE_DIR "${LIBS_PREBUILT_DIR}/include/jsoncpp" "${LIBS_PREBUILT_DIR}/include/json")
endif (USESYSTEMLIBS)
