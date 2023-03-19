#include<unistd.h>
#include<iostream>
#include"server/webserver.h"

int main()
{
    int port = 8081;
    int trigMode = 3;
    int timeout = 60000;
    bool optLinger = false; 
    int threadNumber = 4;
    bool openLog = false;
    int logLevel = 1;
    int logSize = 1024;
    WebServer server(port, trigMode, timeout, optLinger, threadNumber, openLog, logLevel, logSize);
    std::cout << "port is " << port << std::endl;
    server.start();
    return 0;
}

//http://localhost:8081/index.html