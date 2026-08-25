# 代码审查发现汇总（2026-08-24）

> **项目:** ax_core — 高性能视频流人脸检测识别（Axera NPU 边缘设备）
> **数据流:** RTSP → PreProcess(解码/CSC) → InfProcess(SCRFD 检测) → BusProcess(跟踪/识别/上报) → EncProcess(编码推流)
> **审查范围:** models/、tracker/、pipeline/、hal/、stages/、apps/、common/
> **未修复问题:** 1 条 —— 仅 **P2-2**，需上板实测数据才能定论
> **已修复:** P0-2、P1-1~P1-4、P2-3~P2-7、A-1~A-5（2026-08-24）
> **已确认误报:** 5 条（**不要去改**，见第三节）
>
> **除 P2-2 外全部清空。** P2-2 不是代码问题而是「缺一个观测值」：
> 在 `scrfd.cpp` 打印一次 score 取值范围即可关闭或转为待修。

---

## 怎么用这份文档

每条问题都有稳定编号（如 `P0-1`）。逐条修复时直接说**「修 P0-1」**即可，我会按本文档的定位和修法执行。
修完把该条的 `- [ ]` 勾成 `- [x]`。

优先级含义：

- **P0** — 影响正确性或稳定性，每次运行都在触发，或会导致崩溃。优先修。
- **P1** — 「短时测试看不出、长跑必暴露」类型。**建议在你正式实测前修掉**，否则实测数据会被这些问题污染。
- **P2** — 需要实测确认，或当前配置不触发，或仅影响错误路径的健壮性。

---

# 一、P0：优先修复

## ~~P0-1 · MflushCache 传入 host 指针~~ —— 已驳回，不是问题

> **状态:** 误报，代码无需修改。编号保留以免后续引用错位。详见第三节的完整分析。

初版把这条判成 critical，理由是「flush 的虚拟地址与物理地址不匹配，NPU 可能读到陈旧输入」。
**这个结论是错的。** 输入缓冲是非 cached 内存，不存在陈旧数据问题，该 flush 调用本身惰性无害。
且此段代码来自官方 SDK 示例，保持与示例一致有利于后续 diff 升级。**保持现状。**

---

## - [x] P0-2 · 关机竞态：VDEC/FFmpeg 线程可能在 TaskNodeMgr 析构后投递消息

> **已修复（2026-08-24）。** 修复内容见本条末尾「修复记录」。

**位置:** `apps/main_rtsp.cpp:50` 与 `:60`；`stages/pre_process.h:78`、`:32-42`、`:134-137`
**严重程度:** critical　**触发时机:** 每次退出（SIGINT 或 Wait 结束）

时序问题：

```cpp
// apps/main_rtsp.cpp:50-62
app.Exit();                       // ① join 工作线程 + 销毁 TaskNodeMgr
for (...) { delete thread_tbl[i].node; }   // ② 此时才进 ~PreProcess，才 StopDecode
```

而停止解码线程是在 `~PreProcess` 里才做的：

```cpp
// stages/pre_process.h:32-38
~PreProcess() {
  ff_decoder_->StopDecode();
  vdec_->StopDecode();
  pthread_join(ffmpeg_thread_, nullptr);
```

① 和 ② 之间存在窗口期。这期间 VDEC 接收线程仍在跑，其回调
`VdecProcessCallbackFunc`（`pre_process.h:78`）会调用 `pipeline::SendMessage`，
打到已经销毁的 `TaskNodeMgr` 上。FFmpeg 解码线程经
`FrameProcessCallbackFunc` → `vdec_->Write()` 同理。

表现是退出时**偶发**段错误，概率取决于窗口期长短。

**崩溃点的精确位置**（比初版描述更严重，不是「偶发」）：

```cpp
// pipeline/task_scheduler.cpp SendMessage
if (dest < 0 || static_cast<uint32_t>(dest) >= thread_list_.size()) { ... }
return thread_list_[dest]->PushMessage(message);
```

`ReleaseThreads()` 把 `thread_list_` 的元素置空却**不缩小 vector**，所以边界检查照常通过，
随即对空指针调 `PushMessage`。而且 `ReleaseThreads` 是按 1→2→3→4 顺序「join 一个、置空一个」，
PreProcess 是 index 1 **最先被置空**，之后还要花几十毫秒 join 其余三个 worker——
这段时间 VDEC 接收线程仍在以 25fps 往空槽位投递。**基本每次退出都会命中。**

### 修复记录

分四步，均已完成并编译通过：

1. **`TaskNode` 新增 `virtual void StopSources()`**（`pipeline/task_node.h`），
   `ExitPipeline` 在 `app.Exit()` **之前**同步遍历调用（`apps/main_rtsp.cpp`）。
   `PreProcess` 覆写它，把原先在析构里的 `ff_decoder_->StopDecode()` /
   `vdec_->StopDecode()` / `pthread_join` 搬进去，用 `sources_stopped_` 保证幂等，
   析构仍调一次兜底。
   　　*不用 `kMsgAppExit` 消息实现*：消息是异步的、没有「已处理完」的同步点，
   且此刻 `Wait()` 已返回、主线程不再泵消息，无法可靠等待。
   　　*只有管线头部需要覆写*：尾部 `EncProcess` 若也在此停编码器，
   `app.Exit()` 期间上游还在排空队列，会往已停止的编码器投数据。已在钩子注释中写明。
2. **`SendMessage` 增加 `dest < 0` 与 `thread_list_[dest] == nullptr` 检查**
   （`pipeline/task_scheduler.cpp`）。这只是兜底网——该分支若被命中即说明还有
   未纳管的外部线程，可据此排查。
3. **`VdecHelper::StopDecode()` 不再跳过 join**（`hal/ax/vdec_helper.cpp`）。
   原先 `AX_VDEC_StopRecvStream` 返回 `AX_ERR_VDEC_UNEXIST` 时直接 `return 0`，
   接收线程未回收，随后 `delete vdec_` 即是对仍在解引用 `self` 的
   `RecvStreamFunc` 的 use-after-free。第 1 步的正确性依赖 `StopDecode()` 真同步，
   故必须一并修。同时新增 `recv_started_` 避免 join 未创建的线程。
