#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <string.h>

// static sigset_t g_waitset;

// static void *request_channel_id_func(void *argv)
// {
//     int *value = (int *)argv;
//     do
//     {
//         siginfo_t info{0x0};

//         memset(&info, 0x0, sizeof(siginfo_t));

//         printf("wait signo\n");
//         int signo;
//         do
//         {
//             signo = sigwaitinfo(&g_waitset, &info);
//         } while (signo == -1 && errno == EINTR);

//         // printf("收到信号: %d\n", signo);
//         std::cout << "read signo: " << signo << " value:" << info.si_value.sival_int << " from:" << info.si_pid << std::endl;
//         if (signo == -1)
//         {
//             perror("sigwaitinfo");
//             // LOG_SYS_ERROR("sigwaitinfo");
//             usleep(100);
//             continue;
//         }

//         int resource_id = info.si_value.sival_int;
//         *value = resource_id;
//         std::cout << "Resouce ID:" << resource_id << std::endl;

//     } while (false);

//     return nullptr;
// }

// int request_channel_id(int resource_type, pid_t main_pid)
// {
//     int channelId = -1;
//     sigemptyset(&g_waitset);
//     sigaddset(&g_waitset, SIGUSR1);
//     pthread_sigmask(SIG_BLOCK, &g_waitset, nullptr);
//     pthread_t signalThread;
//     pthread_create(&signalThread, nullptr, request_channel_id_func, &channelId);

//     // pid_t mypid = getpid();
//     union sigval val;
//     val.sival_int = resource_type;
//     sigqueue(main_pid, SIGUSR1, val);

//     pthread_join(signalThread, nullptr);

//     return channelId;
// }

enum ResourceType{
    RES_IVPS_ID = 1,
    RES_VDEC_ID = 2,
    RES_VENC_ID = 3
};

class ReqResourceID
{
private:
    sigset_t waitset_;
    int resource_id_ = -1;

    static void *RequestThread(void *argv)
    {
        ReqResourceID *self = (ReqResourceID *)argv;
        do
        {
            siginfo_t info{0x0};

            memset(&info, 0x0, sizeof(siginfo_t));

            printf("wait signo\n");
            int signo;
            do
            {
                signo = sigwaitinfo(&self->waitset_, &info);
            } while (signo == -1 && errno == EINTR);

            // printf("收到信号: %d\n", signo);
            std::cout << "read signo: " << signo << " value:" << info.si_value.sival_int << " from:" << info.si_pid << std::endl;
            if (signo == -1)
            {
                perror("sigwaitinfo");
                // LOG_SYS_ERROR("sigwaitinfo");
                usleep(100);
                continue;
            }

            int resource_id = info.si_value.sival_int;
            self->resource_id_ = resource_id;
            std::cout << "Resouce ID:" << resource_id << std::endl;

        } while (false);

        return nullptr;
    }

    pid_t GetMainPid() {
        // 读取那个文件
        return 0;
    }

public:
    ReqResourceID() {};

    int Request(ResourceType type)
    {
        resource_id_ = -1;
        pid_t pid = GetMainPid();

        sigemptyset(&waitset_);
        sigaddset(&waitset_, SIGUSR1);
        pthread_sigmask(SIG_BLOCK, &waitset_, nullptr);
        pthread_t signalThread;
        pthread_create(&signalThread, nullptr, RequestThread, this);

        union sigval val;
        val.sival_int = type;
        sigqueue(pid, SIGUSR1, val);

        pthread_join(signalThread, nullptr);

        return resource_id_;
    };

    ~ReqResourceID() {};
};
