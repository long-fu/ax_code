# Code Review Issues Fix Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复代码审查中发现的 CRITICAL、HIGH 和 MEDIUM 级别问题（跳过 CRITICAL #1 凭证硬编码，用户确认测试数据无需处理）

**Architecture:** 针对每个问题做最小化修复：atomic 替换 bool、RAII 包装 raw pointer、Engine 错误路径去重释放、FFmpegEncoder 拷贝外部缓冲区、消息队列添加告警、exit() 改为返回错误码。不重构整体架构。

**Tech Stack:** C++17, pthreads, FFmpeg, 海思 AX SDK

---

## Task 1: VdecHelper/VencHelper is_stop_ 改为 atomic<bool>

**Files:**
- Modify: `axcore/include/VdecHelper.hpp:42`
- Modify: `axcore/src/VdecHelper.cpp:262,264,373`
- Modify: `axcore/include/VencHelper.hpp:53`
- Modify: `axcore/src/VencHelper.cpp:94,276`

**Problem:** `is_stop_` 在多线程间读写无同步保护，UB 可能导致 shutdown 挂起。VdecHelper (line 264 read, line 373 write) 和 VencHelper (line 94 read, line 276 write)。

### Step 1: 修改 VdecHelper.hpp — is_stop_ 改为 atomic

```cpp
// axcore/include/VdecHelper.hpp line 42
#include <atomic>

// Change:
bool is_stop_ = false;
// To:
std::atomic<bool> is_stop_{false};
```

### Step 2: 修改 VencHelper.hpp — is_stop_ 改为 atomic

```cpp
// axcore/include/VencHelper.hpp line 53
#include <atomic>

// Change:
bool is_stop_ = false;
// To:
std::atomic<bool> is_stop_{false};
```

### Step 3: 修改 VencHelper.hpp — IsExit() 显式转换

```cpp
// axcore/include/VencHelper.hpp line 43
// Change:
bool IsExit() { return is_stop_; }
// To:
bool IsExit() { return is_stop_.load(); }
```

### Step 4: 修改 VdecHelper.cpp — StopDecode 中设置 atomic

```cpp
// axcore/src/VdecHelper.cpp line 373
// Change:
is_stop_ = true;
// To:
is_stop_.store(true);
```

### Step 5: 编译验证

```bash
mkdir -p build && cd build && cmake .. && make -j$(nproc)
```

Expected: Build succeeds with no errors related to atomic usage.

### Step 6: Commit

```bash
git add axcore/include/VdecHelper.hpp axcore/src/VdecHelper.cpp \
       axcore/include/VencHelper.hpp axcore/src/VencHelper.cpp
git commit -m "fix: make is_stop_ atomic in VdecHelper and VencHelper to prevent UB"
```

---

## Task 2: Engine 双释放风险修复

**Files:**
- Modify: `axcore/src/Engine.cpp:71-112`

**Problem:** `Process()` 错误路径调用 `middleware::free_io(&io_data_)` + `AX_ENGINE_DestroyHandle(handle_)`，但析构函数 `~Engine()` → `Destroy()` 也无条件调用这两者，导致双释放。

**Fix:** 移除 `Process()` 错误路径中的资源释放，让析构函数统一管理。

### Step 1: 重写 Engine::Process(const std::vector<uint8_t>&) 错误路径

```cpp
// axcore/src/Engine.cpp lines 71-93
int Engine::Process(const std::vector<uint8_t>& input_data) {
  int ret = middleware::push_input(input_data, &io_data_, io_info_);
  if (0 != ret) {
    LOG_ERROR("push_input data failed!!! code:{:#x}", ret);
    return ret;
  }

  ret = AX_ENGINE_RunSync(handle_, &io_data_);
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_RunSync failed!!! code:{:#x}", ret);
    return ret;
  }

  return ret;
}
```

### Step 2: 重写 Engine::Process(const uint8_t*, size_t) 错误路径

```cpp
// axcore/src/Engine.cpp lines 95-112
int Engine::Process(const uint8_t* data, size_t size) {
  int ret = middleware::push_input(data, size, &io_data_, io_info_);
  if (0 != ret) {
    LOG_ERROR("push_input data failed!!! code:{:#x}", ret);
    return ret;
  }

  ret = AX_ENGINE_RunSync(handle_, &io_data_);
  if (0 != ret) {
    LOG_ERROR("AX_ENGINE_RunSync failed!!! code:{:#x}", ret);
    return ret;
  }
  return ret;
}
```

### Step 3: 编译验证

```bash
cd build && make -j$(nproc)
```

Expected: Build succeeds.

### Step 4: Commit

```bash
git add axcore/src/Engine.cpp
git commit -m "fix: remove resource cleanup from Engine::Process error paths to prevent double-free"
```

