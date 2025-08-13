#include "ThreadPool.h"

void ThreadPool::workerLoop() {
  while (1) {
    std::shared_ptr<TPTask> task;
    int ret;
    ret = popTask(task);
    if (ret < 0) {
      // no more tasks. done.
      break;
    }
    if (task != nullptr) {
      task->run();
    }
  }
}

unsigned ThreadPool::GetProcessorCount() {
  return std::thread::hardware_concurrency();  
}

int ThreadPool::run() {
  std::lock_guard lock(busyLock);
  unsigned numThreads = userNumThreads;
  if (userNumThreads > 0) {
    numThreads = userNumThreads;
  } else {
    unsigned int processorCount = GetProcessorCount();
    if (processorCount < 3) {
      numThreads = 1;
    } else {
      numThreads = processorCount - 2;
    }
  }
  // check any unfinished tasks
  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  threads.resize(numThreads);
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i] = std::thread(&ThreadPool::workerLoop, this);
  }

  // check any unfinished tasks
  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  return 0;
}

int ThreadPool::popTask(TaskPtr& task) {
  std::lock_guard<std::mutex> lock(queueLock);
  if (taskQueue.empty()) {
    return -1;
  }
  task = taskQueue.front();
  taskQueue.pop();
  return 0;
}

std::vector<Range> divideRange(unsigned nTasks, unsigned nProc) {
  if (nTasks == 0) {
    return {};
  }
  if (nProc == 0) {
    nProc = 1;
  }
  if (nProc > 1000) {
    nProc = 1000;
  }
  unsigned chunkSize = nTasks / nProc + (nTasks % nProc > 0);
  std::vector<Range> chunks;
  for (unsigned i = 0; i < nProc; i++) {
    Range r;
    r.begin() = i * chunkSize;
    r.end() = (i + 1) * chunkSize;
    if (r.end() > nTasks) {
      r.end() = nTasks;
    }
    chunks.push_back(r);
    if (r.end() >= nTasks) {
      break;
    }
  }
  return chunks;
}