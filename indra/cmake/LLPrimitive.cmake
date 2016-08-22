# -*- cmake -*-

# these should be moved to their own cmake file
include(Prebuilt)
include(Boost)
include(LibXML2)

use_prebuilt_binary(colladadom)

set(LLPRIMITIVE_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/llprimitive
    ${LIBXML2_INCLUDES}
    )
if (WINDOWS)
    set(LLPRIMITIVE_LIBRARIES
        debug llprimitive
        optimized llprimitive
        debug libcollada14dom23-sd
        optimized libcollada14dom23-s
        ${BOOST_SYSTEM_LIBRARIES}
        )
elseif (DARWIN)
    set(LLPRIMITIVE_LIBRARIES
        llprimitive
        collada14dom
        minizip
        )
elseif (LINUX)
    set(LLPRIMITIVE_LIBRARIES 
        llprimitive
        debug collada14dom-d
        optimized collada14dom
        minizip
        )
endif (WINDOWS)
LIST(APPEND LLPRIMITIVE_LIBRARIES
     ${LIBXML2_LIBRARIES}
     )
