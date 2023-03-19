#pragma once
#include<string>
#include<unordered_map>
#include<unordered_set>
#include<regex>
#include"../buffer/buffer.h"
#include"../log/log.h"

class HttpRequest
{
public:
    enum PARSE_STATE{REQUEST_LINE, HEADERS, BODY, FINISH};
    
public:
    HttpRequest() {init();}
    ~HttpRequest() = default;

public:
    void init();
    bool parse(Buffer& buff);

    std::string getPath() const;
    std::string& getPath();
    std::string getMethod() const;
    std::string getVersion() const;
    std::string getPost(const std::string& key) const;
    std::string getPost(const char* key) const;

    bool isKeepAlive() const;

private:
    bool parseRequestLine(const std::string& line);
    void parseHeader(const std::string& line);
    void parseBody(const std::string& line);

    void parsePath();
    void parsePost();

    PARSE_STATE m_state;
    std::string m_method, m_path, m_version, m_body;    //都是字符串
    std::unordered_map<std::string, std::string> m_header;    //k：v类型的对
    std::unordered_map<std::string, std::string> m_post;

    static const std::unordered_set<std::string> DEFAULT_HTML;

};