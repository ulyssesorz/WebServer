#pragma once
#include<sys/epoll.h>
#include<fcntl.h>
#include<unistd.h>
#include<assert.h>
#include<vector>
#include<errno.h>
#include<string.h>

class Epoller
{
public:
    explicit Epoller(int maxEvent = 1024);
    ~Epoller();

public:
    //封装epoll_ctl,添加、修改、删除fd
    bool addFd(int fd, uint32_t events);
    bool modFd(int fd, uint32_t events);
    bool delFd(int fd);
    //封装epoll_wait,返回就绪fd的数目
    int wait(int timeout = -1);
    //返回就绪fd
    int getEventFd(size_t i) const;
    //返回事件表
    uint32_t getEvents(size_t i) const;

private:
    //epoll事件表
    int m_epollerFd;
    //存储就绪事件
    std::vector<struct epoll_event> m_events;
};
