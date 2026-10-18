#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <optional>
#include "Inventory.h"
#include "Settings.h"

class AuthManager;

struct TutorialStep {
    std::string title;
    std::string description;
    std::string goal;
};

struct HUDVitalsUI {
    sf::Sprite hpIcon;
    sf::Sprite hungerIcon;
    sf::Sprite thirstIcon;
    sf::Text hpText;
    sf::Text hungerText;
    sf::Text thirstText;
    sf::Text hungerMultiplierText;
    sf::Text thirstMultiplierText;
    sf::Text defText;
    sf::Text levelText;
    sf::Text drinkText;
    sf::RectangleShape hpBack;
    sf::RectangleShape hpBar;
    sf::RectangleShape hungerBack;
    sf::RectangleShape hungerBar;
    sf::RectangleShape thirstBack;
    sf::RectangleShape thirstBar;
    sf::RectangleShape defBack;
    sf::RectangleShape defBar;
    sf::RectangleShape expBack;
    sf::RectangleShape expBar;

    HUDVitalsUI(const sf::Font& font, const sf::Texture& hpIconTex, const sf::Texture& hungerIconTex, const sf::Texture& thirstIconTex, float BAR_WIDTH = 400.f, float BAR_HEIGHT = 22.f);

    void update(
        sf::RenderWindow& window,
        Inventory& inventory,
        bool shieldEquipped,
        int eqShieldSlot,
        float hp, float maxHP,
        float hunger, float maxHunger, int hungerMultiplier,
        float thirst, float maxThirst, int thirstMultiplier,
        int level, int exp
    );

    void draw(
        sf::RenderWindow& window,
        bool inventoryOpen,
        bool shieldEquipped,
        Inventory& inventory
    ) const;
};


void drawHUDVitals(
    sf::RenderWindow& window,
    bool inventoryOpen,
    const sf::RectangleShape& hpBack, const sf::RectangleShape& hpBar, const sf::Sprite& hpIcon, const sf::Text& hpText,
    const sf::RectangleShape& hungerBack, const sf::RectangleShape& hungerBar, const sf::Sprite& hungerIcon, const sf::Text& hungerText,
    const sf::RectangleShape& thirstBack, const sf::RectangleShape& thirstBar, const sf::Sprite& thirstIcon, const sf::Text& thirstText,
    const sf::Text& hungerMultiplierText, const sf::Text& thirstMultiplierText,
    const sf::RectangleShape& expBack, const sf::RectangleShape& expBar, const sf::Text& levelText,
    bool shieldEquipped, const sf::RectangleShape& defBack, const sf::RectangleShape& defBar, const sf::Text& defText,
    Inventory& inventory
);

void updateHUDVitalsLayoutAndTexts(
    sf::RenderWindow& window,
    Inventory& inventory,
    bool shieldEquipped,
    int eqShieldSlot,
    float hp, float maxHP,
    float hunger, float maxHunger, int hungerMultiplier,
    float thirst, float maxThirst, int thirstMultiplier,
    int level, int exp,
    sf::RectangleShape& hpBack, sf::RectangleShape& hpBar, sf::Sprite& hpIcon, sf::Text& hpText,
    sf::RectangleShape& hungerBack, sf::RectangleShape& hungerBar, sf::Sprite& hungerIcon, sf::Text& hungerText,
    sf::RectangleShape& thirstBack, sf::RectangleShape& thirstBar, sf::Sprite& thirstIcon, sf::Text& thirstText,
    sf::Text& hungerMultiplierText, sf::Text& thirstMultiplierText,
    sf::RectangleShape& expBack, sf::RectangleShape& expBar, sf::Text& levelText,
    sf::RectangleShape& defBack, sf::RectangleShape& defBar, sf::Text& defText
);

void drawTopRightButtons(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    bool isAdmin = false
);

void drawGoldCurrency(
    sf::RenderWindow& window,
    const sf::Font& font,
    int currency,
    bool goldTexLoaded,
    sf::Sprite& goldSprite
);

void drawDeathScreen(
    sf::RenderWindow& window,
    const sf::Font& font,
    unsigned int width,
    unsigned int height,
    float elapsedGameTime,
    float timeOffset,
    float totalCycleTime
);

void drawBrightnessOverlay(
    sf::RenderWindow& window,
    unsigned int width,
    unsigned int height,
    float brightness
);

void drawIntroStory(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::optional<sf::Sprite>& introSprite,
    size_t currentIntroFrame,
    const std::vector<std::string>& introTexts
);

void drawTutorial(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    const sf::RectangleShape& tutorialOverlay,
    const sf::RectangleShape& tutorialBox,
    size_t tutorialStep,
    const std::vector<TutorialStep>& tutorialSteps,
    float tutMoveDist,
    bool tutSprinted,
    bool tutInvOpened,
    int tutWoodGathered,
    int tutStoneGathered,
    bool tutAttacked,
    bool tutChatted
);

void drawSpawnOverlay(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    int spawnOverlayDone,
    int spawnOverlayTotal
);

void drawSleepOverlay(
    sf::RenderWindow& window,
    unsigned int width,
    unsigned int height,
    float sleepFadeTimer,
    float sleepFadeDuration
);

void drawDisasterNotification(
    sf::RenderWindow& window,
    const sf::Font& font,
    unsigned int width,
    bool disasterActive,
    float disasterRemaining,
    float bannerTimer,
    const std::string& bannerMessage,
    const std::string& currentDisasterType = "diarrhea"
);

