cmake_minimum_required(VERSION 3.10.0)
project(ax_core VERSION 0.1.0 LANGUAGES C CXX)


set(CMAKE_C_COMPILER "aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++")

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
    include
    tracker/sort
    common
    axcore/include
    3rdpart/freetype/include/freetype
    pipeline/include
    core/inc
)

link_directories(
    3rdpart/ax/lib
    3rdpart/opencv/lib
    3rdpart/ffmpeg/lib
    3rdpart/x264/lib
    3rdpart/spdlog/lib
    3rdpart/freetype/lib
)
aux_source_directory(axcore/src AX_SRCS)
aux_source_directory(pipeline/src PIP_SRCS)
aux_source_directory(tracker/sort TRACKER_SRCS)
aux_source_directory(core/src CORE_SRCS)
aux_source_directory(common COMMON_SRCS)
add_executable(ax_core main.cpp ${AX_SRCS} ${CORE_SRCS} ${TRACKER_SRCS} ${PIP_SRCS} ${COMMON_SRCS})

target_link_libraries(ax_core PUBLIC pthread opencv_imgcodecs opencv_core opencv_imgproc opencv_video opencv_videoio opencv_highgui ax_sys ax_ive ax_ives ax_ivps ax_engine ax_venc ax_vdec ax_interpreter avcodec avutil swscale avformat spdlog freetype dl)

add_subdirectory(rules)
