#include "AuthManager.h"
#include <iostream>
#include <cppconn/exception.h>
#include <random>
#include <sstream>
#include <memory>

#include <vector>
#include <tuple>

AuthManager& AuthManager::getInstance() {
    std::cout << "[DEBUG] AuthManager::getInstance() called." << std::endl;
    static AuthManager instance;
    std::cout << "[DEBUG] static instance returned." << std::endl;
    return instance;
}

void AuthManager::shutdownDB() {
    if (con) {
        delete con;
        con = nullptr;
    }
}

bool AuthManager::initDB() {
    std::cout << "[DEBUG] initDB() executing..." << std::endl;
    try {
        std::cout << "[DEBUG] Calling get_mysql_driver_instance()..." << std::endl;
        driver = sql::mysql::get_mysql_driver_instance();
        std::cout << "[DEBUG] Driver successfully acquired!" << std::endl;
        sql::ConnectOptionsMap connection_properties;
        connection_properties["hostName"] = "mysql-308a256a-econovadb.h.aivencloud.com";
        connection_properties["port"] = 22311;
        connection_properties["userName"] = "avnadmin";
        connection_properties["password"] = std::string("AVNS_YV3eowX") + "Wv3Q2tArX8fd";
        connection_properties["sslCa"] = "ca.pem";
        connection_properties["OPT_SSL_MODE"] = sql::SSL_MODE_REQUIRED;
        
        std::cout << "[DEBUG] Connecting to Aiven Database..." << std::endl;
        con = driver->connect(connection_properties);
        std::cout << "[DEBUG] connect() finished." << std::endl;
        if (!con) {
            std::cout << "[DEBUG] ERROR: Connection returned null." << std::endl;
            return false;
        }

        std::cout << "[DEBUG] Setting schema to mmorpg..." << std::endl;
        con->setSchema("mmorpg");
        std::cout << "[DEBUG] Schema set successfully." << std::endl;

        // Ensure Accounts table has necessary columns (ignoring duplicate column errors)
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN email VARCHAR(255)"); } catch (...) {}
            }
        } catch (...) {}
        
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN reset_token VARCHAR(10)"); } catch (...) {}
            }
        } catch (...) {}

        // We assume Accounts and Inventory tables are already created by the user
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN hunger FLOAT DEFAULT 0"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN thirst FLOAT DEFAULT 0"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN Level INT DEFAULT 1"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN Exp INT DEFAULT 0"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN Gold INT DEFAULT 0"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN isFirstTime BOOLEAN DEFAULT 1"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN tutorialVersion INT DEFAULT 0"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN questID INT DEFAULT 0"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN questProgress INT DEFAULT 0"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN lastQuestReset DATE DEFAULT '2000-01-01'"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN questCompleted BOOLEAN DEFAULT 0"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN posX FLOAT DEFAULT 25000"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN posY FLOAT DEFAULT 25000"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN lastActive TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"); }
                catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN pullBy VARCHAR(50) DEFAULT NULL"); }
                catch (...) {}

                // Force tutorial reset for everyone for this update (v2)
                stmt->execute("UPDATE Accounts SET isFirstTime=1, tutorialVersion=2 WHERE tutorialVersion < 2");
            }
        }
        catch (...) {}
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN elapsedTime FLOAT DEFAULT 0"); } catch (...) {}
                try { stmt->execute("ALTER TABLE Accounts ADD COLUMN gender INT DEFAULT -1"); } catch (...) {}
                
                std::cout << "[DEBUG] Creating structures and campfires tables...\n";
                stmt->execute("CREATE TABLE IF NOT EXISTS structures ("
                              "ID INT AUTO_INCREMENT PRIMARY KEY, "
                              "Username VARCHAR(50), "
                              "ItemID INT, "
                              "PosX FLOAT, "
                              "PosY FLOAT, "
                              "Health FLOAT, "
                              "Rotation FLOAT DEFAULT 0, "
                              "IsOpen TINYINT(1) DEFAULT 0"
                              ")");
                try { stmt->execute("ALTER TABLE structures ADD COLUMN IF NOT EXISTS IsOpen TINYINT(1) DEFAULT 0"); } catch(...) {}
                stmt->execute("CREATE TABLE IF NOT EXISTS campfires ("
                              "ID INT AUTO_INCREMENT PRIMARY KEY, "
                              "Username VARCHAR(50), "
                              "PosX FLOAT, "
                              "PosY FLOAT, "
                              "TimeRemaining FLOAT, "
                              "IsCrafted BOOLEAN"
                              ")");
            }
        } catch (sql::SQLException& e) {
            std::cout << "[DEBUG] Table creation error: " << e.what() << "\n";
        }

        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                stmt->execute("CREATE TABLE IF NOT EXISTS ChatMessages ("
                              "MessageID INT AUTO_INCREMENT PRIMARY KEY, "
                              "Username VARCHAR(50), "
                              "Message TEXT, "
                              "SentAt TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                              ")");
            }
        } catch (...) {}

        return true;
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during init: " << e.what() << std::endl;
        return false;
    }
}

