# -*- cmake -*-

add_subdirectory(base)

if (NOT WORD_SIZE EQUAL 64)
  add_subdirectory(webkit)
endif (NOT WORD_SIZE EQUAL 64)

add_subdirectory(gstreamer010)

if (WINDOWS OR DARWIN AND NOT WORD_SIZE EQUAL 64)
    add_subdirectory(quicktime)
endif (WINDOWS OR DARWIN AND NOT WORD_SIZE EQUAL 64)

if (WINDOWS)
    add_subdirectory(winmmshim)
endif (WINDOWS)

add_subdirectory(example)
