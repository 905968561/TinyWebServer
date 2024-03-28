#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>

class ThreadPool{
public:
    ThreadPool()=default;
    ThreadPool(ThreadPool&&)=default;//定义右值引用，来转移资源的所有权
                                     //，我认为是在线程池当中需要销毁的线程去用到下个线程中，避免频繁销毁
    explicit ThreadPool(int threadCount=8):pool_(std::make_shared<Pool>()){//这里就是一个右值的传递，显示申请了一个右值引用再传递给pool
        assert(threadCount>0);
        for (int i = 0; i < threadCount; i++)
        {   //通过detach建立多个线程，保证线程池一直运行
            std::thread([this](){
                std::unique_lock<std::mutex> locker(pool_->mtx_);
                while(true){
                    if(!pool_->tasks.empty()){
                        auto task=std::move(pool_->tasks.front());
                        pool_->tasks.pop();
                        locker.unlock(); //这里任务已经取出来了，所以要放开锁，让其他线程去取任务了
                        task();//这里只要把this指针传入进来就可以全局运行task
                        locker.lock();//任务执行完了，现在又要去取任务了，所以这里的临界资源其实是连接任务
                    }else if(pool_->isClosed){//和后面析构函数对应，但关闭的时候线程直接break。
                        break;
                    }else {//任务池为空的情况,就等待被唤醒
                        pool_->cond_.wait(locker);
                    }
                }
            }).detach();
        }
    }

    ~ThreadPool(){
        if(pool_){
            std::unique_lock<std::mutex> locker(pool_->mtx_);
            pool_->isClosed=true;
        }
        pool_->cond_.notify_all();//唤醒前面的条件变量，然后循环中发现已经isClosed，直接break
    }

    template<typename T>
    void AddTask(T&& task){
        std::unique_lock<std::mutex> locker(pool_->mtx_);
        pool_->tasks.emplace(std::forward<T>(task));
        //forward 完美转发，保持了task的右值属性，这里使用emplace，直接就用右值传递，不用再内存中多申请一块内存了
        //emplace 直接在队列中构造，因为传入的是右值，所以就直接右值转移了 比insert性能要好一点，因为insert新创建一个对象再copy
        pool_->cond_.notify_one();//这个和前面任务池为空时等待呼应
    }

private:
    struct Pool{
        std::mutex mtx_;
        std::condition_variable cond_;
        bool isClosed;
        std::queue<std::function<void()>> tasks;// 任务队列，函数类型为void ，就是可以为任意类型
    };
    std::shared_ptr<Pool> pool_;
};

#endif