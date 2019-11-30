# -*- cmake -*-
#
# Compilation options shared by all Second Life components.

#*****************************************************************************
#   It's important to realize that CMake implicitly concatenates
#   CMAKE_CXX_FLAGS with (e.g.) CMAKE_CXX_FLAGS_RELEASE for Release builds. So
#   set switches in CMAKE_CXX_FLAGS that should affect all builds, but in
#   CMAKE_CXX_FLAGS_RELEASE or CMAKE_CXX_FLAGS_RELWITHDEBINFO for switches
#   that should affect only that build variant.
#
#   Also realize that CMAKE_CXX_FLAGS may already be partially populated on
#   entry to this file.
#*****************************************************************************

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(CheckCXXCompilerFlag)
include(Variables)

# Portable compilation flags.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DADDRESS_SIZE=${ADDRESS_SIZE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DADDRESS_SIZE=${ADDRESS_SIZE}")
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -DLL_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE
    "-DLL_RELEASE=1 -DLL_RELEASE_FOR_DOWNLOAD=1 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "-DLL_RELEASE=1 -DNDEBUG -DLL_RELEASE_WITH_DEBUG_INFO=1")

# Don't bother with a MinSizeRel build.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
  set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING
      "Supported build types." FORCE)
else()
  set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Release;Debug" CACHE STRING
      "Supported build types." FORCE)
endif()

