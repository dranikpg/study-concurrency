#!/bin/bash
cd "$(dirname "$0")"
nasm -f elf64 main.S -o nasm.o
ar rcs libnasm.a nasm.o
rm -rf nasm.o