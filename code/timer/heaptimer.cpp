//
// Created by 90596 on 2024/3/28.
//

#include "heaptimer.h"

//交换小根堆中的节点
void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i>=0 && i<heap_.size());
    assert(j>=0 && j<heap_.size());
    swap(heap_[i],heap_[j]);
    ref_[heap_[i].id]=i;
    ref_[heap_[j].id]=j;
}

//上移节点
void HeapTimer::siftup_(size_t i) {
    assert(i>=0 && i<heap_.size());
    size_t parent=(i-1)/2;//找到父节点的索引
    while(parent>=0){
        if(heap_[parent]>heap_[i]){
            SwapNode_(i,parent);
            i=parent;
            parent=(i-1)/2;
        }else{
            break;
        }
    }
}

// 下滑节点 true 下滑成功 false 不需要下滑
bool HeapTimer::siftdown_(size_t i, size_t n) {
    assert(i >= 0 && i < heap_.size());
    assert(n >= 0 && n <= heap_.size());    // n:共几个结点
    auto index=i;
    auto child=2*index+1;
    while(child<n){
        if(child+1<n && heap_[child+1]< heap_[child]){
            child++;//看孩子右节点存在不
        }
        if(heap_[child]<heap_[index]){
            SwapNode_(index,child);
            index=child;
            child=2*child+1;
        }
    }
    return i<index;
}

//删除指定位置的节点
void HeapTimer::del_(size_t index) {
    assert(index >= 0 && index < heap_.size());
    size_t tmp=index;
    size_t n=heap_.size()-1;
    assert(tmp<=n);
    //把需要删除的节点和最后节点交换，因为最后节点是最不重要的，所以不影响目前堆的可用性
    if(index<heap_.size()-1){
        SwapNode_(tmp,heap_.size()-1);
        // 交换了之后要对原来位置的数据进行调整，要么下滑，要么上移
        if(!siftdown_(tmp,n)){
            siftup_(tmp);
        }
    }
    //删除节点信息
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

// 调整节点的过期时间
void HeapTimer::adjust(int id, int newExpires) {
    assert(!heap_.empty() &&ref_.count(id));
    heap_[ref_[id]].expires=Clock::now()+MS(newExpires);
    siftdown_(ref_[id],heap_.size());
}

void HeapTimer::add(int id, int timeOut, const TimeoutCallBack &cb) {
    assert(id>=0);
    //如果存在则adjust
    if(ref_.count(id)){
        int tmp=ref_[id];
        heap_[tmp].expires=Clock ::now()+MS(timeOut);
        heap_[tmp].cb=cb;
        if(!siftdown_(tmp,heap_.size())){
            siftup_(tmp);
        }
    }else {
        size_t n=heap_.size();
        ref_[id]=n;
        heap_.push_back({id,Clock::now()+MS(timeOut),cb});//右值传递
        siftup_(n);
    }
}

//删除指定id，并触发回调函数
void HeapTimer::doWork(int id) {
    if (heap_.empty() || ref_.count(id)==0){
        return ;
    }
    size_t i=ref_[id];
    auto node =heap_[i];
    node.cb();
    del_(i);
}

//检测时间堆顶任务是否超时,超时则调用callback
void HeapTimer::tick() {
    if(heap_.empty()){
        return ;
    }
    while(!heap_.empty()){
        TimerNode node =heap_.front();
        //计算当前时间 Clock::now() 与节点的到期时间 node.expires 之间的差值，并将其duration_cast<MS>转换为毫秒
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0){
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);//删除堆顶元素
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    tick();//消费掉当前堆顶
    size_t res=-1;
    if(!heap_.empty()){
        //std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now())这个只是获取了对应的类型
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res<0){res=0;}
    }
    return res;
}