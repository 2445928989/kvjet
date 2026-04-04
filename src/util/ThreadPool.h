#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>
class ThreadPool {
public:
    // 加入一个任务
    template <typename T, typename... Args>
    auto enqueue(T &&f, Args &&...args) -> std::future<typename std::invoke_result<T, Args...>::type> {
        using returntype = typename std::invoke_result<T, Args...>::type;
        auto task = std::make_shared<std::packaged_task<returntype()>>(
            std::bind(std::forward<T>(f), std::forward<Args>(args)...));
        std::future<returntype> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stop) {
                throw std::runtime_error("stop!!");
            }
            tasks.emplace([task]() { (*task)(); });
        }
        cv.notify_one();
        return res;
    }
    // 等待所有任务完成，停止
    void shutdown();
    // 立即停止
    void shutdownnow();
    // 析构函数
    ~ThreadPool();
    ThreadPool(int threadCount);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    bool stop;
    std::condition_variable cv;
};