bool handleIntroAndTutorialClicks(
    sf::Vector2f uiPos,
    bool selectionDone,
    bool& introActive,
    int& currentIntroFrame,
    const std::vector<sf::Texture>& introFrames,
    std::optional<sf::Sprite>& introSprite,
    bool& tutorialActive,
    int& tutorialStep,
    const std::vector<TutorialStep>& tutorialSteps,
    const sf::RectangleShape& tutorialBox,
    float tutMoveDist,
    bool tutSprinted,
    bool tutInvOpened,
    int tutWoodGathered,
    int tutStoneGathered,
    bool tutAttacked,
    bool tutChatted,
    AuthManager& authManager,
    const std::string& username,
    std::string& latestPlayerMsg,
    float& bubbleTimer
);

#include "Crafting.h"
#include "Trade.h"
#include "MarketStats.h"
#include "IAPStore.h"
#include "ChatSystem.h"
#include "StructureSystem.h"
#include "AuthManager.h"
#include <future>

void renderAllGameUIs(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    const sf::Font& font,
    unsigned int width,
    unsigned int height,
    const GameSettings& globalSettings,
    bool wrenchMenuOpen,
    sf::Vector2f wrenchMenuPos,
    bool isTargetNPC,
    const std::string& wrenchTargetPlayer,
    bool p2pInvitePopup,
    int p2pTradeID,
    const P2PTradeState& p2pState,
    bool p2pTradeOpen,
    const std::string& p2pPartner,
    TradeSlot myP2PSlots[4],
    std::string& myP2PGoldInput,
    bool p2pGoldTyping,
    bool p2pMyReady,
    const std::vector<P2PItemDisplay>& partnerItems,
    int partnerGold,
    bool p2pPartnerReady,
    const std::string& p2pStatusMsg,
    bool craftMenuOpen,
    const std::vector<CraftableItem>& craftableItems,
    Inventory& inventory,
    const sf::Texture& woodTex,
    const sf::Texture& stoneTex,
    const sf::Texture& woolTex,
    const sf::Texture& saplingTex,
    const sf::Texture& cookedChickenTex,
    const sf::Texture& cookedPigTex,
    const sf::Texture& cookedSheepTex,
    const sf::Texture& woodenSwordTex,
    const sf::Texture& shieldTex,
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    bool swordCrafting,
    bool shieldCrafting,
    bool placementMode,
    bool saplingPlacementMode,
    float saplingRotation,
    bool tradeMenuOpen,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    bool isSellingToNPC,
    int npcTradeGold,
    std::string& npcGoldInput,
    std::string& playerGoldInput,
    bool playerGoldTyping,
    int currency,
    bool hasBargained,
    bool bargainAccepted,
    const std::string& bargainStatus,
    sf::Color bargainColor,
    bool tradeConfirmOpen,
    const std::string& tradeConfirmMessage,
    bool stockPageOpen,
    bool dbStartupEnabled,
    bool& recentTradesFetchInFlight,
    sf::Clock& recentTradesClock,
    std::future<std::vector<TradeRecord>>& recentTradesFuture,
    std::vector<TradeRecord>& cachedRecentTrades,
    const std::vector<StockItem>& stockMarket,
    bool marketSearchActive,
    const std::string& marketSearchText,
    sf::Clock& marketCursorBlink,
    int marketScrollOffset,
    bool iapPageOpen,
    bool iapVerifyingOpen,
    const std::string& iapVerifyingTier,
    int iapVerifyingGold,
    float iapVerifyingPrice,
    TextInput& iapCodeInput,
    bool isDead,
    float elapsedGameTime,
    float timeOffset,
    float totalCycleTime,
    bool selectionDone,
    bool chatActive,
    int loginMessageIdOffset,
    bool& chatFetchInFlight,
    sf::Clock& chatFetchClock,
    std::future<std::vector<std::pair<std::string, std::string>>>& chatFuture,
    std::vector<std::pair<std::string, std::string>>& chatMessages,
    float chatBoxX, float chatBoxY, float chatBoxWidth, float chatBoxHeight,
    float chatScroll, bool isChatting, TextInput& chatInput,
    bool goldTexLoaded, sf::Sprite& goldSprite,
    bool introActive, const std::optional<sf::Sprite>& introSprite, int currentIntroFrame, const std::vector<std::string>& introTexts,
    bool tutorialActive, const sf::RectangleShape& tutorialOverlay, const sf::RectangleShape& tutorialBox,
    int tutorialStep, const std::vector<TutorialStep>& tutorialSteps,
    float tutMoveDist, bool tutSprinted, bool tutInvOpened, int tutWoodGathered, int tutStoneGathered, bool tutAttacked, bool tutChatted,
    bool spawnOverlayActive, int spawnOverlayDone, int spawnOverlayTotal,
    bool chestMenuOpen,
    PlacedStructure* activeChest,
    int chestSelectedInvSlot,
    std::vector<PlacedCampfire>& campfires,
    std::vector<PlacedStructure>& structures,
    bool quantityPromptOpen, bool quantityPromptIsCampfire, int quantityPromptCampfireSlot, int quantityPromptMax, TextInput& quantityInput,
    bool quantityPromptIsChest, int quantityPromptChestSlot, int quantityPromptSourceSlot,
    bool isSleeping, float sleepFadeTimer, float sleepFadeDuration,
    std::function<const sf::Texture* (int id)> getItemTexture,
    std::function<std::string(int id)> getItemName,
    std::function<int(int id)> getItemPrice,
    bool disasterActive = false,
    float disasterRemaining = 0.f,
    float disasterBannerTimer = 0.f,
    const std::string& disasterBannerMessage = "",
    bool isAdmin = false,
    const std::string& currentDisasterType = "diarrhea"
);
