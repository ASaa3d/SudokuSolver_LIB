#!/bin/bash
ORT_ROOT="/mnt/data/main/c++/min_sud/external/onnxruntime"  # folder containing lib/ and include/

cmake -B build -DONNX_RUNTIME_DIR="$ORT_ROOT"
cmake --build build
if (($? == 0));then
    ./build/min_sud $1
fi