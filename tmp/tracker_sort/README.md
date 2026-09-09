# tracker/sort —— 已停用（保留备查，不参与编译）

> 归档日期：2026-08-24
> 原位置：`tracker/sort/`
> 停用原因：项目已改用 ByteTrack（`tracker/bytetrack/`），本实现无任何调用方。

## 为什么移到这里

这套 SORT 实现**早在归档前就已经不参与编译**——`CMakeLists.txt` 里只有
`aux_source_directory(tracker/bytetrack TRACKER_SRCS)`，从来没有 sort 对应的一行。
但当时 `tracker/sort` 仍留在 `include_directories` 中，等于留了一个「能被
`#include` 进来、却链接不上」的半开状态，而里面有确定的越界缺陷（见下）。

本次归档做了两件事：

1. 源码移到 `tmp/tracker_sort/`（**仍受 git 跟踪，未删除**）。
2. 从 `CMakeLists.txt` 的 `include_directories` 移除 `tracker/sort`，
   彻底断掉误引用的可能。

唯一的外部引用是 `stages/bus_process.h` 里一行被注释掉的 `#include "sort_track.h"`，
已改为说明性注释。

## 复活前必须先修的已知缺陷

这些问题在停用期间不会发作（代码不参与链接），但**一旦重新启用就是真的**：

### 1. 空矩阵越界（`hungarian.cpp:23-24`，`sort_track.cpp:74-79`）

当 `trk_num == 0 && det_num > 0`（例如全部 tracker 预测无效而被 erase）时，
代价矩阵为空，`DistMatrix[0]` 越界访问。

**修法：** `HungarianAlgorithm::Solve` 入口加 `nRows == 0 || nCols == 0` 早退；
调用方在矩阵为空时跳过匹配，直接为所有检测新建 tracker。

### 2. `Solve` 失败后仍访问已清空的 `assignment`（`sort_track.cpp:102-118`）

`Solve` 返回 `-1.0` 时 `assignment` 会被 `clear()`，但后续代码仍按下标
访问 `assignment[i]`。

**修法：** 检查 `Solve` 的返回值，失败时跳过整段匹配逻辑。

## 如何复活

1. 先修上面两条缺陷。
2. `CMakeLists.txt` 的 `include_directories` 加回 `tracker/sort`
   （或本目录路径）。
3. `CMakeLists.txt` 增加 `aux_source_directory(<路径> SORT_SRCS)`，
   并把 `SORT_SRCS` 加进目标源文件列表——注意原先**从未有过**这一行，
   所以这不是「恢复」而是新增。
4. 注意 `tracker/sort/utils.cpp` 与 `tracker/bytetrack/utils.cpp` 同名，
   同时编译时确认构建系统不会产生目标文件名冲突。

## 相关记录

见 `docs/code-review-2026-08-24.md` 的 A-5 条目。
