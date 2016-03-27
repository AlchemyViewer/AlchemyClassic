# -*- cmake -*-

add_subdirectory(base)

if (LINUX)
    add_subdirectory(gstreamer010)
endif (LINUX)

if(CEFPLUGIN)
    add_subdirectory(cef)
endif(CEFPLUGIN)

if (WINDOWS OR DARWIN)
    if(NOT WORD_SIZE EQUAL 64)
        add_subdirectory(quicktime)
    endif(NOT WORD_SIZE EQUAL 64)
endif (WINDOWS OR DARWIN)

add_subdirectory(example)
