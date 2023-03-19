#include"webserver.h"

WebServer::WebServer(int port, int trigMode, int timeout, bool optLinger, int threadNumber, 
bool openLog, int logLevel, int logSize) : 
    m_timer(new TimerManager()), m_threadpool(new ThreadPool(threadNumber)), m_epoller(new Epoller())
{
    assert(m_srcDir);
    m_port = port;
    m_timeout = timeout;
    m_openLinger = optLinger;
    m_isClosed = false;
    m_srcDir = getcwd(nullptr, 256);
    strncat(m_srcDir, "/resources/", 16);
    HttpConnection::userCount = 0;
    HttpConnection::srcDir = m_srcDir;
    initEvenMode(trigMode);
    if(!initSocket())
        m_isClosed = true;
    if(openLog)
    {
        Log::instance()->init(logLevel, "./log", ".log", logSize);
        if(m_isClosed) 
        { 
            LOG_ERROR("========== Server init error =========="); 
        }
        else 
        {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", m_port, optLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (m_listenEvent & EPOLLET ? "ET": "LT"),
                            (m_connEvent & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConnection::srcDir);
        }
    }
}

WebServer::~WebServer()
{
    close(m_listenFd);
    m_isClosed = true;
    free(m_srcDir);
}

bool WebServer::initSocket()
{
    int ret;
    struct sockaddr_in addr;
    if(m_port > 65535 || m_port < 1024)
    {
        LOG_ERROR("Port:%d error!",  m_port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);
    struct linger optLinger;
    optLinger = {0};
    //设置优雅关闭，close时等待一段时间把剩余数据发送完
    if(m_openLinger)
    {
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }    
    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_listenFd < 0)
    {
        LOG_ERROR("Create socket error!", m_port);
        return false;
    }
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0)
    {
        close(m_listenFd);
        LOG_ERROR("Init linger error!", m_port);
        return false;
    }

    //设置端口复用，避免timewait时占用端口
    int optval = 1;
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1)
    {
        LOG_ERROR("set socket setsockopt error !");
        close(m_listenFd);
        return false;
    }

    //绑定监听套接字
    ret = bind(m_listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0)
    {
        LOG_ERROR("Bind Port:%d error!", m_port);
        close(m_listenFd);
        return false;
    }

    //监听端口
    ret = listen(m_listenFd, 5);
    if(ret < 0)
    {
        LOG_ERROR("Listen port:%d error!", m_port);
        close(m_listenFd);
        return false;
    }

    //把监听套接字注册到epoll事件表里
    ret = m_epoller->addFd(m_listenFd, m_listenEvent | EPOLLIN);
    if(ret == 0)
    {
        LOG_ERROR("Add listen error!");
        close(m_listenFd);
        return false;
    }

    //监听套接字非阻塞
    setFdNonblock(m_listenFd);
    LOG_INFO("Server port:%d", m_port);
    return true;
}

//设置epoll事件的属性
void WebServer::initEvenMode(int trigMode)
{
    m_listenEvent = EPOLLRDHUP;
    //避免多个线程同时操作一个socket
    m_connEvent = EPOLLRDHUP | EPOLLONESHOT;
    //设置ET模式
    switch(trigMode)
    {
        case 0:
            break;
        case 1:
            m_connEvent |= EPOLLET;
            break;
        case 2:
            m_listenEvent |= EPOLLET;
            break;
        default:
            m_listenEvent |= EPOLLET;
            m_connEvent |= EPOLLET;
            break;
    }
    HttpConnection::isET = (m_connEvent & EPOLLET);
}

//新建连接
void WebServer::addConnection(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    //初始化连接信息
    m_users[fd].initHttpConn(fd, addr);
    //设置定时器
    if(m_timeout > 0)
    {
        m_timer->addTimer(fd, m_timeout, std::bind(&WebServer::closeConnection, this, &m_users[fd]));
    }
    //注册到事件表
    m_epoller->addFd(fd, EPOLLIN | m_connEvent);
    setFdNonblock(fd);
    LOG_INFO("Client[%d] in!", m_users[fd].getFd());
}

