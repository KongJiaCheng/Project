#include "SQLiteManager.h"
#include "AuthManager.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <windows.h>
#include <bcrypt.h>
#include <iomanip>

#pragma comment(lib, "bcrypt.lib")

struct StmtGuard {
    sqlite3_stmt* stmt = nullptr;
    StmtGuard(sqlite3_stmt* s) : stmt(s) {}
    ~StmtGuard() { if (stmt) sqlite3_finalize(stmt); }
    sqlite3_stmt* get() const { return stmt; }
};

// PBKDF2 Verification Helper for SQLite accounts
static bool verifySQLitePassword(const std::string& password, const std::string& stored) {
    if (stored.find("PBKDF2:") != 0) {
        return password == stored;
    }
    auto parts = [&]() {
        std::vector<std::string> v; std::istringstream ss(stored); std::string t;
        while (std::getline(ss, t, ':')) v.push_back(t);
        return v;
    }();
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

    UCHAR salt[16] = {};
    UCHAR storedHash[32] = {};
    if (!fromHex(parts[3], salt, 16)) return false;
    if (!fromHex(parts[4], storedHash, 32)) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        return false;

    UCHAR hash[32] = {};
    NTSTATUS status = BCryptDeriveKeyPBKDF2(
        hAlg,
        (PUCHAR)password.c_str(), (ULONG)password.size(),
        salt, sizeof(salt),
        (ULONGLONG)iters,
        hash, sizeof(hash),
        0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (status != 0) return false;

    int diff = 0;
    for (int i = 0; i < 32; i++) diff |= (hash[i] ^ storedHash[i]);
    return diff == 0;
}

SQLiteManager& SQLiteManager::getInstance() {
    static SQLiteManager instance;
    return instance;
}

bool SQLiteManager::init(const std::string& dbPath) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    close();

    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "[SQLITE ERROR] Cannot open database: " << sqlite3_errmsg(db) << "\n";
        close();
        return false;
    }

    createTables();
    std::cout << "[SQLITE] Connected to local database: " << dbPath << "\n";
    return true;
}

void SQLiteManager::close() {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool SQLiteManager::isOpen() const {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    return db != nullptr;
}

void SQLiteManager::createTables() {
    if (!db) return;

    const char* schema =
        "CREATE TABLE IF NOT EXISTS Accounts ("
        "Username TEXT PRIMARY KEY COLLATE NOCASE, "
        "Password TEXT NOT NULL, "
        "email TEXT, "
        "roles TEXT DEFAULT 'user', "
        "reset_token TEXT DEFAULT '', "
        "banned_until TEXT DEFAULT NULL, "
        "muted_until TEXT DEFAULT NULL, "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"

        "CREATE TABLE IF NOT EXISTS PlayerStats ("
        "Username TEXT PRIMARY KEY COLLATE NOCASE, "
        "hunger REAL DEFAULT 100.0, "
        "thirst REAL DEFAULT 100.0, "
        "currency INTEGER DEFAULT 50, "
        "level INTEGER DEFAULT 1, "
        "exp INTEGER DEFAULT 0, "
        "elapsedTime REAL DEFAULT 0.0, "
        "gender INTEGER DEFAULT -1, "
        "isFirstTime INTEGER DEFAULT 1, "
        "hungerMultiplier INTEGER DEFAULT 1, "
        "thirstMultiplier INTEGER DEFAULT 1"
        ");"

        "CREATE TABLE IF NOT EXISTS Inventory ("
        "Username TEXT COLLATE NOCASE, "
        "slot INTEGER, "
        "itemId INTEGER, "
        "count INTEGER, "
        "PRIMARY KEY (Username, slot)"
        ");"

        "CREATE TABLE IF NOT EXISTS Structures ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "Username TEXT COLLATE NOCASE, "
        "structureId INTEGER, "
        "posX REAL, "
        "posY REAL, "
        "rotation REAL DEFAULT 0.0, "
        "isOpen INTEGER DEFAULT 0, "
        "shearY REAL DEFAULT 0.0"
        ");"

        "CREATE TABLE IF NOT EXISTS chest_contents ("
        "Username TEXT COLLATE NOCASE, "
        "ChestX REAL, "
        "ChestY REAL, "
        "SlotIndex INTEGER, "
        "ItemID INTEGER, "
        "Quantity INTEGER, "
        "PRIMARY KEY (Username, ChestX, ChestY, SlotIndex)"
        ");"

        "CREATE TABLE IF NOT EXISTS campfires ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "Username TEXT COLLATE NOCASE, "
        "PosX REAL, "
        "PosY REAL, "
        "TimeRemaining REAL DEFAULT 0.0, "
        "IsCrafted INTEGER DEFAULT 1, "
        "Fuel INTEGER DEFAULT 0, "
        "InputItem INTEGER DEFAULT 0, "
        "InputCount INTEGER DEFAULT 0, "
        "OutputItem INTEGER DEFAULT 0, "
        "OutputCount INTEGER DEFAULT 0, "
        "Owner TEXT DEFAULT ''"
        ");"

        "CREATE TABLE IF NOT EXISTS Quests ("
        "Username TEXT PRIMARY KEY COLLATE NOCASE, "
        "questID INTEGER DEFAULT 0, "
        "progress INTEGER DEFAULT 0, "
        "lastReset TEXT DEFAULT '', "
        "isCompleted INTEGER DEFAULT 0"
        ");"

        "CREATE TABLE IF NOT EXISTS MarketStocks ("
        "itemId INTEGER PRIMARY KEY, "
        "currentPrice REAL NOT NULL, "
        "demand REAL DEFAULT 100.0, "
        "supply REAL DEFAULT 100.0, "
        "quantity REAL DEFAULT 100.0"
        ");"

        "CREATE TABLE IF NOT EXISTS scheduled_disasters ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "disaster_type TEXT NOT NULL, "
        "scheduled_time INTEGER NOT NULL, "
        "duration_seconds INTEGER NOT NULL DEFAULT 600, "
        "status TEXT NOT NULL DEFAULT 'PENDING', "
        "created_by TEXT DEFAULT ''"
        ");"

                "CREATE TABLE IF NOT EXISTS ban_records ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT COLLATE NOCASE, "
        "banned_by TEXT, "
        "ban_start TEXT DEFAULT CURRENT_TIMESTAMP, "
        "ban_end TEXT, "
        "reason TEXT, "
        "is_active INTEGER DEFAULT 1"
        ");"

        "CREATE TABLE IF NOT EXISTS mute_records ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT COLLATE NOCASE, "
        "muted_by TEXT, "
        "mute_start TEXT DEFAULT CURRENT_TIMESTAMP, "
        "mute_end TEXT, "
        "reason TEXT, "
        "is_active INTEGER DEFAULT 1"
        ");"

        "CREATE TABLE IF NOT EXISTS WorldSeed ("
        "id INTEGER PRIMARY KEY CHECK (id = 1), "
        "seed INTEGER NOT NULL"
        ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, schema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[SQLITE SCHEMA ERROR] " << (errMsg ? errMsg : "Unknown error") << "\n";
        if (errMsg) sqlite3_free(errMsg);
    }

    sqlite3_exec(db, "ALTER TABLE Accounts ADD COLUMN ban_reason TEXT DEFAULT NULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "ALTER TABLE Accounts ADD COLUMN banned_by TEXT DEFAULT NULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "ALTER TABLE Accounts ADD COLUMN ban_start TEXT DEFAULT NULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "ALTER TABLE Accounts ADD COLUMN mute_reason TEXT DEFAULT NULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "ALTER TABLE Accounts ADD COLUMN muted_by TEXT DEFAULT NULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "ALTER TABLE Accounts ADD COLUMN mute_start TEXT DEFAULT NULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "UPDATE Accounts SET ban_reason = NULL, banned_by = NULL, ban_start = NULL, banned_until = NULL WHERE banned_until IS NULL OR strftime('%s', banned_until) <= strftime('%s', 'now');", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "UPDATE Accounts SET mute_reason = NULL, muted_by = NULL, mute_start = NULL, muted_until = NULL WHERE muted_until IS NULL OR strftime('%s', muted_until) <= strftime('%s', 'now');", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "ALTER TABLE scheduled_disasters ADD COLUMN activated_at INTEGER DEFAULT NULL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "ALTER TABLE scheduled_disasters ADD COLUMN end_time INTEGER DEFAULT NULL;", nullptr, nullptr, nullptr);
}

bool SQLiteManager::registerUser(const std::string& username, const std::string& hashedPassword, const std::string& email, const std::string& role) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return false;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "INSERT INTO Accounts (Username, Password, email, roles) VALUES (?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, hashedPassword.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 3, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 4, role.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool SQLiteManager::loginUser(const std::string& username, const std::string& plainPassword, std::string& outRole, std::string& errMessage) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) {
        errMessage = "Database is not open.";
        return false;
    }

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT Password, roles, banned_until, ban_reason, "
                      "CASE WHEN banned_until IS NOT NULL AND (banned_until LIKE '9999%' OR strftime('%s', banned_until) > strftime('%s', 'now')) THEN 1 ELSE 0 END AS is_banned, "
                      "CASE WHEN banned_until LIKE '9999%' THEN 315360000 ELSE (strftime('%s', banned_until) - strftime('%s', 'now')) END AS rem_sec "
                      "FROM Accounts WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) {
        errMessage = "Query preparation failed.";
        return false;
    }
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        std::string storedHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        const unsigned char* roleText = sqlite3_column_text(stmt.get(), 1);
        outRole = roleText ? reinterpret_cast<const char*>(roleText) : "user";

        if (!verifySQLitePassword(plainPassword, storedHash)) {
            errMessage = "Invalid Credentials!";
            return false;
        }

        int isBanned = sqlite3_column_int(stmt.get(), 4);
        long long remSec = sqlite3_column_int64(stmt.get(), 5);
        if (isBanned == 1 && remSec > 0) {
            const unsigned char* bUntil = sqlite3_column_text(stmt.get(), 2);
            std::string bStr = bUntil ? reinterpret_cast<const char*>(bUntil) : "";
            if (bStr.find("9999") == 0) {
                errMessage = "You are permanently banned!";
            } else {
                errMessage = "You are banned until: " + bStr;
            }
            return false;
        }
        return true;
    }

    errMessage = "Invalid Credentials!";
    return false;
}

