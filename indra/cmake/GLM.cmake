# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  use_prebuilt_binary(glm)
  set(GLM_INCLUDE_DIR "${LIBS_PREBUILT_DIR}/include")
endif (NOT USESYSTEMLIBS)