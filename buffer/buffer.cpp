#include"buffer.h"

//writePos_和readPos_之间的数据是待读取的
size_t Buffer::readableBytes() const
{
    return m_writePos - m_readPos;
}

//writePos_后面剩下的空间是可写的
size_t Buffer::writeableBytes() const
{
    return m_buffer.size() - m_writePos;
}

//已读取的字节数
size_t Buffer::readBytes() const
{
    return m_readPos;
}

const char* Buffer::curReadPtr() const
{
    return BeginPtr() + m_readPos;
}

const char* Buffer::curWritePtrConst() const
{
    return BeginPtr() + m_writePos;
}

char* Buffer::curWritePtr()
{
    return BeginPtr() + m_writePos; 
}

//读取了len的数据，指针后移len位
void Buffer::updateReadPtr(size_t len)
{
    assert(len <= readableBytes());
    m_readPos += len;
}

//读取所有数据
void Buffer::updateReadPtrUntilEnd(const char* end)
{
    assert(end >= curReadPtr());
    updateReadPtr(end - curReadPtr());    
}

//写入了len的数据
void Buffer::updateWritePtr(size_t len)
{
    assert(len <= writeableBytes());
    m_writePos += len;
}

//数组和指针都清零
void Buffer::initPtr()
{
    bzero(&m_buffer[0], m_buffer.size());
    m_readPos = 0;
    m_writePos = 0;
}

//分配len长度的空间
void Buffer::allocateSpace(size_t len)
{
    //空闲空间（write后面和read前面）若不足len，在数组后面扩张len
    if(writeableBytes() + readBytes() < len)
    {
        m_buffer.resize(m_writePos + len + 1);
    }
    else
    {
        size_t readable = readableBytes();
        //把read和write之间待读取的数据拷贝到数组前面(避免申请新空间),更新write和read指针
        std::copy(BeginPtr() + m_readPos, BeginPtr() + m_writePos, BeginPtr());   
        m_readPos = 0;
        m_writePos = readable;
        assert(readable == readableBytes());
    }
}

//利用allocateSpace确保有len的空间可用
void Buffer::ensureWriteable(size_t len)
{
    if(writeableBytes() < len)
    {
        allocateSpace(len);
    }
    assert(writeableBytes() >= len);
} 

//保证空间足够，然后在curWritePtr后面写入字符串
void Buffer::append(const char* str, size_t len)
{
    assert(str != nullptr);
    ensureWriteable(len);
    std::copy(str, str + len, curWritePtr());
    updateWritePtr(len);
}

void Buffer::append(const std::string& str)
{
    append(str.data(), str.size());
}

void Buffer::append(const void* data, size_t len)
{
    assert(data);
    append(static_cast<const char*>(data), len);
}

void Buffer::append(const Buffer& buf)
{
    append(buf.curReadPtr(), buf.readableBytes());
}

//读取fd的数据放到缓冲区里，为了避免盲目扩张，设置一个暂存buf
ssize_t Buffer::readFd(int fd, int* Errno)
{
    char buf[BUFFER_SIZE];
    struct iovec iov[2];
    const size_t writeable = writeableBytes();

    //设置两个缓冲区，先读入buffer_中，不够就暂存到buf里
    iov[0].iov_base = BeginPtr() + m_writePos;
    iov[0].iov_len = writeableBytes();
    iov[1].iov_base  = buf;
    iov[1].iov_len = sizeof(buf);   

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0)
    {
        *Errno = errno;
    }
    else if(static_cast<size_t>(len) < writeable)//剩余写空间足够,已正常读入，后移write指针即可
    {
        m_writePos += len;
    }
    else    //剩余空间不足，需调用append把剩余的数据读回buffer_（append保证不够就扩容）
    {
        m_writePos = m_buffer.size();
        append(buf, len - writeable);
    }
    return len;
}

//写数据到fd里，read到write之间的数据是有效的
ssize_t Buffer::writeFd(int fd, int* Errno)
{
    size_t readSize = readableBytes();
    ssize_t len = write(fd, curReadPtr(), readSize);
    if(len < 0)
    {
        *Errno = errno;
        return len;
    }
    m_readPos += len;
    return len;
}

//将缓冲区的数据全转为string
std::string Buffer::alltoStr()
{
    std::string str(curReadPtr(), readableBytes());
    initPtr();
    return str;
}

char* Buffer::BeginPtr()
{
    return &*m_buffer.begin();
}

const char* Buffer::BeginPtr() const
{
    return &*m_buffer.begin();
}