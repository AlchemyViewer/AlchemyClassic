# -*- cmake -*-

include(Variables)

if (WINDOWS)
  if (WORD_SIZE EQUAL 64)
    set (DIRECTX_ARCHITECTURE x64)
  else (WORD_SIZE EQUAL 32)
    set (DIRECTX_ARCHITECTURE x86)
  endif (WORD_SIZE EQUAL 64)

  set(programfilesx86 "ProgramFiles(x86)")
  find_path (WIN_KIT_ROOT_DIR Include/um/windows.h
             PATHS
             "$ENV{ProgramFiles}/Windows Kits/8.1"
             "$ENV{programfilesx86}/Windows Kits/8.1"
             )

  find_path (WIN_KIT_LIB_DIR dxguid.lib
             "${WIN_KIT_ROOT_DIR}/Lib/winv6.3/um/${DIRECTX_ARCHITECTURE}"
             "${WIN_KIT_ROOT_DIR}/Lib/Win8/um/${DIRECTX_ARCHITECTURE}"
             )

  if (WIN_KIT_ROOT_DIR)
    set (DIRECTX_INCLUDE_DIR "${WIN_KIT_ROOT_DIR}/Include/um" "${WIN_KIT_ROOT_DIR}/Include/shared")
    set (DIRECTX_LIBRARY_DIR "${WIN_KIT_LIB_DIR}")
  endif (WIN_KIT_ROOT_DIR)

  if (DIRECTX_INCLUDE_DIR)
    include_directories(${DIRECTX_INCLUDE_DIR})
    if (DIRECTX_FIND_QUIETLY)
      message(STATUS "Found DirectX include: ${DIRECTX_INCLUDE_DIR}")
    endif (DIRECTX_FIND_QUIETLY)
  else (DIRECTX_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find DirectX SDK Include")
  endif (DIRECTX_INCLUDE_DIR)

  if (DIRECTX_LIBRARY_DIR)
    if (DIRECTX_FIND_QUIETLY)
      message(STATUS "Found DirectX include: ${DIRECTX_LIBRARY_DIR}")
    endif (DIRECTX_FIND_QUIETLY)
  else (DIRECTX_LIBRARY_DIR)
    message(FATAL_ERROR "Could not find DirectX SDK Libraries")
  endif (DIRECTX_LIBRARY_DIR)

endif (WINDOWS)