std::string SQLiteManager::getUserRole(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return "user";

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT roles FROM Accounts WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return "user";
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(stmt.get(), 0);
        return t ? reinterpret_cast<const char*>(t) : "user";
    }
    return "user";
}

bool SQLiteManager::setUserRole(const std::string& username, const std::string& roles) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return false;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "UPDATE Accounts SET roles = ? WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, roles.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, username.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool SQLiteManager::resetPassword(const std::string& email, const std::string& newHashedPassword) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return false;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "UPDATE Accounts SET Password = ?, reset_token = '' WHERE email = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, newHashedPassword.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, email.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

void SQLiteManager::savePlayerStats(const std::string& username, float hunger, float thirst, int currency, int level, int exp, float elapsedTime, int hungerMultiplier, int thirstMultiplier) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "INSERT INTO PlayerStats (Username, hunger, thirst, currency, level, exp, elapsedTime, hungerMultiplier, thirstMultiplier) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(Username) DO UPDATE SET "
        "hunger = excluded.hunger, thirst = excluded.thirst, currency = excluded.currency, "
        "level = excluded.level, exp = excluded.exp, elapsedTime = excluded.elapsedTime, "
        "hungerMultiplier = excluded.hungerMultiplier, thirstMultiplier = excluded.thirstMultiplier";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt.get(), 2, (double)hunger);
    sqlite3_bind_double(stmt.get(), 3, (double)thirst);
    sqlite3_bind_int(stmt.get(), 4, currency);
    sqlite3_bind_int(stmt.get(), 5, level);
    sqlite3_bind_int(stmt.get(), 6, exp);
    sqlite3_bind_double(stmt.get(), 7, (double)elapsedTime);
    sqlite3_bind_int(stmt.get(), 8, hungerMultiplier);
    sqlite3_bind_int(stmt.get(), 9, thirstMultiplier);

    sqlite3_step(stmt.get());
}

