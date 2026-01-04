#pragma once
#include <string>
#include "Schema.h"
#include "SQLProcessor.h"

using namespace std;

class SQLProcessor;

class Database {
private:
    Schema* schema;
    SQLProcessor* processor;

public:
    Database();
    ~Database();
    
    // Инициализация: чтение конфига и создание папок
    void init(const string& configPath);
    
    // Выполнение запроса
    void query(const string& sql);
};