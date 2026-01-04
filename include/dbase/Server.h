#pragma once
#include <string>
#include <mutex>
#include "dataBase.h"

using namespace std;

class Server {
private:
    Database* db;
    int port;
    int serverSocket; //сокет для прослушивания
    mutex dbMutex;
    
    void handleClient(int clientSocket, string clientInfo); //обработка одного клиента
    
public:
    Server(Database* database, int port = 7432);
    ~Server();
    void start();
    void stop();
};
