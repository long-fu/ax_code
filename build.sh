#!/bin/bash

rm -rf build
cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build