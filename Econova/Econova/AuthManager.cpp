#include "AuthManager.h"
#include <iostream>
#include <cppconn/exception.h>
#include <random>
#include <sstream>
#include <memory>
#include <fstream>

#include <vector>
#include <tuple>

AuthManager& AuthManager::getInstance() {
    // Static-local singleton: C++11 guarantees thread-safe one-time init,
    // but we must never call getInstance() from inside the AuthManager constructor
    // (e.g. from the initDB background thread before main has touched the singleton).
    // Placing the first call on the main thread before any worker threads are spawned
    // ensures the static is already fully constructed when threads reach it.
    static AuthManager instance;
    return instance;
}

void AuthManager::shutdownDB() {
    if (con) {
        delete con;
        con = nullptr;
    }
}

#include "ViewUtils.h"

bool AuthManager::initDB() {
    try {
        driver = sql::mysql::get_mysql_driver_instance();
        std::string hostStr = "tcp://mysql-308a256a-econovadb.h.aivencloud.com:22311";
        std::string userStr = "avnadmin";
        std::string passStr = std::string("AVNS_YV3eowX") + "Wv3Q2tArX8fd";
        std::string caStr = resolvePath("ca.pem");

        sql::ConnectOptionsMap connection_properties;
        connection_properties["hostName"] = hostStr;
        connection_properties["userName"] = userStr;
        connection_properties["password"] = passStr;
        connection_properties["sslCa"] = caStr;
        connection_properties["OPT_SSL_MODE"] = sql::SSL_MODE_REQUIRED;
        // IMPORTANT: timeouts must be int, not string – passing a string is silently
        // ignored by connector/C++ and leaves connect() blocking indefinitely.
        connection_properties["OPT_CONNECT_TIMEOUT"] = (int)10;
        connection_properties["OPT_READ_TIMEOUT"] = (int)10;
        connection_properties["OPT_WRITE_TIMEOUT"] = (int)10;

        con = driver->connect(connection_properties);
        if (!con) return false;
        con->setSchema("mmorpg");

        // ---- Schema migration guard ----
        // We write a local file "schema_version.txt" after migrations complete.
        // If it already says the current version, skip all ALTER TABLE calls entirely
        // so startup doesn't waste 20+ network round-trips every launch.
        const int CURRENT_SCHEMA_VERSION = 3;
        const std::string SCHEMA_FILE = "schema_version.txt";
        int localSchemaVersion = 0;
        {
            std::ifstream sf(SCHEMA_FILE);
            if (sf.is_open()) sf >> localSchemaVersion;
        }
        bool needsMigration = (localSchemaVersion < CURRENT_SCHEMA_VERSION);

        if (needsMigration) {
            std::cout << "[DEBUG] Running schema migration (v" << localSchemaVersion << " -> v" << CURRENT_SCHEMA_VERSION << ")...\n";
            try {
                std::unique_ptr<sql::Statement> stmt(con->createStatement());
                if (stmt) {
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN email VARCHAR(255)"); }
                    catch (...) {}
                }
            }
            catch (...) {}

            try {
                std::unique_ptr<sql::Statement> stmt(con->createStatement());
                if (stmt) {
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN reset_token VARCHAR(10)"); }
                    catch (...) {}
                }
            }
            catch (...) {}

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
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN roles VARCHAR(50) DEFAULT 'user'"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN banned_until DATETIME DEFAULT NULL"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN muted_until DATETIME DEFAULT NULL"); }
                    catch (...) {}

                    // Force tutorial reset for everyone for this update (v2)
                    stmt->execute("UPDATE Accounts SET isFirstTime=1, tutorialVersion=2 WHERE tutorialVersion < 2");
                }
            }
            catch (...) {}
            try {
                std::unique_ptr<sql::Statement> stmt(con->createStatement());
                if (stmt) {
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN elapsedTime FLOAT DEFAULT 0"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN gender INT DEFAULT -1"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN hungerMultiplier INT DEFAULT 1"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE Accounts ADD COLUMN thirstMultiplier INT DEFAULT 1"); }
                    catch (...) {}

                    std::cout << "[DEBUG] Creating structures and campfires tables...\n";
                    stmt->execute("CREATE TABLE IF NOT EXISTS structures ("
                        "ID INT AUTO_INCREMENT PRIMARY KEY, "
                        "Username VARCHAR(50), "
                        "ItemID INT, "
                        "PosX FLOAT, "
                        "PosY FLOAT, "
                        "Health FLOAT, "
                        "Rotation FLOAT DEFAULT 0, "
                        "IsOpen TINYINT(1) DEFAULT 0, "
                        "ShearY FLOAT DEFAULT 0"
                        ")");
                    try { stmt->execute("ALTER TABLE structures ADD COLUMN IF NOT EXISTS IsOpen TINYINT(1) DEFAULT 0"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE structures ADD COLUMN IF NOT EXISTS ShearY FLOAT DEFAULT 0"); }
                    catch (...) {}
                    stmt->execute("CREATE TABLE IF NOT EXISTS campfires ("
                        "ID INT AUTO_INCREMENT PRIMARY KEY, "
                        "Username VARCHAR(50), "
                        "PosX FLOAT, "
                        "PosY FLOAT, "
                        "TimeRemaining FLOAT, "
                        "IsCrafted BOOLEAN"
                        ")");
                }
            }
            catch (sql::SQLException& e) {
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
            }
            catch (...) {}

            try {
                std::unique_ptr<sql::Statement> stmt(con->createStatement());
                if (stmt) {
                    stmt->execute("CREATE TABLE IF NOT EXISTS TradeHistory ("
                        "ID INT AUTO_INCREMENT PRIMARY KEY, "
                        "Username VARCHAR(50), "
                        "ActionType VARCHAR(10), "
                        "ItemName VARCHAR(50), "
                        "Quantity INT, "
                        "PricePerUnit INT, "
                        "TotalGold INT, "
                        "TradeTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                        ")");

                    // Prepopulate with sample trades if empty
                    std::unique_ptr<sql::ResultSet> countRes(stmt->executeQuery("SELECT COUNT(*) AS total FROM TradeHistory"));
                    if (countRes && countRes->next() && countRes->getInt("total") == 0) {
                        stmt->execute("INSERT INTO TradeHistory (Username, ActionType, ItemName, Quantity, PricePerUnit, TotalGold) VALUES "
                            "('TraderJoe', 'SELL', 'Wood', 15, 5, 75), "
                            "('MerchantAl', 'BUY', 'Stone', 8, 8, 64), "
                            "('AlphaBuilder', 'SELL', 'Sapling', 4, 12, 48), "
                            "('HunterMax', 'BUY', 'Chicken Meat', 6, 15, 90), "
                            "('ChefGourmet', 'SELL', 'Pig Meat', 3, 25, 75)");
                    }
                }
            }
            catch (...) {}

            // ---- End of schema migration ----
            // Write local schema version file so we skip migrations next launch
            {
                std::ofstream sf(SCHEMA_FILE);
                if (sf.is_open()) sf << CURRENT_SCHEMA_VERSION;
            }
            std::cout << "[DEBUG] Schema migration complete.\n";
        } // end if (needsMigration)

        return true;
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during init: " << e.what() << std::endl;
        return false;

    }
}

