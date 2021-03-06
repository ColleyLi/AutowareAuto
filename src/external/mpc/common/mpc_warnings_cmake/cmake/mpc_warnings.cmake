# Applies all compiler warnings to the target, except for the ones triggered by some
# pre-specified external headers
# :param EXEC_OR_LIB: Input target, gets checks applied to it
# :type EXEC_OR_LIB: A target
# :option GTEST: Disable warnings gtest trips
# :option RCLCPP: Disable warnings rclcpp trips
# :option MSGS: Disable warnings autogenerated messages trip
# :option TF2: Disable warnings tf2 trips
# :option WARNING_AS_ERRORS: enable warnings as errors
# :param ADDITIONAL_WARNINGS: Additional warnings/flags to be forwarded as compile options
# :type ADDITIONAL_WARNINGS: List of strings
function(mpc_warnings EXEC_OR_LIB)
  set(_ARG_OPTIONS "GTEST;RCLCPP;TF2;MSGS;WARNINGS_AS_ERRORS")
  set(_ARGN_NAMES "ADDITIONAL_WARNINGS")

  cmake_parse_arguments(ARG
    "${_ARG_OPTIONS}" # Options
    "" # single value keywords
    "${_ARGN_NAMES}" # ARGN / multi_value_keywords
    ${ARGN})
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "mpc_warnings called with unused arguments: " ${ARG_UNPARSED_ARGUMENTS})
  endif()

  # Base checks
  set(_WARNINGS)
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    list(APPEND _WARNINGS
      -Weverything
      -Wno-reserved-id-macro # __ is forbidden in macros--incompatible with cpplint
      -Wno-c++98-compat
      -Wno-c++98-compat-pedantic)

    if(ARG_GTEST)
      list(APPEND _WARNINGS
        -Wno-padded # Gtest from here on down
        -Wno-weak-vtables
        -Wno-global-constructors
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-undef
        -Wno-deprecated
        -Wno-shift-sign-overflow)
    endif()

    if(ARG_RCLCPP)
      list(APPEND _WARNINGS
        -Wno-inconsistent-missing-destructor-override # rclcpp from here on down
        -Wno-shadow-uncaptured-local
        -Wno-shadow
        -Wno-covered-switch-default
        -Wno-non-virtual-dtor
        -Wno-zero-as-null-pointer-constant
        -Wno-documentation-unknown-command
        -Wno-documentation-pedantic
        -Wno-padded
        -Wno-tautological-type-limit-compare
        -Wno-undefined-func-template
        -Wno-weak-vtables
        -Wno-undef
        -Wno-format-nonliteral # rclcpp logging
        )
    endif()

    if(ARG_MSGS)
      list(APPEND _WARNINGS
        -Wno-padded
        -Wno-documentation)
    endif()

    if(ARG_TF2)
      list(APPEND _WARNINGS
        -Wno-extra-semi  # tf2 from here.. very uncomfortable..
        -Wno-float-equal
        -Wno-double-promotion
        -Wno-comma
        -Wno-old-style-cast
        -Wno-sign-conversion
        -Wno-cast-qual)
    endif()

    if(ARG_ADDITIONAL_WARNINGS)
      list(APPEND _WARNINGS ${ARG_ADDITIONAL_WARNINGS})
    endif()

    if(ARG_WARNINGS_AS_ERRORS)
      list(APPEND _WARNINGS -Werror)
    endif()
  else()
    # Basic checks... should eventually actually fill this out
    set(_WARNINGS -Wall -Wextra -Wpedantic)
  endif()

  target_compile_options(${EXEC_OR_LIB} PRIVATE ${_WARNINGS})
endfunction()
