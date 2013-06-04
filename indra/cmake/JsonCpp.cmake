# -*- cmake -*-

include(Prebuilt)

set(JSONCPP_FIND_QUIETLY ON)
set(JSONCPP_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindJsonCpp)
else (STANDALONE)
  use_prebuilt_binary(jsoncpp)
  if (WINDOWS)
    if(MSVC10)
      set(JSONCPP_LIBRARIES 
        debug json_vc100debug_libmt.lib
        optimized json_vc100_libmt)
    elseif(MSVC11)
      set(JSONCPP_LIBRARIES 
        debug json_vc110debug_libmt.lib
        optimized json_vc110_libmt)
    endif(MSVC10)
  elseif (DARWIN)
    set(JSONCPP_LIBRARIES libjson_linux-gcc-4.0.1_libmt.a)
  elseif (LINUX)
    set(JSONCPP_LIBRARIES libjson_linux-gcc-4.1.3_libmt.a)
  endif (WINDOWS)
  set(JSONCPP_INCLUDE_DIR "${LIBS_PREBUILT_DIR}/include/jsoncpp" "${LIBS_PREBUILT_DIR}/include/json")
endif (STANDALONE)
