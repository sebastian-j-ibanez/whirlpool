#include "../include/whirlpool.hpp"
#include <future>
#include <functional>
#include <unistd.h>

ThreadPool::ThreadPool(int num_threads) {
    active = true;
    for (int i = 0; i < num_threads; i++) {
        thread_pool.emplace_back(&ThreadPool::run, this);
    }
}

ThreadPool::~ThreadPool() {
    {
    std::unique_lock<std::mutex> lock(pool_lock);
    active = false;
    cv.notify_all();
    }

    for (auto& thread : thread_pool) {
        thread.join();    
    }
}

template <typename Function, typename... Args>
auto ThreadPool::post(Function&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using ReturnType = decltype(f(args...));
    
    // Create packaged task using function
    auto func_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(
            std::bind(std::forward<Function>(f), std::forward<Args>(args)...));
    auto func_wrapper = [func_ptr]()  { 
        (*func_ptr)();
    };

    {
        std::unique_lock<std::mutex> lock(pool_lock);
        job_queue.push(func_wrapper);
        cv.notify_one();
    }
    
    // Get task future
    auto future = func_ptr->get_future();
    return future;
}

void ThreadPool::run() {
    while(active) {
        std::unique_lock lock(pool_lock);
        cv.wait(lock, [&] { return !job_queue.empty() || !active; });
        // Get job if thread pool is still active
        if(active) {
            auto job = std::move(job_queue.front());
            job_queue.pop();
            lock.unlock();
            job();
        }
        else {
            break;
        }
    }
}

void ThreadPool::stop() {
    while(!job_queue.empty()) { }
    active = false;
    cv.notify_all();
}

bool ThreadPool::busy() {
    return active;
}
