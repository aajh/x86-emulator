cmake_minimum_required(VERSION 3.22)

project(ComputerEnhanceExercises)

add_library(exercise_compiler_flags INTERFACE)
target_compile_features(exercise_compiler_flags INTERFACE cxx_std_20)
target_compile_options(exercise_compiler_flags INTERFACE "-fno-exceptions;-Wall;-Wextra;-Wpedantic;-fsanitize=address,undefined")
target_link_options(exercise_compiler_flags INTERFACE "-fsanitize=address,undefined")

add_executable(x86-sim)
add_subdirectory(src)
target_link_libraries(x86-sim PRIVATE exercise_compiler_flags)


enable_testing()
add_test(NAME RunsTest COMMAND x86-sim -test)