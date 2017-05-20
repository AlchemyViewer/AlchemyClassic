# -*- cmake -*-
#
# Compilation options shared by all Second Life components.

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(CheckCCompilerFlag)
include(CheckPython)
include(Variables)

# Portable compilation flags.
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -DLL_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE
    "-DLL_RELEASE=1 -DLL_RELEASE_FOR_DOWNLOAD=1 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "-DLL_RELEASE=1 -DNDEBUG -DLL_RELEASE_WITH_DEBUG_INFO=1")

# Configure crash reporting
option(RELEASE_CRASH_REPORTING "Enable use of crash reporting in release builds" OFF)
option(NON_RELEASE_CRASH_REPORTING "Enable use of crash reporting in developer builds" OFF)

if(RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DLL_SEND_CRASH_REPORTS=1")
endif()

if(NON_RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DLL_SEND_CRASH_REPORTS=1")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DLL_SEND_CRASH_REPORTS=1")
endif()

# Don't bother with a MinSizeRel build.
set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Release;Debug" CACHE STRING
    "Supported build types." FORCE)

# Platform-specific compilation flags.
if (WINDOWS)
  # Don't build DLLs.
  set(BUILD_SHARED_LIBS OFF)

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /MDd /MP -D_SCL_SECURE_NO_WARNINGS=1"
      CACHE STRING "C++ compiler debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Od /Zi /MD /MP /Ob0 -D_ITERATOR_DEBUG_LEVEL=0"
      CACHE STRING "C++ compiler release-with-debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /Ot /Zi /Zo /MD /MP /Ob2 /Zc:inline -D_ITERATOR_DEBUG_LEVEL=0"
      CACHE STRING "C++ compiler release options" FORCE)

  if (WORD_SIZE EQUAL 32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
  endif (WORD_SIZE EQUAL 32)

  set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /NODEFAULTLIB:LIBCMT")
  set (CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:LIBCMT /NODEFAULTLIB:LIBCMTD /NODEFAULTLIB:MSVCRT")
  set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /NODEFAULTLIB:LIBCMT")
  set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:LIBCMT /NODEFAULTLIB:LIBCMTD /NODEFAULTLIB:MSVCRT")
  
  if (USE_LTO)
    if(INCREMENTAL_LINK)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG:INCREMENTAL")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG:INCREMENTAL")
      set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG:INCREMENTAL")
    else(INCREMENTAL_LINK)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG")
      set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG")
    endif(INCREMENTAL_LINK)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF /OPT:ICF")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS}")
  elseif (INCREMENTAL_LINK)
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /VERBOSE:INCR")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL /VERBOSE:INCR")
  else ()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /INCREMENTAL:NO")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF /INCREMENTAL:NO")
  endif ()

  set(CMAKE_CXX_STANDARD_LIBRARIES "")
  set(CMAKE_C_STANDARD_LIBRARIES "")

  add_definitions(
      /DLL_WINDOWS=1
      /DNOMINMAX
      /DUNICODE
      /D_UNICODE
      /D_CRT_SECURE_NO_WARNINGS
	  /D_CRT_NONSTDC_NO_DEPRECATE
      /D_WINSOCK_DEPRECATED_NO_WARNINGS
      )

  add_compile_options(
      /GS
      /TP
      /W3
      /c
      /Zc:wchar_t
      /Zc:forScope
      /Zc:rvalueCast
      /Zc:throwingNew
      /nologo
      /Oy-
      /fp:fast
      /Zm140
      )

  if (USE_LTO AND NOT INCREMENTAL_LINK)
    add_compile_options(
        /GL
        /Gy
        /Gw
        )
  endif (USE_LTO AND NOT INCREMENTAL_LINK)

  if (USE_AVX)
    add_compile_options(/arch:AVX)
  elseif (USE_AVX2)
    add_compile_options(/arch:AVX2)
  elseif (WORD_SIZE EQUAL 32)
    add_compile_options(/arch:SSE2)
  endif (USE_AVX)

  if (NOT VS_DISABLE_FATAL_WARNINGS)
    add_compile_options(/WX)
  endif (NOT VS_DISABLE_FATAL_WARNINGS)

  # configure win32 API for 7 and above compatibility
  set(WINVER "0x0601" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
  add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")
endif (WINDOWS)


if (LINUX)
  option(CONSERVE_MEMORY "Optimize for memory usage during link stage for memory-starved systems" OFF)
  set(CMAKE_SKIP_RPATH TRUE)

  add_compile_options(
    -fvisibility=hidden
    -fexceptions
    -fno-math-errno
    -fno-strict-aliasing
    -fsigned-char
    -std=gnu++14
    -g
    -pthread
    )

  add_definitions(
    -DLL_LINUX=1
    -DAPPID=secondlife
    -DLL_IGNORE_SIGCHLD
    -D_REENTRANT
    -DGDK_DISABLE_DEPRECATED 
    -DGTK_DISABLE_DEPRECATED
    -DGSEAL_ENABLE
    -DGTK_DISABLE_SINGLE_INCLUDES
    )

  if (USE_LTO)
    add_compile_options(-flto=8)
  endif (USE_LTO)

  if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    if (USE_ASAN)
      add_compile_options(-fsanitize=address)
      link_libraries(-lasan)
    endif (USE_ASAN)

    if (USE_LEAKSAN)
      add_compile_options(-fsanitize=leak)
      link_libraries(-llsan)
    endif (USE_LEAKSAN)

    if (USE_UBSAN)
      add_compile_options(-fsanitize=undefined)
      link_libraries(-lubsan)
    endif (USE_UBSAN)

    if (USE_THDSAN)
      add_compile_options(-fsanitize=thread)
    endif (USE_THDSAN)
  endif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")

  CHECK_C_COMPILER_FLAG(-Og HAS_DEBUG_OPTIMIZATION)
  CHECK_C_COMPILER_FLAG(-fstack-protector-strong HAS_STRONG_STACK_PROTECTOR)
  CHECK_C_COMPILER_FLAG(-fstack-protector HAS_STACK_PROTECTOR)
  if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    if(HAS_STRONG_STACK_PROTECTOR)
      add_compile_options(-fstack-protector-strong)
    elseif(HAS_STACK_PROTECTOR)
      add_compile_options(-fstack-protector)
    endif(HAS_STRONG_STACK_PROTECTOR)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
  endif (${CMAKE_BUILD_TYPE} STREQUAL "Release")

  if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    if (WORD_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)
      add_compile_options(
        -msse2
        -mfpmath=sse
        -march=pentium4)
    endif (WORD_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)

    if (HAS_DEBUG_OPTIMIZATION)
      set(CMAKE_CXX_FLAGS_DEBUG "-Og ${CMAKE_CXX_FLAGS_DEBUG}")
    else (HAS_DEBUG_OPTIMIZATION)
      set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fno-inline ${CMAKE_CXX_FLAGS_DEBUG}")
    endif (HAS_DEBUG_OPTIMIZATION)
    if (USE_ASAN OR USE_LEAKSAN OR USE_UBSAN OR USE_THDSAN)
      if (HAS_DEBUG_OPTIMIZATION)
        set(CMAKE_CXX_FLAGS_RELEASE "-Og ${CMAKE_CXX_FLAGS_RELEASE}")
      else (HAS_DEBUG_OPTIMIZATION)
        set(CMAKE_CXX_FLAGS_RELEASE "-O1 -fno-omit-frame-pointer ${CMAKE_CXX_FLAGS_RELEASE}")
      endif (HAS_DEBUG_OPTIMIZATION)
    else (USE_ASAN OR USE_LEAKSAN OR USE_UBSAN OR USE_THDSAN)
      set(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")
    endif (USE_ASAN OR USE_LEAKSAN OR USE_UBSAN OR USE_THDSAN)
  elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    if (WORD_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)
      add_compile_options(-msse2 -march=pentium4)
    endif (WORD_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)

    set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CMAKE_CXX_FLAGS_RELEASE}")
  elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
    if (WORD_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)
      add_compile_options(-march=pentium4)
    endif (WORD_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)

    set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")
  endif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")

  # Enable these flags so we have a read only GOT and some linking opts
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,--as-needed ${CMAKE_EXE_LINKER_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,--as-needed ${CMAKE_SHARED_LINKER_FLAGS}")
endif (LINUX)


if (DARWIN)
  add_definitions(-DLL_DARWIN=1)
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-headerpad_max_install_names,-search_paths_first")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
  set(DARWIN_extra_cstar_flags "-g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DARWIN_extra_cstar_flags}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${DARWIN_extra_cstar_flags}")
  # NOTE: it's critical that the optimization flag is put in front.
  # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_C_FLAGS_RELEASE "-O3 ${CMAKE_C_FLAGS_RELEASE}")
endif (DARWIN)


if (LINUX OR DARWIN)
  if (NOT UNIX_DISABLE_FATAL_WARNINGS)
    set(UNIX_WARNINGS "-Werror")
  endif (NOT UNIX_DISABLE_FATAL_WARNINGS)

  if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set(UNIX_WARNINGS "-Wall -Wno-sign-compare -Wno-unused-variable -Wno-maybe-uninitialized ${UNIX_WARNINGS} ")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS} -Wno-reorder")
  elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
    set(UNIX_WARNINGS "-Wall -Wno-sign-compare ${UNIX_WARNINGS} ")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS} -Wno-reorder -Wno-unused-local-typedef")
  elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
    set(UNIX_WARNINGS "-w2 -diag-disable remark -wd68 -wd597 -wd780 -wd858 ${UNIX_WARNINGS} ")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS}")
  endif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${UNIX_WARNINGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${UNIX_CXX_WARNINGS}")

  if (WORD_SIZE EQUAL 32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  elseif (WORD_SIZE EQUAL 64)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  endif (WORD_SIZE EQUAL 32)
endif (LINUX OR DARWIN)


if (USESYSTEMLIBS)
  add_definitions(-DLL_USESYSTEMLIBS=1)

  if (LINUX AND ${ARCH} STREQUAL "i686")
    add_compile_options(-march=pentiumpro)
  endif (LINUX AND ${ARCH} STREQUAL "i686")

else (USESYSTEMLIBS)
  #Define this here so it propagates on all systems to all targets
  #add_definitions(-DBOOST_THREAD_VERSION=4)

  #Uncomment this definition when we can build cleanly against OpenSSL 1.1
  #add_definitions(-DOPENSSL_API_COMPAT=0x10100000L)

  set(${ARCH}_linux_INCLUDES
      atk-1.0
      cairo
      gdk-pixbuf-2.0
      glib-2.0
      gstreamer-0.10
      gtk-2.0
      pango-1.0
      pixman-1
      )
endif (USESYSTEMLIBS)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
