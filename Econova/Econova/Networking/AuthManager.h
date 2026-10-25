#pragma once
#include <string>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <vector>
#include <tuple>
#include <mutex>
#include <atomic>
#include <map>

struct PlayerStats {
    float hunger;
    float thirst;
    int currency;
    int level;
    int exp;
    float elapsedTime;
    int gender; // -1: none, 1: male, 2: female
    bool isFirstTime;
    int hungerMultiplier;
    int thirstMultiplier;
};

// Character persistence methods

struct StoredStructure {
    int id;
    float x, y, health;
    float rotation;
    bool isOpen = false;
    float shearY = 0.f;
    std::string owner = "";
};

struct StoredChestItem {
    float chestX;
    float chestY;
    int slotIndex;
    int itemID;
    int quantity;
};

struct StoredCampfire {
    float x, y, timeRemaining;
    bool isCrafted;
    int fuel = 0;
    int inputItem = 0;
    int inputCount = 0;
    int outputItem = 0;
    int outputCount = 0;
    std::string owner = "";   // username of the owner; "" = self
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
    int gender;
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

struct P2PTradeState {
    int tradeID = -1;
    std::string initiator;
    std::string target;
    std::string status;          // pending/accepted/confirmed/cancelled/completed
    std::string initiatorItems;  // "itemID:count,itemID:count"
    std::string targetItems;
    int initiatorGold = 0;
    int targetGold = 0;
    bool initiatorReady = false;
    bool targetReady = false;
};

// ── Scheduled disaster from admin panel ──────────────────────────────────────
struct BanInfo {
    bool isBanned = false;
    std::string reason = "Violation of Community Guidelines";
    long long remainingSeconds = 0;
    bool isPermanent = false;
    std::string banDate = "";
    std::string bannedBy = "";
};

struct MuteInfo {
    bool isMuted = false;
    long long remainingSeconds = 0;
    std::string muteDate = "";
    std::string reason = "Violation of Community Guidelines";
    std::string mutedBy = "";
};

struct ActiveDisasterInfo {
    bool active = false;
    int eventID = -1;
    std::string type = "";
    float remainingSeconds = 0.f;
    int totalDuration = 0;
    std::string createdBy = "";
};

struct DisasterEvent {
    int  eventID     = -1;
    std::string type = "";          // "diarrhea" | "drought"
    long long scheduledAt = 0;      // Unix epoch seconds (UTC)
    int  durationSeconds  = 600;    // default 10 min
    std::string status    = "";     // pending / active / completed / cancelled
    std::string createdBy = "";
};

class AuthManager {
public:
    AuthManager() : driver(nullptr), con(nullptr) {}
    static AuthManager& getInstance();

    void saveMarketPrice(int itemID, float price);
    float loadMarketPrice(int itemID);
    void saveAllMarketPrices(const std::vector<std::pair<int, float>>& prices);
    std::map<int, float> loadAllMarketPrices();

    void shutdownDB();
    void savePlayerStats(const sql::SQLString& username, float hunger, float thirst, int currency, int level, int exp, float elapsedTime, int hungerMultiplier, int thirstMultiplier);
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
    void setPlayerOffline(const sql::SQLString& username);

    bool sendChatMessage(const sql::SQLString& username, const sql::SQLString& message, std::string& outError);
    std::vector<std::pair<std::string, std::string>> fetchChatMessages(int limit = 10, int sinceMsgId = 0);
    int getMaxMessageID();
    void clearPlayerChatMessages(const sql::SQLString& username);
    void cleanupInactiveChatMessages();
    void clearAllChatMessages();

    bool initDB(bool forceLocal = false);
    bool isConnected() const;
    bool isOfflineMode() const;
    void setOfflineMode(bool offline);
    std::string getDBHost() const;
    static bool isTargetedAdmin(const std::string& username, const std::string& email = "");
    bool registerUser(const sql::SQLString& username, const sql::SQLString& password, const sql::SQLString& email);
    bool loginUser(const sql::SQLString& username, const sql::SQLString& password, std::string& errMessage);

