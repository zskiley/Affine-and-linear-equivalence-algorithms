cmake_minimum_required(VERSION 3.20)

project(AffineEquivalenceAlgorithms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(affine_core STATIC
    src/dfs.cpp
)

target_include_directories(affine_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (MSVC)
    target_compile_options(affine_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(affine_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(affine_equiv
    tools/affine_equiv.cpp
)

target_link_libraries(affine_equiv PRIVATE affine_core)

if (MSVC)
    target_compile_options(affine_equiv PRIVATE /W4 /permissive-)
else()
    target_compile_options(affine_equiv PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_test(
    NAME example_linear_equivalence
    COMMAND affine_equiv equiv --mode linear
            --left examples/identity_3.tt
            --right examples/linear_swap_3.tt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    example_linear_equivalence
    PROPERTIES PASS_REGULAR_EXPRESSION "found=1"
)

add_test(
    NAME example_affine_equivalence
    COMMAND affine_equiv equiv --mode affine
            --left examples/identity_3.tt
            --right examples/affine_translate_3.tt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    example_affine_equivalence
    PROPERTIES PASS_REGULAR_EXPRESSION "found=1"
)

add_test(
    NAME example_seeded_equivalence
    COMMAND affine_equiv seeded-equiv --mode affine
            --left examples/identity_3.tt
            --right examples/affine_translate_3.tt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    example_seeded_equivalence
    PROPERTIES PASS_REGULAR_EXPRESSION "found=1"
)

add_test(
    NAME example_self_equivalence
    COMMAND affine_equiv self --mode affine
            --function examples/identity_3.tt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    example_self_equivalence
    PROPERTIES PASS_REGULAR_EXPRESSION "task=self"
)

add_custom_target(check_examples
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS affine_equiv
)
