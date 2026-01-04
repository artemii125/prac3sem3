#include "../../include/dbase/Exchange.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <random>
#include <sstream>
#include <ctime>

Exchange::Exchange(const std::string& configPath) {
    std::ifstream file(configPath);
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

std::string Exchange::sendToDatabase(const std::string& query) {
    send(dbSocket, query.c_str(), query.length(), 0);
    char buffer[65536] = {0};
    int bytesRead = recv(dbSocket, buffer, 65536, 0);
    std::string result(buffer, bytesRead);
    
    // Преобразование CSV ответа в JSON
    if (query.find("SELECT") == 0) {
        return csvToJson(result, query);
    } else if (query.find("INSERT") == 0) {
        // Для INSERT возвращаем ID из запроса
        size_t valuesPos = query.find("VALUES");
        if (valuesPos != std::string::npos) {
            size_t openParen = query.find('(', valuesPos);
            size_t firstComma = query.find(',', openParen);
            size_t firstQuote = query.find('\'', openParen);
            size_t secondQuote = query.find('\'', firstQuote + 1);
            std::string id = query.substr(firstQuote + 1, secondQuote - firstQuote - 1);
            
            // Определяем имя ID поля из таблицы
            size_t intoPos = query.find("INTO");
            size_t spaceAfterInto = query.find(' ', intoPos + 5);
            std::string tableName = query.substr(intoPos + 5, spaceAfterInto - (intoPos + 5));
            
            return "{\"" + tableName + "_id\":\"" + id + "\"}";
        }
    }
    return result;
}

std::string Exchange::csvToJson(const std::string& csv, const std::string& query) {
    // Определяем таблицу из запроса
    size_t fromPos = query.find("FROM");
    size_t wherePos = query.find("WHERE");
    if (fromPos == std::string::npos) return "[]";
    
    std::string tableName = query.substr(fromPos + 5, wherePos - (fromPos + 5));
    // Убираем пробелы
    tableName.erase(0, tableName.find_first_not_of(" \t\n\r"));
    tableName.erase(tableName.find_last_not_of(" \t\n\r") + 1);
    
    // Определяем колонки для таблицы
    std::vector<std::string> columns;
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
    std::istringstream stream(csv);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.empty() || line.find("Error") != std::string::npos || 
            line.find("Inserted") != std::string::npos) continue;
        
        json row;
        std::istringstream lineStream(line);
        std::string value;
        size_t colIndex = 0;
        
        while (std::getline(lineStream, value, ',') && colIndex < columns.size()) {
            row[columns[colIndex]] = value;
            colIndex++;
        }
        
        if (!row.empty()) result.push_back(row);
    }
    
    return result.dump();
}

std::string Exchange::generateKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    const char* hex = "0123456789abcdef";
    std::string key;
    for (int i = 0; i < 32; i++) key += hex[dis(gen)];
    return key;
}

void Exchange::initializeLots(const json& config) {
    auto lots = config["lots"];
    for (size_t i = 0; i < lots.size(); i++) {
        std::string query = "SELECT lot_id, name FROM lot WHERE name = '" + lots[i].get<std::string>() + "'";
        std::string result = sendToDatabase(query);
        if (result.find("[]") != std::string::npos) {
            query = "INSERT INTO lot VALUES ('" + std::to_string(i+1) + "', '" + lots[i].get<std::string>() + "')";
            sendToDatabase(query);
        }
    }
}

void Exchange::initializePairs() {
    std::string lotsQuery = "SELECT lot_id, name FROM lot WHERE lot_id = lot_id";
    std::string lotsResult = sendToDatabase(lotsQuery);
    json lots = json::parse(lotsResult);
    
    int pairId = 1;
    for (size_t i = 0; i < lots.size(); i++) {
        for (size_t j = 0; j < lots.size(); j++) {
            if (i != j) {
                std::string query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE first_lot_id = '" + lots[i]["lot_id"].get<std::string>() + 
                                  "' AND second_lot_id = '" + lots[j]["lot_id"].get<std::string>() + "'";
                std::string result = sendToDatabase(query);
                if (result.find("[]") != std::string::npos) {
                    query = "INSERT INTO pair VALUES ('" + 
                           std::to_string(pairId++) + "', '" + lots[i]["lot_id"].get<std::string>() + 
                           "', '" + lots[j]["lot_id"].get<std::string>() + "')";
                    sendToDatabase(query);
                }
            }
        }
    }
}

