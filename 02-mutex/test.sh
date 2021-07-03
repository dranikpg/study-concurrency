#!/bin/bash
cd "$(dirname "$0")"
gcc test.c $1_lock.c -lpthread -o test -D $1
./test
rm -rf test