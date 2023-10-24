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
    arguments="src/*.cpp"
fi

$clang_tidy_cmd -checks="-*,bugprone-*,cppcoreguidelines-*,clang-analyzer-*" -p build $arguments
