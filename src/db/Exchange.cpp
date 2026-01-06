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
//отправка sql запроса
string Exchange::sendToDatabase(const string& query) {
    send(dbSocket, query.c_str(), query.length(), 0); //отправляем текст запроса в сокет
    char buffer[65536] = {0}; 
    int bytesRead = recv(dbSocket, buffer, 65536, 0); //сохранение колва байт из ответа
    string result(buffer, bytesRead);
    
    if (query.find("SELECT") == 0) {
        return csvToJson(result, query);
    } else if (query.find("INSERT") == 0) {
        size_t valuesPos = query.find("VALUES");
        if (valuesPos != string::npos) {
            size_t openParen = query.find('(', valuesPos); //находим скобку
            size_t firstQuote = query.find('\'', openParen); //находим первыц символ
            size_t secondQuote = query.find('\'', firstQuote + 1); //находим второй
            string id = query.substr(firstQuote + 1, secondQuote - firstQuote - 1); //берем id
            
            size_t intoPos = query.find("INTO");
            size_t spaceAfterInto = query.find(' ', intoPos + 5); //поиск пробела после имени таблицы
            string tableName = query.substr(intoPos + 5, spaceAfterInto - (intoPos + 5)); //вырезаем имя таблицы
            
            return "{\"" + tableName + "_id\":\"" + id + "\"}"; //возврат json c id
        }
    }
    return result;
}