PlayerStats SQLiteManager::loadPlayerStats(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    PlayerStats stats = { 100.f, 100.f, 50, 1, 0, 0.f, -1, true, 1, 1 };
    if (!db) return stats;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT hunger, thirst, currency, level, exp, elapsedTime, gender, isFirstTime, hungerMultiplier, thirstMultiplier FROM PlayerStats WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return stats;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        stats.hunger = (float)sqlite3_column_double(stmt.get(), 0);
        stats.thirst = (float)sqlite3_column_double(stmt.get(), 1);
        stats.currency = sqlite3_column_int(stmt.get(), 2);
        stats.level = sqlite3_column_int(stmt.get(), 3);
        stats.exp = sqlite3_column_int(stmt.get(), 4);
        stats.elapsedTime = (float)sqlite3_column_double(stmt.get(), 5);
        stats.gender = sqlite3_column_int(stmt.get(), 6);
        stats.isFirstTime = sqlite3_column_int(stmt.get(), 7) != 0;
        stats.hungerMultiplier = sqlite3_column_int(stmt.get(), 8);
        stats.thirstMultiplier = sqlite3_column_int(stmt.get(), 9);
    }
    return stats;
}

void SQLiteManager::setFirstTimeFinished(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "UPDATE PlayerStats SET isFirstTime = 0 WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt.get());
}

void SQLiteManager::saveGender(const std::string& username, int gender) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "INSERT INTO PlayerStats (Username, gender) VALUES (?, ?) "
        "ON CONFLICT(Username) DO UPDATE SET gender = excluded.gender";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, gender);
    sqlite3_step(stmt.get());
}

bool SQLiteManager::saveInventory(const std::string& username, const std::vector<std::pair<int, int>>& inventoryData) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return false;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    {
        sqlite3_stmt* del = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM Inventory WHERE Username = ?", -1, &del, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
    }

    sqlite3_stmt* rawIns = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO Inventory (Username, slot, itemId, count) VALUES (?, ?, ?, ?)", -1, &rawIns, nullptr) == SQLITE_OK) {
        StmtGuard ins(rawIns);
        for (size_t i = 0; i < inventoryData.size(); ++i) {
            if (inventoryData[i].first != 0 && inventoryData[i].second > 0) {
                sqlite3_reset(ins.get());
                sqlite3_bind_text(ins.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(ins.get(), 2, (int)i);
                sqlite3_bind_int(ins.get(), 3, inventoryData[i].first);
                sqlite3_bind_int(ins.get(), 4, inventoryData[i].second);
                sqlite3_step(ins.get());
            }
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

std::vector<std::pair<int, int>> SQLiteManager::loadInventory(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<std::pair<int, int>> inv(20, { 0, 0 });
    if (!db) return inv;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT slot, itemId, count FROM Inventory WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return inv;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int slot = sqlite3_column_int(stmt.get(), 0);
        int item = sqlite3_column_int(stmt.get(), 1);
        int cnt  = sqlite3_column_int(stmt.get(), 2);
        if (slot >= 0 && slot < (int)inv.size()) {
            inv[slot] = { item, cnt };
        }
    }
    return inv;
}

void SQLiteManager::saveStructures(const std::string& username, const std::vector<StoredStructure>& structures) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    {
        sqlite3_stmt* del = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM Structures WHERE Username = ?", -1, &del, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
    }

    sqlite3_stmt* rawIns = nullptr;
    const char* sql = "INSERT INTO Structures (Username, structureId, posX, posY, rotation, isOpen, shearY) VALUES (?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &rawIns, nullptr) == SQLITE_OK) {
        StmtGuard ins(rawIns);
        for (const auto& s : structures) {
            sqlite3_reset(ins.get());
            sqlite3_bind_text(ins.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(ins.get(), 2, s.id);
            sqlite3_bind_double(ins.get(), 3, (double)s.x);
            sqlite3_bind_double(ins.get(), 4, (double)s.y);
            sqlite3_bind_double(ins.get(), 5, (double)s.rotation);
            sqlite3_bind_int(ins.get(), 6, s.isOpen ? 1 : 0);
            sqlite3_bind_double(ins.get(), 7, (double)s.shearY);
            sqlite3_step(ins.get());
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}

std::vector<StoredStructure> SQLiteManager::loadStructures(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredStructure> res;
    if (!db) return res;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT Username, structureId, posX, posY, rotation, isOpen, shearY FROM Structures WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return res;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        StoredStructure s;
        const unsigned char* u = sqlite3_column_text(stmt.get(), 0);
        s.owner = u ? reinterpret_cast<const char*>(u) : username;
        s.id = sqlite3_column_int(stmt.get(), 1);
        s.x = (float)sqlite3_column_double(stmt.get(), 2);
        s.y = (float)sqlite3_column_double(stmt.get(), 3);
        s.health = 100.f;
        s.rotation = (float)sqlite3_column_double(stmt.get(), 4);
        s.isOpen = sqlite3_column_int(stmt.get(), 5) != 0;
        s.shearY = (float)sqlite3_column_double(stmt.get(), 6);
        res.push_back(s);
    }
    return res;
}

std::vector<StoredStructure> SQLiteManager::loadOtherStructures(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredStructure> res;
    if (!db) return res;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT Username, structureId, posX, posY, rotation, isOpen, shearY FROM Structures WHERE Username != ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return res;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        StoredStructure s;
        const unsigned char* u = sqlite3_column_text(stmt.get(), 0);
        s.owner = u ? reinterpret_cast<const char*>(u) : "other";
        s.id = sqlite3_column_int(stmt.get(), 1);
        s.x = (float)sqlite3_column_double(stmt.get(), 2);
        s.y = (float)sqlite3_column_double(stmt.get(), 3);
        s.health = 100.f;
        s.rotation = (float)sqlite3_column_double(stmt.get(), 4);
        s.isOpen = sqlite3_column_int(stmt.get(), 5) != 0;
        s.shearY = (float)sqlite3_column_double(stmt.get(), 6);
        res.push_back(s);
    }
    return res;
}

