# -*- cmake -*-
include(Linking)
include(Prebuilt)

if (LINUX)
  option(USE_OPENAL "Enable OpenAL" ON)
else (LINUX)
  option(USE_OPENAL "Enable OpenAL" OFF)
endif (LINUX)

if (USE_OPENAL)
  set(OPENAL_LIB_INCLUDE_DIRS "${LIBS_PREBUILT_DIR}/include/AL")
  if (USESYSTEMLIBS)
    include(FindPkgConfig)
    include(FindOpenAL)
    pkg_check_modules(OPENAL_LIB REQUIRED openal)
    pkg_check_modules(FREEALUT_LIB REQUIRED freealut)
  else (USESYSTEMLIBS)
    use_prebuilt_binary(openal)
  endif (USESYSTEMLIBS)
  if(WINDOWS)
    set(OPENAL_LIBRARIES 
      OpenAL32
      alut
    )
  else()
    set(OPENAL_LIBRARIES 
      openal
      alut
    )
  endif()
endif (USE_OPENAL)
