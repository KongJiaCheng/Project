#pragma once
#include <string>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

class AuthManager {
public:
    static AuthManager& getInstance();

    void shutdownDB();
    bool initDB();
    bool registerUser(const sql::SQLString& username, const sql::SQLString& password, const sql::SQLString& email);
    bool loginUser(const sql::SQLString& username, const sql::SQLString& password);
    sql::SQLString generateForgotOTP(const sql::SQLString& email);
    bool resetPassword(const sql::SQLString& email, const sql::SQLString& otp, const sql::SQLString& newPassword);

    bool saveInventory(const sql::SQLString& username, const std::vector<std::pair<int, int>>& inventoryData);
    std::vector<std::pair<int, int>> loadInventory(const sql::SQLString& username);

private:
    AuthManager() : driver(nullptr), con(nullptr) {}

    sql::mysql::MySQL_Driver* driver = nullptr;
    sql::Connection* con = nullptr;
};