void SQLiteManager::saveCampfires(const std::string& username, const std::vector<StoredCampfire>& campfires) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    {
        sqlite3_stmt* del = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM campfires WHERE Username = ?", -1, &del, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
    }

    sqlite3_stmt* rawIns = nullptr;
    const char* sql = "INSERT INTO campfires (Username, PosX, PosY, TimeRemaining, IsCrafted, Fuel, InputItem, InputCount, OutputItem, OutputCount, Owner) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &rawIns, nullptr) == SQLITE_OK) {
        StmtGuard ins(rawIns);
        for (const auto& c : campfires) {
            sqlite3_reset(ins.get());
            sqlite3_bind_text(ins.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(ins.get(), 2, (double)c.x);
            sqlite3_bind_double(ins.get(), 3, (double)c.y);
            sqlite3_bind_double(ins.get(), 4, (double)c.timeRemaining);
            sqlite3_bind_int(ins.get(), 5, c.isCrafted ? 1 : 0);
            sqlite3_bind_int(ins.get(), 6, c.fuel);
            sqlite3_bind_int(ins.get(), 7, c.inputItem);
            sqlite3_bind_int(ins.get(), 8, c.inputCount);
            sqlite3_bind_int(ins.get(), 9, c.outputItem);
            sqlite3_bind_int(ins.get(), 10, c.outputCount);
            sqlite3_bind_text(ins.get(), 11, c.owner.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(ins.get());
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}

std::vector<StoredCampfire> SQLiteManager::loadCampfires(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredCampfire> campfires;
    if (!db) return campfires;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT PosX, PosY, TimeRemaining, IsCrafted, Fuel, InputItem, InputCount, OutputItem, OutputCount, Owner FROM campfires WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return campfires;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        StoredCampfire sc;
        sc.x = (float)sqlite3_column_double(stmt.get(), 0);
        sc.y = (float)sqlite3_column_double(stmt.get(), 1);
        sc.timeRemaining = (float)sqlite3_column_double(stmt.get(), 2);
        sc.isCrafted = sqlite3_column_int(stmt.get(), 3) != 0;
        sc.fuel = sqlite3_column_int(stmt.get(), 4);
        sc.inputItem = sqlite3_column_int(stmt.get(), 5);
        sc.inputCount = sqlite3_column_int(stmt.get(), 6);
        sc.outputItem = sqlite3_column_int(stmt.get(), 7);
        sc.outputCount = sqlite3_column_int(stmt.get(), 8);
        const unsigned char* o = sqlite3_column_text(stmt.get(), 9);
        sc.owner = o ? reinterpret_cast<const char*>(o) : "";
        campfires.push_back(sc);
    }
    return campfires;
}

std::vector<StoredCampfire> SQLiteManager::loadOtherCampfires(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredCampfire> campfires;
    if (!db) return campfires;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT PosX, PosY, TimeRemaining, IsCrafted, Fuel, InputItem, InputCount, OutputItem, OutputCount, Owner FROM campfires WHERE Username != ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return campfires;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        StoredCampfire sc;
        sc.x = (float)sqlite3_column_double(stmt.get(), 0);
        sc.y = (float)sqlite3_column_double(stmt.get(), 1);
        sc.timeRemaining = (float)sqlite3_column_double(stmt.get(), 2);
        sc.isCrafted = sqlite3_column_int(stmt.get(), 3) != 0;
        sc.fuel = sqlite3_column_int(stmt.get(), 4);
        sc.inputItem = sqlite3_column_int(stmt.get(), 5);
        sc.inputCount = sqlite3_column_int(stmt.get(), 6);
        sc.outputItem = sqlite3_column_int(stmt.get(), 7);
        sc.outputCount = sqlite3_column_int(stmt.get(), 8);
        const unsigned char* o = sqlite3_column_text(stmt.get(), 9);
        sc.owner = o ? reinterpret_cast<const char*>(o) : "";
        campfires.push_back(sc);
    }
    return campfires;
}

bool SQLiteManager::updateCampfireCookingState(float x, float y, int fuel, int inputItem, int inputCount, int outputItem, int outputCount, float timeRemaining) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return false;
    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "UPDATE campfires SET Fuel = ?, InputItem = ?, InputCount = ?, OutputItem = ?, OutputCount = ?, TimeRemaining = ? "
                      "WHERE ABS(PosX - ?) < 5.0 AND ABS(PosY - ?) < 5.0";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
    StmtGuard stmt(rawStmt);
    sqlite3_bind_int(stmt.get(), 1, fuel);
    sqlite3_bind_int(stmt.get(), 2, inputItem);
    sqlite3_bind_int(stmt.get(), 3, inputCount);
    sqlite3_bind_int(stmt.get(), 4, outputItem);
    sqlite3_bind_int(stmt.get(), 5, outputCount);
    sqlite3_bind_double(stmt.get(), 6, (double)timeRemaining);
    sqlite3_bind_double(stmt.get(), 7, (double)x);
    sqlite3_bind_double(stmt.get(), 8, (double)y);
    int rc = sqlite3_step(stmt.get());
    return rc == SQLITE_DONE;
}

void SQLiteManager::saveChestContents(const std::string& username, const std::vector<StoredChestItem>& items) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    {
        sqlite3_stmt* del = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM chest_contents WHERE Username = ?", -1, &del, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
    }

    sqlite3_stmt* rawIns = nullptr;
    const char* sql = "INSERT INTO chest_contents (Username, ChestX, ChestY, SlotIndex, ItemID, Quantity) VALUES (?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &rawIns, nullptr) == SQLITE_OK) {
        StmtGuard ins(rawIns);
        for (const auto& item : items) {
            sqlite3_reset(ins.get());
            sqlite3_bind_text(ins.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(ins.get(), 2, (double)item.chestX);
            sqlite3_bind_double(ins.get(), 3, (double)item.chestY);
            sqlite3_bind_int(ins.get(), 4, item.slotIndex);
            sqlite3_bind_int(ins.get(), 5, item.itemID);
            sqlite3_bind_int(ins.get(), 6, item.quantity);
            sqlite3_step(ins.get());
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}

std::vector<StoredChestItem> SQLiteManager::loadChestContents(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredChestItem> items;
    if (!db) return items;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT ChestX, ChestY, SlotIndex, ItemID, Quantity FROM chest_contents WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return items;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        items.push_back({
            (float)sqlite3_column_double(stmt.get(), 0),
            (float)sqlite3_column_double(stmt.get(), 1),
            sqlite3_column_int(stmt.get(), 2),
            sqlite3_column_int(stmt.get(), 3),
            sqlite3_column_int(stmt.get(), 4)
        });
    }
    return items;
}

std::vector<StoredChestItem> SQLiteManager::loadOtherChestContents(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<StoredChestItem> items;
    if (!db) return items;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT ChestX, ChestY, SlotIndex, ItemID, Quantity FROM chest_contents WHERE Username != ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return items;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        items.push_back({
            (float)sqlite3_column_double(stmt.get(), 0),
            (float)sqlite3_column_double(stmt.get(), 1),
            sqlite3_column_int(stmt.get(), 2),
            sqlite3_column_int(stmt.get(), 3),
            sqlite3_column_int(stmt.get(), 4)
        });
    }
    return items;
}

