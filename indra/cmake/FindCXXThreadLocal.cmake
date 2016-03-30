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
