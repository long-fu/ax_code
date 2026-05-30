# 代码审查发现汇总

> **审查日期:** 2026-05-29
> **项目:** ax_core — 高性能视频流目标检测 (AX670/AX620 边缘设备)
> **审查范围:** 全部源代码，聚焦内存安全、线程安全、错误处理、架构设计
> **总计:** 5 CRITICAL + 8 HIGH + 6 MEDIUM + 3 LOW = 22 个问题

---

## CRITICAL (5)

### C1: VdecHelper RecvStreamFunc — GetChnFrame 失败后仍 ReleaseChnFrame

**文件:** `axcore/src/VdecHelper.cpp:273-277`

`AX_VDEC_GetChnFrame` 返回失败时，代码仍然调用 `AX_VDEC_ReleaseChnFrame` 释放一个从未成功获取的帧结构体。SDK 可能内部存储了指针，导致 use-after-free。

```cpp
// 当前代码
AX_VIDEO_FRAME_INFO_T *frameInfo = new AX_VIDEO_FRAME_INFO_T();
sRet = AX_VDEC_GetChnFrame(VdGrp, VdChn, frameInfo, -1);
if (sRet != AX_SUCCESS) {
    AX_VDEC_ReleaseChnFrame(VdGrp, VdChn, frameInfo);  // 问题：从未获取到帧
    delete frameInfo;
}
```

**修复:** 失败时跳过 `ReleaseChnFrame`，直接 `delete frameInfo`。

---

### C2: FFmpegEncoder — Release() 泄漏 AVCodecContext，部分初始化析构崩溃

**文件:** `axcore/src/FFmpegEncoder.cpp:157-171`

`Release()` 只关闭了 `AVFormatContext`，未调用 `avcodec_free_context(&m_pVideo_avcc)`。如果 `Init()` 在 `avcodec_open2` 成功后但在 `avformat_write_header` 之前失败，析构函数调 `Release()` 会操作未完全初始化的上下文导致崩溃。

**修复:**
1. `Release()` 末尾添加 `avcodec_free_context(&m_pVideo_avcc)`
2. 各成员指针置 nullptr 前检查非空

---

### C3: VencHelper — callback_/user_data_ 跨线程无同步

**文件:** `axcore/src/VencHelper.cpp:102`

`callback_` 和 `user_data_` 在 `Encode()` 调用线程写入，在 `VencRecvThreadFunc` 工作线程读取，无任何同步原语。C++ 内存模型下属于数据竞争（UB）。

**修复:** 将 `callback_` 声明为 `std::function` + `mutable std::mutex` 保护，或使用锁保护访问点。

---

### C4: PipelineThreadMgr — detach 线程导致 use-after-free

**文件:** `pipeline/src/PipelineThreadMgr.cpp:28-29`

```cpp
void PipelineThreadMgr::CreateThread() {
    std::thread engine(&PipelineThreadMgr::ThreadEntry, this);
    engine.detach();  // 分离后无法追踪生命周期
}
```

`ReleaseThreads()` 轮询状态后直接 `delete thread_list_[i]`，但分离线程仍在运行并访问 `this` 指向的对象 → use-after-free。

**修复:**
1. 用 `std::thread` 句柄替代 `detach()`
2. `ReleaseThreads()` 中调用 `th_mgr->thread_.join()`
3. 或者设置超时强制退出

---

### C5: g_main_thread_id 从未赋值，依赖碰巧正确

**文件:** `pipeline/include/Pipeline.h:12`

全局变量 `g_main_thread_id` 初始化为 0，从未被赋值。`Wait()` 和 `ReleaseThreads()` 用它索引 `thread_list_[0]`，只有碰巧 main thread manager 在 index 0 时才正确。架构变化时会静默出错。

**修复:** 在 `Pipeline::Init()` 中显式赋值 `g_main_thread_id = 0`，或改为 member variable。

---

## HIGH (8)

### H1: IvpsHelper CreateGrp 回滚缺失 — 部分初始化资源泄漏

