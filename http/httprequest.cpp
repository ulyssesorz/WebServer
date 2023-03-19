#include"httprequest.h"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index","/welcome","/video","/picture"
};

//初始化字段和容器
void HttpRequest::init()
{
    m_method = m_path = m_version = m_body = "";
    m_state = REQUEST_LINE;
    m_header.clear();
    m_post.clear();
}

//查看头部的connection和m_version是否符合
bool HttpRequest::isKeepAlive() const
{
    if(m_header.count("Connection"))
        return m_header.find("Connection")->second == "keep-alive" && m_version == "1.1";
    return false;
}

//用正则表达式匹配请求行，格式是方法、路径、版本，eg: GET /qq/abc.html HTTP/1.1
bool HttpRequest::parseRequestLine(const std::string& line)
{
    //第一个^表示开始，$表示结尾，([^ ]*)表示非空字符组
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch submatch;
    if(regex_match(line, submatch, pattern))
    {
        m_method = submatch[1];//0是完整结果，所以从1开始
        m_path = submatch[2];
        m_version = submatch[3];
        m_state = HEADERS; //状态转移至头部
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

//用正则表达式匹配头部，格式是key: value，eg: Host: www.baidu.com
void HttpRequest::parseHeader(const std::string& line)
{
    //先匹配:前的非:字符，然后再匹配:后面的任意字符
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch submatch;
    if(regex_match(line, submatch, pattern))
    {
        m_header[submatch[1]] = submatch[2];
    }
    else
    {
        m_state = BODY;//状态转移至消息体
    }
}

//消息体的数据没有固定格式，全部保存到m_body里，在m_post里解析
void HttpRequest::parseBody(const std::string& line)
{
    m_body = line;
    parsePost();
    m_state = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

//解析m_post数据，格式为key=value，每条数据用&隔开
void HttpRequest::parsePost()
{
    if(m_method == "POST" && m_header["Content-Type"]  == "application/x-www-form-urlencoded")
    {
        std::regex pattern("(?!&)(.*?)=(.*?)(?=&|$)");
        std::smatch submatch;
        std::string::const_iterator beg = m_body.begin();
        std::string::const_iterator end = m_body.end();
        while(std::regex_search(beg, end, submatch, pattern))
        {
            m_post[submatch[1]] = submatch[2];
            beg = submatch[0].second;
        }
    }
}

//给请求的路径加上html
void HttpRequest::parsePath()
{
    if(m_path == "/")
    {
        m_path = "/index.html";
    }
    else
    {
        if(DEFAULT_HTML.count(m_path))
            m_path += ".html";
    }
}

//主解析函数
bool HttpRequest::parse(Buffer& buff)
{
    const char* CRLF = "\r\n";
    if(buff.readableBytes() <= 0)
        return false;
    while(buff.readableBytes() && m_state != FINISH)
    {
        //在缓冲区里查找crlf
        const char* lineEnd = std::search(buff.curReadPtr(), buff.curWritePtrConst(), CRLF, CRLF + 2);
        //获取缓冲区的数据
        std::string line(buff.curReadPtr(), lineEnd);
        switch(m_state)
        {
            case REQUEST_LINE:
                if(!parseRequestLine(line))
                    return false;
                parsePath();
                break;
            case HEADERS:
                parseHeader(line);
                if(buff.readableBytes() <= 2)//只剩换crlf了，是get方法
                    m_state = FINISH;
                break;
            case BODY:
                parseBody(line);
                break;
            default:
                break;
        }
        if(lineEnd == buff.curWritePtr())
            break;
        buff.updateReadPtrUntilEnd(lineEnd + 2);//后移read指针
    }
    LOG_DEBUG("[%s], [%s], [%s]", m_method.c_str(), m_path.c_str(), m_version.c_str());
    return true;
}

//以下函数用于测试

std::string HttpRequest::getPath() const
{
    return m_path;
}

std::string& HttpRequest::getPath()
{
    return m_path;
}

std::string HttpRequest::getMethod() const
{
    return m_method;
}

std::string HttpRequest::getVersion() const
{
    return m_version;
}

std::string HttpRequest::getPost(const std::string& key) const
{
    assert(!key.empty());
    if(m_post.count(key))
        return m_post.find(key)->second;
    return "";    
}

std::string HttpRequest::getPost(const char* key) const
{
    assert(key != nullptr);
    if(m_post.count(key))
        return m_post.find(key)->second;
    return "";    
}
