#include "../include/whirlpool.hpp"
#include <future>
#include <functional>
#include <chrono>
#include <fstream>

#define LOG_FILE "time.log"

ThreadPool::ThreadPool(int num_threads) {
    active = true;
    this.resize(num_threads);
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
        std::unique_lock<std::mutex> lock(pool_lock);
        cv.wait(lock, [&] { return !job_queue.empty() || !active; });
        // Get job if thread pool is still active
        if(active) {
            auto job = std::move(job_queue.front());
            job_queue.pop();
            lock.unlock();
            // THREAD_TIMER macro displays job time in seconds via std::cout;
            #ifdef THREAD_TIMER
            {
                std::chrono::duration<double> thread_time(0);
                auto start_time = std::chrono::high_resolution_clock::now();
                job();
                thread_time = std::chrono::high_resolution_clock::now() - start_time;
                std::lock_guard<std::mutex> time_lock(log_lock);
                std::ofstream log(LOG_FILE, std::ios::app);
                if (log.is_open()) {
                    log << "Job took: " << thread_time.count() * 1000 << " seconds.\n";
                }
                log.close();
            }
            #else
            job();
            #endif
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

void ThreadPool::start() {
    active = true;
    cv.notify_all();
}

bool ThreadPool::busy() {
    return active;
}

// Resize the thread_pool vector given a new_size parameter.
void ThreadPool::resize(int num_threads) {
    // Make sure thread pool is not busy.
    if (!this.busy()) {
        this.stop();
    }

    // Clear thread_pool.
    thread_pool.clear();

    // Add threads to thread_pool.
    for (int i = 0; i < num_threads; i++) {
        thread_pool.emplace_back(&ThreadPool::run, this);
    }
}
