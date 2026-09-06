#pragma once
#include <string>
#include <vector>
#include <functional>
#include <SFML/System.hpp>
#include "AuthManager.h"

using TimedGetMaxMessageID = std::function<int()>;
using TimedGetQuestInfo = std::function<QuestInfo(const std::string&)>;
using TimedLoadMarketPrice = std::function<float(int, float)>;

extern TimedGetMaxMessageID timedGetMaxMessageID;
extern TimedGetQuestInfo timedGetQuestInfo;
extern TimedLoadMarketPrice timedLoadMarketPrice;

PlayerStats constructServer(AuthManager& authManager, bool dbStartupEnabled, const std::string& username, float STARTUP_DB_TIMEOUT_SECS);

struct SyncResult {
    std::vector<OnlinePlayer> players;
    bool hasPull = false;
    sf::Vector2f pullPosition;
    std::string pullByUsername;
    int pendingTradeID = -1;
    P2PTradeState pendingTradeState;
    // World sync fields
    long long serverEpoch = 0;
    std::vector<StoredStructure> otherStructures;
    std::vector<StoredCampfire>  otherCampfires;
    bool structuresUpdated = false;
};

#include <map>
#include <future>
#include <SFML/Graphics.hpp>
#include "StructureSystem.h"

void syncAndRenderOnlinePlayers(
    sf::RenderWindow& window,
    AuthManager& authManager,
    bool dbStartupEnabled,
    const std::string& username,
    sf::Sprite& sprite,
    float dt,
    std::future<SyncResult>& syncFuture,
    bool& syncInFlight,
    std::vector<OnlinePlayer>& otherPlayers,
    std::map<std::string, sf::Vector2f>& onlinePlayerTargets,
    std::map<std::string, sf::Vector2f>& onlinePlayerDisplayPos,
    std::map<std::string, sf::FloatRect>& onlinePlayerWrenchBounds,
    std::vector<PlacedStructure>& structures,
    std::vector<PlacedCampfire>& campfires,
    long long& serverEpochCache,
    const sf::Texture& texFemaleOnline,
    const sf::Texture& texMaleOnline,
    const sf::Texture& wrenchTex,
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Font& uiFont,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    int& p2pTradeID,
    bool p2pTradeOpen,
    bool& p2pInvitePopup,
    bool& p2pAmInitiator,
    P2PTradeState& p2pState,
    std::string& p2pPartner,
    std::string& p2pStatusMsg,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc
);

#include "Inventory.h"
#include "Trade.h"

void saveAllGameDataAndClose(
    sf::RenderWindow& window,
    AuthManager& authManager,
    const std::string& username,
    TradeSlot playerTradeSlots[4],
    TradeSlot myP2PSlots[4],
    int p2pTradeID,
    Inventory& inventory,
    float hunger,
    float thirst,
    int currency,
    int level,
    int exp,
    float elapsedGameTime,
    int hungerMultiplier,
    int thirstMultiplier,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
);

void checkAndPerformPeriodicAutoSave(
    const sf::Sprite& sprite,
    sf::Vector2f& lastSaveCameraPos,
    sf::Clock& saveCooldownClock,
    Inventory& inventory,
    AuthManager& authManager,
    const std::string& username,
    float hunger,
    float thirst,
    int currency,
    int level,
    int exp,
    float elapsedGameTime,
    int hungerMultiplier,
    int thirstMultiplier,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
);