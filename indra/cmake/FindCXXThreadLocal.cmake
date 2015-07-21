# -*- cmake -*-
#
# Hey kids, this module checks that thread_local works!
# Copyright 2015 Cinder Roxley <cinder@sdf.org> All rights reserved.
#
# Output variables:
#
#    CXX_FLAG_CXX11
#    HAVE_THREAD_LOCAL
#

include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)
cmake_push_check_state()

if(NOT MSVC)
  CHECK_CXX_COMPILER_FLAG("-std=c++11" CXX_FLAG_CXX11)
  if(CXX_FLAG_CXX11)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++11")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 ")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -std=c++11 ")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 ")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -std=c++11 ")
    set(CXX_STD0X_FLAGS "-std=c++11")
  else()
    CHECK_CXX_COMPILER_FLAG("-std=c++0x" CXX_FLAG_CXX0X)
    if(CXX_FLAG_CXX0X)
      set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++0x" )
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x ")
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++0x ")
      set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -std=c++0x ")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++0x ")
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -std=c++0x ")
      set(CXX_STD0X_FLAGS "-std=c++0x")
    endif(CXX_FLAG_CXX0X)
  endif(CXX_FLAG_CXX11)
endif(NOT MSVC)

CHECK_CXX_SOURCE_COMPILES("
#include <iostream>
#include <string>
#include <thread>
#include <mutex>

thread_local unsigned int rage = 1;
std::mutex cout_mutex;

void increase_rage(const std::string& thread_name)
{
    ++rage;
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << \"Rage counter for \" << thread_name << \": \" << rage << '\n';
}

int main()
{
    std::thread a(increase_rage, \"a\"), b(increase_rage, \"b\");
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << \"Rage counter for main: \" << rage << '\n';
    }
    
    a.join();
    b.join();
    
    return 0;
}
"  HAVE_THREAD_LOCAL
)
