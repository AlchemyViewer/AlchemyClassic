# -*- cmake -*-

# cmake_minimum_required should appear before any
# other commands to guarantee full compatibility
# with the version specified
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

set(ROOT_PROJECT_NAME "Alchemy" CACHE STRING
    "The root project/makefile/solution name. Defaults to Alchemy.")
project(${ROOT_PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Build type.  One of: Debug Release RelWithDebInfo" FORCE)
endif (NOT CMAKE_BUILD_TYPE)

include(Variables)
include(00-Common)
include(BuildVersion)
include(CTest)

add_subdirectory(deps)

add_subdirectory(cmake)
add_subdirectory(${LIBS_OPEN_PREFIX}llaudio)
add_subdirectory(${LIBS_OPEN_PREFIX}llappearance)
add_subdirectory(${LIBS_OPEN_PREFIX}llcharacter)
add_subdirectory(${LIBS_OPEN_PREFIX}llcommon)
add_subdirectory(${LIBS_OPEN_PREFIX}llcorehttp)
add_subdirectory(${LIBS_OPEN_PREFIX}llimage)
add_subdirectory(${LIBS_OPEN_PREFIX}llkdu)
add_subdirectory(${LIBS_OPEN_PREFIX}llimagej2coj)
add_subdirectory(${LIBS_OPEN_PREFIX}llinventory)
add_subdirectory(${LIBS_OPEN_PREFIX}llmath)
add_subdirectory(${LIBS_OPEN_PREFIX}llmessage)
add_subdirectory(${LIBS_OPEN_PREFIX}llprimitive)
add_subdirectory(${LIBS_OPEN_PREFIX}llrender)
add_subdirectory(${LIBS_OPEN_PREFIX}llvfs)
add_subdirectory(${LIBS_OPEN_PREFIX}llwindow)
add_subdirectory(${LIBS_OPEN_PREFIX}llxml)

if (WINDOWS AND EXISTS ${LIBS_CLOSED_DIR}copy_win_scripts)
  add_subdirectory(${LIBS_CLOSED_PREFIX}copy_win_scripts)
endif (WINDOWS AND EXISTS ${LIBS_CLOSED_DIR}copy_win_scripts)

if (DARWIN)
  add_subdirectory(${LIBS_OPEN_PREFIX}lldarwin)
endif(DARWIN)

add_custom_target(viewer)

add_subdirectory(${LIBS_OPEN_PREFIX}llplugin)
add_subdirectory(${LIBS_OPEN_PREFIX}llui)
add_subdirectory(${LIBS_OPEN_PREFIX}viewer_components)

# Legacy C++ tests. Build and run if LL_TESTS is true.
if (LL_TESTS)
  add_subdirectory(${VIEWER_PREFIX}test)
endif (LL_TESTS)

# viewer media plugins
if (ENABLE_MEDIA_PLUGINS)
  add_subdirectory(${LIBS_OPEN_PREFIX}media_plugins)
endif (ENABLE_MEDIA_PLUGINS)

if (LINUX)
  if (INSTALL_PROPRIETARY)
      include(LLAppearanceUtility)
      add_subdirectory(${LLAPPEARANCEUTILITY_SRC_DIR} ${LLAPPEARANCEUTILITY_BIN_DIR})
  endif (INSTALL_PROPRIETARY)
elseif (DARWIN)
  # add crash logger here in the future
elseif (WINDOWS)
  # add crash logger here in the future
endif (LINUX)

add_subdirectory(${VIEWER_PREFIX}newview)
add_dependencies(viewer ${VIEWER_BINARY_NAME})

add_subdirectory(${VIEWER_PREFIX}doxygen EXCLUDE_FROM_ALL)

if (WINDOWS)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT ${VIEWER_BINARY_NAME})
endif (WINDOWS)

if (LL_TESTS)
  # Define after the custom targets are created so
  # individual apps can add themselves as dependencies
  add_subdirectory(${INTEGRATION_TESTS_PREFIX}integration_tests)
endif (LL_TESTS)

