# -*- cmake -*-
include(Prebuilt)
include(Variables)

if (NVAPI)
  if (WINDOWS)
    use_prebuilt_binary(nvapi)
    if (WORD_SIZE EQUAL 32)
      set(NVAPI_LIBRARY nvapi)
    elseif (WORD_SIZE EQUAL 64)
      set(NVAPI_LIBRARY nvapi64)
    endif (WORD_SIZE EQUAL 32)	
  else (WINDOWS)
    set(NVAPI_LIBRARY "")
  endif (WINDOWS)
else (NVAPI)
  set(NVAPI_LIBRARY "")
endif (NVAPI)

