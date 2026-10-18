-- =============================================================================
-- Econova - Offline SQLite Database Schema
-- File: econova_offline_schema.sql
-- Database: econova_offline.db
-- 
-- HOW IT WORKS:
-- SQLite is a serverless, zero-configuration database engine embedded directly
-- inside the Econova executable. Unlike MySQL:
-- 1. No server installation (like MySQL / MariaDB / XAMPP) is required on any PC.
-- 2. When the game starts in Offline Mode, it automatically creates and opens
--    'econova_offline.db' in the game directory if it does not already exist.
-- 3. If you want to create, inspect, or modify the database yourself, you can run
--    this script using the SQLite CLI:
--       sqlite3 econova_offline.db < econova_offline_schema.sql
--    or using GUI tools like DB Browser for SQLite (https://sqlitebrowser.org/).
-- =============================================================================

PRAGMA foreign_keys = ON;

-- -----------------------------------------------------------------------------
-- 1. Accounts Table (User credentials, roles, and status)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Accounts (
    Username TEXT PRIMARY KEY COLLATE NOCASE,
    Password TEXT NOT NULL,                         -- PBKDF2:SHA256:100000:<salt>:<hash>
    email TEXT,
    roles TEXT DEFAULT 'user',                      -- 'user' | 'admin' | 'moderator'
    reset_token TEXT DEFAULT '',
    banned_until TEXT DEFAULT NULL,
    muted_until TEXT DEFAULT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- -----------------------------------------------------------------------------
-- 2. PlayerStats Table (Health, survival, currency, level, time)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS PlayerStats (
    Username TEXT PRIMARY KEY COLLATE NOCASE,
    hunger REAL DEFAULT 100.0,
    thirst REAL DEFAULT 100.0,
    currency INTEGER DEFAULT 50,
    level INTEGER DEFAULT 1,
    exp INTEGER DEFAULT 0,
    elapsedTime REAL DEFAULT 0.0,
    gender INTEGER DEFAULT -1,                      -- -1: unset, 1: male, 2: female
    isFirstTime INTEGER DEFAULT 1,                  -- 1: tutorial active, 0: finished
    hungerMultiplier INTEGER DEFAULT 1,
    thirstMultiplier INTEGER DEFAULT 1,
    FOREIGN KEY(Username) REFERENCES Accounts(Username) ON DELETE CASCADE
);

-- -----------------------------------------------------------------------------
-- 3. Inventory Table (20 inventory slots per player)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Inventory (
    Username TEXT COLLATE NOCASE,
    slot INTEGER,
    itemId INTEGER,
    count INTEGER,
    PRIMARY KEY (Username, slot),
    FOREIGN KEY(Username) REFERENCES Accounts(Username) ON DELETE CASCADE
);

-- -----------------------------------------------------------------------------
-- 4. Structures Table (Placed world walls, floors, doors, beds, chests, furnaces)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Structures (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    Username TEXT COLLATE NOCASE,
    structureId INTEGER,
    posX REAL,
    posY REAL,
    health REAL DEFAULT 100.0,
    rotation REAL DEFAULT 0.0,
    isOpen INTEGER DEFAULT 0,
    shearY REAL DEFAULT 0.0
);

-- -----------------------------------------------------------------------------
-- 5. Chest Contents Table (Items stored inside placed chests)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS chest_contents (
    Username TEXT COLLATE NOCASE,
    ChestX REAL,
    ChestY REAL,
    SlotIndex INTEGER,
    ItemID INTEGER,
    Quantity INTEGER,
    PRIMARY KEY (Username, ChestX, ChestY, SlotIndex)
);

-- -----------------------------------------------------------------------------
-- 6. Campfires Table (Cooking campfire positions, fuel, and cooking timers)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS campfires (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    Username TEXT COLLATE NOCASE,
    PosX REAL,
    PosY REAL,
    TimeRemaining REAL DEFAULT 0.0,
    IsCrafted INTEGER DEFAULT 1,
    Fuel INTEGER DEFAULT 0,
    InputItem INTEGER DEFAULT 0,
    InputCount INTEGER DEFAULT 0,
    OutputItem INTEGER DEFAULT 0,
    OutputCount INTEGER DEFAULT 0,
    Owner TEXT DEFAULT ''
);

-- -----------------------------------------------------------------------------
-- 7. Quests Table (Daily / quest progression tracking)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS Quests (
    Username TEXT PRIMARY KEY COLLATE NOCASE,
    questID INTEGER DEFAULT 0,
    progress INTEGER DEFAULT 0,
    lastReset TEXT DEFAULT '',
    isCompleted INTEGER DEFAULT 0,
    FOREIGN KEY(Username) REFERENCES Accounts(Username) ON DELETE CASCADE
);

-- -----------------------------------------------------------------------------
-- 8. MarketStocks Table (Commodity prices, supply, demand)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS MarketStocks (
    itemId INTEGER PRIMARY KEY,
    currentPrice REAL NOT NULL,
    demand REAL DEFAULT 100.0,
    supply REAL DEFAULT 100.0,
    quantity REAL DEFAULT 100.0
);

-- Default market prices for key resources
INSERT OR IGNORE INTO MarketStocks (itemId, currentPrice, demand, supply, quantity) VALUES
    (3,   5.0,  100.0, 100.0, 100.0),   -- Wood
    (2,   8.0,  100.0, 100.0, 100.0),   -- Stone
    (201, 15.0, 100.0, 100.0, 100.0),   -- Coal
    (203, 25.0, 100.0, 100.0, 100.0),   -- Copper Bar
    (205, 45.0, 100.0, 100.0, 100.0),   -- Iron Bar
    (207, 90.0, 100.0, 100.0, 100.0);   -- Gold Bar

-- -----------------------------------------------------------------------------
-- 9. Scheduled Disasters Table (Admin simulator events)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS scheduled_disasters (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    disaster_type TEXT NOT NULL,
    scheduled_time INTEGER NOT NULL,            -- Unix epoch seconds
    duration_seconds INTEGER NOT NULL DEFAULT 600,
    status TEXT NOT NULL DEFAULT 'PENDING',     -- 'PENDING' | 'ACTIVE' | 'COMPLETED' | 'CANCELLED'
    created_by TEXT DEFAULT ''
);

-- -----------------------------------------------------------------------------
-- 10. WorldSeed Table (Deterministic map generation seed)
-- -----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS WorldSeed (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    world_seed INTEGER NOT NULL DEFAULT 12345
);

INSERT OR IGNORE INTO WorldSeed (id, world_seed) VALUES (1, 12345);