4. **`AX_VDEC_GetChnFrame` 改用有限超时**（`kRecvFrameTimeoutMs = 200`）。
   原先传 `-1`（无限阻塞），与旁边「有限超时避免 StopDecode 时永久卡住」的注释矛盾；
   若 `StopRecvStream` 未能唤醒 `GetChnFrame`，`while (!is_stop_)` 永无机会检查标志，
   `pthread_join` 挂死、进程退不出去。超时返回 `AX_ERR_VDEC_QUEUE_EMPTY`，
   已有分支按「无数据」处理（去掉了其中冗余的 `usleep`）。
   　　配套把「取帧失败」的 `LOG_ERROR` 从无条件改为**仅未预期错误码**才记，
   否则空闲时每 200ms 刷一条。

**顺带修掉的两处**（同一批改动的必然产物）：

- **P2-7**（`pthread_join` 未创建的线程）：`ffmpeg_thread_` 改零初始化 +
  `ffmpeg_started_` 标志，`pthread_create` 返回值不再被忽略。
- **`is_stop_` 初始化位置**：原先在 `RecvStreamFunc` 内部 `store(false)`，
  若 `StopDecode()` 在线程真正开跑前就置了 `true`，线程反手清成 `false`，
  循环永不退出、join 挂死。已移到 `Decode()` 中 `pthread_create` 之前。
- **`AX_ERR_VDEC_NOT_PERM` 分支补 `usleep`**：该码表示「硬件初始化中」，
  原先无延时 `continue` 会忙等占满一个核。

### 验证方法（待你在板子上确认）

连续 Ctrl-C 十次，确认无段错误、且每次都在 1 秒内退干净。
退出日志顺序应为 `PreProcess::StopSources` → `ExitPipeline sources stopped`
→ `app.Exit()` → `TaskNode thread N released` → `~PreProcess`。

关键观测点：`SendMessage` 里那条 **`node released` 日志不应出现**。
一旦出现，说明还有外部线程没被 `StopSources()` 覆盖，需要顺着日志里的 `dest` 追。

---

# 二、P1：实测前建议修掉

## - [x] P1-1 · `removed_stracks` 无界增长：内存与**每帧 CPU** 双双随运行时长上涨

> **已修复（2026-08-24）。** 完整逻辑核对后确认成立，且危害比初版判断更大——
> 除内存/CPU 增长外还会**误伤复活轨迹**。详见末尾「修复记录」。

**位置:** `tracker/bytetrack/BYTETracker.h:42`、`BYTETracker.cpp:219-222`
**严重程度:** high　**触发条件:** 长时间运行（7×24）

成员 `removed_stracks` 每帧 append，**从不 clear 或裁剪**：

```cpp
// BYTETracker.cpp:219-222
this->lost_stracks = sub_stracks(this->lost_stracks, this->removed_stracks);
for (int i = 0; i < removed_stracks.size(); i++)
{
    this->removed_stracks.push_back(removed_stracks[i]);
}
```

内存增长只是次要问题（`STrack` 含 Eigen 矩阵，每个几百字节）。**更麻烦的是 CPU**：
第 219 行每帧都拿这个只增不减的容器去做 `sub_stracks`，而 `sub_stracks`
（`tracker/bytetrack/utils.cpp:46-60`）会遍历整个 `tlistb`。也就是说
**每帧的跟踪开销随已移除轨迹总数线性增长**。

这是典型的「10 分钟测试完全正常，连续跑几天逐渐掉帧」问题。人流量越大恶化越快。

### 修复记录

**核对结论：成立，且比初版判断更严重。** 逐帧追踪 `update()` 全流程后确认：

`this->removed_stracks` 在全文件中**只有 line 219 一处读取**。而 line 219 用到的
内容恰好只是「上一帧」的移除列表——因为轨迹在第 N 帧 `mark_removed()` 时并不会
立刻从 `lost_stracks` 摘掉（189-196 只标记不删除），要等第 N+1 帧走到 line 219
才被 `sub_stracks` 掉；此后经 198-204 按 `state == Tracked` 过滤，它也不可能再回到
`tracked_stracks`。**所以保留一帧就够，保留更多毫无用处。**

而且不 clear 还有一个初版没发现的**功能性副作用**：

Removed 轨在被摘掉之前还会参与一帧关联（line 76 的 `strack_pool` 包含
`lost_stracks`）。若此时匹配上，line 91 因 `state != Tracked` 走 `re_activate()`
**复活**该轨。但它的 id 已经永久留在 `this->removed_stracks` 里，于是该轨日后
再次 `mark_lost()` 时，会在 line 219 被立刻删除，**彻底失去 `max_time_lost` 的
宽限期**，只能以新 `track_id` 重新出现。对上层的表现就是同一个人被重复报警。

**修法（已实施）:** 在 line 219 之后、追加本帧移除列表之前插入
`this->removed_stracks.clear();`。

选择这个改法而非「改为 `unordered_set<int>`」的理由：clear 之后容器每帧只剩个位数
元素，`STrack` 拷贝开销可忽略，**无需改动 `sub_stracks` 签名**——对第三方库代码
最小化 diff 更安全。

**与上游的分歧：** 官方 ByteTrack（Python 与 ncnn C++ 端口）同样没有 clear，
这是上游本身的缺陷。在定长测试视频上跑 benchmark 不会暴露，7×24 会。
已在代码中写明分歧原因，便于将来对照上游更新。

---

## - [x] P1-2 · lapjv：`sizeof` 误用导致 8 倍超额分配，失败时 `exit(0)` 杀进程

> **已修复（2026-08-24）。两处都成立，但初版给的因果链是错的**——
> 「超额分配 → 易 OOM → 触发 exit」这条链不成立。详见末尾「修复记录」。

**位置:** `tracker/bytetrack/utils.cpp:350-364`（超额分配）、`:366-372`（exit）
**严重程度:** high　**触发条件:** 内存紧张或目标数较多时

这里有两个 bug，而且**互相放大**：

```cpp
// utils.cpp:350-364
cost_ptr = new double *[sizeof(double *) * n];   // 应为 new double*[n]
for (int i = 0; i < n; i++)
    cost_ptr[i] = new double[sizeof(double) * n]; // 应为 new double[n]
...
int* x_c = new int[sizeof(int) * n];              // 应为 new int[n]
int *y_c = new int[sizeof(int) * n];              // 同上
```

`sizeof` 被当成了长度的一部分，64 位平台上每次多分配 **8 倍**内存。
矩阵是 `(n_rows+n_cols)²` 规模，人多的时候本就不小，8 倍放大后在嵌入式上很容易分配失败。

