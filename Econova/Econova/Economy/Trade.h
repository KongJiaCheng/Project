#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <functional>
#include "AuthManager.h"

struct TradeSlot {
    int id = 0;
    const sf::Texture* tex = nullptr;
    int count = 0;
    int originalSlot = -1;
};

struct P2PItemDisplay {
    int id = 0;
    int count = 0;
};

void drawP2PTrade(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::string& p2pPartner,
    const TradeSlot myP2PSlots[4],
    const std::string& myP2PGoldInput,
    bool p2pGoldTyping,
    bool p2pMyReady,
    const std::vector<P2PItemDisplay>& partnerItems,
    int partnerGold,
    bool partnerReady,
    const std::string& p2pStatusMsg,
    const sf::Color& p2pStatusColor,
    std::function<const sf::Texture*(int id)> getItemTexture,
    std::function<std::string(int id)> getItemName
);

void drawNPCTrade(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const TradeSlot playerTradeSlots[4],
    const TradeSlot npcTradeSlots[4],
    bool isSellingToNPC,
    int npcTradeGold,
    const std::string& npcGoldInput,
    const std::string& playerGoldInput,
    bool playerGoldTyping,
    int currency,
    bool hasBargained,
    bool bargainAccepted,
    const std::string& bargainStatus,
    const sf::Color& bargainColor,
    std::function<int(int id)> getItemPrice
);

class Inventory;
class AuthManager;
struct StockItem;
class TextInput;

std::string evaluateEconomicTradeWarning(
    bool isP2P,
    bool isSelling,
    int playerGoldCost,
    int playerGoldGained,
    int currentCurrency,
    const std::vector<std::pair<int, int>>& playerGivingItems,
    const std::vector<std::pair<int, int>>& playerReceivingItems,
    bool disasterActive,
    const std::string& currentDisasterType,
    const std::vector<StockItem>& stockMarket
);

void executeDirectNPCTrade(
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    bool isSellingToNPC,
    int& npcTradeGold,
    std::string& npcGoldInput,
    int& playerTradeGold,
    std::string& playerGoldInput,
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    int& currency,
    bool& tradeMenuOpen,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    const std::string& username,
    float elapsedGameTime,
    int level,
    int exp,
    int hungerMultiplier,
    int thirstMultiplier,
    float hunger,
    float thirst,
    Inventory& inventory,
    AuthManager& authManager,
    std::function<int(int id)> getItemPrice,
    std::function<std::string(int id)> getItemName,
    std::function<void(int id, float percentChange)> adjustMarketItem,
    bool disasterActive = false,
    const std::string& currentDisasterType = ""
);

bool handleP2PTradeClick(
    sf::Vector2f uiPos,
    sf::Vector2i mousePixelPos,
    unsigned int width,
    unsigned int height,
    int& p2pTradeID,
    const std::string& username,
    std::string& p2pPartner,
    TradeSlot myP2PSlots[4],
    int& myP2PGold,
    std::string& myP2PGoldInput,
    bool& p2pGoldTyping,
    bool& playerGoldTyping,
    bool& p2pMyReady,
    bool& p2pPartnerReady,
    bool& p2pOfferDirty,
    std::string& p2pStatusMsg,
    bool& p2pTradeOpen,
    std::vector<P2PItemDisplay>& partnerItems,
    int& partnerGold,
    Inventory& inventory,
    AuthManager& authManager,
    bool& quantityPromptOpen,
    int& quantityPromptMax,
    int& quantityPromptTargetSlot,
    int& quantityPromptSourceSlot,
    bool& quantityPromptIsP2P,
    TextInput& quantityInput,
    sf::RenderWindow& window,
    bool& tradeConfirmOpen,
    std::string& tradeConfirmMessage,
    bool& tradeConfirmIsP2P,
    int currency,
    bool disasterActive,
    const std::string& currentDisasterType,
    const std::vector<StockItem>& stockMarket
);

