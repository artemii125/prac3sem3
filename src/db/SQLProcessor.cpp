#include <iostream>
#include <fstream>
#include <algorithm>
#include "../include/dbase/SQLProcessor.h"
#include "../include/Utils.h"

using namespace std;

SQLProcessor::SQLProcessor(Schema* sc) : schema(sc) {}

// Получение значения из CSV строки по имени колонки
string SQLProcessor::getColumnValue(const string& row, const string& tableName, const string& colName) {
    //ищется описание таблицы в схеме
    TableMetadata* meta = schema->getTable(tableName);
    if (!meta) return "";

    //поиск индекс нужной колонки
    int colIndex = -1;
    for (int i = 0; i < meta->columnNames.Length(); i++) {
        if (meta->columnNames.GetAt(i) == colName) {
            colIndex = i;
            break;
        }
    }
    if (colIndex == -1) return "";

    //csv строка разбивается на массив
    Massive values = split(row, ',');
    //при наличии достаточых данных, возвращаем нужное значение
    if (colIndex < values.Length()) {
        return values.GetAt(colIndex);
    }
    return "";
}

//проверка условий с учетом с операндами AND, OR
bool SQLProcessor::checkConditions(Condition* conditions, int condCount, 
                                   const string& row1, const string& table1Name,
                                   const string& row2, const string& table2Name) {
    if (condCount == 0) return true; 

    //начальное значение true, так как первое условие обычно соединяется через AND
    bool finalResult = true;
    bool isFirst = true;

    for (int i = 0; i < condCount; i++) {
        //вычисляется истинность текущего условия (a = b)
        Condition& cond = conditions[i];
        string leftVal;
        Massive leftParts = split(cond.leftOperand, '.'); //system.id > [system, id]
        if (leftParts.Length() == 2) { 
            //если колонка первой таблицы - row1 ()
            if (leftParts.GetAt(0) == table1Name) leftVal = getColumnValue(row1, table1Name, leftParts.GetAt(1));
            //если колонка второй таблицы - row2
            else if (leftParts.GetAt(0) == table2Name) leftVal = getColumnValue(row2, table2Name, leftParts.GetAt(1));
        } else {
             leftVal = cond.leftOperand; // Если вдруг слева константа
        }

        //вычисляется истинность текущего условия (с = d)
        string rightVal;
        if (cond.rightOperand.find('.') != string::npos) {
            Massive rightParts = split(cond.rightOperand, '.');
            if (rightParts.GetAt(0) == table1Name) rightVal = getColumnValue(row1, table1Name, rightParts.GetAt(1));
            else if (rightParts.GetAt(0) == table2Name) rightVal = getColumnValue(row2, table2Name, rightParts.GetAt(1));
        } else {
            rightVal = cond.rightOperand;
        }

        bool currentCheck = (leftVal == rightVal);

        //объединяем с общим результатом
        if (isFirst) {
            finalResult = currentCheck;
            isFirst = false;
        } else {
            if (cond.logicalConn == "OR") {
                finalResult = finalResult || currentCheck;
            } else {
                finalResult = finalResult && currentCheck;
            }
        }
    }
    return finalResult;
}

//обработка команд
void SQLProcessor::execute(const string& query) {
    string q = trim(query);
    if (!q.empty() && q.back() == ';') q.pop_back();
    
    if (q.find("INSERT") == 0) processInsert(q);
    else if (q.find("SELECT") == 0) processSelect(q);
    else if (q.find("DELETE") == 0) processDelete(q);
    else cout << "Unknown command" << endl;
}

void SQLProcessor::processInsert(const string& query) {
    size_t intoPos = query.find("INTO");
    size_t valuesPos = query.find("VALUES");
    
    //проверка на валидность
    if (intoPos == string::npos || valuesPos == string::npos) {
        cout << "Syntax error" << endl;
        return;
    }
    //вырезается имя таблицы между INTO и VALUES 
    string tableName = trim(query.substr(intoPos + 4, valuesPos - (intoPos + 4)));
    //вырезаются данные после VALUES
    string valuesStr = query.substr(valuesPos + 6);
    //убираются скобки
    size_t openP = valuesStr.find('(');
    size_t closeP = valuesStr.find(')');
    if (openP != string::npos && closeP != string::npos) {
        valuesStr = valuesStr.substr(openP + 1, closeP - openP - 1);
    }
    //разбивается на список
    Massive rawValues = split(valuesStr, ',');
    SinglyLinkedList list;
    for (int i = 0; i < rawValues.Length(); i++) {
        list.AddTailAfter(trim(rawValues.GetAt(i)));
    }

    //записываются данные
    Collection table(schema, tableName);
    if (table.recordExists(list)) {
        cout << "Error: Record already exists in " << tableName << endl;
        return;
    }
    
    table.insert(list);
    cout << "Inserted into " << tableName << endl;
}