而分配失败正好撞上第二个 bug：

```cpp
// utils.cpp:366-372
int ret = lapjv_internal(n, cost_ptr, x_c, y_c);
if (ret != 0)
{
    std::cout << "Calculate Wrong!" << std::endl;
    system("pause");   // Linux 上无效
    exit(0);           // 直接杀掉整个进程
}
```

### 修复记录

**核对结论：两个问题都真实存在，但初版把它们串成了一条因果链，这是错的。**

先确认无泄漏：函数末尾 414-420 行完整释放了 `cost_ptr` 各行、`cost_ptr`、
`x_c`、`y_c`。所以**只是超额分配，没有泄漏**。

**因果链不成立的原因：** 超额分配用的是 `new`，失败时抛 `std::bad_alloc` 而非
返回空——那会走未捕获异常 → `std::terminate` → abort，**根本到不了 `exit(0)`**。
而 `exit(0)` 的实际前提是 `lapjv_internal` 返回非 0，追进 `lapjv.cpp` 确认
它**只在 malloc 失败时**返回非 0（`lapjv.h:13` 的 `NEW` 宏；`lapjv.cpp:208`
那个 `return -1` 属于 Dijkstra 内层的正常控制流，不是 `lapjv_internal` 的返回），
且那里只分配 O(n) 的小缓冲。两条路径都要求内存近乎耗尽。

**所以两处各自的真实理由是：**

- **超额分配**：`n = n_rows + n_cols`，人脸场景下 n 通常几十，8 倍后也只是
  几十 KB 到几百 KB（n=100 时 80KB → 640KB），**够不上 OOM 风险**。
  真正的代价是 `linear_assignment` 每帧调用 3 次，等于持续制造 8 倍的
  分配/释放抖动——长跑的内存碎片压力。修它的主要理由是「三个 token 的改动、
  正确性显然」，而不是防 OOM。
- **`exit(0)`**：触发概率确实很低，但**后果被低估了**。退出码 0 意味着
  systemd 的 `Restart=on-failure`、docker 的重启策略都会认为这是**正常退出**
  而不重启服务。视频服务静默消失且不自愈，比崩溃更难发现。
  修它的理由是后果而非概率。

**修法（已实施）:**

1. `new double*[n]` / `new double[n]` / `new int[n]`，去掉误加的 `sizeof`。
2. 失败路径改为：`LOG_ERROR` + 把 `rowsol`/`colsol` 全置 -1 + 正常释放 + 返回 0。
   上层 `linear_assignment` 会把全部轨迹与检测都归入未匹配（轨迹转 lost、
   检测另起新轨），下一帧内存恢复后自然回到正常匹配。

**顺带完成 A-1 中的一处**：同一函数内 line 286-291 的另一处 `exit(0)`
（`extend_cost=false` 分支）已按同样方式改为放弃本帧匹配。该分支当前不可达
（调用方恒传 `true`），但留着 `exit` 意味着将来新增调用点就会变成进程级杀手。
`system("pause")` 两处一并删除——在 Linux 上只会让 `/bin/sh` 报一句 not found。

---

## - [x] P1-3 · 队列满时静默丢帧，无日志无背压

> **已修复（2026-08-24）。** 按你的要求只加可观测性，未改背压策略。
> 实现放在唯一的收敞口 `TaskNodeMgr::PushMessage`，无需改动四处调用点。

**位置:** `stages/pre_process.h:78`、`:118`；`stages/inf_process.h:76`
**严重程度:** high　**触发条件:** 任一 stage 处理速度跟不上上游

三处 `pipeline::SendMessage` 的返回值都没有检查：

```cpp
// pre_process.h:78
pipeline::SendMessage(self->InstanceId(), kMsgVdecData, data);
// pre_process.h:118
pipeline::SendMessage(next_thread_id_, kMsgPreprocData, data);
// inf_process.h:76
pipeline::SendMessage(next_thread_id_, kMsgInfprocData, out_data);
```

队列容量 256，满时 `PushMessage` 返回失败，帧被丢弃且**完全没有日志**。
结果是跟踪 ID 断裂、漏检，但你从日志里看不出发生过丢帧，只会觉得「跟踪效果不好」。

> 注：Qdrant 检索异步化之后 BusProcess 主线程已经快了很多，这条的触发概率降低了，
> 但可观测性仍然必须补上——否则实测时无法区分「算法效果差」和「在丢帧」。

### 修复记录

**实现位置的选择：** 没有去改那四处 `SendMessage` 调用点，而是改
`TaskNodeMgr::PushMessage`（`pipeline/task_node_mgr.cpp`）——它是所有
`SendMessage` 的**唯一收敞口**，只在这里记账即可全覆盖，且它自带节点名与
队列容量上下文，不会四处重复同样的日志代码。

**日志策略（三段式）：**

1. **首次丢弃立即打印**（`WARNING`），便于定位开始掉帧的确切时刻：
   `Node {} 队列满，开始丢弃消息 msg_id={} 容量={}`
2. **之后按 5 秒窗口汇总**：`Node {} 队列满丢弃 {} 条/{}秒 (累计 {})`。
   满载时丢弃可达每秒数十次，逐条打印会把日志刷爆。
3. **析构时汇报总量**：`Node {} 生命周期内共丢弃 {} 条消息(队列满)`，
   这样跑完一轮不必翻实时日志就能拿到数字。

另新增 `TaskNodeMgr::DroppedTotal()` 供将来接入统计上报。

**线程安全：** `PushMessage` 会被多线程调用（VDEC 回调线程 + 各 worker），
所以总计数用 `std::atomic`；窗口状态（`dropped_in_window_` /
`last_drop_log_`）只在丢弃分支内加锁，**正常路径无任何额外开销**。

**一处需要确认的细节：** 日志里读了 `message->msg_id`，而这发生在
`msg_queue_.Push(message)` 返回 false **之后**。已确认
`ThreadSafeQueue::Push(T input_value)` 是**按值**接收（`thread_safe_queue.h:27`），
失败时不会动到调用方的 `message`，故该读取安全。

**未做的部分：** 背压/阻塞等待、丢弃最旧帧等策略均未实施——按你的要求先只做
「能看见」。等实测拿到实际丢弃率再决定要不要动策略。

