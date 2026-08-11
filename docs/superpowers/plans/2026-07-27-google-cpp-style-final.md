# Google C++ Style Final Compliance Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete all remaining Google C++ Style compliance fixes: class names, enum values, `#define` to `constexpr`, header guards, `using namespace std` removal, and typedef naming.

**Architecture:** Mechanical renames and replacements with sed/edits. Each task produces compilable code. Build verification after each commit.

**Tech Stack:** C++17, CMake, aarch64-linux-gnu-gcc

## Global Constraints

- Cross-compiler: `aarch64-linux-gnu-gcc/g++`
- CMake: `-G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release -S . -B build`
- All renames must be tracked via `git mv` (not plain `mv`)
- Every `#include` directive must be updated after file renames
- Build must pass (0 errors) after each task

---

### Task 1: Class name fixes — BYTETracker, Bbox, ReqResourceID, ImgSImg

**Files:**
- Modify: `tracker/bytetrack/byte_tracker.h` — `BYTETracker` → `ByteTracker`
- Modify: `tracker/bytetrack/byte_tracker.cpp` — all `BYTETracker::` references
- Modify: `tracker/bytetrack/utils.cpp` — all `BYTETracker::` references
- Modify: `tracker/sort/datatrans.h` — `Bbox` → `BoundingBox`
- Modify: `tracker/sort/utils.h` — `Bbox` references in function signatures
- Modify: `tracker/sort/utils.cpp` — `Bbox` references in definitions
- Modify: `axcore/include/req_sys_id.hpp` — `ReqResourceID` → `ReqResourceId`
- Modify: `img_s_img/img_s_img.hpp` — `ImgSImg` → `ImgSimg`

**Interfaces:**
- Produces: `ByteTracker` class, `BoundingBox` struct, `ReqResourceId` class, `ImgSimg` class

- [ ] **Step 1: Rename BYTETracker → ByteTracker**

```bash
cd /home/haoshuai/code/ax_core

# byte_tracker.h
sed -i 's/class BYTETracker/class ByteTracker/g' tracker/bytetrack/byte_tracker.h
sed -i 's/BYTETracker(/ByteTracker(/g' tracker/bytetrack/byte_tracker.h

# byte_tracker.cpp
sed -i 's/BYTETracker::/ByteTracker::/g' tracker/bytetrack/byte_tracker.cpp

# utils.cpp
sed -i 's/BYTETracker::/ByteTracker::/g' tracker/bytetrack/utils.cpp
```

- [ ] **Step 2: Rename Bbox → BoundingBox**

```bash
cd /home/haoshuai/code/ax_core

# datatrans.h
sed -i 's/struct Bbox/struct BoundingBox/g' tracker/sort/datatrans.h
sed -i 's/TrackingBox(Bbox obj)/TrackingBox(BoundingBox obj)/g' tracker/sort/datatrans.h

# utils.h
sed -i 's/std::vector<Bbox>/std::vector<BoundingBox>/g' tracker/sort/utils.h

# utils.cpp
sed -i 's/<Bbox>/<BoundingBox>/g' tracker/sort/utils.cpp
sed -i 's/GetDetectResults/GetDetectResults/g' tracker/sort/utils.cpp
```

- [ ] **Step 3: Rename ReqResourceID → ReqResourceId**

```bash
cd /home/haoshuai/code/ax_core

sed -i 's/class ReqResourceID/class ReqResourceId/g' axcore/include/req_sys_id.hpp
sed -i 's/ReqResourceID \*self/ReqResourceId *self/g' axcore/include/req_sys_id.hpp
```

- [ ] **Step 4: Rename ImgSImg → ImgSimg**

```bash
cd /home/haoshuai/code/ax_core

sed -i 's/class ImgSImg/class ImgSimg/g' img_s_img/img_s_img.hpp
sed -i 's/ImgSImg(/ImgSimg(/g' img_s_img/img_s_img.hpp
```

- [ ] **Step 5: Build and verify**

```bash
cd /home/haoshuai/code/ax_core
rm -rf build && cmake -G "Unix Makefiles" -D CMAKE_BUILD_TYPE=Release -S . -B build && cmake --build build 2>&1 | grep -E "error:" | grep -v "collect2\|ld returned"
```

Expected: 0 compilation errors (only pre-existing linker error for `-lidn2`)

- [ ] **Step 6: Commit**

