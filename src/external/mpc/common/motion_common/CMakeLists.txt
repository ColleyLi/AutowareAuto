cmake_minimum_required(VERSION 3.6)

project(motion_common)

### Dependencies
find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()
### Compile options
set(CMAKE_CXX_STANDARD 14)

# Build library
ament_auto_add_library(${PROJECT_NAME} SHARED
  src/motion_common/config.cpp
  src/motion_common/motion_common.cpp
)
mpc_warnings(${PROJECT_NAME} MSGS)
mpc_clang_tidy(${PROJECT_NAME})

### Test
if(BUILD_TESTING)
  # Linters
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
  # Unit test
  apex_test_tools_add_gtest(motion_common_unit_tests
    test/gtest_main.cpp
    test/interpolation.cpp)
  target_link_libraries(motion_common_unit_tests ${PROJECT_NAME})
  mpc_warnings(motion_common_unit_tests GTEST MSGS ADDITIONAL_WARNINGS -Wno-missing-prototypes)
  add_dependencies(motion_common_unit_tests ${PROJECT_NAME})
endif()

# Install snippets for code generation
install(DIRECTORY scripts/autogeneration_code_snippets DESTINATION share/)

ament_auto_package()
