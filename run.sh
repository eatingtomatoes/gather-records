#! /usr/bin/env bash

export LLVM_SYMBOLIZER_PATH=$(which llvm-symbolizer-10)

~/Projects/cpp/LLVM/build/bin/clang++ \
    -Xclang -load \
    -Xclang ~/Projects/cpp/LLVM/build/lib/gather-records.so \
    -Xclang -plugin \
    -Xclang gather-records \
    -I/home/zhangzhang/Projects/cpp/debug_template/cutlass/include \
    -I/home/zhangzhang/Projects/cpp/debug_template/cutlass/tools/util/include  \
    -g -x cu \
    -std=c++17 \
    --cuda-gpu-arch=sm_50 \
    -Wno-unknown-cuda-version \
    -fsyntax-only \
    -c /home/zhangzhang/Projects/cpp/debug_template/src/main.cu \
    -o main.cu.o