```bash
cd /home/haoshuai/code/ax_core
git add -A
git commit -m "style: rename BYTETracker, Bbox, ReqResourceID, ImgSImg to PascalCase per Google style"
```

---

### Task 2: Enum value naming — kPascalCase

**Files:**
- Modify: `pipeline/include/pipeline_thread_mgr.h` — PipelineThreadStatus values
- Modify: `pipeline/src/pipeline.cpp` — references to THREAD_* values
- Modify: `pipeline/src/pipeline_thread_mgr.cpp` — references to THREAD_* values
- Modify: `axcore/include/frame_data.hpp` — MemId values
- Modify: `axcore/include/ffmpeg_decoder.hpp` — StreamType and DecodeStatus values
- Modify: `axcore/src/vdec_helper.cpp` — references to DECODE_* values
- Modify: `axcore/include/req_sys_id.hpp` — ResourceType values

**Interfaces:**
- Produces: `kThreadReady`, `kMemIdVdec`, `kStreamVideo`, `kDecodeError`, `kResIvpsId`

- [ ] **Step 1: PipelineThreadStatus enum values**

```bash
cd /home/haoshuai/code/ax_core

# pipeline_thread_mgr.h
sed -i \
  -e 's/\bTHREAD_READY\b/kThreadReady/g' \
  -e 's/\bTHREAD_RUNNING\b/kThreadRunning/g' \
  -e 's/\bTHREAD_EXITING\b/kThreadExiting/g' \
  -e 's/\bTHREAD_EXITED\b/kThreadExited/g' \
  -e 's/\bTHREAD_ERROR\b/kThreadError/g' \
  pipeline/include/pipeline_thread_mgr.h

# pipeline.cpp
sed -i \
  -e 's/\bTHREAD_READY\b/kThreadReady/g' \
  -e 's/\bTHREAD_RUNNING\b/kThreadRunning/g' \
  -e 's/\bTHREAD_EXITING\b/kThreadExiting/g' \
  -e 's/\bTHREAD_EXITED\b/kThreadExited/g' \
  -e 's/\bTHREAD_ERROR\b/kThreadError/g' \
  pipeline/src/pipeline.cpp pipeline/src/pipeline_thread_mgr.cpp
```

- [ ] **Step 2: MemId enum values**

```bash
cd /home/haoshuai/code/ax_core

sed -i \
  -e 's/\bMEM_ID_MIN\b/kMemIdMin/g' \
  -e 's/\bMEM_ID_VDEC\b/kMemIdVdec/g' \
  -e 's/\bMEM_ID_VENC\b/kMemIdVenc/g' \
  -e 's/\bMEM_ID_IVPS\b/kMemIdIvps/g' \
  -e 's/\bMEM_ID_IVES\b/kMemIdIves/g' \
  -e 's/\bMEM_ID_JENC\b/kMemIdJenc/g' \
  -e 's/\bMEM_ID_JDEC\b/kMemIdJdec/g' \
  -e 's/\bMEM_ID_NPU\b/kMemIdNpu/g' \
  -e 's/\bMEM_ID_SYS\b/kMemIdSys/g' \
  -e 's/\bMEM_ID_MAX\b/kMemIdMax/g' \
  axcore/include/frame_data.hpp

# Find all references to MEM_ID_* in other files
grep -rn "MEM_ID_" --include="*.cpp" --include="*.hpp" --include="*.h" \
  $(find . -not -path "*/3rdpart/*" -not -path "*/build/*" -not -path "*/spdlog/*" -type f) \
  | grep -v "frame_data.hpp" | grep -v "//\|#\|/\*"

# Fix each file found
for f in axcore/src/frame_data.cpp axcore/src/vdec_helper.cpp axcore/src/ivps_helper.cpp; do
  sed -i \
    -e 's/\bMEM_ID_MIN\b/kMemIdMin/g' \
    -e 's/\bMEM_ID_VDEC\b/kMemIdVdec/g' \
    -e 's/\bMEM_ID_VENC\b/kMemIdVenc/g' \
    -e 's/\bMEM_ID_IVPS\b/kMemIdIvps/g' \
    -e 's/\bMEM_ID_IVES\b/kMemIdIves/g' \
    -e 's/\bMEM_ID_JENC\b/kMemIdJenc/g' \
    -e 's/\bMEM_ID_JDEC\b/kMemIdJdec/g' \
    -e 's/\bMEM_ID_NPU\b/kMemIdNpu/g' \
    -e 's/\bMEM_ID_SYS\b/kMemIdSys/g' \
    -e 's/\bMEM_ID_MAX\b/kMemIdMax/g' \
    "$f"
done
```

