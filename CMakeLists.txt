cmake_minimum_required(VERSION 3.10.0)
project(ax_core VERSION 0.1.0 LANGUAGES C CXX)

# set(CMAKE_C_COMPILER "aarch64-linux-gnu-gcc-9")
# set(CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++-9")

# set(CMAKE_C_COMPILER clang-15)
# set(CMAKE_CXX_COMPILER clang-15)

# add_compile_options(-std=c++11)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_COMPILER aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-none-linux-gnu-g++)

set(CMAKE_CXX_FLAGS_DEBUG "-fPIC -O0 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-fPIC -O2 -Wall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Cross-link target shared libs (e.g. yaml-cpp.so) whose C++ ABI resolves on device.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# Host and plugins share libax_runtime.so via $ORIGIN.
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_BUILD_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH "$ORIGIN")

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# DrawText backend: ON = bitmap OSD (default), OFF = FreeType.
option(AX_USE_OSD_TEXT "Use bitmap OSD fonts for DrawText" ON)

include_directories(
    third-party/ax/include
    third-party/opencv/include
    third-party/ffmpeg/include
    third-party/x264/include
    third-party/eigen3
    tracker/bytetrack
    common
    hal
    hal/ax
    hal/osd
    pipeline
    stages
    models
    models/detail
    third-party/include
    third-party/curl/include
    third-party/libuuid/include
    third-party/openssl/include
    third-party/zlib/include
    third-party/libidn2/include
    third-party/yaml-cpp/include
    third-party/spdlog/include
    utils
    plugin
    plugins/face_recognition
)

# FreeType headers only needed when DrawText uses FreeType.
if(NOT AX_USE_OSD_TEXT)
  include_directories(third-party/freetype/include/freetype)
endif()

link_directories(
    third-party/ax/lib
    third-party/ffmpeg/lib
    third-party/x264/lib
    third-party/nlohmann/
    third-party/curl/lib
    third-party/libuuid/lib
    third-party/openssl/lib64
    third-party/zlib/lib
    third-party/libidn2/lib
    # third-party/opencv-4.5.5/lib/opencv4/3rdparty
    third-party/opencv/lib
    third-party/openssl/lib64
    third-party/yaml-cpp/lib
    third-party/spdlog/lib
)

if(NOT AX_USE_OSD_TEXT)
  link_directories(third-party/freetype/lib)
endif()

aux_source_directory(hal HAL_SRCS)
# OSD-on: never compile FreeType into any target that might consume HAL_SRCS.
if(AX_USE_OSD_TEXT)
  list(FILTER HAL_SRCS EXCLUDE REGEX "freetype_helper\\.cpp$")
endif()
aux_source_directory(hal/ax AX_SRCS)
aux_source_directory(hal/osd OSD_SRCS)
aux_source_directory(pipeline PIP_SRCS)

aux_source_directory(tracker/bytetrack TRACKER_SRCS)
aux_source_directory(stages STAGES_SRCS)
aux_source_directory(models MODELS_SRCS)
aux_source_directory(common COMMON_SRCS)

# Runtime shared lib: common / models / frame+image / IVPS / JPEG / DrawText.
# Host and plugins both link this so curl_global_init and AX_ENGINE_* stay unique.
set(RUNTIME_SRCS
    ${COMMON_SRCS}
    ${MODELS_SRCS}
    plugin/host_services.cpp
    hal/image_data.cpp
    hal/frame_data.cpp
    hal/drawing.cpp
    hal/ax/jpeg_help.cpp
    hal/ax/ivps_helper.cpp
    hal/ax/hw_id_allocator.cpp
    ${OSD_SRCS}
)

# OSD-on: exclude FreeType helper. OSD-off: keep it so DrawText can use FreeType.
if(NOT AX_USE_OSD_TEXT)
  list(APPEND RUNTIME_SRCS hal/freetype_helper.cpp)
endif()

add_library(ax_runtime SHARED ${RUNTIME_SRCS})
set_target_properties(ax_runtime PROPERTIES
    OUTPUT_NAME ax_runtime
    POSITION_INDEPENDENT_CODE ON
)

if(AX_USE_OSD_TEXT)
  set(_AX_OSD_TEXT_DEF AX_USE_OSD_TEXT=1)
