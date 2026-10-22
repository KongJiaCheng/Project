#include "AuthManager.h"
#include "SQLiteManager.h"
#include <iostream>
#include <cppconn/exception.h>
#include <random>
#include <sstream>
#include <memory>
#include <fstream>
#include <thread>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <tuple>
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

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
    if (SQLiteManager::getInstance().isOpen()) {
        SQLiteManager::getInstance().close();
    }
    if (con) {
        try {
            cleanupInactiveChatMessages();
        } catch (...) {}
        delete con;
        con = nullptr;
    }
}

#include "ViewUtils.h"

struct DBConfigData {
    std::string mode = "cloud";
    // Local (Offline mode)
    std::string local_host = "tcp://127.0.0.1:3306";
    std::string local_user = "root";
    std::string local_pass = "";
    std::string local_schema = "mmorpg";
    bool        local_ssl = false;
    // Cloud (Online mode)
    std::string cloud_host = "tcp://mysql-308a256a-econovadb.h.aivencloud.com:22311";
    std::string cloud_user = "avnadmin";
    std::string cloud_pass = std::string("AVNS_YV3eowX") + "Wv3Q2tArX8fd";
    std::string cloud_schema = "mmorpg";
    bool        cloud_ssl = true;
    std::string cloud_ca = "ca.pem";
};

static DBConfigData loadDBConfig() {
    DBConfigData cfg;
    std::ifstream f("db_config.txt");
    if (!f.is_open()) {
        f.open(resolvePath("db_config.txt"));
    }
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t' || key.back() == '\r')) key.pop_back();
            while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r')) val.pop_back();
            while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());

            if (key == "mode") cfg.mode = val;
            else if (key == "local_host") cfg.local_host = val;
            else if (key == "local_user") cfg.local_user = val;
            else if (key == "local_password") cfg.local_pass = val;
            else if (key == "local_schema") cfg.local_schema = val;
            else if (key == "local_ssl") cfg.local_ssl = (val == "true" || val == "1" || val == "yes");
            else if (key == "cloud_host") cfg.cloud_host = val;
            else if (key == "cloud_user") cfg.cloud_user = val;
            else if (key == "cloud_password") cfg.cloud_pass = val;
            else if (key == "cloud_schema") cfg.cloud_schema = val;
            else if (key == "cloud_ssl") cfg.cloud_ssl = (val == "true" || val == "1" || val == "yes");
            else if (key == "cloud_ca") cfg.cloud_ca = val;
        }
    }
    return cfg;
}

bool AuthManager::isConnected() const {
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().isOpen();
    }
    return con != nullptr;
}

bool AuthManager::isOfflineMode() const {
    return isLocalModeActive.load();
}

void AuthManager::setOfflineMode(bool offline) {
    isLocalModeActive = offline;
}

std::string AuthManager::getDBHost() const {
    return currentConnectedHost;
}