QuestInfo SQLiteManager::getQuestInfo(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    QuestInfo q = { 0, 0, "", false };
    if (!db) return q;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT questID, progress, lastReset, isCompleted FROM Quests WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return q;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        q.questID = sqlite3_column_int(stmt.get(), 0);
        q.progress = sqlite3_column_int(stmt.get(), 1);
        const unsigned char* d = sqlite3_column_text(stmt.get(), 2);
        q.lastReset = d ? reinterpret_cast<const char*>(d) : "";
        q.isCompleted = sqlite3_column_int(stmt.get(), 3) != 0;
    }
    return q;
}

void SQLiteManager::updateQuestProgress(const std::string& username, int progress, bool completed) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "INSERT INTO Quests (Username, progress, isCompleted) VALUES (?, ?, ?) "
        "ON CONFLICT(Username) DO UPDATE SET progress = excluded.progress, isCompleted = excluded.isCompleted";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, progress);
    sqlite3_bind_int(stmt.get(), 3, completed ? 1 : 0);
    sqlite3_step(stmt.get());
}

void SQLiteManager::resetQuest(const std::string& username, int questID, const std::string& date) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "INSERT INTO Quests (Username, questID, progress, lastReset, isCompleted) VALUES (?, ?, 0, ?, 0) "
        "ON CONFLICT(Username) DO UPDATE SET questID = excluded.questID, progress = 0, lastReset = excluded.lastReset, isCompleted = 0";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, questID);
    sqlite3_bind_text(stmt.get(), 3, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt.get());
}

void SQLiteManager::saveMarketPrice(int itemID, float price) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "INSERT INTO MarketStocks (itemId, currentPrice, demand, supply, quantity) VALUES (?, ?, 100.0, 100.0, 100.0) "
        "ON CONFLICT(itemId) DO UPDATE SET currentPrice = excluded.currentPrice";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_int(stmt.get(), 1, itemID);
    sqlite3_bind_double(stmt.get(), 2, (double)price);
    sqlite3_step(stmt.get());
}

float SQLiteManager::loadMarketPrice(int itemID) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return 0.f;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT currentPrice FROM MarketStocks WHERE itemId = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return 0.f;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_int(stmt.get(), 1, itemID);
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return (float)sqlite3_column_double(stmt.get(), 0);
    }
    return 0.f;
}

void SQLiteManager::saveAllMarketPrices(const std::vector<std::pair<int, float>>& prices) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "INSERT INTO MarketStocks (itemId, currentPrice, demand, supply, quantity) VALUES (?, ?, 100.0, 100.0, 100.0) "
        "ON CONFLICT(itemId) DO UPDATE SET currentPrice = excluded.currentPrice";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) == SQLITE_OK) {
        StmtGuard stmt(rawStmt);
        for (const auto& p : prices) {
            sqlite3_reset(stmt.get());
            sqlite3_bind_int(stmt.get(), 1, p.first);
            sqlite3_bind_double(stmt.get(), 2, (double)p.second);
            sqlite3_step(stmt.get());
        }
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}

std::map<int, float> SQLiteManager::loadAllMarketPrices() {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::map<int, float> res;
    if (!db) return res;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT itemId, currentPrice FROM MarketStocks";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return res;
    StmtGuard stmt(rawStmt);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt.get(), 0);
        float p = (float)sqlite3_column_double(stmt.get(), 1);
        res[id] = p;
    }
    return res;
}

int SQLiteManager::getOrCreateWorldSeed() {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return 42;

    sqlite3_stmt* rawStmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT seed FROM WorldSeed WHERE id = 1", -1, &rawStmt, nullptr) == SQLITE_OK) {
        StmtGuard stmt(rawStmt);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            return sqlite3_column_int(stmt.get(), 0);
        }
    }

    int newSeed = 1000 + (rand() % 9000);
    sqlite3_stmt* ins = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO WorldSeed (id, seed) VALUES (1, ?)", -1, &ins, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(ins, 1, newSeed);
        sqlite3_step(ins);
        sqlite3_finalize(ins);
    }
    return newSeed;
}

int SQLiteManager::scheduleDisaster(const std::string& type, long long scheduledAtEpoch, int durationSeconds, const std::string& createdBy) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return -1;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "INSERT INTO scheduled_disasters (disaster_type, scheduled_time, duration_seconds, status, created_by) VALUES (?, ?, ?, 'PENDING', ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return -1;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_text(stmt.get(), 1, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 2, scheduledAtEpoch);
    sqlite3_bind_int(stmt.get(), 3, durationSeconds);
    sqlite3_bind_text(stmt.get(), 4, createdBy.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
        return (int)sqlite3_last_insert_rowid(db);
    }
    return -1;
}

DisasterEvent SQLiteManager::fetchPendingDisaster() {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    DisasterEvent ev;
    if (!db) return ev;

    long long now = (long long)std::time(nullptr);
    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT id, disaster_type, scheduled_time, duration_seconds, created_by FROM scheduled_disasters WHERE status = 'PENDING' AND scheduled_time <= ? ORDER BY scheduled_time ASC LIMIT 1";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return ev;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_int64(stmt.get(), 1, now);
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        ev.eventID = sqlite3_column_int(stmt.get(), 0);
        const unsigned char* t = sqlite3_column_text(stmt.get(), 1);
        ev.type = t ? reinterpret_cast<const char*>(t) : "";
        ev.scheduledAt = sqlite3_column_int64(stmt.get(), 2);
        ev.durationSeconds = sqlite3_column_int(stmt.get(), 3);
        ev.status = "pending";
        const unsigned char* c = sqlite3_column_text(stmt.get(), 4);
        ev.createdBy = c ? reinterpret_cast<const char*>(c) : "";
    }
    return ev;
}

void SQLiteManager::markDisasterActive(int eventID) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;
    sqlite3_stmt* rawStmt = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE scheduled_disasters SET status = 'ACTIVE' WHERE id = ?", -1, &rawStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(rawStmt, 1, eventID);
        sqlite3_step(rawStmt);
        sqlite3_finalize(rawStmt);
    }
}

void SQLiteManager::markDisasterCompleted(int eventID) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;
    sqlite3_stmt* rawStmt = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE scheduled_disasters SET status = 'COMPLETED' WHERE id = ?", -1, &rawStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(rawStmt, 1, eventID);
        sqlite3_step(rawStmt);
        sqlite3_finalize(rawStmt);
    }
}

void SQLiteManager::cancelDisasterEvent(int eventID) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;
    sqlite3_stmt* rawStmt = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE scheduled_disasters SET status = 'CANCELLED' WHERE id = ?", -1, &rawStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(rawStmt, 1, eventID);
        sqlite3_step(rawStmt);
        sqlite3_finalize(rawStmt);
    }
}

