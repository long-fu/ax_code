# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 需求

高性能视频流目标检测 — 在 AX670/AX620 边缘设备上实现 RTSP 拉流 → 目标检测 → SORT 跟踪 → RTMP 推流的实时视频分析管线。

## Build & Run

```bash
# Build (cross-compile for ARM64)
rm -rf build
cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build

# Run on device (LD_LIBRARY_PATH must include 3rdparty libs)
# ./run.sh
```

交叉编译器：`aarch64-linux-gnu-gcc/g++`。项目没有测试框架或 lint 配置。编译选项分 Debug (`-O0 -g -Wall`) 和 Release (`-O2 -Wall`)。

## Pipeline 架构

4 个 PipelineThread 通过消息传递串联：

```
[PreProcess] --kMsgPreprocData--> [InfProccess] --kMsgInfprocData--> [BusProcess] --kMsgBusprocData--> [EncProcess]
```

每个阶段是一个继承 `PipelineThread` 的类，通过 `SendMessage()` 和 `Process(msg_id, msg_data)` 通信。

### 各阶段职责

| 阶段 | 类文件 | 关键逻辑 |
|------|--------|----------|
| **PreProcess** | `core/inc/pre_process.hpp` | FFmpegDecoder 拉 RTSP 流 → VDEC 硬解码 → IVPS 硬缩放(640x640) → 拷贝到 host 内存 |
| **InfProccess** | `core/inc/inf_process.hpp` | Yolov5 NPU 推理 → 后处理（坐标映射到原始分辨率） |
| **BusProcess** | `core/inc/bus_process.hpp` | SORT 多目标跟踪 → 画框/画 ID（硬件映射 → draw → unmap） |
| **EncProcess** | `core/inc/enc_process.hpp` | VENC 硬编码 H264 → FFmpegEncoder 推 RTMP |

### 数据流消息类型（`core/inc/process_msg.h`）

- `kMsgVdecData` — `ImageData`（解码帧）
- `kMsgPreprocData` — `PreData`（image + 预处理后的 host buffer）
- `kMsgInfprocData` — `InfData`（image + detection::Object 列表）
- `kMsgBusprocData` — `BusData`（image，已绘制跟踪结果）

### 关键模块

- **axcore/**: 硬件抽象层 — VdecHelper（H264 硬解码）、IvpsHelper（图像缩放/CSC）、VencHelper（H264 硬编码）、FFmpegDecoder/Encoder、FrameData（帧生命周期管理，含 IVPS/VDEC/VENC/SYS 多种内存类型）
- **pipeline/**: 线程框架 — PipelineThread（消息循环基类）、PipelineThreadMgr（线程注册与查找）
- **tracker/sort/**: SORT 跟踪器，基于 KalmanFilter + Hungarian 匹配
- **core/src/**: 业务逻辑 — Yolov5 推理封装、BusiniessProcess（SORT + 绘制）
- **common/**: Logger（spdlog 封装，支持 async 和 crash signal handler）

### 硬件帧生命周期

`FrameData` 管理 AX SDK 视频帧的引用计数和释放。`shared_ptr<FrameData>` 通过自定义析构释放 IVPS/VDEC 硬件帧。内存类型包括 `MEM_ID_IVPS`、`MEM_ID_VDEC`、`MEM_ID_VENC`、`MEM_ID_SYS`。

### 配置

- `person.yaml` — 模型和跟踪参数（Yolov5 模型路径、anchor、tracker 参数）
- `person.axmodel` — 编译好的 NPU 模型
- RTSP 输入和 RTMP 输出 URL 硬编码在 `main.cpp` 中