bool AuthManager::initDB(bool forceLocal) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    shutdownDB();

    DBConfigData cfg = loadDBConfig();
    bool useLocal = forceLocal || (cfg.mode == "local" || cfg.mode == "offline");
    isLocalModeActive = useLocal;

    if (useLocal) {
        bool ok = SQLiteManager::getInstance().init("econova_offline.db");
        if (ok) {
            currentConnectedHost = "econova_offline.db (SQLite)";
            std::cout << "[DB] Connected to LOCAL SQLite database: econova_offline.db\n";
            return true;
        } else {
            std::cerr << "[DB ERROR] Failed to initialize SQLite database (econova_offline.db)\n";
            return false;
        }
    }

    try {
        driver = sql::mysql::get_mysql_driver_instance();
        std::string hostStr = useLocal ? cfg.local_host : cfg.cloud_host;
        std::string userStr = useLocal ? cfg.local_user : cfg.cloud_user;
        std::string passStr = useLocal ? cfg.local_pass : cfg.cloud_pass;
        std::string schemaStr = useLocal ? cfg.local_schema : cfg.cloud_schema;
        bool useSSL = useLocal ? cfg.local_ssl : cfg.cloud_ssl;

        currentConnectedHost = hostStr;

        sql::ConnectOptionsMap connection_properties;
        connection_properties["hostName"] = hostStr;
        connection_properties["userName"] = userStr;
        connection_properties["password"] = passStr;

        if (useSSL) {
            std::string caStr = resolvePath(cfg.cloud_ca.c_str());
            connection_properties["sslCa"] = caStr;
            connection_properties["OPT_SSL_MODE"] = sql::SSL_MODE_REQUIRED;
        } else {
            connection_properties["OPT_SSL_MODE"] = sql::SSL_MODE_DISABLED;
        }

        connection_properties["OPT_CONNECT_TIMEOUT"] = useLocal ? (int)4 : (int)10;
        connection_properties["OPT_READ_TIMEOUT"] = (int)10;
        connection_properties["OPT_WRITE_TIMEOUT"] = (int)10;

        std::cout << "[DB] Connecting to " << (useLocal ? "LOCAL" : "CLOUD") << " database (" << hostStr << ")...\n";
        con = driver->connect(connection_properties);
        if (!con) {
            std::cerr << "[DB ERROR] Failed to connect to " << hostStr << "\n";
            return false;
        }
        con->setSchema(schemaStr);
        std::cout << "[DB] Successfully connected to " << (useLocal ? "LOCAL" : "CLOUD") << " database: " << schemaStr << "\n";

        // ---- Schema migration guard ----
        // We write a local file "schema_version.txt" after migrations complete.
        // If it already says the current version, skip all ALTER TABLE calls entirely
        // so startup doesn't waste 20+ network round-trips every launch.
        const int CURRENT_SCHEMA_VERSION = 4;
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
                        "IsCrafted BOOLEAN, "
                        "Fuel INT DEFAULT 0, "
                        "InputItem INT DEFAULT 0, "
                        "InputCount INT DEFAULT 0, "
                        "OutputItem INT DEFAULT 0, "
                        "OutputCount INT DEFAULT 0"
                        ")");
                    try { stmt->execute("ALTER TABLE campfires ADD COLUMN Fuel INT DEFAULT 0"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE campfires ADD COLUMN InputItem INT DEFAULT 0"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE campfires ADD COLUMN InputCount INT DEFAULT 0"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE campfires ADD COLUMN OutputItem INT DEFAULT 0"); }
                    catch (...) {}
                    try { stmt->execute("ALTER TABLE campfires ADD COLUMN OutputCount INT DEFAULT 0"); }
                    catch (...) {}
                }
            }
            catch (sql::SQLException& e) {
                std::cout << "[DEBUG] Table creation error: " << e.what() << "\n";
            }

            // world_settings --- shared world seed (one row, created once)
            try {
                std::unique_ptr<sql::Statement> stmt(con->createStatement());
                if (stmt) {
                    stmt->execute("CREATE TABLE IF NOT EXISTS world_settings ("
                        "id INT PRIMARY KEY DEFAULT 1, "
                        "world_seed INT NOT NULL DEFAULT 0)");
                    stmt->execute("INSERT IGNORE INTO world_settings (id, world_seed) "
                        "VALUES (1, FLOOR(RAND() * 2147483647))");
                }
            }
            catch (...) {}

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

            // chest_contents
            try {
                std::unique_ptr<sql::Statement> stmt(con->createStatement());
                if (stmt) {
                    stmt->execute("CREATE TABLE IF NOT EXISTS chest_contents ("
                        "Username VARCHAR(50), "
                        "ChestX FLOAT, "
                        "ChestY FLOAT, "
                        "SlotIndex INT, "
                        "ItemID INT, "
                        "Quantity INT, "
                        "PRIMARY KEY (Username, ChestX, ChestY, SlotIndex)"
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

bool AuthManager::isTargetedAdmin(const std::string& username, const std::string& email) {
    std::string u = username;
    for (char& c : u) c = (char)::tolower((unsigned char)c);
    std::string e = email;
    for (char& c : e) c = (char)::tolower((unsigned char)c);

    std::string uNoSpaces = u;
    uNoSpaces.erase(std::remove(uNoSpaces.begin(), uNoSpaces.end(), ' '), uNoSpaces.end());

    // Targeted accounts authorized as administrators
    if (u == "admin" || u == "administrator" || u == "kongjiacheng" || u == "staff" || uNoSpaces == "kongjiacheng") {
        return true;
    }
    if (u.rfind("admin", 0) == 0 || u.rfind("moderator", 0) == 0) {
        return true;
    }
    if (!e.empty()) {
        if (e.rfind("admin@", 0) == 0 || e == "admin@econova.com" || e.find("admin") != std::string::npos) {
            return true;
        }
    }
    return false;
}

// â”€â”€â”€ PBKDF2-SHA256 password hashing (Windows BCrypt API) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static std::string hashPasswordPBKDF2(const std::string& password) {
    UCHAR salt[16] = {};
    if (BCryptGenRandom(nullptr, salt, sizeof(salt), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        return password;   // fallback to plaintext if BCrypt unavailable

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                    BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        return password;

    UCHAR hash[32] = {};
    NTSTATUS st = BCryptDeriveKeyPBKDF2(
        hAlg,
        (PUCHAR)password.c_str(), (ULONG)password.size(),
        salt, sizeof(salt),
        100000, hash, sizeof(hash), 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (st != 0) return password;

    std::ostringstream oss;
    oss << "PBKDF2:SHA256:100000:";
    for (int i = 0; i < 16; i++)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)salt[i];
    oss << ":";
    for (int i = 0; i < 32; i++)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

static bool verifyPasswordPBKDF2(const std::string& password, const std::string& stored) {
    // Plaintext migration: passwords without the PBKDF2 prefix are compared directly
    if (stored.find("PBKDF2:") != 0)
        return password == stored;

    // Parse: PBKDF2:SHA256:<iter>:<salt_hex>:<hash_hex>
    std::istringstream ss(stored);
    std::vector<std::string> parts;
    std::string tok;
    while (std::getline(ss, tok, ':')) parts.push_back(tok);
    if (parts.size() < 5) return false;

    int iters = 100000;
    try { iters = std::stoi(parts[2]); } catch (...) { return false; }

    auto fromHex = [](const std::string& s, UCHAR* out, int len) {
        if ((int)s.size() < len * 2) return false;
        for (int i = 0; i < len; i++) {
            try { out[i] = (UCHAR)std::stoi(s.substr(i * 2, 2), nullptr, 16); }
            catch (...) { return false; }
        }
        return true;
    };

    UCHAR salt[16] = {}, storedHash[32] = {};
    if (!fromHex(parts[3], salt, 16)) return false;
    if (!fromHex(parts[4], storedHash, 32)) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                    BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return false;

    UCHAR hash[32] = {};
    NTSTATUS st = BCryptDeriveKeyPBKDF2(
        hAlg,
        (PUCHAR)password.c_str(), (ULONG)password.size(),
        salt, sizeof(salt),
        (ULONGLONG)iters, hash, sizeof(hash), 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (st != 0) return false;

    int diff = 0;
    for (int i = 0; i < 32; i++) diff |= (hash[i] ^ storedHash[i]);
    return diff == 0;
}

bool AuthManager::registerUser(const sql::SQLString& username, const sql::SQLString& password, const sql::SQLString& email) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::string uStr = std::string(username.c_str());
    std::string eStr = std::string(email.c_str());
    std::string assignedRole = isTargetedAdmin(uStr, eStr) ? "admin" : "user";
    std::string hashed = hashPasswordPBKDF2(std::string(password.c_str()));

    if (isOfflineMode()) {
        return SQLiteManager::getInstance().registerUser(uStr, hashed, eStr, assignedRole);
    }

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

        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "INSERT INTO Accounts (Username, Password, email, roles) VALUES (?, ?, ?, ?)"
        ));
        if (!pstmt) return false;
        pstmt->setString(1, username);
        pstmt->setString(2, sql::SQLString(hashed.c_str()));
        pstmt->setString(3, email);
        pstmt->setString(4, assignedRole);
        pstmt->executeUpdate();
        std::cout << "[AUTH] Account '" << uStr << "' registered with role '" << assignedRole << "'.\n";
        return true;
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during register: " << e.what() << " (Code: " << e.getErrorCode() << ")" << std::endl;
        return false;
    }
}

bool AuthManager::loginUser(const sql::SQLString& username, const sql::SQLString& password, std::string& errMessage) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        if (!SQLiteManager::getInstance().isOpen()) {
            errMessage = "Cannot access Local Offline Database (econova_offline.db)!";
            return false;
        }
        std::string outRole;
        std::string uStr = std::string(username.c_str());
        std::string pStr = std::string(password.c_str());
        bool ok = SQLiteManager::getInstance().loginUser(uStr, pStr, outRole, errMessage);
        if (ok) {
            if (isTargetedAdmin(uStr, "") && outRole != "admin" && outRole != "administrator") {
                SQLiteManager::getInstance().setUserRole(uStr, "admin");
                std::cout << "[AUTH] Targeted admin account '" << uStr << "' verified (offline). Role set to admin.\n";
            }
        }
        return ok;
    }
    if (!con) {
        errMessage = "Cannot connect to Server! Check internet or switch to Offline Mode.";
        return false;
    }
    try {
        // Fetch stored hash (or plaintext) by username only â€” verify in C++
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT Password, roles, (banned_until > CURRENT_TIMESTAMP) AS is_banned, "
            "DATE_FORMAT(banned_until, '%Y-%m-%d %H:%i:%S') as ban_date "
            "FROM Accounts WHERE Username = ?"
        ));
        if (!pstmt) {
            errMessage = "Database query preparation failed.";
            return false;
        }
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            std::string storedPwd = std::string(res->getString("Password").c_str());
            std::string inputPwd  = std::string(password.c_str());

            if (!verifyPasswordPBKDF2(inputPwd, storedPwd)) {
                errMessage = "Invalid Credentials!";
                return false;
            }

            // Auto-migrate plaintext password to PBKDF2 on first successful login
            if (storedPwd.find("PBKDF2:") != 0) {
                std::string newHash = hashPasswordPBKDF2(inputPwd);
                try {
                    std::unique_ptr<sql::PreparedStatement> psMig(con->prepareStatement(
                        "UPDATE Accounts SET Password = ? WHERE Username = ?"));
                    if (psMig) {
                        psMig->setString(1, sql::SQLString(newHash.c_str()));
                        psMig->setString(2, username);
                        psMig->executeUpdate();
                        std::cout << "[AUTH] Migrated plaintext password to PBKDF2 for: "
                                  << std::string(username.c_str()) << "\n";
                    }
                } catch (...) {}
            }

            bool isBanned = res->getBoolean("is_banned");
            if (isBanned) {
                std::string banDate = std::string(res->getString("ban_date"));
                if (banDate.find("9999") == 0) {
                    errMessage = "You are permanently banned!";
                } else {
                    errMessage = "You are banned until: " + banDate;
                }
                return false;
            }

            std::string uStr = std::string(username);
            std::string currentRole = std::string(res->getString("roles"));
            if (isTargetedAdmin(uStr, "") && currentRole != "admin" && currentRole != "administrator") {
                setUserRole(uStr, "admin");
                std::cout << "[AUTH] Targeted admin account '" << uStr << "' verified. Role set to admin.\n";
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
    if (isOfflineMode()) {
        std::cout << "[AUTH] Offline Mode: OTP is 123456\n";
        return "123456";
    }
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

        // â”€â”€ Send real OTP email via PowerShell SMTP â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        {
            std::string emailStr(email.c_str());
            std::string otpStr2(otp.c_str());

            // Always display generated OTP in server/console so reset is never blocked
            std::cout << "[AUTH] Generated OTP for " << emailStr << ": " << otpStr2 << "\n";

            // Read email credentials from email_config.txt (check multiple common paths)
            std::string fromEmail, appPassword, smtpServer = "smtp.gmail.com";
            int smtpPort = 587;
            const std::vector<std::string> configPaths = {
                "email_config.txt",
                "Release/email_config.txt",
                "../email_config.txt",
                "Econova/email_config.txt"
            };

            for (const auto& path : configPaths) {
                std::ifstream cfgFile(path);
                if (cfgFile.is_open()) {
                    std::string ln;
                    while (std::getline(cfgFile, ln)) {
                        while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ' || ln.back() == '\t')) ln.pop_back();
                        if (ln.empty() || ln[0] == '#') continue;
                        if (ln.rfind("from=", 0) == 0)          fromEmail   = ln.substr(5);
                        else if (ln.rfind("password=", 0) == 0) appPassword = ln.substr(9);
                        else if (ln.rfind("smtp=", 0) == 0)     smtpServer  = ln.substr(5);
                        else if (ln.rfind("port=", 0) == 0)     { try { smtpPort = std::stoi(ln.substr(5)); } catch (...) {} }
                    }
                    if (!fromEmail.empty() && !appPassword.empty()) break;
                }
            }

            // Clean up credentials
            while (!fromEmail.empty() && std::isspace((unsigned char)fromEmail.front())) fromEmail.erase(fromEmail.begin());
            while (!fromEmail.empty() && std::isspace((unsigned char)fromEmail.back()))  fromEmail.pop_back();
            // Remove any spaces from app password (Gmail app passwords are given as 4x4 groups like "abcd efgh ijkl mnop")
            appPassword.erase(std::remove_if(appPassword.begin(), appPassword.end(), [](unsigned char c) { return std::isspace(c); }), appPassword.end());

            bool isConfigured = !fromEmail.empty() && !appPassword.empty() &&
                                fromEmail.find("your_game_email") == std::string::npos &&
                                fromEmail.find("example.com") == std::string::npos &&
                                fromEmail.find('@') != std::string::npos &&
                                appPassword.find("your_16_char_app_password") == std::string::npos;

            if (isConfigured) {
                std::cout << "[AUTH] Dispatching OTP email via " << smtpServer << ":" << smtpPort << " to " << emailStr << "...\n";
                // Fire-and-forget async email via PowerShell Send-MailMessage with TLS 1.2/1.3 and try/catch
                std::thread([emailStr, otpStr2, fromEmail, appPassword, smtpServer, smtpPort]() {
                    try {
                        auto esc = [](std::string s) {
                            std::string r; r.reserve(s.size());
                            for (char c : s) { if (c == '\'') r += "''"; else r += c; }
                            return r;
                        };
                        std::string body =
                            "Your Econova password reset OTP is: " + otpStr2 +
                            ". This code is valid for 15 minutes."
                            " If you did not request this, please ignore this email.";
                        std::string ps =
                            "powershell -NonInteractive -NoProfile -Command \""
                            "try { "
                            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13; "
                            "$pw = ConvertTo-SecureString '" + esc(appPassword) + "' -AsPlainText -Force; "
                            "$cred = New-Object System.Management.Automation.PSCredential('" + esc(fromEmail) + "', $pw); "
                            "$mailParams = @{ "
                            "  SmtpServer = '" + esc(smtpServer) + "'; "
                            "  Port = " + std::to_string(smtpPort) + "; "
                            "  UseSsl = $true; "
                            "  Credential = $cred; "
                            "  From = '" + esc(fromEmail) + "'; "
                            "  To = '" + esc(emailStr) + "'; "
                            "  Subject = 'Econova Password Reset OTP'; "
                            "  Body = '" + esc(body) + "'; "
                            "}; "
                            "Send-MailMessage @mailParams -ErrorAction Stop; "
                            "Write-Host '[AUTH] Email sent successfully to " + esc(emailStr) + "'; "
                            "} catch { "
                            "Write-Host ('[AUTH ERROR] Email dispatch failed: ' + $_.Exception.Message); "
                            "if ($_.Exception.Message -match 'transport connection' -or $_.Exception.Message -match '535' -or $_.Exception.Message -match 'Failure sending mail') { "
                            "  Write-Host '[AUTH HINT] Gmail authentication failed. Ensure 2-Step Verification is active and you are using a 16-character App Password (https://myaccount.google.com/apppasswords).'; "
                            "} "
                            "}\"";
                        system(ps.c_str());
                    } catch (...) {}
                }).detach();
            } else {
                std::cout << "=======================================================\n"
                          << "[AUTH] EMAIL SERVICE NOT CONFIGURED\n"
                          << "To send real OTP emails, edit 'email_config.txt' with:\n"
                          << "  from=yourrealemail@gmail.com\n"
                          << "  password=your16characterapppassword\n"
                          << "(Generate a 16-character App Password at: https://myaccount.google.com/apppasswords)\n"
                          << ">>> Enter OTP in-game: " << otpStr2 << " <<<\n"
                          << "=======================================================\n";
            }
        }

        return otp;
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error during OTP gen: " << e.what() << std::endl;
        return "";
    }
}