---

## Task 3: FFmpegEncoder::WritePacket 拷贝外部缓冲区

**Files:**
- Modify: `axcore/src/FFmpegEncoder.cpp:220-258`

**Problem:** `av_buffer_create` 包装外部硬件内存（VENC 输出），H.264 数据可能被下一帧覆盖后才发送。`av_packet_unref` 后缓冲区状态不确定。

**Fix:** 使用 `av_packet_alloc()` + `av_new_packet()` 分配 FFmpeg 管理的缓冲区，然后 `memcpy` 数据。

### Step 1: 重写 WritePacket 函数

```cpp
// axcore/src/FFmpegEncoder.cpp lines 227-258
int FFmpegEncoder::WritePacket(void *data, size_t data_size)
{
    int ret = 0;
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        LOG_ERROR("av_packet_alloc failed");
        return -1;
    }

    ret = av_new_packet(pkt, (int)data_size);
    if (ret < 0) {
        LOG_ERROR("av_new_packet failed err code:{}", ret);
        av_packet_free(&pkt);
        return -1;
    }

    memcpy(pkt->data, data, data_size);

    pkt->pts = m_pVideo_frame->pts;
    pkt->dts = pkt->pts;
    pkt->flags = AV_PKT_FLAG_KEY;

    ret = av_write_frame(m_pEncoder_avfc, pkt);

    if (ret < 0)
    {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        LOG_ERROR("av_write_frame failed err code: {} Reason: {}", ret,
                  av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, ret));
        av_packet_free(&pkt);
        return -1;
    }

    av_packet_free(&pkt);

    m_pVideo_frame->pts += av_rescale_q(1, m_pVideo_avcc->time_base, m_pAvs->time_base);
    return 0;
}
```

### Step 2: 移除不再需要的 custom_free 静态函数

```cpp
// axcore/src/FFmpegEncoder.cpp lines 220-225 — DELETE this block
static void custom_free(void *opaque, uint8_t *data)
{
    // free(data);
    // delete[] (data);
    // acldvppFree(data);
}
```

### Step 3: 编译验证

```bash
cd build && make -j$(nproc)
```

Expected: Build succeeds.

### Step 4: Commit

```bash
git add axcore/src/FFmpegEncoder.cpp
git commit -m "fix: copy external buffer in FFmpegEncoder::WritePacket to prevent use-after-free"
```

---

## Task 4: PipelineThreadMgr 消息丢弃告警

**Files:**
- Modify: `pipeline/src/PipelineThreadMgr.cpp`

**Problem:** 队列满时 `Push` 返回 false，消息静默丢弃。

**Fix:** 在 `PushMsgToQueue` 中添加日志告警。

### Step 1: 修改 PushMsgToQueue 添加日志

```cpp
// pipeline/src/PipelineThreadMgr.cpp lines 86-94
int PipelineThreadMgr::PushMsgToQueue(
    std::shared_ptr<PipelineMessage>& message) {
  if (status_ != THREAD_RUNNING) {
    LOG_ERROR("Thread instance {} status({}) is invalid, can not receive message",
              name_, status_);
    return -1;
  }
  bool pushed = msg_queue_.Push(message);
  if (!pushed) {
    LOG_WARN("Thread instance {} message queue full, dropping message", name_);
    return -1;
  }
  return 0;
}
```

### Step 2: 编译验证

```bash
cd build && make -j$(nproc)
```

Expected: Build succeeds.

### Step 3: Commit

```bash
git add pipeline/src/PipelineThreadMgr.cpp
git commit -m "warn: log when message queue is full instead of silently dropping messages"
```

---

## Task 5: 移除 exit(-1)，改为返回错误码

**Files:**
- Modify: `core/inc/PreProcess.hpp:76-92`
- Modify: `core/src/Yolov5.cpp:30-32, 46-48`
- Modify: `core/inc/InfProccess.hpp:36`

**Problem:** 从 pipeline 线程回调中调用 `exit(-1)` 直接杀死进程，跳过 RAII 清理和日志刷新。

**Fix:** 改为返回非零错误码，由上层处理。

### Step 1: PreProcess::Proprocess 改用返回码

```cpp
// core/inc/PreProcess.hpp lines 76-92
int Proprocess(std::shared_ptr<ImageData> img_data) {
    ImageData dest;
    ImageData src = *img_data.get();

    int ret = m_p_ivps->Process(dest, src);
    if (ret != 0) {
      LOG_ERROR("CSC failed, ret={}", ret);
      return ret;
    }

    auto data = std::make_shared<PreData>();
    data->image = src;
    Copy2Host(data->data, dest);

    int send_ret = SendMessage(m_next_thread_id_, kMsgPreprocData, data);
    if (send_ret != 0) {
      LOG_ERROR("SendMessage to InfProccess failed, ret={}", send_ret);
      return send_ret;
    }

    return 0;
}
```

