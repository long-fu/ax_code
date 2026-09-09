#!/bin/bash

WORK_PATH=/home/workspace

LIB_PATH="${WORK_PATH}/3rdparty/ffmpeg/lib:\
${WORK_PATH}/3rdparty/opencv/lib:\
${WORK_PATH}/3rdparty/x264/lib:\
${WORK_PATH}/3rdparty/gdb/lib:\
${WORK_PATH}/3rdparty/freetype/lib:\
${WORK_PATH}/3rdparty/curl/lib:\
${WORK_PATH}/3rdparty/libuuid/lib:\
${WORK_PATH}/3rdparty/openssl/lib64:\
${WORK_PATH}/3rdparty/zlib/lib:\
${WORK_PATH}/3rdparty/libidn2/lib:\
${WORK_PATH}/3rdparty/yaml-cpp/lib:\
third-party/spdlog/lib:\
/soc/lib:\
"
export LD_LIBRARY_PATH=$LIB_PATH:$LD_LIBRARY_PATH
export PATH=/home/workspace/3rdparty/gdb/bin:$PATH 
rm -rf ./logs/*
./build/ax_core ./build/config.yaml
# gdb ./build/ax_core
# ./build/ax_core
