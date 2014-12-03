# -*- cmake -*-
#
# Module that checks for C++14 features
# Copyright 2014 Cinder Roxley <cinder@sdf.org> All rights reserved.
#
# Output variables:
#
#    HAVE_BINARY_LITERAL
#    HAVE_LAMBDA_CAPTURE
#    HAVE_GENERIC_LAMBDA
#    HAVE_ATTRIBUTE_DEPRECATED

include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
cmake_push_check_state()

# Test for C++14 flags :o
include(TestCXXAcceptsFlag)

CHECK_CXX_ACCEPTS_FLAG("-std=c++14" CXX_FLAG_CXX14)
if(CXX_FLAG_CXX14)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++14")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 ")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -std=c++14 ")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 ")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -std=c++14 ")
  set(CXX_STD14_FLAGS "-std=c++14")
else()
  CHECK_CXX_ACCEPTS_FLAG("-std=c++1y" CXX_FLAG_CXX1Y)
  if(CXX_FLAG_CXX1Y)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++1y" )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y ")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++1y ")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -std=c++1y ")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++1y ")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -std=c++1y ")
    set(CXX_STD14_FLAGS "-std=c++1y")
  endif(CXX_FLAG_CXX1Y)
endif(CXX_FLAG_CXX14)

# Binary literals
CHECK_CXX_SOURCE_COMPILES("
    int main(void) {
      int x = 0b1001;
      return 0;
    }
"  HAVE_BINARY_LITERAL
)

# Lambda captures
CHECK_CXX_SOURCE_COMPILES("
    int main(void) {
      int x = 4;
      auto y = [&r = x, x = x+1]()->int {
        r += 2;
        return x+2;
      }();
      return 0;
    }
" HAVE_LAMBDA_CAPTURE
)

# Generic lambda
CHECK_CXX_SOURCE_COMPILES("
    int main(void) {
        auto glambda  = [](auto a, auto&& b) { return  a < b; };
        bool b = glambda(3, 3.14);
        return b ? 0 : 1;
    }
" HAVE_GENERIC_LAMBDA
)

# [[deprecated]] attribute
CHECK_CXX_SOURCE_COMPILES("
    void __attribute__((deprecated)) foo(void) {}
    int main(void) {
      foo();
      return 0;
    }
" HAVE_ATTRIBUTE_DEPRECATED
)

cmake_pop_check_state()
