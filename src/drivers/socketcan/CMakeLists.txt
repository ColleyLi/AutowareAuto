cmake_minimum_required(VERSION 3.5)
project(socketcan)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

ament_auto_add_library(${PROJECT_NAME} SHARED
  src/socket_can_common.hpp
  src/socket_can_common.cpp
  src/socket_can_id.cpp
  src/socket_can_receiver.cpp
  src/socket_can_sender.cpp)
autoware_set_compile_options(${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # TODO(c.ho) Make this into a pytest
  ament_add_gtest(socketcan_test
    test/gtest_main.cpp
    test/receiver.cpp
    test/sanity_checks.cpp)
  target_include_directories(socketcan_test PUBLIC include)
  target_link_libraries(socketcan_test ${PROJECT_NAME})
endif()

ament_auto_package()
