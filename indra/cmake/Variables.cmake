# -*- cmake -*-
#
# Definitions of variables used throughout the Second Life build
# process.
#
# Platform variables:
#
#   DARWIN  - Mac OS X
#   LINUX   - Linux
#   WINDOWS - Windows

# Relative and absolute paths to subtrees.
if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

if(NOT DEFINED COMMON_CMAKE_DIR)
    set(COMMON_CMAKE_DIR "${CMAKE_SOURCE_DIR}/cmake")
endif(NOT DEFINED COMMON_CMAKE_DIR)

set(LIBS_CLOSED_PREFIX)
set(LIBS_OPEN_PREFIX)
set(SCRIPTS_PREFIX ../scripts)
set(VIEWER_PREFIX)
set(INTEGRATION_TESTS_PREFIX)
option(LL_TESTS "Build and run unit and integration tests (disable for build timing runs to reduce variation" OFF)

# Compiler and toolchain options
option(USESYSTEMLIBS "Use libraries from your system rather than Linden-supplied prebuilt libraries." OFF)
option(INCREMENTAL_LINK "Use incremental linking on win32 builds (enable for faster links on some machines)" OFF)
option(USE_PRECOMPILED_HEADERS "Enable use of precompiled header directives where supported." ON)
option(USE_LTO "Enable Whole Program Optimization and related folding and binary reduction routines" OFF)
option(USE_ASAN "Enable address sanitizer for detection of memory issues" OFF)
option(USE_LEAKSAN "Enable address sanitizer for detection of memory leaks" OFF)
option(USE_UBSAN "Enable undefined behavior sanitizer" OFF)
option(USE_THDSAN "Enable thread sanitizer for detection of thread data races and mutexing issues" OFF)
if(USE_ASAN AND USE_LEAKSAN)
  message(FATAL_ERROR "You may only enable either USE_ASAN or USE_LEAKSAN not both")
elseif((USE_ASAN OR USE_LEAKSAN) AND USE_THDSAN)
  message(FATAL_ERROR "Address and Leak sanitizers are incompatible with thread sanitizer")
endif(USE_ASAN AND USE_LEAKSAN)

option(UNATTENDED "Disable use of uneeded tooling for automated builds" OFF)

# Media Plugins
option(ENABLE_MEDIA_PLUGINS "Turn off building media plugins if they are imported by third-party library mechanism" ON)

# Mallocs
option(USE_TCMALLOC "Build the viewer with google tcmalloc" OFF)
option(USE_TBBMALLOC "Build the viewer with intel tbbmalloc" OFF)
if (USE_TCMALLOC AND USE_TBBMALLOC)
  message(FATAL_ERROR "Only one malloc may be enabled at a time.")
endif (USE_TCMALLOC AND USE_TBBMALLOC)

# Audio Engines
option(FMODSTUDIO "Build with support for the FMOD Studio audio engine" OFF)

# Proprietary Library Features
option(NVAPI "Use nvapi driver interface library" OFF)

if(LIBS_CLOSED_DIR)
  file(TO_CMAKE_PATH "${LIBS_CLOSED_DIR}" LIBS_CLOSED_DIR)
else(LIBS_CLOSED_DIR)
  set(LIBS_CLOSED_DIR ${CMAKE_SOURCE_DIR}/${LIBS_CLOSED_PREFIX})
endif(LIBS_CLOSED_DIR)
if(LIBS_COMMON_DIR)
  file(TO_CMAKE_PATH "${LIBS_COMMON_DIR}" LIBS_COMMON_DIR)
else(LIBS_COMMON_DIR)
  set(LIBS_COMMON_DIR ${CMAKE_SOURCE_DIR}/${LIBS_OPEN_PREFIX})
endif(LIBS_COMMON_DIR)
set(LIBS_OPEN_DIR ${LIBS_COMMON_DIR})

