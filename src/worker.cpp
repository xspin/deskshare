#include "worker.hpp"

Worker::Worker() : _thread(work, this) {
}

Worker::~Worker() {
}

void Worker::work(Worker *self) {
}

void Worker::start() {
    _thread.join();
}
