project(deps)

include(FetchContent)

set(CMAKE_FOLDER "Third Party")

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.10.0
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
  )
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        7512a55aa3ae309587ca89668ef9ec4074a51a1f
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
  )
FetchContent_Declare(
  absl
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        e9f9000c7c80993cb589d011616b7a8016e42f4a
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)

# This is a hack because absl has dumb cmake
set(OLD_BUILD_TEST ${BUILD_TESTING})
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(absl)
set(BUILD_TESTING ${OLD_BUILD_TEST})

# Supress warnings inside abseil under MSVC
if(WINDOWS)
  target_compile_options(absl_strings PRIVATE /wd4018)
  target_compile_options(absl_str_format_internal PRIVATE /wd4018)
  target_compile_options(absl_flags_usage_internal PRIVATE /wd4018)
endif()


if (BUILD_TESTING)
  FetchContent_MakeAvailable(Catch2)
endif()

#Download the rest of the libraries
FetchContent_MakeAvailable(fmt)

set(CMAKE_FOLDER "")
