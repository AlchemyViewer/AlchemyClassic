# -*- cmake -*-

include(Variables)
include(GLEW)
include(Prebuilt)

if (USESYSTEMLIBS)
  include(FindSDL)

  # This should be done by FindSDL.  Sigh.
  mark_as_advanced(
      SDLMAIN_LIBRARY
      SDL_INCLUDE_DIR
      SDL_LIBRARY
      )
else (USESYSTEMLIBS)
  if (LINUX)
    use_prebuilt_binary(SDL)
    set (SDL_FOUND TRUE)
    set (SDL_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/i686-linux)
    set (SDL_LIBRARY SDL)
  endif (LINUX)
endif (USESYSTEMLIBS)

if (SDL_FOUND)
  include_directories(${SDL_INCLUDE_DIR})
endif (SDL_FOUND)

if (LLWINDOW_SDL2)
    use_prebuilt_binary(SDL2)
    set (SDL2_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/SDL2)
    set (SDL2_LIBRARY SDL2)
  include_directories(${SDL2_INCLUDE_DIR})
endif (LLWINDOW_SDL2)

set(LLWINDOW_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/llwindow
    )

if (BUILD_HEADLESS)
  set(LLWINDOW_HEADLESS_LIBRARIES
      llwindowheadless
      )
endif (BUILD_HEADLESS)

  set(LLWINDOW_LIBRARIES
      llwindow
      )
