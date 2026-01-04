#ifndef SINGLY_LINKED_LIST_H
#define SINGLY_LINKED_LIST_H

#include <iostream>
#include <string>
#include <ostream>
using namespace std;

class SinglyLinkedList {
public:
    struct Node {
        string value;
        Node* next;
    };
    
    SinglyLinkedList();
    ~SinglyLinkedList();
    void AddHeadAfter(const string& value);
    void AddHeadBefore(const string& value);
    void AddTailAfter(const string& value);
    void AddTailBefore(const string& value);
    void DeleteHeadAfter(Node* ptr);
    void DeleteHead();
    void DeleteTail();
    void DeleteBeforeTail();
    void DeleteValue(const string& value);
    Node* FindValue(const string& value);
    void AddBeforeValue(const string& before, const string& value);
    void AddAfterValue(const string& after, const string& value);
    void DeleteBeforeValue(const string& value);
    void DeleteAfterValue(const string& value);
    void PrintWhile() const;
    void PrintFor() const;
    void PrintRecursive(Node* head) const;
    void FreeSList();
    void writeToStream(ostream& out, const string& delimiter) const; //метод для записи списка в файл

private:
    Node* head;
};

#endif
