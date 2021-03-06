cmake_minimum_required(VERSION 3.6)

project(mpc_planner_node)

### Dependencies
find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()
### Compile options
set(CMAKE_CXX_STANDARD 14)

### Build
ament_auto_add_library(${PROJECT_NAME} SHARED
  src/mpc_planner_node/mpc_planner_node.cpp)
mpc_warnings(${PROJECT_NAME} RCLCPP MSGS TF2)
mpc_clang_tidy(${PROJECT_NAME} RCLCPP MSGS TF2
  ADDITIONAL_CHECKS "-fuchsia-default-arguments" # I'm honestly just being lazy
)

ament_auto_add_executable(${PROJECT_NAME}_exe
  src/mpc_planner_node/mpc_planner_main.cpp)
mpc_warnings(${PROJECT_NAME}_exe RCLCPP MSGS TF2)
mpc_clang_tidy(${PROJECT_NAME}_exe RCLCPP MSGS TF2
  ADDITIONAL_CHECKS "-fuchsia-default-arguments" # I'm honestly just being lazy
)

### Test
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # Linters
  ament_lint_auto_find_test_dependencies()
  # Unit test
  # ament_add_gtest(mpc_planner_node_unit_tests
  #   test/gtest_main.cpp
  #   test/sanity_check.cpp)
  # target_link_libraries(mpc_planner_node_unit_tests ${PROJECT_NAME})
  # mpc_warnings(mpc_planner_node_unit_tests GTEST MSGS RCLCPP TF2)
  # add_dependencies(mpc_planner_node_unit_tests ${PROJECT_NAME})
endif()

ament_auto_package(INSTALL_TO_SHARE param/)