bool AuthManager::resetPassword(const sql::SQLString& email, const sql::SQLString& otp, const sql::SQLString& newPassword) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        std::string hashed = hashPasswordPBKDF2(std::string(newPassword.c_str()));
        return SQLiteManager::getInstance().resetPassword(std::string(email.c_str()), hashed);
    }
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
        {
            std::string hashed = hashPasswordPBKDF2(std::string(newPassword.c_str()));
            pstmtUpdate->setString(1, sql::SQLString(hashed.c_str()));
        }
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
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().saveInventory(std::string(username.c_str()), inventoryData);
    }
    if (!con) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM Inventory WHERE Username = ?"));
        if (!delStmt) return false;
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        struct ValidItem { std::string name; int count; };
        std::vector<ValidItem> toInsert;
        for (int slot = 0; slot < (int)inventoryData.size(); ++slot) {
            int item_id = inventoryData[slot].first;
            int count = inventoryData[slot].second;
            if (item_id != 0 && count > 0) {
                std::string itemName =
                    (item_id == 1) ? "Tree" :
                    (item_id == 2) ? "Stone" :
                    (item_id == 3) ? "Wood" :
                    (item_id == 4) ? "WoodenSword" :
                    (item_id == 5) ? "Sapling" :
                    (item_id == 6) ? "Shield" :
                    (item_id == 7) ? "Furnace" :
                    (item_id == 8) ? "WoodTile" :
                    (item_id == 9) ? "WoodWall" :
                    (item_id == 10) ? "WoodDoor" :
                    (item_id == 11) ? "WoodRoof" :
                    (item_id == 12) ? "WoodStairs" :
                    (item_id == 13) ? "Bed" :
                    (item_id == 14) ? "Wrench" :
                    (item_id == 100) ? "ChickenMeat" :
                    (item_id == 101) ? "PigMeat" :
                    (item_id == 102) ? "ZombieFlesh" :
                    (item_id == 103) ? "CookedChicken" :
                    (item_id == 104) ? "CookedPig" :
                    (item_id == 105) ? "SheepMeat" :
                    (item_id == 106) ? "CookedSheep" :
                    (item_id == 107) ? "Wool" :
                    (item_id == 201) ? "Coal" :
                    (item_id == 202) ? "CopperOre" :
                    (item_id == 203) ? "CopperBar" :
                    (item_id == 204) ? "IronOre" :
                    (item_id == 205) ? "IronBar" :
                    (item_id == 206) ? "GoldOre" :
                    (item_id == 207) ? "GoldBar" :
                    (item_id == 208) ? "Diamond" : "Unknown";
                toInsert.push_back({ itemName, count });
            }
        }

        if (!toInsert.empty()) {
            // MySQL prepared statements cap at 65535 placeholders total.
            // Inventory has 3 columns per row => max safe chunk = 65535/3 = 21845.
            // Use 500 rows per chunk to be consistent and future-proof.
            const int CHUNK_SIZE = 500;
            const int total = (int)toInsert.size();
            for (int base = 0; base < total; base += CHUNK_SIZE) {
                int count = (std::min)(CHUNK_SIZE, total - base);
                std::string sql = "INSERT INTO Inventory (Username, ItemName, Quantity) VALUES ";
                for (int i = 0; i < count; ++i) {
                    if (i > 0) sql += ", ";
                    sql += "(?, ?, ?)";
                }
                std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement(sql));
                if (!insStmt) continue;
                int paramIdx = 1;
                for (int i = 0; i < count; ++i) {
                    const auto& item = toInsert[base + i];
                    insStmt->setString(paramIdx++, username);
                    insStmt->setString(paramIdx++, item.name);
                    insStmt->setInt(paramIdx++, item.count);
                }
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
    if (isOfflineMode()) {
        SQLiteManager::getInstance().savePlayerStats(std::string(username.c_str()), hunger, thirst, currency, level, exp, elapsedTime, hungerMultiplier, thirstMultiplier);
        return;
    }
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
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadPlayerStats(std::string(username.c_str()));
    }
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
    if (isOfflineMode()) {
        SQLiteManager::getInstance().saveGender(std::string(username.c_str()), gender);
        return;
    }
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
    if (isOfflineMode()) {
        SQLiteManager::getInstance().saveStructures(std::string(username.c_str()), structures);
        return;
    }
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM structures WHERE Username = ?"));
        if (!delStmt) return;
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        if (!structures.empty()) {
            // MySQL prepared statements cap at 65535 placeholders total.
            // structures has 8 columns per row => max safe chunk = 65535/8 = 8191.
            // Use 500 rows per chunk (4000 placeholders) for a comfortable safety margin.
            const int CHUNK_SIZE = 500;
            const int total = (int)structures.size();
            for (int base = 0; base < total; base += CHUNK_SIZE) {
                int count = (std::min)(CHUNK_SIZE, total - base);
                std::string sql = "INSERT INTO structures (Username, ItemID, PosX, PosY, Health, Rotation, IsOpen, ShearY) VALUES ";
                for (int i = 0; i < count; ++i) {
                    if (i > 0) sql += ", ";
                    sql += "(?, ?, ?, ?, ?, ?, ?, ?)";
                }
                std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement(sql));
                if (!insStmt) continue;
                int paramIdx = 1;
                for (int i = 0; i < count; ++i) {
                    const auto& s = structures[base + i];
                    insStmt->setString(paramIdx++, username);
                    insStmt->setInt(paramIdx++, s.id);
                    insStmt->setDouble(paramIdx++, (double)s.x);
                    insStmt->setDouble(paramIdx++, (double)s.y);
                    insStmt->setDouble(paramIdx++, (double)s.health);
                    insStmt->setDouble(paramIdx++, (double)s.rotation);
                    insStmt->setBoolean(paramIdx++, s.isOpen);
                    insStmt->setDouble(paramIdx++, (double)s.shearY);
                }
                insStmt->executeUpdate();
            }
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "saveStructures error: " << e.what() << "\n";
    }
}