bool AuthManager::registerUser(const sql::SQLString& username, const sql::SQLString& password, const sql::SQLString& email) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return false;
    try {
        // Check if username exists
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM Accounts WHERE Username = ?"));
        if (!pstmtCheck) return false;
        pstmtCheck->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (res && res->next()) {
            return false; // User already exists
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("INSERT INTO Accounts (Username, Password, email) VALUES (?, ?, ?)"));
        if (!pstmt) return false;
        pstmt->setString(1, username);
        pstmt->setString(2, password);
        pstmt->setString(3, email);
        pstmt->executeUpdate();
        return true;
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during register: " << e.what() << " (Code: " << e.getErrorCode() << ")" << std::endl;
        return false;
    }
}

bool AuthManager::loginUser(const sql::SQLString& username, const sql::SQLString& password, std::string& errMessage) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) {
        errMessage = "Database connection error.";
        return false;
    }
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT roles, (banned_until > CURRENT_TIMESTAMP) AS is_banned, "
            "DATE_FORMAT(banned_until, '%Y-%m-%d %H:%i:%S') as ban_date "
            "FROM Accounts WHERE Username = ? AND Password = ?"
        ));
        if (!pstmt) {
            errMessage = "Database query preparation failed.";
            return false;
        }
        pstmt->setString(1, username);
        pstmt->setString(2, password);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            bool isBanned = res->getBoolean("is_banned");
            if (isBanned) {
                std::string banDate = std::string(res->getString("ban_date"));
                if (banDate.find("9999") == 0) {
                    errMessage = "You are permanently banned!";
                }
                else {
                    errMessage = "You are banned until: " + banDate;
                }
                return false;
            }
            return true;
        }
        errMessage = "Invalid Credentials!";
        return false;
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during login: " << e.what() << std::endl;
        errMessage = "Database query error: " + std::string(e.what());
        return false;
    }
}

