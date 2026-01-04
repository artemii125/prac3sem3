#include "../../include/dbase/dataBase.h"
#include <iostream>

using namespace std;

//создание бд
Database::Database() {
    schema = new Schema();
    processor = new SQLProcessor(schema);
}

//деструктор бд
Database::~Database() {
    delete processor;
    delete schema;
}

//инициализация бд
void Database::init(const string& configPath) { //загрузка конфига 
    schema->load(configPath); //
    schema->setupDirectories(); //создание папок
    cout << "Database initialized." << endl;
}

//выполнение SQL-запроса
void Database::query(const string& sql) {
    try {
        processor->execute(sql); //передача запроса процессору
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
}