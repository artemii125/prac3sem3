#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include "Exchange.h"

class HttpServer {
private:
    int serverSocket;
    Exchange* exchange;
    
    std::string parseMethod(const std::string& request);
    std::string parsePath(const std::string& request);
    std::string parseBody(const std::string& request);
    std::string parseHeader(const std::string& request, const std::string& headerName);
    std::string handleRequest(const std::string& request);
    
public:
    HttpServer(int port, Exchange* exch);
    ~HttpServer();
    void start();
};

#endif
