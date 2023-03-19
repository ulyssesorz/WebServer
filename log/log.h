#pragma once
#include<mutex>
#include<thread>
#include<string>
#include<sys/time.h>
#include<sys/stat.h>
#include<stdarg.h>
#include<assert.h>
#include"blockqueue.h"
#include"../buffer/buffer.h"

class Log
{
public:
    void init(int level = 1, const char* path = "./log", const char* suffix = ".log", int maxCapacity = 1024);
    static Log*  instance();
    static void flushLogThread();

    void write(int level, const char* format, ...);
    void flush();

    int getLevel();
    void setLevel(int level);
    bool isOpen();

private:
    Log();
    ~Log();
    void appendTitle(int level);
    void asyncWrite();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char* m_path;
    const char* m_suffix;

    int m_lineCount;
    int m_today;
    bool m_isOpen;
    int m_level;
    bool m_isAsync;
    Buffer m_buffer;

    FILE* m_fp;
    std::unique_ptr<BlockQueue<std::string>> m_bq;
    std::unique_ptr<std::thread> m_writeThread;
    std::mutex m_mutex;
};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::instance();\
        if (log->isOpen() && log->getLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

//四种日志级别
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);