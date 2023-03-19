#include<thread>
#include<condition_variable>
#include<mutex>
#include<queue>
#include<functional>
#include<assert.h>

class ThreadPool
{
public:
    //构造函数，把队列里的任务取出并执行
    explicit ThreadPool(size_t threadNumber = 8) : m_pool(std::make_shared<Pool>())
    {
        assert(threadNumber > 0);
        for(size_t i = 0; i < threadNumber; i++)
        {
            //起一个线程，传入lambda表达式
            std::thread([pool = m_pool]  
            {
                std::unique_lock<std::mutex> locker(pool->mtx);//防竞争，上锁
                while(1)
                {
                    if(!pool->tasks.empty())
                    {
                        auto task = std::move(pool->tasks.front());//从队列中取出一个任务
                        pool->tasks.pop();
                        locker.unlock();//获取锁
                        task();
                        locker.lock();//释放锁
                    }
                    else if(pool->isClosed) //线程池终止时通知所有线程退出执行
                    {
                        break;
                    }
                    else
                    {
                        pool->cond.wait(locker);//任务队列为空，且线程池未关闭，则线程阻塞等待
                    }
                }
            }).detach();//分离线程
        }
    }

    //禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool()
    {
        if(static_cast<bool>(m_pool))
        {
            {
                std::lock_guard<std::mutex> locker(m_pool->mtx);//加锁保护，作用域结束自动解锁
                m_pool->isClosed = true;
            }
            m_pool->cond.notify_all();//通知所有线程，isClosed已被修改
        }
    }

public:
    template<typename T>
    void addTask(T&& task)  //万能引用
    {
        {
            std::lock_guard<std::mutex> locker(m_pool->mtx);
            m_pool->tasks.emplace(std::forward<T>(task)); //完美转发
        }
        m_pool->cond.notify_one();    //队列中加入了新任务，唤醒阻塞线程
    }

private:
    struct Pool
    {
        bool isClosed;
        std::mutex mtx;
        std::condition_variable cond;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> m_pool;
};