#include <iostream>
#include <string>
#include "../include/dbase/dataBase.h"
#include "../include/dbase/Server.h"

using namespace std;

int main(int argc, char* argv[]) {
    Database db;
    db.init("schema.json");

    if (argc > 1 && string(argv[1]) == "--server") {
        cout << "--- Custom DB Server Mode ---" << endl;
        Server server(&db, 7432);
        server.start();
    } else {
        cout << "--- Custom DB Started ---" << endl;
        cout << "Commands: SELECT, INSERT, DELETE, EXIT" << endl;

        string input;
        while (true) {
            cout << "> ";
            getline(cin, input);

            if (input == "EXIT" || input == "exit") {
                break;
            }

            if (input.empty()) continue;

            db.query(input);
        }
    }

    return 0;
}