- [ ] **Step 3: StreamType enum values**

```bash
cd /home/haoshuai/code/ax_core

sed -i \
  -e 's/\bSTREAM_VIDEO\b/kStreamVideo/g' \
  -e 's/\bSTREAM_RTSP\b/kStreamRtsp/g' \
  axcore/include/ffmpeg_decoder.hpp

# Find references
grep -rn "STREAM_VIDEO\|STREAM_RTSP" --include="*.cpp" --include="*.hpp" \
  $(find . -not -path "*/3rdpart/*" -not -path "*/build/*" -not -path "*/spdlog/*" -type f) \
  | grep -v "ffmpeg_decoder.hpp" | grep -v "//"
```

- [ ] **Step 4: DecodeStatus enum values**

```bash
cd /home/haoshuai/code/ax_core

sed -i \
  -e 's/\bDECODE_ERROR\b/kDecodeError/g' \
  -e 's/\bDECODE_UNINIT\b/kDecodeUninit/g' \
  -e 's/\bDECODE_READY\b/kDecodeReady/g' \
  -e 's/\bDECODE_START\b/kDecodeStart/g' \
  -e 's/\bDECODE_FFMPEG_FINISHED\b/kDecodeFfmpegFinished/g' \
  -e 's/\bDECODE_DVPP_FINISHED\b/kDecodeDvppFinished/g' \
  -e 's/\bDECODE_FINISHED\b/kDecodeFinished/g' \
  axcore/include/ffmpeg_decoder.hpp

# Find references in other files
grep -rn "DECODE_" --include="*.cpp" --include="*.hpp" \
  $(find . -not -path "*/3rdpart/*" -not -path "*/build/*" -not -path "*/spdlog/*" -type f) \
  | grep -v "ffmpeg_decoder.hpp" | grep -v "//"
```

- [ ] **Step 5: ResourceType enum values**

```bash
cd /home/haoshuai/code/ax_core

sed -i \
  -e 's/\bRES_IVPS_ID\b/kResIvpsId/g' \
  -e 's/\bRES_VDEC_ID\b/kResVdecId/g' \
  -e 's/\bRES_VENC_ID\b/kResVencId/g' \
  axcore/include/req_sys_id.hpp
```

- [ ] **Step 6: Build and verify**

```bash
cd /home/haoshuai/code/ax_core
cmake --build build 2>&1 | grep -E "error:" | grep -v "collect2\|ld returned"
```

Expected: 0 compilation errors

- [ ] **Step 7: Commit**

```bash
cd /home/haoshuai/code/ax_core
git add -A
git commit -m "style: rename enum values to kPascalCase per Google C++ Style"
```

---

### Task 3: #define constants to constexpr

**Files:**
- Modify: `axcore/include/ffmpeg_decoder.hpp` — 5 `#define` constants → `constexpr`
- Modify: `axcore/src/ffmpeg_decoder.cpp` — update references
- Modify: `pipeline/include/pipeline_thread.h` — `#define INVALID_INSTANCE_ID` → `constexpr`

**Interfaces:**
- Produces: `constexpr int kInvalidChannelId = -1`, `constexpr int kInvalidStreamFormat = -1`, `constexpr int kVideoChannelMax = 256`, `constexpr char* kRtspTransportUdp = "udp"`, `constexpr char* kRtspTransportTcp = "tcp"`, `constexpr int kInvalidInstanceId = -1`

- [ ] **Step 1: Fix ffmpeg_decoder.hpp constants**

Replace the `#define` lines in `axcore/include/ffmpeg_decoder.hpp`:

Current:
```cpp
#define INVALID_CHANNEL_ID (-1)
#define INVALID_STREAM_FORMAT (-1)
#define VIDEO_CHANNEL_MAX (256)
#define RTSP_TRANSPORT_UDP "udp"
#define RTSP_TRANSPORT_TCP "tcp"
```

New:
```cpp
constexpr int kInvalidChannelId = -1;
constexpr int kInvalidStreamFormat = -1;
constexpr int kVideoChannelMax = 256;
constexpr const char* kRtspTransportUdp = "udp";
constexpr const char* kRtspTransportTcp = "tcp";
```

- [ ] **Step 2: Update references in ffmpeg_decoder.cpp**

