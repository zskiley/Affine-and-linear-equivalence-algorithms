cmake_minimum_required(VERSION 3.20)

project(AffineEquivalenceAlgorithms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(affine_core STATIC
    src/dfs.cpp
    src/equivalence_api.cpp
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

add_executable(aleq
    tools/aleq.cpp
)

target_link_libraries(aleq PRIVATE affine_core)

if (MSVC)
    target_compile_options(aleq PRIVATE /W4 /permissive-)
else()
    target_compile_options(aleq PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(rectangular_api_example
    examples/rectangular_api.cpp
)

target_link_libraries(rectangular_api_example PRIVATE affine_core)

if (MSVC)
    target_compile_options(rectangular_api_example PRIVATE /W4 /permissive-)
else()
    target_compile_options(rectangular_api_example PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_executable(equivalence_api_tests
    tests/equivalence_api_tests.cpp
)

target_link_libraries(equivalence_api_tests PRIVATE affine_core)

if (MSVC)
    target_compile_options(equivalence_api_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(equivalence_api_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(
    NAME equivalence_api
    COMMAND equivalence_api_tests
)

add_test(
    NAME rectangular_api_example
    COMMAND rectangular_api_example
)
set_tests_properties(
    rectangular_api_example
    PROPERTIES PASS_REGULAR_EXPRESSION "equivalences: 4"
)

add_test(
    NAME aleq_affine_equivalence
    COMMAND aleq examples/identity_3.tt examples/affine_translate_3.tt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_affine_equivalence
    PROPERTIES PASS_REGULAR_EXPRESSION "equivalent: yes"
)

add_test(
    NAME aleq_linear_nonequivalence
    COMMAND aleq examples/identity_3.tt examples/affine_translate_3.tt --linear
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_linear_nonequivalence
    PROPERTIES PASS_REGULAR_EXPRESSION "equivalent: no"
)

add_test(
    NAME aleq_self_all
    COMMAND aleq examples/identity_3.tt --self --all
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_self_all
    PROPERTIES PASS_REGULAR_EXPRESSION "self-equivalences: 1344"
)

add_test(
    NAME aleq_self_generators
    COMMAND aleq examples/identity_3.tt --self
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_self_generators
    PROPERTIES PASS_REGULAR_EXPRESSION "generators: [1-9][0-9]*"
)

add_test(
    NAME aleq_rectangular
    COMMAND aleq examples/embedding_1_to_2.tt
            examples/embedding_1_to_2_alt.tt --codomain-dim 2 --all
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_rectangular
    PROPERTIES PASS_REGULAR_EXPRESSION "equivalences: 4"
)

add_test(
    NAME aleq_show_witness
    COMMAND aleq examples/identity_3.tt examples/affine_translate_3.tt --show
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_show_witness
    PROPERTIES PASS_REGULAR_EXPRESSION "witness:"
)

add_test(
    NAME aleq_show_generators
    COMMAND aleq examples/identity_3.tt --self --show
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_show_generators
    PROPERTIES PASS_REGULAR_EXPRESSION "generator 1:"
)

add_test(
    NAME aleq_show_all
    COMMAND aleq examples/embedding_1_to_2.tt
            examples/embedding_1_to_2_alt.tt
            --codomain-dim 2 --all --show
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_show_all
    PROPERTIES PASS_REGULAR_EXPRESSION "equivalence 4:"
)

add_test(
    NAME aleq_invalid_arguments
    COMMAND aleq examples/identity_3.tt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(
    aleq_invalid_arguments
    PROPERTIES WILL_FAIL TRUE
)

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
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS affine_equiv aleq rectangular_api_example equivalence_api_tests
)
