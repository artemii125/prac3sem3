#include "../../include/dbase/Exchange.h"
#include "../../include/ADT/massive.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <random>
#include <sstream>
#include <ctime>

using namespace std;

Exchange::Exchange(const string& configPath) {
    ifstream file(configPath);
    json config = json::parse(file);
    
    dbHost = config["database_ip"];
    dbPort = config["database_port"];
    
    dbSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dbPort);
    inet_pton(AF_INET, dbHost.c_str(), &addr.sin_addr);
    connect(dbSocket, (sockaddr*)&addr, sizeof(addr));
    
    initializeLots(config);
    initializePairs();
}

Exchange::~Exchange() {
    close(dbSocket);
}

string Exchange::sendToDatabase(const string& query) {
    send(dbSocket, query.c_str(), query.length(), 0);
    char buffer[65536] = {0};
    int bytesRead = recv(dbSocket, buffer, 65536, 0);
    string result(buffer, bytesRead);
    
    if (query.find("SELECT") == 0) {
        return csvToJson(result, query);
    } else if (query.find("INSERT") == 0) {
        size_t valuesPos = query.find("VALUES");
        if (valuesPos != string::npos) {
            size_t openParen = query.find('(', valuesPos);
            size_t firstQuote = query.find('\'', openParen);
            size_t secondQuote = query.find('\'', firstQuote + 1);
            string id = query.substr(firstQuote + 1, secondQuote - firstQuote - 1);
            
            size_t intoPos = query.find("INTO");
            size_t spaceAfterInto = query.find(' ', intoPos + 5);
            string tableName = query.substr(intoPos + 5, spaceAfterInto - (intoPos + 5));
            
            return "{\"" + tableName + "_id\":\"" + id + "\"}";
        }
    }
    return result;
}

string Exchange::csvToJson(const string& csv, const string& query) {
    size_t fromPos = query.find("FROM");
    size_t wherePos = query.find("WHERE");
    if (fromPos == string::npos) return "[]";
    
    string tableName = query.substr(fromPos + 5, wherePos - (fromPos + 5));
    tableName.erase(0, tableName.find_first_not_of(" \t\n\r"));
    tableName.erase(tableName.find_last_not_of(" \t\n\r") + 1);
    
    Massive columns;
    if (tableName == "user") {
        columns.AddEnd("user_id"); columns.AddEnd("username"); columns.AddEnd("key");
    } else if (tableName == "user_lot") {
        columns.AddEnd("user_id"); columns.AddEnd("lot_id"); columns.AddEnd("quantity");
    } else if (tableName == "order") {
        columns.AddEnd("order_id"); columns.AddEnd("user_id"); columns.AddEnd("pair_id");
        columns.AddEnd("quantity"); columns.AddEnd("price"); columns.AddEnd("type"); columns.AddEnd("closed");
    } else if (tableName == "lot") {
        columns.AddEnd("lot_id"); columns.AddEnd("name");
    } else if (tableName == "pair") {
        columns.AddEnd("pair_id"); columns.AddEnd("first_lot_id"); columns.AddEnd("second_lot_id");
    }
    
    json result = json::array();
    istringstream stream(csv);
    string line;
    
    while (getline(stream, line)) {
        //очистка от мусора
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty() || line.find("Error") != string::npos || 
            line.find("Inserted") != string::npos || line == "OK") continue;
        
        json row;
        istringstream lineStream(line);
        string value;
        int colIndex = 0;
        
        while (getline(lineStream, value, ',') && colIndex < columns.Length()) {
            size_t first = value.find_first_not_of(" \t\r");
            size_t last = value.find_last_not_of(" \t\r");
            if (first != string::npos && last != string::npos) {
                value = value.substr(first, last - first + 1);
            } else if (first == string::npos) {
                value = ""; 
            }            
            
            row[columns.GetAt(colIndex)] = value;
            colIndex++;
        }
        
        if (!row.empty()) result.push_back(row);
    }
    
    return result.dump();
}

string Exchange::generateKey() {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 15);
    const char* hex = "0123456789abcdef";
    string key;
    for (int i = 0; i < 32; i++) key += hex[dis(gen)];
    return key;
}

