#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <chrono>
#include <string>
#include <thread>
#include <iomanip>
#include "mpir.h"

#include "threadsafe_queue.h"

extern void p171();
extern int sp17();

void TestBigInt()
{
  mpz_t a,b,c;
  mpz_init_set_str(a, "123456789101112131415161718192F", 16);
  mpz_init_set_str(b, "8765432108988878685848382818079", 10);
  mpz_init(c);
  //mpz_add(c, a, b);
  //floor div
  mpz_fdiv_r(c, b, a);
  int strSize = mpz_sizeinbase(c, 10) + 2;
  std::string buf(strSize, 0);
  mpz_get_str(&(buf[0]), 10, c);
  std::cout << buf << "\n";
}

bool running;
struct Task {
  std::chrono::steady_clock::time_point t0, t1;
};
threadsafe_queue<Task> q;

void WorkerFun()
{  
  while (running) {
    Task task;
    bool success = q.wait_and_pop(task, 50);
    if (success) {
      task.t1 = std::chrono::steady_clock::now();
      std::cout << "got task ";
      std::cout << (task.t1 - task.t0).count() / 1000000.0 << " ms\n";
    }
  }
}

void TestThreadSafeQueue()
{
  running = true;
  std::thread workerThread(&WorkerFun);
  for (int i = 0; i < 100; i++) {
    if (!q.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    else {
      Task t;
      t.t0 = std::chrono::steady_clock::now();
      q.push(t);
    }
  }

  running = false;
  workerThread.join();
}

int main(int argc, char * argv[]) {
  //TestBigInt();
  //TestThreadSafeQueue();
  sp17();
  return 0;
}