//关闭连接
void WebServer::closeConnection(HttpConnection* client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    m_epoller->delFd(client->getFd());
    client->closeHttpConn();
}

void WebServer::sendError(int fd, const char* info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0)
    {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

//重置定时器
void WebServer::extentTime(HttpConnection* client)
{
    assert(client);
    if(m_timeout > 0)
    {
        m_timer->update(client->getFd(), m_timeout);
    }
}

//处理监听套接字，触发后就建立新连接
void WebServer::handleListen()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do
    {
        int fd = accept(m_listenFd, (struct sockaddr*)&addr, &len);
        if(fd <= 0)
        {
            return;
        }
        else if(HttpConnection::userCount >= MAX_FD)
        {
            sendError(fd, "Server busy");
            LOG_WARN("Clients is full!");
            return;
        }
        addConnection(fd, addr);
    }while(m_listenEvent & EPOLLET);    //监听需要持续进行
}

//处理读行为，加入到线程池中，线程调用onread函数
void WebServer::handleRead(HttpConnection* client)
{
    assert(client);
    extentTime(client);
    m_threadpool->addTask(std::bind(&WebServer::onRead, this, client));
}

void WebServer::handleWrite(HttpConnection* client)
{
    assert(client);
    extentTime(client);
    m_threadpool->addTask(std::bind(&WebServer::onWrite, this, client));
}

//线程进行读
void WebServer::onRead(HttpConnection* client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->readBuffer(&readErrno);    //读取数据
    if(ret <= 0 && readErrno != EAGAIN)     //连接关闭
    {
        closeConnection(client);
        return;
    }
    onProcess(client);
}

void WebServer::onWrite(HttpConnection* client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->writeBuffer(&writeErrno);
    //没有写入新的数据
    if(client->writeBytes() == 0) 
    {
        //如果是长连接，不断开
        if(client->isKeepAlive())
        {
            onProcess(client);
            return;
        }
    }
    else if(ret < 0)
    {
        //缓冲区满了,继续传输
        if(writeErrno == EAGAIN)
        {
            m_epoller->modFd(client->getFd(), m_connEvent | EPOLLOUT);
            return;
        }
    }
    closeConnection(client);
}

void WebServer::onProcess(HttpConnection* client)
{
    //已经有http请求，可写
    if(client->handleHttpConn())
    {
        m_epoller->modFd(client->getFd(), m_connEvent | EPOLLOUT);
    }
    //无http请求，可读
    else
    {
        m_epoller->modFd(client->getFd(), m_connEvent | EPOLLIN);
    }
}

int WebServer::setFdNonblock(int fd)
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void WebServer::start()
{
    int timeMS = -1;//epoll默认一直阻塞
    if(!m_isClosed)
    {
        std::cout << "================";
        std::cout << "  Server Start  ";
        std::cout << "================";
        std::cout << std::endl;
        LOG_INFO("========== Server Start ==========");
    }
    while(!m_isClosed)
    {
        if(m_timeout > 0)
        {
            timeMS = m_timer->getNextHandle();//最小超时时间
        }
        int eventCnt = m_epoller->wait(timeMS);//等到计时结束关闭连接还没触发就退出等待
        for(int i = 0; i < eventCnt; i++)
        {
            //获取触发的fd和event
            int fd = m_epoller->getEventFd(i);
            uint32_t events = m_epoller->getEvents(i);

            //有新连接
            if(fd == m_listenFd)
            {
                handleListen();
            }
            //对端已关闭连接
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                assert(m_users.count(fd));
                closeConnection(&m_users[fd]);
            }
            //读
            else if(events & EPOLLIN)
            {
                assert(m_users.count(fd));
                handleRead(&m_users[fd]);
            }
            //写
            else if(events & EPOLLOUT)
            {
                assert(m_users.count(fd));
                handleWrite(&m_users[fd]);
            }
            else
            {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}