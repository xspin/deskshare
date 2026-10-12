#pragma once

#include <thread>
#include <vector>

class Worker {
  public:
    Worker();
    ~Worker();

    void start();

  private:
    static void work(Worker *self);

  private:
    std::thread _thread;
    std::vector<unsigned char> _frame;
    size_t _width;
    size_t _height;
};