    std::string getUserRole(const sql::SQLString& username);
    bool setUserRole(const std::string& username, const std::string& roles);
    BanInfo getBanInfo(const std::string& username);
    MuteInfo getMuteStatus(const std::string& username);
    ActiveDisasterInfo getActiveDisaster();
    bool triggerDisaster(const std::string& type, const std::string& triggeredBy, std::string& outMsg);
    bool stopActiveDisaster(const std::string& triggeredBy, std::string& outMsg);

    bool banUser(const std::string& target, const std::string& durationStr, std::string& outMsg, const std::string& reason = "Violation of Community Guidelines", const std::string& adminUser = "admin");
    bool unbanUser(const std::string& target, std::string& outMsg);
    bool muteUser(const std::string& target, int minutes, std::string& outMsg, const std::string& reason = "Violation of Community Guidelines", const std::string& adminUser = "admin");
    bool unmuteUser(const std::string& target, std::string& outMsg);
    sql::SQLString generateForgotOTP(const sql::SQLString& email);
    bool resetPassword(const sql::SQLString& email, const sql::SQLString& otp, const sql::SQLString& newPassword);

    bool saveInventory(const sql::SQLString& username, const std::vector<std::pair<int, int>>& inventoryData);
    std::vector<std::pair<int, int>> loadInventory(const sql::SQLString& username);

    void addTradeRecord(const sql::SQLString& username, const std::string& actionType, const std::string& itemName, int quantity, int pricePerUnit, int totalGold);
    std::vector<TradeRecord> getRecentTrades(int limit = 5);

    // --- P2P Trading ---
    int  createP2PTrade(const std::string& from, const std::string& to);
    void acceptP2PTrade(int tradeID);
    void updateP2POffer(int tradeID, const std::string& user, const std::string& itemsCSV, int gold);
    void setP2PReady(int tradeID, const std::string& user, bool ready);
    P2PTradeState getP2PTradeState(int tradeID);
    void cancelP2PTrade(int tradeID);
    int  getPendingP2PTradeFor(const std::string& username); // returns tradeID or -1
    void completeP2PTrade(int tradeID);

    void saveStructures(const sql::SQLString& username, const std::vector<StoredStructure>& structures);
    std::vector<StoredStructure> loadStructures(const sql::SQLString& username);
    std::vector<StoredStructure> loadOtherStructures(const sql::SQLString& username);

    void saveCampfires(const sql::SQLString& username, const std::vector<StoredCampfire>& campfires);
    std::vector<StoredCampfire> loadCampfires(const sql::SQLString& username);
    std::vector<StoredCampfire> loadOtherCampfires(const sql::SQLString& username);
    bool updateCampfireCookingState(float x, float y, int fuel, int inputItem, int inputCount, int outputItem, int outputCount, float timeRemaining);

    void saveChestContents(const sql::SQLString& username, const std::vector<StoredChestItem>& items);
    std::vector<StoredChestItem> loadChestContents(const sql::SQLString& username);
    std::vector<StoredChestItem> loadOtherChestContents(const sql::SQLString& username);

    // World / server-time helpers
    int       getOrCreateWorldSeed();
    long long getServerEpochSeconds();

    // ── Disaster scheduling (admin panel) ────────────────────────────────────
    int  scheduleDisaster(const std::string& type, long long scheduledAtEpoch,
                          int durationSeconds, const std::string& createdBy);
    DisasterEvent fetchPendingDisaster();          // returns first due-now pending event; empty if none
    void markDisasterActive(int eventID);
    void markDisasterCompleted(int eventID);
    void cancelDisasterEvent(int eventID);
    std::vector<DisasterEvent> getDisasterHistory(int limit = 20);

    // Background chat notifications
    void setPendingChatNotification(const std::string& notif);
    std::string getAndClearChatNotification();

    // Death / Permadeath
    void clearPlayerData(const sql::SQLString& username); // wipes inventory, structures, campfires, chests, resets stats

private:
    void ensureCampfireColumns();
    sql::mysql::MySQL_Driver* driver = nullptr;
    sql::Connection* con = nullptr;
    std::recursive_mutex dbMutex;
    std::atomic<bool> isLocalModeActive{ false };
    std::string currentConnectedHost = "";
    std::mutex chatNotifMutex;
    std::string pendingChatNotification = "";
    BanInfo cachedBanInfo;
};