bool AuthManager::registerUser(const sql::SQLString& username, const sql::SQLString& password, const sql::SQLString& email) {
    if (!con) return false;
    try {
        // Check if username exists
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM Accounts WHERE Username = ?"));
        pstmtCheck->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (res && res->next()) {
            return false; // User already exists
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("INSERT INTO Accounts (Username, Password, email) VALUES (?, ?, ?)"));
        pstmt->setString(1, username);
        pstmt->setString(2, password);
        pstmt->setString(3, email);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during register: " << e.what() << " (Code: " << e.getErrorCode() << ")" << std::endl;
        return false;
    }
}

bool AuthManager::loginUser(const sql::SQLString& username, const sql::SQLString& password) {
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT * FROM Accounts WHERE Username = ? AND Password = ?"));
        pstmt->setString(1, username);
        pstmt->setString(2, password);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        return res && res->next();
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during login: " << e.what() << std::endl;
        return false;
    }
}

sql::SQLString AuthManager::generateForgotOTP(const sql::SQLString& email) {
    if (!con) return "";
    try {
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM Accounts WHERE email = ?"));
        pstmtCheck->setString(1, email);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (!res || !res->next()) {
            return ""; // email not found
        }

        // Generate 6 digit OTP
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        std::string otpStdStr = std::to_string(dis(gen));
        sql::SQLString otp(otpStdStr.c_str());

        std::unique_ptr<sql::PreparedStatement> pstmtUpdate(con->prepareStatement("UPDATE Accounts SET reset_token = ? WHERE email = ?"));
        pstmtUpdate->setString(1, otp);
        pstmtUpdate->setString(2, email);
        pstmtUpdate->executeUpdate();

        std::cout << "==== SIMULATED EMAIL DELIVERY ====\n";
        std::cout << "To: " << email.c_str() << "\n";
        std::cout << "Your password reset OTP is: " << otp.c_str() << "\n";
        std::cout << "==================================\n";

        return otp;
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during OTP gen: " << e.what() << std::endl;
        return "";
    }
}

bool AuthManager::resetPassword(const sql::SQLString& email, const sql::SQLString& otp, const sql::SQLString& newPassword) {
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM Accounts WHERE email = ? AND reset_token = ?"));
        pstmtCheck->setString(1, email);
        pstmtCheck->setString(2, otp);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (!res || !res->next()) {
            return false; // Invalid OTP or email
        }

        std::unique_ptr<sql::PreparedStatement> pstmtUpdate(con->prepareStatement("UPDATE Accounts SET Password = ?, reset_token = '' WHERE email = ?"));
        pstmtUpdate->setString(1, newPassword);
        pstmtUpdate->setString(2, email);
        pstmtUpdate->executeUpdate();

        return true;
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during password reset: " << e.what() << std::endl;
        return false;
    }
}

