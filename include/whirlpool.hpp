#pragma once

#include <functional>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>

class ThreadPool {
private:
  std::vector<std::thread> threads;
  std::mutex queue_lock;
  std::queue<std::function<void()>> job_queue;
public:
  void QueueJob();
  void Start();
  void Stop();
  bool Busy();
};
