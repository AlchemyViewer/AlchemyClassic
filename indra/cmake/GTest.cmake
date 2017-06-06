include(FindGTest)

use_prebuilt_binary(google_test)

set(GTEST_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)

set(GTEST_LIBRARIES gtest)

set(GTEST_MAIN_LIBRARIES gtest_main)
  
set(GTEST_BOTH_LIBRARIES gtest gtest_main)