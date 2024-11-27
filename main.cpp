#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <condition_variable>
#include <functional>

class Counter
{
public:
  virtual void increment() = 0;
  virtual int get_value() const = 0;
  virtual void reset() = 0;
  virtual ~Counter() {}
};

class MutexCounter : public Counter
{
private:
  int value;
  mutable std::mutex mtx;

public:
  MutexCounter() : value(0) {}

  void increment() override
  {
    std::lock_guard<std::mutex> lock(mtx);
    ++value;
  }

  int get_value() const override
  {
    std::lock_guard<std::mutex> lock(mtx);
    return value;
  }

  void reset() override
  {
    std::lock_guard<std::mutex> lock(mtx);
    value = 0;
  }
};

class AtomicCounter : public Counter
{
private:
  std::atomic<int> value;

public:
  AtomicCounter() : value(0) {}
  void increment() override
  {
    ++value;
  }
  int get_value() const override
  {
    return value.load();
  }
  void reset() override
  {
    value.store(0);
  }
};

class SpinlockCounter : public Counter {
private:
    int value;
    mutable std::atomic_flag lock = ATOMIC_FLAG_INIT;
public:
    SpinlockCounter() : value(0) {}
    
    void increment() override {
        while (lock.test_and_set(std::memory_order_acquire));
        ++value;
        lock.clear(std::memory_order_release);
    }
    
    int get_value() const override {
        while (lock.test_and_set(std::memory_order_acquire));
        int val = value;
        lock.clear(std::memory_order_release);
        return val;
    }
    
    void reset() override {
        while (lock.test_and_set(std::memory_order_acquire));
        value = 0;
        lock.clear(std::memory_order_release);
    }
};

class Timer
{
private:
  std::chrono::high_resolution_clock::time_point start_time;

public:
  void start()
  {
    start_time = std::chrono::high_resolution_clock::now();
  }
  double elapsed() const
  {
    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(end_time - start_time).count();
  }
};

void run_test(Counter &counter, int num_threads, int num_increments, int delay_ms = 0, bool random_delay = false)
{
  counter.reset();
  Timer timer;
  std::vector<std::thread> threads;
  std::atomic<bool> stop_flag(false);

  auto task = [&](int tid)
  {
    std::mt19937 rng(tid);
    std::uniform_int_distribution<int> delay_dist(1, delay_ms);
    for (int i = 0; i < num_increments; ++i)
    {
      counter.increment();
      if (delay_ms > 0)
      {
        int delay = random_delay ? delay_dist(rng) : delay_ms;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
      }
    }
  };

  timer.start();
  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back(task, i);
  }
  for (auto &t : threads)
  {
    t.join();
  }
  double time_taken = timer.elapsed();
  int final_value = counter.get_value();

  std::cout << "Threads: " << num_threads << ", Increments: " << num_increments
            << ", Time: " << time_taken << "s, Value: " << final_value
            << ", Correct: " << (final_value == num_threads * num_increments ? "Yes" : "No") << std::endl;
}

int main()
{
  constexpr int NUM_INCREMENTS = 100;

  MutexCounter mutex_counter;
  AtomicCounter atomic_counter;
  SpinlockCounter spinlock_counter;

  std::cout << "Testing MutexCounter:" << std::endl;
  run_test(mutex_counter, 2, NUM_INCREMENTS, 100);
  run_test(mutex_counter, 8, NUM_INCREMENTS, 0);
  run_test(mutex_counter, 4, NUM_INCREMENTS, 100, true);

  std::cout << "Testing AtomicCounter:" << std::endl;
  run_test(atomic_counter, 2, NUM_INCREMENTS, 100);
  run_test(atomic_counter, 8, NUM_INCREMENTS, 0);
  run_test(atomic_counter, 4, NUM_INCREMENTS, 100, true);

  std::cout << "Testing SpinlockCounter:" << std::endl;
  run_test(spinlock_counter, 2, NUM_INCREMENTS, 100);
  run_test(spinlock_counter, 8, NUM_INCREMENTS, 0);
  run_test(spinlock_counter, 4, NUM_INCREMENTS, 100, true);

  return 0;
}
