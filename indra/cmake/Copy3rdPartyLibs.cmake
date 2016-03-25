# -*- cmake -*-

# The copy_win_libs folder contains file lists and a script used to
# copy dlls, exes and such needed to run the SecondLife from within
# VisualStudio.

include(CMakeCopyIfDifferent)
include(Linking)
include(Variables)
include(LLCommon)

###################################################################
# set up platform specific lists of files that need to be copied
###################################################################
if(WINDOWS)
    set(SHARED_LIB_STAGING_DIR_DEBUG            "${SHARED_LIB_STAGING_DIR}/Debug")
    set(SHARED_LIB_STAGING_DIR_RELWITHDEBINFO   "${SHARED_LIB_STAGING_DIR}/RelWithDebInfo")
    set(SHARED_LIB_STAGING_DIR_RELEASE          "${SHARED_LIB_STAGING_DIR}/Release")

    #*******************************
    # VIVOX - *NOTE: no debug version
    set(vivox_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(vivox_files
        SLVoice.exe
        ca-bundle.crt
        libsndfile-1.dll
        vivoxsdk.dll
        ortp.dll
        vivoxoal.dll
        )

    #*******************************
    # Misc shared libs 

    set(debug_src_dir "${ARCH_PREBUILT_DIRS_DEBUG}")
    set(debug_files
        openjpegd.dll
        ssleay32.dll
        libeay32.dll
        glod.dll    
        libhunspell.dll
        )

    set(release_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(release_files
        openjpeg.dll
        ssleay32.dll
        libeay32.dll
        glod.dll
        libhunspell.dll
        )

    if (LLCOMMON_LINK_SHARED)
      list(APPEND debug_files 
        libapr-1.dll
        libaprutil-1.dll
        libapriconv-1.dll
        )
      list(APPEND release_files 
        libapr-1.dll
        libaprutil-1.dll
        libapriconv-1.dll
        )
    endif (LLCOMMON_LINK_SHARED)

    if(USE_TCMALLOC)
      list(APPEND debug_files libtcmalloc_minimal-debug.dll)
      list(APPEND release_files libtcmalloc_minimal.dll)
    endif(USE_TCMALLOC)

    if(USE_TBBMALLOC)
      list(APPEND debug_files tbbmalloc_debug.dll tbbmalloc_proxy_debug.dll)
      list(APPEND release_files tbbmalloc.dll tbbmalloc_proxy.dll)
    endif(USE_TBBMALLOC)

    if(OPENAL)
      list(APPEND debug_files alut.dll OpenAL32.dll)
      list(APPEND release_files alut.dll OpenAL32.dll)
    endif(OPENAL)

    if (FMODSTUDIO)
      if(WORD_SIZE STREQUAL 64)
        list(APPEND debug_files fmodL64.dll)
        list(APPEND release_files fmod64.dll)
      else(WORD_SIZE STREQUAL 64)
        list(APPEND debug_files fmodL.dll)
        list(APPEND release_files fmod.dll)
      endif(WORD_SIZE STREQUAL 64)
    endif (FMODSTUDIO)

    #*******************************
    # Copy MS C runtime dlls, required for packaging.
    # *TODO - Adapt this to support VC9
    if (MSVC12) # VisualStudio 2013, which is (sigh) VS 12
        set(MSVC_VER 120)
        set(MSVC_VERDOT 12.0)
    else (MSVC14)
        set(MSVC_VER 140)
        set(MSVC_VERDOT 14.0)
    else (MSVC12)
        MESSAGE(WARNING "New MSVC_VERSION ${MSVC_VERSION} of MSVC: adapt Copy3rdPartyLibs.cmake")
    endif (MSVC12)
	if (WORD_SIZE EQUAL 32)
      set (CRT_ARCHITECTURE x86)
    elseif (WORD_SIZE EQUAL 64)
      set (CRT_ARCHITECTURE x64)
    endif (WORD_SIZE EQUAL 32)

    FIND_PATH(debug_msvc_redist_path msvcr${MSVC_VER}d.dll
        PATHS
        ${MSVC_DEBUG_REDIST_PATH}
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7;${MSVC_VERDOT}]/VC/redist/Debug_NonRedist/${CRT_ARCHITECTURE}/Microsoft.VC${MSVC_VER}.DebugCRT
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/SysWOW64
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/System32
        NO_DEFAULT_PATH
        )

	mark_as_advanced(debug_msvc_redist_path)
    if(EXISTS ${debug_msvc_redist_path})
        set(debug_msvc_files
            msvcr${MSVC_VER}d.dll
            msvcp${MSVC_VER}d.dll
            )

        copy_if_different(
            ${debug_msvc_redist_path}
            "${SHARED_LIB_STAGING_DIR_DEBUG}"
            out_targets
            ${debug_msvc_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})

    endif ()

    FIND_PATH(release_msvc_redist_path msvcr${MSVC_VER}.dll
        PATHS
        ${MSVC_REDIST_PATH}
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7;${MSVC_VERDOT}]/VC/redist/${CRT_ARCHITECTURE}/Microsoft.VC${MSVC_VER}.CRT
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/SysWOW64
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/System32
        NO_DEFAULT_PATH
        )

    mark_as_advanced(release_msvc_redist_path)
    if(EXISTS ${release_msvc_redist_path})
        set(release_msvc_files
            msvcr${MSVC_VER}.dll
            msvcp${MSVC_VER}.dll
            )

        copy_if_different(
            ${release_msvc_redist_path}
            "${SHARED_LIB_STAGING_DIR_RELEASE}"
            out_targets
            ${release_msvc_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})

        copy_if_different(
            ${release_msvc_redist_path}
            "${SHARED_LIB_STAGING_DIR_RELWITHDEBINFO}"
            out_targets
            ${release_msvc_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})
          
    endif ()

