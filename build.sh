#!/bin/bash

rm -rf build
cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release -DAX_BUILD_TESTS=ON -S . -B build
cmake --build build -j2