```bash
cd /home/haoshuai/code/ax_core

sed -i \
  -e 's/INVALID_CHANNEL_ID/kInvalidChannelId/g' \
  -e 's/INVALID_STREAM_FORMAT/kInvalidStreamFormat/g' \
  -e 's/VIDEO_CHANNEL_MAX/kVideoChannelMax/g' \
  -e 's/RTSP_TRANSPORT_UDP/kRtspTransportUdp/g' \
  -e 's/RTSP_TRANSPORT_TCP/kRtspTransportTcp/g' \
  axcore/src/ffmpeg_decoder.cpp
```

- [ ] **Step 3: Fix INVALID_INSTANCE_ID in pipeline_thread.h**

Current:
```cpp
#define INVALID_INSTANCE_ID (-1)
```

New:
```cpp
constexpr int kInvalidInstanceId = -1;
```

- [ ] **Step 4: Update references to INVALID_INSTANCE_ID**

```bash
cd /home/haoshuai/code/ax_core

sed -i 's/INVALID_INSTANCE_ID/kInvalidInstanceId/g' \
  pipeline/include/pipeline_thread.h \
  pipeline/include/pipeline.h \
  pipeline/src/pipeline.cpp \
  pipeline/src/pipeline_thread.cpp \
  pipeline/src/pipeline_thread_mgr.cpp
```

- [ ] **Step 5: Build and verify**

```bash
cd /home/haoshuai/code/ax_core
cmake --build build 2>&1 | grep -E "error:" | grep -v "collect2\|ld returned"
```

Expected: 0 compilation errors

- [ ] **Step 6: Commit**

```bash
cd /home/haoshuai/code/ax_core
git add -A
git commit -m "style: replace #define constants with constexpr kPascalCase"
```

---

### Task 4: Header guard fixes — #pragma once

**Files:**
- Modify: `tracker/sort/sort_track.h` — replace `#ifndef __TRACK_H__` with `#pragma once`
- Modify: `axcore/include/freetype_helper.h` — replace `#ifndef __FREETYPE_HELPER__` with `#pragma once`
- Modify: `axcore/include/file.hpp` — replace `#ifndef __FILE_H__` with `#pragma once`
- Modify: `axcore/include/detection.hpp` — replace `#ifndef __detection__` with `#pragma once`
- Modify: `tracker/sort/datatrans.h` — replace `#ifndef __DATATRANS_H` with `#pragma once`
- Modify: `axcore/include/io.hpp` — replace `#ifndef __IO__` with `#pragma once`
- Modify: `axcore/include/drawing.h` — replace `#ifndef __DRAWING__` with `#pragma once`
- Modify: `axcore/include/ffmpeg_encoder.hpp` — replace `#ifndef __FFMPEGENCODER__` with `#pragma once`

- [ ] **Step 1: Replace all 8 header guards**

For each file, replace the `#ifndef`/`#define`/`#endif` guard with `#pragma once`.

Example (sort_track.h):
```diff
-#ifndef __TRACK_H__
-#define __TRACK_H__
+#pragma once
```

And remove the `#endif` at the end of each file.

- [ ] **Step 2: Build and verify**

```bash
cd /home/haoshuai/code/ax_core
cmake --build build 2>&1 | grep -E "error:" | grep -v "collect2\|ld returned"
```

Expected: 0 compilation errors

- [ ] **Step 3: Commit**

```bash
cd /home/haoshuai/code/ax_core
git add -A
git commit -m "style: replace #ifndef guards with #pragma once"
```

---

### Task 5: Remove using namespace std from headers

**Files:**
- Modify: `tracker/sort/sort_track.h`
- Modify: `tracker/sort/hungarian.h`
- Modify: `tracker/sort/utils.h`
- Modify: `tracker/sort/kalman_tracker.h`
- Modify: `tracker/sort/datatrans.h`

- [ ] **Step 1: Remove `using namespace std;` and `using namespace cv;`**

```bash
cd /home/haoshuai/code/ax_core

for f in tracker/sort/sort_track.h tracker/sort/hungarian.h tracker/sort/utils.h \
         tracker/sort/kalman_tracker.h tracker/sort/datatrans.h; do
  sed -i '/^using namespace std;/d' "$f"
  sed -i '/^using namespace cv;/d' "$f"
done
```

- [ ] **Step 2: Add `std::` and `cv::` prefixes**

