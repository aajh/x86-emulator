#!/bin/bash

echoerr() { printf "%s\n" "$*" >&2; }

if hash clang-tidy 2> /dev/null; then
    clang_tidy_cmd=clang-tidy
elif /usr/local/opt/llvm/bin/clang-tidy --version > /dev/null 2>&1; then
    clang_tidy_cmd=/usr/local/opt/llvm/bin/clang-tidy
else
    echoerr "clang-tidy not found from PATH or /usr/local/opt/llvm/bin/"
    exit 1
fi

if [ ! -f build/compile_commands.json ]; then
    echo "build/compile_commands.json not found, creating build..."
    mkdir build && cd $_
    cmake ..
    cd ..
    if [ ! -f build/compile_commands.json ]; then
        echoerr "couldn't create build/compile_commands.json"
        exit 1
    fi
fi

arguments="$@"
if [ -z "$1" ]; then
    arguments="src/*.cpp src/*.hpp"
fi

disabled_checks="-cppcoreguidelines-pro-type-vararg,-cppcoreguidelines-pro-bounds-pointer-arithmetic,-cppcoreguidelines-owning-memory,-cppcoreguidelines-avoid-magic-numbers,-cppcoreguidelines-avoid-do-while,-cppcoreguidelines-pro-bounds-constant-array-index,-cppcoreguidelines-avoid-goto,-cppcoreguidelines-pro-type-union-access,-bugprone-easily-swappable-parameters,-bugprone-branch-clone"

$clang_tidy_cmd -checks="-*,bugprone-*,cppcoreguidelines-*,clang-analyzer-*,performance-*,${disabled_checks}" -p build $arguments
