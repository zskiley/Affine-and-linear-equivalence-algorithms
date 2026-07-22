cmake_minimum_required(VERSION 3.20)

set(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(BUILD_DIR "${ROOT_DIR}/build")

message(STATUS "Configuring affine_equiv")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
    RESULT_VARIABLE CONFIGURE_RESULT
)
if (NOT CONFIGURE_RESULT EQUAL 0)
    message(FATAL_ERROR "Configuration failed")
endif()

message(STATUS "Building and running example checks")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target check_examples
    RESULT_VARIABLE BUILD_RESULT
)
if (NOT BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "Build or example checks failed")
endif()

message(STATUS "Done. The affine_equiv executable is in ${BUILD_DIR}.")
