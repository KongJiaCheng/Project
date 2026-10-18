-- =============================================================================
-- Econova Local / Offline Database Initialization Script
-- =============================================================================
-- This script creates the complete database and all required tables for
-- running Econova in Offline / Local Database mode.
--
-- How to run:
--   1. Open MySQL Workbench, HeidiSQL, phpMyAdmin, or MySQL CLI.
--   2. Connect to your local MySQL server (e.g. 127.0.0.1:3306).
--   3. Execute this entire script.
--   4. Ensure 'db_config.txt' has mode=local (or switch to Offline Mode in game).
-- =============================================================================

CREATE DATABASE IF NOT EXISTS mmorpg
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE mmorpg;

-- -----------------------------------------------------------------------------
-- 1. Accounts Table
-- Stores user credentials, hashed passwords, survival stats, roles, coordinates.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Accounts (
    Username          VARCHAR(50)   PRIMARY KEY,
    Password          VARCHAR(255)  NOT NULL,
    email             VARCHAR(255)  DEFAULT '',
    reset_token       VARCHAR(10)   DEFAULT '',
    roles             VARCHAR(50)   DEFAULT 'user',
    banned_until      DATETIME      DEFAULT NULL,
    muted_until       DATETIME      DEFAULT NULL,
    hunger            FLOAT         DEFAULT 0,
    thirst            FLOAT         DEFAULT 0,
    Level             INT           DEFAULT 1,
    Exp               INT           DEFAULT 0,
    Gold              INT           DEFAULT 0,
    isFirstTime       BOOLEAN       DEFAULT 1,
    tutorialVersion   INT           DEFAULT 2,
    questID           INT           DEFAULT 0,
    questProgress     INT           DEFAULT 0,
    lastQuestReset    DATE          DEFAULT '2000-01-01',
    questCompleted    BOOLEAN       DEFAULT 0,
    posX              FLOAT         DEFAULT 25000,
    posY              FLOAT         DEFAULT 25000,
    lastActive        TIMESTAMP     DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    pullBy            VARCHAR(50)   DEFAULT NULL,
    elapsedTime       FLOAT         DEFAULT 0,
    gender            INT           DEFAULT -1,
    hungerMultiplier  INT           DEFAULT 1,
    thirstMultiplier  INT           DEFAULT 1
);

-- -----------------------------------------------------------------------------
-- 2. Inventory Table
-- Stores inventory items per player slot.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Inventory (
    ID                INT           AUTO_INCREMENT PRIMARY KEY,
    Username          VARCHAR(50)   NOT NULL,
    ItemName          VARCHAR(50)   NOT NULL,
    Quantity          INT           NOT NULL DEFAULT 1,
    KEY idx_username (Username)
);

-- -----------------------------------------------------------------------------
-- 3. Structures Table
-- Stores placed world structures (walls, doors, beds, furnaces, roofs).
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS structures (
    ID                INT           AUTO_INCREMENT PRIMARY KEY,
    Username          VARCHAR(50),
    ItemID            INT,
    PosX              FLOAT,
    PosY              FLOAT,
    Health            FLOAT,
    Rotation          FLOAT         DEFAULT 0,
    IsOpen            TINYINT(1)    DEFAULT 0,
    ShearY            FLOAT         DEFAULT 0,
    KEY idx_struct_user (Username)
);

-- -----------------------------------------------------------------------------
-- 4. Campfires Table
-- Stores placed and crafted campfires with fuel, input food, output food.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS campfires (
    ID                INT           AUTO_INCREMENT PRIMARY KEY,
    Username          VARCHAR(50),
    PosX              FLOAT,
    PosY              FLOAT,
    TimeRemaining     FLOAT         DEFAULT 0,
    IsCrafted         BOOLEAN       DEFAULT 1,
    Fuel              INT           DEFAULT 0,
    InputItem         INT           DEFAULT 0,
    InputCount        INT           DEFAULT 0,
    OutputItem        INT           DEFAULT 0,
    OutputCount       INT           DEFAULT 0,
    KEY idx_camp_user (Username)
);

-- -----------------------------------------------------------------------------
-- 5. Chest Contents Table
-- Stores items stored in wooden chests placed in the world.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS chest_contents (
    Username          VARCHAR(50),
    ChestX            FLOAT,
    ChestY            FLOAT,
    SlotIndex         INT,
    ItemID            INT,
    Quantity          INT,
    PRIMARY KEY (Username, ChestX, ChestY, SlotIndex)
);

-- -----------------------------------------------------------------------------
-- 6. World Settings Table
-- Stores shared procedural map seed so world terrain remains consistent.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS world_settings (
    id                INT           PRIMARY KEY DEFAULT 1,
    world_seed        INT           NOT NULL DEFAULT 0
);
INSERT IGNORE INTO world_settings (id, world_seed) VALUES (1, FLOOR(RAND() * 2147483647));