> 补充：`SendMessage` 的另外两种失败（`kDestInvalid` 目标无效/已释放、
> `kThreadAbnormal` 节点非运行态）此前就有日志，加上本次改动后
> **三种失败路径已全部可观测**。

---

## - [x] P1-4 · InfProcess 失败返回 0，错误被上层当成成功

> **已修复（2026-08-24）。** 但需修正初版判断：**严重程度被高估了**，
> 这是契约违约（latent），不是活跃 bug；其中一个分支还是不可达的。
> 详见末尾「修复记录」。

**位置:** `stages/inf_process.h:39-42`、`:55-58`、`:70-73`
**严重程度:** high

三个失败分支全部 `return 0`：

```cpp
// inf_process.h:55-58
if (infer_ret != 0) {
  LOG_ERROR("Engine Process failed, ret={}", infer_ret);
  return 0;          // ← 上层 TaskNodeMgr 视为成功
}
```

`engine_ == nullptr`、推理失败、后处理失败三种情况都返回 0。`TaskNodeMgr` 把非零返回值
当作 process error 打日志，返回 0 就什么都不做。虽然这里各自打了 `LOG_ERROR`，
但上层无法据此做任何统计或降级判断。

### 修复记录

**先修正两处初版判断：**

1. **`engine_ == nullptr` 分支不可达。** 初版建议「让 pipeline 停下来而不是每帧刷
   错误日志」，但追完启动链发现根本不会走到：`Init()` 返回非 0 → `ThreadEntry`
   置 `kError` 并退出 worker 线程 → `WaitThreadInitEnd()` 返回 `kStartThread`
   → `app.Start()` 失败 → `main` 直接 `ExitPipeline` 并 `return -1`。
   **启动期失败已经被正确处理了**，`Process` 里那个判空是纯防御代码。已保留但
   在注释中标明不可达。
2. **严重程度应为 medium 而非 high。** 当前 `ThreadEntry` 对非 0 返回值的处理就是
   「打一条日志 + continue」，没有任何统计/降级逻辑消费它。所以返回 0 造成的是
   **契约违约（latent）**——将来一旦有健康监控、看门狗或丢帧统计依赖这个返回值，
   就会静默失效。修它是为了契约诚实，不是修一个正在发作的故障。

**修法（已实施）：** 三条失败路径改为返回具名错误码
（`kErrEngineNotReady=-1` / `kErrInfer=-2` / `kErrPostprocess=-3`）。

**为什么同时要处理日志量：** 返回非 0 会让 `TaskNodeMgr::ThreadEntry` 额外打一条
`process function return error`。持续故障下每帧命中，25fps 即
「25 行本节点 + 25 行 ThreadEntry = 50 行/秒」，会把日志刷爆——等于用一个
可观测性改动制造了一个新的可观测性问题。

因此配了 `ReportFailure()` / `ReportRecovered()` 两个私有辅助（仅 worker 线程调用，
无需同步），日志变成**故障区间**的形式而非逐帧刷：

```
InfProcess: Engine Process 失败 ret=-2，开始丢弃帧          ← 首帧立即报
InfProcess: Engine Process 持续失败 5 秒，已连续丢弃 127 帧(累计 127)   ← 每 5s 汇总
InfProcess: 已恢复，本次故障持续 12 秒、丢弃 305 帧(累计 305)          ← 恢复时报
```

**恢复日志是这里最有价值的部分**：排查时真正想知道的是「从何时开始失败、丢了
多少帧、何时恢复」，而无论原来的代码还是单纯的限频都给不出「何时恢复」。

**刻意没做的一处：** `SendMessage` 的返回值仍不检查。队列满属**背压**而非本节点
故障（帧本身是正常产出的），且已由 P1-3 在 `TaskNodeMgr::PushMessage` 统一记账。
若在此再返回非 0，会把背压误报成推理失败，反而污染信号。已在代码中注明。

---

# 三、已确认的误报 —— 不要修改这些地方

自动审查把下面几条报成了 critical/high，我逐条核对后确认**当前代码是正确的**。
记录在此以免后续误改。

### P2-1 · ArcFace 通道序 —— `AX_FORMAT_RGB888` 的字节序实际是 BGR

**关键事实（由硬件行为决定，非代码可推导）：** Axera 的 `AX_FORMAT_RGB888`
**只是 SDK 的枚举名**，IVPS 实际输出的字节序是 **B, G, R**。

初版把「IVPS 输出 RGB888 → 未做 CSC → 模型要 BGR」串成了通道颠倒的证据链。
链条本身没错，错在第一环的前提：输出的根本就不是 RGB 排列。

**全链路核对（byte-verbatim，无任何转换点）：**

| 环节 | 位置 | 内容 |
|---|---|---|
| IVPS 裁剪输出 | `face_align.cpp:428` | 请求 `AX_FORMAT_RGB888`，实际吐 B,G,R |
| 拷进 cv::Mat | `image_data.cpp` `Copy2Mat` | 原样逐行拷贝，注释已写明「不做 CSC」 |
| 接收变量命名 | `face_align.cpp:449` | `cv::Mat roi_bgr` ← 命名即断言内容是 BGR |
| 仿射对齐 | `NormCrop` | 几何变换，与通道顺序无关 |
| 打包给模型 | `arcface.cpp:188` | `out.assign(...)` 原样，`BgrToRgbPacked` 注释掉 |
| 模型期望 | `arcface.h:15` `w600k_r50-bgr-std` | **BGR** ✓ 对上 |

**佐证：**

- `Copy2Mat` 全项目**只有一个调用方**（`face_align.cpp:449`），
  且 `HwRoiNormCrop` 的两个调用方（`arcface.cpp:214`、`bus_process.h` 的
  `CropFaceRoiFromFrame`）都不做通道处理——不存在某条路径反过来假设 RGB。
- 全项目**没有任何活跃的 `cvtColor`**，两处都在注释里。
- `face_align.cpp:469` 有 `cv::imwrite("roi_bgr.jpg", roi_bgr)` 的调试转储，
  而 `cv::imwrite` 按 BGR 解释 `CV_8UC3`。若字节实际是 RGB，
  转储出来的人脸会是明显的蓝脸。该调试代码被保留说明当时结果是正常的。
- `ivps_helper.cpp:324-325` 同时留着 `AX_FORMAT_RGB888` 与 `AX_FORMAT_BGR888`
  两行注释——正是发现枚举名与字节序不一致时会留下的痕迹。

