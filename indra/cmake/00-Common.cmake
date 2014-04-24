# -*- cmake -*-
#
# Compilation options shared by all Second Life components.

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(Variables)

# Portable compilation flags.
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -DLL_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE
    "-DLL_RELEASE=1 -DLL_RELEASE_FOR_DOWNLOAD=1 -DNDEBUG") 

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
    "-DLL_RELEASE=1 -DNDEBUG -DLL_RELEASE_WITH_DEBUG_INFO=1")

# Configure crash reporting
set(RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in release builds")
set(NON_RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in developer builds")

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

  # for "backwards compatibility", cmake sneaks in the Zm1000 option which royally
  # screws incredibuild. this hack disables it.
  # for details see: http://connect.microsoft.com/VisualStudio/feedback/details/368107/clxx-fatal-error-c1027-inconsistent-values-for-ym-between-creation-and-use-of-precompiled-headers
  # http://www.ogre3d.org/forums/viewtopic.php?f=2&t=60015
  # http://www.cmake.org/pipermail/cmake/2009-September/032143.html
  string(REPLACE "/Zm1000" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /MDd /MP -D_SCL_SECURE_NO_WARNINGS=1"
      CACHE STRING "C++ compiler debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Od /Zi /MD /MP /Ob0 -D_SECURE_STL=0"
      CACHE STRING "C++ compiler release-with-debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${LL_CXX_FLAGS} /O2 /Zi /MD /MP /Ob2 -D_SECURE_STL=0 -D_HAS_ITERATOR_DEBUGGING=0"
      CACHE STRING "C++ compiler release options" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
  if (MSVC11 OR MSVC12)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
  endif (MSVC11 OR MSVC12)

  set(CMAKE_CXX_STANDARD_LIBRARIES "")
  set(CMAKE_C_STANDARD_LIBRARIES "")

  add_definitions(
      /DLL_WINDOWS=1
      /DDOM_DYNAMIC
      /DUNICODE
      /D_UNICODE 
      /GS
      /TP
      /W3
      /c
      /Zc:forScope
      /nologo
      /Oy-
      /Zc:wchar_t-
      /fp:fast
      )
     
  if (MSVC11 OR MSVC12)
    if (RELEASE_EXTRA_DEBUG)
      add_definitions(/d2Zi+)
    endif (RELEASE_EXTRA_DEBUG)
  endif (MSVC11 OR MSVC12)

  if (WORD_SIZE EQUAL 32)
    add_definitions(
		/arch:SSE2
        )
  endif (WORD_SIZE EQUAL 32)

  # Are we using the crummy Visual Studio KDU build workaround?
  if (NOT VS_DISABLE_FATAL_WARNINGS)
    add_definitions(/WX)
  endif (NOT VS_DISABLE_FATAL_WARNINGS)

  if (MSVC11 OR MSVC12)
    # configure win32 API for windows vista+ compatibility
    set(WINVER "0x0600" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
    add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")
  else (MSVC11 OR MSVC12)
    # configure win32 API for windows XP+ compatibility
    set(WINVER "0x0501" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
    add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")
  endif (MSVC11 OR MSVC12)
endif (WINDOWS)


if (LINUX)
  set(CMAKE_SKIP_RPATH TRUE)
  add_definitions(
      -DLL_LINUX=1
      -DAPPID=secondlife
      -DLL_IGNORE_SIGCHLD
      -D_REENTRANT
      -fvisibility=hidden
      -fexceptions
      -g
      -pthread
      )


  if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    find_program(GXX g++)
    mark_as_advanced(GXX)

    if (GXX)
      execute_process(
          COMMAND ${GXX} --version
          COMMAND sed "s/^[gc+ ]*//"
          COMMAND head -1
          OUTPUT_VARIABLE GXX_VERSION
          OUTPUT_STRIP_TRAILING_WHITESPACE
          )
    else (GXX)
      set(GXX_VERSION x)
    endif (GXX)

  # The quoting hack here is necessary in case we're using distcc or
  # ccache as our compiler.  CMake doesn't pass the command line
  # through the shell by default, so we end up trying to run "distcc"
  # " g++" - notice the leading space.  Ugh.

  execute_process(
      COMMAND sh -c "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} --version"
      COMMAND sed "s/^[gc+ ]*//"
      COMMAND head -1
      OUTPUT_VARIABLE CXX_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    if (${GXX_VERSION} STREQUAL ${CXX_VERSION})
      add_definitions(-D_FORTIFY_SOURCE=2)
    endif (${GXX_VERSION} STREQUAL ${CXX_VERSION})

    # Let's actually get a numerical version of gxx's version
    STRING(REGEX REPLACE ".* ([0-9])\\.([0-9])\\.([0-9]).*" "\\1\\2\\3" CXX_VERSION_NUMBER ${CXX_VERSION})

    if(${CXX_VERSION_NUMBER} GREATER 459)
      set(CMAKE_CXX_FLAGS "-Wno-deprecated -Wno-unused-but-set-variable -Wno-unused-variable ${CMAKE_CXX_FLAGS}")
    endif (${CXX_VERSION_NUMBER} GREATER 459)

    # gcc 4.3 and above don't like the LL boost and also
    # cause warnings due to our use of deprecated headers
    if(${CXX_VERSION_NUMBER} GREATER 429)
      add_definitions(-Wno-parentheses)
      set(CMAKE_CXX_FLAGS "-Wno-deprecated ${CMAKE_CXX_FLAGS}")
    endif (${CXX_VERSION_NUMBER} GREATER 429)

    # gcc 4.8 and above added a new spammy warnings!
    if (${CXX_VERSION_NUMBER} GREATER 479)
      set(CMAKE_CXX_FLAGS "-Wno-attributes -Wno-unused-local-typedefs ${CMAKE_CXX_FLAGS}")
    endif (${CXX_VERSION_NUMBER} GREATER 479)
    # End of hacks.

    add_definitions(
        -fno-math-errno
        -fno-strict-aliasing
        -fsigned-char
        -msse2
        -mfpmath=sse
        )

    if (WORD_SIZE EQUAL 32)
      add_definitions(-march=pentium4)
    endif (WORD_SIZE EQUAL 32)

    if (NOT STANDALONE)
      # this stops us requiring a really recent glibc at runtime
      add_definitions(-fno-stack-protector)
      # linking can be very memory-hungry, especially the final viewer link
      set(CMAKE_CXX_LINK_FLAGS "-Wl,--no-keep-memory")
    endif (NOT STANDALONE)

    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fno-inline ${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -ffast-math ${CMAKE_CXX_FLAGS_RELEASE}")
  elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    add_definitions(
        -fno-math-errno
        -msse2
        )

    if (WORD_SIZE EQUAL 32)
      add_definitions(-march=pentium4)
    endif (WORD_SIZE EQUAL 32)

    if (NOT STANDALONE)
      # this stops us requiring a really recent glibc at runtime
      add_definitions(-fno-stack-protector)
      # linking can be very memory-hungry, especially the final viewer link
      set(CMAKE_CXX_LINK_FLAGS "-Wl,--no-keep-memory")
    endif (NOT STANDALONE)

    set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CMAKE_CXX_FLAGS_RELEASE}")
  elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
    add_definitions(
        -fno-math-errno
        -msse2
        )

    if (NOT STANDALONE)
      add_definitions(-march=pentium4)
    endif (NOT STANDALONE)

    if (NOT STANDALONE)
      # this stops us requiring a really recent glibc at runtime
      add_definitions(-fno-stack-protector)
      # linking can be very memory-hungry, especially the final viewer link
      set(CMAKE_CXX_LINK_FLAGS "-Wl,--no-keep-memory")
    endif (NOT STANDALONE)

    set(CMAKE_CXX_FLAGS_DEBUG "-O0 ${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")
  endif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
endif (LINUX)


if (DARWIN)
  add_definitions(-DLL_DARWIN=1)
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-no_compact_unwind -Wl,-headerpad_max_install_names,-search_paths_first")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
  set(DARWIN_extra_cstar_flags "-g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DARWIN_extra_cstar_flags}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${DARWIN_extra_cstar_flags}")
  # NOTE: it's critical that the optimization flag is put in front.
  # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_C_FLAGS_RELEASE "-O2 ${CMAKE_C_FLAGS_RELEASE}")
  if (XCODE_VERSION GREATER 4.2)
    set(ENABLE_SIGNING TRUE)
    set(SIGNING_IDENTITY "Developer ID Application: Linden Research, Inc.")
  endif (XCODE_VERSION GREATER 4.2)
endif (DARWIN)


if (LINUX OR DARWIN)
  if (NOT UNIX_DISABLE_FATAL_WARNINGS)
    set(UNIX_WARNINGS "-Werror")
  endif (NOT UNIX_DISABLE_FATAL_WARNINGS)

  if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set(UNIX_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs ${UNIX_WARNINGS} ")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS} -Wno-reorder -Wno-non-virtual-dtor")
  elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    set(UNIX_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs ${UNIX_WARNINGS} ")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS} -Wno-reorder -Wno-unused-const-variable -Wno-format-extra-args -Wno-unused-private-field -Wno-unused-function -Wno-tautological-compare -Wno-empty-body -Wno-unused-variable -Wno-unused-value")
  elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
    set(UNIX_WARNINGS "-w2 -diag-disable remark -wd68 -wd597 -wd780 -wd858 ${UNIX_WARNINGS} ")
    set(UNIX_CXX_WARNINGS "${UNIX_WARNINGS}")
  endif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")

  set(CMAKE_C_FLAGS "${UNIX_WARNINGS} ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${UNIX_CXX_WARNINGS} ${CMAKE_CXX_FLAGS}")

  if (WORD_SIZE EQUAL 32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  elseif (WORD_SIZE EQUAL 64)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  endif (WORD_SIZE EQUAL 32)
endif (LINUX OR DARWIN)


if (STANDALONE)
  add_definitions(-DLL_STANDALONE=1)

  if (LINUX AND ${ARCH} STREQUAL "i686")
    add_definitions(-march=pentiumpro)
  endif (LINUX AND ${ARCH} STREQUAL "i686")

else (STANDALONE)
  set(${ARCH}_linux_INCLUDES
      ELFIO
      atk-1.0
      glib-2.0
      gstreamer-0.10
      gtk-2.0
      pango-1.0
      )
endif (STANDALONE)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
