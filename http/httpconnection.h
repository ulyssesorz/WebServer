#pragma once
#include<arpa/inet.h>
#include<sys/uio.h>
#include<iostream>
#include<sys/types.h>
#include<assert.h>
#include"../buffer/buffer.h"
#include"../log/log.h"
#include"httprequest.h"
#include"httpresponse.h"

class HttpConnection
{
public:
    HttpConnection();
    ~HttpConnection();

public:
    //处理http连接
    void initHttpConn(int fd, const sockaddr_in& addr);
    void closeHttpConn();
    bool handleHttpConn();

    //读写socket
    ssize_t readBuffer(int* Errno);
    ssize_t writeBuffer(int* Errno);

    //获取数据
    const char* getIp() const;
    int getPort() const;
    int getFd() const;
    sockaddr_in getAddr() const;

    int writeBytes() const
    {
        return m_iov[0].iov_len +  m_iov[1].iov_len;
    }

    bool isKeepAlive() const
    {
        return m_request.isKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<size_t> userCount;

private:
    int m_fd;
    struct sockaddr_in m_addr;
    bool m_isClosed;

    int m_iovCnt;
    struct iovec m_iov[2];

    Buffer m_readBuffer;
    Buffer m_writeBuffer;

    HttpRequest m_request;
    HttpResponse m_response;
};
