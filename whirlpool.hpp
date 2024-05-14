#pragma once

#include <condition_variable>
#include <future>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <future>
#include <functional>
#include <iostream>

#define LOG_FILE "time.log"


class ThreadPool {
private:
  std::atomic_bool active;
  std::vector<std::thread> thread_pool;
  std::queue<std::function<void()>> job_queue;
  std::mutex pool_lock;
  std::mutex log_lock;
  std::condition_variable cv;
  
  // Logic for each thread in thread_pool.
  void run() {
    while(active) {
      // Create local unique lock from the shared pool_lock mutex.
      std::unique_lock<std::mutex> lock(pool_lock);
      // Wait until:
      // 1. Condition variable is notified.
      // 2. The unique lock is initialized.
      // 3. The job queue is not empty.
      // 4. The active flag is set.
      cv.wait(lock, [&] { return !job_queue.empty() || !active; });
      if(active) {
        // Get the function from the font of the queue.
        // Use move to transfer ownership of function from job_queue to current thread.
        auto job = std::move(job_queue.front());
        job_queue.pop();
        lock.unlock();
        // This macro displays the job time in seconds to cout.
#ifdef THREAD_TIMER
        {
          // Calculate job exectution time using chrono::high_resolution_clock.
          std::chrono::duration<double> thread_time(0);
          auto start_time = std::chrono::high_resolution_clock::now();
          job();
          thread_time = std::chrono::high_resolution_clock::now() - start_time;
          // Use lock guard to prevent threads from overwriting each other.
          std::lock_guard<std::mutex> time_lock(log_lock);
          // Write thread time to log.
          std::ofstream log(LOG_FILE, std::ios::app);
          if (log.is_open()) {
            log << "Job took: " << thread_time.count() * 1000 << " seconds.\n";
          }
          log.close();
        }
        // If macro is not defined, execute job without timer logic.
#else
        job();
#endif
      }
      else {
        return;
      }
    }
  }
  
public:
  // Parameterized constructor
  ThreadPool(int num_threads) {
    active = true;
    for (int i = 0; i < num_threads; i++) {
      thread_pool.emplace_back(&ThreadPool::run, this);
    }
  }

  ~ThreadPool() { this->stop(); }

  // Template to define a generic function address and dynamic number of arguments.
  template <typename Function, typename... Args>
  // Post function using the generic function/arg template.
  auto post(Function&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    // Get return type of Function parameter.
    using ReturnType = decltype(f(args...));

    // Create packaged task using function
    auto func_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(
                                                                                 std::bind(std::forward<Function>(f), std::forward<Args>(args)...));

    auto func_wrapper = [func_ptr]()  {
      (*func_ptr)();
    };

    // Use new scope for unique lock.
    {
      std::unique_lock<std::mutex> lock(pool_lock);
      job_queue.push(func_wrapper);
      cv.notify_all();
    }

    // Get and return task future
    auto future = func_ptr->get_future();
    return future;
  }

  // Start the thread pool:
  // 1. Set the active flag.
  // 2. Notify threads via condition variable.
  void start() {
    active = true;
    cv.notify_all();
  }

  // Stop the thread pool:
  // 1. Set the active flag to false.
  // 2. Notify threads using condition variable.
  void stop() {
    // Use new scope for unique lock
    {
      std::unique_lock<std::mutex> lock(pool_lock);
      active = false;
      cv.notify_all();
    }

    for (std::thread &t : thread_pool) {
      if (t.joinable()) t.join();
      else std::cout << "Thread not joinable." << std::endl;
    }
  }

  // Resize the thread_pool vector given a new_size parameter.
  void resize(int num_threads) {
    // Make sure thread pool is not busy.
    if (this->busy()) {
      this->stop();
    }

    // Clear thread_pool.
    thread_pool = std::vector<std::thread>();

    // Add threads to thread_pool.
    for (int i = 0; i < num_threads; i++) {
      thread_pool.emplace_back(&ThreadPool::run, this);
    }
  }

  // Return the active flag.
  bool busy() {
    return active;
  }
};