set(SCRIPTS_DIR ${CMAKE_SOURCE_DIR}/${SCRIPTS_PREFIX})
set(VIEWER_DIR ${CMAKE_SOURCE_DIR}/${VIEWER_PREFIX})

set(AUTOBUILD_INSTALL_DIR ${CMAKE_BINARY_DIR}/packages)

set(LIBS_PREBUILT_DIR ${AUTOBUILD_INSTALL_DIR} CACHE PATH
    "Location of prebuilt libraries.")

if (EXISTS ${CMAKE_SOURCE_DIR}/Server.cmake)
  # We use this as a marker that you can try to use the proprietary libraries.
  set(INSTALL_PROPRIETARY ON CACHE BOOL "Install proprietary binaries")
endif (EXISTS ${CMAKE_SOURCE_DIR}/Server.cmake)
set(TEMPLATE_VERIFIER_OPTIONS "" CACHE STRING "Options for scripts/template_verifier.py")
set(TEMPLATE_VERIFIER_MASTER_URL "http://bitbucket.org/lindenlab/master-message-template/raw/tip/message_template.msg" CACHE STRING "Location of the master message template")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Build type.  One of: Debug Release RelWithDebInfo" FORCE)
endif (NOT CMAKE_BUILD_TYPE)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(WINDOWS ON BOOL FORCE)
  if (WORD_SIZE EQUAL 64)
    set(ARCH x86_64 CACHE STRING "Viewer Architecture")
    set(LL_ARCH ${ARCH}_win64)
    set(LL_ARCH_DIR ${ARCH}-win64)
    set(WORD_SIZE 64)
    set(AUTOBUILD_PLATFORM_NAME "windows64" CACHE STRING "Autobuild Platform Name")
  else (WORD_SIZE EQUAL 64)
    set(ARCH i686 CACHE STRING "Viewer Architecture")
    set(LL_ARCH ${ARCH}_win32)
    set(LL_ARCH_DIR ${ARCH}-win32)
    set(WORD_SIZE 32)
    set(AUTOBUILD_PLATFORM_NAME "windows" CACHE STRING "Autobuild Platform Name")
  endif (WORD_SIZE EQUAL 64)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(LINUX ON BOOl FORCE)

  # If someone has specified a word size, use that to determine the
  # architecture.  Otherwise, let the architecture specify the word size.
  if (WORD_SIZE EQUAL 32)
    #message(STATUS "WORD_SIZE is 32")
    set(ARCH i686)
    set(AUTOBUILD_PLATFORM_NAME "linux" CACHE STRING "Autobuild Platform Name")
  elseif (WORD_SIZE EQUAL 64)
    #message(STATUS "WORD_SIZE is 64")
    set(ARCH x86_64)
    set(AUTOBUILD_PLATFORM_NAME "linux64" CACHE STRING "Autobuild Platform Name")
  else (WORD_SIZE EQUAL 32)
    #message(STATUS "WORD_SIZE is UNDEFINED")
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
      message(STATUS "Size of void pointer is detected as 8; ARCH is 64-bit")
      set(WORD_SIZE 64)
      set(AUTOBUILD_PLATFORM_NAME "linux64" CACHE STRING "Autobuild Platform Name")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
      message(STATUS "Size of void pointer is detected as 4; ARCH is 32-bit")
      set(WORD_SIZE 32)
    else()
      message(FATAL_ERROR "Unkown Architecture!")
    endif (CMAKE_SIZEOF_VOID_P EQUAL 8)
  endif (WORD_SIZE EQUAL 32)

  if (WORD_SIZE EQUAL 32)
    set(DEB_ARCHITECTURE i386)
    set(FIND_LIBRARY_USE_LIB64_PATHS OFF)
    set(CMAKE_SYSTEM_LIBRARY_PATH /usr/lib32 ${CMAKE_SYSTEM_LIBRARY_PATH})
  else (WORD_SIZE EQUAL 32)
    set(DEB_ARCHITECTURE amd64)
    set(FIND_LIBRARY_USE_LIB64_PATHS ON)
  endif (WORD_SIZE EQUAL 32)

  execute_process(COMMAND dpkg-architecture -a${DEB_ARCHITECTURE} -qDEB_HOST_MULTIARCH 
      RESULT_VARIABLE DPKG_RESULT
      OUTPUT_VARIABLE DPKG_ARCH
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  #message (STATUS "DPKG_RESULT ${DPKG_RESULT}, DPKG_ARCH ${DPKG_ARCH}")
  if (DPKG_RESULT EQUAL 0)
    set(CMAKE_LIBRARY_ARCHITECTURE ${DPKG_ARCH})
    set(CMAKE_SYSTEM_LIBRARY_PATH /usr/lib/${DPKG_ARCH} /usr/local/lib/${DPKG_ARCH} ${CMAKE_SYSTEM_LIBRARY_PATH})
  endif (DPKG_RESULT EQUAL 0)

  include(ConfigurePkgConfig)

  set(LL_ARCH ${ARCH}_linux)
  set(LL_ARCH_DIR ${ARCH}-linux)

  if (INSTALL_PROPRIETARY)
    # Only turn on headless if we can find osmesa libraries.
    include(FindPkgConfig)
    #pkg_check_modules(OSMESA osmesa)
    #if (OSMESA_FOUND)
    #  set(BUILD_HEADLESS ON CACHE BOOL "Build headless libraries.")
    #endif (OSMESA_FOUND)
  endif (INSTALL_PROPRIETARY)

endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(DARWIN 1)
  
  if (XCODE_VERSION LESS 7.0.0)
    message( FATAL_ERROR "Xcode 7.0.0 or greater is required." )
  endif (XCODE_VERSION LESS 7.0.0)
  message( "Building with " ${CMAKE_OSX_SYSROOT} )
  set(CMAKE_OSX_DEPLOYMENT_TARGET 10.8)

  set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")
  set(CMAKE_XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL 3)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_STRICT_ALIASING NO)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_FAST_MATH NO)
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14")
  if (USE_AVX2)
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS avx2)
  elseif (USE_AVX)
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS avx)
  else ()
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS sse4.1)
  endif ()

  if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf-with-dsym)
  else (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf)
  endif (${CMAKE_BUILD_TYPE} STREQUAL "Release")

  set(WORD_SIZE 64)
  if (NOT CMAKE_OSX_ARCHITECTURES)
    if (WORD_SIZE EQUAL 64)
      set(CMAKE_OSX_ARCHITECTURES x86_64)
    else (WORD_SIZE EQUAL 64)
      set(CMAKE_OSX_ARCHITECTURES i386)
    endif (WORD_SIZE EQUAL 64)
  endif (NOT CMAKE_OSX_ARCHITECTURES)

  if (WORD_SIZE EQUAL 64)
    set(ARCH x86_64)
  else (WORD_SIZE EQUAL 64)
    set(ARCH i386)
  endif (WORD_SIZE EQUAL 64)
  set(LL_ARCH ${ARCH}_darwin)
  set(LL_ARCH_DIR universal-darwin)
  set(AUTOBUILD_PLATFORM_NAME "darwin" CACHE STRING "Autobuild Platform Name")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Default deploy grid
set(GRID agni CACHE STRING "Target Grid")

set(VIEWER_CHANNEL "Alchemy Test" CACHE STRING "Viewer Channel Name")
set(VERSION_BUILD "0" CACHE STRING "Revision number passed in from the outside")

option(ENABLE_SIGNING "Enable signing the viewer" OFF)
set(SIGNING_IDENTITY "" CACHE STRING "Specifies the signing identity to use, if necessary.")

source_group("CMake Rules" FILES CMakeLists.txt)

mark_as_advanced(AUTOBUILD_PLATFORM_NAME)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