std::vector<DisasterEvent> SQLiteManager::getDisasterHistory(int limit) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    std::vector<DisasterEvent> list;
    if (!db) return list;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "SELECT id, disaster_type, scheduled_time, duration_seconds, status, created_by FROM scheduled_disasters ORDER BY id DESC LIMIT ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return list;
    StmtGuard stmt(rawStmt);

    sqlite3_bind_int(stmt.get(), 1, limit);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        DisasterEvent ev;
        ev.eventID = sqlite3_column_int(stmt.get(), 0);
        const unsigned char* t = sqlite3_column_text(stmt.get(), 1);
        ev.type = t ? reinterpret_cast<const char*>(t) : "";
        ev.scheduledAt = sqlite3_column_int64(stmt.get(), 2);
        ev.durationSeconds = sqlite3_column_int(stmt.get(), 3);
        const unsigned char* s = sqlite3_column_text(stmt.get(), 4);
        ev.status = s ? reinterpret_cast<const char*>(s) : "";
        const unsigned char* c = sqlite3_column_text(stmt.get(), 5);
        ev.createdBy = c ? reinterpret_cast<const char*>(c) : "";
        list.push_back(ev);
    }
    return list;
}

// --- Death: wipe all player data and reset stats to defaults ---
void SQLiteManager::clearPlayerData(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return;

    auto deleteWhere = [&](const char* sql) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    };

    deleteWhere("DELETE FROM Inventory       WHERE Username = ?");
    deleteWhere("DELETE FROM Structures      WHERE Username = ?");
    deleteWhere("DELETE FROM campfires       WHERE Username = ?");
    deleteWhere("DELETE FROM chest_contents  WHERE Username = ?");

    const char* resetSql =
        "INSERT INTO PlayerStats "
        "(Username, hunger, thirst, currency, level, exp, elapsedTime, hungerMultiplier, thirstMultiplier) "
        "VALUES (?, 0.0, 0.0, 50, 1, 0, 0.0, 1, 1) "
        "ON CONFLICT(Username) DO UPDATE SET "
        "hunger=0.0, thirst=0.0, currency=50, level=1, exp=0, "
        "elapsedTime=0.0, hungerMultiplier=1, thirstMultiplier=1";
    sqlite3_stmt* rst = nullptr;
    if (sqlite3_prepare_v2(db, resetSql, -1, &rst, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(rst, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(rst);
        sqlite3_finalize(rst);
    }
    std::cout << "[DEATH] Cleared all data for player: " << username << "\n";
}

// =========================================================
// BAN, MUTE & DISASTER IMPLEMENTATIONS
// =========================================================

BanInfo SQLiteManager::getBanInfo(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    BanInfo info;
    if (!db) return info;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "SELECT (CASE WHEN banned_until IS NOT NULL AND (banned_until LIKE '9999%' OR strftime('%s', banned_until) > strftime('%s', 'now')) THEN 1 ELSE 0 END) AS is_banned, "
        "(CASE WHEN banned_until LIKE '9999%' THEN 315360000 ELSE (strftime('%s', banned_until) - strftime('%s', 'now')) END) AS rem_sec, "
        "banned_until, ban_reason, banned_by "
        "FROM Accounts WHERE Username = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return info;
    StmtGuard stmt(rawStmt);
    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        if (sqlite3_column_int(stmt.get(), 0) == 1) {
            info.isBanned = true;
            info.remainingSeconds = sqlite3_column_int64(stmt.get(), 1);
            const unsigned char* bd = sqlite3_column_text(stmt.get(), 2);
            info.banDate = bd ? reinterpret_cast<const char*>(bd) : "";
            const unsigned char* br = sqlite3_column_text(stmt.get(), 3);
            info.reason = br ? reinterpret_cast<const char*>(br) : "";
            const unsigned char* bb = sqlite3_column_text(stmt.get(), 4);
            info.bannedBy = bb ? reinterpret_cast<const char*>(bb) : "";
            info.isPermanent = (info.banDate.find("9999") == 0);
        } else {
            info.isBanned = false;
            info.reason = "";
            info.banDate = "";
            info.bannedBy = "";
            info.remainingSeconds = 0;
            sqlite3_stmt* psClean = nullptr;
            if (sqlite3_prepare_v2(db, "UPDATE Accounts SET banned_until = NULL, ban_reason = NULL, banned_by = NULL, ban_start = NULL WHERE Username = ? AND (banned_until IS NULL OR strftime('%s', banned_until) <= strftime('%s', 'now'))", -1, &psClean, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(psClean, 1, username.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(psClean);
                sqlite3_finalize(psClean);
            }
        }
    }
    return info;
}

MuteInfo SQLiteManager::getMuteStatus(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    MuteInfo info;
    if (!db) return info;

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "SELECT (CASE WHEN muted_until IS NOT NULL AND strftime('%s', muted_until) > strftime('%s', 'now') THEN 1 ELSE 0 END) AS is_muted, "
        "(strftime('%s', muted_until) - strftime('%s', 'now')) AS rem_sec, "
        "muted_until, mute_reason, muted_by "
        "FROM Accounts WHERE Username = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return info;
    StmtGuard stmt(rawStmt);
    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        if (sqlite3_column_int(stmt.get(), 0) == 1) {
            info.isMuted = true;
            info.remainingSeconds = sqlite3_column_int64(stmt.get(), 1);
            const unsigned char* md = sqlite3_column_text(stmt.get(), 2);
            info.muteDate = md ? reinterpret_cast<const char*>(md) : "";
            const unsigned char* mr = sqlite3_column_text(stmt.get(), 3);
            info.reason = mr ? reinterpret_cast<const char*>(mr) : "";
            const unsigned char* mb = sqlite3_column_text(stmt.get(), 4);
            info.mutedBy = mb ? reinterpret_cast<const char*>(mb) : "";
        } else {
            info.isMuted = false;
            info.reason = "";
            info.muteDate = "";
            info.mutedBy = "";
            info.remainingSeconds = 0;
            sqlite3_stmt* psClean = nullptr;
            if (sqlite3_prepare_v2(db, "UPDATE Accounts SET muted_until = NULL, mute_reason = NULL, muted_by = NULL, mute_start = NULL WHERE Username = ? AND (muted_until IS NULL OR strftime('%s', muted_until) <= strftime('%s', 'now'))", -1, &psClean, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(psClean, 1, username.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(psClean);
                sqlite3_finalize(psClean);
            }
        }
    }
    return info;
}

ActiveDisasterInfo SQLiteManager::getActiveDisaster() {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    ActiveDisasterInfo info;
    if (!db) return info;

    long long now = (long long)std::time(nullptr);
    // 1. Auto-complete expired active disasters
    {
        sqlite3_stmt* stmtExp = nullptr;
        if (sqlite3_prepare_v2(db, "UPDATE scheduled_disasters SET status = 'COMPLETED' WHERE status = 'ACTIVE' AND end_time IS NOT NULL AND end_time <= ?", -1, &stmtExp, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmtExp, 1, now);
            sqlite3_step(stmtExp);
            sqlite3_finalize(stmtExp);
        }
    }

    // 2. Query authoritative active disaster
    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "SELECT id, disaster_type, duration_seconds, (end_time - ?), created_by "
        "FROM scheduled_disasters "
        "WHERE status = 'ACTIVE' AND end_time > ? "
        "ORDER BY id DESC LIMIT 1";

    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) == SQLITE_OK) {
        StmtGuard stmt(rawStmt);
        sqlite3_bind_int64(stmt.get(), 1, now);
        sqlite3_bind_int64(stmt.get(), 2, now);
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            long long rem = sqlite3_column_int64(stmt.get(), 3);
            if (rem > 0) {
                info.active = true;
                info.eventID = sqlite3_column_int(stmt.get(), 0);
                const unsigned char* t = sqlite3_column_text(stmt.get(), 1);
                info.type = t ? reinterpret_cast<const char*>(t) : "";
                info.totalDuration = sqlite3_column_int(stmt.get(), 2);
                info.remainingSeconds = static_cast<float>(rem);
                const unsigned char* c = sqlite3_column_text(stmt.get(), 4);
                info.createdBy = c ? reinterpret_cast<const char*>(c) : "";
            }
        }
    }
    return info;
}

