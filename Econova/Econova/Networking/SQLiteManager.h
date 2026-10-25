#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "sqlite3.h"
#include "AuthManager.h"

struct PlayerStats;
struct StoredStructure;
struct StoredChestItem;
struct StoredCampfire;
struct QuestInfo;
struct DisasterEvent;

class SQLiteManager {
public:
    static SQLiteManager& getInstance();

    bool init(const std::string& dbPath = "econova_offline.db");
    void close();
    bool isOpen() const;

    // Authentication & Accounts
    bool registerUser(const std::string& username, const std::string& hashedPassword, const std::string& email, const std::string& role);
    bool loginUser(const std::string& username, const std::string& plainPassword, std::string& outRole, std::string& errMessage);
    std::string getUserRole(const std::string& username);
    bool setUserRole(const std::string& username, const std::string& roles);
    bool resetPassword(const std::string& email, const std::string& newHashedPassword);

    // Player Stats & Progression
    void savePlayerStats(const std::string& username, float hunger, float thirst, int currency, int level, int exp, float elapsedTime, int hungerMultiplier, int thirstMultiplier);
    PlayerStats loadPlayerStats(const std::string& username);
    void setFirstTimeFinished(const std::string& username);
    void saveGender(const std::string& username, int gender);

    // Inventory
    bool saveInventory(const std::string& username, const std::vector<std::pair<int, int>>& inventoryData);
    std::vector<std::pair<int, int>> loadInventory(const std::string& username);

    // Structures
    void saveStructures(const std::string& username, const std::vector<StoredStructure>& structures);
    std::vector<StoredStructure> loadStructures(const std::string& username);
    std::vector<StoredStructure> loadOtherStructures(const std::string& username);

    // Campfires
    void saveCampfires(const std::string& username, const std::vector<StoredCampfire>& campfires);
    std::vector<StoredCampfire> loadCampfires(const std::string& username);
    std::vector<StoredCampfire> loadOtherCampfires(const std::string& username);
    bool updateCampfireCookingState(float x, float y, int fuel, int inputItem, int inputCount, int outputItem, int outputCount, float timeRemaining);

    // Chests
    void saveChestContents(const std::string& username, const std::vector<StoredChestItem>& items);
    std::vector<StoredChestItem> loadChestContents(const std::string& username);
    std::vector<StoredChestItem> loadOtherChestContents(const std::string& username);

    // Quests
    QuestInfo getQuestInfo(const std::string& username);
    void updateQuestProgress(const std::string& username, int progress, bool completed);
    void resetQuest(const std::string& username, int questID, const std::string& date);

    // Market Prices
    void saveMarketPrice(int itemID, float price);
    float loadMarketPrice(int itemID);
    void saveAllMarketPrices(const std::vector<std::pair<int, float>>& prices);
    std::map<int, float> loadAllMarketPrices();

    // World Seed
    int getOrCreateWorldSeed();

    // Ban & Mute & Disasters
    BanInfo getBanInfo(const std::string& username);
    MuteInfo getMuteStatus(const std::string& username);
    ActiveDisasterInfo getActiveDisaster();
    bool triggerDisaster(const std::string& type, const std::string& triggeredBy, std::string& outMsg);
    bool stopActiveDisaster(const std::string& triggeredBy, std::string& outMsg);
    bool banUser(const std::string& target, const std::string& durationStr, std::string& outMsg, const std::string& reason = "Violation of Community Guidelines", const std::string& adminUser = "admin");
    bool unbanUser(const std::string& target, std::string& outMsg);
    bool muteUser(const std::string& target, int minutes, std::string& outMsg, const std::string& reason = "Violation of Community Guidelines", const std::string& adminUser = "admin");
    bool unmuteUser(const std::string& target, std::string& outMsg);

    // Scheduled Disasters
    int scheduleDisaster(const std::string& type, long long scheduledAtEpoch, int durationSeconds, const std::string& createdBy);
    DisasterEvent fetchPendingDisaster();
    void markDisasterActive(int eventID);
    void markDisasterCompleted(int eventID);
    void cancelDisasterEvent(int eventID);
    std::vector<DisasterEvent> getDisasterHistory(int limit = 20);

    // Death / Permadeath
    void clearPlayerData(const std::string& username); // wipes inventory, structures, campfires, chests, resets stats

private:
    SQLiteManager() = default;
    ~SQLiteManager() { close(); }
    SQLiteManager(const SQLiteManager&) = delete;
    SQLiteManager& operator=(const SQLiteManager&) = delete;

    void createTables();

    sqlite3* db = nullptr;
    mutable std::recursive_mutex dbMutex;
};