json Exchange::createUser(const json& request) {
    std::string username = request["username"];
    std::string key = generateKey();
    
    std::string query = "INSERT INTO user (username, key) VALUES ('" + username + "', '" + key + "')";
    std::string result = sendToDatabase(query);
    json userResult = json::parse(result);
    std::string userId = userResult["user_id"];
    
    std::string lotsQuery = "SELECT lot_id, name FROM lot WHERE lot_id = lot_id";
    std::string lotsResult = sendToDatabase(lotsQuery);
    json lots = json::parse(lotsResult);
    
    for (auto& lot : lots) {
        query = "INSERT INTO user_lot (user_id, lot_id, quantity) VALUES ('" + 
               userId + "', '" + lot["lot_id"].get<std::string>() + "', '1000')";
        sendToDatabase(query);
    }
    
    return json{{"key", key}};
}

void Exchange::matchOrders(const std::string& pairId, const std::string& type) {
    std::string oppositeType = (type == "buy") ? "sell" : "buy";
    std::string query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE pair_id = '" + pairId + "' AND type = '" + oppositeType + "' AND closed = ''";
    std::string result = sendToDatabase(query);
    json oppositeOrders = json::parse(result);
    
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE pair_id = '" + pairId + "' AND type = '" + type + "' AND closed = ''";
    result = sendToDatabase(query);
    json currentOrders = json::parse(result);
    if (currentOrders.empty()) return;
    
    json currentOrder = currentOrders[currentOrders.size() - 1];
    double currentQty = std::stod(currentOrder["quantity"].get<std::string>());
    double currentPrice = std::stod(currentOrder["price"].get<std::string>());
    std::string currentOrderId = currentOrder["order_id"];
    std::string currentUserId = currentOrder["user_id"];
    
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + pairId + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    std::string firstLotId = pair["first_lot_id"];
    std::string secondLotId = pair["second_lot_id"];
    
    for (auto& oppositeOrder : oppositeOrders) {
        if (currentQty <= 0) break;
        
        double oppositeQty = std::stod(oppositeOrder["quantity"].get<std::string>());
        double oppositePrice = std::stod(oppositeOrder["price"].get<std::string>());
        std::string oppositeOrderId = oppositeOrder["order_id"];
        std::string oppositeUserId = oppositeOrder["user_id"];
        
        bool priceMatch = (type == "buy") ? (currentPrice >= oppositePrice) : (currentPrice <= oppositePrice);
        if (!priceMatch) continue;
        
        double matchQty = std::min(currentQty, oppositeQty);
        double tradePrice = oppositePrice;
        std::string timestamp = std::to_string(std::time(nullptr));
        
        if (type == "buy") {
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            json bal = json::parse(result)[0];
            double newQty = std::stod(bal["quantity"].get<std::string>()) + matchQty;
            query = "UPDATE user_lot SET quantity = '" + std::to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
            
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            bal = json::parse(result)[0];
            newQty = std::stod(bal["quantity"].get<std::string>()) - matchQty;
            query = "UPDATE user_lot SET quantity = '" + std::to_string(newQty) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
            
            double cost = matchQty * tradePrice;
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            bal = json::parse(result)[0];
            newQty = std::stod(bal["quantity"].get<std::string>()) + cost;
            query = "UPDATE user_lot SET quantity = '" + std::to_string(newQty) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
        } else {
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            json bal = json::parse(result)[0];
            double newQty = std::stod(bal["quantity"].get<std::string>()) + matchQty;
            query = "UPDATE user_lot SET quantity = '" + std::to_string(newQty) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
            
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            bal = json::parse(result)[0];
            newQty = std::stod(bal["quantity"].get<std::string>()) - matchQty;
            query = "UPDATE user_lot SET quantity = '" + std::to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
            
            double cost = matchQty * tradePrice;
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            bal = json::parse(result)[0];
            newQty = std::stod(bal["quantity"].get<std::string>()) + cost;
            query = "UPDATE user_lot SET quantity = '" + std::to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
        }
        
        currentQty -= matchQty;
        oppositeQty -= matchQty;
        
        if (oppositeQty == 0) {
            query = "UPDATE order SET closed = '" + timestamp + "' WHERE order_id = '" + oppositeOrderId + "'";
            sendToDatabase(query);
        } else {
            query = "UPDATE order SET quantity = '" + std::to_string(oppositeQty) + "' WHERE order_id = '" + oppositeOrderId + "'";
            sendToDatabase(query);
        }
        
        if (currentQty == 0) {
            query = "UPDATE order SET closed = '" + timestamp + "' WHERE order_id = '" + currentOrderId + "'";
            sendToDatabase(query);
        } else {
            query = "UPDATE order SET quantity = '" + std::to_string(currentQty) + "' WHERE order_id = '" + currentOrderId + "'";
            sendToDatabase(query);
        }
    }
}

