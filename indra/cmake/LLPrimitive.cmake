# -*- cmake -*-

# these should be moved to their own cmake file
include(Prebuilt)
include(Boost)
include(LibXML2)
include(ICU4C)

use_prebuilt_binary(colladadom)
use_prebuilt_binary(pcre)

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
        debug pcrecppd
        optimized pcrecpp
        debug pcred
        optimized pcre
        ${BOOST_SYSTEM_LIBRARIES}
        )
elseif (DARWIN)
    set(LLPRIMITIVE_LIBRARIES
        llprimitive
        collada14dom
        minizip
        pcrecpp
        pcre
        )
elseif (LINUX)
    set(LLPRIMITIVE_LIBRARIES 
        llprimitive
        debug collada14dom-d
        optimized collada14dom
        minizip
        pcrecpp
        pcre
        )
endif (WINDOWS)
LIST(APPEND LLPRIMITIVE_LIBRARIES
     ${LIBXML2_LIBRARIES}
     ${ICU4C_LIBRARIES}
     )
