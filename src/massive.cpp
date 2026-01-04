#include "../include/ADT/massive.h"

Massive::Massive() {
    data = nullptr;
    size = 0;
}

Massive::~Massive() {
    delete[] data;
}

void Massive::resize(int newSize) {
    string* newData = new string[newSize];

    for (int i = 0; i < size; i++) {
        newData[i] = data[i];
    }

    delete[] data;
    data = newData;
    size = newSize;
}

void Massive::AddEnd(const string& value) {
    resize(size + 1);
    data[size - 1] = value;
}

void Massive::AddAt(int index, const string& value) {
    if (index < 0 || index > size) {
        cout << "Неверный индекс\n";
        return;
    }

    resize(size + 1);

    for (int i = size - 1; i > index; --i) {
        data[i] = data[i - 1];
    }

    data[index] = value;
}

void Massive::RemoveAt(int index) {
    if (index < 0 || index >= size) {
        cout << "Неверный индекс\n";
        return;
    }

    for (int i = index; i < size - 1; ++i) {
        data[i] = data[i + 1];
    }

    resize(size - 1);
}

void Massive::SetAt(int index, const string& value) {
    if (index < 0 || index >= size) {
        cout << "Неверный индекс\n";
        return;
    }
    data[index] = value;
}

string Massive::GetAt(int index) const {
    if (index < 0 || index >= size) {
        cout << "Неверный индекс\n";
        return "";
    }
    return data[index];
}

int Massive::Length() const {
    return size;
}

void Massive::Print() const {
    cout << "Массив: ";
    for (int i = 0; i < size; i++) {
        cout << data[i] << " ";
    }
    cout << endl;
}