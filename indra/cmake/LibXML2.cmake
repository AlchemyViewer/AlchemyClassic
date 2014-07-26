# -*- cmake -*-
include(Prebuilt)

if (USESYSTEMLIBS)
  include(FindPkgConfig)
  pkg_check_modules(LIBXML2_LIBRARIES REQUIRED xml2)
else (USESYSTEMLIBS)
  use_prebuilt_binary(libxml2)

  set(LIBXML2_INCLUDES
      ${LIBS_PREBUILT_DIR}/include/libxml2}
	  )
  if (DARWIN OR LINUX)
    set(LIBXML2_LIBRARIES
        xml2
        iconv
	    )
  elseif (WINDOWS)
    set(LIBXML2_LIBRARIES
	    libxml2_a
	    )
  endif()
endif (USESYSTEMLIBS)
