#include "../include/dbase/Collection.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

using namespace std;
namespace fs = filesystem;

Collection::Collection(Schema* sc, string tn) : schema(sc), tableName(tn) {
    path = schema->getSchemaName() + "/" + tableName; //путь к папке таблицы
    pkFile = path + "/" + tableName + "_pk_sequence"; //путь к файлу с последним ID
    
    //проверка наличия
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
}

//путь csv
string Collection::getCsvPath(int index) const {
    return path + "/" + to_string(index) + ".csv";
}

//генерация primary key
int Collection::generateId() {
    int id = 0;
    
    //чтитаем последний id, если таковой есть
    if (fs::exists(pkFile)) {
        ifstream in(pkFile);
        in >> id;
        in.close();
    }
    
    id++;

    //перезапись с новым значением
    ofstream out(pkFile);
    out << id;
    out.close();
    
    return id;
}

//работа с файлами
int Collection::getLastFileIndex() const {
    int index = 1;
    //ищется первый файл которого нет
    while (fs::exists(getCsvPath(index))) {
        index++;
    }
    //в зависимости от ситуаци возвращаем последний/первый
    return (index > 1) ? index - 1 : 1;
}

//счетчик строк для дальнейшего сравнения с границей
int Collection::countLines(const string& fullPath) const {
    ifstream f(fullPath);
    if (!f.is_open()) return 0;
    string line;
    int count = 0;
    while (getline(f, line)) {
        count++;
    }
    return count;
}

//стоки в csv разбиваются на массив строк
Massive Collection::splitCSV(const string& line, char delimiter) const {
    Massive tokens;
    string token;
    istringstream tokenStream(line);
    //процесс заполнения по разделителю
    while (getline(tokenStream, token, delimiter)) {
        tokens.AddEnd(token);
    }
    return tokens;
}


void Collection::insert(SinglyLinkedList& values) {; 
    if (recordExists(values)) {
        cout << "Error: Record already exists in table '" << tableName << "'" << endl;
        return;
    }
    
    int fileIdx = getLastFileIndex();
    string currentPath = getCsvPath(fileIdx);

    //проверка лимита строк, если переполнен -> создаем следующий
    if (fs::exists(currentPath) && countLines(currentPath) >= schema->tuples_limit) {
        fileIdx++;
        currentPath = getCsvPath(fileIdx);
    }

    //открытие файла в режиме добавления в конец
    ofstream f(currentPath, ios::app);
    
    if (f.is_open()) {
        //пишутся данные из списка (включая ID который уже в списке)
        values.writeToStream(f, ",");
        f << "\n";
        f.close();
    } else {
        cerr << "Error: Could not open file for writing: " << currentPath << endl;
    }
}

//удобный вывод данных для таблицы
void Collection::printAll() {
    int idx = 1;
    //перебор всех csv
    while (fs::exists(getCsvPath(idx))) {
        ifstream f(getCsvPath(idx));
        string line;
        while (getline(f, line)) {
            cout << line << endl;
        }
        f.close();
        idx++;
    }
}

void Collection::deleteRows(const string& columnName, const string& value) {;
    //находится индекс колонки по которой фильтруем
    TableMetadata* meta = schema->getTable(tableName);
    if (!meta) {
        cout << "Table not found!" << endl;
        return;
    }

    //ищем индекс колонки по ее имени
    int colIndex = -1;
    for (int i = 0; i < meta->columnNames.Length(); i++) {
        if (meta->columnNames.GetAt(i) == columnName) {
            colIndex = i;
            break;
        }
    }

    if (colIndex == -1) {
        cout << "Column '" << columnName << "' not found in table '" << tableName << "'" << endl;
        return;
    }

    //проход по всем файлам с данными
    int idx = 1;
    while (fs::exists(getCsvPath(idx))) {
        string inFile = getCsvPath(idx); //исходный csv
        string tempFile = inFile + ".tmp"; //временный csv

        ifstream src(inFile);
        ofstream dst(tempFile);
        string line;

        bool fileChanged = false; //флаг чтобы понять удалили ли строки в файле

        if (!src.is_open() || !dst.is_open()) {
            idx++; 
            continue;
        }

        while (getline(src, line)) { //обработка одной строки для эффективного расходывания памяти
            //парсинг строки в массив
            Massive row = splitCSV(line, ',');
            
            //проверка на выход за границы
            if (colIndex < row.Length()) {
                //проверка на соответствие удаляемого со значением в колонке 
                if (row.GetAt(colIndex) == value) {
                    fileChanged = true; //флаг удаления
                    continue; 
                }
            }
            //или пишем строку как есть во временный
            dst << line << endl;
        }

        src.close();
        dst.close();

        if (fileChanged) {
            //если были изменения то меняем старый на новый
            fs::remove(inFile);
            fs::rename(tempFile, inFile);
        } else {
            fs::remove(tempFile);
        }

        idx++;
    }
}

bool Collection::recordExists(SinglyLinkedList& values) {
    //конвертируется список в строку для сравнения
    ostringstream oss;
    values.writeToStream(oss, ",");
    string searchValue = oss.str();
    
    int idx = 1;
    //перебор всех CSV файлов
    while (fs::exists(getCsvPath(idx))) {
        ifstream f(getCsvPath(idx));
        string line;
        
        while (getline(f, line)) {
            //сравниваем всю строку
            if (line == searchValue) {
                f.close();
                return true; //дубликат
            }
        }
        f.close();
        idx++;
    }
    return false; //не дубликат
}