#pragma once

#include <condition_variable>
#include <future>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>

class ThreadPool {
private:
    std::atomic_bool active;
    std::vector<std::thread> thread_pool;
    std::queue<std::function<void()>> job_queue;
    std::mutex pool_lock;
    std::mutex log_lock;
    std::condition_variable cv;
    void run();
public:
    explicit ThreadPool(int num_threads = 1);
    ~ThreadPool();
    template <typename Function, typename... Args>
    auto post(Function&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    void start();
    void stop();
    void resize(int num_threads);
    bool busy();
};
