# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS AND NOT DARWIN)
  use_prebuilt_binary(glew)
  if (WINDOWS)
    set(GLEW_LIBRARIES
      debug glew32sd.lib
      optimized glew32s.lib)
  elseif (LINUX)
    set(GLEW_LIBRARIES libGLEW.a)
  endif (WINDOWS)
  set(GLEW_INCLUDE_DIR "${LIBS_PREBUILT_DIR}/include")
endif (NOT USESYSTEMLIBS AND NOT DARWIN)