bool AuthManager::saveInventory(const sql::SQLString& username, const std::vector<std::pair<int, int>>& inventoryData) {
    if (!con) return false;
    try {
        // Clear old inventory for this user
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM Inventory WHERE Username = ?"));
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        // Insert new slots
        std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement("INSERT INTO Inventory (Username, ItemName, Quantity) VALUES (?, ?, ?)"));
        for (int slot = 0; slot < (int)inventoryData.size(); ++slot) {
            int item_id = inventoryData[slot].first;
            int count = inventoryData[slot].second;
            
            if (item_id != 0 && count > 0) {
                std::string itemName = "";
                if (item_id == 1) itemName = "ChickenMeat";
                else if (item_id == 2) itemName = "PigMeat";
                else if (item_id == 3) itemName = "ZombieFlesh";
                else if (item_id == 4) itemName = "WoodenSword";
                else if (item_id == 6) itemName = "Shield";
                else itemName = "Unknown";

                insStmt->setString(1, username);
                insStmt->setString(2, itemName);
                insStmt->setInt(3, count);
                insStmt->executeUpdate();
            }
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during saveInventory: " << e.what() << std::endl;
        return false;
    }
}

void AuthManager::savePlayerStats(const sql::SQLString& username, float hunger, float thirst, int currency, int level, int exp, float elapsedTime)
{
    if (!con) return;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("UPDATE Accounts SET hunger=?, thirst=?, Gold=?, Level=?, Exp=?, elapsedTime=? WHERE Username=?")
        );

        pstmt->setDouble(1, hunger);
        pstmt->setDouble(2, thirst);
        pstmt->setInt(3, currency);
        pstmt->setInt(4, level);
        pstmt->setInt(5, exp);
        pstmt->setDouble(6, elapsedTime);
        pstmt->setString(7, username);

        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cout << "Save stats error: " << e.what() << std::endl;
    }
}

PlayerStats AuthManager::loadPlayerStats(const sql::SQLString& username)
{
    if (!con) return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false };

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("SELECT hunger, thirst, Gold, Level, Exp, elapsedTime, gender, isFirstTime FROM Accounts WHERE Username=?")
        );

        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (res && res->next()) {
            PlayerStats stats;
            stats.hunger = (float)res->getDouble("hunger");
            stats.thirst = (float)res->getDouble("thirst");
            stats.currency = res->getInt("Gold");
            stats.level = res->getInt("Level");
            stats.exp = res->getInt("Exp");
            stats.elapsedTime = (float)res->getDouble("elapsedTime");
            stats.gender = res->getInt("gender");
            stats.isFirstTime = res->getBoolean("isFirstTime");
            return stats;
        }
    }
    catch (sql::SQLException& e) {
        std::cout << "Load stats error: " << e.what() << std::endl;
    }

    return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false };
}

void AuthManager::saveGender(const sql::SQLString& username, int gender) {
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET gender = ? WHERE Username = ?"));
        pstmt->setInt(1, gender);
        pstmt->setString(2, username);
        pstmt->executeUpdate();
    } catch (sql::SQLException& e) {
        std::cerr << "saveGender error: " << e.what() << "\n";
    }
}

void AuthManager::saveStructures(const sql::SQLString& username, const std::vector<StoredStructure>& structures) {
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM structures WHERE Username = ?"));
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement("INSERT INTO structures (Username, ItemID, PosX, PosY, Health, Rotation, IsOpen) VALUES (?, ?, ?, ?, ?, ?, ?)"));
        for (auto& s : structures) {
            insStmt->setString(1, username);
            insStmt->setInt(2, s.id);
            insStmt->setDouble(3, (double)s.x);
            insStmt->setDouble(4, (double)s.y);
            insStmt->setDouble(5, (double)s.health);
            insStmt->setDouble(6, (double)s.rotation);
            insStmt->setBoolean(7, s.isOpen);
            insStmt->executeUpdate();
        }
    } catch (sql::SQLException& e) {
        std::cerr << "saveStructures error: " << e.what() << "\n";
    }
}

