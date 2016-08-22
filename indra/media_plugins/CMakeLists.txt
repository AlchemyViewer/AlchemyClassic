# -*- cmake -*-

add_subdirectory(base)

if (LINUX)
    add_subdirectory(gstreamer010)
endif (LINUX)

add_subdirectory(cef)

if (WINDOWS OR DARWIN)
    if(NOT WORD_SIZE EQUAL 64)
        add_subdirectory(quicktime)
    endif(NOT WORD_SIZE EQUAL 64)
endif (WINDOWS OR DARWIN)

#add_subdirectory(example)