string Exchange::csvToJson(const string& csv, const string& query) {
    size_t fromPos = query.find("FROM");
    size_t wherePos = query.find("WHERE");
    if (fromPos == string::npos) return "[]";
    
    string tableName = query.substr(fromPos + 5, wherePos - (fromPos + 5)); //вырезаем имя таблицы
    tableName.erase(0, tableName.find_first_not_of(" \t\n\r")); //удаление пробелов в начале имени
    tableName.erase(tableName.find_last_not_of(" \t\n\r") + 1); //удаленеи в конце
    
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
    
    json result = json::array(); //создаем пустой JSON-массив для результата
    istringstream stream(csv); //поток чтения из csv
    string line; //в перемнноц храним значения результата
    
    while (getline(stream, line)) { //здесь мы читаем уже строки
        //очистка от мусора
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty() || line.find("Error") != string::npos || 
            line.find("Inserted") != string::npos || line == "OK") continue; //пропускаем ошибки, пустые, служебные
        
        json row; //создаем JSON-объект для одной строки
        istringstream lineStream(line); //поток для разбора по разделителям
        string value;
        int colIndex = 0;
        
        while (getline(lineStream, value, ',') && colIndex < columns.Length()) { 
            size_t first = value.find_first_not_of(" \t\r"); //берем после пробелов
            size_t last = value.find_last_not_of(" \t\r"); //и до следующих
            if (first != string::npos && last != string::npos) {
                value = value.substr(first, last - first + 1); //берем чистый текст
            } else if (first == string::npos) {
                value = ""; 
            }            
            
            row[columns.GetAt(colIndex)] = value; //запись в json под именем колонки
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

void Exchange::initializeLots(const json& config) { //функция инита валют
    auto lots = config["lots"];
    //берем имеющиеся и парсим
    string result = sendToDatabase("SELECT name FROM lot WHERE lot_id > '0'");
    json existingLotsJson = json::parse(result);
    
    Massive existingNames; //тут валюты хранятся
    
    for (auto& row : existingLotsJson) {
        if (row.contains("name")) {
            existingNames.AddEnd(row["name"].get<string>());
        }
    }

    int nextId = existingNames.Length() + 1; //здесь id для последующих
    //в этом блоке сравниваются валюты из конфига
    for (size_t i = 0; i < lots.size(); i++) {
        string lotName = lots[i].get<string>();
        
        bool exists = false; //если не найдет в базе
        for (int j = 0; j < existingNames.Length(); j++) {
            if (existingNames.GetAt(j) == lotName) {
                exists = true;
                break;
            }
        }
        //добавление новой валюты, если не инитная в бд
        if (!exists) {
            cout << "Creating lot: " << lotName << "\n";
            sendToDatabase("INSERT INTO lot VALUES ('" + to_string(nextId++) + "', '" + lotName + "')"); //запись добавления валюты
            existingNames.AddEnd(lotName); //добавление в список
        }
    }
}

void Exchange::initializePairs() {
    string lotsQuery = "SELECT lot_id, name FROM lot WHERE lot_id > '0'"; //зарпос валют
    string lotsResult = sendToDatabase(lotsQuery); //ответ
    json lots = json::parse(lotsResult); //парсинг
    
    string pairsResult = sendToDatabase("SELECT first_lot_id, second_lot_id FROM pair WHERE pair_id > '0'"); //существующие пары
    json existingPairsJson = json::parse(pairsResult); //парсинг
    
    Massive existingPairsList; //тут будут хранится ключи пар
    
    //тут блок для удобного хранения пары ("1:2") 
    for (auto& row : existingPairsJson) { 
        if (row.contains("first_lot_id") && row.contains("second_lot_id")) {
            string p = row["first_lot_id"].get<string>() + ":" + row["second_lot_id"].get<string>();
            existingPairsList.AddEnd(p);
        }
    }
    
    int nextPairId = existingPairsList.Length() + 1; //расчет следующей id для пар
    
    for (size_t i = 0; i < lots.size(); i++) { //первая валюта
        for (size_t j = 0; j < lots.size(); j++) { //вторая
            string id1 = lots[i]["lot_id"].get<string>(); //id 1
            string id2 = lots[j]["lot_id"].get<string>(); //id 2
            
            if (id1 == id2) continue; //пропуск в случае если валюты одинаковые
            string currentPairKey = id1 + ":" + id2;
            
            //аналогично в этом блоке сравниваются пары валют
            bool pairExists = false; //флаг еси нет в бд
            for (int k = 0; k < existingPairsList.Length(); k++) {
                if (existingPairsList.GetAt(k) == currentPairKey) {
                    pairExists = true;
                    break;
                }
            }
            //добавление новой пары
            if (!pairExists) { 
                cout << "Creating pair: " << lots[i]["name"].get<string>() << " -> " << lots[j]["name"].get<string>() << "\n";
                sendToDatabase("INSERT INTO pair VALUES ('" + to_string(nextPairId++) + "', '" + id1 + "', '" + id2 + "')");
                existingPairsList.AddEnd(currentPairKey);
            }
        }
    }
}

json Exchange::createUser(const json& request) { //здесь создаю пользователя по запросу
    string username = request["username"];
    string key = generateKey();
    
    string query = "SELECT user_id, username, key FROM user WHERE user_id > '0'";
    string result = sendToDatabase(query);
    json users = json::parse(result);
    int nextUserId = users.size() + 1; //вычисление слеующего id
    string userId = to_string(nextUserId);
    
    query = "INSERT INTO user VALUES ('" + userId + "', '" + username + "', '" + key + "')";
    sendToDatabase(query);
    
    string lotsQuery = "SELECT lot_id, name FROM lot WHERE lot_id > '0'"; //список валют
    string lotsResult = sendToDatabase(lotsQuery);
    json lots = json::parse(lotsResult);
    
    for (auto& lot : lots) { //и для каждой валюты начисляем 1000 единиц
        query = "INSERT INTO user_lot VALUES ('" + 
               userId + "', '" + lot["lot_id"].get<string>() + "', '1000')";
        sendToDatabase(query);
    }
    
    return json{{"key", key}}; //возврат json с ключом
}

//функция матчинга ордеров
void Exchange::matchOrders(const string& pairId, const string& type) {
    string oppositeType = (type == "buy") ? "sell" : "buy";
    
    cout << "[MATCH] Checking pair " << pairId << ", type " << type << " (looking for " << oppositeType << ")\n";
    //запрос на все встречные ордеры
    string query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE pair_id = '" + pairId + "' AND type = '" + oppositeType + "'";
    string result = sendToDatabase(query);
    json oppositeOrders = json::parse(result);
    
    //запрос на текущие ордеры, те созданный
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE pair_id = '" + pairId + "' AND type = '" + type + "'";
    result = sendToDatabase(query);
    json currentOrders = json::parse(result);
    
    if (currentOrders.empty()) {
        cout << "[MATCH] No current orders found in DB.\n";
        return;
    }
    json currentOrder = currentOrders[currentOrders.size() - 1]; //берем последний созданный ордер
    
    double currentQty = stod(currentOrder["quantity"].get<string>()); //преобразуем строку в число для дальнецших операций
    //провверка на пустой, закрытый ордер
    if (currentQty <= 0.000001) {
         cout << "[MATCH] Current order is already closed (qty=0).\n";
         return;
    }
    //здесь мы распаковывем остальные данные
    double currentPrice = stod(currentOrder["price"].get<string>()); //текущая цена для сравнения с ценами других ордеров
    string currentOrderId = currentOrder["order_id"]; //id ордера
    string currentUserId = currentOrder["user_id"]; //id владельца
    
    //здесь обрабатывается информация о паре, выполняется запрос и парсинг, где первая - покупаем, вторая - отдаем
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + pairId + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    string firstLotId = pair["first_lot_id"];
    string secondLotId = pair["second_lot_id"];
    
    cout << "[MATCH] Found " << oppositeOrders.size() << " potential opposite orders.\n"; //при успешном подбоере лог о кандидатах 

    for (auto& oppositeOrder : oppositeOrders) { //работа со встречными ордерами
        if (currentQty <= 0.000001) break;
        
        double oppositeQty = stod(oppositeOrder["quantity"].get<string>()); //здесь хранится его кол-во
        
        if (oppositeQty <= 0.000001) {
            continue;
        }

        double oppositePrice = stod(oppositeOrder["price"].get<string>()); //цена встречного ордера
        string oppositeOrderId = oppositeOrder["order_id"];
        string oppositeUserId = oppositeOrder["user_id"];
        
        bool priceMatch = (type == "buy") ? (currentPrice >= oppositePrice) : (currentPrice <= oppositePrice); //при покупке и продаже ищется выгодная стоимость 
        
        if (!priceMatch) { //неподходящие отсеиваю
            cout << "[MATCH] Price mismatch: " << currentPrice << " vs " << oppositePrice << "\n";
            continue;
        }
        
        cout << "[MATCH] MATCHED! Trade price: " << oppositePrice << "\n";

        double matchQty = min(currentQty, oppositeQty); //покупка возможного кол-ва валюты у продавца
        double tradePrice = min(currentPrice, oppositePrice); //покупка по выгодной цене
        string timestamp = to_string(time(nullptr)); //флаг завршенного ордера по времени
        string buyerUserId;
        double buyerOriginalPrice = 0.0;
        if (type == "buy") { //проверка если инициатор покупатель, то текущий юзер он и его цена
            buyerUserId = currentUserId;
            buyerOriginalPrice = currentPrice;
            //начисление товара, те первая валюта, покупателю, те текущему юзеру
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            json bal = json::parse(result)[0];
            double newQty = stod(bal["quantity"].get<string>()) + matchQty;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
            //начисляются деньги, те вторая валюта, продавцу, те встречному
            double cost = matchQty * tradePrice;
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            json bal2 = json::parse(result)[0]; 
            newQty = stod(bal2["quantity"].get<string>()) + cost;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
            
        } else { //инициатор продавец 
            buyerUserId = oppositeUserId; //покупатель встреный юзер
            buyerOriginalPrice = oppositePrice; //его цена
            //начисляем деньги, те вторая валюта, продавцу, те текущему
            double cost = matchQty * tradePrice;
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            json bal = json::parse(result)[0];
            double newQty = stod(bal["quantity"].get<string>()) + cost;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + currentUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
            //начисляем товар, те первая валюта, покупателю, те встречному
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            result = sendToDatabase(query);
            json bal2 = json::parse(result)[0];
            double newQty2 = stod(bal2["quantity"].get<string>()) + matchQty;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty2) + "' WHERE user_id = '" + oppositeUserId + "' AND lot_id = '" + firstLotId + "'";
            sendToDatabase(query);
        }

        //в блоке реализовал возврат стредств, те если купили дешевле, чем заморозили
        if (buyerOriginalPrice > tradePrice) {
            double refund = (buyerOriginalPrice - tradePrice) * matchQty; //разница через объем
            cout << "[MATCH] Refunding " << refund << " to user " << buyerUserId << "\n";
            //возврат разницы покупателю, здесь вторая валюта
            query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + buyerUserId + "' AND lot_id = '" + secondLotId + "'";
            result = sendToDatabase(query);
            json balRefund = json::parse(result)[0];
            double newQty = stod(balRefund["quantity"].get<string>()) + refund;
            query = "UPDATE user_lot SET quantity = '" + to_string(newQty) + "' WHERE user_id = '" + buyerUserId + "' AND lot_id = '" + secondLotId + "'";
            sendToDatabase(query);
        }
        //вычитаем объем сделки из текущего и встречного ордеров
        currentQty -= matchQty;
        oppositeQty -= matchQty;
        //обновляем встречный в бд
        if (oppositeQty <= 0.000001) {
            query = "UPDATE order SET closed = '" + timestamp + "', quantity = '0' WHERE order_id = '" + oppositeOrderId + "'";
        } else {
            query = "UPDATE order SET quantity = '" + to_string(oppositeQty) + "' WHERE order_id = '" + oppositeOrderId + "'";
        }
        sendToDatabase(query);
        //обновляем текущий
        if (currentQty <= 0.000001) {
            query = "UPDATE order SET closed = '" + timestamp + "', quantity = '0' WHERE order_id = '" + currentOrderId + "'";
        } else {
            query = "UPDATE order SET quantity = '" + to_string(currentQty) + "' WHERE order_id = '" + currentOrderId + "'";
        }
        sendToDatabase(query);
    }
}

json Exchange::createOrder(const json& request, const string& userKey) { //в этом методе создание ордеров
    string query = "SELECT user_id, username, key FROM user WHERE key = '" + userKey + "'"; //запрос юзера по ключу
    string result = sendToDatabase(query);
    json users = json::parse(result);
    if (users.empty()) return json{{"error", "Invalid key"}};
    //получаем необходимые значения
    string userId = users[0]["user_id"];
    string pairId = to_string(request["pair_id"].get<int>());
    double quantity = request["quantity"];
    double price = request["price"];
    string type = request["type"];
    
    //получение инормации о паре
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + pairId + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    //определяем что списывать, id валюты и сумма списания
    string lotId;
    double cost;
    if (type == "buy") { //для покупки тратим вторую валюту
        lotId = pair["second_lot_id"].get<string>();
        cost = quantity * price;
    } else { //для продажи тратим ервую валюту
        lotId = pair["first_lot_id"].get<string>();
        cost = quantity;
    }
    //проверка баланса
    query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    result = sendToDatabase(query);
    json balance = json::parse(result)[0];
    double currentBalance = stod(balance["quantity"].get<string>());
    
    if (currentBalance < cost) return json{{"error", "Insufficient balance"}};
    //списываем средства
    query = "UPDATE user_lot SET quantity = '" + to_string(currentBalance - cost) + "' WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    sendToDatabase(query);
    //id для нового ордера
    query = "SELECT order_id, user_id, pair_id, quantity, price, type, closed FROM order WHERE user_id > '0'";
    result = sendToDatabase(query);
    json orders = json::parse(result);
    int nextOrderId = orders.size() + 1;
    //формирование и всавка ордера в базу
    query = "INSERT INTO order VALUES ('" + to_string(nextOrderId) + "', '" +
           userId + "', '" + pairId + "', '" + to_string(quantity) + "', '" + 
           to_string(price) + "', '" + type + "', '')";
    sendToDatabase(query);
    
    matchOrders(pairId, type); //запуск матчинга
    
    return json{{"order_id", nextOrderId}}; //возврат id ордера
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
    //проерка найден ли ордер и принадлежит ли он этому юзеру
    if (orders.empty() || orders[0]["user_id"] != userId) return json{{"error", "Order not found"}};
    json order = orders[0];
    query = "SELECT pair_id, first_lot_id, second_lot_id FROM pair WHERE pair_id = '" + order["pair_id"].get<string>() + "'";
    result = sendToDatabase(query);
    json pair = json::parse(result)[0];
    //блок за возврат средств
    string type = order["type"]; //сел или бай
    double quantity = stod(order["quantity"].get<string>());
    double price = stod(order["price"].get<string>());
    
    string lotId;
    double refund;

    if (type == "buy") { //если покупка, возвращаем всю сумму на покупку в ордере
        lotId = pair["second_lot_id"].get<string>();
        refund = quantity * price;
    } else { //если продажа, возращаем кол-во валюты
        lotId = pair["first_lot_id"].get<string>();
        refund = quantity;
    }
    //проверка текущего баланса
    query = "SELECT user_id, lot_id, quantity FROM user_lot WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    result = sendToDatabase(query);
    json balance = json::parse(result)[0];
    double currentBalance = stod(balance["quantity"].get<string>());
    //зачисьение седств обратно
    query = "UPDATE user_lot SET quantity = '" + to_string(currentBalance + refund) + "' WHERE user_id = '" + userId + "' AND lot_id = '" + lotId + "'";
    sendToDatabase(query);
    //удаляем ордер
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
    //мапинг данных
    for (auto& pair : pairs) {
        response.push_back({
            {"pair_id", stoi(pair["pair_id"].get<string>())},
            {"sale_lot_id", stoi(pair["first_lot_id"].get<string>())}, //id валюты продажи
            {"buy_lot_id", stoi(pair["second_lot_id"].get<string>())} //id валюты покупки
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