sql::SQLString AuthManager::generateForgotOTP(const sql::SQLString& email) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return "";
    try {
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM Accounts WHERE email = ?"));
        if (!pstmtCheck) return "";
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
        if (!pstmtUpdate) return "";
        pstmtUpdate->setString(1, otp);
        pstmtUpdate->setString(2, email);
        pstmtUpdate->executeUpdate();

        std::cout << "==== SIMULATED EMAIL DELIVERY ====\n";
        std::cout << "To: " << email.c_str() << "\n";
        std::cout << "Your password reset OTP is: " << otp.c_str() << "\n";
        std::cout << "==================================\n";

        return otp;
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during OTP gen: " << e.what() << std::endl;
        return "";
    }
}

bool AuthManager::resetPassword(const sql::SQLString& email, const sql::SQLString& otp, const sql::SQLString& newPassword) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM Accounts WHERE email = ? AND reset_token = ?"));
        if (!pstmtCheck) return false;
        pstmtCheck->setString(1, email);
        pstmtCheck->setString(2, otp);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (!res || !res->next()) {
            return false; // Invalid OTP or email
        }

        std::unique_ptr<sql::PreparedStatement> pstmtUpdate(con->prepareStatement("UPDATE Accounts SET Password = ?, reset_token = '' WHERE email = ?"));
        if (!pstmtUpdate) return false;
        pstmtUpdate->setString(1, newPassword);
        pstmtUpdate->setString(2, email);
        pstmtUpdate->executeUpdate();

        return true;
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during password reset: " << e.what() << std::endl;
        return false;
    }
}

bool AuthManager::saveInventory(const sql::SQLString& username, const std::vector<std::pair<int, int>>& inventoryData) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return false;
    try {
        // Clear old inventory for this user
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM Inventory WHERE Username = ?"));
        if (!delStmt) return false;
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        // Insert new slots
        std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement("INSERT INTO Inventory (Username, ItemName, Quantity) VALUES (?, ?, ?)"));
        if (!insStmt) return false;
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
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during saveInventory: " << e.what() << std::endl;
        return false;
    }
}

void AuthManager::savePlayerStats(const sql::SQLString& username, float hunger, float thirst, int currency, int level, int exp, float elapsedTime, int hungerMultiplier, int thirstMultiplier)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("UPDATE Accounts SET hunger=?, thirst=?, Gold=?, Level=?, Exp=?, elapsedTime=?, hungerMultiplier=?, thirstMultiplier=? WHERE Username=?")
        );
        if (!pstmt) return;

        pstmt->setDouble(1, hunger);
        pstmt->setDouble(2, thirst);
        pstmt->setInt(3, currency);
        pstmt->setInt(4, level);
        pstmt->setInt(5, exp);
        pstmt->setDouble(6, elapsedTime);
        pstmt->setInt(7, hungerMultiplier);
        pstmt->setInt(8, thirstMultiplier);
        pstmt->setString(9, username);

        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cout << "Save stats error: " << e.what() << std::endl;
    }
}

PlayerStats AuthManager::loadPlayerStats(const sql::SQLString& username)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("SELECT hunger, thirst, Gold, Level, Exp, elapsedTime, gender, isFirstTime, hungerMultiplier, thirstMultiplier FROM Accounts WHERE Username=?")
        );
        if (!pstmt) return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };

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

            try {
                stats.hungerMultiplier = res->getInt("hungerMultiplier");
                if (stats.hungerMultiplier < 1) stats.hungerMultiplier = 1;
            }
            catch (...) {
                stats.hungerMultiplier = 1;
            }

            try {
                stats.thirstMultiplier = res->getInt("thirstMultiplier");
                if (stats.thirstMultiplier < 1) stats.thirstMultiplier = 1;
            }
            catch (...) {
                stats.thirstMultiplier = 1;
            }

            return stats;
        }
    }
    catch (sql::SQLException& e) {
        std::cout << "Load stats error: " << e.what() << std::endl;
    }

    return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };
}

void AuthManager::saveGender(const sql::SQLString& username, int gender) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET gender = ? WHERE Username = ?"));
        if (!pstmt) return;
        pstmt->setInt(1, gender);
        pstmt->setString(2, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "saveGender error: " << e.what() << "\n";
    }
}