**结论:** 当前实现正确，`BgrToRgbPacked` 被注释掉是**对的**。

**⚠ 不要做这两件事**，它们都会真正把通道换反，且**不报错、只静默掉精度**：

1. 把 `face_align.cpp:428` 改成 `AX_FORMAT_BGR888`
2. 启用 `arcface.cpp` 里注释掉的 `BgrToRgbPacked`

**已做的改动（非修复，仅防呆）:** 在 `face_align.cpp:428` 与
`image_data.cpp` `Copy2Mat` 的文档注释处写明了这个枚举名与字节序不一致的
硬件行为。原先这条约定只体现在 `roi_bgr` / `aligned_bgr` 的变量命名里，
过于隐晦——我和自动审查都因此误判过一次。

---

### P0-1 · `AX_SYS_MflushCache` 的地址参数不匹配 —— 惰性调用，保持现状

**位置:** `models/io.cpp:119`、`:139`（代码来自官方 SDK 示例）

```cpp
memcpy(io_t->pInputs[0].pVirAddr, data.data(), data.size());
// AX_SYS_MemAllocCached
AX_SYS_MflushCache(io_t->pInputs[0].phyAddr, (void*)data.data(), data.size());
```

表面上看，物理地址是 NPU 缓冲的 `phyAddr`，虚拟地址却是 host 侧 `std::vector` 的
`data.data()`，两者不指向同一块内存。初版据此判为 critical，认为「NPU 会读到陈旧输入」。

**这个推断是错的。** 关键在于输入缓冲的分配方式：`PrepareIo` 对输入使用
`strategy.first`（`models/io.cpp:56-63`），而 `models/engine.cpp:71` 传入的是
`AX_ENGINE_ABST_DEFAULT`，因此走 `AX_SYS_MemAlloc` 而非 `AX_SYS_MemAllocCached`
—— **输入是非 cached 内存**。

非 cached 内存的写入是直写的，不存在「脏 cache line 卡住数据」的情形，
因此根本不需要 flush。这个调用无论传哪个地址都是惰性的：既不会让 NPU 读到陈旧数据，
也不会有其他可观测后果（驱动最多返回一个被忽略的错误码）。

**结论:** 保持现状。代码来自官方 SDK 示例，与示例保持一致有利于后续跟随 SDK 升级做 diff。

**唯一需要留意的场景:** 如果将来把输入改为 `AX_ENGINE_ABST_CACHED`
（旁边那行 `// AX_SYS_MemAllocCached` 注释说明曾考虑过），flush 就变成必需的，
届时这个地址参数必须改成 `io_t->pInputs[0].pVirAddr`，否则 flush 会静默失效。
改分配策略时记得回来看这里。

### SCRFD anchor 中心的 `+0.5` 偏移 —— 当前不加是对的

**位置:** `models/scrfd.cpp:167-171`

审查建议恢复注释掉的 `+ 0.5f`。**不要改。** 官方 insightface `scrfd.py` 的实现是：

```python
anchor_centers = np.stack(np.mgrid[:height, :width][::-1], axis=-1).astype(np.float32)
anchor_centers = (anchor_centers * stride).reshape((-1, 2))
```

没有 `+0.5`。当前代码与官方一致。

### SCRFD 输出按扁平索引读取 —— 布局是对的

**位置:** `models/scrfd.cpp:191-203`

审查认为输出是 NCHW、应改成 `(q * feat_size + index)` 索引。**不要改。**
insightface 的 ONNX 导出输出已经展平为 `(N,1)` / `(N,4)`（N = h×w×2），
当前的 `scores[i]` / `bboxes[i*4+k]` 与之匹配。审查是拿 ax-samples 里未展平的
NCHW 变体做对比得出的结论。

更直接的反证：如果索引真的跨通道错位，框和关键点会完全错乱，
现在根本不可能跟踪成功、也不可能对齐出可用的人脸特征。

### 两处 `exit(0)` 当前不可达

- `tracker/bytetrack/utils.cpp:290` —— 仅在 `extend_cost=false` 且矩阵非方阵时触发，
  但唯一调用点 `linear_assignment` 恒传 `extend_cost=true`，不可达。
- `tracker/bytetrack/BytekalmanFilter.cpp:135` —— `gating_distance(..., only_position=true)`
  的未实现分支，当前代码库没有任何调用点，不可达。

建议顺手清掉（见附录 A-1），但不属于需要紧急处理的问题。
**注意：`utils.cpp:371` 的那处 exit 是可达的真问题，见 P1-2。**

---

# 四、P2：需实测确认或当前不触发

## ~~P2-1 · ArcFace 输入通道序 RGB/BGR 颠倒~~ —— 已驳回，不是问题

> **状态:** 误报，代码本身无需修改。编号保留以免后续引用错位。
> 完整分析见第三节。已在两处补充注释防止后续被"修正"。

---

## - [ ] P2-2 · SCRFD 分数未做 sigmoid（需实测确认）

**位置:** `models/scrfd.cpp:193`
**严重程度:** medium（若成立）

```cpp
const float score = scores[i];
if (score < config_.prob_threshold) continue;
```

直接把模型输出与阈值（0.5~0.8）比较，没有 sigmoid。官方实现是先 sigmoid 再比阈值。
若模型输出是 logits，则阈值语义完全不同（`sigmoid(0.5) ≈ 0.62`）——注意这**不会导致检测失效**，
只是让 `prob_threshold` 这个配置项的实际含义和你以为的不一样，调参时会很困惑。

模型名带 `-std` 后缀，有可能已经把 sigmoid 折进 axmodel 图里了。

**验证方法:** 在 `scrfd.cpp:193` 临时打印 `scores[i]` 的取值范围。
如果全部落在 `[0,1]` → sigmoid 已折进模型，当前代码正确，此条关闭。
如果出现负数或 >1 → 是 logits，需要补 sigmoid。

---

## - [x] P2-3 · `Engine::Process` 不校验 Init 是否成功

> **已修复（2026-08-24）。** 新增 `Engine::IsReady()`，两个 `Process` 重载入口自查。

**位置:** `models/engine.cpp:83`、`:103`
**严重程度:** medium

`Process` 直接使用 `io_data_` / `io_info_` / `handle_`，不检查 `handle_valid_`。
若 `Init()` 在 `AX_ENGINE_GetIOInfo`（`engine.cpp:59`）之前就失败，`io_info_` 仍是空的，
`PushInput` 里 `info_t->nInputSize` 就是对空指针解引用。

