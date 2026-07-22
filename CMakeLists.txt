cmake_minimum_required(VERSION 3.20)

project(affine_equivalence_algorithms LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(affine_core INTERFACE)
target_include_directories(affine_core INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_link_libraries(affine_core INTERFACE Threads::Threads)

function(affine_add_executable target source)
    add_executable("${target}" "${source}")
    target_link_libraries("${target}" PRIVATE affine_core)
endfunction()

affine_add_executable(aleq app/main.cpp)

if(BUILD_TESTING)
    set(AFFINE_TEST_TARGETS
        affine_map_tests
        apn_refinement_tests
        cross_signature_tests
        dfs_skeleton_tests
        equivalence_api_tests
        group_tests
        parallel_search_tests
        partition_tests
        refinement_tests
        signatures_f2_tests
        signatures_partition_refinement_tests
        solution_store_tests
        work_tests
        worker_tests
    )

    foreach(test_target IN LISTS AFFINE_TEST_TARGETS)
        affine_add_executable("${test_target}" "tests/${test_target}.cpp")

        # The tests use assert(), including in Release builds.
        if(MSVC)
            target_compile_options("${test_target}" PRIVATE /UNDEBUG)
        else()
            target_compile_options("${test_target}" PRIVATE -UNDEBUG)
        endif()

        if(test_target STREQUAL "apn_refinement_tests")
            add_test(
                NAME "${test_target}"
                COMMAND "${test_target}" "${CMAKE_CURRENT_SOURCE_DIR}/apn_original_function_folders"
            )
        else()
            add_test(NAME "${test_target}" COMMAND "${test_target}")
        endif()
    endforeach()
endif()

install(TARGETS aleq
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(DIRECTORY examples/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/aleq/examples"
)
