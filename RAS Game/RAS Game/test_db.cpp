#include <iostream>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>

int main() {
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        sql::ConnectOptionsMap connection_properties;
        connection_properties["hostName"] = "tcp://mysql-308a256a-econovadb.h.aivencloud.com:22311";
        connection_properties["userName"] = "avnadmin";
        connection_properties["password"] = "AVNS_YV3eowXWv3Q2tArX8fd";
        connection_properties["sslCa"] = "ca.pem";
        connection_properties["OPT_SSL_MODE"] = sql::SSL_MODE_REQUIRED;

        sql::Connection* con = driver->connect(connection_properties);
        if (!con) {
            std::cout << "Connection failed!" << std::endl;
            return 1;
        }
        std::cout << "Connected!" << std::endl;
        con->setSchema("mmorpg");

        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("INSERT INTO players (username, password, email) VALUES (?, ?, ?)"));
        pstmt->setString(1, "testuser");
        pstmt->setString(2, "Test1234!");
        pstmt->setString(3, "test@test.com");
        pstmt->executeUpdate();

        std::cout << "Insert successful!" << std::endl;
        delete con;
    } catch (sql::SQLException& e) {
        std::cout << "MySQL Error: " << e.what() << " (Code: " << e.getErrorCode() << ")" << std::endl;
    }
    return 0;
}