std::vector<StoredStructure> AuthManager::loadStructures(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadStructures(std::string(username.c_str()));
    }
    std::vector<StoredStructure> structures;
    if (!con) return structures;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ItemID, PosX, PosY, Health, Rotation, IsOpen, ShearY FROM structures WHERE Username = ?"));
        if (!pstmt) return structures;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            structures.push_back({ res->getInt("ItemID"), (float)res->getDouble("PosX"), (float)res->getDouble("PosY"), (float)res->getDouble("Health"), (float)res->getDouble("Rotation"), res->getBoolean("IsOpen"), (float)res->getDouble("ShearY"), std::string(username.c_str()) });
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "loadStructures error: " << e.what() << "\n";
    }
    return structures;
}

std::vector<StoredStructure> AuthManager::loadOtherStructures(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadOtherStructures(std::string(username.c_str()));
    }
    std::vector<StoredStructure> structures;
    if (!con) return structures;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT Username, ItemID, PosX, PosY, Health, Rotation, IsOpen, ShearY FROM structures WHERE Username != ?"));
        if (!pstmt) return structures;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            structures.push_back({ res->getInt("ItemID"), (float)res->getDouble("PosX"), (float)res->getDouble("PosY"), (float)res->getDouble("Health"), (float)res->getDouble("Rotation"), res->getBoolean("IsOpen"), (float)res->getDouble("ShearY"), std::string(res->getString("Username")) });
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "loadOtherStructures error: " << e.what() << "\n";
    }
    return structures;
}

void AuthManager::ensureCampfireColumns() {
    if (!con) return;
    try {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        if (stmt) {
            stmt->execute("CREATE TABLE IF NOT EXISTS campfires ("
                "ID INT AUTO_INCREMENT PRIMARY KEY, "
                "Username VARCHAR(50), "
                "PosX FLOAT, "
                "PosY FLOAT, "
                "TimeRemaining FLOAT, "
                "IsCrafted BOOLEAN, "
                "Fuel INT DEFAULT 0, "
                "InputItem INT DEFAULT 0, "
                "InputCount INT DEFAULT 0, "
                "OutputItem INT DEFAULT 0, "
                "OutputCount INT DEFAULT 0"
                ")");
            try { stmt->execute("ALTER TABLE campfires ADD COLUMN Fuel INT DEFAULT 0"); } catch (...) {}
            try { stmt->execute("ALTER TABLE campfires ADD COLUMN InputItem INT DEFAULT 0"); } catch (...) {}
            try { stmt->execute("ALTER TABLE campfires ADD COLUMN InputCount INT DEFAULT 0"); } catch (...) {}
            try { stmt->execute("ALTER TABLE campfires ADD COLUMN OutputItem INT DEFAULT 0"); } catch (...) {}
            try { stmt->execute("ALTER TABLE campfires ADD COLUMN OutputCount INT DEFAULT 0"); } catch (...) {}
        }
    }
    catch (...) {}
}

