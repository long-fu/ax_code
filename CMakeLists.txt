cmake_minimum_required(VERSION 3.10.0)
project(ax_core VERSION 0.1.0 LANGUAGES C CXX)

# set(CMAKE_C_COMPILER "aarch64-linux-gnu-gcc-9")
# set(CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++-9")

# set(CMAKE_C_COMPILER clang-15)
# set(CMAKE_CXX_COMPILER clang-15)

# add_compile_options(-std=c++11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_COMPILER aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-none-linux-gnu-g++)

set(CMAKE_CXX_FLAGS_DEBUG "-fPIC -O0 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-fPIC -O2 -Wall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

include_directories(
    third-party/ax/include
    third-party/opencv/include
    third-party/opencv-4.5.5/include/opencv4
    third-party/ffmpeg/include
    third-party/x264/include
    tracker/sort
    common
    hal
    hal/ax
    third-party/freetype/include/freetype
    pipeline
    nodes
    image_search
    third-party/curl/include
    third-party/libuuid/include
    third-party/include
    third-party/openssl/include
    third-party/zlib/include
    third-party/libidn2/include
    third-party/include
)

link_directories(
    third-party/ax/lib
    third-party/opencv/lib
    third-party/ffmpeg/lib
    third-party/x264/lib
    third-party/freetype/lib
    third-party/nlohmann/
    third-party/curl/lib
    third-party/libuuid/lib
    third-party/openssl/lib64
    third-party/zlib/lib
    third-party/libidn2/lib
    third-party/opencv-4.5.5/lib/opencv4/3rdparty
    third-party/opencv-4.5.5/lib
    third-party/openssl/lib64
)

aux_source_directory(hal HAL_SRCS)
aux_source_directory(hal/ax AX_SRCS)
aux_source_directory(pipeline PIP_SRCS)

# aux_source_directory(tracker/sort TRACKER_SRCS)
aux_source_directory(nodes NODES_SRCS)
aux_source_directory(common COMMON_SRCS)

add_executable(ax_core
    apps/main_rtsp.cpp
    hal/drawing.cpp
    ${HAL_SRCS}
    ${AX_SRCS}
    ${NODES_SRCS}
    ${TRACKER_SRCS}
    ${PIP_SRCS}
    ${COMMON_SRCS}
    pipeline/error.h
)

# Suppress warnings in detection.cpp (model utility library with many inference backends)
set_source_files_properties(hal/ax/detection.cpp PROPERTIES COMPILE_FLAGS
    "-Wno-unused-function -Wno-unknown-pragmas -Wno-sign-compare -Wno-unused-but-set-variable")

# Suppress warnings in jpeg_help.cpp (fread result checks and sign-compare with off_t)
set_source_files_properties(hal/ax/jpeg_help.cpp PROPERTIES COMPILE_FLAGS
    "-Wno-unused-result -Wno-sign-compare")

# ── Link libraries ──────────────────────────────────────────
set(SYSTEM_LIBS
    pthread
    dl
)

set(OPENCV_LIBS
    opencv_highgui opencv_imgcodecs opencv_imgproc opencv_core
    ittnotify tegra_hal libjpeg-turbo libpng libtiff libwebp libopenjp2 zlib
    dl m rt
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
    avfilter
    swresample
)

set(THIRDPARTY_LIBS
    freetype
    curl
    uuid
    crypto
    z
    idn2
    ssl
)

target_link_libraries(ax_core PUBLIC
    ${SYSTEM_LIBS}
    ${OPENCV_LIBS}
    ${AX_LIBS}
    ${FFMPEG_LIBS}
    ${THIRDPARTY_LIBS}
    
)

add_subdirectory(rules)
