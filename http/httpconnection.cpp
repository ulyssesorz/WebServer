#include"httpconnection.h"

const char* HttpConnection::srcDir;
std::atomic<size_t> HttpConnection::userCount;
bool HttpConnection::isET;

HttpConnection::HttpConnection()
{
    m_fd = -1;
    m_addr = {0};
    m_isClosed = true;
}

HttpConnection::~HttpConnection()
{
    closeHttpConn();
}

//设置http连接信息
void HttpConnection::initHttpConn(int fd, const sockaddr_in& addr)
{
    assert(fd > 0);
    userCount++;
    m_fd = fd;
    m_addr = addr;
    m_writeBuffer.initPtr();
    m_readBuffer.initPtr();
    m_isClosed = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", m_fd, getIp(), getPort(), (int)userCount);
}

void HttpConnection::closeHttpConn()
{
    m_response.unmapFile();
    if(m_isClosed == false)
    {
        m_isClosed = true;
        userCount--;
        close(m_fd);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", m_fd, getIp(), getPort(), (int)userCount);
    }
}

int HttpConnection::getFd() const
{
    return m_fd;
}

struct sockaddr_in HttpConnection::getAddr() const
{
    return m_addr;
}

const char* HttpConnection::getIp() const
{
    return inet_ntoa(m_addr.sin_addr);
}

int HttpConnection::getPort() const
{
    return m_addr.sin_port;
}

//将socket的数据读入到缓冲区
ssize_t HttpConnection::readBuffer(int* saveErrno)
{
    ssize_t len = -1;
    do
    {
        len = m_readBuffer.readFd(m_fd, saveErrno);
        if(len <= 0)
            break;
    } while(isET);
    return len;
}

//把缓冲区的http响应（状态行、头部、内容）和指定的文件从缓冲区写到socket
ssize_t HttpConnection::writeBuffer(int* saveErrno)
{
    ssize_t len = -1;
    do
    {
        len = writev(m_fd, m_iov, m_iovCnt);
        if(len <= 0)
        {
            *saveErrno = errno;
            break;
        }
        //传输结束
        if(m_iov[0].iov_len + m_iov[1].iov_len == 0)
        {
            break;
        }
        //iov[1]也参与了写,更新iov[1]的base和len
        else if(static_cast<size_t>(len) > m_iov[0].iov_len)
        {
            m_iov[1].iov_base = (uint8_t*)m_iov[1].iov_base + (len - m_iov[0].iov_len);
            m_iov[1].iov_len -= (len - m_iov[0].iov_len);
            //iov[0]是http状态信息，写完就能重置
            if(m_iov[0].iov_len)  
            {
                m_writeBuffer.initPtr();
                m_iov[0].iov_len = 0;
            }
        }
        //只有iov[0]参与写，更新
        else
        {
            m_iov[0].iov_base = (uint8_t*)m_iov[0].iov_base + len;
            m_iov[0].iov_len -= len;
            m_writeBuffer.updateReadPtr(len);
        }
    }while(isET || writeBytes() > 10240);
    return len;
}

//接收http请求，返回http应答
bool HttpConnection::handleHttpConn()
{
    m_request.init();
    //还没有http请求
    if(m_readBuffer.readableBytes() <= 0)
    {
        return false;
    }
    //解析http请求，并构造应答
    else if(m_request.parse(m_readBuffer))
    {
        LOG_DEBUG("%s", m_request.getPath().c_str());
        m_response.init(srcDir, m_request.getPath(), m_request.isKeepAlive(), 200);
    }
    //解析失败，构造失败应答400
    else
    {
        m_response.init(srcDir, m_request.getPath(), false, 400);
    }

    m_response.makeResponse(m_writeBuffer);
    //状态信息
    m_iov[0].iov_base = const_cast<char*>(m_writeBuffer.curReadPtr());
    m_iov[0].iov_len = m_writeBuffer.readableBytes();
    m_iovCnt = 1;
    //文件
    if(m_response.fileLen() > 0 && m_response.file())
    {
        m_iov[1].iov_base = m_response.file();
        m_iov[1].iov_len = m_response.fileLen();
        m_iovCnt = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", m_response.fileLen() , m_iovCnt, writeBytes());
    return true;
}