void Exchange::initializeLots(const json& config) {
    auto lots = config["lots"];
    
    // ИСПРАВЛЕНО: Добавили WHERE lot_id = lot_id, чтобы база не ругалась
    string result = sendToDatabase("SELECT name FROM lot WHERE lot_id > '0'");
    json existingLotsJson = json::parse(result);
    
    Massive existingNames;
    
    for (auto& row : existingLotsJson) {
        if (row.contains("name")) {
            existingNames.AddEnd(row["name"].get<string>());
        }
    }

    // Расчет ID
    int nextId = existingNames.Length() + 1;

    for (size_t i = 0; i < lots.size(); i++) {
        string lotName = lots[i].get<string>();
        
        bool exists = false;
        for (int j = 0; j < existingNames.Length(); j++) {
            if (existingNames.GetAt(j) == lotName) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            cout << "Creating lot: " << lotName << "\n";
            sendToDatabase("INSERT INTO lot VALUES ('" + to_string(nextId++) + "', '" + lotName + "')");
            existingNames.AddEnd(lotName); 
        }
    }
}

void Exchange::initializePairs() {
    string lotsQuery = "SELECT lot_id, name FROM lot WHERE lot_id > '0'";
    string lotsResult = sendToDatabase(lotsQuery);
    json lots = json::parse(lotsResult);
    
    string pairsResult = sendToDatabase("SELECT first_lot_id, second_lot_id FROM pair WHERE pair_id > '0'");
    json existingPairsJson = json::parse(pairsResult);
    
    Massive existingPairsList;
    
    for (auto& row : existingPairsJson) {
        if (row.contains("first_lot_id") && row.contains("second_lot_id")) {
            string p = row["first_lot_id"].get<string>() + ":" + row["second_lot_id"].get<string>();
            existingPairsList.AddEnd(p);
        }
    }
    
    //расчет ID для пар
    int nextPairId = existingPairsList.Length() + 1;
    
    for (size_t i = 0; i < lots.size(); i++) {
        for (size_t j = 0; j < lots.size(); j++) {
            string id1 = lots[i]["lot_id"].get<string>();
            string id2 = lots[j]["lot_id"].get<string>();
            
            if (id1 == id2) continue; 
            
            string currentPairKey = id1 + ":" + id2;
            
            bool pairExists = false;
            for (int k = 0; k < existingPairsList.Length(); k++) {
                if (existingPairsList.GetAt(k) == currentPairKey) {
                    pairExists = true;
                    break;
                }
            }
            
            if (!pairExists) {
                cout << "Creating pair: " << lots[i]["name"].get<string>() << " -> " << lots[j]["name"].get<string>() << "\n";
                //передача ID
                sendToDatabase("INSERT INTO pair VALUES ('" + to_string(nextPairId++) + "', '" + id1 + "', '" + id2 + "')");
                existingPairsList.AddEnd(currentPairKey);
            }
        }
    }
}

json Exchange::createUser(const json& request) {
    string username = request["username"];
    string key = generateKey();
    
    string query = "SELECT user_id, username, key FROM user WHERE user_id > '0'";
    string result = sendToDatabase(query);
    json users = json::parse(result);
    int nextUserId = users.size() + 1;
    string userId = to_string(nextUserId);
    
    query = "INSERT INTO user VALUES ('" + userId + "', '" + username + "', '" + key + "')";
    sendToDatabase(query);
    
    string lotsQuery = "SELECT lot_id, name FROM lot WHERE lot_id > '0'";
    string lotsResult = sendToDatabase(lotsQuery);
    json lots = json::parse(lotsResult);
    
    for (auto& lot : lots) {
        query = "INSERT INTO user_lot VALUES ('" + 
               userId + "', '" + lot["lot_id"].get<string>() + "', '1000')";
        sendToDatabase(query);
    }
    
    return json{{"key", key}};
}