void SQLProcessor::processDelete(const string& query) {
    //DELETE FROM table WHERE col = 'val'
    Massive tokens = split(query, ' ');
    if (tokens.Length() < 3) return;

    string tableName = tokens.GetAt(2);
    string colName, value;

    size_t wherePos = query.find("WHERE");
    if (wherePos != string::npos) {
        string condition = query.substr(wherePos + 5);
        size_t eqPos = condition.find('=');
        if (eqPos != string::npos) {
            string left = trim(condition.substr(0, eqPos));
            string right = trim(condition.substr(eqPos + 1));
            Massive parts = split(left, '.');
            if (parts.Length() == 2) colName = parts.GetAt(1);
            value = right;
        }
    }

    if (!colName.empty()) {
        Collection table(schema, tableName);
        table.deleteRows(colName, value);
        cout << "Deleted rows from " << tableName << endl;
    }
}

void SQLProcessor::processSelect(const string& query) {
    size_t fromPos = query.find("FROM");
    size_t wherePos = query.find("WHERE");
    size_t selectPos = query.find("SELECT");
    string selectPart = query.substr(selectPos + 6, fromPos - (selectPos + 6));
    if (trim(selectPart) == "*") {
        cout << "Error: SELECT * is not allowed. Specify columns explicitly" << endl;
        return;
    }
    if (wherePos == string::npos) {
        cout << "Error: SELECT without WHERE is not allowed" << endl;
        return;
    }
    
    //получаем названия таблиц
    string fromPart = query.substr(fromPos + 4, wherePos - (fromPos + 4));
    
    Massive tablesRaw = split(fromPart, ',');
    string tables[10];
    int tablesCount = 0;
    for (int i = 0; i < tablesRaw.Length() && i < 10; i++) tables[tablesCount++] = trim(tablesRaw.GetAt(i));

    //парсинг WHERE
    int condCapacity = 20; //возможный макс 20 условий
    Condition* conditions = new Condition[condCapacity]; //массив хранения условий
    int condCount = 0;
    
    //отрезание всего до WHERE (5 б. + пробел)
    if (wherePos != string::npos) {
        string remaining = query.substr(wherePos + 5);
        string nextLogicalConn = "AND";
        while (!remaining.empty() && condCount < condCapacity) {
            //ищем ближайший AND или OR
            size_t pAnd = remaining.find(" AND ");
            size_t pOr = remaining.find(" OR ");
            size_t pNextOp = string::npos;
            string opFound = "";

            //выбирается тот, что встречается раньше
            if (pAnd != string::npos && (pOr == string::npos || pAnd < pOr)) {
                pNextOp = pAnd;
                opFound = "AND";
            } else if (pOr != string::npos) {
                pNextOp = pOr;
                opFound = "OR";
            }

            //выделяется текущее условие (например "t1.c1 = 'val'")
            string conditionStr;
            if (pNextOp != string::npos) {
                conditionStr = trim(remaining.substr(0, pNextOp));
                // Обрезаем строку для следующей итерации, + AND (5 симв), OR (4 симв)
                remaining = remaining.substr(pNextOp + opFound.length() + 2); 
            } else {
                //операторы кончились, значит последнее условие
                conditionStr = trim(remaining);
                remaining = "";
            }

            //парсим само равенство a = b
            size_t eqPos = conditionStr.find('=');
            if (eqPos != string::npos) {
                Condition c;
                c.leftOperand = trim(conditionStr.substr(0, eqPos)); //слева
                c.rightOperand = trim(conditionStr.substr(eqPos + 1)); //справ
                c.oper = "=";
                c.logicalConn = nextLogicalConn; //запоминается, тк условие оединяется через AND/OR
                
                conditions[condCount++] = c; //сохранение в массив
            }

            //запоминаем оператор для следующего условия
            if (!opFound.empty()) {
                nextLogicalConn = opFound;
            }
        }
    }
    if (tablesCount == 1) {
        Collection t1(schema, tables[0]);
        int idx = 1;
        //чтение всех csv
        while (true) {
            string file = t1.getCsvPath(idx);
            ifstream f(file);
            if (!f.is_open()) break;
            string line;
            while (getline(f, line)) {
                //проверяются условия для каждой строки
                if (checkConditions(conditions, condCount, line, tables[0], "", "")) {
                    cout << line << endl;
                }
            }
            f.close();
            idx++;
        }
    } 
    else if (tablesCount == 2) {
        Collection t1(schema, tables[0]);
        Collection t2(schema, tables[1]);

        int idx1 = 1;
        while (true) {
            string file1 = t1.getCsvPath(idx1);
            ifstream f1(file1);
            if (!f1.is_open()) break;
             
            string line1;
            while (getline(f1, line1)) {
                //сross join (для каждой строки T1 бежим по T2
                int idx2 = 1;
                while (true) {
                    string file2 = t2.getCsvPath(idx2);
                    ifstream f2(file2);
                    if (!f2.is_open()) break;

                    string line2;
                    while (getline(f2, line2)) {
                         //проверка условий WHERE для пары строк
                        if (checkConditions(conditions, condCount, line1, tables[0], line2, tables[1])) {
                            cout << line1 << " | " << line2 << endl;
                        }
                    }
                    f2.close();
                    idx2++;
                }
            }
            f1.close();
            idx1++;
        }
    }
    delete[] conditions;
}