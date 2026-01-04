#pragma once
#include <string>
#include "../include/ADT/massive.h" //хранение имен колонок

using namespace std;

struct TableMetadata { //cтруктура для хранения метаданных одной таблицы
    string name;
    Massive columnNames; //cписок названий колонок
    string pkSequenceFile; //имя файла для генерации ID
};

class Schema {
private:
    TableMetadata* tables; //список таблиц 
    int tablesCount; //текущ кол-во таблиц
    int tablesCapacity; //вместимость 
    string name; //сама схемы

    void resizeTables();
    
public:
    int tuples_limit; //лимит строк в одном CSV

    Schema();
    ~Schema();

    void load(const string& filename); //читает JSON и заполняет структуры
    void setupDirectories(); //создание папки Schema/Table
    TableMetadata* getTable(const string& tableName);
    string getSchemaName() const { return name; }
};