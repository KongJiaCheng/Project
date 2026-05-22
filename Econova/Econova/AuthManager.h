#pragma once
#include <string>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <vector>
#include <tuple>

struct PlayerStats {
    float hunger;
    float thirst;
    int currency;
    int level;
    int exp;
    float elapsedTime;
    int gender; // -1: none, 1: male, 2: female
    bool isFirstTime;
};

// Character persistence methods

struct StoredStructure {
    int id;
    float x, y, health;
    float rotation;
    bool isOpen = false;
    float shearY = 0.f;
};

struct StoredCampfire {
    float x, y, timeRemaining;
    bool isCrafted;
};

struct QuestInfo {
    int questID;
    int progress;
    std::string lastReset;
    bool isCompleted;
};

struct OnlinePlayer {
    std::string username;
    float x, y;
    std::string pullBy;
};

struct TradeRecord {
    std::string username;
    std::string actionType;
    std::string itemName;
    int quantity;
    int pricePerUnit;
    int totalGold;
    std::string tradeTime;
};

class AuthManager {
public:
    AuthManager() : driver(nullptr), con(nullptr) {}
    static AuthManager& getInstance();

    void shutdownDB();
    void savePlayerStats(const sql::SQLString& username, float hunger, float thirst, int currency, int level, int exp, float elapsedTime);
    PlayerStats loadPlayerStats(const sql::SQLString& username);
    void setFirstTimeFinished(const sql::SQLString& username);
    void saveGender(const sql::SQLString& username, int gender);

    QuestInfo getQuestInfo(const sql::SQLString& username);
    void updateQuestProgress(const sql::SQLString& username, int progress, bool completed);
    void resetQuest(const sql::SQLString& username, int questID, const sql::SQLString& date);

    void updateOnlineStatus(const sql::SQLString& username, float x, float y);
    std::vector<OnlinePlayer> getOnlinePlayers(const sql::SQLString& excludeUser);
    OnlinePlayer getSelfOnlineInfo(const sql::SQLString& username);
    void sendCommand(const sql::SQLString& fromUser, const sql::SQLString& toUser, const sql::SQLString& cmd);
    void clearPull(const sql::SQLString& username);

    bool sendChatMessage(const sql::SQLString& username, const sql::SQLString& message, std::string& outError);
    std::vector<std::pair<std::string, std::string>> fetchChatMessages(int limit = 10);

    bool initDB();
    bool registerUser(const sql::SQLString& username, const sql::SQLString& password, const sql::SQLString& email);
    bool loginUser(const sql::SQLString& username, const sql::SQLString& password, std::string& errMessage);

    std::string getUserRole(const sql::SQLString& username);
    bool setUserRole(const std::string& username, const std::string& roles);
    bool banUser(const std::string& target, const std::string& durationStr, std::string& outMsg);
    bool unbanUser(const std::string& target, std::string& outMsg);
    bool muteUser(const std::string& target, int minutes, std::string& outMsg);
    bool unmuteUser(const std::string& target, std::string& outMsg);
    sql::SQLString generateForgotOTP(const sql::SQLString& email);
    bool resetPassword(const sql::SQLString& email, const sql::SQLString& otp, const sql::SQLString& newPassword);

    bool saveInventory(const sql::SQLString& username, const std::vector<std::pair<int, int>>& inventoryData);
    std::vector<std::pair<int, int>> loadInventory(const sql::SQLString& username);

    void addTradeRecord(const sql::SQLString& username, const std::string& actionType, const std::string& itemName, int quantity, int pricePerUnit, int totalGold);
    std::vector<TradeRecord> getRecentTrades(int limit = 5);

    void saveStructures(const sql::SQLString& username, const std::vector<StoredStructure>& structures);
    std::vector<StoredStructure> loadStructures(const sql::SQLString& username);

    void saveCampfires(const sql::SQLString& username, const std::vector<StoredCampfire>& campfires);
    std::vector<StoredCampfire> loadCampfires(const sql::SQLString& username);

private:
    sql::mysql::MySQL_Driver* driver = nullptr;
    sql::Connection* con = nullptr;
};
