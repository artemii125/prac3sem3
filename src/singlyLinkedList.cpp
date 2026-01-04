#include "../include/ADT/singlyLinkedList.h"

SinglyLinkedList::SinglyLinkedList() {
    head = nullptr;
}

SinglyLinkedList::~SinglyLinkedList() {
    FreeSList();
}

void SinglyLinkedList::AddHeadAfter(const string& key) {
    if (head == nullptr) return;

    Node* newNode = new Node;
    newNode->value = key;
    newNode->next = head->next;
    head->next = newNode;
}

void SinglyLinkedList::AddHeadBefore(const string& key) {
    Node* newBefore = new Node;
    newBefore->value = key;
    newBefore->next = head;
    head = newBefore;
}

void SinglyLinkedList::AddTailAfter(const string& key) {
    Node* newTail = new Node;
    newTail->value = key;
    newTail->next = nullptr;

    if (head == nullptr) {
        head = newTail;
        return;
    }

    Node* ptr = head;
    while (ptr->next != nullptr) {
        ptr = ptr->next;
    }

    ptr->next = newTail;
}

void SinglyLinkedList::AddTailBefore(const string& key) {
    if (head == nullptr) return;

    if (head->next == nullptr) {
        AddHeadBefore(key);
        return;
    }

    Node* temp = head;
    while (temp->next->next != nullptr) temp = temp->next;

    Node* TailBefore = new Node;
    TailBefore->value = key;
    TailBefore->next = temp->next;
    temp->next = TailBefore;
}

void SinglyLinkedList::DeleteHeadAfter(Node* ptr) {
    if (ptr == nullptr || ptr->next == nullptr) return;

    Node* deleteHeadAfter = ptr->next;
    ptr->next = deleteHeadAfter->next;
    delete deleteHeadAfter;
}

void SinglyLinkedList::DeleteHead() {
    if (head == nullptr) return;

    Node* deleteHead = head;
    head = head->next;
    delete deleteHead;
}

void SinglyLinkedList::DeleteTail() {
    if (head == nullptr) return;

    if (head->next == nullptr) {
        delete head;
        head = nullptr;
        return;
    }

    Node* ptr = head;
    while (ptr->next->next != nullptr) {
        ptr = ptr->next;
    }
    delete ptr->next;
    ptr->next = nullptr;
}

void SinglyLinkedList::DeleteBeforeTail() {
    if (head == nullptr || head->next == nullptr) return;

    if (head->next->next == nullptr) {
        Node* deleteNode = head;
        head = head->next;
        delete deleteNode;
        return;
    }

    Node* ptr = head;
    while (ptr->next->next->next != nullptr) {
        ptr = ptr->next;
    }

    Node* deleteNode = ptr->next;
    ptr->next = ptr->next->next;
    delete deleteNode;
}

void SinglyLinkedList::DeleteValue(const string& key) {
    if (head == nullptr) return;

    if (head->value == key) {
        Node* deleteNode = head;
        head = head->next;
        delete deleteNode;
        return;
    }

    Node* current = head;
    while (current->next != nullptr && current->next->value != key) {
        current = current->next;
    }

    if (current->next == nullptr)
        return;

    Node* deleteNode = current->next;
    current->next = deleteNode->next;
    delete deleteNode;
}

SinglyLinkedList::Node* SinglyLinkedList::FindValue(const string& key) {
    Node* current = head;

    int i = 0;
    while (current != nullptr) {
        if (current->value == key) {
            cout << "Элемент \"" << key << "\" найден на позиции: " << i << endl;
            return current;
        }
        current = current->next;
        i++;
    }
    cout << "Элемент \"" << key << "\" не найден." << endl;
    return nullptr;
}

void SinglyLinkedList::AddBeforeValue(const string& before, const string& key) {
    if (head == nullptr) return;

    if (head->value == before) {
        Node* newNode = new Node;
        newNode->value = key;

newNode->next = head;
        head = newNode;
        return;
    }

    Node* prev = nullptr;
    Node* curr = head;
    while (curr != nullptr && curr->value != before) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == nullptr) {
        cout << "Элемент \"" << before << "\" не найден.\n";
        return;
    }

    Node* newNode = new Node;
    newNode->value = key;
    newNode->next = curr;
    prev->next = newNode;
}

void SinglyLinkedList::AddAfterValue(const string& after, const string& key) {
    Node* temp = head;
    while (temp != nullptr && temp->value != after)
        temp = temp->next;

    if (temp == nullptr) {
        cout << "Элемент \"" << after << "\" не найден.\n";
        return;
    }

    Node* newNode = new Node;
    newNode->value = key;
    newNode->next = temp->next;
    temp->next = newNode;
}

void SinglyLinkedList::DeleteAfterValue(const string& key) {
    if (!head || !head->next) {
        cout << "Нечего удалять после элемента." << endl;
        return;
    }

    Node* current = head;
    while (current && current->value != key) {
        current = current->next;
    }
    if (!current || !current->next) {
        cout << "Элемент '" << key << "' не найден или он последний." << endl;
        return;
    }

    Node* tmp = current->next;
    current->next = tmp->next;
    delete tmp;
    cout << "Элемент после '" << key << "' удален." << endl;
}

void SinglyLinkedList::DeleteBeforeValue(const string& key) {
    if (!head || !head->next) {
        cout << "Нечего удалять перед элементом." << endl;
        return;
    }

    if (head->next->value == key) {
        DeleteHead();
        cout << "Элемент перед '" << key << "' удален." << endl;
        return;
    }

    Node* current = head;
    while (current->next && current->next->next && current->next->next->value != key) {
        current = current->next;
    }

    if (!current->next || !current->next->next) {
        cout << "Элемент '" << key << "' не найден или перед ним нечего удалять." << endl;
        return;
    }

    Node* tmp = current->next;
    current->next = tmp->next;
    delete tmp;
    cout << "Элемент перед '" << key << "' удален." << endl;
}

void SinglyLinkedList::PrintWhile() const {
    Node* ptr = head;
    while (ptr != nullptr) {
        cout << ptr->value << " ";
        ptr = ptr->next;
    }
    cout << endl;
}

void SinglyLinkedList::PrintFor() const {
    for (Node* ptr = head; ptr != nullptr; ptr = ptr->next) {
        cout << ptr->value << " ";
    }
    cout << endl;
}

void SinglyLinkedList::PrintRecursive(Node* head) const {
    if (head == nullptr) return;
    cout << head->value << " ";
    PrintRecursive(head->next);
}

void SinglyLinkedList::FreeSList() {
    Node* current = head;
    while (current != nullptr) {
        Node* next = current->next;
        delete current;
        current = next;
    }
    head = nullptr;
}

void SinglyLinkedList::writeToStream(ostream& out, const string& delimiter) const {
    Node* current = head;
    while (current != nullptr) {
        out << current->value;
        if (current->next != nullptr) {
            out << delimiter;
        }
        current = current->next;
    }
}