# Platform-specific compilation flags.
if (WINDOWS)
  # Don't build DLLs.
  set(BUILD_SHARED_LIBS OFF)

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m${ADDRESS_SIZE}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m${ADDRESS_SIZE}")
  endif ()

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /MDd /EHsc -D_SCL_SECURE_NO_WARNINGS=1")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Od /Zi /MD /Ob0 /EHsc -D_ITERATOR_DEBUG_LEVEL=0")

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS_RELEASE
        "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /Ot /Gy /Zi /MD /Ob3 /Oy- /Zc:inline /EHsc /fp:fast -D_ITERATOR_DEBUG_LEVEL=0")
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE
        "${CMAKE_CXX_FLAGS_RELEASE} /clang:-Ofast /clang:-ffast-math /Oi /Ot /Gy /Zi /MD /Ob2 /Oy- /Zc:inline /EHsc /fp:fast -D_ITERATOR_DEBUG_LEVEL=0")
  endif()

  if (ADDRESS_SIZE EQUAL 32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
  endif (ADDRESS_SIZE EQUAL 32)

  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR FULL_DEBUG_SYMS OR USE_CRASHPAD)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG:FULL")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG:FULL")
  else ()
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG:FASTLINK")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG:FASTLINK")
  endif ()

  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /NODEFAULTLIB:LIBCMT")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:LIBCMT /NODEFAULTLIB:LIBCMTD /NODEFAULTLIB:MSVCRT")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /NODEFAULTLIB:LIBCMT")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:LIBCMT /NODEFAULTLIB:LIBCMTD /NODEFAULTLIB:MSVCRT")
  
  if (USE_LTO)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
      if(INCREMENTAL_LINK)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG:incremental")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG:incremental")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG")
      else(INCREMENTAL_LINK)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG")
      endif(INCREMENTAL_LINK)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /OPT:ICF /INCREMENTAL:NO")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF /OPT:ICF /INCREMENTAL:NO")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
    elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      if(INCREMENTAL_LINK)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=thin -fwhole-program-vtables /clang:-fforce-emit-vtables")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin -fwhole-program-vtables /clang:-fforce-emit-vtables")
      else(INCREMENTAL_LINK)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=full -fwhole-program-vtables /clang:-fforce-emit-vtables")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=full -fwhole-program-vtables /clang:-fforce-emit-vtables")
      endif(INCREMENTAL_LINK)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /OPT:ICF")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF /OPT:ICF")
    endif()
  elseif (INCREMENTAL_LINK)
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /INCREMENTAL")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL")
  else ()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /OPT:ICF /INCREMENTAL:NO")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF /OPT:ICF /INCREMENTAL:NO")
  endif ()

  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    # This is a massive hack and makes me sad. clang-cl fails to find its own builtins library :/ x64 only for now.
    set(CLANG_RT_NAMES clang_rt.builtins-x86_64)
    find_library(CLANG_RT NAMES ${CLANG_RT_NAMES} 
                PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\LLVM\\LLVM]/lib/clang/${CMAKE_CXX_COMPILER_VERSION}/lib/windows 
                [HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\LLVM\\LLVM]/lib/clang/${CMAKE_CXX_COMPILER_VERSION}/lib/windows)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /defaultlib:\"${CLANG_RT}\"")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /defaultlib:\"${CLANG_RT}\"")
  endif()

  set(GLOBAL_CXX_FLAGS 
      "/GS /W3 /c /Zc:__cplusplus  /Zc:forScope /Zc:rvalueCast /Zc:strictStrings /Zc:ternary /Zc:wchar_t /nologo"
      )

  if (USE_AVX2)
    set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} /arch:AVX2")
  elseif (ADDRESS_SIZE EQUAL 64)
    set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} /arch:AVX")
  elseif (ADDRESS_SIZE EQUAL 32)
    set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} /arch:SSE2")
  endif ()

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} /Zc:externConstexpr /Zc:referenceBinding /Zc:throwingNew")
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} /Qvec /Zc:dllexportInlines- /clang:-mprefer-vector-width=128 -fno-strict-aliasing -Wno-ignored-pragma-intrinsic -Wno-unused-local-typedef")
  endif()

  if(FAVOR_AMD AND FAVOR_INTEL)
      message(FATAL_ERROR "Cannot enable FAVOR_AMD and FAVOR_INTEL at the same time")
  elseif(FAVOR_AMD)
      set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} /favor:AMD64")
  elseif(FAVOR_INTEL)
      set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} /favor:INTEL64")
  endif()

  if (NOT VS_DISABLE_FATAL_WARNINGS)
    set(GLOBAL_CXX_FLAGS "${GLOBAL_CXX_FLAGS} /WX")
  endif (NOT VS_DISABLE_FATAL_WARNINGS)

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${GLOBAL_CXX_FLAGS}" CACHE STRING "C++ compiler debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${GLOBAL_CXX_FLAGS}" CACHE STRING "C++ compiler release-with-debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${GLOBAL_CXX_FLAGS}" CACHE STRING "C++ compiler release options" FORCE)

  add_definitions(
      /DLL_WINDOWS=1
      /DNOMINMAX
      /DUNICODE
      /DURI_STATIC_BUILD
      /D_UNICODE
      /D_CRT_SECURE_NO_WARNINGS
      /D_CRT_NONSTDC_NO_DEPRECATE
      /D_WINSOCK_DEPRECATED_NO_WARNINGS
      /D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
      /DBOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
      )

  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    add_definitions(-DBOOST_USE_WINDOWS_H)
  endif()

  # configure win32 API for 7 and above compatibility
  set(WINVER "0x0601" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
  add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")
endif (WINDOWS)


if (LINUX)
  set(CMAKE_SKIP_RPATH TRUE)

  add_compile_options(
    -fvisibility=hidden
    -fexceptions
    -fno-math-errno
    -fno-strict-aliasing
    -fsigned-char
    -g
    -pthread
    -msse4.2
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

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if (USE_ASAN)
      add_compile_options(-fsanitize=address)
      link_libraries(-lasan)
    endif (USE_ASAN)

    if (USE_LEAKSAN)
      add_compile_options(-fsanitize=leak)
      link_libraries(-llsan)
    endif (USE_LEAKSAN)

    if (USE_UBSAN)
      add_compile_options(-fsanitize=undefined -fno-sanitize=vptr)
      link_libraries(-lubsan)
    endif (USE_UBSAN)

    if (USE_THDSAN)
      add_compile_options(-fsanitize=thread)
    endif (USE_THDSAN)
  endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

  CHECK_CXX_COMPILER_FLAG(-Og HAS_DEBUG_OPTIMIZATION)
  CHECK_CXX_COMPILER_FLAG(-fstack-protector-strong HAS_STRONG_STACK_PROTECTOR)
  CHECK_CXX_COMPILER_FLAG(-fstack-protector HAS_STACK_PROTECTOR)
  if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    if(HAS_STRONG_STACK_PROTECTOR)
      add_compile_options(-fstack-protector-strong)
    elseif(HAS_STACK_PROTECTOR)
      add_compile_options(-fstack-protector)
    endif(HAS_STRONG_STACK_PROTECTOR)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
  endif (${CMAKE_BUILD_TYPE} STREQUAL "Release")

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (ADDRESS_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)
      add_compile_options(
        -mfpmath=sse
        -march=pentium4)
    endif (ADDRESS_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)

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
    else ()
      set(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")
    endif ()
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if (ADDRESS_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)
      add_compile_options(-msse2 -march=pentium4)
    endif (ADDRESS_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)

    set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CMAKE_CXX_FLAGS_RELEASE}")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    if (ADDRESS_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)
      add_compile_options(-march=pentium4)
    endif (ADDRESS_SIZE EQUAL 32 AND NOT USESYSTEMLIBS)

    set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")
  endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

  # Enable these flags so we have a read only GOT and some linking opts
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,--as-needed ${CMAKE_EXE_LINKER_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,--as-needed ${CMAKE_SHARED_LINKER_FLAGS}")
endif (LINUX)

if (DARWIN)
  add_definitions(-DLL_DARWIN=1 -DGL_SILENCE_DEPRECATION)
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-headerpad_max_install_names,-search_paths_first")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
  set(DARWIN_extra_cstar_flags "-gdwarf-2 -fobjc-arc -fno-strict-aliasing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DARWIN_extra_cstar_flags}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${DARWIN_extra_cstar_flags}")
  # NOTE: it's critical that the optimization flag is put in front.
  # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
  if (USE_ASAN OR USE_LEAKSAN OR USE_UBSAN OR USE_THDSAN)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O1 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O1 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O1 -mavx -mprefer-vector-width=128 ${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "-O1 -mavx -mprefer-vector-width=128 ${CMAKE_C_FLAGS_RELEASE}")
  else()
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -ffast-math -mavx -mprefer-vector-width=128 ${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "-Ofast -ffast-math -mavx -mprefer-vector-width=128 ${CMAKE_C_FLAGS_RELEASE}")
    if(USE_LTO)
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fforce-emit-vtables -fwhole-program-vtables")
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fforce-emit-vtables -fwhole-program-vtables")
    endif()
  endif()
endif (DARWIN)


if (LINUX OR DARWIN)
  if (NOT UNIX_DISABLE_FATAL_WARNINGS)
    set(UNIX_WARNINGS "-Werror")
  endif (NOT UNIX_DISABLE_FATAL_WARNINGS)

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(UNIX_WARNINGS "-Wall -Wno-unused-variable -Wno-maybe-uninitialized -Wno-sign-compare -Wno-attributes ${UNIX_WARNINGS}")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS} -Wno-reorder -Wno-unused-local-typedefs")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set(UNIX_WARNINGS "-Wall ${UNIX_WARNINGS} ")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS} -Wno-unused-local-typedef -Wempty-body -Wunreachable-code -Wundefined-bool-conversion -Wenum-conversion -Wassign-enum -Wint-conversion -Wconstant-conversion -Wnewline-eof -Wno-protocol -Wno-tautological-type-limit-compare -Wno-unused-template -Wno-undef")
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    set(UNIX_WARNINGS "-w2 -diag-disable remark -wd68 -wd597 -wd780 -wd858 ${UNIX_WARNINGS} ")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS}")
  endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${UNIX_WARNINGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${UNIX_CXX_WARNINGS}")

  if (ADDRESS_SIZE EQUAL 32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  elseif (ADDRESS_SIZE EQUAL 64)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  endif (ADDRESS_SIZE EQUAL 32)
endif (LINUX OR DARWIN)

if (USESYSTEMLIBS)
  add_definitions(-DLL_USESYSTEMLIBS=1)

  if (LINUX AND ${ARCH} STREQUAL "i686")
    add_compile_options(-march=pentiumpro)
  endif (LINUX AND ${ARCH} STREQUAL "i686")

else (USESYSTEMLIBS)
  #Define this here so it propagates on all systems to all targets
  #add_definitions(-DBOOST_THREAD_VERSION=4)

  #Enforce compile-time correctness for fmt strings
  add_definitions(-DFMT_STRING_ALIAS=1)

  #Uncomment this definition when we can build cleanly against OpenSSL 1.1
  add_definitions(-DOPENSSL_API_COMPAT=0x10100000L)

  #Force glm ctor init
  add_definitions(-DGLM_FORCE_CTOR_INIT=1)

  if(USE_CRASHPAD)
    add_definitions(-DUSE_CRASHPAD=1 -DCRASHPAD_URL="${CRASHPAD_URL}")
  endif()

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

option(RELEASE_SHOW_ASSERTS "Enable asserts in release builds" OFF)

if(RELEASE_SHOW_ASSERTS)
  add_definitions(-DRELEASE_SHOW_ASSERT=1)
else()
  add_definitions(-URELEASE_SHOW_ASSERT)
endif()

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
