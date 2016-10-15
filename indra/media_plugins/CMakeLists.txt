# -*- cmake -*-

add_subdirectory(base)

if (LINUX)
    add_subdirectory(gstreamer010)
endif (LINUX)

if (WINDOWS OR LINUX)
    add_subdirectory(libvlc)
endif (WINDOWS OR LINUX)

add_subdirectory(cef)

### add_subdirectory(example)