void AuthManager::saveStructures(const sql::SQLString& username, const std::vector<StoredStructure>& structures) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM structures WHERE Username = ?"));
        if (!delStmt) return;
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement("INSERT INTO structures (Username, ItemID, PosX, PosY, Health, Rotation, IsOpen, ShearY) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        if (!insStmt) return;
        for (auto& s : structures) {
            insStmt->setString(1, username);
            insStmt->setInt(2, s.id);
            insStmt->setDouble(3, (double)s.x);
            insStmt->setDouble(4, (double)s.y);
            insStmt->setDouble(5, (double)s.health);
            insStmt->setDouble(6, (double)s.rotation);
            insStmt->setBoolean(7, s.isOpen);
            insStmt->setDouble(8, (double)s.shearY);
            insStmt->executeUpdate();
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "saveStructures error: " << e.what() << "\n";
    }
}

std::vector<StoredStructure> AuthManager::loadStructures(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredStructure> structures;
    if (!con) return structures;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ItemID, PosX, PosY, Health, Rotation, IsOpen, ShearY FROM structures WHERE Username = ?"));
        if (!pstmt) return structures;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            structures.push_back({ res->getInt("ItemID"), (float)res->getDouble("PosX"), (float)res->getDouble("PosY"), (float)res->getDouble("Health"), (float)res->getDouble("Rotation"), res->getBoolean("IsOpen"), (float)res->getDouble("ShearY") });
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "loadStructures error: " << e.what() << "\n";
    }
    return structures;
}

std::vector<StoredStructure> AuthManager::loadOtherStructures(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredStructure> structures;
    if (!con) return structures;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ItemID, PosX, PosY, Health, Rotation, IsOpen, ShearY FROM structures WHERE Username != ?"));
        if (!pstmt) return structures;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            structures.push_back({ res->getInt("ItemID"), (float)res->getDouble("PosX"), (float)res->getDouble("PosY"), (float)res->getDouble("Health"), (float)res->getDouble("Rotation"), res->getBoolean("IsOpen"), (float)res->getDouble("ShearY") });
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "loadOtherStructures error: " << e.what() << "\n";
    }
    return structures;
}

void AuthManager::saveCampfires(const sql::SQLString& username, const std::vector<StoredCampfire>& campfires) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM campfires WHERE Username = ?"));
        if (!delStmt) return;
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement("INSERT INTO campfires (Username, PosX, PosY, TimeRemaining, IsCrafted) VALUES (?, ?, ?, ?, ?)"));
        if (!insStmt) return;
        for (auto& c : campfires) {
            insStmt->setString(1, username);
            insStmt->setDouble(2, c.x);
            insStmt->setDouble(3, c.y);
            insStmt->setDouble(4, c.timeRemaining);
            insStmt->setBoolean(5, c.isCrafted);
            insStmt->executeUpdate();
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "saveCampfires error: " << e.what() << "\n";
    }
}

std::vector<StoredCampfire> AuthManager::loadCampfires(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredCampfire> campfires;
    if (!con) return campfires;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT PosX, PosY, TimeRemaining, IsCrafted FROM campfires WHERE Username = ?"));
        if (!pstmt) return campfires;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            campfires.push_back({ (float)res->getDouble("PosX"), (float)res->getDouble("PosY"), (float)res->getDouble("TimeRemaining"), res->getBoolean("IsCrafted") });
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "loadCampfires error: " << e.what() << "\n";
    }
    return campfires;
}

void AuthManager::setFirstTimeFinished(const sql::SQLString& username)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("UPDATE Accounts SET isFirstTime=0 WHERE Username=?")
        );
        if (!pstmt) return;
        pstmt->setString(1, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cout << "Update first time error: " << e.what() << std::endl;
    }
}

bool AuthManager::sendChatMessage(const sql::SQLString& username, const sql::SQLString& message, std::string& outError)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) {
        outError = "Database connection error.";
        return false;
    }
    try {
        // Mute check
        std::unique_ptr<sql::PreparedStatement> pstmtMute(
            con->prepareStatement("SELECT (muted_until > CURRENT_TIMESTAMP) AS is_muted, DATE_FORMAT(muted_until, '%Y-%m-%d %H:%i:%S') as mute_date FROM Accounts WHERE Username = ?")
        );
        if (pstmtMute) {
            pstmtMute->setString(1, username);
            std::unique_ptr<sql::ResultSet> resMute(pstmtMute->executeQuery());
            if (resMute && resMute->next()) {
                bool isMuted = resMute->getBoolean("is_muted");
                if (isMuted) {
                    outError = "You are muted until: " + std::string(resMute->getString("mute_date"));
                    return false;
                }
            }
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO ChatMessages (Username, Message) VALUES (?, ?)")
        );
        if (!pstmt) return false;
        pstmt->setString(1, username);
        pstmt->setString(2, message);
        pstmt->executeUpdate();
        return true;
    }
    catch (sql::SQLException& e) {
        std::cout << "Send chat message error: " << e.what() << std::endl;
        outError = "Database error: " + std::string(e.what());
        return false;
    }
}

