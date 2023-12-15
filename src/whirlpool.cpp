#include "../include/whirlpool.hpp"
#include <future>

ThreadPool::ThreadPool(int num_threads) {
    for (int i = 0; i < num_threads; i++) {
        std::thread t(&ThreadPool::run, this);
        thread_pool.push_back(t);
    }
}

ThreadPool::~ThreadPool() {
    active = false;
    cv.notify_all();
    for (int i = 0; i < thread_pool.size(); i++) {
        thread_pool[i].join();
    }
}

void ThreadPool::post(std::packaged_task<void()> task) {
    std::unique_lock lock(pool_lock);
    job_queue.emplace(task);
    cv.notify_one();
}

void ThreadPool::run() {
    while(active) {
        std::unique_lock lock(pool_lock);
        cv.wait(lock, [&] { return !job_queue.empty() || !active; });
        if(!active) break;
        // Get next job from queue
        std::packaged_task<void()> job;
        job.swap(job_queue.front());
        job_queue.pop();
    }
}
