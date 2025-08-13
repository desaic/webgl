///\file ThreadPool.h
/// a simple multithreading class

#include <memory>
#include <mutex>
#include <queue>
#include <thread>

// thread pool task
class TPTask {
 public:
  virtual void run() = 0;
  virtual ~TPTask() = default;
};

class ThreadPool {
 public:
  using TaskPtr = std::shared_ptr<TPTask>;
  ThreadPool() {}

  void addTask(TaskPtr task) {
    std::lock_guard<std::mutex> lock(queueLock);
    taskQueue.push(task);
  }

  ///\param task if queue is non empty,
  /// a TPTask pointer is assigned to task.
  int popTask(TaskPtr& task);

  /// runs all the tasks using mutiple threads and blocks until all threads
  /// finish. if run is called from multiple threads, they are executed one at a
  /// time.
  int run();
  // if 0, use total num CPU threads-2.
  // max is 1000;
  void setUserNumThreads(unsigned n) {
    if (n > 1000) {
      n = 1000;
    }
    userNumThreads = n;
  }

  unsigned GetProcessorCount();

 private:
  std::queue<TaskPtr> taskQueue;
  std::mutex queueLock;
  std::vector<std::thread> threads;
  std::mutex busyLock;

  /// function run by each worker
  void workerLoop();
  // use user config num threads instead auto
  unsigned userNumThreads = 0;
};

struct Range {
  unsigned i0 = 0, i1 = 0;
  Range() {}
  Range(unsigned a, unsigned b) : i0(a), i1(b) {}
  unsigned begin() const { return i0; }
  unsigned end() const { return i1; }
  unsigned& begin() { return i0; }
  unsigned& end() { return i1; }
};

std::vector<Range> divideRange(unsigned nTasks, unsigned nProc);