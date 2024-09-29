# LC3-VM-C
LC-3 Virtual Machine in C
Overview

This project implements a virtual machine to simulate the LC-3 (Little Computer 3) architecture in C. The LC-3 is a simple educational computer architecture that is widely used for teaching computer organization and assembly language. This implementation supports the execution of LC-3 machine code programs from image files, simulating memory, registers, and handling a wide array of LC-3 instructions.
Features

    Supports core LC-3 instructions: ADD, AND, NOT, LD, ST, JSR, BR, JMP, LDR, STR, and more.
    Handles input/output operations via TRAP instructions:
        TRAP_GETC: Reads a single character from the keyboard.
        TRAP_OUT: Outputs a character to the console.
        TRAP_PUTS: Outputs a null-terminated string.
        TRAP_IN: Reads a character from the keyboard and echoes it to the console.
        TRAP_HALT: Halts the program.
    Supports loading LC-3 machine code from image files.
    Simulates keyboard input via memory-mapped registers (KBSR, KBDR).

Requirements

    A C compiler (GCC, Clang, MSVC, etc.)
    Windows environment for console input handling (Windows API used for non-blocking input). Linux users may need to modify input handling.

How to Build and Run
Compilation

To compile the project, run the following command using your C compiler:

bash

gcc lc3.c -o lc3_vm

This will create an executable named lc3_vm.
Running the Virtual Machine

The virtual machine expects one or more LC-3 machine code image files as input. These image files contain the machine instructions to be loaded into memory. To run the virtual machine with an image file:

bash

lc3_vm [image-file1]

For example:

lc3_vm program.obj
