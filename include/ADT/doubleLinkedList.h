#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

#include <iostream>
#include <string>
using namespace std;

class DoublyLinkedList {
public:
    struct DNode {
        string value;
        DNode* prev; //предыдущий узел
        DNode* next; //следующий
    };

    DoublyLinkedList();
    ~DoublyLinkedList();
    void AddHeadBefore(const string& value);
    void AddHeadAfter(const string& value);
    void AddTailBefore(const string& value);
    void AddTailAfter(const string& value);
    void DeleteHead();
    void DeleteHeadAfter();
    void DeleteTail();
    void DeleteTailBefore();
    void DeleteValue(const string& value);
    DNode* FindValue(const string& value);
    void AddBeforeValue(const string& before, const string& newValue);
    void AddAfterValue(const string& after, const string& newValue);
    void DeleteBeforeValue(const string& key);
    void DeleteAfterValue(const string& key);
    void PrintForward() const;
    void PrintBackward() const;
    void PrintRecursiveForward(DNode* head) const;
    void PrintRecursiveBackward(DNode* tail) const;
    void FreeDList();

private:    
    DNode* head; //голова списка
    DNode* tail; //хвост списка
};

#endif