std::vector<std::pair<std::string, std::string>> AuthManager::fetchChatMessages(int limit, int sinceMsgId)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<std::pair<std::string, std::string>> messages;
    if (!con) return messages;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("SELECT Username, Message FROM ChatMessages WHERE MessageID > ? ORDER BY SentAt DESC LIMIT ?")
        );
        if (!pstmt) return messages;
        pstmt->setInt(1, sinceMsgId);
        pstmt->setInt(2, limit);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        while (res && res->next()) {
            std::string user = res->isNull("Username") ? "" : std::string(res->getString("Username"));
            std::string msg = res->isNull("Message") ? "" : std::string(res->getString("Message"));
            messages.push_back({ user, msg });
        }
        // Reverse so that the oldest is at the top of the vector, newest at the bottom
        std::reverse(messages.begin(), messages.end());
    }
    catch (const std::exception& e) {
        std::cout << "Fetch chat messages error: " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "Fetch chat messages unknown error" << std::endl;
    }
    return messages;
}

int AuthManager::getMaxMessageID()
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return 0;
    try {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        if (!stmt) return 0;
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT MAX(MessageID) AS max_id FROM ChatMessages"));
        if (res && res->next()) {
            return res->getInt("max_id");
        }
    }
    catch (sql::SQLException& e) {
        std::cout << "Get max message ID error: " << e.what() << std::endl;
    }
    return 0;
}

std::vector<std::pair<int, int>> AuthManager::loadInventory(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<std::pair<int, int>> inventoryData(8, { 0, 0 }); // Default 8 empty slots based on Game.cpp
    if (!con) return inventoryData;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ItemName, Quantity FROM Inventory WHERE Username = ?"));
        if (!pstmt) return inventoryData;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        int slot = 0;
        while (res && res->next() && slot < 8) {
            std::string itemName = std::string(res->getString("ItemName"));
            int count = res->getInt("Quantity");

            int item_id = 0;
            if (itemName == "ChickenMeat") item_id = 1;
            else if (itemName == "PigMeat") item_id = 2;
            else if (itemName == "ZombieFlesh") item_id = 3;
            else if (itemName == "WoodenSword") item_id = 4;
            else if (itemName == "Shield") item_id = 6;

            if (item_id != 0) {
                inventoryData[slot] = { item_id, count };
                slot++;
            }
        }
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during loadInventory: " << e.what() << std::endl;
    }
    return inventoryData;
}

// Daily Quest Support
QuestInfo AuthManager::getQuestInfo(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    QuestInfo q = { 0, 0, "2000-01-01", false };
    if (!con) return q;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT questID, questProgress, lastQuestReset, questCompleted FROM Accounts WHERE Username = ?"));
        if (!pstmt) return q;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            q.questID = res->getInt("questID");
            q.progress = res->getInt("questProgress");
            q.lastReset = std::string(res->getString("lastQuestReset"));
            q.isCompleted = res->getBoolean("questCompleted");
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in getQuestInfo: " << e.what() << "\n";
    }
    return q;
}

void AuthManager::updateQuestProgress(const sql::SQLString& username, int progress, bool completed) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET questProgress = ?, questCompleted = ? WHERE Username = ?"));
        if (!pstmt) return;
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
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET questID = ?, questProgress = 0, lastQuestReset = ?, questCompleted = 0 WHERE Username = ?"));
        if (!pstmt) return;
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
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET posX = ?, posY = ?, lastActive = CURRENT_TIMESTAMP WHERE Username = ?"));
        if (!pstmt) return;
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
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<OnlinePlayer> players;
    if (!con) return players;
    try {
        // Only fetch players active in the last 10 seconds
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT Username, posX, posY, pullBy, gender FROM Accounts WHERE Username != ? AND lastActive > (CURRENT_TIMESTAMP - INTERVAL 10 SECOND)"));
        if (!pstmt) return players;
        pstmt->setString(1, excludeUser);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            OnlinePlayer p;
            p.username = std::string(res->getString("Username"));
            p.x = (float)res->getDouble("posX");
            p.y = (float)res->getDouble("posY");
            p.gender = res->getInt("gender");
            if (res->isNull("pullBy")) {
                p.pullBy = "";
            }
            else {
                std::string pb = std::string(res->getString("pullBy"));
                p.pullBy = pb;
            }
            players.push_back(p);
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in getOnlinePlayers: " << e.what() << "\n";
    }
    return players;
}