bool handleNPCTradeClick(
    sf::Vector2f uiPos,
    sf::Vector2i mousePixelPos,
    unsigned int width,
    unsigned int height,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    bool& isSellingToNPC,
    int& npcTradeGold,
    std::string& npcGoldInput,
    int& playerTradeGold,
    std::string& playerGoldInput,
    bool& playerGoldTyping,
    bool& stockPageOpen,
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    sf::Color& bargainColor,
    int& currency,
    bool& tradeMenuOpen,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    const std::string& username,
    float elapsedGameTime,
    int level,
    int exp,
    int hungerMultiplier,
    int thirstMultiplier,
    float hunger,
    float thirst,
    Inventory& inventory,
    AuthManager& authManager,
    std::vector<StockItem>& stockMarket,
    std::function<int(int id)> getItemPrice,
    std::function<std::string(int id)> getItemName,
    std::function<void(int id, float percentChange)> adjustMarketItem,
    bool& quantityPromptOpen,
    int& quantityPromptMax,
    int& quantityPromptTargetSlot,
    int& quantityPromptSourceSlot,
    bool& quantityPromptIsP2P,
    TextInput& quantityInput,
    sf::RenderWindow& window,
    bool& tradeConfirmOpen,
    std::string& tradeConfirmMessage,
    bool& tradeConfirmIsP2P,
    bool disasterActive,
    const std::string& currentDisasterType
);

bool executeNPCTradeConfirm(
    bool confirmed,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    int& playerTradeGold,
    int& npcTradeGold,
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    sf::Color& bargainColor,
    int& currency,
    const std::string& username,
    float elapsedGameTime,
    int level,
    int exp,
    int hungerMultiplier,
    int thirstMultiplier,
    float hunger,
    float thirst,
    Inventory& inventory,
    AuthManager& authManager,
    std::vector<StockItem>& stockMarket
);

bool handleP2PInvitePopupClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& p2pInvitePopup,
    int& p2pTradeID,
    bool& p2pTradeOpen,
    bool& p2pAmInitiator,
    std::string& p2pPartner,
    const P2PTradeState& p2pState,
    bool& p2pMyReady,
    bool& p2pPartnerReady,
    std::string& p2pStatusMsg,
    TradeSlot myP2PSlots[4],
    int& myP2PGold,
    std::string& myP2PGoldInput,
    std::vector<P2PItemDisplay>& partnerItems,
    int& partnerGold,
    AuthManager& authManager
);

bool handleNPCTradeConfirmClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& tradeConfirmOpen,
    std::string& tradeConfirmMessage,
    bool& tradeConfirmIsP2P,
    bool& tradeMenuOpen,
    bool& tradeBoxesTicked,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    bool& isSellingToNPC,
    int& playerTradeGold,
    std::string& playerGoldInput,
    int& npcTradeGold,
    std::string& npcGoldInput,
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    sf::Color& bargainColor,
    int& currency,
    const std::string& username,
    float elapsedGameTime,
    int level,
    int exp,
    int hungerMultiplier,
    int thirstMultiplier,
    float hunger,
    float thirst,
    Inventory& inventory,
    AuthManager& authManager,
    std::vector<StockItem>& stockMarket,
    std::function<int(int id)> getItemPrice,
    std::function<std::string(int id)> getItemName,
    std::function<void(int id, float percentChange)> adjustMarketItem,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    int p2pTradeID = -1,
    TradeSlot myP2PSlots[4] = nullptr,
    int myP2PGold = 0,
    bool* p2pOfferDirty = nullptr,
    bool* p2pMyReady = nullptr,
    bool p2pPartnerReady = false,
    std::string* p2pStatusMsg = nullptr,
    const std::string& p2pPartner = "",
    bool disasterActive = false,
    const std::string& currentDisasterType = ""
);

void pollAndSyncP2PTrade(
    float dt,
    int& p2pTradeID,
    float& p2pPollTimer,
    float& p2pOfferPushTimer,
    P2PTradeState& p2pState,
    bool p2pAmInitiator,
    int& partnerGold,
    bool& p2pPartnerReady,
    bool& p2pMyReady,
    std::vector<P2PItemDisplay>& partnerItems,
    bool& p2pInvitePopup,
    bool& p2pTradeOpen,
    std::string& p2pStatusMsg,
    Inventory& inventory,
    std::function<const sf::Texture*(int id)> getItemTexture,
    std::function<std::string(int id)> getItemName,
    std::function<int(int id)> getItemPrice,
    std::function<void(int id, float delta)> adjustMarketItem,
    int& currency,
    TradeSlot myP2PSlots[4],
    int& myP2PGold,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    const std::string& username,
    std::string& p2pPartner,
    float hunger,
    float thirst,
    int level,
    int exp,
    float elapsedGameTime,
    int hungerMultiplier,
    int thirstMultiplier,
    bool& p2pOfferDirty,
    AuthManager& authManager,
    bool disasterActive = false,
    const std::string& currentDisasterType = ""
);



