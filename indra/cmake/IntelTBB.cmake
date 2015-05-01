# -*- cmake -*-
include(Prebuilt)

if (STANDALONE)
else (STANDALONE)
  if (WINDOWS)
    if (USE_TBBMALLOC AND NOT USE_TCMALLOC)
       use_prebuilt_binary(inteltbb)
       set(TBBMALLOC_LIBRARIES 
         debug tbbmalloc_proxy_debug
         optimized tbbmalloc_proxy)
       if(WORD_SIZE EQUAL 64)
         set(MALLOC_LINK_FLAGS  "/INCLUDE:__TBB_malloc_proxy")
       else(WORD_SIZE EQUAL 64) 
         set(MALLOC_LINK_FLAGS  "/INCLUDE:___TBB_malloc_proxy")
       endif(WORD_SIZE EQUAL 64)
    else (USE_TCMALLOC AND NOT USE_TCMALLOC)
      set(TBBMALLOC_LIBRARIES)
      set(MALLOC_LINK_FLAGS)
    endif (USE_TBBMALLOC AND NOT USE_TCMALLOC)
  endif (WINDOWS)
  if (LINUX)
    if (USE_TBBMALLOC AND NOT USE_TCMALLOC)
      use_prebuilt_binary(inteltbb)
      set(TBBMALLOC_LIBRARIES 
        debug tbbmalloc_proxy_debug tbbmalloc_debug
        optimized tbbmalloc_proxy tbbmalloc)
    else (USE_TBBMALLOC AND NOT USE_TCMALLOC)
      set(TBBMALLOC_LIBRARIES)
    endif (USE_TBBMALLOC AND NOT USE_TCMALLOC)
  endif (LINUX)
endif (STANDALONE)

