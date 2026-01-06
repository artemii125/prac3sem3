#include "../../include/dbase/HttpServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <sstream>

using namespace std;

HttpServer::HttpServer(int port, Exchange* exch) : exchange(exch) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0); //здесь создается точка входа
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //настрока адреса сервера
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    bind(serverSocket, (sockaddr*)&addr, sizeof(addr)); //связка сокета с адресом и портом
    listen(serverSocket, 10);
}

HttpServer::~HttpServer() {
    close(serverSocket);
}

string HttpServer::parseMethod(const string& request) {
    return request.substr(0, request.find(' ')); //ищем метод до первого пробела
}

string HttpServer::parsePath(const string& request) {
    size_t start = request.find(' ') + 1;
    size_t end = request.find(' ', start);
    return request.substr(start, end - start); //так находим путь, он между первым и вторым пробелом
}

string HttpServer::parseBody(const string& request) {
    size_t pos = request.find("\r\n\r\n");
    if (pos == string::npos) return "";
    return request.substr(pos + 4); //возвращаем всё, что идет после разделителя pos
}

//функция для поиска значения конкретного заголовка
string HttpServer::parseHeader(const string& request, const string& headerName) {
    size_t pos = request.find(headerName + ": "); //поиск подстроки 
    if (pos == string::npos) return "";
    pos += headerName.length() + 2; //сдвиг на начало значения пропуском имени и ": "
    size_t end = request.find("\r\n", pos); //конец строки
    return request.substr(pos, end - pos); //возврат самого значения заголовка
}

string HttpServer::handleRequest(const string& request) { //обработка запроса
    string method = parseMethod(request);
    string path = parsePath(request);
    string body = parseBody(request);
    string userKey = parseHeader(request, "X-USER-KEY");
    
    json response; //в переменной хранится json-ответ
    
    try { //блок для обработки ошибок
        if (path == "/user" && method == "POST") { //создание пользователя
            response = exchange->createUser(json::parse(body)); 
        } else if (path == "/order" && method == "POST") { //создание ордера 
            response = exchange->createOrder(json::parse(body), userKey);
        } else if (path == "/order" && method == "GET") { //список ордеров
            response = exchange->getOrders();
        } else if (path == "/order" && method == "DELETE") { //удаление ордера
            response = exchange->deleteOrder(json::parse(body), userKey);
        } else if (path == "/lot" && method == "GET") { //список валют
            response = exchange->getLots();
        } else if (path == "/pair" && method == "GET") { //список апар валют
            response = exchange->getPairs();
        } else if (path == "/balance" && method == "GET") { //баланс пользователя
            response = exchange->getBalance(userKey);
        } else {
            response = json{{"error", "Not found"}};
        }
    } catch (...) {
        response = json{{"error", "Bad request"}};
    }
    //блок корректного чтения json-ответа для дальнейшей передачи данных по сети
    string responseStr = response.dump() + "\n"; //json-объект в строку 
    ostringstream oss; 
    oss << "HTTP/1.1 200 OK\r\n"; //статус
    oss << "Content-Type: application/json\r\n"; //указание типа json
    oss << "Content-Length: " << responseStr.length() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "\r\n";
    oss << responseStr;
    
    return oss.str(); //готовая строка ответа
}

void HttpServer::start() {
    cout << "HTTP Server started\n";
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        
        //буфер для чтения что в него прислал клиент
        char buffer[65536] = {0};
        recv(clientSocket, buffer, 65536, 0);
        //преобразование байт в строоку и полученный ответ клиенту
        string response = handleRequest(string(buffer));
        send(clientSocket, response.c_str(), response.length(), 0);
        
        close(clientSocket);
    }
}
