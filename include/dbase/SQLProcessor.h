#pragma once
#include <string>
#include "Schema.h"
#include "Collection.h"

using namespace std;

class SQLProcessor {
private:
    Schema* schema;

    // Структура для хранения разобранного условия WHERE
    // Например: table1.col1 = 'value'
    struct Condition {
        string leftOperand;  // table1.col1
        string oper;         // =
        string rightOperand; // 'value' или table2.col2
        string logicalConn;  // AND, OR (для следующего условия)
    };
    
    // Получение значения из конкретной колонки строки CSV
    string getColumnValue(const string& row, const string& tableName, const string& colName);

    // Проверка условий WHERE для текущей строки (или комбинации строк)
    bool checkConditions(struct Condition* conditions, int condCount, 
                         const string& row1, const string& table1Name,
                         const string& row2, const string& table2Name);

    // Обработчики команд
    void processInsert(const string& query);
    void processDelete(const string& query);
    void processSelect(const string& query);

public:
    SQLProcessor(Schema* sc);
    void execute(const string& query);
};