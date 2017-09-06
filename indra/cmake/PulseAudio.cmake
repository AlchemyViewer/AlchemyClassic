# -*- cmake -*-
include(Prebuilt)

set(PULSEAUDIO OFF CACHE BOOL "Build with PulseAudio support, if available.")

if (PULSEAUDIO)
    include(FindPkgConfig)

    pkg_check_modules(PULSEAUDIO libpulse)
endif (PULSEAUDIO)

if (PULSEAUDIO_FOUND)
  add_definitions(-DLL_PULSEAUDIO_ENABLED=1)
endif (PULSEAUDIO_FOUND)
