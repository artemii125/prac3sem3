#include "dbase/Server.h"
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>

using namespace std;

void printElka() {
    cout << "        *\n";
    int spaces = 7;
    int stars = 3;
    
    for (int tier = 0; tier < 4; tier++) {
        for (int row = 0; row < 2 + tier; row++) {
            cout << string(max(0, spaces), ' ') << string(stars, '*') << "\n";
            if (row < 1 + tier) {
                spaces--;
                stars += 2;
            }
        }
        spaces++;
        stars -= 2;
    }
    
    cout << "       |||\n       |||\n";
}

Server::Server(Database* database, int port) : db(database), port(port), serverSocket(-1) {}
Server::~Server() {
    stop();
}

//создание сокета, привязка к порту, прослушивание подключений
void Server::start() {
    printElka();
    cout << "DB Server starting...\n" << endl;
    
    serverSocket = socket(AF_INET, SOCK_STREAM, 0); //создание TCP сокета (IPv4, TCP)
    if (serverSocket < 0) {
        cerr << "Error creating socket" << endl;
        return;
    }

    //переиспользование адреса для быстрого перезапуска сервера
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //настройка адреса сервера
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET; 
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Bind failed" << endl;
        close(serverSocket);
        return;
    }

    if (listen(serverSocket, 10) < 0) {
        cerr << "Listen failed" << endl;
        close(serverSocket);
        return;
    }

    cout << "Server listening on port " << port << endl;

    //принятие подключения клиентов
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen); //подключение клиента
        
        if (clientSocket < 0) {
            if (serverSocket == -1) break;  // Сервер остановлен
            cerr << "Accept failed" << endl;
            continue;
        }

        //создается отдельный поток для обработки клиента
        string clientInfo = string(inet_ntoa(clientAddr.sin_addr)) + ":" + to_string(ntohs(clientAddr.sin_port));
        cout << "Client connected from " << clientInfo << endl;
        thread(&Server::handleClient, this, clientSocket, clientInfo).detach();
    }
}

//обработка одного клиента в отдельном потоке
void Server::handleClient(int clientSocket, string clientInfo) {
    char buffer[4096]; //заданный буфер
    
    //цикл обработки запросов от клиента
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        
        //получение данных от клиента из сокета
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRead <= 0) {
            cout << "Client disconnected: " << clientInfo << endl;
            break;
        }
        
        //преобразование строки и чистка от пробелов
        string query(buffer);
        size_t end = query.find_last_not_of(" \n\r\t");
        if (end != string::npos) {
            query.erase(end + 1);
        } else {
            query.clear();
        }
        
        if (query == "EXIT" || query == "exit") {
            cout << "Client disconnected (EXIT): " << clientInfo << endl;
            break;
        }

        stringstream result; //буфер для захвата
        streambuf* oldCout = cout.rdbuf(result.rdbuf()); // cout в result
        
        {
            lock_guard<mutex> lock(dbMutex); //захватываем мьютекс
            db->query(query); //выполняем запрос к БД
        } //мьютекс автоматически освобождается
        cout.rdbuf(oldCout); //возврат
        
        //формирование самого ответ
        string response = result.str();
        if (response.empty()) response = "OK\n";
        
        send(clientSocket, response.c_str(), response.length(), 0);
    }
    close(clientSocket);
}
void Server::stop() {
    if (serverSocket != -1) {
        close(serverSocket);
        serverSocket = -1;
    }
}
