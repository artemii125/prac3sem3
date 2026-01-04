#include <iostream>
#include "../include/dbase/Exchange.h"
#include "../include/dbase/HttpServer.h"

int main() {
    Exchange exchange("config.json");
    HttpServer server(8080, &exchange);
    server.start();
    return 0;
}
