# -*- cmake -*-
include(Prebuilt)

set(Boost_FIND_QUIETLY ON)
set(Boost_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindBoost)

  set(BOOST_CHRONO_LIBRARY boost_chrono-mt)
  set(BOOST_CONTEXT_LIBRARY boost_context-mt)
  set(BOOST_COROUTINE_LIBRARY boost_coroutine-mt)
  set(BOOST_DATE_TIME_LIBRARY boost_date_time-mt)
  set(BOOST_FILESYSTEM_LIBRARY boost_filesystem-mt)
  set(BOOST_IOSTREAMS_LIBRARY boost_iostreams-mt)
  set(BOOST_PROGRAM_OPTIONS_LIBRARY boost_program_options-mt)
  set(BOOST_REGEX_LIBRARY boost_regex-mt)
  set(BOOST_SIGNALS_LIBRARY boost_signals-mt)
  set(BOOST_SYSTEM_LIBRARY boost_system-mt)
  set(BOOST_THREAD_LIBRARY boost_thread-mt)
else (USESYSTEMLIBS)
  use_prebuilt_binary(boost)
  set(Boost_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)
  set(BOOST_VERSION "1.69")

  if (WINDOWS)
    if (ADDRESS_SIZE EQUAL 64)
      set(BOOST_LIB_POSTFIX "-x64")
    else()
      set(BOOST_LIB_POSTFIX "-x32")
    endif()
    set(BOOST_CHRONO_LIBRARY
        optimized libboost_chrono-mt${BOOST_LIB_POSTFIX}
        debug libboost_chrono-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_CONTEXT_LIBRARY
        optimized libboost_context-mt${BOOST_LIB_POSTFIX}
        debug libboost_context-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_COROUTINE_LIBRARY
        optimized libboost_coroutine-mt${BOOST_LIB_POSTFIX}
        debug libboost_coroutine-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_DATE_TIME_LIBRARY
        optimized libboost_date_time-mt${BOOST_LIB_POSTFIX}
        debug libboost_date_time-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_FILESYSTEM_LIBRARY
        optimized libboost_filesystem-mt${BOOST_LIB_POSTFIX}
        debug libboost_filesystem-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_IOSTREAMS_LIBRARY
        optimized libboost_iostreams-mt${BOOST_LIB_POSTFIX}
        debug libboost_iostreams-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_PROGRAM_OPTIONS_LIBRARY
        optimized libboost_program_options-mt${BOOST_LIB_POSTFIX}
        debug libboost_program_options-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_REGEX_LIBRARY
        optimized libboost_regex-mt${BOOST_LIB_POSTFIX}
        debug libboost_regex-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_SIGNALS_LIBRARY
        optimized libboost_signals-mt${BOOST_LIB_POSTFIX}
        debug libboost_signals-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_SYSTEM_LIBRARY
        optimized libboost_system-mt${BOOST_LIB_POSTFIX}
        debug libboost_system-mt-gd${BOOST_LIB_POSTFIX})
    set(BOOST_THREAD_LIBRARY
        optimized libboost_thread-mt${BOOST_LIB_POSTFIX}
        debug libboost_thread-mt-gd${BOOST_LIB_POSTFIX})
  elseif (LINUX)
    set(BOOST_CHRONO_LIBRARY
        optimized boost_chrono-mt
        debug boost_chrono-mt-d)
    set(BOOST_CONTEXT_LIBRARY
        optimized boost_context-mt
        debug boost_context-mt-d)
    set(BOOST_COROUTINE_LIBRARY
        optimized boost_coroutine-mt
        debug boost_coroutine-mt-d)
    set(BOOST_DATE_TIME_LIBRARY
        optimized boost_date_time-mt
        debug boost_date_time-mt-d)
    set(BOOST_FILESYSTEM_LIBRARY
        optimized boost_filesystem-mt
        debug boost_filesystem-mt-d)
    set(BOOST_IOSTREAMS_LIBRARY
        optimized boost_iostreams-mt
        debug boost_iostreams-mt-d)
    set(BOOST_PROGRAM_OPTIONS_LIBRARY
        optimized boost_program_options-mt
        debug boost_program_options-mt-d)
    set(BOOST_REGEX_LIBRARY
        optimized boost_regex-mt
        debug boost_regex-mt-d)
    set(BOOST_SIGNALS_LIBRARY
        optimized boost_signals-mt
        debug boost_signals-mt-d)
    set(BOOST_SYSTEM_LIBRARY
        optimized boost_system-mt
        debug boost_system-mt-d)
    set(BOOST_THREAD_LIBRARY
        optimized boost_thread-mt
        debug boost_thread-mt-d)
  elseif (DARWIN)
    set(BOOST_CHRONO_LIBRARY
        optimized boost_chrono-mt-x64
        debug boost_chrono-mt-x64-d)
    set(BOOST_CONTEXT_LIBRARY
        optimized boost_context-mt-x64
        debug boost_context-mt-x64-d)
    set(BOOST_COROUTINE_LIBRARY
        optimized boost_coroutine-mt-x64
        debug boost_coroutine-mt-x64-d)
    set(BOOST_DATE_TIME_LIBRARY
        optimized boost_date_time-mt-x64
        debug boost_date_time-mt-x64-d)
    set(BOOST_FILESYSTEM_LIBRARY
        optimized boost_filesystem-mt-x64
        debug boost_filesystem-mt-x64-d)
    set(BOOST_IOSTREAMS_LIBRARY
        optimized boost_iostreams-mt-x64
        debug boost_iostreams-mt-x64-d)
    set(BOOST_PROGRAM_OPTIONS_LIBRARY
        optimized boost_program_options-mt-x64
        debug boost_program_options-mt-x64-d)
    set(BOOST_REGEX_LIBRARY
        optimized boost_regex-mt-x64
        debug boost_regex-mt-x64-d)
    set(BOOST_SIGNALS_LIBRARY
        optimized boost_signals-mt-x64
        debug boost_signals-mt-x64-d)
    set(BOOST_SYSTEM_LIBRARY
        optimized boost_system-mt-x64
        debug boost_system-mt-x64-d)
    set(BOOST_THREAD_LIBRARY
        optimized boost_thread-mt-x64
        debug boost_thread-mt-x64-d)
  endif ()
endif (USESYSTEMLIBS)

if (LINUX)
    set(BOOST_SYSTEM_LIBRARY ${BOOST_SYSTEM_LIBRARY} rt)
    set(BOOST_THREAD_LIBRARY ${BOOST_THREAD_LIBRARY} rt)
endif (LINUX)