void AuthManager::saveCampfires(const sql::SQLString& username, const std::vector<StoredCampfire>& campfires) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        SQLiteManager::getInstance().saveCampfires(std::string(username.c_str()), campfires);
        return;
    }
    if (!con) return;
    for (int attempt = 0; attempt < 2; attempt++) {
        try {
            std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM campfires WHERE Username = ?"));
            if (!delStmt) return;
            delStmt->setString(1, username);
            delStmt->executeUpdate();

            if (!campfires.empty()) {
                // MySQL prepared statements cap at 65535 placeholders total.
                // campfires has 10 columns per row => max safe chunk = 65535/10 = 6553.
                // Use 500 rows per chunk (5000 placeholders) for a comfortable safety margin.
                const int CHUNK_SIZE = 500;
                const int total = (int)campfires.size();
                for (int base = 0; base < total; base += CHUNK_SIZE) {
                    int count = (std::min)(CHUNK_SIZE, total - base);
                    std::string sql = "INSERT INTO campfires (Username, PosX, PosY, TimeRemaining, IsCrafted, Fuel, InputItem, InputCount, OutputItem, OutputCount) VALUES ";
                    for (int i = 0; i < count; ++i) {
                        if (i > 0) sql += ", ";
                        sql += "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
                    }
                    std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement(sql));
                    if (!insStmt) continue;
                    int paramIdx = 1;
                    for (int i = 0; i < count; ++i) {
                        const auto& c = campfires[base + i];
                        insStmt->setString(paramIdx++, username);
                        insStmt->setDouble(paramIdx++, c.x);
                        insStmt->setDouble(paramIdx++, c.y);
                        insStmt->setDouble(paramIdx++, c.timeRemaining);
                        insStmt->setBoolean(paramIdx++, c.isCrafted);
                        insStmt->setInt(paramIdx++, c.fuel);
                        insStmt->setInt(paramIdx++, c.inputItem);
                        insStmt->setInt(paramIdx++, c.inputCount);
                        insStmt->setInt(paramIdx++, c.outputItem);
                        insStmt->setInt(paramIdx++, c.outputCount);
                    }
                    insStmt->executeUpdate();
                }
            }
            return;
        }
        catch (sql::SQLException& e) {
            if (attempt == 0) {
                ensureCampfireColumns();
                continue;
            }
            std::cerr << "saveCampfires error: " << e.what() << "\n";
        }
    }
}

std::vector<StoredCampfire> AuthManager::loadCampfires(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadCampfires(std::string(username.c_str()));
    }
    std::vector<StoredCampfire> campfires;
    if (!con) return campfires;
    for (int attempt = 0; attempt < 2; attempt++) {
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT PosX, PosY, TimeRemaining, IsCrafted, Fuel, InputItem, InputCount, OutputItem, OutputCount FROM campfires WHERE Username = ?"));
            if (!pstmt) return campfires;
            pstmt->setString(1, username);
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            while (res && res->next()) {
                StoredCampfire sc;
                sc.x = (float)res->getDouble("PosX");
                sc.y = (float)res->getDouble("PosY");
                sc.timeRemaining = (float)res->getDouble("TimeRemaining");
                sc.isCrafted = res->getBoolean("IsCrafted");
                sc.fuel = res->getInt("Fuel");
                sc.inputItem = res->getInt("InputItem");
                sc.inputCount = res->getInt("InputCount");
                sc.outputItem = res->getInt("OutputItem");
                sc.outputCount = res->getInt("OutputCount");
                sc.owner = std::string(username.c_str());  // own campfires
                campfires.push_back(sc);
            }
            return campfires;
        }
        catch (sql::SQLException& e) {
            if (attempt == 0) {
                ensureCampfireColumns();
                continue;
            }
            std::cerr << "loadCampfires error: " << e.what() << "\n";
        }
    }
    return campfires;
}


std::vector<StoredCampfire> AuthManager::loadOtherCampfires(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadOtherCampfires(std::string(username.c_str()));
    }
    std::vector<StoredCampfire> campfires;
    if (!con) return campfires;
    for (int attempt = 0; attempt < 2; attempt++) {
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
                "SELECT Username, PosX, PosY, TimeRemaining, IsCrafted, Fuel, InputItem, InputCount, OutputItem, OutputCount FROM campfires WHERE Username != ?"));
            if (!pstmt) return campfires;
            pstmt->setString(1, username);
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            while (res && res->next()) {
                StoredCampfire sc;
                sc.owner      = std::string(res->getString("Username"));
                sc.x          = (float)res->getDouble("PosX");
                sc.y          = (float)res->getDouble("PosY");
                sc.timeRemaining = (float)res->getDouble("TimeRemaining");
                sc.isCrafted  = res->getBoolean("IsCrafted");
                sc.fuel       = res->getInt("Fuel");
                sc.inputItem  = res->getInt("InputItem");
                sc.inputCount = res->getInt("InputCount");
                sc.outputItem = res->getInt("OutputItem");
                sc.outputCount= res->getInt("OutputCount");
                campfires.push_back(sc);
            }
            return campfires;
        }
        catch (sql::SQLException& e) {
            if (attempt == 0) {
                ensureCampfireColumns();
                continue;
            }
            std::cerr << "loadOtherCampfires error: " << e.what() << "\n";
        }
    }
    return campfires;
}

bool AuthManager::updateCampfireCookingState(float x, float y, int fuel, int inputItem, int inputCount, int outputItem, int outputCount, float timeRemaining) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().updateCampfireCookingState(x, y, fuel, inputItem, inputCount, outputItem, outputCount, timeRemaining);
    }
    if (!con) return false;
    for (int attempt = 0; attempt < 2; attempt++) {
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
                "UPDATE campfires SET Fuel = ?, InputItem = ?, InputCount = ?, OutputItem = ?, OutputCount = ?, TimeRemaining = ? "
                "WHERE ABS(PosX - ?) < 5.0 AND ABS(PosY - ?) < 5.0"
            ));
            if (!pstmt) return false;
            pstmt->setInt(1, fuel);
            pstmt->setInt(2, inputItem);
            pstmt->setInt(3, inputCount);
            pstmt->setInt(4, outputItem);
            pstmt->setInt(5, outputCount);
            pstmt->setDouble(6, (double)timeRemaining);
            pstmt->setDouble(7, (double)x);
            pstmt->setDouble(8, (double)y);
            int rows = pstmt->executeUpdate();
            return rows > 0;
        }
        catch (sql::SQLException& e) {
            if (attempt == 0) {
                ensureCampfireColumns();
                continue;
            }
            std::cerr << "updateCampfireCookingState error: " << e.what() << "\n";
        }
    }
    return false;
}

