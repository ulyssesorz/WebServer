#include"log.h"

Log::Log()
{
    m_lineCount = 0;
    m_today = 0;
    m_isAsync = false;
    m_writeThread = nullptr;
    m_bq = nullptr;
    m_fp = nullptr;
}

Log::~Log()
{
    if(m_writeThread && m_writeThread->joinable())
    {
        //取走队列剩余数据
        while(!m_bq->empty())
        {
            m_bq->flush();
        }
        m_bq->close();
        m_writeThread->join();
    }
    if(m_fp)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        flush();
        fclose(m_fp);
    }
}

Log* Log::instance()
{
    static Log inst;
    return &inst;
}

int Log::getLevel()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_level;
}

void Log::setLevel(int level)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_level = level;
}

bool Log::isOpen()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_isOpen;
}

//判断是否异步，设置当前时间
void Log::init(int level, const char* path, const char* suffix, int maxCapacity)
{
    m_isOpen = true;
    m_level = level;
    //有阻塞队列说明是异步的
    if(maxCapacity > 0)
    {
        m_isAsync = true;
        if(!m_bq)
        {
            //新建阻塞队列
            std::unique_ptr<BlockQueue<std::string>> newQueue(new BlockQueue<std::string>);
            m_bq = move(newQueue);

            //新建写线程
            std::unique_ptr<std::thread> newThread(new std::thread(flushLogThread));
            m_writeThread = move(newThread);
        }
    }
    else
    {
        m_isAsync = false;
    }

    m_lineCount = 0;
    time_t timer = time(nullptr);
    struct tm* systime = localtime(&timer);
    struct tm t = *systime;
    m_path = path;
    m_suffix = suffix;
    char filename[LOG_NAME_LEN] = {0};
    //记录日志日期，tm的年份从1900开始算，月份从0开始算
    snprintf(filename, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", m_path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, m_suffix);
    m_today = t.tm_mday;

    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_buffer.initPtr();
        //文件已打开，刷新并关闭
        if(m_fp)
        {
            flush();
            fclose(m_fp);
        }
        m_fp = fopen(filename, "a");//追加
        if(m_fp == nullptr)
        {
            mkdir(m_path, 0777);//新建并设置权限
            m_fp = fopen(filename, "a");
        }
        assert(m_fp);
    }
}

void Log::write(int level, const char* format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tsec = now.tv_sec;
    struct tm *systime = localtime(&tsec);
    struct tm t = *systime;
    va_list valist;

    //日志文件不是今天，或者文件已写满，需新建日志文件
    if(m_today != t.tm_mday || (m_lineCount && (m_lineCount % MAX_LINES == 0)))
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        locker.unlock();
        char newfile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        //新的一天新的日志
        if(m_today != t.tm_mday)
        {
            snprintf(newfile, LOG_NAME_LEN - 72, "%s%s%s", m_path, tail, m_suffix);
            m_today = t.tm_mday;
            m_lineCount = 0;
        }
        //今天的日志已满
        else
        {
            snprintf(newfile, LOG_NAME_LEN - 72, "%s%s-%d%s", m_path, tail, (m_lineCount / MAX_LINES), m_suffix);           
        }

        locker.lock();
        flush();
        fclose(m_fp);
        m_fp = fopen(newfile, "a");
        assert(m_fp);
    }

    {
        std::unique_lock<std::mutex> locker(m_mutex);
        m_lineCount++;
        int n = snprintf(m_buffer.curWritePtr(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ", t.tm_year + 1900,
        t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        m_buffer.updateWritePtr(n);
        appendTitle(level);

        va_start(valist, format);
        int m = vsnprintf(m_buffer.curWritePtr(), m_buffer.writeableBytes(), format, valist);
        va_end(valist);

        m_buffer.updateWritePtr(m);
        m_buffer.append("\n\0", 2);

        //异步日志，从阻塞队列取
        if(m_isAsync && m_bq && !m_bq->full())
        {
            m_bq->push_back(m_buffer.alltoStr());
        }
        //同步方式
        else
        {
            fputs(m_buffer.curReadPtr(), m_fp);
        }
        m_buffer.initPtr();
    }
}

//添加日志消息类型头
void Log::appendTitle(int level)
{
    switch(level)
    {
        case 0:
            m_buffer.append("[debug]: ", 9);
            break;
        case 1:
            m_buffer.append("[info]:  ", 9);
            break;
        case 2:
            m_buffer.append("[warn]:  ", 9);
            break;
        case 3:
            m_buffer.append("[error]: ", 9);
            break;
        default:
            m_buffer.append("[info]:  ", 9);
            break;
    }
}

//唤醒写线程
void Log::flush()
{
    if(m_isAsync)
        m_bq->flush();
    fflush(m_fp);
}

//把队列中的数据写入日志
void Log::asyncWrite()
{
    std::string str = "";
    while(m_bq->pop(str))
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        fputs(str.c_str(), m_fp);
    }
}

//写日志线程的回调函数
void Log::flushLogThread()
{
    Log::instance()->asyncWrite();
}