-- SQL Update statement to promote a user to moderator
-- Replace 'your_username' with the actual username you want to promote

USE mmorpg;

UPDATE Accounts 
SET roles = 'moderator' 
WHERE Username = 'your_username';
