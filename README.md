# x86-emulator
An Intel 8086 emulator and disassembler written in C++20. Currently it can decode and disassemble most Intel 8086 machine code and emulate a few basic instructions (mov, add, sub, cmp, jumps, loop, call and ret) without segmented memory access.

Written while following along [Performance-Aware Programming Series](https://www.computerenhance.com/p/table-of-contents).

Tested on macOS 12.6 with Apple Clang 13.1.6 and Ubuntu 22.04.3 LTS with GNU C++ Compiler 11.4.0.


## Compiling
This project requires CMake 3.22 and a C++20 compiler. This project has no external dependecies.

The requirements for building and testing can be installed with one of the following ways:
```
# macOS with Homebrew
brew install cmake nasm
# Ubuntu
sudo apt install build-essential cmake nasm
```

To compile, run the following commands in the repository root:
```
mkdir build && cd $_
cmake ..
cmake --build . -j
```
This will build a binary named `x86-emulator` in the build folder.


## Usage
Run `x86-emulator --help` to get the complete usage information.

To disassemble a file, run
```
x86-emulator --disassemble file_with_machine_code
```
To execute it, run
```
x86-emulator --execute file_with_machine_code
```

If the given file ends with `.asm`, it will be assembled with `nasm` assembler and the resulting binary
will be used as input, e.g.
```
x86-emulator --disassemble program.asm
```

When executing, the program will be loaded to memory address 0, where the execution will also begin.
A special halt instruction (opcode `0x0f`, normally unused in an 8086) will be inserted at the end of the program.


## Testing
A suite of tests can be run with command `ctest` from the build folder.

The tests require that `nasm` disassembler is installed and that `computer_enhance` git submodule is present (run `git submodule update --init --recursive` to initialize it).
Make sure that the build folder is in the repository root, so that the test files can be found.

To see what tests run, use `cmake --build . --target x86-emulator-test -j && ./x86-emulator-test`.

The disassembly tests take compiled machine code (or assembles `.asm` file), decompiles that, reassemble the decompiled assembly and compare the resulting machine code with the original.

The emulation tests run a program and compare the final state of the registers and flags to the provided expected state.

