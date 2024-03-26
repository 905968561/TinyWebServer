# ifndef BLOCKQUEUE_H
# define BLOCKQUEUE_H

#include <deque>
#include <condition_variable>
#include <mutex>
#include <sys/time.h>
using namespace std;

template<typename T>
class BlockQueue{
    explicit BlockQueue(size_t maxsize=1000);
    ~BlockQueue();
    bool empty();
    bool full();
    void push_back(const T& item);
    void push_front(const T& item); 
    bool pop(T& item);  // 弹出的任务放入item
    bool pop(T& item, int timeout);  // 等待时间
    void clear();
    T front();
    T back();
    size_t capacity(); //容量
    size_t size();

    void flush(); //唤醒全部消费者
    void Close();
private:
    deque<T> deq_;                      // 底层数据结构
    mutex mtx_;                         // 锁
    bool isClose_;                      // 关闭标志
    size_t capacity_;                   // 容量
    condition_variable condConsumer_;   // 消费者条件变量
    condition_variable condProducer_;   // 生产者条件变量
};


template<typename T>
BlockQueue<T>::BlockQueue(size_t maxsize):capacity_(maxsize){
    assert(maxsize>0);
    isClose_=false;
};

template<typename T>
BlockQueue<T>::~BlockQueue(){
    Close();
};

template<typename T>
void BlockQueue<T>::Close(){
    clear();
    isClose_=true;
    condConsumer_.notify_all();
    condProducer_.notify_all();
};

template<typename T>
void BlockQueue<T>::clear() {
    lock_guard<mutex> locker(mtx_); //上锁操作
    deq_.clear();
}

template<typename T>
bool BlockQueue<T>::empty() {
    lock_guard<mutex> locker(mtx_); //上锁操作,也需要上锁，万一就是在中间一瞬间插入信息了呢
    return deq_.empty();
}

template<typename T>
bool BlockQueue<T>::full() {
    lock_guard<mutex> locker(mtx_); //上锁操作,也需要上锁，万一就是在中间一瞬间拿走信息了呢
    return deq_.size()>=capacity_;
}


template<typename T>
void BlockQueue<T>::push_back(const T& item) {
    unique_lock<mutex> locker(mtx_);//管理锁
    while(deq_.size()>=capacity_){ //队列满了，之后等待消费
        condProducer_.wait(locker);//暂停生产，等待消费和唤醒，同时wait会释放锁
    }
    deq_.push_back(item);
    condConsumer_.notify_one(); //唤醒一个消费者
}

template<typename T>
void BlockQueue<T>::push_front(const T& item) { //和上面一样只是pushfront
    unique_lock<mutex> locker(mtx_);//管理锁
    while(deq_.size()>=capacity_){ //队列满了，之后等待消费
        condProducer_.wait(locker);//暂停生产，等待唤醒，同时wait会释放锁，这里不能主动唤醒
    }
    deq_.push_back(item);
    condConsumer_.notify_one(); //唤醒一个消费者
}

template<typename T>
bool BlockQueue<T>::pop(T& item) { //消费
    unique_lock<mutex> locker(mtx_);//管理锁
    while(deq_.empty()){ //队列空，之后等待生产
        condConsumer_.wait(locker);//暂停消费，等待生产和唤醒，同时wait会释放锁
    }
    item=deq_.front();
    deq_.pop_front();
    condProducer_.notify_one(); //唤醒一个消费者
    return true;
}

template<typename T>
bool BlockQueue<T>::pop(T& item,int timeout) { //带时间要求的消费
    unique_lock<mutex> locker(mtx_);//管理锁
    while(deq_.empty()){ //队列空，之后等待生产
        if(condConsumer_.wait_for(locker,std::chrono::seconds(timeout))==std::cv_status::timeout){ //时间要求到了
            return false;
        }
        if (isClose_)
        {
            return false;
        }
        
    }
    item=deq_.front();
    deq_.pop_front();
    condProducer_.notify_one(); //唤醒一个消费者
}


template<typename T>
T BlockQueue<T>::front(){
    lock_guard<mutex> locker(mtx_); 
    return deq_.front();
};

template<typename T>
T BlockQueue<T>::back(){
    lock_guard<mutex> locker(mtx_); 
    return deq_.back();
};

template<typename T>
size_t BlockQueue<T>::capacity(){
    lock_guard<mutex> locker(mtx_); 
    return capacity_;
};

template<typename T>
size_t BlockQueue<T>::size(){
    lock_guard<mutex> locker(mtx_); 
    return deq_.size();
};

template<typename T>
void BlockQueue<T>::flush(){
    condConsumer_.notify_one(); //唤醒消费者，保证全部消费完毕
};
# endif