std::vector<StoredStructure> AuthManager::loadStructures(const sql::SQLString& username) {
    std::vector<StoredStructure> structures;
    if (!con) return structures;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ItemID, PosX, PosY, Health, Rotation, IsOpen, ShearY FROM structures WHERE Username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            structures.push_back({ res->getInt("ItemID"), (float)res->getDouble("PosX"), (float)res->getDouble("PosY"), (float)res->getDouble("Health"), (float)res->getDouble("Rotation"), res->getBoolean("IsOpen"), (float)res->getDouble("ShearY") });
        }
    } catch (sql::SQLException& e) {
        std::cerr << "loadStructures error: " << e.what() << "\n";
    }
    return structures;
}

void AuthManager::saveCampfires(const sql::SQLString& username, const std::vector<StoredCampfire>& campfires) {
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM campfires WHERE Username = ?"));
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement("INSERT INTO campfires (Username, PosX, PosY, TimeRemaining, IsCrafted) VALUES (?, ?, ?, ?, ?)"));
        for (auto& c : campfires) {
            insStmt->setString(1, username);
            insStmt->setDouble(2, c.x);
            insStmt->setDouble(3, c.y);
            insStmt->setDouble(4, c.timeRemaining);
            insStmt->setBoolean(5, c.isCrafted);
            insStmt->executeUpdate();
        }
    } catch (sql::SQLException& e) {
        std::cerr << "saveCampfires error: " << e.what() << "\n";
    }
}

std::vector<StoredCampfire> AuthManager::loadCampfires(const sql::SQLString& username) {
    std::vector<StoredCampfire> campfires;
    if (!con) return campfires;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT PosX, PosY, TimeRemaining, IsCrafted FROM campfires WHERE Username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            campfires.push_back({ (float)res->getDouble("PosX"), (float)res->getDouble("PosY"), (float)res->getDouble("TimeRemaining"), res->getBoolean("IsCrafted") });
        }
    } catch (sql::SQLException& e) {
        std::cerr << "loadCampfires error: " << e.what() << "\n";
    }
    return campfires;
}

void AuthManager::setFirstTimeFinished(const sql::SQLString& username)
{
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("UPDATE Accounts SET isFirstTime=0 WHERE Username=?")
        );
        pstmt->setString(1, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cout << "Update first time error: " << e.what() << std::endl;
    }
}

void AuthManager::sendChatMessage(const sql::SQLString& username, const sql::SQLString& message)
{
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO ChatMessages (Username, Message) VALUES (?, ?)")
        );
        pstmt->setString(1, username);
        pstmt->setString(2, message);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cout << "Send chat message error: " << e.what() << std::endl;
    }
}

std::vector<std::pair<std::string, std::string>> AuthManager::fetchChatMessages(int limit)
{
    std::vector<std::pair<std::string, std::string>> messages;
    if (!con) return messages;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("SELECT Username, Message FROM ChatMessages ORDER BY SentAt DESC LIMIT ?")
        );
        pstmt->setInt(1, limit);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res && res->next()) {
            messages.push_back({ res->getString("Username").c_str(), res->getString("Message").c_str() });
        }
        // Reverse so that the oldest is at the top of the vector, newest at the bottom
        std::reverse(messages.begin(), messages.end());
    }
    catch (sql::SQLException& e) {
        std::cout << "Fetch chat messages error: " << e.what() << std::endl;
    }
    return messages;
}

std::vector<std::pair<int, int>> AuthManager::loadInventory(const sql::SQLString& username) {
    std::vector<std::pair<int, int>> inventoryData(8, {0, 0}); // Default 8 empty slots based on Game.cpp
    if (!con) return inventoryData;
    
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ItemName, Quantity FROM Inventory WHERE Username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        int slot = 0;
        while (res && res->next() && slot < 8) {
            std::string itemName = res->getString("ItemName").c_str();
            int count = res->getInt("Quantity");
            
            int item_id = 0;
            if (itemName == "ChickenMeat") item_id = 1;
            else if (itemName == "PigMeat") item_id = 2;
            else if (itemName == "ZombieFlesh") item_id = 3;
            else if (itemName == "WoodenSword") item_id = 4;
            else if (itemName == "Shield") item_id = 6;
            
            if (item_id != 0) {
                inventoryData[slot] = {item_id, count};
                slot++;
            }
        }
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during loadInventory: " << e.what() << std::endl;
    }
    return inventoryData;
}

