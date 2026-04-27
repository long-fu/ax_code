#!/bin/bash

WORK_PATH=$(pwd)

LIB_PATH="${WORK_PATH}/3rdpart/ffmpeg/lib:\
${WORK_PATH}/3rdpart/opencv/lib:\
${WORK_PATH}/3rdpart/x264/lib:\
/home/workspace/3rdparty/gdb/lib:\
${WORK_PATH}/3rdpart/freetype/lib:\
/soc/lib:\
"
echo "${LIB_PATH}"
export LD_LIBRARY_PATH=$LIB_PATH:$LD_LIBRARY_PATH
export PATH=/home/workspace/3rdparty/gdb/bin:$PATH 
rm rf ./logs/*
#gdb ./build/ax_core
./build/ax_core
