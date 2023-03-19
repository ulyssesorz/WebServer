#include"epoller.h"

Epoller::Epoller(int maxEvent) : m_epollerFd(epoll_create(512)), m_events(maxEvent)
{
    assert(m_epollerFd >= 0 && m_events.size() > 0);
}

Epoller::~Epoller()
{
    close(m_epollerFd);
}

bool Epoller::addFd(int fd, uint32_t events)
{
    if(fd < 0)
        return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return epoll_ctl(m_epollerFd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool Epoller::modFd(int fd, uint32_t events)
{
    if(fd < 0)
        return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return epoll_ctl(m_epollerFd, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool Epoller::delFd(int fd)
{
    if(fd < 0)
        return false;
    epoll_event ev = {0};
    return epoll_ctl(m_epollerFd, EPOLL_CTL_DEL, fd, &ev) == 0;
}

int Epoller::wait(int timeout)
{
    return epoll_wait(m_epollerFd, &m_events[0], static_cast<int>(m_events.size()), timeout);
}

int Epoller::getEventFd(size_t i) const
{
    assert(i < m_events.size() && i >= 0);
    return m_events[i].data.fd;
}

uint32_t Epoller::getEvents(size_t i) const
{
    assert(i < m_events.size() && i >= 0);
    return m_events[i].events;    
}