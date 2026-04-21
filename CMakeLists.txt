cmake_minimum_required(VERSION 3.20)

project(stronghold LANGUAGES CXX)

option(STRONGHOLD_ENABLE_OPENMP "Enable OpenMP if available" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(stronghold-cli
    stronghold-cli.cpp
)

target_include_directories(stronghold-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lyra
    ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann
)

target_compile_features(stronghold-cli PRIVATE cxx_std_23)

if(STRONGHOLD_ENABLE_OPENMP)
    find_package(OpenMP)
endif()

if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: building with OpenMP support")
    target_link_libraries(stronghold-cli PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(stronghold-cli PRIVATE STRONGHOLD_HAS_OPENMP=1)
else()
    message(STATUS "OpenMP not found or disabled: building without OpenMP")
    target_compile_definitions(stronghold-cli PRIVATE STRONGHOLD_HAS_OPENMP=0)
endif()

# On MinGW/MSYS2, copy the correct runtime DLLs next to the executable
# so the program can run from a normal Windows terminal as well.
if(MINGW)
    get_filename_component(_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)

    set(_runtime_dlls
        "${_compiler_bin}/libstdc++-6.dll"
        "${_compiler_bin}/libgcc_s_seh-1.dll"
        "${_compiler_bin}/libwinpthread-1.dll"
    )

    if(OpenMP_CXX_FOUND)
        list(APPEND _runtime_dlls "${_compiler_bin}/libgomp-1.dll")
    endif()

    add_custom_command(TARGET stronghold-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_runtime_dlls}
            $<TARGET_FILE_DIR:stronghold-cli>
        COMMAND_EXPAND_LISTS
    )
endif()

# -------------------------
# Tests
# -------------------------
enable_testing()

add_test(NAME cli_help
    COMMAND $<TARGET_FILE:stronghold-cli> --help
)

add_test(NAME cli_demo_ci
    COMMAND $<TARGET_FILE:stronghold-cli>
        --demo
        --nodes 20
        -n 1024
        -r 1
        --seed 1
        --demo-seed 1
        -o ctest-demo.json
)

set_tests_properties(cli_help cli_demo_ci PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    TIMEOUT 60
)