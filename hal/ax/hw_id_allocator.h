#pragma once

// 系统级 AX 硬件资源 ID（IVPS GRP / VDEC GRP / VENC CHN）。
// 进程间通过 POSIX 共享内存互斥分配，崩溃进程的槽位按 pid 回收。

enum class HwIdKind {
    kIvps = 0,
    kVdec = 1,
    kVenc = 2,
};

class HwIdAllocator {
public:
    // 领取一个空闲 ID。失败返回 -1。
    static int Acquire(HwIdKind kind);
    // 仅当槽位属于本进程时归还。
    static void Release(HwIdKind kind, int id);

    HwIdAllocator() = delete;
};