// Daily Quest Support
QuestInfo AuthManager::getQuestInfo(const sql::SQLString& username) {
    QuestInfo q = { 0, 0, "2000-01-01", false };
    if (!con) return q;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT questID, questProgress, lastQuestReset, questCompleted FROM Accounts WHERE Username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            q.questID = res->getInt("questID");
            q.progress = res->getInt("questProgress");
            q.lastReset = res->getString("lastQuestReset").c_str();
            q.isCompleted = res->getBoolean("questCompleted");
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in getQuestInfo: " << e.what() << "\n";
    }
    return q;
}

void AuthManager::updateQuestProgress(const sql::SQLString& username, int progress, bool completed) {
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET questProgress = ?, questCompleted = ? WHERE Username = ?"));
        pstmt->setInt(1, progress);
        pstmt->setBoolean(2, completed);
        pstmt->setString(3, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in updateQuestProgress: " << e.what() << "\n";
    }
}

void AuthManager::resetQuest(const sql::SQLString& username, int questID, const sql::SQLString& date) {
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET questID = ?, questProgress = 0, lastQuestReset = ?, questCompleted = 0 WHERE Username = ?"));
        pstmt->setInt(1, questID);
        pstmt->setString(2, date);
        pstmt->setString(3, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in resetQuest: " << e.what() << "\n";
    }
}

void AuthManager::updateOnlineStatus(const sql::SQLString& username, float x, float y) {
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET posX = ?, posY = ?, lastActive = CURRENT_TIMESTAMP WHERE Username = ?"));
        pstmt->setDouble(1, (double)x);
        pstmt->setDouble(2, (double)y);
        pstmt->setString(3, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in updateOnlineStatus: " << e.what() << "\n";
    }
}

std::vector<OnlinePlayer> AuthManager::getOnlinePlayers(const sql::SQLString& excludeUser) {
    std::vector<OnlinePlayer> players;
    if (!con) return players;
    try {
        // Only fetch players active in the last 10 seconds
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT Username, posX, posY, pullBy FROM Accounts WHERE Username != ? AND lastActive > (CURRENT_TIMESTAMP - INTERVAL 10 SECOND)"));
        pstmt->setString(1, excludeUser);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            OnlinePlayer p;
            p.username = res->getString("Username").c_str();
            p.x = (float)res->getDouble("posX");
            p.y = (float)res->getDouble("posY");
            p.pullBy = res->isNull("pullBy") ? "" : res->getString("pullBy").c_str();
            players.push_back(p);
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in getOnlinePlayers: " << e.what() << "\n";
    }
    return players;
}

OnlinePlayer AuthManager::getSelfOnlineInfo(const sql::SQLString& username) {
    OnlinePlayer p;
    p.username = username.c_str();
    if (!con) return p;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT posX, posY, pullBy FROM Accounts WHERE Username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            p.x = (float)res->getDouble("posX");
            p.y = (float)res->getDouble("posY");
            p.pullBy = res->isNull("pullBy") ? "" : res->getString("pullBy").c_str();
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in getSelfOnlineInfo: " << e.what() << "\n";
    }
    return p;
}

void AuthManager::sendCommand(const sql::SQLString& fromUser, const sql::SQLString& toUser, const sql::SQLString& cmd) {
    if (!con) return;
    try {
        if (cmd == "pull") {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET pullBy = ? WHERE Username = ?"));
            pstmt->setString(1, fromUser);
            pstmt->setString(2, toUser);
            pstmt->executeUpdate();
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in sendCommand: " << e.what() << "\n";
    }
}

void AuthManager::clearPull(const sql::SQLString& username) {
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET pullBy = NULL WHERE Username = ?"));
        pstmt->setString(1, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in clearPull: " << e.what() << "\n";
    }
}

