#include "AuthManager.h"
#include <iostream>
#include <cppconn/exception.h>
#include <random>
#include <sstream>
#include <memory>

#include <vector>

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
        std::cout << "[DEBUG] Connecting to TCP socket..." << std::endl;
        con = driver->connect("tcp://127.0.0.1:3306", "root", "Ras5664@");
        std::cout << "[DEBUG] connect() finished." << std::endl;
        if (!con) {
            std::cout << "[DEBUG] ERROR: Connection returned null." << std::endl;
            return false;
        }

        std::cout << "[DEBUG] Setting schema to game_db..." << std::endl;
        con->setSchema("game_db");
        std::cout << "[DEBUG] Schema set successfully." << std::endl;

        // Ensure players table has necessary columns (ignoring duplicate column errors)
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                try { stmt->execute("ALTER TABLE players ADD COLUMN password VARCHAR(255)"); } catch (...) {}
            }
        } catch (...) {}

        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                try { stmt->execute("ALTER TABLE players ADD COLUMN email VARCHAR(255)"); } catch (...) {}
            }
        } catch (...) {}
        
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                try { stmt->execute("ALTER TABLE players ADD COLUMN reset_token VARCHAR(10)"); } catch (...) {}
            }
        } catch (...) {}

        // Create inventory table
        try {
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            if (stmt) {
                stmt->execute("CREATE TABLE IF NOT EXISTS inventory (username VARCHAR(255), slot INT, item_id INT, count INT, PRIMARY KEY(username, slot))");
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
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM players WHERE username = ?"));
        pstmtCheck->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (res && res->next()) {
            return false; // User already exists
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("INSERT INTO players (username, password, email) VALUES (?, ?, ?)"));
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
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT * FROM players WHERE username = ? AND password = ?"));
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
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM players WHERE email = ?"));
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

        std::unique_ptr<sql::PreparedStatement> pstmtUpdate(con->prepareStatement("UPDATE players SET reset_token = ? WHERE email = ?"));
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
        std::unique_ptr<sql::PreparedStatement> pstmtCheck(con->prepareStatement("SELECT * FROM players WHERE email = ? AND reset_token = ?"));
        pstmtCheck->setString(1, email);
        pstmtCheck->setString(2, otp);
        std::unique_ptr<sql::ResultSet> res(pstmtCheck->executeQuery());
        if (!res || !res->next()) {
            return false; // Invalid OTP or email
        }

        std::unique_ptr<sql::PreparedStatement> pstmtUpdate(con->prepareStatement("UPDATE players SET password = ?, reset_token = '' WHERE email = ?"));
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
        std::unique_ptr<sql::PreparedStatement> delStmt(con->prepareStatement("DELETE FROM inventory WHERE username = ?"));
        delStmt->setString(1, username);
        delStmt->executeUpdate();

        // Insert new slots
        std::unique_ptr<sql::PreparedStatement> insStmt(con->prepareStatement("INSERT INTO inventory (username, slot, item_id, count) VALUES (?, ?, ?, ?)"));
        for (int slot = 0; slot < (int)inventoryData.size(); ++slot) {
            int item_id = inventoryData[slot].first;
            int count = inventoryData[slot].second;
            
            if (item_id != 0 && count > 0) {
                insStmt->setString(1, username);
                insStmt->setInt(2, slot);
                insStmt->setInt(3, item_id);
                insStmt->setInt(4, count);
                insStmt->executeUpdate();
            }
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during saveInventory: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::pair<int, int>> AuthManager::loadInventory(const sql::SQLString& username) {
    std::vector<std::pair<int, int>> inventoryData(10, {0, 0}); // Default 10 empty slots
    if (!con) return inventoryData;
    
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT slot, item_id, count FROM inventory WHERE username = ?"));
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res && res->next()) {
            int slot = res->getInt("slot");
            int item_id = res->getInt("item_id");
            int count = res->getInt("count");
            
            if (slot >= 0 && slot < 10) {
                inventoryData[slot] = {item_id, count};
            }
        }
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error during loadInventory: " << e.what() << std::endl;
    }
    return inventoryData;
}
