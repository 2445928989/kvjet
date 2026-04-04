#include "ThreadPool.h"
#include <iostream>
ThreadPool::ThreadPool(int threadCount) : stop(false) {
    for (int i = 0; i < threadCount; i++) {
        workers.emplace_back([this] {
            while (1) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    cv.wait(lock, [this] {
                        return stop || !tasks.empty();
                    });
                    if (stop && tasks.empty())
                        return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdownnow();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    cv.notify_all();
    for (std::thread &worker : workers) {
        if (worker.joinable())
            worker.join();
    }
    workers.clear();
}

void ThreadPool::shutdownnow() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
        while (!tasks.empty())
            tasks.pop();
    }
    cv.notify_all();
    for (std::thread &worker : workers) {
        if (worker.joinable())
            worker.join();
    }
    workers.clear();
}