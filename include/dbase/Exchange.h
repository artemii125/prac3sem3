#ifndef EXCHANGE_H
#define EXCHANGE_H

#include <string>
#include <vector>
#include "../jsonlib/json.hpp"

using json = nlohmann::json;

class Exchange {
private:
    std::string dbHost;
    int dbPort;
    int dbSocket;
    
    std::string sendToDatabase(const std::string& query);
    std::string csvToJson(const std::string& csv, const std::string& query);
    std::string generateKey();
    void initializeLots(const json& config);
    void initializePairs();
    void matchOrders(const std::string& pairId, const std::string& type);
    
public:
    Exchange(const std::string& configPath);
    ~Exchange();
    
    json createUser(const json& request);
    json createOrder(const json& request, const std::string& userKey);
    json getOrders();
    json deleteOrder(const json& request, const std::string& userKey);
    json getLots();
    json getPairs();
    json getBalance(const std::string& userKey);
};

#endif
