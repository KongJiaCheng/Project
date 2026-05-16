CREATE DATABASE mmorpg;

USE mmorpg;

CREATE TABLE Accounts (
    ID INT AUTO_INCREMENT PRIMARY KEY,
    Username VARCHAR(50) UNIQUE NOT NULL,
    Password VARCHAR(255) NOT NULL,
    Level INT DEFAULT 1,
    Gold INT DEFAULT 0,
    PosX FLOAT DEFAULT 0,
    PosY FLOAT DEFAULT 0,
    CreatedAt TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE Inventory (
    InventoryID INT AUTO_INCREMENT PRIMARY KEY,
    Username VARCHAR(50),
    ItemName VARCHAR(100),
    Quantity INT DEFAULT 1
);

CREATE TABLE ChatMessages (
    MessageID INT AUTO_INCREMENT PRIMARY KEY,
    Username VARCHAR(50),
    Message TEXT,
    SentAt TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS structures (
    ID INT AUTO_INCREMENT PRIMARY KEY,
    Username VARCHAR(50),
    ItemID INT,
    PosX FLOAT,
    PosY FLOAT,
    Health FLOAT
);


CREATE TABLE IF NOT EXISTS campfires (
    ID INT AUTO_INCREMENT PRIMARY KEY,
    Username VARCHAR(50),
    PosX FLOAT,
    PosY FLOAT,
    TimeRemaining FLOAT,
    IsCrafted BOOLEAN
);


SELECT * FROM Accounts;

UPDATE Accounts
SET Password = 'KJC123@'
WHERE Username = 'Kong Jia Cheng';

UPDATE Accounts
SET Hunger = 0, Thirst = 0
WHERE Username = 'Kong Jia Cheng';

DELETE FROM Accounts
WHERE Username = 'Kong Jia Cheng';