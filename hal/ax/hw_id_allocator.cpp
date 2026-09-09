#include "hw_id_allocator.h"

#include <cerrno>
#include <cstdint>
#include <csignal>
#include <cstring>
#include <mutex>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

#include "logger.h"

namespace {

constexpr uint32_t kMagic = 0x41584944;  // 'AXID'
constexpr uint32_t kVersion = 1;
constexpr const char* kShmName = "/ax_core_hw_id_v1";

// 与 SDK 上限对齐：AX_IVPS_MAX_GRP_NUM / AX_VDEC_MAX_GRP_NUM / MAX_VENC_CHN_NUM
constexpr int kIvpsMax = 256;
constexpr int kVdecMax = 164;
constexpr int kVencMax = 64;

struct ShmLayout {
    uint32_t magic;
    uint32_t version;
    pthread_mutex_t mu;
    uint32_t ivps_pid[kIvpsMax];
    uint32_t vdec_pid[kVdecMax];
    uint32_t venc_pid[kVencMax];
};

ShmLayout* g_shm = nullptr;

const char* KindName(HwIdKind kind)
{
    switch (kind)
    {
    case HwIdKind::kIvps:
        return "IVPS";
    case HwIdKind::kVdec:
        return "VDEC";
    case HwIdKind::kVenc:
        return "VENC";
    }
    return "?";
}

uint32_t* Slots(HwIdKind kind, int& count)
{
    switch (kind)
    {
    case HwIdKind::kIvps:
        count = kIvpsMax;
        return g_shm->ivps_pid;
    case HwIdKind::kVdec:
        count = kVdecMax;
        return g_shm->vdec_pid;
    case HwIdKind::kVenc:
        count = kVencMax;
        return g_shm->venc_pid;
    }
    count = 0;
    return nullptr;
}

bool PidAlive(uint32_t pid)
{
    if (pid == 0)
    {
        return false;
    }
    if (kill(static_cast<pid_t>(pid), 0) == 0)
    {
        return true;
    }
    return errno != ESRCH;
}

int InitMutex(pthread_mutex_t* mu)
{
    pthread_mutexattr_t attr;
    int ret = pthread_mutexattr_init(&attr);
    if (ret != 0)
    {
        return ret;
    }
    ret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (ret == 0)
    {
        ret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    }
    if (ret == 0)
    {
        ret = pthread_mutex_init(mu, &attr);
    }
    pthread_mutexattr_destroy(&attr);
    return ret;
}

bool InitLayout(ShmLayout* layout)
{
    std::memset(layout, 0, sizeof(ShmLayout));
    const int ret = InitMutex(&layout->mu);
    if (ret != 0)
    {
        LOG_ERROR("HwIdAllocator: pthread_mutex_init failed, errno={}", ret);
        return false;
    }
    layout->version = kVersion;
    layout->magic = kMagic;
    return true;
}

bool MapAndInit(int fd)
{
    if (flock(fd, LOCK_EX) != 0)
    {
        LOG_ERROR("HwIdAllocator: flock failed, errno={}", errno);
        return false;
    }

    struct stat st {};
    if (fstat(fd, &st) != 0)
    {
        LOG_ERROR("HwIdAllocator: fstat failed, errno={}", errno);
        flock(fd, LOCK_UN);
        return false;
    }

    if (st.st_size < static_cast<off_t>(sizeof(ShmLayout)))
    {
        if (ftruncate(fd, sizeof(ShmLayout)) != 0)
        {
            LOG_ERROR("HwIdAllocator: ftruncate failed, errno={}", errno);
            flock(fd, LOCK_UN);
            return false;
        }
    }

    void* addr = mmap(nullptr, sizeof(ShmLayout), PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        LOG_ERROR("HwIdAllocator: mmap failed, errno={}", errno);
        flock(fd, LOCK_UN);
        return false;
    }

    auto* layout = static_cast<ShmLayout*>(addr);
    if (layout->magic != kMagic || layout->version != kVersion)
    {
        if (!InitLayout(layout))
        {
            munmap(addr, sizeof(ShmLayout));
            flock(fd, LOCK_UN);
            return false;
        }
    }

    g_shm = layout;
    flock(fd, LOCK_UN);
    return true;
}

bool EnsureShm()
{
    static std::once_flag once;
    static bool ok = false;
    std::call_once(once, [] {
        const int fd = shm_open(kShmName, O_RDWR | O_CREAT, 0666);
        if (fd < 0)
        {
            LOG_ERROR("HwIdAllocator: shm_open({}) failed, errno={}", kShmName,
                      errno);
            return;
        }
        ok = MapAndInit(fd);
        close(fd);
        if (!ok)
        {
            g_shm = nullptr;
        }
    });
    return ok && g_shm != nullptr;
}

int LockMu()
{
    int ret = pthread_mutex_lock(&g_shm->mu);
    if (ret == EOWNERDEAD)
    {
        pthread_mutex_consistent(&g_shm->mu);
        return 0;
    }
    return ret;
}

}  // namespace

int HwIdAllocator::Acquire(HwIdKind kind)
{
    if (!EnsureShm())
    {
        return -1;
    }
    int count = 0;
    uint32_t* slots = Slots(kind, count);
    if (slots == nullptr || count <= 0)
    {
        return -1;
    }

    const int lock_ret = LockMu();
    if (lock_ret != 0)
    {
        LOG_ERROR("HwIdAllocator: mutex lock failed, errno={}", lock_ret);
        return -1;
    }

    const uint32_t self = static_cast<uint32_t>(getpid());
    int id = -1;
    for (int i = 0; i < count; ++i)
    {
        if (slots[i] != 0 && PidAlive(slots[i]))
        {
            continue;
        }
        slots[i] = self;
        id = i;
        break;
    }

    pthread_mutex_unlock(&g_shm->mu);

    if (id < 0)
    {
        LOG_ERROR("HwIdAllocator: {} IDs exhausted (max={})", KindName(kind),
                  count);
        return -1;
    }
    LOG_INFO("HwIdAllocator: acquire {} id={} pid={}", KindName(kind), id,
             self);
    return id;
}

void HwIdAllocator::Release(HwIdKind kind, int id)
{
    if (id < 0 || !EnsureShm())
    {
        return;
    }
    int count = 0;
    uint32_t* slots = Slots(kind, count);
    if (slots == nullptr || id >= count)
    {
        return;
    }

    const int lock_ret = LockMu();
    if (lock_ret != 0)
    {
        LOG_ERROR("HwIdAllocator: mutex lock failed on release, errno={}",
                  lock_ret);
        return;
    }

    const uint32_t self = static_cast<uint32_t>(getpid());
    if (slots[id] == self)
    {
        slots[id] = 0;
        LOG_INFO("HwIdAllocator: release {} id={} pid={}", KindName(kind), id,
                 self);
    }
    pthread_mutex_unlock(&g_shm->mu);
}