json Exchange::createOrder(const json& request, const std::string& userKey) {
    std::string query = "SELECT user_id, username, key FROM user WHERE key = '" + userKey + "'";
    std::string result = sendToDatabase(query);
    json users = json::parse(result);
    if (users.empty()) return json{{"error", "Invalid key"}};
    
    std::string userId = users[0]["user_id"];
    std::string pairId = std::to_string(request["pair_id"].get<int>());
    double quantity = request["quantity"];
    double price = request["price"];
    std::string type = request["type"];
    
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + pairId + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    
    std::string lotId = (type == "buy") ? pair["second_lot_id"].get<std::string>() : pair["first_lot_id"].get<std::string>();
    double cost = (type == "buy") ? (quantity * price) : quantity;
    
    query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    result = sendToDatabase(query);
    json balance = json::parse(result)[0];
    double currentBalance = std::stod(balance["quantity"].get<std::string>());
    
    if (currentBalance < cost) return json{{"error", "Insufficient balance"}};
    
    query = "UPDATE user_lot SET quantity = '" + std::to_string(currentBalance - cost) + "' WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    sendToDatabase(query);
    
    query = "INSERT INTO order (user_id, pair_id, quantity, price, type, closed) VALUES ('" + 
           userId + "', '" + pairId + "', '" + std::to_string(quantity) + "', '" + 
           std::to_string(price) + "', '" + type + "', '')";
    result = sendToDatabase(query);
    json orderResult = json::parse(result);
    
    matchOrders(pairId, type);
    
    return json{{"order_id", std::stoi(orderResult["order_id"].get<std::string>())}};
}

json Exchange::getOrders() {
    std::string query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE order_id = order_id";
    std::string result = sendToDatabase(query);
    return json::parse(result);
}

json Exchange::deleteOrder(const json& request, const std::string& userKey) {
    std::string query = "SELECT user_id, username, key FROM user WHERE key = '" + userKey + "'";
    std::string result = sendToDatabase(query);
    json users = json::parse(result);
    if (users.empty()) return json{{"error", "Invalid key"}};
    
    std::string userId = users[0]["user_id"];
    std::string orderId = std::to_string(request["order_id"].get<int>());
    
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE order_id = '" + orderId + "'";
    result = sendToDatabase(query);
    json orders = json::parse(result);
    if (orders.empty() || orders[0]["user_id"] != userId) return json{{"error", "Order not found"}};
    
    json order = orders[0];
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + order["pair_id"].get<std::string>() + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    
    std::string type = order["type"];
    std::string lotId = (type == "buy") ? pair["second_lot_id"].get<std::string>() : pair["first_lot_id"].get<std::string>();
    double quantity = std::stod(order["quantity"].get<std::string>());
    double price = std::stod(order["price"].get<std::string>());
    double refund = (type == "buy") ? (quantity * price) : quantity;
    
    query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    result = sendToDatabase(query);
    json balance = json::parse(result)[0];
    double currentBalance = std::stod(balance["quantity"].get<std::string>());
    
    query = "UPDATE user_lot SET quantity = '" + std::to_string(currentBalance + refund) + "' WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    sendToDatabase(query);
    
    query = "DELETE FROM order WHERE order_id = '" + orderId + "'";
    sendToDatabase(query);
    
    return json{{"success", true}};
}

json Exchange::getLots() {
    std::string query = "SELECT lot_id, name FROM lot WHERE lot_id = lot_id";
    std::string result = sendToDatabase(query);
    return json::parse(result);
}

json Exchange::getPairs() {
    std::string query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = pair_id";
    std::string result = sendToDatabase(query);
    json pairs = json::parse(result);
    json response = json::array();
    for (auto& pair : pairs) {
        response.push_back({
            {"pair_id", std::stoi(pair["pair_id"].get<std::string>())},
            {"sale_lot_id", std::stoi(pair["first_lot_id"].get<std::string>())},
            {"buy_lot_id", std::stoi(pair["second_lot_id"].get<std::string>())}
        });
    }
    return response;
}

json Exchange::getBalance(const std::string& userKey) {
    std::string query = "SELECT user_id, username, key FROM user WHERE key = '" + userKey + "'";
    std::string result = sendToDatabase(query);
    json users = json::parse(result);
    if (users.empty()) return json{{"error", "Invalid key"}};
    
    std::string userId = users[0]["user_id"];
    query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + userId + "'";
    result = sendToDatabase(query);
    json balances = json::parse(result);
    json response = json::array();
    for (auto& balance : balances) {
        response.push_back({
            {"lot_id", std::stoi(balance["lot_id"].get<std::string>())},
            {"quantity", std::stod(balance["quantity"].get<std::string>())}
        });
    }
    return response;
}
