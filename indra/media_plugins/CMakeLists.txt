# -*- cmake -*-

add_subdirectory(base)

if (WINDOWS OR LINUX OR DARWIN AND WORD_SIZE EQUAL 32)
    add_subdirectory(webkit)
endif (WINDOWS OR LINUX OR DARWIN AND WORD_SIZE EQUAL 32)

if (LINUX)
    add_subdirectory(gstreamer010)
endif (LINUX)

if (WINDOWS OR DARWIN AND NOT WORD_SIZE EQUAL 64)
    add_subdirectory(quicktime)
endif (WINDOWS OR DARWIN AND NOT WORD_SIZE EQUAL 64)

if (WINDOWS AND MSVC10)
    add_subdirectory(winmmshim)
endif (WINDOWS AND MSVC10)

add_subdirectory(example)
