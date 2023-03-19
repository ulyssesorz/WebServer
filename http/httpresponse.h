#pragma once
#include<unordered_map>
#include<fcntl.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<assert.h>
#include"../buffer/buffer.h"
#include"../log/log.h"

class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

public:
    void init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void makeResponse(Buffer& buff);
    char* file();
    size_t fileLen() const;
    void unmapFile();
    void errorContent(Buffer& buff, std::string message);
    int code() const;

private:
    void addStateLine(Buffer& buff);
    void addResponseHeader(Buffer& buff);
    void addResponseContent(Buffer& buff);

    void errorHtml();
    std::string getFileType();

    //http状态码
    int m_code;
    //http是否保持
    bool m_isKeepAlive;
    //解析得到的路径
    std::string m_path;
    //根目录
    std::string m_srcDir;

    //文件映射的区域
    char* m_mmFile;
    struct stat m_mmFileStat;

    //文件后缀到文件夹的映射
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    //状态码到含义的映射
    static const std::unordered_map<int, std::string> CODE_STATUS;
    //状态码到对应html文件名的映射
    static const std::unordered_map<int, std::string> CODE_PATH;
};