bool SQLiteManager::triggerDisaster(const std::string& type, const std::string& triggeredBy, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) {
        outMsg = "Database error.";
        return false;
    }

    std::string lowerType = type;
    for (char& c : lowerType) c = (char)::tolower((unsigned char)c);
    int durationSec = 600;
    if (lowerType == "diarrhea") {
        durationSec = 600;
    } else if (lowerType == "drought") {
        durationSec = 180;
    } else {
        outMsg = "Unknown disaster: '" + type + "'. Available: diarrhea, drought, stop.";
        return false;
    }

    ActiveDisasterInfo current = getActiveDisaster();
    if (current.active && current.remainingSeconds > 0.f) {
        outMsg = "Cannot trigger disaster: An active disaster ('" + current.type + "') is already in progress with " +
                 std::to_string((int)current.remainingSeconds) + "s remaining. Use /disaster stop first.";
        return false;
    }

    long long now = (long long)std::time(nullptr);
    long long endTime = now + durationSec;
    sqlite3_stmt* rawStmt = nullptr;
    const char* sql =
        "INSERT INTO scheduled_disasters (disaster_type, scheduled_time, duration_seconds, status, created_by, activated_at, end_time) "
        "VALUES (?, ?, ?, 'ACTIVE', ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) {
        outMsg = "Failed to prepare disaster statement.";
        return false;
    }
    StmtGuard stmt(rawStmt);
    sqlite3_bind_text(stmt.get(), 1, lowerType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 2, now);
    sqlite3_bind_int(stmt.get(), 3, durationSec);
    sqlite3_bind_text(stmt.get(), 4, triggeredBy.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 5, now);
    sqlite3_bind_int64(stmt.get(), 6, endTime);

    if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
        std::string durStr = (durationSec == 600) ? "10m (600s)" : "3m (180s)";
        outMsg = "[STAFF] " + ((lowerType == "drought") ? std::string("Severe Drought") : std::string("Acute Diarrhea")) +
                 " disaster activated across all players! Duration: " + durStr + ".";
        return true;
    }
    outMsg = "Database insert error.";
    return false;
}

bool SQLiteManager::stopActiveDisaster(const std::string& triggeredBy, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) {
        outMsg = "Database error.";
        return false;
    }

    ActiveDisasterInfo current = getActiveDisaster();
    if (!current.active) {
        outMsg = "No natural disaster is currently active.";
        return false;
    }

    sqlite3_stmt* rawStmt = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE scheduled_disasters SET status = 'COMPLETED' WHERE status = 'ACTIVE'", -1, &rawStmt, nullptr) == SQLITE_OK) {
        StmtGuard stmt(rawStmt);
        sqlite3_step(stmt.get());
    }
    outMsg = "[STAFF] Natural Disaster stopped manually across all players. Environment and prices restored.";
    return true;
}

