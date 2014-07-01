# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  use_prebuilt_binary(libhunspell)
  use_prebuilt_binary(libuuid)
  use_prebuilt_binary(slvoice)
  use_prebuilt_binary(fontconfig)
  if (DARWIN AND NOT WORD_SIZE EQUAL 32)
    use_prebuilt_binary(slplugin_x86)
  endif (DARWIN AND NOT WORD_SIZE EQUAL 32)
endif(NOT USESYSTEMLIBS)