OnlinePlayer AuthManager::getSelfOnlineInfo(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    OnlinePlayer p;
    p.username = username.c_str();
    if (!con) return p;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT posX, posY, pullBy FROM Accounts WHERE Username = ?"));
        if (!pstmt) return p;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            p.x = (float)res->getDouble("posX");
            p.y = (float)res->getDouble("posY");
            if (res->isNull("pullBy")) {
                p.pullBy = "";
            }
            else {
                std::string pb = std::string(res->getString("pullBy"));
                p.pullBy = pb;
            }
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in getSelfOnlineInfo: " << e.what() << "\n";
    }
    return p;
}

void AuthManager::sendCommand(const sql::SQLString& fromUser, const sql::SQLString& toUser, const sql::SQLString& cmd) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        if (cmd == "pull") {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET pullBy = ? WHERE Username = ?"));
            if (!pstmt) return;
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
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET pullBy = NULL WHERE Username = ?"));
        if (!pstmt) return;
        pstmt->setString(1, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in clearPull: " << e.what() << "\n";
    }
}

void AuthManager::addTradeRecord(const sql::SQLString& username, const std::string& actionType, const std::string& itemName, int quantity, int pricePerUnit, int totalGold) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO TradeHistory (Username, ActionType, ItemName, Quantity, PricePerUnit, TotalGold) VALUES (?, ?, ?, ?, ?, ?)")
        );
        if (!pstmt) return;
        pstmt->setString(1, username);
        pstmt->setString(2, actionType);
        pstmt->setString(3, itemName);
        pstmt->setInt(4, quantity);
        pstmt->setInt(5, pricePerUnit);
        pstmt->setInt(6, totalGold);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "addTradeRecord error: " << e.what() << "\n";
    }
}


std::vector<TradeRecord> AuthManager::getRecentTrades(int limit) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<TradeRecord> records;
    if (!con) return records;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("SELECT Username, ActionType, ItemName, Quantity, PricePerUnit, TotalGold FROM TradeHistory ORDER BY TradeTime DESC LIMIT ?")
        );
        if (!pstmt) return records;
        pstmt->setInt(1, limit);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            TradeRecord r;
            r.username = res->isNull("Username") ? "" : std::string(res->getString("Username"));
            r.actionType = res->isNull("ActionType") ? "" : std::string(res->getString("ActionType"));
            r.itemName = res->isNull("ItemName") ? "" : std::string(res->getString("ItemName"));
            r.quantity = res->isNull("Quantity") ? 0 : res->getInt("Quantity");
            r.pricePerUnit = res->isNull("PricePerUnit") ? 0 : res->getInt("PricePerUnit");
            r.totalGold = res->isNull("TotalGold") ? 0 : res->getInt("TotalGold");
            r.tradeTime = "";
            records.push_back(r);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "getRecentTrades error: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "getRecentTrades unknown error\n";
    }
    return records;
}

std::string AuthManager::getUserRole(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return "user";
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT roles FROM Accounts WHERE Username = ?"));
        if (!pstmt) return "user";
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            std::string roleStr = std::string(res->getString("roles"));
            if (roleStr.empty()) return "user";
            return roleStr;
        }
    }
    catch (...) {}
    return "user";
}

bool AuthManager::setUserRole(const std::string& username, const std::string& roles) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET roles = ? WHERE Username = ?"));
        if (!pstmt) return false;
        pstmt->setString(1, roles);
        pstmt->setString(2, username);
        pstmt->executeUpdate();
        return true;
    }
    catch (...) {}
    return false;
}

bool AuthManager::banUser(const std::string& target, const std::string& durationStr, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT Username FROM Accounts WHERE Username = ?"));
        if (!pstmtCheck) return false;
        pstmtCheck->setString(1, target);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (!res || !res->next()) {
            outMsg = "Error: User '" + target + "' does not exist.";
            return false;
        }

        int days = 0;
        bool isPerm = false;
        if (durationStr == "3") days = 3;
        else if (durationStr == "7") days = 7;
        else if (durationStr == "14") days = 14;
        else if (durationStr == "30") days = 30;
        else if (durationStr == "perm" || durationStr == "permanent") isPerm = true;
        else {
            outMsg = "Error: Invalid duration. Use 3, 7, 14, 30, or perm.";
            return false;
        }

        if (isPerm) {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET banned_until = '9999-12-31 23:59:59' WHERE Username = ?"));
            if (!pstmt) return false;
            pstmt->setString(1, target);
            pstmt->executeUpdate();
            outMsg = "Successfully permanently banned " + target + ".";
        }
        else {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET banned_until = CURRENT_TIMESTAMP + INTERVAL ? DAY WHERE Username = ?"));
            if (!pstmt) return false;
            pstmt->setInt(1, days);
            pstmt->setString(2, target);
            pstmt->executeUpdate();
            outMsg = "Successfully banned " + target + " for " + durationStr + " days.";
        }
        return true;
    }
    catch (sql::SQLException& e) {
        outMsg = "Database error: " + std::string(e.what());
        return false;
    }
}

