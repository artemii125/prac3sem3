#include "../../include/dbase/HttpServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <sstream>

HttpServer::HttpServer(int port, Exchange* exch) : exchange(exch) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    bind(serverSocket, (sockaddr*)&addr, sizeof(addr));
    listen(serverSocket, 10);
}

HttpServer::~HttpServer() {
    close(serverSocket);
}

std::string HttpServer::parseMethod(const std::string& request) {
    return request.substr(0, request.find(' '));
}

std::string HttpServer::parsePath(const std::string& request) {
    size_t start = request.find(' ') + 1;
    size_t end = request.find(' ', start);
    return request.substr(start, end - start);
}

std::string HttpServer::parseBody(const std::string& request) {
    size_t pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return request.substr(pos + 4);
}

std::string HttpServer::parseHeader(const std::string& request, const std::string& headerName) {
    size_t pos = request.find(headerName + ": ");
    if (pos == std::string::npos) return "";
    pos += headerName.length() + 2;
    size_t end = request.find("\r\n", pos);
    return request.substr(pos, end - pos);
}

std::string HttpServer::handleRequest(const std::string& request) {
    std::string method = parseMethod(request);
    std::string path = parsePath(request);
    std::string body = parseBody(request);
    std::string userKey = parseHeader(request, "X-USER-KEY");
    
    json response;
    
    try {
        if (path == "/user" && method == "POST") {
            response = exchange->createUser(json::parse(body));
        } else if (path == "/order" && method == "POST") {
            response = exchange->createOrder(json::parse(body), userKey);
        } else if (path == "/order" && method == "GET") {
            response = exchange->getOrders();
        } else if (path == "/order" && method == "DELETE") {
            response = exchange->deleteOrder(json::parse(body), userKey);
        } else if (path == "/lot" && method == "GET") {
            response = exchange->getLots();
        } else if (path == "/pair" && method == "GET") {
            response = exchange->getPairs();
        } else if (path == "/balance" && method == "GET") {
            response = exchange->getBalance(userKey);
        } else {
            response = json{{"error", "Not found"}};
        }
    } catch (...) {
        response = json{{"error", "Bad request"}};
    }
    
    std::string responseStr = response.dump();
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Content-Length: " << responseStr.length() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "\r\n";
    oss << responseStr;
    
    return oss.str();
}

void HttpServer::start() {
    std::cout << "HTTP Server started\n";
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        
        char buffer[65536] = {0};
        recv(clientSocket, buffer, 65536, 0);
        
        std::string response = handleRequest(std::string(buffer));
        send(clientSocket, response.c_str(), response.length(), 0);
        
        close(clientSocket);
    }
}