bool SQLiteManager::banUser(const std::string& target, const std::string& durationStr, std::string& outMsg, const std::string& reason, const std::string& adminUser) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) {
        outMsg = "Database not open.";
        return false;
    }

    // Check user exists
    {
        sqlite3_stmt* chk = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT Username FROM Accounts WHERE Username = ?", -1, &chk, nullptr) != SQLITE_OK) return false;
        StmtGuard stmt(chk);
        sqlite3_bind_text(stmt.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
            outMsg = "Error: User '" + target + "' does not exist.";
            return false;
        }
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

    bool hasReason = !reason.empty();
    std::string finalAdmin = adminUser.empty() ? "admin" : adminUser;

    if (isPerm) {
        sqlite3_stmt* rawStmt = nullptr;
        const char* sql = "UPDATE Accounts SET banned_until = '9999-12-31 23:59:59', ban_reason = ?, banned_by = ?, ban_start = datetime('now') WHERE Username = ?";
        if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
        StmtGuard stmt(rawStmt);
        if (hasReason) sqlite3_bind_text(stmt.get(), 1, reason.c_str(), -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(stmt.get(), 1);
        sqlite3_bind_text(stmt.get(), 2, finalAdmin.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, target.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt.get());

        sqlite3_stmt* pRec = nullptr;
        if (sqlite3_prepare_v2(db, "INSERT INTO ban_records (username, banned_by, ban_start, ban_end, reason, is_active) VALUES (?, ?, datetime('now'), '9999-12-31 23:59:59', ?, 1)", -1, &pRec, nullptr) == SQLITE_OK) {
            StmtGuard sRec(pRec);
            sqlite3_bind_text(sRec.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(sRec.get(), 2, finalAdmin.c_str(), -1, SQLITE_TRANSIENT);
            if (hasReason) sqlite3_bind_text(sRec.get(), 3, reason.c_str(), -1, SQLITE_TRANSIENT);
            else sqlite3_bind_null(sRec.get(), 3);
            sqlite3_step(sRec.get());
        }

        outMsg = "Successfully permanently banned " + target + (hasReason ? (". Reason: " + reason) : " (No reason).");
    } else {
        sqlite3_stmt* rawStmt = nullptr;
        std::string durExpr = "+" + std::to_string(days) + " days";
        const char* sql = "UPDATE Accounts SET banned_until = datetime('now', ?), ban_reason = ?, banned_by = ?, ban_start = datetime('now') WHERE Username = ?";
        if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
        StmtGuard stmt(rawStmt);
        sqlite3_bind_text(stmt.get(), 1, durExpr.c_str(), -1, SQLITE_TRANSIENT);
        if (hasReason) sqlite3_bind_text(stmt.get(), 2, reason.c_str(), -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(stmt.get(), 2);
        sqlite3_bind_text(stmt.get(), 3, finalAdmin.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 4, target.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt.get());

        sqlite3_stmt* pRec = nullptr;
        if (sqlite3_prepare_v2(db, "INSERT INTO ban_records (username, banned_by, ban_start, ban_end, reason, is_active) VALUES (?, ?, datetime('now'), datetime('now', ?), ?, 1)", -1, &pRec, nullptr) == SQLITE_OK) {
            StmtGuard sRec(pRec);
            sqlite3_bind_text(sRec.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(sRec.get(), 2, finalAdmin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(sRec.get(), 3, durExpr.c_str(), -1, SQLITE_TRANSIENT);
            if (hasReason) sqlite3_bind_text(sRec.get(), 4, reason.c_str(), -1, SQLITE_TRANSIENT);
            else sqlite3_bind_null(sRec.get(), 4);
            sqlite3_step(sRec.get());
        }

        outMsg = "Successfully banned " + target + " for " + durationStr + " days" + (hasReason ? (". Reason: " + reason) : ".");
    }
    return true;
}

bool SQLiteManager::unbanUser(const std::string& target, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return false;

    // Check user exists
    {
        sqlite3_stmt* chk = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT Username FROM Accounts WHERE Username = ?", -1, &chk, nullptr) != SQLITE_OK) return false;
        StmtGuard stmt(chk);
        sqlite3_bind_text(stmt.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
            outMsg = "Error: User '" + target + "' does not exist.";
            return false;
        }
    }

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "UPDATE Accounts SET banned_until = NULL, ban_reason = NULL, banned_by = NULL, ban_start = NULL WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
    StmtGuard stmt(rawStmt);
    sqlite3_bind_text(stmt.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt.get());

    sqlite3_stmt* pRec = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE ban_records SET is_active = 0 WHERE username = ?", -1, &pRec, nullptr) == SQLITE_OK) {
        StmtGuard sRec(pRec);
        sqlite3_bind_text(sRec.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(sRec.get());
    }

    outMsg = "Successfully unbanned " + target + ".";
    return true;
}

bool SQLiteManager::muteUser(const std::string& target, int minutes, std::string& outMsg, const std::string& reason, const std::string& adminUser) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return false;

    // Check user exists
    {
        sqlite3_stmt* chk = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT Username FROM Accounts WHERE Username = ?", -1, &chk, nullptr) != SQLITE_OK) return false;
        StmtGuard stmt(chk);
        sqlite3_bind_text(stmt.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
            outMsg = "Error: User '" + target + "' does not exist.";
            return false;
        }
    }

    bool hasReason = !reason.empty();
    std::string finalAdmin = adminUser.empty() ? "admin" : adminUser;
    std::string durExpr = "+" + std::to_string(minutes) + " minutes";

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "UPDATE Accounts SET muted_until = datetime('now', ?), mute_reason = ?, muted_by = ?, mute_start = datetime('now') WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
    StmtGuard stmt(rawStmt);
    sqlite3_bind_text(stmt.get(), 1, durExpr.c_str(), -1, SQLITE_TRANSIENT);
    if (hasReason) sqlite3_bind_text(stmt.get(), 2, reason.c_str(), -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt.get(), 2);
    sqlite3_bind_text(stmt.get(), 3, finalAdmin.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 4, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt.get());

    sqlite3_stmt* pRec = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO mute_records (username, muted_by, mute_start, mute_end, reason, is_active) VALUES (?, ?, datetime('now'), datetime('now', ?), ?, 1)", -1, &pRec, nullptr) == SQLITE_OK) {
        StmtGuard sRec(pRec);
        sqlite3_bind_text(sRec.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(sRec.get(), 2, finalAdmin.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(sRec.get(), 3, durExpr.c_str(), -1, SQLITE_TRANSIENT);
        if (hasReason) sqlite3_bind_text(sRec.get(), 4, reason.c_str(), -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(sRec.get(), 4);
        sqlite3_step(sRec.get());
    }

    outMsg = "Successfully muted " + target + " for " + std::to_string(minutes) + " minutes" + (hasReason ? (". Reason: " + reason) : ".");
    return true;
}

bool SQLiteManager::unmuteUser(const std::string& target, std::string& outMsg) {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (!db) return false;

    // Check user exists
    {
        sqlite3_stmt* chk = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT Username FROM Accounts WHERE Username = ?", -1, &chk, nullptr) != SQLITE_OK) return false;
        StmtGuard stmt(chk);
        sqlite3_bind_text(stmt.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
            outMsg = "Error: User '" + target + "' does not exist.";
            return false;
        }
    }

    sqlite3_stmt* rawStmt = nullptr;
    const char* sql = "UPDATE Accounts SET muted_until = NULL, mute_reason = NULL, muted_by = NULL, mute_start = NULL WHERE Username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &rawStmt, nullptr) != SQLITE_OK) return false;
    StmtGuard stmt(rawStmt);
    sqlite3_bind_text(stmt.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt.get());

    sqlite3_stmt* pRec = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE mute_records SET is_active = 0 WHERE username = ?", -1, &pRec, nullptr) == SQLITE_OK) {
        StmtGuard sRec(pRec);
        sqlite3_bind_text(sRec.get(), 1, target.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(sRec.get());
    }

    outMsg = "Successfully unmuted " + target + ".";
    return true;
}