bool AuthManager::unbanUser(const std::string& target, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT Username FROM Accounts WHERE Username = ?"));
        if (!pstmtCheck) return false;
        pstmtCheck->setString(1, target);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (!res || !res->next()) {
            outMsg = "Error: User '" + target + "' does not exist.";
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET banned_until = NULL WHERE Username = ?"));
        if (!pstmt) return false;
        pstmt->setString(1, target);
        pstmt->executeUpdate();
        outMsg = "Successfully unbanned " + target + ".";
        return true;
    }
    catch (sql::SQLException& e) {
        outMsg = "Database error: " + std::string(e.what());
        return false;
    }
}

bool AuthManager::muteUser(const std::string& target, int minutes, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT Username FROM Accounts WHERE Username = ?"));
        if (!pstmtCheck) return false;
        pstmtCheck->setString(1, target);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (!res || !res->next()) {
            outMsg = "Error: User '" + target + "' does not exist.";
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET muted_until = CURRENT_TIMESTAMP + INTERVAL ? MINUTE WHERE Username = ?"));
        if (!pstmt) return false;
        pstmt->setInt(1, minutes);
        pstmt->setString(2, target);
        pstmt->executeUpdate();
        outMsg = "Successfully muted " + target + " for " + std::to_string(minutes) + " minutes.";
        return true;
    }
    catch (sql::SQLException& e) {
        outMsg = "Database error: " + std::string(e.what());
        return false;
    }
}

bool AuthManager::unmuteUser(const std::string& target, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT Username FROM Accounts WHERE Username = ?"));
        if (!pstmtCheck) return false;
        pstmtCheck->setString(1, target);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (!res || !res->next()) {
            outMsg = "Error: User '" + target + "' does not exist.";
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET muted_until = NULL WHERE Username = ?"));
        if (!pstmt) return false;
        pstmt->setString(1, target);
        pstmt->executeUpdate();
        outMsg = "Successfully unmuted " + target + ".";
        return true;
    }
    catch (sql::SQLException& e) {
        outMsg = "Database error: " + std::string(e.what());
        return false;
    }
}

float AuthManager::loadMarketPrice(int itemID)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);

    if (!con)
        return 0.f;

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "SELECT Price FROM MarketPrices WHERE ItemID=?"
            )
        );

        if (!pstmt)
            return 0.f;

        pstmt->setInt(1, itemID);

        std::unique_ptr<sql::ResultSet> res(
            pstmt->executeQuery()
        );

        if (res && res->next())
        {
            return (float)res->getDouble("Price");
        }
    }
    catch (sql::SQLException& e)
    {
        std::cout << "loadMarketPrice error: "
            << e.what() << std::endl;
    }

    return 0.f;
}

void AuthManager::saveMarketPrice(int itemID, float price)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);

    if (!con)
        return;

    try
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "REPLACE INTO MarketPrices (ItemID, Price) VALUES (?, ?)"
            )
        );

        if (!pstmt)
            return;

        pstmt->setInt(1, itemID);
        pstmt->setDouble(2, price);

        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e)
    {
        std::cout << "saveMarketPrice error: "
            << e.what() << std::endl;
    }
}

// =========================================================
// P2P TRADING METHODS
// =========================================================

int AuthManager::createP2PTrade(const std::string& from, const std::string& to) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return -1;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "INSERT INTO PlayerTrades (InitiatorUser, TargetUser, Status) VALUES (?, ?, 'pending')"
        ));
        pstmt->setString(1, from);
        pstmt->setString(2, to);
        pstmt->executeUpdate();
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
        if (res && res->next()) return res->getInt("id");
    }
    catch (sql::SQLException& e) { std::cerr << "createP2PTrade error: " << e.what() << "\n"; }
    return -1;
}

