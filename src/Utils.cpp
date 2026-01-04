#include "../include/Utils.h"
#include <sstream>

//удаление пробелов и кавычек с начала и конца строки
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n'\"");
    if (string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\r\n'\"");
    return str.substr(first, (last - first + 1));
}

//разбитие строки по разделителю
Massive split(const string& s, char delimiter) {
    Massive tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        if (!token.empty()) tokens.AddEnd(token);
    }
    return tokens;
}