# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  use_prebuilt_binary(glew)
  if (WINDOWS)
    set(GLEW_LIBRARIES
      debug glew32sd.lib
      optimized glew32s.lib)
  elseif (DARWIN OR LINUX)
	# TODO
    #set(GLEW_LIBRARIES libglew.a)
  endif (WINDOWS)
  set(GLEW_INCLUDE_DIR "${LIBS_PREBUILT_DIR}/include")
endif (NOT USESYSTEMLIBS)