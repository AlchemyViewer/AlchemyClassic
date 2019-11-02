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
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibrariesAL)

    set(SHARED_LIB_STAGING_DIR_DEBUG            "${SHARED_LIB_STAGING_DIR}/Debug")
    set(SHARED_LIB_STAGING_DIR_RELWITHDEBINFO   "${SHARED_LIB_STAGING_DIR}/RelWithDebInfo")
    set(SHARED_LIB_STAGING_DIR_RELEASE          "${SHARED_LIB_STAGING_DIR}/Release")

    #*******************************
    # VIVOX - *NOTE: no debug version
    set(vivox_lib_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(slvoice_src_dir "${ARCH_PREBUILT_BIN_RELEASE}")    
    set(slvoice_files SLVoice.exe )
    if (ADDRESS_SIZE EQUAL 64)
        list(APPEND vivox_libs
            vivoxsdk_x64.dll
            ortp_x64.dll
            )
    else (ADDRESS_SIZE EQUAL 64)
        list(APPEND vivox_libs
            vivoxsdk.dll
            ortp.dll
            )
    endif (ADDRESS_SIZE EQUAL 64)

    #*******************************
    # Misc shared libs 

    set(debug_src_dir "${ARCH_PREBUILT_DIRS_DEBUG}")
    set(debug_files
        openjpeg.dll
        )

    set(release_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(release_files
        openjpeg.dll
        openjpeg.pdb
        nghttp2.dll
        glod.dll
        )

    if(ADDRESS_SIZE EQUAL 64)
      list(APPEND debug_files
           libcrypto-1_1-x64.dll
           libcrypto-1_1-x64.pdb
           libssl-1_1-x64.dll
           libssl-1_1-x64.pdb
           )
      list(APPEND release_files
           libcrypto-1_1-x64.dll
           libcrypto-1_1-x64.pdb
           libssl-1_1-x64.dll
           libssl-1_1-x64.pdb
           )
    else(ADDRESS_SIZE EQUAL 64)
      list(APPEND debug_files
           libcrypto-1_1.dll
           libcrypto-1_1.pdb
           libssl-1_1.dll
           libssl-1_1.pdb
           )
      list(APPEND release_files
           libcrypto-1_1.dll
           libcrypto-1_1.pdb
           libssl-1_1.dll
           libssl-1_1.pdb
           )
    endif(ADDRESS_SIZE EQUAL 64)

    if(USE_TCMALLOC)
      list(APPEND debug_files libtcmalloc_minimal-debug.dll)
      list(APPEND release_files libtcmalloc_minimal.dll)
    endif(USE_TCMALLOC)

    if(USE_OPENAL)
      list(APPEND debug_files alut.dll OpenAL32.dll)
      list(APPEND release_files alut.dll OpenAL32.dll)
    endif(USE_OPENAL)

    if (USE_FMODSTUDIO)
      list(APPEND debug_files fmodL.dll)
      list(APPEND release_files fmod.dll)
    endif (USE_FMODSTUDIO)

    foreach(redistfullfile IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        get_filename_component(redistfilepath ${redistfullfile} DIRECTORY )
        get_filename_component(redistfilename ${redistfullfile} NAME)
        copy_if_different(
            ${redistfilepath}
            "${SHARED_LIB_STAGING_DIR_RELEASE}"
            out_targets
            ${redistfilename}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})

        copy_if_different(
            ${redistfilepath}
            "${SHARED_LIB_STAGING_DIR_RELWITHDEBINFO}"
            out_targets
            ${redistfilename}
            )
        set(third_party_targets ${third_party_targets} ${out_targets})
    endforeach()

elseif(DARWIN)
    set(SHARED_LIB_STAGING_DIR_DEBUG            "${SHARED_LIB_STAGING_DIR}/Debug/Resources")
    set(SHARED_LIB_STAGING_DIR_RELWITHDEBINFO   "${SHARED_LIB_STAGING_DIR}/RelWithDebInfo/Resources")
    set(SHARED_LIB_STAGING_DIR_RELEASE          "${SHARED_LIB_STAGING_DIR}/Release/Resources")

    set(vivox_lib_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(slvoice_files SLVoice)
    set(vivox_libs
        libortp.dylib
        libvivoxsdk.dylib
       )
    set(debug_src_dir "${ARCH_PREBUILT_DIRS_DEBUG}")
    set(debug_files
       )
    set(release_src_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(release_files
        libGLOD.dylib
        libopenjpeg.dylib
       )

    if (USE_OPENAL)
      list(APPEND release_files libopenal.dylib libalut.dylib)
    endif (USE_OPENAL)

    if (USE_FMODSTUDIO)
      list(APPEND debug_files libfmodL.dylib)
      list(APPEND release_files libfmod.dylib)
    endif (USE_FMODSTUDIO)

elseif(LINUX)
    # linux is weird, multiple side by side configurations aren't supported
    # and we don't seem to have any debug shared libs built yet anyways...
    set(SHARED_LIB_STAGING_DIR_DEBUG            "${SHARED_LIB_STAGING_DIR}")
    set(SHARED_LIB_STAGING_DIR_RELWITHDEBINFO   "${SHARED_LIB_STAGING_DIR}")
    set(SHARED_LIB_STAGING_DIR_RELEASE          "${SHARED_LIB_STAGING_DIR}")

    set(vivox_lib_dir "${ARCH_PREBUILT_DIRS_RELEASE}")
    set(vivox_libs
        libsndfile.so.1
        libortp.so
        libvivoxoal.so.1
        libvivoxsdk.so
        )
    set(slvoice_files SLVoice)

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
        libfreetype.so.6.14.0
        libfreetype.so.6
        libfreetype.so
        libGLOD.so
        libgmodule-2.0.so
        libgobject-2.0.so
        libopenal.so
        libopenjpeg.so
        libfontconfig.so.1.9.2
        libfontconfig.so.1
        libfontconfig.so
       )

    if (USE_TCMALLOC)
      list(APPEND release_files "libtcmalloc_minimal.so")
    endif (USE_TCMALLOC)

    if (USE_FMODSTUDIO)
      list(APPEND debug_files "libfmodL.so")
      list(APPEND release_files "libfmod.so")
    endif (USE_FMODSTUDIO)

else(WINDOWS)
    message(STATUS "WARNING: unrecognized platform for staging 3rd party libs, skipping...")
    set(vivox_lib_dir "${CMAKE_SOURCE_DIR}/newview/vivox-runtime/i686-linux")
    set(vivox_libs "")
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
    ${vivox_lib_dir}
    "${SHARED_LIB_STAGING_DIR_DEBUG}"
    out_targets 
    ${vivox_libs}
    )
set(third_party_targets ${third_party_targets} ${out_targets})

copy_if_different(
    ${slvoice_src_dir}
    "${SHARED_LIB_STAGING_DIR_RELEASE}"
    out_targets
    ${slvoice_files}
    )
copy_if_different(
    ${vivox_lib_dir}
    "${SHARED_LIB_STAGING_DIR_RELEASE}"
    out_targets
    ${vivox_libs}
    )

set(third_party_targets ${third_party_targets} ${out_targets})

copy_if_different(
    ${vivox_lib_dir}
    "${SHARED_LIB_STAGING_DIR_RELWITHDEBINFO}"
    out_targets
    ${vivox_libs}
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