**文件:** `axcore/src/IvpsHelper.cpp:94-133, 140-186

`AX_IVPS_CreateGrp` 成功后，后续步骤（`SetPipelineAttr`、`EnableChn`、`StartGrp`）失败时，IVPS group 资源未被清理。重新初始化也会失败因为 group 已存在。

**修复:** 每个 SDK 调用后检查返回值，失败时按逆序清理已分配的资源。

---

### H2: IvpsHelper GetChnFrame timeout=-1 — 硬件挂起无限阻塞

**文件:** `axcore/src/IvpsHelper.cpp:266-283`

`AX_IVPS_GetChnFrame(grp, chn, &tDstFrame->stVFrame, -1)` 使用 `-1` 作为超时（无限等待）。硬件挂起时无限阻塞，且源帧已通过 `SendFrame` 送入管线但未返回，造成管线 buffer 泄漏。

**修复:** 改用有限超时（如 100ms），超时后记录日志并返回错误。

---

### H3: FFmpegEncoder WriteFrame 无限循环

**文件:** `axcore/src/FFmpegEncoder.cpp:191-216`

```cpp
while (true) {
    ret = avcodec_receive_packet(m_pVideo_avcc, &pkt);
    if (ret < 0) {
        // ...
        return -1;
    }
    // ...
}
```

当 `avcodec_receive_packet` 返回 `AVERROR(EAGAIN)` 时，编码器缓冲区已排空，循环应退出但实际会自旋。`ret < 0` 判断不够精确，`EAGAIN` 也是负值但不该 break。

**修复:** 明确判断 `ret == AVERROR_EOF` 才 break，`ret == AVERROR(EAGAIN)` 也 break。

---

### H4: FFmpegDecoder Decode 泄漏 AVFormatContext

**文件:** `axcore/src/FFmpegDecoder.cpp:148-198`

`Decode()` 中有多个 early return 路径（`OpenVideo` 失败、`video_index == -1` 等），`av_format_context` 均未被释放。

**修复:** 每个 return 前调用 `avformat_close_input(&av_format_context)`。

---

### H5: RuleEngine dlopen 路径未验证

**文件:** `common/RuleEngine.cpp:71`

```cpp
loaded.handle = dlopen(cfg.so_path.c_str(), RTLD_NOW);
```

`so_path` 来自 YAML config，未经任何校验即可传入 `dlopen`。攻击者修改 config.yaml 可加载任意库实现任意代码执行。

**修复:** 限制插件目录白名单，要求路径以指定目录开头；可选检查文件是否存在。

---

### H6: Hungarian 算法 early-exit 不清空 Assignment

**文件:** `tracker/sort/Hungarian.cpp:29,35`

输入无效时 `Solve()` 返回 `-1.0` 但不清空 `Assignment` 向量。调用者在 `sort_track.cpp:108` 迭代 `assignment[i]` 读到陈旧数据。

**修复:** early-exit 路径中将 `Assignment` 清空或填充为 -1。

---

### H7: BusProcess Map/Unmap 返回值忽略

**文件:** `core/inc/BusProcess.hpp:82,90`

硬件内存映射失败后，`DrawText`/`DrawRect` 操作未映射的地址会导致 bus fault / segfault。

**修复:** 检查 `Map()` 返回值，失败则跳过绘制并返回错误码。

---

### H8: Yolov5 Process 返回值忽略

**文件:** `core/inc/InfProccess.hpp:31`

NPU 推理失败时，未处理的输出缓冲区传给 `Postprocess` 产生垃圾检测。

**修复:** 捕获返回值，失败则 log 错误并跳过本帧。

---

## MEDIUM (6)

### M1: ThreadSafeQueue::ExtendCapacity 无锁写

**文件:** `common/ThreadSafeQueue.h:50-52`

`m_queue_capacity_` 在持锁时被读，但在 `ExtendCapacity()` 中无锁写入，数据竞争。

**修复:** 在 `ExtendCapacity()` 内持锁写入，或改为 `std::atomic<uint32_t>`。

---

### M2: PreProcess 构造函数 raw new/delete

**文件:** `core/inc/PreProcess.hpp:14-23`

Raw `new`/`delete` 对，若 `Init()` 抛异常，dtor 可能在半构造对象上运行。`EncProcess` 同理。

**修���:** 改用 `std::unique_ptr<VencHelper>` 等智能指针。

---

### M3: SampleRule Init std::stof 不捕获异常

**文件:** `rules/sample_rule/sample_rule.cpp:28`

`std::stof` 在格式错误时抛出 `std::invalid_argument`，`Init()` 无 try/catch，异常逃逸导致未定义行为。

**修复:** 包裹 `try/catch`，解析失败返回错误码。

---

### M4: BusProcess YAML 解析脆弱

**文件:** `core/inc/BusProcess.hpp:25-36`

字符串 `find("rules:")` 匹配易误触发（如注释中包含相同文本）。配置格式变化时静默产生零规则。

**修复:** 使用 yaml-cpp 等正规 YAML 解析库。

---

### M5: EncProcess Process 始终 return 0

**文件:** `core/inc/EncProcess.hpp:73`

```cpp
ret = m_p_venc->Write(&in_data->image, nullptr);
// ...
return 0;  // 硬编码返回 0！
```

`ret` 设置了但函数硬编码返回 0，VENC 写入失败被静默吞掉。

**修复:** 改为 `return ret;`

---

### M6: IVPS Re-init 仅重设 channel 0

**文件:** `axcore/src/IvpsHelper.cpp:162-164`

Re-init 时只 disable/enable channel 0，但 `CreateGrp` 循环遍��所有 output channels。虽当前 `nOutChnNum=1` 不影响，但逻辑不完整。

---

## LOW (3)

### L1: RTSP/RTMP 凭证硬编码

**文件:** `main.cpp:53,87`

URL 中的用户名密码会被提交到 git 历史并在二进制中暴露。建议移入环境变量或 config.yaml。

---

### L2: is_finished_ 非 atomic

**文件:** `axcore/include/VdecHelper.hpp:42`

`is_finished_` 在 `StopDecode()` 中被写入，可能被其他线程读取，应为 `std::atomic<bool>`。

---

### L3: pthread_create 失败后 recv_thd_ 未初始化

**文件:** `axcore/src/VencHelper.cpp:234-241`

`pthread_create` 失败时 `recv_thd_` 为未初始化值，`StopEncode()` 中的 `pthread_join` 会 join 垃圾值。

**修复:** `pthread_create` 失败时初始化 `recv_thd_ = pthread_t{}`。

---

## 优先级建议

| 优先级 | 问题 | 影响 |
|--------|------|------|
| P0 | C1, C2, C4 | 运行时 crash / memory corruption |
| P0 | C3, C5 | 数据竞争 / 逻辑错误 |
| P1 | H1-H4 | 资源泄漏 / 死锁风险 |
| P1 | H5 | 安全风险 |
| P1 | H6-H8 | 功能正确性 |
| P2 | M1-M6 | 健壮性改进 |
| P3 | L1-L3 | 小问题 |
