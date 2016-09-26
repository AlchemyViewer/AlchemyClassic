# -*- cmake -*-
#
# Module that checks for C++ features
#

include(WriteCompilerDetectionHeader)

write_compiler_detection_header(
  FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/llcompilerfeatures.h"
  PREFIX LL
  COMPILERS AppleClang Clang GNU MSVC
  FEATURES
  cxx_alignas cxx_alignof cxx_attribute_deprecated cxx_decltype cxx_decltype_auto cxx_nullptr cxx_override cxx_thread_local cxx_range_for cxx_static_assert
)