-- -----------------------------------------------------------------------------
-- 7. Chat Messages Table
-- Stores global/local player chat logs.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS ChatMessages (
    MessageID         INT           AUTO_INCREMENT PRIMARY KEY,
    Username          VARCHAR(50),
    Message           TEXT,
    SentAt            TIMESTAMP     DEFAULT CURRENT_TIMESTAMP
);

-- -----------------------------------------------------------------------------
-- 8. Trade History Table
-- Logs player marketplace buys and sells.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS TradeHistory (
    ID                INT           AUTO_INCREMENT PRIMARY KEY,
    Username          VARCHAR(50),
    ActionType        VARCHAR(10),
    ItemName          VARCHAR(50),
    Quantity          INT,
    PricePerUnit      INT,
    TotalGold         INT,
    TradeTime         TIMESTAMP     DEFAULT CURRENT_TIMESTAMP
);
INSERT IGNORE INTO TradeHistory (ID, Username, ActionType, ItemName, Quantity, PricePerUnit, TotalGold) VALUES
(1, 'TraderJoe', 'SELL', 'Wood', 15, 5, 75),
(2, 'MerchantAl', 'BUY', 'Stone', 8, 8, 64),
(3, 'AlphaBuilder', 'SELL', 'Sapling', 4, 12, 48),
(4, 'HunterMax', 'BUY', 'Chicken Meat', 6, 15, 90),
(5, 'ChefGourmet', 'SELL', 'Pig Meat', 3, 25, 75);

-- -----------------------------------------------------------------------------
-- 9. Market Prices Table
-- Dynamic commodity prices in the game economy.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS MarketPrices (
    ItemID            INT           PRIMARY KEY,
    Price             FLOAT         NOT NULL
);
INSERT IGNORE INTO MarketPrices (ItemID, Price) VALUES
(1, 5.0),    -- Wood
(2, 8.0),    -- Stone
(3, 12.0),   -- Sapling
(4, 15.0),   -- Chicken Meat
(5, 25.0),   -- Pig Meat
(6, 20.0),   -- Sheep Meat
(7, 15.0),   -- Wool
(8, 10.0),   -- Zombie Flesh
(9, 30.0),   -- Cooked Chicken
(10, 50.0),  -- Cooked Pig
(11, 40.0),  -- Cooked Sheep
(12, 55.0),  -- Wooden Sword
(13, 115.0), -- Sturdy Shield
(14, 84.0),  -- Furnace
(15, 5.0),   -- Wood Tile
(16, 5.0),   -- Wood Wall
(17, 30.0),  -- Wood Door
(18, 30.0),  -- Wood Roof
(19, 48.0),  -- Wood Stairs
(20, 80.0),  -- Bed
(21, 15.0),  -- Coal
(22, 20.0),  -- Copper Ore
(23, 45.0),  -- Copper Bar
(24, 35.0),  -- Iron Ore
(25, 75.0),  -- Iron Bar
(26, 60.0),  -- Gold Ore
(27, 150.0); -- Gold Bar

-- -----------------------------------------------------------------------------
-- 10. Player Trades Table
-- Peer-to-peer (P2P) trading window data.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS PlayerTrades (
    TradeID           INT           AUTO_INCREMENT PRIMARY KEY,
    InitiatorUser     VARCHAR(50)   NOT NULL,
    TargetUser        VARCHAR(50)   NOT NULL,
    Status            VARCHAR(20)   NOT NULL DEFAULT 'pending',
    InitiatorItems    TEXT,
    TargetItems       TEXT,
    InitiatorGold     INT           DEFAULT 0,
    TargetGold        INT           DEFAULT 0,
    InitiatorReady    TINYINT(1)    DEFAULT 0,
    TargetReady       TINYINT(1)    DEFAULT 0,
    CreatedAt         TIMESTAMP     DEFAULT CURRENT_TIMESTAMP
);

-- -----------------------------------------------------------------------------
-- 11. Disaster Events Table
-- Natural disaster scheduling table for admin panel.
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS disaster_events (
    event_id          INT           AUTO_INCREMENT PRIMARY KEY,
    type              VARCHAR(30)   NOT NULL,
    scheduled_at      DATETIME      NOT NULL,
    duration_sec      INT           NOT NULL DEFAULT 600,
    status            VARCHAR(20)   NOT NULL DEFAULT 'pending',
    created_by        VARCHAR(50)   NOT NULL DEFAULT 'system',
    created_at        TIMESTAMP     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    activated_at      DATETIME      DEFAULT NULL,
    completed_at      DATETIME      DEFAULT NULL
);
