#!/bin/bash

if hash clang-tidy 2> /dev/null; then
    echo "Has clang-tidy"
elif /usr/local/opt/llvm/bin/clang-tidy --version > /dev/null 2>&1; then
    echo "Has /usr/local/opt/llvm/bin/clang-tidy"
else
    echo "Has neither"
    exit
fi

echo "ASD"
