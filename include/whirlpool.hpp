#pragma once

#include <condition_variable>
#include <future>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>

class ThreadPool {
private:
    std::atomic_bool active { true };
    std::vector<std::thread> thread_pool;
    std::queue<std::packaged_task<void()>> job_queue;
    std::mutex pool_lock;
    std::condition_variable cv;
    void run();
public:
    explicit ThreadPool(int num_threads = 1);
    ~ThreadPool();
    void post(std::packaged_task<void()>);
    void stop();
    bool busy();
};
