# -*- cmake -*-

# The copy_win_libs folder contains file lists and a script used to
# copy dlls, exes and such needed to run the SecondLife from within
# VisualStudio.

include(CMakeCopyIfDifferent)
include(Linking)
include(Variables)

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
        vivoxplatform.dll
        vivoxsdk.dll
        ortp.dll
        zlib1.dll
        vivoxoal.dll
        )

    #*******************************
    # Misc shared libs 

    set(debug_src_dir "${ARCH_PREBUILT_DIRS_DEBUG}")
    set(debug_files
        openjpegd.dll
        libapr-1.dll
        libaprutil-1.dll
        libapriconv-1.dll
        ssleay32.dll
        libeay32.dll
        glod.dll    
        libhunspell.dll
        icuin54d.dll
        icuuc54d.dll
        icudt54.dll
        )

    set(release_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(release_files
        openjpeg.dll
        libapr-1.dll
        libaprutil-1.dll
        libapriconv-1.dll
        ssleay32.dll
        libeay32.dll
        glod.dll
        libhunspell.dll
        icuin54.dll
        icuuc54.dll
        icudt54.dll
        )

    if(USE_TCMALLOC)
      set(debug_files ${debug_files} libtcmalloc_minimal-debug.dll)
      set(release_files ${release_files} libtcmalloc_minimal.dll)
    endif(USE_TCMALLOC)

    if(USE_TBBMALLOC)
      set(debug_files ${debug_files} tbbmalloc_debug.dll tbbmalloc_proxy_debug.dll)
      set(release_files ${release_files} tbbmalloc.dll tbbmalloc_proxy.dll)
    endif(USE_TBBMALLOC)

    if(OPENAL)
      set(debug_files ${debug_files} alut.dll OpenAL32.dll)
      set(release_files ${release_files} alut.dll OpenAL32.dll)
    endif(OPENAL)

    if (FMODSTUDIO)
      if(WORD_SIZE STREQUAL 64)
        set(debug_files ${debug_files} fmodL64.dll)
        set(release_files ${release_files} fmod64.dll)
      else(WORD_SIZE STREQUAL 64)
        set(debug_files ${debug_files} fmodL.dll)
        set(release_files ${release_files} fmod.dll)
      endif(WORD_SIZE STREQUAL 64)
    endif (FMODSTUDIO)

    if (FMODEX)
      if(WORD_SIZE STREQUAL 64)
        set(debug_files ${debug_files} fmodexL64.dll)
        set(release_files ${release_files} fmodex64.dll)
      else(WORD_SIZE STREQUAL 64)
        set(debug_files ${debug_files} fmodexL.dll)
        set(release_files ${release_files} fmodex.dll)
      endif(WORD_SIZE STREQUAL 64)
    endif (FMODEX)

#*******************************
# Copy MS C runtime dlls, required for packaging.
# *TODO - Adapt this to support VC9
if (MSVC_VERSION EQUAL 1600) # VisualStudio 2010
    FIND_PATH(debug_msvc10_redist_path msvcr100d.dll
        PATHS
        ${MSVC_DEBUG_REDIST_PATH}
         [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\10.0\\Setup\\VC;ProductDir]/redist/Debug_NonRedist/x86/Microsoft.VC100.DebugCRT
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/SysWOW64
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/System32
        NO_DEFAULT_PATH
        )

    if(EXISTS ${debug_msvc10_redist_path})
        set(debug_msvc10_files
            msvcr100d.dll
            msvcp100d.dll
            )

        copy_if_different(
            ${debug_msvc10_redist_path}
            "${SHARED_LIB_STAGING_DIR_DEBUG}"
            out_targets
            ${debug_msvc10_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})

    endif (EXISTS ${debug_msvc10_redist_path})

    FIND_PATH(release_msvc10_redist_path msvcr100.dll
        PATHS
        ${MSVC_REDIST_PATH}
         [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\10.0\\Setup\\VC;ProductDir]/redist/x86/Microsoft.VC100.CRT
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/SysWOW64
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/System32
        NO_DEFAULT_PATH
        )

    if(EXISTS ${release_msvc10_redist_path})
        set(release_msvc10_files
            msvcr100.dll
            msvcp100.dll
            )

        copy_if_different(
            ${release_msvc10_redist_path}
            "${SHARED_LIB_STAGING_DIR_RELEASE}"
            out_targets
            ${release_msvc10_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})

        copy_if_different(
            ${release_msvc10_redist_path}
            "${SHARED_LIB_STAGING_DIR_RELWITHDEBINFO}"
            out_targets
            ${release_msvc10_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})
          
    endif (EXISTS ${release_msvc10_redist_path})
elseif (MSVC12) # VisualStudio 2013
    if (WORD_SIZE STREQUAL 32)
      set (CRT_ARCHITECTURE x86)
    elseif (WORD_SIZE STREQUAL 64)
      set (CRT_ARCHITECTURE x64)
    endif (WORD_SIZE STREQUAL 32)
    FIND_PATH(debug_msvc12_redist_path msvcr120d.dll
        PATHS
        ${MSVC_DEBUG_REDIST_PATH}
        [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7;12.0]/VC/redist/Debug_NonRedist/${CRT_ARCHITECTURE}/Microsoft.VC120.DebugCRT
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/SysWOW64
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/System32
        NO_DEFAULT_PATH
        )

    if(EXISTS ${debug_msvc12_redist_path})
        set(debug_msvc12_files
            msvcr120d.dll
            msvcp120d.dll
            )

        copy_if_different(
            ${debug_msvc12_redist_path}
            "${SHARED_LIB_STAGING_DIR_DEBUG}"
            out_targets
            ${debug_msvc12_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})

    endif ()

    FIND_PATH(release_msvc12_redist_path msvcr120.dll
        PATHS
        ${MSVC_REDIST_PATH}
         [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7;12.0]/VC/redist/${CRT_ARCHITECTURE}/Microsoft.VC120.CRT
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/SysWOW64
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Windows;Directory]/System32
        NO_DEFAULT_PATH
        )

    if(EXISTS ${release_msvc12_redist_path})
        set(release_msvc12_files
            msvcr120.dll
            msvcp120.dll
            )

        copy_if_different(
            ${release_msvc12_redist_path}
            "${SHARED_LIB_STAGING_DIR_RELEASE}"
            out_targets
            ${release_msvc12_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})

        copy_if_different(
            ${release_msvc12_redist_path}
            "${SHARED_LIB_STAGING_DIR_RELWITHDEBINFO}"
            out_targets
            ${release_msvc12_files}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})
          
    endif (EXISTS ${release_msvc12_redist_path})
endif (MSVC_VERSION EQUAL 1600)

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
        libjpeg.8.0.2.dylib
        libndofdev.dylib
       )

    if (OPENAL)
      set(release_files ${release_files} libopenal.dylib libalut.dylib)
    endif (OPENAL)

    if (FMODEX)
      set(debug_files ${debug_files} libfmodexL.dylib)
      set(release_files ${release_files} libfmodex.dylib)
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
        libdb-5.1.so
        libexpat.so
        libexpat.so.1
        libfreetype.so.6.6.2
        libfreetype.so.6
        libGLOD.so
        libgmodule-2.0.so
        libgobject-2.0.so
        libhunspell-1.3.so.0.0.0
        libopenal.so
        libopenjpeg.so
        libuuid.so.16
        libuuid.so.16.0.22
        libfontconfig.so.1.8.0
        libfontconfig.so.1
       )

    if (USE_TCMALLOC)
      set(release_files ${release_files} "libtcmalloc_minimal.so")
    endif (USE_TCMALLOC)

    if (FMODEX)
      set(debug_files ${debug_files} "libfmodexL.so")
      set(release_files ${release_files} "libfmodex.so")
    endif (FMODEX)

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
