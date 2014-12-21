# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  use_prebuilt_binary(libhunspell)
  use_prebuilt_binary(slvoice)
  use_prebuilt_binary(fonts)
  if (LINUX)
    use_prebuilt_binary(libuuid)
    use_prebuilt_binary(fontconfig)
  endif (LINUX)
endif(NOT USESYSTEMLIBS)
