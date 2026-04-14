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
    include/
)

link_directories(
    3rdpart/ax/lib
    3rdpart/opencv/lib
    3rdpart/ffmpeg/lib
    3rdpart/x264/lib

)
aux_source_directory(src SRCS)
add_executable(ax_core main.cpp ${SRCS})

target_link_libraries(ax_core PUBLIC pthread opencv_imgcodecs opencv_core opencv_imgproc opencv_video opencv_videoio opencv_highgui ax_sys ax_ive ax_ives ax_ivps ax_engine ax_venc ax_vdec ax_interpreter avcodec avutil swscale avformat)