int AuthManager::getOrCreateWorldSeed() {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().getOrCreateWorldSeed();
    }
    if (!con) return 12345;
    try {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        if (!stmt) return 12345;
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
            "SELECT world_seed FROM world_settings WHERE id = 1"));
        if (res && res->next()) return res->getInt("world_seed");
    }
    catch (sql::SQLException& e) {
        std::cerr << "getOrCreateWorldSeed error: " << e.what() << "\n";
    }
    return 12345;
}

long long AuthManager::getServerEpochSeconds() {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return (long long)std::time(nullptr);
    }
    if (!con) return 0LL;
    try {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        if (!stmt) return 0LL;
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT UNIX_TIMESTAMP() AS epoch"));
        if (res && res->next()) return (long long)res->getInt64("epoch");
    }
    catch (sql::SQLException& e) {
        std::cerr << "getServerEpochSeconds error: " << e.what() << "\n";
    }
    return 0LL;
}

void AuthManager::setFirstTimeFinished(const sql::SQLString& username)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        SQLiteManager::getInstance().setFirstTimeFinished(std::string(username.c_str()));
        return;
    }
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

void AuthManager::setPendingChatNotification(const std::string& notif)
{
    std::lock_guard<std::mutex> lock(chatNotifMutex);
    pendingChatNotification = notif;
}

std::string AuthManager::getAndClearChatNotification()
{
    std::lock_guard<std::mutex> lock(chatNotifMutex);
    std::string res = pendingChatNotification;
    pendingChatNotification.clear();
    return res;
}

bool AuthManager::sendChatMessage(const sql::SQLString& username, const sql::SQLString& message, std::string& outError)
{
    if (isOfflineMode() && !con) {
        return true;
    }
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) {
        if (isOfflineMode()) return true;
        outError = "Database connection error.";
        return false;
    }
    try {
        if (con->isClosed()) {
            if (isOfflineMode()) return true;
            outError = "Database connection closed.";
            return false;
        }
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
        if (!pstmt) {
            if (isOfflineMode()) return true;
            outError = "Query preparation failed.";
            return false;
        }
        pstmt->setString(1, username);
        pstmt->setString(2, message);
        pstmt->executeUpdate();
        return true;
    }
    catch (sql::SQLException& e) {
        std::cout << "Send chat message error: " << e.what() << std::endl;
        if (isOfflineMode()) return true;
        outError = "Database error: " + std::string(e.what());
        return false;
    }
    catch (...) {
        if (isOfflineMode()) return true;
        outError = "Unknown chat error.";
        return false;
    }
}

std::vector<std::pair<std::string, std::string>> AuthManager::fetchChatMessages(int limit, int sinceMsgId)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<std::pair<std::string, std::string>> messages;
    if (!con) return messages;
    try {
        if (con->isClosed()) return messages;
        // Only fetch messages from players currently in the game (active within last 20 seconds)
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "SELECT c.Username, c.Message FROM ChatMessages c "
                "JOIN Accounts a ON c.Username = a.Username "
                "WHERE c.MessageID > ? AND a.lastActive > (CURRENT_TIMESTAMP - INTERVAL 20 SECOND) "
                "ORDER BY c.SentAt DESC LIMIT ?"
            )
        );
        if (!pstmt) {
            std::unique_ptr<sql::PreparedStatement> pstmtFallback(
                con->prepareStatement("SELECT Username, Message FROM ChatMessages WHERE MessageID > ? ORDER BY SentAt DESC LIMIT ?")
            );
            if (!pstmtFallback) return messages;
            pstmtFallback->setInt(1, sinceMsgId);
            pstmtFallback->setInt(2, limit);
            std::unique_ptr<sql::ResultSet> resFb(pstmtFallback->executeQuery());
            while (resFb && resFb->next()) {
                std::string user = resFb->isNull("Username") ? "" : std::string(resFb->getString("Username"));
                std::string msg = resFb->isNull("Message") ? "" : std::string(resFb->getString("Message"));
                messages.push_back({ user, msg });
            }
            std::reverse(messages.begin(), messages.end());
            return messages;
        }
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

void AuthManager::clearPlayerChatMessages(const sql::SQLString& username)
{
    if (isOfflineMode() && !con) return;
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        if (con->isClosed()) return;
        // Delete this player's chat messages from server database
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("DELETE FROM ChatMessages WHERE Username = ?")
        );
        if (pstmt) {
            pstmt->setString(1, username);
            pstmt->executeUpdate();
        }

        // Check if other players are active in the game
        std::unique_ptr<sql::PreparedStatement> pstmtActive(
            con->prepareStatement("SELECT COUNT(*) AS active_count FROM Accounts WHERE lastActive > (CURRENT_TIMESTAMP - INTERVAL 20 SECOND) AND Username != ?")
        );
        if (pstmtActive) {
            pstmtActive->setString(1, username);
            std::unique_ptr<sql::ResultSet> res(pstmtActive->executeQuery());
            if (res && res->next()) {
                int activeCount = res->getInt("active_count");
                if (activeCount <= 0) {
                    std::unique_ptr<sql::Statement> stmt(con->createStatement());
                    if (stmt) {
                        stmt->execute("DELETE FROM ChatMessages");
                    }
                }
            }
        }
    }
    catch (sql::SQLException& e) {
        std::cout << "Clear player chat error: " << e.what() << std::endl;
    }
    catch (...) {}
}

void AuthManager::cleanupInactiveChatMessages()
{
    if (isOfflineMode() && !con) return;
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        if (con->isClosed()) return;
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        if (stmt) {
            // Delete messages from users who have left (not active in the last 20 seconds)
            stmt->execute(
                "DELETE FROM ChatMessages WHERE Username NOT IN "
                "(SELECT Username FROM Accounts WHERE lastActive > (CURRENT_TIMESTAMP - INTERVAL 20 SECOND))"
            );
        }
    }
    catch (sql::SQLException& e) {
        std::cout << "Cleanup inactive chat error: " << e.what() << std::endl;
    }
    catch (...) {}
}

void AuthManager::clearAllChatMessages()
{
    if (isOfflineMode() && !con) return;
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        if (con->isClosed()) return;
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        if (stmt) {
            stmt->execute("DELETE FROM ChatMessages");
        }
    }
    catch (sql::SQLException& e) {
        std::cout << "Clear all chat error: " << e.what() << std::endl;
    }
    catch (...) {}
}

