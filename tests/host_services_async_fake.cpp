#include "host_services.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

struct AsyncState
{
    std::mutex mutex;
    std::condition_variable cv;
    std::unordered_map<std::string, int> inflight;
    std::vector<std::thread> threads;
};

std::mutex g_states_mutex;
std::unordered_map<const HostServices*, std::unique_ptr<AsyncState>> g_states;

std::mutex g_block_mutex;
std::condition_variable g_block_cv;
bool g_task_started = false;
bool g_task_released = false;

AsyncState& State(const HostServices* host)
{
    std::lock_guard<std::mutex> lock(g_states_mutex);
    return *g_states.at(host);
}

} // namespace

HostServices::HostServices()
{
    std::lock_guard<std::mutex> lock(g_states_mutex);
    g_states.emplace(this, std::make_unique<AsyncState>());
}

HostServices::~HostServices()
{
    auto& state = State(this);
    for (auto& thread : state.threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    std::lock_guard<std::mutex> lock(g_states_mutex);
    g_states.erase(this);
}

bool HostServices::SubmitAsync(const std::string& plugin_name,
                               std::function<void()> fn)
{
    auto& state = State(this);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        ++state.inflight[plugin_name];
    }
    state.threads.emplace_back([&state, plugin_name, fn = std::move(fn)] {
        fn();
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            --state.inflight[plugin_name];
        }
        state.cv.notify_all();
    });
    return true;
}

bool HostServices::WaitQuiesce(const std::string& plugin_name, int timeout_ms)
{
    auto& state = State(this);
    std::unique_lock<std::mutex> lock(state.mutex);
    const auto idle = [&] { return state.inflight[plugin_name] == 0; };
    if (timeout_ms < 0)
    {
        state.cv.wait(lock, idle);
        return true;
    }
    return state.cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), idle);
}

extern "C" void AxTestResetAsyncTask()
{
    std::lock_guard<std::mutex> lock(g_block_mutex);
    g_task_started = false;
    g_task_released = false;
}

extern "C" void AxTestBlockAsyncTask()
{
    std::unique_lock<std::mutex> lock(g_block_mutex);
    g_task_started = true;
    g_block_cv.notify_all();
    g_block_cv.wait(lock, [] { return g_task_released; });
}

extern "C" bool AxTestWaitAsyncTaskStarted(int timeout_ms)
{
    std::unique_lock<std::mutex> lock(g_block_mutex);
    return g_block_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                               [] { return g_task_started; });
}

extern "C" void AxTestReleaseAsyncTask()
{
    std::lock_guard<std::mutex> lock(g_block_mutex);
    g_task_released = true;
    g_block_cv.notify_all();
}