void Exchange::matchOrders(const string& pairId, const string& type) {
    string oppositeType = (type == "buy") ? "sell" : "buy";
    
    cout << "[MATCH] Checking pair " << pairId << ", type " << type << " (looking for " << oppositeType << ")\n";

    string query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE pair_id = '" + pairId + "' AND type = '" + oppositeType + "'";
    string result = sendToDatabase(query);
    json oppositeOrders = json::parse(result);
    
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE pair_id = '" + pairId + "' AND type = '" + type + "'";
    result = sendToDatabase(query);
    json currentOrders = json::parse(result);
    
    if (currentOrders.empty()) {
        cout << "[MATCH] No current orders found in DB.\n";
        return;
    }
    
    json currentOrder = currentOrders[currentOrders.size() - 1];
    
    double currentQty = stod(currentOrder["quantity"].get<string>());
    if (currentQty <= 0.000001) {
         cout << "[MATCH] Current order is already closed (qty=0).\n";
         return;
    }

    double currentPrice = stod(currentOrder["price"].get<string>());
    string currentOrderId = currentOrder["order_id"];
    string currentUserId = currentOrder["user_id"];
    
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + pairId + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    string firstLotId = pair["first_lot_id"];
    string secondLotId = pair["second_lot_id"];
    
    cout << "[MATCH] Found " << oppositeOrders.size() << " potential opposite orders.\n";

    for (auto& oppositeOrder : oppositeOrders) {
        if (currentQty <= 0.000001) break;
        
        double oppositeQty = stod(oppositeOrder["quantity"].get<string>());
        
        if (oppositeQty <= 0.000001) {
            continue;
        }

        double oppositePrice = stod(oppositeOrder["price"].get<string>());
        string oppositeOrderId = oppositeOrder["order_id"];
        string oppositeUserId = oppositeOrder["user_id"];
        
        bool priceMatch = (type == "buy") ? (currentPrice >= oppositePrice) : (currentPrice <= oppositePrice);
        
        if (!priceMatch) {
            cout << "[MATCH] Price mismatch: " << currentPrice << " vs " << oppositePrice << "\n";
            continue;
        }
        
        cout << "[MATCH] MATCHED! Trade price: " << oppositePrice << "\n";

        double matchQty = min(currentQty, oppositeQty);
        double tradePrice = min(currentPrice, oppositePrice);
        string timestamp = to_string(time(nullptr));
        
        string buyerUserId;
        double buyerOriginalPrice = 0.0;

        if (type == "buy") {
            buyerUserId = currentUserId;
            buyerOriginalPrice = currentPrice;
            
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            json bal = json::parse(result)[0];
            double newQty = stod(bal["quantity"].get<string>()) + matchQty;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
            
            double cost = matchQty * tradePrice;
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            json bal2 = json::parse(result)[0]; 
            newQty = stod(bal2["quantity"].get<string>()) + cost;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
            
        } else {
            buyerUserId = oppositeUserId;
            buyerOriginalPrice = oppositePrice;
            
            double cost = matchQty * tradePrice;
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            json bal = json::parse(result)[0];
            double newQty = stod(bal["quantity"].get<string>()) + cost;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
            
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            json bal2 = json::parse(result)[0];
            double newQty2 = stod(bal2["quantity"].get<string>()) + matchQty;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty2) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
        }

        if (buyerOriginalPrice > tradePrice) {
            double refund = (buyerOriginalPrice - tradePrice) * matchQty;
            cout << "[MATCH] Refunding " << refund << " to user " << buyerUserId << "\n";
            
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + buyerUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            json balRefund = json::parse(result)[0];
            double newQty = stod(balRefund["quantity"].get<string>()) + refund;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + buyerUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
        }
        
        currentQty -= matchQty;
        oppositeQty -= matchQty;
        
        if (oppositeQty <= 0.000001) {
            query = "UPDATE order SET closed = '" + timestamp + "', quantity = '0' WHERE order_id = '" + oppositeOrderId + "'";
        } else {
            query = "UPDATE order SET quantity = '" + to_string(oppositeQty) + "' WHERE order_id = '" + oppositeOrderId + "'";
        }
        sendToDatabase(query);
        
        if (currentQty <= 0.000001) {
            query = "UPDATE order SET closed = '" + timestamp + "', quantity = '0' WHERE order_id = '" + currentOrderId + "'";
        } else {
            query = "UPDATE order SET quantity = '" + to_string(currentQty) + "' WHERE order_id = '" + currentOrderId + "'";
        }
        sendToDatabase(query);
    }
}

