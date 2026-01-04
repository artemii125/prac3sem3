#include "../include/ADT/doubleLinkedList.h"

DoublyLinkedList::DoublyLinkedList() {
    head = nullptr;
    tail = nullptr;
}

DoublyLinkedList::~DoublyLinkedList() {
    FreeDList();
}

void DoublyLinkedList::AddHeadBefore(const string& key) {
    DNode* newNode = new DNode;
    newNode->value = key;
    newNode->prev = nullptr;
    newNode->next = head;

    if (head != nullptr) head->prev = newNode;

    head = newNode;

    if (tail == nullptr) tail = newNode;
}

void DoublyLinkedList::AddHeadAfter(const string& key) {
    if (head == nullptr) {
        AddHeadBefore(key);
        return;
    }

    DNode* newNode = new DNode;
    newNode->value = key;
    newNode->prev = head;
    newNode->next = head->next;

    if (head->next != nullptr)
        head->next->prev = newNode;

    head->next = newNode;
}

void DoublyLinkedList::AddTailBefore(const string& key) {
    if (head == nullptr) {
        AddHeadBefore(key);
        return;
    }

    DNode* temp = head;
    while (temp->next != nullptr)
        temp = temp->next;

    DNode* newNode = new DNode;
    newNode->value = key;
    newNode->next = temp;
    newNode->prev = temp->prev;

    if (temp->prev != nullptr)
        temp->prev->next = newNode;

    temp->prev = newNode;
}

void DoublyLinkedList::AddTailAfter(const string& key) {
    DNode* newNode = new DNode;
    newNode->value = key;
    newNode->next = nullptr;
    newNode->prev = nullptr;

    if (head == nullptr) {
        head = newNode;
        return;
    }

    DNode* temp = head;
    while (temp->next != nullptr)
        temp = temp->next;

    temp->next = newNode;
    newNode->prev = temp;
    tail = newNode;
}

void DoublyLinkedList::DeleteHead() {
    if (head == nullptr)
        return;

    DNode* del = head;
    head = head->next;

    if (head != nullptr)
        head->prev = nullptr;

    delete del;

    if (head == nullptr) tail = nullptr; 
}

void DoublyLinkedList::DeleteHeadAfter() {
    if (head == nullptr || head->next == nullptr)
        return;

    DNode* del = head->next;
    head->next = del->next;

    if (del->next != nullptr)
        del->next->prev = head;

    delete del;
}

void DoublyLinkedList::DeleteTail() {
    if (head == nullptr)
        return;

    DNode* temp = head;
    while (temp->next != nullptr)
        temp = temp->next;

    if (temp->prev != nullptr)
        temp->prev->next = nullptr;
    else
        head = nullptr;

    delete temp;
}

void DoublyLinkedList::DeleteTailBefore() {
    if (head == nullptr || head->next == nullptr)
        return;

    DNode* temp = head;
    while (temp->next->next != nullptr)
        temp = temp->next;

    DNode* del = temp;
    temp->prev->next = temp->next;
    temp->next->prev = temp->prev;

    delete del;
}

void DoublyLinkedList::DeleteValue(const string& key) {
    DNode* temp = FindValue(key);

    if (temp == nullptr)
        return;

    if (temp->prev != nullptr)
        temp->prev->next = temp->next;
    else
        head = temp->next;

    if (temp->next != nullptr)
        temp->next->prev = temp->prev;

    delete temp;
}

DoublyLinkedList::DNode* DoublyLinkedList::FindValue(const string& key) {
    DNode* temp = head;
    while (temp != nullptr) {
        if (temp->value == key)
            return temp;
        temp = temp->next;
    }
    return nullptr;
}

void DoublyLinkedList::AddBeforeValue(const string& before, const string& newValue) {
    DNode* temp = head;
    while (temp != nullptr && temp->value != before)

temp = temp->next;

    if (temp == nullptr) {
        cout << "Элемент \"" << before << "\" не найден.\n";
        return;
    }

    DNode* newNode = new DNode;
    newNode->value = newValue;
    newNode->next = temp;
    newNode->prev = temp->prev;

    if (temp->prev != nullptr)
        temp->prev->next = newNode;
    else
        head = newNode;

    temp->prev = newNode;
}

void DoublyLinkedList::AddAfterValue(const string& after, const string& newValue) {
    DNode* temp = head;
    while (temp != nullptr && temp->value != after)
        temp = temp->next;

    if (temp == nullptr) {
        cout << "Элемент \"" << after << "\" не найден.\n";
        return;
    }

    DNode* newNode = new DNode;
    newNode->value = newValue;
    newNode->prev = temp;
    newNode->next = temp->next;

    if (temp->next != nullptr)
        temp->next->prev = newNode;

    temp->next = newNode;
}

void DoublyLinkedList::DeleteAfterValue(const string& key) {
    DNode* target = FindValue(key);
    if (!target || !target->next) {
        cout << "Элемент '" << key << "' не найден или он последний." << endl;
        return;
    }
    DeleteValue(target->next->value);
}

void DoublyLinkedList::DeleteBeforeValue(const string& key) {
    DNode* target = FindValue(key);
    if (!target || !target->prev) {
        cout << "Элемент '" << key << "' не найден или он первый." << endl;
        return;
    }
    DeleteValue(target->prev->value);
}

void DoublyLinkedList::PrintForward() const {
    DNode* temp = head;
    while (temp != nullptr) {
        cout << temp->value << " ";
        temp = temp->next;
    }
    cout << endl;
}

void DoublyLinkedList::PrintBackward() const {
    if (head == nullptr)
        return;

    DNode* temp = head;
    while (temp->next != nullptr)
        temp = temp->next;

    while (temp != nullptr) {
        cout << temp->value << " ";
        temp = temp->prev;
    }
    cout << endl;
}

void DoublyLinkedList::PrintRecursiveForward(DNode* head) const {
    if (head == nullptr)
        return;
    cout << head->value << " ";
    PrintRecursiveForward(head->next);
}

void DoublyLinkedList::PrintRecursiveBackward(DNode* tail) const {
    if (tail == nullptr)
        return;
    cout << tail->value << " ";
    PrintRecursiveBackward(tail->prev);
}

void DoublyLinkedList::FreeDList() {
    DNode* current = head;
    while (current != nullptr) {
        DNode* next = current->next;
        delete current;
        current = next;
    }
    head = nullptr;
}