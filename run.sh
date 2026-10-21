#!/bin/bash

cmake -B build
cmake --build build
if (($? == 0));then
    ./build/min_sud $1
fi