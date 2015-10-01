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
#    HAVE_SINGLE_QUOTE_DIGIT_SEPERATOR
#    HAVE_REVERSE_ITERATOR
#    HAVE_MAKE_REVERSE_ITERATOR
#    HAVE_OPTIONAL

include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)
cmake_push_check_state()

# Test for C++14 flags for non-msvc compilers
if(NOT MSVC)
  CHECK_CXX_COMPILER_FLAG("-std=c++14" CXX_FLAG_CXX14)
  if(CXX_FLAG_CXX14)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++14")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++14 ")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -std=c++14 ")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++14 ")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -std=c++14 ")
    set(CXX_STD14_FLAGS "-std=c++14")
  else()
    CHECK_CXX_COMPILER_FLAG("-std=c++1y" CXX_FLAG_CXX1Y)
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
endif(NOT MSVC)

# Binary literals
CHECK_CXX_SOURCE_COMPILES("
    int main(void) {
      int x = 0b000;
      return x;
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

# Single-quote digit seperator
CHECK_CXX_SOURCE_COMPILES("
    int main(void) {
      float x = 1.602'176'565e-19;
      return x-x;
    }
" HAVE_SINGLE_QUOTE_DIGIT_SEPERATOR
)

CHECK_CXX_SOURCE_COMPILES("
    #include <iostream>
    #include <vector>
    #include <iterator>
    #include <algorithm>
    int main(void) {
      std::vector<int> v = { 1, 2, 3 };
      std::copy(std::rbegin(v), std::rend(v), std::ostream_iterator<int>(std::cout, ' '));
      return 0;
    }
" HAVE_REVERSE_ITERATOR
)

CHECK_CXX_SOURCE_COMPILES("
	#include <iostream>
	#include <iterator>
	#include <vector>
	#include <algorithm>

	int main() {
		auto v = std::vector<int>{ 1, 3, 10, 8, 22 };
 
		std::sort(v.begin(), v.end());
		std::copy(v.begin(), v.end(), std::ostream_iterator<int>(std::cout, \", \"));
 
		std::cout << '\n';
 
		std::copy(
			std::make_reverse_iterator(v.end()),
			std::make_reverse_iterator(v.begin()),
			std::ostream_iterator<int>(std::cout, \", \"));
		return 0;
}
" HAVE_MAKE_REVERSE_ITERATOR
)

CHECK_CXX_SOURCE_COMPILES("
	#include <experimental/optional>
	template<typename T>
	using optional = std::experimental::optional<T>;

	optional<float> square_root( float x ) {
		return x > 0 ? std::sqrt(x) : optional<float>();
	}

	int main(void) {
		float sq = square_root(9);
		return sq-3;
	}
}

" HAVE_OPTIONAL
)

cmake_pop_check_state()
