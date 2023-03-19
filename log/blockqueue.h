#pragma once
#include<mutex>
#include<condition_variable>
#include<deque>
#include<sys/time.h>

template<class T>
class BlockQueue
{
public:
    explicit BlockQueue(size_t MaxCapacity = 1000);
    ~BlockQueue();

public:
    void clear();
    void close();
    bool empty();
    bool full();
    size_t size();
    size_t capacity();
    T front();
    T back();
    void push_back(const T& item);
    void push_front(const T& item);
    bool pop(T& item);
    bool pop(T& item, int timeout);
    void flush();

private:
    std::deque<T> m_dq;
    size_t m_capacity;
    bool m_isClosed;
    std::mutex m_mutex;
    std::condition_variable m_condConsumer;
    std::condition_variable m_condProducer;
};

template<class T>
BlockQueue<T>::BlockQueue(size_t Maxcapacity) : m_capacity(Maxcapacity)
{
    assert(m_capacity > 0);
    m_isClosed = false;
}

template<class T>
BlockQueue<T>::~BlockQueue()
{
    close();
}

template<class T>
void BlockQueue<T>::close()
{
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_dq.clear();
        m_isClosed = true;
    }
    m_condProducer.notify_all();    //通知所有阻塞线程队列已关闭，停止等待
    m_condConsumer.notify_all();
}

template<class T>
void BlockQueue<T>::clear()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_dq.clear();
}

template<class T>
void BlockQueue<T>::flush()
{
    m_condConsumer.notify_one();
}

template<class T>
T BlockQueue<T>::front()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_dq.front();
}

template<class T>
T BlockQueue<T>::back()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_dq.back();
}

template<class T>
size_t BlockQueue<T>::size()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_dq.size();
}

template<class T>
size_t BlockQueue<T>::capacity()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_capacity;
}

template<class T>
bool BlockQueue<T>::full()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_dq.size() >= m_capacity;   
}

template<class T>
bool BlockQueue<T>::empty()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_dq.empty();   
}

template<class T>
void BlockQueue<T>::push_back(const T& item)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    while(m_dq.size() >= m_capacity)    //队满等待
    {
        m_condProducer.wait(locker);
    }
    m_dq.push_back(item);
    m_condConsumer.notify_one();    //唤醒一个消费者
}

template<class T>
void BlockQueue<T>::push_front(const T& item)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    while(m_dq.size() >= m_capacity)
    {
        m_condProducer.wait(locker);
    }
    m_dq.push_front(item);
    m_condConsumer.notify_one();
}

template<class T>
bool BlockQueue<T>::pop(T& item)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    while(m_dq.empty()) //队空阻塞
    {
        m_condConsumer.wait(locker);
        if(m_isClosed)
            return false;
    }
    item = m_dq.front();
    m_dq.pop_front();
    m_condProducer.notify_one();
    return true;
}

template<class T>
bool BlockQueue<T>::pop(T& item, int timeout)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    while(m_dq.empty())
    {
        //设置超时时间
        if(m_condConsumer.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout)
            return false;
        if(m_isClosed)
            return false;
    }
    item = m_dq.front();
    m_dq.pop_front();
    m_condProducer.notify_one();
    return true;
};