json Exchange::createOrder(const json& request, const string& userKey) {
    string query = "SELECT user_id, username, key FROM user WHERE key = '" + userKey + "'";
    string result = sendToDatabase(query);
    json users = json::parse(result);
    if (users.empty()) return json{{"error", "Invalid key"}};
    
    string userId = users[0]["user_id"];
    string pairId = to_string(request["pair_id"].get<int>());
    double quantity = request["quantity"];
    double price = request["price"];
    string type = request["type"];
    
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + pairId + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    
    string lotId;
    double cost;
    if (type == "buy") {
        lotId = pair["second_lot_id"].get<string>();
        cost = quantity * price;
    } else {
        lotId = pair["first_lot_id"].get<string>();
        cost = quantity;
    }
    
    query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    result = sendToDatabase(query);
    json balance = json::parse(result)[0];
    double currentBalance = stod(balance["quantity"].get<string>());
    
    if (currentBalance < cost) return json{{"error", "Insufficient balance"}};
    
    query = "UPDATE user_lot SET quantity = '" + to_string(currentBalance - cost) + "' WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    sendToDatabase(query);
    
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE user_id > '0'";
    result = sendToDatabase(query);
    json orders = json::parse(result);
    int nextOrderId = orders.size() + 1;
    
    query = "INSERT INTO order VALUES ('" + to_string(nextOrderId) + "', '" +
           userId + "', '" + pairId + "', '" + to_string(quantity) + "', '" + 
           to_string(price) + "', '" + type + "', '')";
    sendToDatabase(query);
    
    matchOrders(pairId, type);
    
    return json{{"order_id", nextOrderId}};
}

json Exchange::getOrders() {
    string query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE user_id > '0'";
    string result = sendToDatabase(query);
    return json::parse(result);
}

json Exchange::deleteOrder(const json& request, const string& userKey) {
    string query = "SELECT user_id, username, key FROM user WHERE key = '" + userKey + "'";
    string result = sendToDatabase(query);
    json users = json::parse(result);
    if (users.empty()) return json{{"error", "Invalid key"}};
    
    string userId = users[0]["user_id"];
    string orderId = to_string(request["order_id"].get<int>());
    
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE order_id = '" + orderId + "'";
    result = sendToDatabase(query);
    json orders = json::parse(result);
    if (orders.empty() || orders[0]["user_id"] != userId) return json{{"error", "Order not found"}};
    
    json order = orders[0];
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + order["pair_id"].get<string>() + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    
    string type = order["type"];
    double quantity = stod(order["quantity"].get<string>());
    double price = stod(order["price"].get<string>());
    
    string lotId;
    double refund;
    if (type == "buy") {
        lotId = pair["second_lot_id"].get<string>();
        refund = quantity * price;
    } else {
        lotId = pair["first_lot_id"].get<string>();
        refund = quantity;
    }
    
    query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    result = sendToDatabase(query);
    json balance = json::parse(result)[0];
    double currentBalance = stod(balance["quantity"].get<string>());
    
    query = "UPDATE user_lot SET quantity = '" + to_string(currentBalance + refund) + "' WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    sendToDatabase(query);
    
    query = "DELETE FROM order WHERE order_id = '" + orderId + "'";
    sendToDatabase(query);
    
    return json{{"success", true}};
}

json Exchange::getLots() {
    string query = "SELECT lot_id, name FROM lot WHERE lot_id > '0'";
    string result = sendToDatabase(query);
    return json::parse(result);
}

json Exchange::getPairs() {
    string query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id > '0'";
    string result = sendToDatabase(query);
    json pairs = json::parse(result);
    json response = json::array();
    for (auto& pair : pairs) {
        response.push_back({
            {"pair_id", stoi(pair["pair_id"].get<string>())},
            {"sale_lot_id", stoi(pair["first_lot_id"].get<string>())},
            {"buy_lot_id", stoi(pair["second_lot_id"].get<string>())}
        });
    }
    return response;
}

json Exchange::getBalance(const string& userKey) {
    string query = "SELECT user_id, username, key FROM user WHERE key = '" + userKey + "'";
    string result = sendToDatabase(query);
    json users = json::parse(result);
    if (users.empty()) return json{{"error", "Invalid key"}};
    
    string userId = users[0]["user_id"];
    query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + userId + "'";
    result = sendToDatabase(query);
    json balances = json::parse(result);
    json response = json::array();
    for (auto& balance : balances) {
        response.push_back({
            {"lot_id", stoi(balance["lot_id"].get<string>())},
            {"quantity", stod(balance["quantity"].get<string>())}
        });
    }
    return response;
}