elseif(DARWIN)
    set(SHARED_LIB_STAGING_DIR_DEBUG            "${SHARED_LIB_STAGING_DIR}/Debug/Resources")
    set(SHARED_LIB_STAGING_DIR_RELWITHDEBINFO   "${SHARED_LIB_STAGING_DIR}/RelWithDebInfo/Resources")
    set(SHARED_LIB_STAGING_DIR_RELEASE          "${SHARED_LIB_STAGING_DIR}/Release/Resources")

    set(vivox_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(vivox_files
        SLVoice
        ca-bundle.crt
        libsndfile.dylib
        libvivoxoal.dylib
        libortp.dylib
        libvivoxplatform.dylib
        libvivoxsdk.dylib
       )
    set(debug_src_dir "${ARCH_PREBUILT_DIRS_DEBUG}")
    set(debug_files
       )
    set(release_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(release_files
        libapr-1.0.dylib
        libapr-1.dylib
        libaprutil-1.0.dylib
        libaprutil-1.dylib
        libexception_handler.dylib
        libfreetype.6.dylib
        libGLOD.dylib
        libndofdev.dylib
        libopenjpeg.5.dylib
       )

    if (OPENAL)
      list(APPEND release_files libopenal.dylib libalut.dylib)
    endif (OPENAL)

    if (FMODEX)
      list(APPEND debug_files libfmodexL.dylib)
      list(APPEND release_files libfmodex.dylib)
    endif (FMODEX)

elseif(LINUX)
    # linux is weird, multiple side by side configurations aren't supported
    # and we don't seem to have any debug shared libs built yet anyways...
    set(SHARED_LIB_STAGING_DIR_DEBUG            "${SHARED_LIB_STAGING_DIR}")
    set(SHARED_LIB_STAGING_DIR_RELWITHDEBINFO   "${SHARED_LIB_STAGING_DIR}")
    set(SHARED_LIB_STAGING_DIR_RELEASE          "${SHARED_LIB_STAGING_DIR}")

    set(vivox_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(vivox_files
        libsndfile.so.1
        libortp.so
        libvivoxoal.so.1
        libvivoxplatform.so
        libvivoxsdk.so
        SLVoice
        # ca-bundle.crt   #No cert for linux.  It is actually still 3.2SDK.
       )
    # *TODO - update this to use LIBS_PREBUILT_DIR and LL_ARCH_DIR variables
    # or ARCH_PREBUILT_DIRS
    set(debug_src_dir "${ARCH_PREBUILT_DIRS_DEBUG}")
    set(debug_files
       )
    # *TODO - update this to use LIBS_PREBUILT_DIR and LL_ARCH_DIR variables
    # or ARCH_PREBUILT_DIRS
    set(release_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    # *FIX - figure out what to do with duplicate libalut.so here -brad
    set(release_files
        libapr-1.so.0
        libaprutil-1.so.0
        libatk-1.0.so
        libexpat.so
        libexpat.so.1
        libfreetype.so.6.12.3
        libfreetype.so.6
        libfreetype.so
        libGLOD.so
        libgmodule-2.0.so
        libgobject-2.0.so
        libopenal.so
        libopenjpeg.so
        libfontconfig.so.1.8.0
        libfontconfig.so.1
        libfontconfig.so
       )

    if (USE_TCMALLOC)
      list(APPEND release_files "libtcmalloc_minimal.so")
    endif (USE_TCMALLOC)

    if (FMODSTUDIO)
      list(APPEND debug_files "libfmodL.so")
      list(APPEND release_files "libfmod.so")
    endif (FMODSTUDIO)

else(WINDOWS)
    message(STATUS "WARNING: unrecognized platform for staging 3rd party libs, skipping...")
    set(vivox_src_dir "${CMAKE_SOURCE_DIR}/newview/vivox-runtime/i686-linux")
    set(vivox_files "")
    # *TODO - update this to use LIBS_PREBUILT_DIR and LL_ARCH_DIR variables
    # or ARCH_PREBUILT_DIRS
    set(debug_src_dir "${CMAKE_SOURCE_DIR}/../libraries/i686-linux/lib/debug")
    set(debug_files "")
    # *TODO - update this to use LIBS_PREBUILT_DIR and LL_ARCH_DIR variables
    # or ARCH_PREBUILT_DIRS
    set(release_src_dir "${CMAKE_SOURCE_DIR}/../libraries/i686-linux/lib/release")
    set(release_files "")

    set(debug_llkdu_src "")
    set(debug_llkdu_dst "")
    set(release_llkdu_src "")
    set(release_llkdu_dst "")
    set(relwithdebinfo_llkdu_dst "")
endif(WINDOWS)


################################################################
# Done building the file lists, now set up the copy commands.
################################################################

copy_if_different(
    ${vivox_src_dir}
    "${SHARED_LIB_STAGING_DIR_DEBUG}"
    out_targets 
    ${vivox_files}
    )
set(third_party_targets ${third_party_targets} ${out_targets})

copy_if_different(
    ${vivox_src_dir}
    "${SHARED_LIB_STAGING_DIR_RELEASE}"
    out_targets
    ${vivox_files}
    )
set(third_party_targets ${third_party_targets} ${out_targets})

copy_if_different(
    ${vivox_src_dir}
    "${SHARED_LIB_STAGING_DIR_RELWITHDEBINFO}"
    out_targets
    ${vivox_files}
    )
set(third_party_targets ${third_party_targets} ${out_targets})



copy_if_different(
    ${debug_src_dir}
    "${SHARED_LIB_STAGING_DIR_DEBUG}"
    out_targets
    ${debug_files}
    )
set(third_party_targets ${third_party_targets} ${out_targets})

copy_if_different(
    ${release_src_dir}
    "${SHARED_LIB_STAGING_DIR_RELEASE}"
    out_targets
    ${release_files}
    )
set(third_party_targets ${third_party_targets} ${out_targets})

copy_if_different(
    ${release_src_dir}
    "${SHARED_LIB_STAGING_DIR_RELWITHDEBINFO}"
    out_targets
    ${release_files}
    )
set(third_party_targets ${third_party_targets} ${out_targets})

if(NOT USESYSTEMLIBS)
  add_custom_target(
      stage_third_party_libs ALL
      DEPENDS ${third_party_targets}
      )
endif(NOT USESYSTEMLIBS)
