# -*- cmake -*-
include(Prebuilt)

if (USESYSTEMLIBS)
else (USESYSTEMLIBS)
  if (WINDOWS)
    use_prebuilt_binary(icu4c)

    set(ICU4C_INCLUDES
        ${LIBS_PREBUILT_DIR}/include
	    )

    set(ICU4C_LIBRARIES
        optimized icuin
        debug icuind
        optimized icuuc
        debug icuucd
        icudt
	    )
  elseif (LINUX)
    use_prebuilt_binary(icu4c)

    set(ICU4C_INCLUDES
        ${LIBS_PREBUILT_DIR}/include
	    )

    set(ICU4C_LIBRARIES
        icui18n
        icuuc
        icudata
	    )
  endif (WINDOWS)
endif (USESYSTEMLIBS)
