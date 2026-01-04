#include <fstream>
#include <iostream>
#include <filesystem>
#include "../../include/jsonlib/json.hpp"
#include "../../include/dbase/Schema.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

Schema::Schema() : tables(nullptr), tablesCount(0), tablesCapacity(0), tuples_limit(1000) {
    //инициализация массива
    tablesCapacity = 10; //начальное выделенное место
    tables = new TableMetadata[tablesCapacity]; //выделение динамич памяти под массив метаданных
}

Schema::~Schema() {
    if (tables) {
        delete[] tables;
    }
}

//увеличение массива
void Schema::resizeTables() {
    tablesCapacity *= 2; //удвоение вместимости в 2 раза 
    TableMetadata* newTables = new TableMetadata[tablesCapacity]; //новая память
    //копирка старых данных в новый
    for (int i = 0; i < tablesCount; i++) {
        newTables[i] = tables[i];
    }
    delete[] tables; //очистка старого массива
    tables = newTables; //переключение указателя на новый массив
}

//чтение json
void Schema::load(const string& filename) {
    ifstream f(filename);
    if (!f.is_open()) {
        cerr << "Error: Cannot open schema.json" << endl;
        return;
    }

    json data;
    f >> data; //парсинг json в объект data

    //чтение таблицыы json
    this->name = data["name"];
    this->tuples_limit = data["tuples_limit"];

    //структура таблиц
    auto structure = data["structure"];
    //походим по таблицам, берем ключ (название таблицы), значание (массив колонок)
    for (auto& [tName, columns] : structure.items()) {
        if (tablesCount >= tablesCapacity) {
            resizeTables();
        }

        tables[tablesCount].name = tName; //заполнение структуры напрямую в массиве
        tables[tablesCount].pkSequenceFile = tName + "_pk_sequence"; //генерация имени файла для счетчика id

        //добавление колонок из json (первая колонка - это PK)
        for (const auto& col : columns) {
            tables[tablesCount].columnNames.AddEnd(col.get<string>());
        }
        
        tablesCount++;
    }
}

//создание папок
void Schema::setupDirectories() {
    //создание главной папки бд
    if (!fs::exists(name)) {
        fs::create_directory(name);
    }

    //проход по всем имеющимся таблицам
    for (int i = 0; i < tablesCount; i++) {
        string path = name + "/" + tables[i].name; //формирование пути
        if (!fs::exists(path)) { //если папки нет то создается
            fs::create_directory(path);
        }
        //создание файла счетчика id
        string pkPath = path + "/" + tables[i].pkSequenceFile;
        if (!fs::exists(pkPath)) {  
            ofstream pkFile(pkPath);
            pkFile << 0; //начальное значение
            pkFile.close();
        }
    }
}

//поиск таблицы
TableMetadata* Schema::getTable(const string& tableName) {
    for (int i = 0; i < tablesCount; i++) {
        if (tables[i].name == tableName) return &tables[i]; //если имя совпало - возврат указателя на таблицу
    }
    return nullptr;
}