配合 P1-4（InfProcess 在 engine 无效时每帧只是打日志继续跑），这条更容易被触发。

### 修复记录

新增公开方法 `Engine::IsReady()`（`engine.h` / `engine.cpp`），判据为
`handle_valid_ && handle_ != nullptr && io_info_ != nullptr && !is_released_`。
两个 `Process` 重载在入口自查，不满足即 `LOG_ERROR` + 返回 -1。

`Destroy()` 中补上 `io_info_ = nullptr`——它本来只清 handle，不清 `io_info_`，
导致 Destroy 之后 `IsReady()` 仍可能为真。同时 `is_released_` 也纳入判据，
使「已析构后误调 Process」同样被拦住。

配合 P1-4，`InfProcess` 现在即便在 engine 无效时被调用也只会拿到 -1 并计入
故障统计，不会解引用空的 `io_info_`。

---

## - [x] P2-4 · `Engine::Init` 失败路径未对称调用 `AX_ENGINE_Deinit`

> **已修复（2026-08-24）。** 修复过程中发现它依赖一个前置问题（A-4），
> 直接改会引入**双重释放**，故两者一并处理。详见末尾「修复记录」。

**位置:** `models/engine.cpp:28`（置位）、`:32-36`（ReadFile 失败路径）
**严重程度:** medium

```cpp
// engine.cpp:23-36
auto ret = AX_ENGINE_Init(&npu_attr);
if (0 != ret) { ...; return ret; }
engine_inited_ = true;

if (!utilities::ReadFile(model_path_, model_buffer)) {
  LOG_ERROR(...);
  return -1;              // ← 没有 Deinit，全局 NPU runtime 停在已 Init 状态
}
```

`AX_ENGINE_Init` 成功后若 `ReadFile` 失败直接返回，不调用 `AX_ENGINE_Deinit`。
依赖对象析构走 `Destroy()` 清理——如果 Init 失败后对象仍存活（当前 `InfProcess`
就是保留 `engine_` 指针的），全局 NPU runtime 就处于「已 Init 但无有效 handle」的状态。

### 修复记录

**修法:** 五条失败路径（ReadFile / CreateHandle / CreateContext / GetIOInfo /
PrepareIo）统一改为调用 `Destroy()` 后返回，删掉各自手写的 `DestroyHandle`。
`Destroy()` 由 `is_released_` 保证幂等，析构再调一次无副作用。
另在 `Init()` 开头重置 `is_released_ = false`，使 Init 失败后可重试。

**⚠ 发现的前置依赖（原计划之外）：** 直接让失败路径调 `Destroy()` 会引入
**双重释放**。原因是 `Destroy()` 第一件事就是 `middleware::FreeIo(&io_data_)`，
而 `PrepareIo` 失败时留下的 `io_data` 是**脏的**：

- CMM 已由内部 `FreeIoIndex` 释放，但 `pInputs`/`pOutputs` 数组仍在、
  `nInputSize`/`nOutputSize` 仍是有效值 → `FreeIo` 会对同一批缓冲二次
  `AX_SYS_MemFree`；
- 且 `new AX_ENGINE_IO_BUFFER_T[n]` **不做初始化**，尚未分配到的下标里是
  不定值 → `FreeIo` 会拿垃圾指针去 `AX_SYS_MemFree`。

因此一并修了 **A-4**（`models/io.cpp`）：

1. 新增 `middleware::ResetIo()`：`delete[]` 两个数组并 `memset` 清零 `io_data`。
2. `PrepareIo` 的两条失败路径在 `FreeIoIndex` 之后调用 `ResetIo`，
   保证失败返回时 `io_data` 是干净的空状态。
3. 数组分配改为 `new AX_ENGINE_IO_BUFFER_T[n]()`（零初始化），消除对不定值的依赖。
4. `FreeIo` 末尾也调用 `ResetIo`，使其可安全重复调用。

**顺带对齐官方 sample 的一处：** `PrepareIo` 原先只给 output 设
`buffer->nSize = meta.nSize`，**input 漏了**（官方 ax-samples 两者都设）。
现已补上。说明一下：我无法证明这在当前 SDK 上导致了实际故障
（`AX_ENGINE_RunSync` 是否读取 input 侧的 `nSize` 未知），
但结合第 3 点，此前该字段是不定值——补上后至少不再依赖未定义内容。

---

## - [x] P2-5 · SCRFD 未校验输出 tensor 数量即按下标访问

> **已修复（2026-08-24）。** Postprocess 入口校验
> `nOutputSize == kFmc * 3`（=9）并额外判空 `pOutputs`，不符即 `LOG_ERROR` 返回 -1。

**位置:** `models/scrfd.cpp:142-148`
**严重程度:** medium

```cpp
auto outs = GetOutput().pOutputs;
for (int idx = 0; idx < kFmc; ++idx) {
    float* scores = static_cast<float*>(outs[idx].pVirAddr);
    float* bboxes = static_cast<float*>(outs[idx + kFmc].pVirAddr);
    float* kps    = static_cast<float*>(outs[idx + kFmc * 2].pVirAddr);
```

假定至少 9 路输出（3 score + 3 bbox + 3 kps），没有校验 `nOutputSize >= 9`。
换成不带关键点的模型（如 `scrfd_*_bnkps` 之外的变体）就会越界读。

**修法（已实施）:** 在 `models/scrfd.cpp` 的 Postprocess 循环之前加：

```cpp
constexpr AX_U32 kExpectedOutputs = kFmc * 3;
const auto* info = GetInfo();
if (info == nullptr || info->nOutputSize != kExpectedOutputs) { LOG_ERROR(...); return -1; }
auto outs = GetOutput().pOutputs;
if (outs == nullptr) { LOG_ERROR(...); return -1; }
```

换成不带关键点或层数不同的模型时，现在会明确报「模型与后处理不匹配」
而不是越界读 `pOutputs` 数组。

---

## - [x] P2-6 · YOLOv5：stride 硬编码 + buffer 尺寸校验用了两次 `letterbox_cols`

> **已修复（2026-08-24）。** 修复时发现**第三个**同源问题：
> `letterbox_cols` 与 `letterbox_rows` 的赋值本身就是反的。详见末尾「修复记录」。

