#pragma once
#include<queue>
#include<deque>
#include<unordered_map>
#include<ctime>
#include<chrono>
#include<functional>
#include<memory>
#include<assert.h>
#include"../http/httpconnection.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode
{
    int id;
    TimeStamp expire;
    TimeoutCallBack cb;
    
    TimerNode(int i, TimeStamp e, TimeoutCallBack c) : id(i), expire(e), cb(c) {}
    bool operator<(const TimerNode& rhs)
    {
        return expire < rhs.expire;
    }
    bool operator>(const TimerNode& rhs)
    {
        return expire > rhs.expire;
    }
};

class TimerManager
{
public:
    TimerManager() {m_heap.reserve(64);}
    ~TimerManager() {clear();}

public:
    void addTimer(int id, int timeout, const TimeoutCallBack& cb);
    void handleExpiredEvent();
    int getNextHandle();

    void update(int id, int timeout);
    void work(int id);
    void pop();
    void clear();

private:
    void del(size_t i);
    void shiftup(size_t i);
    bool shiftdown(size_t index, size_t n);
    void swapNode(size_t i, size_t j);

    std::vector<TimerNode> m_heap;    //用vector维护小根堆i（为了随机存取）
    std::unordered_map<int, size_t> m_hash;   //从计时器id到堆序号的哈希表
};