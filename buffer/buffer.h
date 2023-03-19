#pragma once
#include<iostream>
#include<vector>
#include<cstring>
#include<atomic>
#include<unistd.h>
#include<sys/uio.h>
#include<assert.h>

#define BUFFER_SIZE 65535

class Buffer
{
public:
    Buffer(int initBufferSize = 1024) : m_buffer(initBufferSize), m_readPos(0), m_writePos(0){}
    ~Buffer() = default;

public:
    //可以写入的字节数
    size_t writeableBytes() const;
    //可以读取的字节数
    size_t readableBytes() const;
    //已经读取的字节数
    size_t readBytes() const;

    //获取当前的读写指针
    const char* curReadPtr() const;
    const char* curWritePtrConst() const;
    char* curWritePtr();

    //更新读写指针
    void updateReadPtr(size_t len);
    void updateReadPtrUntilEnd(const char* end);
    void updateWritePtr(size_t len);
    void initPtr();

    void ensureWriteable(size_t len);
    //写入数据
    void append(const char* str, size_t len);
    void append(const std::string& str);
    void append(const void* data, size_t len);
    void append(const Buffer& buffer);

    ssize_t readFd(int fd, int* Errno);
    ssize_t writeFd(int fd, int* Errno);

    std::string alltoStr();

private:
    char* BeginPtr();
    const char* BeginPtr() const;
    void allocateSpace(size_t len);

    std::vector<char> m_buffer;
    std::atomic<std::size_t> m_readPos;  //当前读取的位置，看作begin
    std::atomic<std::size_t> m_writePos; //当前写入的位置，看作end

};