**位置:** `models/yolov5.cpp:49-50`、`:52-54`
**严重程度:** medium（当前 640×640 配置不触发）

```cpp
// yolov5.cpp:49-50
// int32_t stride = strides[i];
int32_t stride = (1 << i) * 8;          // 忽略了配置里的 strides

// yolov5.cpp:52-54
size_t countSize =
    (letterbox_cols / stride) * (letterbox_cols / stride) *   // 两次 cols，没用 rows
        (labels.size() + 5) * 3 * sizeof(float);
```

第一处：默认 `{8,16,32}` 时 `(1<<i)*8` 碰巧相等，但改 YAML 就错。
第二处：非正方形输入（如 640×384）时校验结果错误，而
`generate_proposals_yolov5` 内部没有边界保护，存在**静默越界读**。

> 注：当前人脸主链路走 SCRFD，YOLOv5 是另一条路径。优先级可以放低，
> 但如果之后要用 YOLOv5 跑非正方形输入，这两条必须先修。

### 修复记录

**新发现的第三处：`cols` / `rows` 赋值颠倒。**

```cpp
// 原实现
int letterbox_cols = config_.inputs[2];   // inputs[2] 是 H！
int letterbox_rows = config_.inputs[3];   // inputs[3] 是 W！
```

`inputs` 是 NCHW（配置为 `[1, 3, 640, 640]`），`inputs[2]=H`、`inputs[3]=W`——
`models/scrfd.cpp:126` 那句「修正输入宽高索引 (inputs[2]=H, inputs[3]=W)」的注释
就是证据，说明有人在 SCRFD 里改对过，但 YOLOv5 这边没同步。
640×640 时两者相等所以看不出来。已改为 `rows = inputs[2]`、`cols = inputs[3]`。

**三处一并修好：**

1. `stride` 改回 `strides[i]`，不再用 `(1 << i) * 8`。
2. `countSize` 的第二个因子改为 `(letterbox_rows / stride)`。
   该式必须与下游 `generate_proposals_yolov5`（`detection.h:738-739`）的实际
   读取量一致——它按 `feat_w = cols/stride`、`feat_h = rows/stride` 遍历，
   每格 3 anchor、每 anchor `cls_num + 5` 个 float。
3. `cols` / `rows` 赋值对调回来。

**额外加的配置校验：** 查 `detection.h:741-746` 发现下游是用 stride 反推
`anchor_group` 的（8→1、16→2、32→3），**其余取值会让 `anchor_group` 保持未初始化**
并越界读 anchors；它同时把每层 anchor 数硬编码为 3。既然第 1 点让 stride 真正
跟随配置了，就必须挡住配置越界的情况，否则等于把静默越界读的入口交给了 YAML。
现在 Postprocess 会校验每层 `stride ∈ {8,16,32}` 且 `num_anchors[i] == 3`，
不符即报错返回 -1。

> 当前人脸主链路走 SCRFD，YOLOv5 是另一条路径，所以这几处此前都没被触发。

---

## - [x] P2-7 · `PreProcess` 对未创建的线程调用 `pthread_join`

> **已随 P0-2 修复（2026-08-24）。** `ffmpeg_thread_` 改零初始化，新增
> `std::atomic<bool> ffmpeg_started_` 仅在 `pthread_create` 成功后置位，
> `StopSources()` 按标志决定是否 join。`pthread_create` 的返回值也不再被忽略。
> 同类问题在 `VdecHelper` 侧一并处理（新增 `recv_started_`）。

**位置:** `stages/pre_process.h:148`（初值）、`:38`（join）
**严重程度:** medium

```cpp
// pre_process.h:148
pthread_t ffmpeg_thread_ = -1;
// pre_process.h:38（析构中，无条件执行）
pthread_join(ffmpeg_thread_, nullptr);
```

`pthread_t` 在 glibc 上是 `unsigned long`，`-1` 会变成 `ULONG_MAX`。
若 `Init`/`Start` 未成功、或从未收到 `kMsgAppStart`（`Start()` 里才 `pthread_create`），
析构仍会拿这个非法 id 去 join，属未定义行为。

**修法:** 加 `std::atomic<bool> ffmpeg_started_{false}`，仅在 `pthread_create` 成功后置位，
析构时按标志位决定是否 join。

---

# 五、附录：low（可选，顺手处理）

## - [x] A-1 · 清理不可达的 `exit(0)` 与 `system("pause")` —— 已全部完成

**第一批（随 P1-2）:** `tracker/bytetrack/utils.cpp` 的 `extend_cost=false` 分支改为
放弃本帧匹配，两处 `system("pause")` 删除。

**第二批（本次）:** `tracker/bytetrack/BytekalmanFilter.cpp` 的
`gating_distance(..., only_position=true)` 未实现分支。

改为 `LOG_ERROR` + 返回**填满 `FLT_MAX` 的矩阵**，而不是空矩阵或 0。理由是语义方向：
调用方拿本函数的返回值与 `chi2inv95` 阈值比较来决定是否关联，**极大值会让全部候选
被拒绝**（安全），而返回 0 会让所有候选都通过门限，造成错误关联（危险）。
降级方向选错比不降级更糟。

至此项目内已无 `exit()` / `abort()` / `system("pause")` 残留于库代码路径。

## - [x] A-2 · `Copy2Host` 返回值被忽略

> **已修复（2026-08-24）。** `stages/pre_process.h` 的 `Preprocess()` 现在检查
> `Copy2Host` 返回值，失败即 `LOG_ERROR` 并返回错误码，不再把空的/不完整的
> `data->data` 发往下游。

与 `ivps_->Process` 的既有错误处理保持同一模式（同函数内上方几行）。
返回值经 `Process()` 透传给 `TaskNodeMgr`，会被记为 process error。

**修完之后这条的价值比初版描述更高：** P1-4 让 `InfProcess` 的失败开始计入
故障统计并输出「持续失败/已恢复」区间日志。若这里继续放行坏数据，下游
`PushInput` 的尺寸校验会把它报成**推理失败**并计入 InfProcess 的故障计数——
根因在 PreProcess 的映射失败，却记在了 InfProcess 头上。两条修在一起才自洽。

## - [x] A-3 · `next_thread_id_` 未校验

> **已修复（2026-08-24）。** 三个节点的 `Init()` 都加了 `next_thread_id_ < 0`
> 校验，失败即 `LOG_ERROR` 并返回非零，启动阶段直接失败。

