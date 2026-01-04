#include "../include/ADT/stack.h"

Stack::Stack() : peak(nullptr) {}

Stack::~Stack() {
    freeStack();
}

void Stack::push(const string& key) {
    SNode* newNode = new SNode;
    newNode->value = key;
    newNode->next = peak;
    peak = newNode;
}

void Stack::pop() {
    if (peak == nullptr) {
        cout << "Стек пуст\n";
        return;
    }

    SNode* deleteNode = peak;
    peak = peak->next;
    delete deleteNode;
}

void Stack::peek() const {
    if (peak == nullptr) {
        cout << "Стек пуст\n";
    } else {
        cout << "Вершина стека: " << peak->value << endl;
    }
}

void Stack::print() const {
    if (peak == nullptr) {
        cout << "Стек пуст\n";
        return;
    }

    SNode* temp = peak;
    cout << "Элементы стека (сверху вниз): ";
    while (temp != nullptr) {
        cout << temp->value << " ";
        temp = temp->next;
    }
    cout << endl;
}

void Stack::freeStack() {
    while (peak != nullptr) {
        pop();
    }
}