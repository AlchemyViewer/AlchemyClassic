# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  if (NOT DARWIN)
    use_prebuilt_binary(glext)
  endif (NOT DARWIN)
  use_prebuilt_binary(glh_linear)
  set(GLEXT_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
endif (NOT USESYSTEMLIBS)
