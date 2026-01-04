#ifndef STACK_H
#define STACK_H

#include <iostream>
#include <string>

using namespace std;

class Stack {
private:
    struct SNode {
        string value;
        SNode* next;
    };
    SNode* peak; //указатель на вершину стека
    
public:
    Stack();
    ~Stack();

    void push(const string& value);
    void pop();
    void peek() const;
    void print() const; 
    void freeStack();
};

#endif
