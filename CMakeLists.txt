cmake_minimum_required(VERSION 3.22)

project(ComputerEnhanceExercises)

add_library(exercise_compiler_flags INTERFACE)
target_compile_features(exercise_compiler_flags INTERFACE cxx_std_20)
target_compile_options(exercise_compiler_flags INTERFACE "-fno-exceptions;-Wall;-Wextra;-Wpedantic;")
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    target_compile_options(exercise_compiler_flags INTERFACE "-fsanitize=address,undefined")
    target_link_options(exercise_compiler_flags INTERFACE "-fsanitize=address,undefined")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(target_sources
    common.cpp
    instruction.cpp
    program.cpp
    emulator.cpp
)
list(TRANSFORM target_sources PREPEND "src/")

function(configure_executable target)
    target_sources(${target} PRIVATE ${target_sources})
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/lib/include")
    target_link_libraries(${target} PRIVATE exercise_compiler_flags)
endfunction()

add_executable(x86-emulator)
configure_executable(x86-emulator)
target_sources(x86-emulator PRIVATE src/main.cpp)


enable_testing()

add_executable(x86-emulator-test)
configure_executable(x86-emulator-test)
target_sources(x86-emulator-test PRIVATE src/test.cpp)
target_compile_definitions(x86-emulator-test PRIVATE TESTING)
set_target_properties(x86-emulator-test PROPERTIES EXCLUDE_FROM_ALL ON)

add_test(test_build
    "${CMAKE_COMMAND}"
    --build "${CMAKE_BINARY_DIR}"
    --config "$<CONFIG>"
    --target x86-emulator-test
    -j
    )
set_tests_properties(test_build PROPERTIES FIXTURES_SETUP test_fixture)

add_test(NAME tests COMMAND x86-emulator-test)
set_tests_properties(tests PROPERTIES FIXTURES_REQUIRED test_fixture)
