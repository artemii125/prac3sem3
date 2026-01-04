#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "../include/ADT/singlyLinkedList.h"
#include "../include/ADT/massive.h"
#include "../include/dbase/Schema.h"

using namespace std;

class Collection {
private:
    Schema* schema;
    string tableName;
    string path; //путь к папке таблицы
    string pkFile; //путь к файлу sequence

    int generateId(); 
    int getLastFileIndex() const;
    int countLines(const string& fullPath) const;
    Massive splitCSV(const string& line, char delimiter) const;

public:
    Collection(Schema* schema, string tableName);
    string getCsvPath(int index) const;
    void insert(SinglyLinkedList& values);
    void deleteRows(const string& columnName, const string& value);
    void printAll();
    bool recordExists(SinglyLinkedList& values);
    //инлайн геттера для имени таблицы
    string getName() const { return tableName; }
};