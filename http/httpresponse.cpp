#include"httpresponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse()
{
    m_code = -1;
    m_path = m_srcDir = "";
    m_isKeepAlive = false;
    m_mmFile = nullptr;
    m_mmFileStat = {0};
}

HttpResponse::~HttpResponse()
{
    unmapFile();
}

void HttpResponse::init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code)
{
    assert(srcDir != "");
    if(m_mmFile)
        unmapFile();
    m_srcDir = srcDir;
    m_path = path;
    m_isKeepAlive = isKeepAlive;
    m_code = code;
    m_mmFileStat = {0};
}

//返回文件映射的内存区域
char* HttpResponse::file()
{
    return m_mmFile;
}

//返回文件大小
size_t HttpResponse::fileLen() const
{
    return m_mmFileStat.st_size;
}

//4开头的http状态，无法满足，返回对应html网页
void HttpResponse::errorHtml()
{
    if(CODE_PATH.count(m_code))
    {
        m_path = CODE_PATH.find(m_code)->second;
        stat((m_srcDir + m_path).data(), &m_mmFileStat);
    }
}

//添加状态行，eg: HTTP/1.1 200 ok
void HttpResponse::addStateLine(Buffer& buff)
{
    std::string status;
    if(CODE_STATUS.count(m_code))
    {
        status = CODE_STATUS.find(m_code)->second;
    }
    else    //其他状态均返回bad request
    {
        m_code = 400;
        status = CODE_STATUS.find(m_code)->second;
    }
    buff.append("HTTP/1.1 " + std::to_string(m_code) + " " + status + "\r\n");
}

//http响应头，包括Connection和Content-type字段
void HttpResponse::addResponseHeader(Buffer& buff)
{
    buff.append("Connection: ");
    if(m_isKeepAlive)
    {
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buff.append("close\r\n");
    }
    buff.append("Content-type: " + getFileType() + "\r\n");
}

//回应的内容（文件），进行mmap把文件从磁盘映射到内存，减少系统调用
void HttpResponse::addResponseContent(Buffer& buff)
{
    int srcFd = open((m_srcDir + m_path).data(), O_RDONLY);
    if(srcFd < 0)
    {
        errorContent(buff, "File Not Found");
        return;
    }
    LOG_DEBUG("file path %s", (m_srcDir + m_path).data());
    //将文件srcFd映射到mmRet
    int* mmRet = (int*)mmap(0, m_mmFileStat.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1)
    {
        errorContent(buff, "File Not Found");
        return;
    }
    m_mmFile = (char*)mmRet;
    close(srcFd);
    buff.append("Content-length: " + std::to_string(m_mmFileStat.st_size) + "\r\n\r\n");
}

//4开头的状态返回的内容
void HttpResponse::errorContent(Buffer& buff, std::string message)
{
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\"";
    if(CODE_STATUS.count(m_code))
    {
        status = CODE_STATUS.find(m_code)->second;
    }
    else
    {
        status = "Bad Request";
    }
    body += std::to_string(m_code) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>WebServer</em></body></html>";

    buff.append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}

void HttpResponse::unmapFile()
{
    if(m_mmFile)
    {
        munmap(m_mmFile, m_mmFileStat.st_size);
        m_mmFile = nullptr;
    }
}

int HttpResponse::code() const
{
    return m_code;
}

//文件类型
std::string HttpResponse::getFileType()
{
    std::string::size_type index = m_path.find_last_of('.');
    //没有后缀，返回空白文件
    if(index == std::string::npos)
    {
        return "text/plain";
    }
    std::string suffix = m_path.substr(index);
    if(SUFFIX_TYPE.count(suffix))
    {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

//判断需求文件能否满足，构造http应答
void HttpResponse::makeResponse(Buffer& buff)
{
    //找不到指定文件，或者目标是目录
    if(stat((m_srcDir + m_path).data(), &m_mmFileStat) < 0 || S_ISDIR(m_mmFileStat.st_mode))
    {
        m_code = 404;
    }
    else if(!(m_mmFileStat.st_mode & S_IROTH))
    {
        m_code = 403;
    }
    else if(m_code == -1)
    {
        m_code = 200;
    }
    errorHtml();
    addStateLine(buff);
    addResponseHeader(buff);
    addResponseContent(buff);
}
