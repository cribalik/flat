#!/usr/bin/env sh
cd $(dirname $0)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib:./build
gdb ./build/flat