For each file, add `std::` and `cv::` prefixes to all usage of standard types.

For `sort_track.h`:
```cpp
// Add these prefixes
std::vector<KalmanTracker> trackers;
std::vector<Rect_<float>> predicted_boxes;
std::vector<std::vector<double>> iou_matrix;
std::vector<int> assignment;
std::set<int> unmatched_detections;
std::set<int> unmatched_trajectories;
std::set<int> all_items;
std::set<int> matched_items;
std::vector<cv::Point> matched_pairs;
std::vector<TrackingBox> frame_tracking_result;
```

For `kalman_tracker.h`:
```cpp
std::vector<StateType> history_;
std::vector<float> kps_in_pic;
std::vector<cv::Point2f> kps_in_robot;
```

- [ ] **Step 3: Build and verify**

```bash
cd /home/haoshuai/code/ax_core
cmake --build build 2>&1 | grep -E "error:" | grep -v "collect2\|ld returned"
```

Expected: 0 compilation errors

- [ ] **Step 4: Commit**

```bash
cd /home/haoshuai/code/ax_core
git add -A
git commit -m "style: remove using namespace std from tracker headers"
```

---

### Task 6: Typedef naming — CallBack → Callback

**Files:**
- Modify: `axcore/include/venc_helper.hpp` — `VencProcessCallBack` → `VencProcessCallback`
- Modify: `axcore/include/vdec_helper.hpp` — `VdecProcessCallBack` → `VdecProcessCallback`
- Modify: `axcore/include/ffmpeg_decoder.hpp` — `FrameProcessCallBack` → `FrameProcessCallback`
- Modify: `axcore/src/venc_helper.cpp` — references
- Modify: `axcore/src/vdec_helper.cpp` — references
- Modify: `axcore/src/ffmpeg_decoder.cpp` — references
- Modify: `core/inc/enc_process.hpp` — references
- Modify: `core/inc/pre_process.hpp` — references

- [ ] **Step 1: Rename all CallBack → Callback**

```bash
cd /home/haoshuai/code/ax_core

sed -i 's/VencProcessCallBack/VencProcessCallback/g' \
  axcore/include/venc_helper.hpp axcore/src/venc_helper.cpp \
  core/inc/enc_process.hpp

sed -i 's/VdecProcessCallBack/VdecProcessCallback/g' \
  axcore/include/vdec_helper.hpp axcore/src/vdec_helper.cpp \
  core/inc/pre_process.hpp

sed -i 's/FrameProcessCallBack/FrameProcessCallback/g' \
  axcore/include/ffmpeg_decoder.hpp axcore/src/ffmpeg_decoder.cpp

# Also fix any CallBackFunc -> CallbackFunc
sed -i 's/CallBackFunc/CallbackFunc/g' \
  core/inc/pre_process.hpp core/inc/enc_process.hpp
```

- [ ] **Step 2: Build and verify**

```bash
cd /home/haoshuai/code/ax_core
cmake --build build 2>&1 | grep -E "error:" | grep -v "collect2\|ld returned"
```

Expected: 0 compilation errors

- [ ] **Step 3: Commit**

```bash
cd /home/haoshuai/code/ax_core
git add -A
git commit -m "style: rename CallBack to Callback per Google C++ Style"
```

---

### Task 7: CLAUDE.md path and reference updates

**Files:**
- Modify: `CLAUDE.md` — update PascalCase file paths to lowercase

- [ ] **Step 1: Update all file paths in CLAUDE.md**

```bash
cd /home/haoshuai/code/ax_core

sed -i \
  -e 's|core/inc/PreProcess.hpp|core/inc/pre_process.hpp|g' \
  -e 's|core/inc/InfProccess.hpp|core/inc/inf_process.hpp|g' \
  -e 's|core/inc/BusProcess.hpp|core/inc/bus_process.hpp|g' \
  -e 's|core/inc/EncProcess.hpp|core/inc/enc_process.hpp|g' \
  -e 's|core/inc/ProcessMsg.h|core/inc/process_msg.h|g' \
  CLAUDE.md
```

- [ ] **Step 2: Commit**

```bash
cd /home/haoshuai/code/ax_core
git add CLAUDE.md
git commit -m "docs: update CLAUDE.md file paths to lowercase"
```

---

### Task 8: Push all changes

- [ ] **Step 1: Push to remote**

```bash
cd /home/haoshuai/code/ax_core
git push origin dev_img
```