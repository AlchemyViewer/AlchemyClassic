# -*- cmake -*-
include(Prebuilt)

use_prebuilt_binary(fmtlib-src)
set(FMT_SRC_DIR ${LIBS_PREBUILT_DIR}/fmt)
set(FMT_BIN_DIR ${CMAKE_BINARY_DIR}/fmt)
