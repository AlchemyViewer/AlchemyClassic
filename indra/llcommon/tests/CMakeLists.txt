include(00-Common)
include(LLCommon)
include(LLMath)
include(Linking)
include(Boost)
include(OpenSSL)
include(LLSharedLibs)
include(JsonCpp)
include(GoogleBreakpad)
include(GooglePerfTools)
include(GTest)
include(ZLIB)
include(URIPARSER)

set(LLCOMMON_TEST_LIBRARIES
      ${GTEST_BOTH_LIBRARIES}   
      ${LLCOMMON_LIBRARIES} 
      ${WINDOWS_LIBRARIES} 
      ${BOOST_COROUTINE_LIBRARY} 
      ${BOOST_CONTEXT_LIBRARY} 
      ${BOOST_THREAD_LIBRARY} 
      ${BOOST_SYSTEM_LIBRARY}
      ${OPENSSL_LIBRARIES}
      ${CRYPTO_LIBRARIES})

# creates the executable
add_executable(test_llrand llrand_test.cpp)

# indicates the include paths
target_include_directories(test_llrand PRIVATE ${GTEST_INCLUDE_DIRS} ${LLCOMMON_INCLUDE_DIRS})

target_link_libraries(test_llrand ${LLCOMMON_TEST_LIBRARIES})

# declares a test with our executable
GTEST_ADD_TESTS(test_llrand "" AUTO)
	  
	  
# creates the executable
add_executable(test_llbase64 llbase64_test.cpp)

# indicates the include paths
target_include_directories(test_llbase64 PRIVATE ${GTEST_INCLUDE_DIRS} ${LLCOMMON_INCLUDE_DIRS})

target_link_libraries(test_llbase64 ${LLCOMMON_TEST_LIBRARIES})

# declares a test with our executable
GTEST_ADD_TESTS(test_llbase64 "" AUTO)