std::vector<std::pair<int, int>> AuthManager::loadInventory(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadInventory(std::string(username.c_str()));
    }
    std::vector<std::pair<int, int>> inventoryData(20, { 0, 0 }); // Default 20 empty slots
    if (!con) return inventoryData;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ItemName, Quantity FROM Inventory WHERE Username = ?"));
        if (!pstmt) return inventoryData;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        int slot = 0;
        while (res && res->next() && slot < 20) {
            std::string itemName = std::string(res->getString("ItemName"));
            int count = res->getInt("Quantity");

            int item_id =
                (itemName == "Tree") ? 1 :
                (itemName == "Stone") ? 2 :
                (itemName == "Wood") ? 3 :
                (itemName == "WoodenSword") ? 4 :
                (itemName == "Sapling") ? 5 :
                (itemName == "Shield") ? 6 :
                (itemName == "Furnace") ? 7 :
                (itemName == "WoodTile") ? 8 :
                (itemName == "WoodWall") ? 9 :
                (itemName == "WoodDoor") ? 10 :
                (itemName == "WoodRoof") ? 11 :
                (itemName == "WoodStairs") ? 12 :
                (itemName == "Bed") ? 13 :
                (itemName == "Wrench") ? 14 :
                (itemName == "ChickenMeat") ? 100 :
                (itemName == "PigMeat") ? 101 :
                (itemName == "ZombieFlesh") ? 102 :
                (itemName == "CookedChicken") ? 103 :
                (itemName == "CookedPig") ? 104 :
                (itemName == "SheepMeat") ? 105 :
                (itemName == "CookedSheep") ? 106 :
                (itemName == "Wool") ? 107 :
                (itemName == "Coal") ? 201 :
                (itemName == "CopperOre") ? 202 :
                (itemName == "CopperBar") ? 203 :
                (itemName == "IronOre") ? 204 :
                (itemName == "IronBar") ? 205 :
                (itemName == "GoldOre") ? 206 :
                (itemName == "GoldBar") ? 207 :
                (itemName == "Diamond") ? 208 : 0;

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
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().getQuestInfo(std::string(username.c_str()));
    }
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
    if (isOfflineMode()) {
        SQLiteManager::getInstance().updateQuestProgress(std::string(username.c_str()), progress, completed);
        return;
    }
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
    if (isOfflineMode()) {
        SQLiteManager::getInstance().resetQuest(std::string(username.c_str()), questID, std::string(date.c_str()));
        return;
    }
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
    if (isOfflineMode()) return;
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

void AuthManager::setPlayerOffline(const sql::SQLString& username) {
    if (isOfflineMode()) return;
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE Accounts SET lastActive = '2000-01-01 00:00:00' WHERE Username = ?"));
        if (!pstmt) return;
        pstmt->setString(1, username);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error in setPlayerOffline: " << e.what() << "\n";
    }
}

std::vector<OnlinePlayer> AuthManager::getOnlinePlayers(const sql::SQLString& excludeUser) {
    if (isOfflineMode()) return {};
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<OnlinePlayer> players;
    if (!con) return players;
    try {
        // Fetch players active in the last 30 seconds
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT Username, posX, posY, pullBy, gender FROM Accounts WHERE Username != ? AND lastActive > (CURRENT_TIMESTAMP - INTERVAL 30 SECOND)"));
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
    OnlinePlayer p;
    p.username = username.c_str();
    if (isOfflineMode()) return p;
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
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
    if (isOfflineMode()) return;
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
    if (isOfflineMode()) return;
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
    if (isOfflineMode()) return;
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
    if (isOfflineMode()) return {};
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
    std::string uStr = std::string(username);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().getUserRole(uStr);
    }
    if (!con) return isTargetedAdmin(uStr, "") ? "admin" : "user";
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT roles FROM Accounts WHERE Username = ?"));
        if (!pstmt) return isTargetedAdmin(uStr, "") ? "admin" : "user";
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            std::string roleStr = std::string(res->getString("roles"));
            if (roleStr.empty()) {
                roleStr = isTargetedAdmin(uStr, "") ? "admin" : "user";
            }
            return roleStr;
        }
    }
    catch (...) {}
    return isTargetedAdmin(uStr, "") ? "admin" : "user";
}

bool AuthManager::setUserRole(const std::string& username, const std::string& roles) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().setUserRole(username, roles);
    }
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
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadMarketPrice(itemID);
    }
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
    if (isOfflineMode()) {
        SQLiteManager::getInstance().saveMarketPrice(itemID, price);
        return;
    }
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

std::map<int, float> AuthManager::loadAllMarketPrices()
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadAllMarketPrices();
    }
    std::map<int, float> prices;
    if (!con)
        return prices;

    try
    {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        if (!stmt)
            return prices;

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT ItemID, Price FROM MarketPrices"));
        while (res && res->next())
        {
            prices[res->getInt("ItemID")] = (float)res->getDouble("Price");
        }
    }
    catch (sql::SQLException& e)
    {
        std::cout << "loadAllMarketPrices error: "
            << e.what() << std::endl;
    }

    return prices;
}

void AuthManager::saveAllMarketPrices(const std::vector<std::pair<int, float>>& prices)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        SQLiteManager::getInstance().saveAllMarketPrices(prices);
        return;
    }
    if (!con || prices.empty())
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

        for (const auto& p : prices)
        {
            pstmt->setInt(1, p.first);
            pstmt->setDouble(2, (double)p.second);
            pstmt->executeUpdate();
        }
    }
    catch (sql::SQLException& e)
    {
        std::cout << "saveAllMarketPrices error: "
            << e.what() << std::endl;
    }
}

// =========================================================
// P2P TRADING METHODS
// =========================================================

int AuthManager::createP2PTrade(const std::string& from, const std::string& to) {
    if (isOfflineMode()) return -1;
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
    if (isOfflineMode()) return;
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
            ? "UPDATE PlayerTrades SET InitiatorItems=?, InitiatorGold=? WHERE TradeID=?"
            : "UPDATE PlayerTrades SET TargetItems=?, TargetGold=? WHERE TradeID=?";
        std::unique_ptr<sql::PreparedStatement> upd(con->prepareStatement(sql));
        upd->setString(1, itemsCSV);
        upd->setInt(2, gold);
        upd->setInt(3, tradeID);
        upd->executeUpdate();
    }
    catch (sql::SQLException& e) { std::cerr << "updateP2POffer error: " << e.what() << "\n"; }
}

void AuthManager::setP2PReady(int tradeID, const std::string& user, bool ready) {
    if (isOfflineMode()) return;
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
    P2PTradeState s;
    s.tradeID = tradeID;
    if (isOfflineMode()) return s;
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
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
    if (isOfflineMode()) return;
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
    if (isOfflineMode()) return -1;
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
    if (isOfflineMode()) return;
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
    if (isOfflineMode()) return;
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

void AuthManager::saveChestContents(const sql::SQLString& username, const std::vector<StoredChestItem>& items) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        SQLiteManager::getInstance().saveChestContents(std::string(username.c_str()), items);
        return;
    }
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM chest_contents WHERE Username = ?"));
        if (!delStmt) return;
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement("INSERT INTO chest_contents (Username, ChestX, ChestY, SlotIndex, ItemID, Quantity) VALUES (?, ?, ?, ?, ?, ?)"));
        if (!insStmt) return;
        for (auto& item : items) {
            insStmt->setString(1, username);
            insStmt->setDouble(2, (double)item.chestX);
            insStmt->setDouble(3, (double)item.chestY);
            insStmt->setInt(4, item.slotIndex);
            insStmt->setInt(5, item.itemID);
            insStmt->setInt(6, item.quantity);
            insStmt->executeUpdate();
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "saveChestContents error: " << e.what() << "\n";
    }
}

std::vector<StoredChestItem> AuthManager::loadChestContents(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadChestContents(std::string(username.c_str()));
    }
    std::vector<StoredChestItem> items;
    if (!con) return items;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ChestX, ChestY, SlotIndex, ItemID, Quantity FROM chest_contents WHERE Username = ?"));
        if (!pstmt) return items;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            items.push_back({
                (float)res->getDouble("ChestX"),
                (float)res->getDouble("ChestY"),
                res->getInt("SlotIndex"),
                res->getInt("ItemID"),
                res->getInt("Quantity")
            });
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "loadChestContents error: " << e.what() << "\n";
    }
    return items;
}

