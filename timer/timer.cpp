#include"timer.h"

//交换节点在vector里的位置，并改变m_hash表的映射
void TimerManager::swapNode(size_t i, size_t j)
{
    assert(i >= 0 && i < m_heap.size());
    assert(j >= 0 && j < m_heap.size());
    using std::swap;
    swap(m_heap[i], m_heap[j]);
    m_hash[m_heap[i].id] = i;
    m_hash[m_heap[j].id] = j;
}

//维护小根堆，把剩余时间小的节点上移
void TimerManager::shiftup(size_t i)
{
    assert(i >= 0 && i < m_heap.size());
    int j = (i - 1) / 2;
    while(j >= 0)
    {
        if(m_heap[j] < m_heap[i])
            break;
        swapNode(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

//向下调整，非递归实现
bool TimerManager::shiftdown(size_t index, size_t n)
{
    assert(index >= 0 && index < m_heap.size());
    assert(n >= 0 && n <= m_heap.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n)
    {
        if(j + 1 < n && m_heap[j + 1] < m_heap[j])
            j++;
        if(m_heap[j] > m_heap[i])
            break;
        swapNode(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;   //若true说明发生了向下调整
}

//删除堆元素：和最后一个节点交换，然后调整并移除
void TimerManager::del(size_t i)
{
    assert(!m_heap.empty() && i >= 0 && i < m_heap.size());
    size_t n = m_heap.size();
    if(i < n - 1)
    {
        swapNode(i, n - 1);
        if(!shiftdown(i, n - 1))
        {
            shiftup(i);
        }
    }
    m_hash.erase(m_heap.back().id);
    m_heap.pop_back();
}

//删除id节点，调用回调函数
void TimerManager::work(int id)
{
    if(m_heap.empty() || !m_hash.count(id))
        return;
    size_t i = m_hash[id];
    m_heap[i].cb();
    del(i);
}

//更新计时器的过期时间
void TimerManager::update(int id, int timeout)
{
    assert(!m_heap.empty() && m_hash.count(id));
    size_t i = m_hash[id];
    m_heap[i].expire = Clock::now() + MS(timeout);
    shiftdown(i, m_heap.size());  //过期时间只会增大，向下调整即可
}

//弹出第一个元素
void TimerManager::pop()
{
    assert(!m_heap.empty());
    del(0);
}

void TimerManager::clear()
{
    m_heap.clear();
    m_hash.clear();
}

void TimerManager::addTimer(int id, int timeout, const TimeoutCallBack& cb)
{
    assert(id >= 0);
    size_t i;
    if(m_hash.count(id))//已存在计时器堆里，更新过期时间和回调函数并调整堆
    {
        i = m_hash[id];
        m_heap[i].expire = Clock::now() + MS(timeout);
        m_heap[i].cb = cb;
        if(!shiftdown(i, m_heap.size()))
        {
            shiftup(i);
        }
    }
    else    //计时器id不存在，在m_heap后面加入并调整
    {
        i = m_heap.size();
        m_hash[id] = i;
        m_heap.emplace_back(id, Clock::now() + MS(timeout), cb);
        shiftup(i);
    }
}

//淘汰已经超时的计时器
void TimerManager::handleExpiredEvent()
{
    if(m_heap.empty())
        return;
    while(!m_heap.empty())
    {
        //小根堆堆顶
        TimerNode node = m_heap.front();
        //还没到过期时间
        if(std::chrono::duration_cast<MS>(node.expire - Clock::now()).count() > 0)
            break;
        node.cb();
        pop();          
    }
}

//下次过期还多久
int TimerManager::getNextHandle()
{
    handleExpiredEvent();
    size_t ret = -1;
    if(!m_heap.empty())
    {
        ret = std::chrono::duration_cast<MS>(m_heap.front().expire - Clock::now()).count();
        if(ret < 0)
            ret = 0;
    }
    return ret;    
}