void AuthManager::updateP2POffer(int tradeID, const std::string& user, const std::string& itemsCSV, int gold) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> chk(con->prepareStatement(
            "SELECT InitiatorUser FROM PlayerTrades WHERE TradeID = ?"
        ));
        chk->setInt(1, tradeID);
        std::unique_ptr<sql::ResultSet> res(chk->executeQuery());
        if (!res || !res->next()) return;
        std::string initiator = std::string(res->getString("InitiatorUser"));
        bool isInitiator = (initiator == user);
        std::string sql = isInitiator
            ? "UPDATE PlayerTrades SET InitiatorItems=?, InitiatorGold=?, InitiatorReady=0 WHERE TradeID=?"
            : "UPDATE PlayerTrades SET TargetItems=?, TargetGold=?, TargetReady=0 WHERE TradeID=?";
        std::unique_ptr<sql::PreparedStatement> upd(con->prepareStatement(sql));
        upd->setString(1, itemsCSV);
        upd->setInt(2, gold);
        upd->setInt(3, tradeID);
        upd->executeUpdate();
    }
    catch (sql::SQLException& e) { std::cerr << "updateP2POffer error: " << e.what() << "\n"; }
}

void AuthManager::setP2PReady(int tradeID, const std::string& user, bool ready) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> chk(con->prepareStatement(
            "SELECT InitiatorUser FROM PlayerTrades WHERE TradeID = ?"
        ));
        chk->setInt(1, tradeID);
        std::unique_ptr<sql::ResultSet> res(chk->executeQuery());
        if (!res || !res->next()) return;
        std::string initiator = std::string(res->getString("InitiatorUser"));
        bool isInitiator = (initiator == user);
        std::string sql = isInitiator
            ? "UPDATE PlayerTrades SET InitiatorReady=? WHERE TradeID=?"
            : "UPDATE PlayerTrades SET TargetReady=? WHERE TradeID=?";
        std::unique_ptr<sql::PreparedStatement> upd(con->prepareStatement(sql));
        upd->setInt(1, ready ? 1 : 0);
        upd->setInt(2, tradeID);
        upd->executeUpdate();
    }
    catch (sql::SQLException& e) { std::cerr << "setP2PReady error: " << e.what() << "\n"; }
}

P2PTradeState AuthManager::getP2PTradeState(int tradeID) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    P2PTradeState s;
    s.tradeID = tradeID;
    if (!con) return s;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT InitiatorUser, TargetUser, Status, InitiatorItems, TargetItems, "
            "InitiatorGold, TargetGold, InitiatorReady, TargetReady "
            "FROM PlayerTrades WHERE TradeID = ?"
        ));
        pstmt->setInt(1, tradeID);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            s.initiator      = std::string(res->getString("InitiatorUser"));
            s.target         = std::string(res->getString("TargetUser"));
            s.status         = std::string(res->getString("Status"));
            s.initiatorItems = res->isNull("InitiatorItems") ? "" : std::string(res->getString("InitiatorItems"));
            s.targetItems    = res->isNull("TargetItems")    ? "" : std::string(res->getString("TargetItems"));
            s.initiatorGold  = res->getInt("InitiatorGold");
            s.targetGold     = res->getInt("TargetGold");
            s.initiatorReady = res->getBoolean("InitiatorReady");
            s.targetReady    = res->getBoolean("TargetReady");
        }
    }
    catch (sql::SQLException& e) { std::cerr << "getP2PTradeState error: " << e.what() << "\n"; }
    return s;
}

void AuthManager::cancelP2PTrade(int tradeID) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "UPDATE PlayerTrades SET Status='cancelled' WHERE TradeID=?"
        ));
        pstmt->setInt(1, tradeID);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) { std::cerr << "cancelP2PTrade error: " << e.what() << "\n"; }
}

int AuthManager::getPendingP2PTradeFor(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return -1;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT TradeID FROM PlayerTrades "
            "WHERE TargetUser=? AND Status='pending' "
            "ORDER BY CreatedAt DESC LIMIT 1"
        ));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) return res->getInt("TradeID");
    }
    catch (sql::SQLException& e) { std::cerr << "getPendingP2PTradeFor error: " << e.what() << "\n"; }
    return -1;
}

void AuthManager::acceptP2PTrade(int tradeID) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "UPDATE PlayerTrades SET Status='accepted' WHERE TradeID=?"
        ));
        pstmt->setInt(1, tradeID);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) { std::cerr << "acceptP2PTrade error: " << e.what() << "\n"; }
}

void AuthManager::completeP2PTrade(int tradeID) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "UPDATE PlayerTrades SET Status='completed' WHERE TradeID=? AND InitiatorReady=1 AND TargetReady=1"
        ));
        pstmt->setInt(1, tradeID);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) { std::cerr << "completeP2PTrade error: " << e.what() << "\n"; }
}