else()
  set(_AX_OSD_TEXT_DEF AX_USE_OSD_TEXT=0)
endif()
target_compile_definitions(ax_runtime PUBLIC ${_AX_OSD_TEXT_DEF})

# Pipeline HAL that stays in the executable (decode/encode path).
set(PIPE_HAL_SRCS
    hal/ffmpeg_decoder.cpp
    hal/ffmpeg_encoder.cpp
    hal/ax/vdec_helper.cpp
    hal/ax/venc_helper.cpp
)

add_executable(ax_core
    apps/main_rtsp.cpp
    plugin/postprocessor_chain.cpp
    plugin/scene_config.cpp
    plugin/plugin_loader.cpp
    ${PIPE_HAL_SRCS}
    ${STAGES_SRCS}
    ${PIP_SRCS}
    ${TRACKER_SRCS}
    pipeline/error.h
)
target_compile_definitions(ax_core PRIVATE ${_AX_OSD_TEXT_DEF})

# Face plugin .so: C ABI factory loaded from the scene configuration.
add_library(face_recognition SHARED
    plugins/face_recognition/face_plugin.cpp
    plugins/face_recognition/face_plugin_config.cpp
)

add_library(tracker_postprocessor SHARED
    postprocessors/tracker/tracker_processor.cpp
    ${TRACKER_SRCS}
)
set_source_files_properties(
    postprocessors/tracker/tracker_processor.cpp
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
set_target_properties(tracker_postprocessor PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "tracker"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/postprocessors"
    BUILD_RPATH "$ORIGIN/.."
    INSTALL_RPATH "$ORIGIN/.."
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(face_recognition PRIVATE FACE_PLUGIN_SHARED ${_AX_OSD_TEXT_DEF})
set_target_properties(face_recognition PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "face_recognition"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    BUILD_RPATH "$ORIGIN/.."
    INSTALL_RPATH "$ORIGIN/.."
    POSITION_INDEPENDENT_CODE ON
)

# Suppress warnings in jpeg_help.cpp (fread result checks and sign-compare with off_t)
set_source_files_properties(hal/ax/jpeg_help.cpp PROPERTIES COMPILE_FLAGS
    "-Wno-unused-result -Wno-sign-compare")

# ── Link libraries ──────────────────────────────────────────
set(SYSTEM_LIBS
    pthread
    dl
)

# set(OPENCV_LIBS
#     opencv_highgui opencv_imgcodecs opencv_imgproc opencv_core
#     ittnotify tegra_hal libjpeg-turbo libpng libtiff libwebp libopenjp2 zlib
#     dl m rt
# )

set(OPENCV_LIBS
    opencv_highgui opencv_imgcodecs opencv_imgproc opencv_core
        
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
    curl
    uuid
    crypto
    z
    idn2
    ssl
    yaml-cpp
)

# set(THIRDPARTY_LIBS
#     z
#     yaml-cpp
# )

# FreeType only when DrawText uses the FreeType backend.
if(NOT AX_USE_OSD_TEXT)
  list(APPEND THIRDPARTY_LIBS freetype)
endif()

target_link_libraries(ax_runtime PUBLIC
    ${SYSTEM_LIBS}
    ${OPENCV_LIBS}
    ${AX_LIBS}
    ${THIRDPARTY_LIBS}
    spdlog
)

target_link_libraries(face_recognition PRIVATE ax_runtime)
target_link_libraries(tracker_postprocessor PRIVATE yaml-cpp opencv_core)

target_link_libraries(ax_core PUBLIC
    ax_runtime
    ${SYSTEM_LIBS}
    ${OPENCV_LIBS}
    ${AX_LIBS}
    ${FFMPEG_LIBS}
    ${THIRDPARTY_LIBS}
    spdlog
)

include(CTest)
option(AX_BUILD_TESTS "Build ax_core tests" OFF)
if(AX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

configure_file(config.yaml "${CMAKE_BINARY_DIR}/config.yaml" COPYONLY)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/configs")
configure_file(configs/scenes/face_recognition.yaml
               "${CMAKE_BINARY_DIR}/configs/face_recognition.yaml" COPYONLY)

# add_subdirectory(rules)
