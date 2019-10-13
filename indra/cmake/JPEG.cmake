# -*- cmake -*-
include(Prebuilt)

include(Linking)
set(JPEG_FIND_QUIETLY ON)
set(JPEG_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindJPEG)
else (USESYSTEMLIBS)

  if (LINUX)
    use_prebuilt_binary(jpeglib)
    set(JPEG_LIBRARIES jpeg)
  elseif (DARWIN)
    use_prebuilt_binary(jpeglib)
    set(JPEG_LIBRARIES jpeg)
  elseif (WINDOWS)
    use_prebuilt_binary(libjpeg-turbo)
    set(JPEG_LIBRARIES jpeg-static)
  endif (LINUX)
  set(JPEG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
endif (USESYSTEMLIBS)
