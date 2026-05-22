-- Alter table script for Econova database to add roles and moderation columns
USE mmorpg;

-- 1. Add roles column and moderation timestamp columns to Accounts table
ALTER TABLE Accounts ADD COLUMN roles VARCHAR(50) DEFAULT 'user';
ALTER TABLE Accounts ADD COLUMN banned_until DATETIME DEFAULT NULL;
ALTER TABLE Accounts ADD COLUMN muted_until DATETIME DEFAULT NULL;

-- 2. SQL to promote a specific user to moderator
-- Replace 'your_username' with the player's username you want to promote
-- UPDATE Accounts SET roles = 'moderator' WHERE Username = 'your_username';
