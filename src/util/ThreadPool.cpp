#include "ThreadPool.h"
ThreadPool::ThreadPool(int threadCount):stop(false){
    for(int i=0;i<threadCount;i++){
        workers.emplace_back([this]{
            while(1){
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    cv.wait(lock,[this]{
                        return stop||!tasks.empty();
                    });
                    if(stop&&tasks.empty()) return;
                    task=std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

template<typename T,typename... Args>
auto ThreadPool::enqueue(T&& f, Args&&... args)->std::future<typename std::invoke_result<T,Args...>::type>{
    using returntype=typename std::invoke_result<T,Args...>::type;
    auto task=std::make_shared<std::packaged_task<returntype()>>(
        std::bind(std::forward<T>(f),std::forward<Args>(args)...)
    );
    std::future<returntype> res=task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if(stop){
            throw std::runtime_error("stop!!");
        }
        tasks.emplace([task](){(*task)();});
    }
    cv.notify_one();
    return res;
}

void ThreadPool::shutdown(){
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop=true;
    }
    cv.notify_all();
    for(std::thread &worker:workers){
        if(worker.joinable()) worker.join();
    }
    workers.clear();
}

void ThreadPool::shutdownnow(){
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop=true;
        while(!tasks.empty()) tasks.pop();
    }
    cv.notify_all();
    for(std::thread &worker:workers){
        if(worker.joinable()) worker.join();
    }
    workers.clear();
}