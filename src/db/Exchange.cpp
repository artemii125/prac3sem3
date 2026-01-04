#include "../../include/dbase/Exchange.h"
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
    
    // Преобразование CSV ответа в JSON
    if (query.find("SELECT") == 0) {
        return csvToJson(result, query);
    } else if (query.find("INSERT") == 0) {
        // Для INSERT возвращаем ID из запроса
        size_t valuesPos = query.find("VALUES");
        if (valuesPos != string::npos) {
            size_t openParen = query.find('(', valuesPos);
            size_t firstQuote = query.find('\'', openParen);
            size_t secondQuote = query.find('\'', firstQuote + 1);
            string id = query.substr(firstQuote + 1, secondQuote - firstQuote - 1);
            
            // Определяем имя ID поля из таблицы
            size_t intoPos = query.find("INTO");
            size_t spaceAfterInto = query.find(' ', intoPos + 5);
            string tableName = query.substr(intoPos + 5, spaceAfterInto - (intoPos + 5));
            
            return "{\"" + tableName + "_id\":\"" + id + "\"}";
        }
    }
    return result;
}

string Exchange::csvToJson(const string& csv, const string& query) {
    // Определяем таблицу из запроса
    size_t fromPos = query.find("FROM");
    size_t wherePos = query.find("WHERE");
    if (fromPos == string::npos) return "[]";
    
    string tableName = query.substr(fromPos + 5, wherePos - (fromPos + 5));
    // Убираем пробелы
    tableName.erase(0, tableName.find_first_not_of(" \t\n\r"));
    tableName.erase(tableName.find_last_not_of(" \t\n\r") + 1);
    
    // Определяем колонки для таблицы
    vector<string> columns;
    if (tableName == "user") {
        columns = {"user_id", "username", "key"};
    } else if (tableName == "user_lot") {
        columns = {"user_id", "lot_id", "quantity"};
    } else if (tableName == "order") {
        columns = {"order_id", "user_id", "pair_id", "quantity", "price", "type", "closed"};
    } else if (tableName == "lot") {
        columns = {"lot_id", "name"};
    } else if (tableName == "pair") {
        columns = {"pair_id", "first_lot_id", "second_lot_id"};
    }
    
    json result = json::array();
    istringstream stream(csv);
    string line;
    
    while (getline(stream, line)) {
        if (line.empty() || line.find("Error") != string::npos || 
            line.find("Inserted") != string::npos || line == "OK") continue;
        
        json row;
        istringstream lineStream(line);
        string value;
        size_t colIndex = 0;
        
        while (getline(lineStream, value, ',') && colIndex < columns.size()) {
            row[columns[colIndex]] = value;
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
    for (size_t i = 0; i < lots.size(); i++) {
        string query = "SELECT lot_id, name FROM lot WHERE name = '" + lots[i].get<string>() + "'";
        string result = sendToDatabase(query);
        if (result.find("[]") != string::npos) {
            query = "INSERT INTO lot VALUES ('" + to_string(i+1) + "', '" + lots[i].get<string>() + "')";
            sendToDatabase(query);
        }
    }
}

void Exchange::initializePairs() {
    string lotsQuery = "SELECT lot_id, name FROM lot WHERE lot_id = '1' OR lot_id = '2' OR lot_id = '3' OR lot_id = '4' OR lot_id = '5' OR lot_id = '6' OR lot_id = '7' OR lot_id = '8' OR lot_id = '9' OR lot_id = '10'";
    string lotsResult = sendToDatabase(lotsQuery);
    json lots = json::parse(lotsResult);
    
    int pairId = 1;
    for (size_t i = 0; i < lots.size(); i++) {
        for (size_t j = 0; j < lots.size(); j++) {
            if (i != j) {
                string query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE first_lot_id = '" + lots[i]["lot_id"].get<string>() + 
                                  "' AND second_lot_id = '" + lots[j]["lot_id"].get<string>() + "'";
                string result = sendToDatabase(query);
                if (result.find("[]") != string::npos) {
                    query = "INSERT INTO pair VALUES ('" + 
                           to_string(pairId++) + "', '" + lots[i]["lot_id"].get<string>() + 
                           "', '" + lots[j]["lot_id"].get<string>() + "')";
                    sendToDatabase(query);
                }
            }
        }
    }
}

json Exchange::createUser(const json& request) {
    string username = request["username"];
    string key = generateKey();
    
    // Получаем следующий user_id
    string query = "SELECT user_id, username, key FROM user WHERE user_id = '1' OR user_id = '2' OR user_id = '3' OR user_id = '4' OR user_id = '5' OR user_id = '6' OR user_id = '7' OR user_id = '8' OR user_id = '9' OR user_id = '10'";
    string result = sendToDatabase(query);
    json users = json::parse(result);
    int nextUserId = users.size() + 1;
    string userId = to_string(nextUserId);
    
    query = "INSERT INTO user VALUES ('" + userId + "', '" + username + "', '" + key + "')";
    sendToDatabase(query);
    
    string lotsQuery = "SELECT lot_id, name FROM lot WHERE lot_id = '1' OR lot_id = '2' OR lot_id = '3' OR lot_id = '4' OR lot_id = '5' OR lot_id = '6' OR lot_id = '7' OR lot_id = '8' OR lot_id = '9' OR lot_id = '10'";
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

    // 1. ИЗМЕНЕНИЕ: Убрали "AND closed = ''" из SQL. Запрашиваем ВСЕ ордера этого типа.
    string query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE pair_id = '" + pairId + "' AND type = '" + oppositeType + "'";
    string result = sendToDatabase(query);
    json oppositeOrders = json::parse(result);
    
    // 2. ИЗМЕНЕНИЕ: Здесь тоже убрали "AND closed = ''"
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE pair_id = '" + pairId + "' AND type = '" + type + "'";
    result = sendToDatabase(query);
    json currentOrders = json::parse(result);
    
    if (currentOrders.empty()) {
        cout << "[MATCH] No current orders found in DB.\n";
        return;
    }
    
    // Берем последний ордер
    json currentOrder = currentOrders[currentOrders.size() - 1];
    
    // 3. ИЗМЕНЕНИЕ: Проверяем, активен ли текущий ордер (по количеству)
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
        
        // 4. ИЗМЕНЕНИЕ: Фильтруем закрытые ордера здесь, в C++
        if (oppositeQty <= 0.000001) {
            continue; // Пропускаем уже исполненные ордера
        }

        double oppositePrice = stod(oppositeOrder["price"].get<string>());
        string oppositeOrderId = oppositeOrder["order_id"];
        string oppositeUserId = oppositeOrder["user_id"];
        
        // Проверка цены
        bool priceMatch = (type == "buy") ? (currentPrice >= oppositePrice) : (currentPrice <= oppositePrice);
        
        if (!priceMatch) {
            cout << "[MATCH] Price mismatch: " << currentPrice << " vs " << oppositePrice << "\n";
            continue;
        }
        
        cout << "[MATCH] MATCHED! Trade price: " << oppositePrice << "\n";

        double matchQty = min(currentQty, oppositeQty);
        double tradePrice = oppositePrice;
        string timestamp = to_string(time(nullptr));
        
        if (type == "buy") {
            // Текущий (Покупатель): +Товар
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            json bal = json::parse(result)[0];
            double newQty = stod(bal["quantity"].get<string>()) + matchQty;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
            
            // Встречный (Продавец): +Деньги
            double cost = matchQty * tradePrice;
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            json bal2 = json::parse(result)[0]; 
            newQty = stod(bal2["quantity"].get<string>()) + cost;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
            
            // Возврат разницы покупателю, если цена матчинга лучше
            cout << "[REFUND CHECK] tradePrice=" << tradePrice << " currentPrice=" << currentPrice << "\n";
            if (tradePrice < currentPrice) {
                double priceDiff = (currentPrice - tradePrice) * matchQty;
                cout << "[REFUND] Returning " << priceDiff << " to buyer (currentUser)\n";
                query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
                result = sendToDatabase(query);
                bal = json::parse(result)[0];
                newQty = stod(bal["quantity"].get<string>()) + priceDiff;
                query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
                sendToDatabase(query);
            }
            
        } else { // type == "sell"
            // Текущий (Продавец): +Деньги
            double cost = matchQty * tradePrice;
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            json bal = json::parse(result)[0];
            double newQty = stod(bal["quantity"].get<string>()) + cost;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
            
            // Встречный (Покупатель): +Товар
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            json bal2 = json::parse(result)[0];
            double newQty2 = stod(bal2["quantity"].get<string>()) + matchQty;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty2) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
            
            // Возврат разницы покупателю (opposite), если цена матчинга лучше
            double oppositeOrderPrice = stod(oppositeOrder["price"].get<string>());
            cout << "[REFUND CHECK SELL] tradePrice=" << tradePrice << " oppositeOrderPrice=" << oppositeOrderPrice << "\n";
            if (tradePrice < oppositeOrderPrice) {
                double priceDiff = (oppositeOrderPrice - tradePrice) * matchQty;
                cout << "[REFUND SELL] Returning " << priceDiff << " to buyer (oppositeUser)\n";
                query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
                result = sendToDatabase(query);
                bal2 = json::parse(result)[0];
                newQty2 = stod(bal2["quantity"].get<string>()) + priceDiff;
                query = "UPDATE user_lot SET quantity = '" + to_string(newQty2) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
                sendToDatabase(query);
            }
        }
        
        currentQty -= matchQty;
        oppositeQty -= matchQty;
        
        // Обновляем ордера
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
    
    // Для пары first/second (RUB/USD):
    // buy: покупаем first (RUB), платим second (USD) - списываем quantity * price USD
    // sell: продаем first (RUB), получаем second (USD) - списываем quantity RUB
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
    
    // Получаем следующий order_id
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE order_id = '1' OR order_id = '2' OR order_id = '3' OR order_id = '4' OR order_id = '5' OR order_id = '6' OR order_id = '7' OR order_id = '8' OR order_id = '9' OR order_id = '10' OR order_id = '11' OR order_id = '12' OR order_id = '13' OR order_id = '14' OR order_id = '15' OR order_id = '16' OR order_id = '17' OR order_id = '18' OR order_id = '19' OR order_id = '20'";
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
    string query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE order_id = '1' OR order_id = '2' OR order_id = '3' OR order_id = '4' OR order_id = '5' OR order_id = '6' OR order_id = '7' OR order_id = '8' OR order_id = '9' OR order_id = '10' OR order_id = '11' OR order_id = '12' OR order_id = '13' OR order_id = '14' OR order_id = '15' OR order_id = '16' OR order_id = '17' OR order_id = '18' OR order_id = '19' OR order_id = '20'";
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
    string query = "SELECT lot_id, name FROM lot WHERE lot_id = '1' OR lot_id = '2' OR lot_id = '3' OR lot_id = '4' OR lot_id = '5' OR lot_id = '6' OR lot_id = '7' OR lot_id = '8' OR lot_id = '9' OR lot_id = '10'";
    string result = sendToDatabase(query);
    return json::parse(result);
}

json Exchange::getPairs() {
    string query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '1' OR pair_id = '2' OR pair_id = '3' OR pair_id = '4' OR pair_id = '5' OR pair_id = '6' OR pair_id = '7' OR pair_id = '8' OR pair_id = '9' OR pair_id = '10' OR pair_id = '11' OR pair_id = '12' OR pair_id = '13' OR pair_id = '14' OR pair_id = '15' OR pair_id = '16' OR pair_id = '17' OR pair_id = '18' OR pair_id = '19' OR pair_id = '20'";
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
