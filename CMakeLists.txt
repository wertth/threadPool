cmake_minimum_required(VERSION 3.28)
project(threadPool)

include_directories("${CMAKE_SOURCE_DIR}/include")
set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/vcpkg_installed/arm64-osx)
enable_testing()

set(CMAKE_CXX_STANDARD 17)


add_executable(threadPool main.cpp)
add_executable(tests tests/taskTests.cpp)

find_package(GTest CONFIG REQUIRED)
target_link_libraries(tests PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

add_test(AllTestsInMain tests)


