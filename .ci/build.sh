#!/bin/bash -ex

mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER=clang++-19 \
    -DCMAKE_C_COMPILER=clang-19 
ninja
