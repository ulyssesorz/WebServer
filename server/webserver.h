#include<unordered_map>
#include<fcntl.h>
#include<unistd.h>
#include<assert.h>
#include<errno.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include"threadpool.h"
#include"../epoller/epoller.h"
#include"../timer/timer.h"
#include"../http/httpconnection.h"
#include"../log/log.h"

class WebServer
{
public:
    WebServer(int port, int trigMode, int timeout, bool optLinger, int threadNumber, bool openLog, int logLevel, int logSize);
    ~WebServer();

    void start();

private:
    bool initSocket();
    void initEvenMode(int trigMode);
    void addConnection(int fd, sockaddr_in addr);
    void closeConnection(HttpConnection* client);

    void handleListen();
    void handleWrite(HttpConnection* client);
    void handleRead(HttpConnection* client);

    void onRead(HttpConnection* client);
    void onWrite(HttpConnection* client);
    void onProcess(HttpConnection* client);

    void sendError(int fd, const char* info);
    void extentTime(HttpConnection* client);

    static const int MAX_FD = 65535;
    static int setFdNonblock(int fd);

private:
    int m_port;
    int m_timeout;
    bool m_isClosed;
    int m_listenFd;
    bool m_openLinger;
    char* m_srcDir;

    uint32_t m_listenEvent;
    uint32_t m_connEvent;

    std::unique_ptr<TimerManager> m_timer;
    std::unique_ptr<ThreadPool> m_threadpool;
    std::unique_ptr<Epoller> m_epoller;
    std::unordered_map<int, HttpConnection> m_users;
};