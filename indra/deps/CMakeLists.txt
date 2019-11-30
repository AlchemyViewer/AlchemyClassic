project(deps)

include(FetchContent)

set(CMAKE_FOLDER "Third Party")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.11.0
  )
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        7512a55aa3ae309587ca89668ef9ec4074a51a1f
  )
FetchContent_Declare(
  absl
  GIT_REPOSITORY https://git.alchemyviewer.org/alchemy/thirdparty/abseil-cpp.git
  GIT_TAG        9b9a2536d1c212d792db9eb26b670b30397ee14c
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

unset(CMAKE_FOLDER)
unset(CMAKE_POSITION_INDEPENDENT_CODE)