**比初版多修了一处：** 文档原先只列了 `pre_process.h` 与 `inf_process.h`，
但 `stages/bus_process.h`（查找 `EncProcess`）是完全相同的模式，一并修了。
漏掉它的话，症状会是「识别正常但画面到不了编码器」，更难定位。

三处分别是：

- `PreProcess::Init` → 查找 `InfProcess`，失败返回 -3
- `InfProcess::Init` → 查找 `BusProcess`，失败返回 -1
- `BusProcess::Init` → 查找 `EncProcess`，失败返回 -1

`Init()` 返回非零后 `TaskNodeMgr` 会置 `kError`、`app.Start()` 失败、
`main` 走 `ExitPipeline` 退出，配上这里的 `LOG_ERROR` 就能直接看出是哪个
节点名对不上。

> 时序上是安全的：`TaskScheduler::Start` 先为所有节点建好 TaskNodeMgr
> 注册名字，之后才 `CreateThread` 并跑 `Init`（`task_scheduler.cpp:81-82`
> 的注释亦说明了这一点），所以拼写正确的名字不会被误判为失败。

## - [x] A-4 · `middleware::PrepareIo` 失败路径未 `delete[]` IO 数组

> **已随 P2-4 修复（2026-08-24）。** 它不只是泄漏——脏的 `io_data` 会让
> 后续 `FreeIo` 二次释放 CMM、并对 `new[]` 的不定值调用 `AX_SYS_MemFree`。
> 因此它是 P2-4 的**前置依赖**，必须先修。新增 `ResetIo()`、数组改零初始化、
> 失败路径复位 `io_data`、`FreeIo` 变为可重复调用。详见 P2-4 的修复记录。

## - [x] A-5 · SORT 实现中的越界风险（死代码）—— 已归档隔离

> **已处理（2026-08-24）:** 源码移至 `tmp/tracker_sort/`（**仍受 git 跟踪，未删除**），
> 并从 `CMakeLists.txt` 的 `include_directories` 移除 `tracker/sort`。
> 缺陷本身未修——代码不参与编译，复活前再修即可，修法已写入
> `tmp/tracker_sort/README.md`。

### 核查中的一个意外发现

**这套代码此前就已经不参与编译了。** `CMakeLists.txt` 里只有
`aux_source_directory(tracker/bytetrack TRACKER_SRCS)`，**从来没有** sort 对应的一行。
所以初版说的「当前为死代码」比预想的更彻底：那些越界缺陷根本没被链接进产物。

但当时 `tracker/sort` 仍留在 `include_directories`（原 `CMakeLists.txt:41`），
形成一个「能 `#include` 进来、却链接不上」的半开状态——真正需要关掉的是这个。

### 具体动作

1. `tracker/sort/*` → `tmp/tracker_sort/`。用普通 `mv` 而非 `git mv`，
   避免污染暂存区；`.gitignore` 中没有 `tmp/`，故文件仍受跟踪。
2. `CMakeLists.txt` 移除 `include_directories` 里的 `tracker/sort`，
   彻底断掉误引用可能。
3. `stages/bus_process.h` 中那行被注释的 `#include "sort_track.h"`
   改为指向新位置的说明性注释。
4. 新增 `tmp/tracker_sort/README.md`：记录停用原因、两条待修缺陷的位置与修法、
   以及复活步骤。特别注明「加回编译」不是恢复而是**新增**
   `aux_source_directory`（因为原先从未有过），且
   `tracker/sort/utils.cpp` 与 `tracker/bytetrack/utils.cpp` **同名**，
   同时编译时需确认不会产生目标文件名冲突。

已重新 `cmake ..` 并全量重建（35/35）验证无残留引用。

---

# 六、已修复（存档，无需处理）

## 本次会话修复

- **Qdrant 检索同步阻塞热路径** — 整块（Search/判定/Upsert/JPEG/推送）搬进线程池，
  新增 `identify_inflight` 状态 + 结果回传队列，`track_pending_` 保持单线程读写。
  `stages/bus_process.h`
- **`push_ivps_` 多 worker 并发无保护** — 新增 `push_ivps_mutex_` 串行化硬件调用，
  网络部分仍并行。`stages/bus_process.h`
- **`feat_done` 过早置位** — 改为检索成功回传后才置位，失败可重试。
- **`VdecHelper::Write` 缺长度校验** — 补 `data_size > buf_size_` 拦截 + 错误日志，
  防止越界覆写相邻 CMM pool。`hal/ax/vdec_helper.cpp`
- **`identify_inflight` 永久卡死导致 `track_pending_` 无界增长** — 重构中发现并修复：
  所有不进线程池的分支都 `RollbackInflight`，另加 `kTrackInflightMaxFrames` 超时闸。

## 你自己修复（`hal/image_data.cpp` 重构）

- **`Copy2Mat` 映射泄漏** — 改为收非 const 引用并统一走 `EnsureMapped`。
- **`Clone` 的 const 语义与映射生命周期** — 统一 `EnsureMapped` / `Unmap` 约定。
- **`Clone` 分配失败时会 free 源帧物理地址** — 继承 `FrameInfo` 后立刻清零地址。
- **mmap 部分失败留下半映射状态** — `EnsureMapped` 增加回滚。
- **`kMemIdSys` 帧被误 mmap** — 显式拦截并报错（否则 mmap 地址会被当作 `MemFree` 入参）。

---

# 七、生产上线前处理（详见 `TODO.md`）

以下为测试阶段可接受、上线前必须调整的项，明细在 `TODO.md` 的
「生产上线前必须处理」分区：

- FaceServer API Key 硬编码为结构体默认值，导致「key 未配置」检查永不触发
- Qdrant 无鉴权 + 明文 HTTP 传输人脸图与特征向量（含向量库可写→报警可被绕过的风险，
  以及切 HTTPS 时 `http_client.cpp` 缺 `CURLOPT_CAINFO` 的坑）
- RTSP 账号密码硬编码在 `apps/main_rtsp.cpp:83`
- `test_bank_id` / `test_org_id` / `test_camera` / `stat_id` 等占位值
- InfProcess 与 BusProcess 各自调用全局 `AX_ENGINE_Init` / `AX_ENGINE_Deinit`
  （重复 Init 实测无影响，仅退出期 Deinit 顺序需观察）
