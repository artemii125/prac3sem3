#ifndef MASSIVE_H
#define MASSIVE_H

#include <iostream>
#include <string>
using namespace std;

class Massive {
private:
    string* data; //динамический массив строк
    int size; //размер массива
    void resize(int newSize); //новый размер массива

public:
    Massive();
    ~Massive();
    void AddEnd(const string& value);
    void AddAt(int index, const string& value);
    void RemoveAt(int index);
    void SetAt(int index, const string& value);
    string GetAt(int index) const;
    int Length() const;
    void Print() const;
};

#endif