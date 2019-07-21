# -*- cmake -*-
include(Prebuilt)

use_prebuilt_binary(abseil-cpp)
set(ABSEIL_SRC_DIR ${LIBS_PREBUILT_DIR}/abseil-cpp)
set(ABSEIL_BIN_DIR ${CMAKE_BINARY_DIR}/abseil-cpp)

