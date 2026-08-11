# Fix Code Review Issues Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix all Critical/Important/Minor findings from the 2026-08-11 `dev_pipeline` review, then merge into `main`.

**Architecture:** Keep joinable `std::thread` in `TaskNodeMgr`; fix shutdown order (signal → join threads → delete nodes); repair HAL resource/lifetime bugs that survived the rename.

**Tech Stack:** C++17, AX SDK HAL, FFmpeg, pipeline TaskScheduler/TaskNode

## Global Constraints

- Branch: `dev_pipeline` → merge to `main` after fixes
- Do not invent new APIs beyond what's needed for join/shutdown
- Prefer minimal, targeted fixes matching existing style
- Build verification before merge claim

---

### Task 1: Pipeline thread lifetime + shutdown

**Files:**
- Modify: `pipeline/task_node_mgr.h`, `pipeline/task_node_mgr.cpp`
- Modify: `pipeline/task_scheduler.cpp`, `pipeline/task_scheduler.h`
- Modify: `apps/main_rtsp.cpp`

- [ ] Store `std::thread` member; `CreateThread` joinable; `Join()` API
- [ ] `ReleaseThreads` set kExiting, join all, then delete
- [ ] Hot-path: non-fatal Process errors log+continue (not kError exit) for enqueue-style failures; keep fatal for Init
- [ ] `ExitPipeline`: `app.Exit()` then delete nodes
- [ ] SIGINT → `SignalWaitEnd()`; include `<csignal>` / `<atomic>`

### Task 2: Critical HAL / image

**Files:**
- Modify: `hal/ax/vdec_helper.cpp` — skip ReleaseChnFrame on Get failure; bounded StopDecode
- Modify: `hal/ffmpeg_encoder.cpp` — free AVCodecContext; fix WriteFrame EAGAIN loop
- Modify: `hal/image_data.cpp` — null-safe JpegDecode failure path

### Task 3: Important HAL / nodes

**Files:**
- Modify: `hal/ax/venc_helper.*`, `hal/ax/vdec_helper.*` — mutex for callback_
- Modify: `hal/ffmpeg_decoder.cpp` — close format on early return
- Modify: `nodes/inf_process.h` — check Engine Process return
- Modify: `nodes/bus_process.h` — Map check + DrawRect with item.rect
- Modify: `hal/ax/engine.cpp` — only DestroyHandle if valid
- Modify: `hal/ax/ivps_helper.cpp` — rollback + finite timeout
- Modify: `pipeline/resource.cpp` — unwind partial Init
- Modify: `tracker/sort/hungarian.cpp` — clear Assignment on early exit
- Modify: `common/rule_engine.cpp` — basic so_path check if easy

### Task 4: Minor

**Files:**
- Modify: `apps/main_img.cpp`, `hal/drawing.h`/`drawing.cpp`, `CMakeLists.txt`
- Fix wrong Wait log message

### Task 5: Verify + merge

- [ ] Build Release
- [ ] Commit on `dev_pipeline`
- [ ] Merge into `main`