std::vector<StoredChestItem> AuthManager::loadOtherChestContents(const sql::SQLString& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().loadOtherChestContents(std::string(username.c_str()));
    }
    std::vector<StoredChestItem> items;
    if (!con) return items;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT ChestX, ChestY, SlotIndex, ItemID, Quantity FROM chest_contents WHERE Username != ?"));
        if (!pstmt) return items;
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            items.push_back({
                (float)res->getDouble("ChestX"),
                (float)res->getDouble("ChestY"),
                res->getInt("SlotIndex"),
                res->getInt("ItemID"),
                res->getInt("Quantity")
            });
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "loadOtherChestContents error: " << e.what() << "\n";
    }
    return items;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Disaster Scheduling â€” admin panel support
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

// Schedule a new disaster. scheduled_at is UTC epoch seconds.
// Returns the new event_id, or -1 on failure.
int AuthManager::scheduleDisaster(const std::string& type, long long scheduledAtEpoch,
                                   int durationSeconds, const std::string& createdBy)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().scheduleDisaster(type, scheduledAtEpoch, durationSeconds, createdBy);
    }
    if (!con) return -1;
    try {
        // Convert epoch â†’ "YYYY-MM-DD HH:MM:SS" (UTC)
        std::string dtStr;
        {
            time_t t = (time_t)scheduledAtEpoch;
            struct tm utmBuf;
#ifdef _WIN32
            gmtime_s(&utmBuf, &t);
#else
            gmtime_r(&t, &utmBuf);
#endif
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &utmBuf);
            dtStr = buf;
        }
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "INSERT INTO disaster_events (type, scheduled_at, duration_sec, status, created_by) "
            "VALUES (?, ?, ?, 'pending', ?)"));
        if (!pstmt) return -1;
        pstmt->setString(1, sql::SQLString(type.c_str()));
        pstmt->setString(2, sql::SQLString(dtStr.c_str()));
        pstmt->setInt(3, durationSeconds);
        pstmt->setString(4, sql::SQLString(createdBy.c_str()));
        pstmt->executeUpdate();

        // Retrieve the auto-generated ID
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
        if (res && res->next()) return res->getInt("id");
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Disaster] scheduleDisaster error: " << e.what() << "\n";
    }
    return -1;
}

// Returns the first pending disaster whose scheduled_at <= NOW() (UTC).
// Returns an empty DisasterEvent (eventID == -1) if nothing is due.
DisasterEvent AuthManager::fetchPendingDisaster()
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().fetchPendingDisaster();
    }
    DisasterEvent ev;
    if (!con) return ev;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT event_id, type, UNIX_TIMESTAMP(scheduled_at) AS sched_epoch, "
            "duration_sec, status, created_by "
            "FROM disaster_events "
            "WHERE status = 'pending' AND scheduled_at <= UTC_TIMESTAMP() "
            "ORDER BY scheduled_at ASC LIMIT 1"));
        if (!pstmt) return ev;
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            ev.eventID        = res->getInt("event_id");
            ev.type           = std::string(res->getString("type").c_str());
            ev.scheduledAt    = (long long)res->getInt64("sched_epoch");
            ev.durationSeconds= res->getInt("duration_sec");
            ev.status         = std::string(res->getString("status").c_str());
            ev.createdBy      = std::string(res->getString("created_by").c_str());
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Disaster] fetchPendingDisaster error: " << e.what() << "\n";
    }
    return ev;
}

void AuthManager::markDisasterActive(int eventID)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        SQLiteManager::getInstance().markDisasterActive(eventID);
        return;
    }
    if (!con || eventID < 0) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "UPDATE disaster_events SET status='active', activated_at=UTC_TIMESTAMP() "
            "WHERE event_id=?"));
        if (!pstmt) return;
        pstmt->setInt(1, eventID);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Disaster] markDisasterActive error: " << e.what() << "\n";
    }
}

void AuthManager::markDisasterCompleted(int eventID)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        SQLiteManager::getInstance().markDisasterCompleted(eventID);
        return;
    }
    if (!con || eventID < 0) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "UPDATE disaster_events SET status='completed', completed_at=UTC_TIMESTAMP() "
            "WHERE event_id=?"));
        if (!pstmt) return;
        pstmt->setInt(1, eventID);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Disaster] markDisasterCompleted error: " << e.what() << "\n";
    }
}

void AuthManager::cancelDisasterEvent(int eventID)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        SQLiteManager::getInstance().cancelDisasterEvent(eventID);
        return;
    }
    if (!con || eventID < 0) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "UPDATE disaster_events SET status='cancelled' WHERE event_id=?"));
        if (!pstmt) return;
        pstmt->setInt(1, eventID);
        pstmt->executeUpdate();
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Disaster] cancelDisasterEvent error: " << e.what() << "\n";
    }
}

// Returns the last `limit` disaster events (newest first) for the admin panel history view.
std::vector<DisasterEvent> AuthManager::getDisasterHistory(int limit)
{
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode()) {
        return SQLiteManager::getInstance().getDisasterHistory(limit);
    }
    std::vector<DisasterEvent> history;
    if (!con) return history;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT event_id, type, UNIX_TIMESTAMP(scheduled_at) AS sched_epoch, "
            "duration_sec, status, created_by "
            "FROM disaster_events "
            "ORDER BY scheduled_at DESC LIMIT ?"));
        if (!pstmt) return history;
        pstmt->setInt(1, limit);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            DisasterEvent ev;
            ev.eventID         = res->getInt("event_id");
            ev.type            = std::string(res->getString("type").c_str());
            ev.scheduledAt     = (long long)res->getInt64("sched_epoch");
            ev.durationSeconds = res->getInt("duration_sec");
            ev.status          = std::string(res->getString("status").c_str());
            ev.createdBy       = std::string(res->getString("created_by").c_str());
            history.push_back(ev);
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Disaster] getDisasterHistory error: " << e.what() << "\n";
    }
    return history;
}

// --- Death: wipe all player data (online MySQL path) ---
void AuthManager::clearPlayerData(const sql::SQLString& username) {
    // Always clear from local SQLite
    SQLiteManager::getInstance().clearPlayerData(std::string(username.c_str()));

    // Also clear from MySQL if online
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (isOfflineMode() || !con) return;
    try {
        // Delete player-owned rows from each table
        const char* tables[] = { "Inventory", "Structures", "campfires", "chest_contents" };
        for (const char* tbl : tables) {
            std::string delSql = std::string("DELETE FROM ") + tbl + " WHERE Username = ?";
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(delSql));
            if (ps) { ps->setString(1, username); ps->executeUpdate(); }
        }
        // Reset player stats to fresh-start defaults
        std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
            "UPDATE Accounts SET hunger=0, thirst=0, Gold=50, Level=1, Exp=0, elapsedTime=0, "
            "hungerMultiplier=1, thirstMultiplier=1 WHERE Username=?"));
        if (ps) { ps->setString(1, username); ps->executeUpdate(); }

        std::cout << "[DEATH] Cleared MySQL data for player: " << username.c_str() << "\n";
    }
    catch (sql::SQLException& e) {
        std::cerr << "[DEATH] clearPlayerData MySQL error: " << e.what() << "\n";
    }
}