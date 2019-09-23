# -*- cmake -*-

if (DARWIN)
  set(LLDARWIN_INCLUDE_DIRS
    ${LIBS_OPEN_DIR}/lldarwin
    )

  set(LLDARWIN_LIBRARIES
    lldarwin
    )
endif (DARWIN)