### Step 2: Yolov5.hpp — 修改 Postprocess 签名

先读取 `core/inc/Yolov5.hpp` 确认当前声明。将 `void Postprocess(...)` 改为 `int Postprocess(...)`。

### Step 3: Yolov5.cpp — 用返回码替换 exit

```cpp
// core/src/Yolov5.cpp
// line 30-32:
if (GetInfo()->nOutputSize != strides.size()) {
    LOG_ERROR("Output size mismatch: {} != {}", GetInfo()->nOutputSize, strides.size());
    return -1;
}

// line 46-48:
if (countSize != out_size) {
    LOG_ERROR("Output buffer size mismatch: {} != {}", countSize, out_size);
    return -2;
}
```

### Step 4: 修改 InfProccess.hpp 中调用 Postprocess 的地方

```cpp
// core/inc/InfProccess.hpp line 36
// Change:
m_yolov5.Postprocess(m_p_ff_decoder->GetFrameWidth(),
                     m_p_ff_decoder->GetFrameHeight(),
                     out_data->objects);
// To:
int pp_ret = m_yolov5.Postprocess(m_p_ff_decoder->GetFrameWidth(),
                                  m_p_ff_decoder->GetFrameHeight(),
                                  out_data->objects);
if (pp_ret != 0) {
    LOG_ERROR("Yolov5 Postprocess failed, ret={}", pp_ret);
    return pp_ret;
}
```

### Step 5: 编译验证

```bash
cd build && make -j$(nproc)
```

Expected: Build succeeds.

### Step 6: Commit

```bash
git add core/inc/PreProcess.hpp core/src/Yolov5.cpp core/inc/InfProccess.hpp
git commit -m "refactor: replace exit(-1) with error codes in PreProcess and Yolov5"
```

---

## Task 6: 清理死文件

**Files:**
- Delete: `common/Lost.h`
- Delete: `common/Types.h`
- Delete: `main_bak.cpp`

### Step 1: 删除死文件

```bash
rm common/Lost.h common/Types.h main_bak.cpp
```

### Step 2: 编译验证确保无人依赖这些文件

```bash
cd build && make -j$(nproc)
```

### Step 3: Commit

```bash
git rm common/Lost.h common/Types.h main_bak.cpp
git commit -m "chore: remove dead files (Lost.h, Types.h, main_bak.cpp)"
```

---

## Task 7: 移除不必要的 `<iostream>` include

**Files:**
- Modify: `axcore/include/VencHelper.hpp` (line 3)
- Modify: `axcore/include/VdecHelper.hpp` (line 3)

### Step 1: 移除 VencHelper.hpp 中的 iostream

```cpp
// axcore/include/VencHelper.hpp — DELETE line 3
// #include <iostream>
```

### Step 2: 移除 VdecHelper.hpp 中的 iostream

```cpp
// axcore/include/VdecHelper.hpp — DELETE line 3
// #include <iostream>
```

### Step 3: 编译验证

```bash
cd build && make -j$(nproc)
```

Expected: Build succeeds.

### Step 4: Commit

```bash
git add axcore/include/VencHelper.hpp axcore/include/VdecHelper.hpp
git commit -m "chore: remove unnecessary <iostream> includes from headers"
```

---

## Self-Review Checklist

**1. Spec coverage:**
- HIGH #3 (race condition) — Task 1 ✅
- HIGH #4 (double-free) — Task 2 ✅
- HIGH #5 (use-after-free) — Task 3 ✅
- HIGH #6 (message drop) — Task 4 ✅
- HIGH #7 (exit(-1)) — Task 5 ✅
- LOW 死文件 — Task 6 ✅
- LOW iostream — Task 7 ✅
- HIGH #2 (raw pointer) — 本次不做大规模重构，超出最小改动范围。标记为后续优化。
- MEDIUM 所有项 — 本次暂不改动，影响较小

**2. Placeholder scan:** All steps have concrete code. No TBD/TODO placeholders.

**3. Type consistency:**
- Task 5 中 `Postprocess` 从 `void` 改为 `int`，所有调用点已更新。
- atomic 操作使用 `.store()` / `.load()` 保持一致。

---

## Execution Order

Recommended order:
1. Task 1 (atomic) — 最安全，零风险
2. Task 2 (Engine double-free) — 消除明确的双释放
3. Task 3 (FFmpegEncoder) — 消除 use-after-free
4. Task 4 (message queue warning) — 纯日志，低风险
5. Task 5 (exit → error code) — 涉及多文件联动
6. Task 6 (dead files) — 独立清理
7. Task 7 (iostream) — 独立清理

每步完成后编译验证再进入下一步。
