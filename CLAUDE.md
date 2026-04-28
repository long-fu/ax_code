# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 需求

高性能视频流目标检测


## Code Style

Follow Google C++ Style Guide with C++17 standard. 

## Build

This is a CMake-based C++ project cross-compiled for AX (ARM64) edge AI devices.

```bash
# Build (from project root)
mkdir -p build && cd build && cmake .. && make -j$(nproc)

```

The project links against AX SDK libraries (ax_sys, ax_ive, ax_ivps, ax_engine, ax_venc, ax_vdec), OpenCV, FFmpeg, x264, and spdlog.

## Architecture Overview

This is an **AI video analytics pipeline** for AX670/AX620 edge devices. The pipeline flow:

```
RTSP Stream → FFmpegDecoder → VDEC (HW) → IVPS (resize/CSC) →
Yolov5 Inference → SORT Tracker → Drawing → VENC (HW) → RTMP Output
```

### Core Components

- **axcore/**: Media handling - FFmpegDecoder/Encoder, VdecHelper/VencHelper (AX hardware), IvpsHelper (image preprocessing), ImageData, detection (Yolov5), drawing
- **pipeline/**: Thread pool and pipeline management (PipelineThread, PipelineThreadMgr)
- **tracker/sort**: SORT multi-object tracker implementation
- **common/**: Shared utilities

### Main Pipeline (main.cpp)

The application runs 3 parallel threads:
1. **FFmpegDecodeCallBack**: Pulls RTSP stream and feeds to VDEC
2. **ReadImageDataCallBack**: Receives decoded frames from VDEC, runs IVPS preprocessing (resize to 640x640, YUV CSC), pushes to inference queue
3. **InferCallBack**: Runs Yolov5 inference on preprocessed frames, performs SORT tracking, draws bounding boxes/IDs, encodes with VENC and pushes to RTMP output

### Key Configuration

- Input: RTSP stream (hardcoded in main.cpp: `rtsp://123:123@22.10.54.60:8555/live21`)
- Output: RTMP stream (hardcoded: `rtmp://123:123@22.10.57.15/mylive/live`)
- Inference input: 640x640 NV12
- Object detection: Yolov5 for person detection (label == 1)