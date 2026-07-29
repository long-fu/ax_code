cmake_minimum_required(VERSION 3.10.0)
project(ax_core VERSION 0.1.0 LANGUAGES C CXX)


# set(CMAKE_C_COMPILER "aarch64-linux-gnu-gcc-9")
# set(CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++-9")

# set(CMAKE_C_COMPILER clang-15)
# set(CMAKE_CXX_COMPILER clang-15)

# add_compile_options(-std=c++11)

set(CMAKE_CXX_FLAGS_DEBUG "-fPIC -O0 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-fPIC -O2 -Wall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(
    3rdpart/ax/include
    3rdpart/opencv/include
    3rdpart/ffmpeg/include
    3rdpart/x264/include
    tracker/sort
    common
    hal
    3rdpart/freetype/include/freetype
    pipeline
    nodes
    image_search
    3rdpart/curl/include
    3rdpart/libuuid/include
    3rdpart/include
    3rdpart/openssl/include
    3rdpart/zlib/include
    3rdpart/libidn2/include
    3rdpart/include
)

link_directories(
    3rdpart/ax/lib
    3rdpart/opencv/lib
    3rdpart/ffmpeg/lib
    3rdpart/x264/lib
    3rdpart/spdlog/lib
    3rdpart/freetype/lib
    3rdpart/nlohmann/
    3rdpart/curl/lib
    3rdpart/libuuid/lib
    3rdpart/openssl/lib64
    3rdpart/zlib/lib
    3rdpart/libidn2/lib
)

aux_source_directory(hal HAL_SRCS)
aux_source_directory(pipeline PIP_SRCS)
aux_source_directory(tracker/sort TRACKER_SRCS)
aux_source_directory(nodes NODES_SRCS)
aux_source_directory(common COMMON_SRCS)

add_executable(ax_core
    apps/main_rtsp.cpp
    ${HAL_SRCS}
    ${NODES_SRCS}
    ${TRACKER_SRCS}
    ${PIP_SRCS}
    ${COMMON_SRCS}
    pipeline/error.h
)

# Suppress warnings in detection.cpp (model utility library with many inference backends)
set_source_files_properties(hal/detection.cpp PROPERTIES COMPILE_FLAGS
    "-Wno-unused-function -Wno-unknown-pragmas -Wno-sign-compare -Wno-unused-but-set-variable")

# Suppress warnings in jpeg_help.cpp (fread result checks and sign-compare with off_t)
set_source_files_properties(hal/jpeg_help.cpp PROPERTIES COMPILE_FLAGS
    "-Wno-unused-result -Wno-sign-compare")

# ── Link libraries ──────────────────────────────────────────

set(SYSTEM_LIBS
    pthread
    dl
)

set(OPENCV_LIBS
    opencv_imgcodecs
    opencv_core
    opencv_imgproc
    opencv_video
    opencv_videoio
    opencv_highgui
)

set(AX_LIBS
    ax_sys
    ax_ive
    ax_ives
    ax_ivps
    ax_engine
    ax_venc
    ax_vdec
    ax_interpreter
)

set(FFMPEG_LIBS
    avcodec
    avutil
    swscale
    avformat
)

set(THIRDPARTY_LIBS
    spdlog
    freetype
    curl
    uuid
    crypto
    z
    idn2
)

target_link_libraries(ax_core PUBLIC
    ${SYSTEM_LIBS}
    ${OPENCV_LIBS}
    ${AX_LIBS}
    ${FFMPEG_LIBS}
    ${THIRDPARTY_LIBS}
)

add_subdirectory(rules)
