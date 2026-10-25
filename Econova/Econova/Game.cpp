#define _CRT_SECURE_NO_WARNINGS
#include "Game.h"
#include "ViewUtils.h"
#include <future>
#include <mutex>
#include <map>
#include "Camera.h"
#include "Inventory.h"
#include "ItemManager.h"
#include "Animation.h"
#include "Animal.h"
#include "Monster.h"
#include "Chicken.h"
#include "Pig.h"
#include "Zombie.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "ZombieFlesh.h"
#include "Sheep.h"
#include "SheepMeat.h"
#include "AuthManager.h"
#include "ItemID.h"
#include "Settings.h"
#include "TextInput.h"
#include "MarketStats.h"
#include "Server.h"
#include "Trade.h"
#include "IAPStore.h"
#include "Crafting.h"
#include "StructureSystem.h"
#include "ChatSystem.h"
#include "HUD.h"
#include "WorldRenderer.h"
#include "PlayerController.h"
#include "GameAssets.h"
#include "GameInput.h"
#include "SpawnEntity.h"
#include "CharacterSelection.h"
#include "Button.h"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <algorithm>
#include <cmath>
#include <queue>
#include <map>
#include <iostream>
#include <optional>

const float GRID_STEP_X = 500.f;
const float GRID_STEP_Y = 250.f;
const float GLOBAL_SHEAR = -0.5f;

#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <chrono>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

const int MAP_WIDTH = 50000;
const int MAP_HEIGHT = 50000;

void gameLoop(sf::RenderWindow& window, unsigned int width, unsigned int height, const std::string& username, bool isFirstTimeFlag, BanInfo* outBanKick)
{

    std::cout << "[DEBUG] Entering gameLoop. isFirstTime: " << isFirstTimeFlag << "\n";
    {
        std::ofstream f("game_startup_trace.log", std::ios::trunc);
        if (f.is_open()) f << "trace_begin\n";
    }
    auto traceStartup = [](const std::string& msg) {
        std::ofstream f("game_startup_trace.log", std::ios::app);
        if (f.is_open()) f << msg << "\n";
    };
    traceStartup("start");
    // Reinitialize render context pacing to avoid stuck first present on some drivers.
    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(60);

    // Force an immediate visible break from the MainMenu "Loading complete!" frame
    // before any heavy initialization runs.
    window.setTitle("Econova - In Game");
    window.setView(window.getDefaultView());
    window.clear(sf::Color(30, 30, 30));
    window.display();
    traceStartup("first_present_done");

    AuthManager& authManager = AuthManager::getInstance();
    constexpr float STARTUP_DB_TIMEOUT_SECS = 6.0f;
    // Safety switch: startup DB calls are disabled to prevent transition hangs.
    const bool dbStartupEnabled = true;

    PlayerStats stats = constructServer(authManager, dbStartupEnabled, username, STARTUP_DB_TIMEOUT_SECS);
    traceStartup("after_stats");
    int option = (stats.gender != -1) ? stats.gender : 0;
    bool selectionDone = (stats.gender != -1);

    bool inventoryOpen = false;
    bool invKeyHeld = false;
    bool itemSpawned = false;
    bool musicLoaded = false;
    bool musicStarted = false;
    bool nearPond = false;
    bool isDead = false;
    bool wasNight = false;
    bool isMenuOpen = false;
    bool shieldEquipped = false;
    bool swordEquipped = false;
    float playerAttackCooldown = 0.f;

    sf::Music nightMusic;
    bool nightMusicLoaded = false;
    bool nightMusicPlaying = false;

    std::vector<std::unique_ptr<Animal>> animals;
    std::vector<std::unique_ptr<Monster>> monsters;

    sf::RectangleShape darkness;
    darkness.setSize(sf::Vector2f((float)MAP_WIDTH, (float)MAP_HEIGHT));
    darkness.setFillColor(sf::Color(0, 0, 0, 0));

    Inventory inventory;
    ItemManager itemManager;

    const int FRAMES_PER_DIR = 6;
    AnimSet animWalk;
    AnimSet animWalkWithSword;
    AnimSet animWalkWithShield;
    AnimSet animWalkWithShieldAndSword;
    AnimSet animAttackWithSword;
    AnimSet animAttackWithShieldAndSword;
    bool animLoadedForSelection = false;

    traceStartup("after_font");

    GameAssets assets;
    loadGameAssets(assets, MAP_WIDTH, MAP_HEIGHT);

    sf::Font& font = assets.font;
    sf::Font& uiFont = assets.font;

    CharacterSelection characterSelection(font, width, height);

    sf::Texture& selBgTexture = assets.selBgTexture;
    sf::Sprite& selBgSprite = *assets.selBgSprite;

    sf::Texture& bgTexture = assets.bgTexture;
    traceStartup("after_background");

    sf::Sprite& background = *assets.background;

    const float BAR_WIDTH = 400.f;
    const float BAR_HEIGHT = 22.f;

    float maxHP = 100.f;
    float hp = 100.f;

    float maxHunger = 100.f;
    float hunger = 0.f;

    float maxThirst = 100.f;
    float thirst = 0.f;

    // Food multiplier system
    int hungerMultiplier = 1;
    int thirstMultiplier = 1;
    const int MAX_MULTIPLIER = 10;

    int currency = 0;
    int level = 1;
    int exp = 0;
    float timeOffset = 0.f;
    float elapsedGameTime = 0.f;
    long long serverEpochCache = 0LL;  // server UNIX epoch for synced day/night

    bool isSleeping = false;
    float sleepFadeTimer = 0.f;
    const float sleepFadeDuration = 1.5f;
    bool sleepTimeSkipped = false;
    bool justFinishedSleeping = false;

    // rates (per second)
    const float HUNGER_RATE = 1.f;
    const float THIRST_RATE = 1.f;
    const float DAMAGE_RATE = 10.f;

    float zombieAttackTimer = 0.f;
    float playerHurtTimer = 0.f;
    bool isPlayerHurt = false;
    const float ZOMBIE_ATTACK_COOLDOWN = 0.8f;
    const int ZOMBIE_DAMAGE = 10;

    int eqShieldSlot = -1;
    int eqSwordSlot = -1;

    sf::Texture& texMaleOnline = assets.texMaleOnline;
    sf::Texture& texFemaleOnline = assets.texFemaleOnline;

    sf::Texture texSouth;
    sf::Sprite sprite(texSouth);
    sprite.setOrigin({ 32.f, 32.f });
    sprite.setPosition({ MAP_WIDTH / 2.f, MAP_HEIGHT / 2.f });

    sf::Texture& hpIconTex = assets.hpIconTex;
    sf::Texture& hungerIconTex = assets.hungerIconTex;
    sf::Texture& thirstIconTex = assets.thirstIconTex;
    traceStartup("after_ui_icons");

    sf::Texture& wrenchTex = assets.wrenchTex;

    NPC npcFemale(wrenchTex), npcMale(wrenchTex);
    npcFemale.name = "Female NPC";
    npcMale.name = "Male NPC";

    bool wrenchMenuOpen = false;
    sf::Vector2f wrenchMenuPos;
    bool tradeMenuOpen = false;
    bool tradeBoxesTicked = false;
    std::string playerGoldInput = "0";
    std::string npcGoldInput = "0";

    bool isNight = false;
    bool isSellingToNPC = false;
    bool playerGoldTyping = false;
    bool p2pGoldTyping = false;
    bool npcGoldTyping = false;
    bool tradeConfirmOpen = false;
    std::string tradeConfirmMessage = "";
    bool tradeConfirmIsP2P = false;
    bool hasBargained = false;
    bool bargainAccepted = false;
    std::string bargainStatus = "";
    sf::Color bargainColor = sf::Color::White;

    bool craftMenuOpen = false;
    bool placementMode = false;
    float placementRotation = 0.f; // 0: Vertical, 90: Horizontal
    int playerFloorLevel = 0;
    float stairOffset = 0.f;  // set each frame by renderWorldDepthScene; used to offset collision probe
    bool placementFollowingCursor = true;

    int placementItemID = 0;
    sf::Vector2f placementPos;
    int placementFromInventorySlot = -1;
    int playerTradeGold = 0;
    int npcTradeGold = 0;

    // --- P2P Online Trade State ---
    bool   p2pTradeOpen      = false;
    int    p2pTradeID        = -1;
    bool   p2pAmInitiator    = false;
    std::string p2pPartner   = "";
    float  p2pPollTimer      = 0.f;
    P2PTradeState p2pState   = {};
    // My 4 offer slots + gold
    TradeSlot myP2PSlots[4]  = {};
    int   myP2PGold          = 0;
    std::string myP2PGoldInput = "0";
    bool  p2pMyReady         = false;
    bool  p2pPartnerReady    = false;
    std::string p2pStatusMsg = "";
    bool  p2pInvitePopup     = false;   // show invite for target player
    bool  p2pOfferDirty      = false;   // need to push offer to DB
    float p2pOfferPushTimer  = 0.f;
    // Parsed partner offer (from DB CSV)
    std::vector<P2PItemDisplay> partnerItems;
    int   partnerGold        = 0;
    std::string wrenchTargetPlayer = ""; // username of online player clicked

    bool saplingPlacementMode = false;
    bool saplingFollowingCursor = true;
    sf::Vector2f saplingPlacementPos;
    int saplingFromInventorySlot = -1;
    float saplingRotation = 0.f;

    std::vector<PlantedSapling> plantedSaplings;
    std::vector<PlacedStructure> structures;
    bool chestMenuOpen = false;
    PlacedStructure* activeChest = nullptr;
    int chestSelectedInvSlot = -1;    // -1 = none; set when player clicks an inv slot in chest menu
    bool amountPopupOpen = false;
    int amountPopupSlot = -1;
    int amountSelected = 1;
    int amountMax = 1;
    std::vector<PlacedCampfire> campfires;

    auto syncSaveLocalStructuresAndCampfires = [&]() {
        syncSaveStructuresAndCampfires(authManager, username, dbStartupEnabled, structures, campfires, &inventory);
    };
    traceStartup("after_campfire_frames");

    bool isTargetNPC = false;
// struct TradeSlot moved to the top of gameLoop
    TradeSlot playerTradeSlots[4];
    TradeSlot npcTradeSlots[4];

    bool stockPageOpen = false;
    std::string marketSearchText = "";
    bool marketSearchActive = false;
    sf::Clock marketCursorBlink;
    int marketScrollOffset = 0;
    bool adminSimOpen = false;
    int adminSimScrollOffset = 0;
    bool iapPageOpen = false;
    bool iapVerifyingOpen = false;
    int iapVerifyingGold = 0;
    float iapVerifyingPrice = 0.f;
    std::string iapVerifyingTier = "";
    sf::Clock stockUpdateClock;

    // Natural Disaster system (e.g. Diarrhea Outbreak, Drought)
    sf::Clock disasterCycleClock;
    bool disasterActive = false;
    float disasterRemaining = 0.f;
    float disasterBannerTimer = 0.f;
    std::string disasterBannerMessage = "";
    std::string currentDisasterType = "diarrhea";

    // Authoritative security and disaster sync state
    BanInfo selfBanInfo;
    MuteInfo localMuteInfo = authManager.getMuteStatus(username);
    ActiveDisasterInfo serverDisasterInfo;
    bool serverDisasterUpdated = false;


    // DB-scheduled disaster polling
    sf::Clock disasterPollClock;          // polls DB every 15 s
    int  activeScheduledEventID = -1;     // event_id from disaster_events currently running (-1 = none)
    const float DISASTER_POLL_INTERVAL = 15.f;

    std::vector<StockItem> stockMarket;
    initStockMarket(stockMarket, timedLoadAllMarketPrices);

    // Load initial authoritative disaster state from DB upon connect/login
    ActiveDisasterInfo initDisaster = authManager.getActiveDisaster();
    if (initDisaster.active && initDisaster.remainingSeconds > 0.f) {
        disasterActive = true;
        currentDisasterType = initDisaster.type;
        disasterRemaining = initDisaster.remainingSeconds;
        activeScheduledEventID = initDisaster.eventID;
        disasterBannerTimer = 5.f;
        if (currentDisasterType == "drought") {
            disasterBannerMessage = "NATURAL DISASTER: Severe Drought strikes! Ponds dried up! Flora & Fauna withering!";
            applyDroughtMarketImpact(stockMarket, authManager);
        } else {
            disasterBannerMessage = "NATURAL DISASTER: Acute Diarrhea strikes! Raw food is POISONOUS! Cooked food in high demand!";
            applyDisasterFoodMarketImpact(stockMarket, authManager);
        }
    }

    std::string userRole = authManager.getUserRole(username.c_str());
    std::string lowerRole = userRole;
    for (char& c : lowerRole) c = (char)::tolower((unsigned char)c);
    bool isAdmin = (lowerRole == "admin" || lowerRole == "administrator" || AuthManager::isTargetedAdmin(username, ""));

    auto getItemPrice = [&](int id) -> int {
        return getMarketItemPrice(id, stockMarket);
    };

    auto getItemName = [&](int id) -> std::string {
        return getMarketItemName(id, stockMarket);
    };

    auto adjustMarketItem = [&](int itemId, float percentChange) {
        ::adjustMarketItem(stockMarket, itemId, percentChange, authManager);
    };

    auto customCommandHandler = [&](const std::string& cmd, const std::string& args, std::string& feedback) -> bool {
        if (cmd == "disaster") {
            std::string lowerArg = args;
            for (char& c : lowerArg) c = (char)::tolower((unsigned char)c);
            if (lowerArg == "diarrhea" || lowerArg == "start diarrhea") {
                bool ok = authManager.triggerDisaster("diarrhea", username, feedback);
                if (ok) {
                    currentDisasterType = "diarrhea";
                    disasterActive = true;
                    disasterRemaining = 600.f; // Exactly 10 minutes (600s)
                    disasterBannerTimer = 7.f;
                    disasterBannerMessage = "[STAFF TRIGGER] NATURAL DISASTER: Acute Diarrhea strikes! Raw meat POISONED! Cooked food prices surging!";
                    applyDisasterFoodMarketImpact(stockMarket, authManager);
                    inventory.showBanner("[STAFF] Acute Diarrhea Outbreak ACTIVATED! (10m)");
                }
                return true;
            }
            else if (lowerArg == "drought" || lowerArg == "start drought") {
                bool ok = authManager.triggerDisaster("drought", username, feedback);
                if (ok) {
                    currentDisasterType = "drought";
                    disasterActive = true;
                    disasterRemaining = 180.f; // Exactly 3 minutes (180 seconds)!
                    disasterBannerTimer = 7.f;
                    disasterBannerMessage = "[STAFF TRIGGER] NATURAL DISASTER: Severe Drought strikes! Ponds dried up! Trees & Wildlife dying! Wood & Meat prices surging!";

                    // 1. Cull 65% of trees
                    auto& items = itemManager.getItems();
                    std::vector<Item> keptItems;
                    keptItems.reserve(items.size());
                    for (auto& it : items) {
                        if (it.id == ITEM_TREE) {
                            if ((rand() % 100) < 65) continue;
                        }
                        keptItems.push_back(std::move(it));
                    }
                    items = std::move(keptItems);
                    itemManager.rebuildSpatialGrid();

                    // 2. Cull 70% of animals
                    std::vector<std::unique_ptr<Animal>> keptAnimals;
                    keptAnimals.reserve(animals.size());
                    for (auto& a : animals) {
                        if ((rand() % 100) < 70) continue;
                    }
                    animals = std::move(keptAnimals);

                    applyDroughtMarketImpact(stockMarket, authManager);
                    inventory.showBanner("[STAFF] Severe Drought ACTIVATED! Ponds dried up! (3m)");
                }
                return true;
            }
            else if (lowerArg == "stop" || lowerArg == "clear" || lowerArg == "end") {
                bool ok = authManager.stopActiveDisaster(username, feedback);
                if (ok || disasterActive) {
                    if (currentDisasterType == "drought") {
                        restoreDroughtMarketImpact(stockMarket, authManager);
                        inventory.showBanner("[STAFF] Severe Drought STOPPED! Ponds restored, wood & meat prices normalized.");
                    }
                    else {
                        restoreDisasterFoodMarketImpact(stockMarket, authManager);
                        inventory.showBanner("[STAFF] Natural Disaster STOPPED! Meat prices normalized.");
                    }
                    disasterActive = false;
                    disasterRemaining = 0.f;
                    disasterBannerTimer = 5.f;
                    disasterBannerMessage = "[STAFF TRIGGER] Natural Disaster ended manually across all players.";
                    if (!ok) feedback = "[STAFF] Natural Disaster stopped.";
                    activeScheduledEventID = -1;
                }
                return true;
            }
            else {
                feedback = "Unknown disaster: '" + args + "'. Available: diarrhea, drought, stop.";
                return false;
            }
        }
        else if (cmd == "sim") {
            adminSimOpen = !adminSimOpen;
            feedback = adminSimOpen ? "Admin Quantity Simulator opened." : "Admin Quantity Simulator closed.";
            return true;
        }
        return false;
    };

    sf::Sprite playerWrenchBtn(wrenchTex);
    playerWrenchBtn.setScale({ 0.05f, 0.05f });
    npcFemale.wrenchBtn->setTexture(wrenchTex);
    npcFemale.wrenchBtn->setScale({ 0.05f, 0.05f });
    npcMale.wrenchBtn->setTexture(wrenchTex);
    npcMale.wrenchBtn->setScale({ 0.05f, 0.05f });

    HUDVitalsUI vitalsUI(font, hpIconTex, hungerIconTex, thirstIconTex, BAR_WIDTH, BAR_HEIGHT);
    auto& hungerMultiplierText = vitalsUI.hungerMultiplierText;
    auto& thirstMultiplierText = vitalsUI.thirstMultiplierText;


    sf::Texture& woodTex = assets.woodTex;
    sf::Texture& treeTex = assets.treeTex;
    sf::Texture& stoneTex = assets.stoneTex;
    sf::Texture& pondTex = assets.pondTex;
    sf::Texture& saplingTex = assets.saplingTex;
    sf::Texture& goldTex = assets.goldTex;
    sf::Texture& woodenSwordTex = assets.woodenSwordTex;
    sf::Texture& shieldTex = assets.shieldTex;
    sf::Texture& wrenchItemTex = assets.wrenchItemTex;
    sf::Texture& furnaceTex = assets.furnaceTex;
    sf::Texture& woodTileTex = assets.woodTileTex;
    sf::Texture& woodWallTex = assets.woodWallTex;
    sf::Texture& woodDoorTex = assets.woodDoorTex;
    sf::Texture& woodRoofTex = assets.woodRoofTex;
    sf::Texture& woodStairsTex = assets.woodStairsTex;
    sf::Texture& bedTex = assets.bedTex;
    sf::Texture& chestTex = assets.chestTex;
    sf::Texture& swordIconTex = assets.swordIconTex;
    sf::Texture& shieldIconTex = assets.shieldIconTex;
    sf::Texture& tileIconTex = assets.tileIconTex;
    sf::Texture& wallIconTex = assets.wallIconTex;
    sf::Texture& doorIconTex = assets.doorIconTex;
    sf::Texture& roofIconTex = assets.roofIconTex;
    sf::Texture& stairsIconTex = assets.stairsIconTex;
    sf::Texture& bedIconTex = assets.bedIconTex;
    sf::Texture& cookedChickenTex = assets.cookedChickenTex;
    sf::Texture& cookedPigTex = assets.cookedPigTex;
    sf::Texture& cookedSheepTex = assets.cookedSheepTex;
    sf::Texture& woolTex = assets.woolTex;
    sf::Texture& coalTex = assets.coalTex;
    sf::Texture& copperOreTex = assets.copperOreTex;
    sf::Texture& copperBarTex = assets.copperBarTex;
    sf::Texture& ironOreTex = assets.ironOreTex;
    sf::Texture& ironBarTex = assets.ironBarTex;
    sf::Texture& goldOreTex = assets.goldOreTex;
    sf::Texture& goldBarTex = assets.goldBarTex;
    sf::Texture& diamondTex = assets.diamondTex;
    sf::Texture* campfireFrames = assets.campfireFrames;


    bool goldTexLoaded = (goldTex.getSize().x > 0);
    sf::Sprite goldSprite(goldTex);
    goldSprite.setScale({ 0.5f, 0.5f });

    bool shieldCrafting = false;
    float shieldCraftTimer = 0.f;

    bool swordCrafting = false;
    float swordCraftTimer = 0.f;
    sf::Sprite playerSwordSprite(woodenSwordTex);
    playerSwordSprite.setScale({ 0.5f, 0.5f });
    sf::Sprite pond(pondTex);
    pond.setScale({ 10.0f, 10.0f });

    itemManager.setResourceTextures(&woodTex, &stoneTex);

    auto getItemTexture = [&](int id) -> const sf::Texture* {
        return assets.getItemTexture(id);
    };

    sf::Clock animalRespawnClock;

    sf::Clock deathClock;

    const float totalCycleTime = 900.f;

    sf::Clock deltaClock;
    sf::Clock dayNightClock;

    UpdateViewToWindow(&window);

    Dir currentDir = Dir::South;

    const float MOVE_SPEED = 250.f;



    std::vector<sf::Sprite> ponds;
    std::vector<sf::FloatRect> pondBoundsList;
    for (int i = 0; i < 20; i++) {
        sf::Sprite pSprite(pondTex);
        pSprite.setScale({ 10.0f, 10.0f });
        pSprite.setPosition({ (float)(rand() % (int)MAP_WIDTH), (float)(rand() % (int)MAP_HEIGHT) });
        ponds.push_back(pSprite);
        pondBoundsList.push_back(pSprite.getGlobalBounds());
    }


    sf::Music bgMusic;
    if (bgMusic.openFromFile("Music/Locally Sourced - Jason Farnham.mp3"))
    {
        bgMusic.setLooping(true);
        bgMusic.setVolume(globalSettings.volume); // 0–100
        musicLoaded = true;
        currentActiveMusic = &bgMusic;
    }
    else
    {
        std::cerr << "Failed to load background music\n";
    }
    traceStartup("after_bgm");

    float chatBoxWidth = 800.f;
    float chatBoxHeight = 400.f;
    float chatBoxX = (1920.f - chatBoxWidth) / 2.f;
    float chatBoxY = 20.f;
    TextInput chatInput(font, { chatBoxX + 10.f, chatBoxY + chatBoxHeight - 50.f }, { chatBoxWidth - 20.f, 40.f });
    TextInput iapCodeInput(font, { (width - 400.f) / 2.f, (height - 40.f) / 2.f + 40.f }, { 400.f, 40.f });
    bool quantityPromptOpen = false;
    int quantityPromptMax = 0;
    int quantityPromptTargetSlot = -1;
    int quantityPromptSourceSlot = -1;
    bool quantityPromptIsP2P = false;
    bool quantityPromptIsCampfire = false;
    int quantityPromptCampfireSlot = 0;
    bool quantityPromptIsChest = false;
    int quantityPromptChestSlot = -1;
    TextInput quantityInput(font, { (width - 200.f) / 2.f, (height - 40.f) / 2.f + 10.f }, { 200.f, 40.f });
    bool adminPromptOpen = false;
    bool adminPromptIsBan = true;
    std::string adminPromptTarget = "";
    std::string adminPromptDuration = "";
    int adminPromptMuteMinutes = 60;
    sf::Clock adminPromptClock;
    TextInput adminReasonInput(font, { ((float)width - 480.f) / 2.f, ((float)height - 42.f) / 2.f }, { 480.f, 42.f });
    bool isChatting = false;
    bool chatActive = false;
    std::vector<std::pair<std::string, std::string>> chatMessages;
    sf::Clock chatFetchClock;
    std::string latestPlayerMsg = "";
    float bubbleTimer = 0.f;
    float chatScroll = 0.f;
    bool isDraggingChat = false;
    sf::Vector2f lastMousePosChat;

    // --- Active session chat: show chat messages and history of players currently in the game ---
    int loginMessageIdOffset = 0;
    traceStartup("after_chat_offset");
    authManager.clearPlayerChatMessages(username.c_str());
    authManager.cleanupInactiveChatMessages();

    // --- Async multiplayer sync state ---
    std::future<SyncResult> syncFuture;
    bool syncInFlight = false;

    // --- Async chat fetch state ---
    std::future<std::vector<std::pair<std::string, std::string>>> chatFuture;
    bool chatFetchInFlight = false;

    // --- Async recent trades state ---
    std::future<std::vector<TradeRecord>> recentTradesFuture;
    bool recentTradesFetchInFlight = false;
    sf::Clock recentTradesClock;
    std::vector<TradeRecord> cachedRecentTrades;

    // --- Graceful Logout State ---
    bool isLoggingOut = false;
    bool logoutCloseWindow = false;

    // --- Online player interpolation: store target (DB) positions ---
    std::map<std::string, sf::Vector2f> onlinePlayerTargets;  // target pos from DB
    std::map<std::string, sf::Vector2f> onlinePlayerDisplayPos; // smoothed display pos
    std::map<std::string, sf::FloatRect> onlinePlayerWrenchBounds; // wrench hitbox per online player

    // Story Cinematic State
    bool introActive = isFirstTimeFlag;
    int currentIntroFrame = 0;

    std::optional<sf::Sprite> introSprite;
    auto& introFrames = assets.introFrames;
    if (introActive && !introFrames.empty()) {
        introSprite.emplace(introFrames[0]);
    }
    auto& introTexts = assets.introTexts;

    bool tutorialActive = false;
    int tutorialStep = 0;
    sf::RectangleShape tutorialOverlay({ (float)width, (float)height });
    tutorialOverlay.setFillColor(sf::Color::Transparent);

    // Tutorial Goal Tracking
    float tutMoveDist = 0.f;
    bool tutSprinted = false;
    bool tutInvOpened = false;
    int tutWoodGathered = 0;
    int tutStoneGathered = 0;
    bool tutAttacked = false;
    bool tutChatted = false;
    sf::Vector2f tutLastPos = sprite.getPosition();

    auto& tutorialSteps = assets.tutorialSteps;
    sf::RectangleShape tutorialBox({ 800.f, 400.f });
    tutorialBox.setFillColor(sf::Color(20, 20, 20, 240));
    tutorialBox.setOutlineColor(sf::Color::Cyan);
    tutorialBox.setOutlineThickness(3.f);
    tutorialBox.setPosition({ (width - 800.f) / 2.f, height - 450.f });

    // Daily Quest System
    auto& availableQuests = assets.availableQuests;
    auto& craftableItems = assets.craftableItems;
    QuestInfo qInfo = timedGetQuestInfo(username);
    traceStartup("after_quest_info");

    time_t tnow = time(0);
    struct tm tstruct;
#ifdef _WIN32
    localtime_s(&tstruct, &tnow);
#else
    tstruct = *localtime(&tnow);
#endif
    char tbuf[80] = { 0 };
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d", &tstruct);
    std::string today(tbuf);

    if (dbStartupEnabled && qInfo.lastReset != today) {
        int newQuestID = (rand() % (availableQuests.size())) + 1;
        std::string unameCopy = username;
        std::string todayCopy = today;
        std::thread([&authManager, unameCopy, newQuestID, todayCopy]() {
            authManager.resetQuest(unameCopy.c_str(), newQuestID, todayCopy.c_str());
            }).detach();
        qInfo.questID = newQuestID;
        qInfo.progress = 0;
        qInfo.lastReset = today;
    }

    std::vector<OnlinePlayer> otherPlayers;

    const float SPAWN_RADIUS = 2000.f;
    const float DESPAWN_RADIUS = 3500.f;

    const int MAX_ANIMALS = 6;
    const int MAX_MONSTERS = 6;

    sf::Clock mobSpawnClock;
    sf::Clock monsterSpawnClock;

    // Avoid potential platform blocking in std::random_device during startup.
    auto seed = static_cast<unsigned int>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
        ^ static_cast<long long>(reinterpret_cast<std::uintptr_t>(&window))
    );
    if (dbStartupEnabled) {
        seed = static_cast<unsigned int>(authManager.getOrCreateWorldSeed());
    }
    std::mt19937 gen(seed);
    traceStartup("before_main_loop");

    auto getDistance = [](const sf::Vector2f& a, const sf::Vector2f& b)
        {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        };

    // Faster squared-distance check (avoids sqrt) for culling comparisons
    auto getDistanceSq = [](const sf::Vector2f& a, const sf::Vector2f& b)
        {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            return dx * dx + dy * dy;
        };

    std::uniform_real_distribution<float> randX(
        0.f,
        static_cast<float>(MAP_WIDTH)
    );

    std::uniform_real_distribution<float> randY(
        0.f,
        static_cast<float>(MAP_HEIGHT)
    );

    float dt = 0.f;
    sf::Clock saveCooldownClock;
    sf::Vector2f lastSaveCameraPos = sprite.getPosition();
    bool tracedLoopEntered = false;
    bool tracedFirstDisplay = false;
    bool tracedAfterFirstDisplay = false;
    std::uint64_t debugFrameIndex = 0;
    const sf::View defaultView = window.getDefaultView();

    SpawnPlan spawnPlan;
    bool spawnOverlayActive = false;
    int spawnOverlayDone = 0;
    int spawnOverlayTotal = 0;
    long long sleepTimeOffset = 0LL;

    while (window.isOpen())
    {
        ++debugFrameIndex;
        if (!tracedLoopEntered) {
            traceStartup("loop_entered");
            tracedLoopEntered = true;
        }
        dt = deltaClock.restart().asSeconds();
        dt = std::min(dt, 0.1f);
        if (selectionDone && !spawnOverlayActive && !craftMenuOpen && !p2pTradeOpen) {
            if (isSleeping) {

                // Player has started sleeping
                if (!sleepTimeSkipped) {

                    // Immediately move to the beginning of the next daytime cycle
                    elapsedGameTime =
                        (std::floor(elapsedGameTime / totalCycleTime) + 1.f) * totalCycleTime;

                    if (serverEpochCache > 0LL) {
                        long long curSec = (serverEpochCache + sleepTimeOffset) % static_cast<long long>(totalCycleTime);
                        sleepTimeOffset += (static_cast<long long>(totalCycleTime) - curSec);
                    }

                    // Restore HP
                    hp = maxHP;

                    // Remove ALL monsters
                    monsters.clear();

                    // Force daytime visually
                    darkness.setFillColor(sf::Color(0, 0, 0, 0));
                    isNight = false;
                    wasNight = false;

                    // Stop night music and start daytime music
                    nightMusic.stop();
                    nightMusicPlaying = false;

                    if (musicLoaded) {
                        if (!musicStarted) {
                            bgMusic.setLooping(true);
                            bgMusic.setVolume(globalSettings.volume);
                            bgMusic.play();
                            musicStarted = true;
                        }
                        else if (bgMusic.getStatus() != sf::Music::Status::Playing) {
                            bgMusic.play();
                        }
                    }

                    // Save the new daytime
                    std::thread([&authManager, username, hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier]() {
                        try {
                            authManager.savePlayerStats(
                                username.c_str(),
                                hunger,
                                thirst,
                                currency,
                                level,
                                exp,
                                elapsedGameTime,
                                hungerMultiplier,
                                thirstMultiplier
                            );
                        }
                        catch (...) {}
                    }).detach();

                    // Only perform the sleep operation once
                    sleepTimeSkipped = true;

                    // Keep monsters from respawning during the sleep transition
                    justFinishedSleeping = true;

                    // Start fade
                    sleepFadeTimer = sleepFadeDuration;
                }
                else {

                    sleepFadeTimer -= dt;

                    if (sleepFadeTimer <= 0.f) {
                        sleepFadeTimer = 0.f;
                        isSleeping = false;
                        sleepTimeSkipped = false;

                        // Keep the world in daytime after waking
                        elapsedGameTime =
                            (std::floor(elapsedGameTime / totalCycleTime) + 0.f) * totalCycleTime;

                        darkness.setFillColor(sf::Color(0, 0, 0, 0));
                        isNight = false;
                        wasNight = false;

                        // Make absolutely sure no monsters remain
                        monsters.clear();

                        // Daytime music
                        nightMusic.stop();
                        nightMusicPlaying = false;

                        if (musicLoaded) {
                            if (bgMusic.getStatus() != sf::Music::Status::Playing) {
                                bgMusic.play();
                            }
                        }

                        justFinishedSleeping = false;
                    }
                }
            }
            else {
                elapsedGameTime += dt;
            }
        }

        // Always clear a fresh frame immediately, so we never "keep showing"
        // the last MainMenu backbuffer if the game update takes an early path.
        // (Safe: Intel crashes were caused by setActive toggles, not clear().)
        window.setView(defaultView);
        window.clear(sf::Color(30, 30, 30));



        if (tracedLoopEntered) {
            // lightweight marker that we got past dt without crashing
            traceStartup("after_dt");
        }

        // Campfire button click system removed (handled in Event Loop instead)

        // Start incremental spawning once character selection is done.
        if (selectionDone && !itemSpawned && !spawnPlan.active) {
            spawnPlan.start();
            traceStartup("spawn_plan_started");
        }

        runIncrementalWorldSpawnStep(
            spawnPlan,
            itemManager,
            treeTex,
            stoneTex,
            animals,
            monsters,
            structures,
            campfires,
            pondBoundsList,
            MAP_WIDTH,
            MAP_HEIGHT,
            snapToGrid,
            gen,
            itemSpawned,
            spawnOverlayActive,
            spawnOverlayDone,
            spawnOverlayTotal
        );

        sf::Vector2f playerPos = sprite.getPosition();


        // =====================================================
        UpdateCamera(window, sprite, MAP_WIDTH, MAP_HEIGHT);
        sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        sf::View currentView = window.getView();
        sf::Vector2f camCenter = currentView.getCenter();
        sf::Vector2f camSize = currentView.getSize();
        sf::FloatRect cameraBounds({ camCenter.x - camSize.x * 0.5f, camCenter.y - camSize.y * 0.5f }, camSize);

        bool anyCampfireMenuOpen = false;
        for (auto& fire : campfires) {
            if (fire.menuOpen) {
                anyCampfireMenuOpen = true;
                break;
            }
        }
        bool anyFurnaceMenuOpen = false;
        for (auto& s : structures) {
            if (s.id == ITEM_FURNACE && s.menuOpen) {
                anyFurnaceMenuOpen = true;
                break;
            }
        }
        isMenuOpen = (adminPromptOpen || adminSimOpen || stockPageOpen || iapPageOpen || iapVerifyingOpen || tradeMenuOpen || tradeConfirmOpen || craftMenuOpen || p2pTradeOpen || anyCampfireMenuOpen || anyFurnaceMenuOpen || chestMenuOpen || isSleeping || quantityPromptOpen);

        // =====================================================
        // DESPAWN / SPAWN ENTITIES (ONLY IF NOT IN LOADING SCREEN)
        // Spawns and moves strictly within camera bounds to prevent lag
        // =====================================================
        if (!spawnOverlayActive)
        {
            despawnFarEntities(animals, monsters, cameraBounds, 600.f);
            int activeMaxAnimals = (disasterActive && currentDisasterType == "drought") ? 3 : MAX_ANIMALS;
            spawnNearbyAnimals(animals, playerPos, mobSpawnClock, activeMaxAnimals, cameraBounds, MAP_WIDTH, MAP_HEIGHT, structures, campfires);
            bool isNight = darkness.getFillColor().a > 150;
            spawnNightZombies(monsters, playerPos, monsterSpawnClock, MAX_MONSTERS, cameraBounds, MAP_WIDTH, MAP_HEIGHT, isNight, structures, campfires);
        }

        auto isBlocked = [&](sf::Vector2f pos) {
            bool isDrought = (disasterActive && currentDisasterType == "drought");
            static const std::vector<sf::Sprite> emptyPonds;
            return isWorldPositionBlocked(pos, structures, isDrought ? emptyPonds : ponds);
        };

        // Update nearby animals and monsters (always — monsters must attack even when menus are open)
        if (!spawnOverlayActive)
        {
            updateNearbyEntities(animals, monsters, playerPos, dt, isBlocked, cameraBounds);
        }

        bubbleTimer = std::max(0.f, bubbleTimer - dt);

        std::string chatAlert = authManager.getAndClearChatNotification();
        if (!chatAlert.empty()) {
            latestPlayerMsg = chatAlert;
            bubbleTimer = 5.0f;
        }

        pollAndSyncP2PTrade(
            dt,
            p2pTradeID,
            p2pPollTimer,
            p2pOfferPushTimer,
            p2pState,
            p2pAmInitiator,
            partnerGold,
            p2pPartnerReady,
            p2pMyReady,
            partnerItems,
            p2pInvitePopup,
            p2pTradeOpen,
            p2pStatusMsg,
            inventory,
            getItemTexture,
            getItemName,
            getItemPrice,
            adjustMarketItem,
            currency,
            myP2PSlots,
            myP2PGold,
            latestPlayerMsg,
            bubbleTimer,
            username,
            p2pPartner,
            hunger,
            thirst,
            level,
            exp,
            elapsedGameTime,
            hungerMultiplier,
            thirstMultiplier,
            p2pOfferDirty,
            authManager,
            disasterActive,
            currentDisasterType
        );

        // --- Natural Disaster System Update (Diarrhea Outbreak / Severe Drought) ---
        if (!introActive && !tutorialActive && !isDead) {
            // Periodic chance of a natural disaster every 30 minutes (1800 seconds)
            if (!disasterActive && disasterCycleClock.getElapsedTime().asSeconds() >= 1800.f) {
                disasterCycleClock.restart();
                // 50% chance every 30 minutes
                if ((rand() % 100) < 50) {
                    disasterActive = true;
                    disasterRemaining = 600.f; // Extended duration: 600 seconds (10 minutes)
                    disasterBannerTimer = 6.f;
                    if ((rand() % 2) == 0) {
                        currentDisasterType = "diarrhea";
                        disasterBannerMessage = "NATURAL DISASTER: Acute Diarrhea strikes! Raw food is POISONOUS! Cooked food in high demand!";
                        applyDisasterFoodMarketImpact(stockMarket, authManager);
                    } else {
                        currentDisasterType = "drought";
                        disasterBannerMessage = "NATURAL DISASTER: Severe Drought strikes! Ponds dried up! Flora & Fauna withering!";
                        // Cull 65% of trees
                        auto& items = itemManager.getItems();
                        std::vector<Item> retainedItems;
                        retainedItems.reserve(items.size());
                        for (const auto& it : items) {
                            if (it.id == ITEM_TREE) {
                                if ((rand() % 100) < 35) retainedItems.push_back(it);
                            } else {
                                retainedItems.push_back(it);
                            }
                        }
                        items = std::move(retainedItems);
                        itemManager.rebuildSpatialGrid();

                        // Cull 70% of animals
                        std::vector<std::unique_ptr<Animal>> retainedAnimals;
                        for (auto& a : animals) {
                            if ((rand() % 100) < 30) retainedAnimals.push_back(std::move(a));
                        }
                        animals = std::move(retainedAnimals);

                        applyDroughtMarketImpact(stockMarket, authManager);
                    }
                }
            }

            // ── Poll DB for admin-scheduled disasters (every 15 s, non-blocking async) ─────────────
            static std::future<DisasterEvent> disasterPollFuture;
            static bool disasterPollInFlight = false;

            if (!disasterActive && !disasterPollInFlight && disasterPollClock.getElapsedTime().asSeconds() >= DISASTER_POLL_INTERVAL) {
                disasterPollClock.restart();
                disasterPollInFlight = true;
                disasterPollFuture = std::async(std::launch::async, [&authManager]() {
                    try {
                        return authManager.fetchPendingDisaster();
                    } catch (...) {
                        return DisasterEvent();
                    }
                });
            }

            if (disasterPollInFlight && disasterPollFuture.valid()) {
                if (disasterPollFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    DisasterEvent ev;
                    try {
                        ev = disasterPollFuture.get();
                    } catch (...) {
                        ev = DisasterEvent();
                    }
                    disasterPollInFlight = false;

                    if (!disasterActive && ev.eventID >= 0) {
                        activeScheduledEventID = ev.eventID;
                        currentDisasterType    = ev.type;
                        disasterActive         = true;
                        disasterRemaining      = (float)ev.durationSeconds;
                        disasterBannerTimer    = 7.f;
                        std::thread([&authManager, eid = ev.eventID]() {
                            try { authManager.markDisasterActive(eid); } catch (...) {}
                        }).detach();

                        if (ev.type == "drought") {
                            disasterBannerMessage =
                                "[ADMIN EVENT] NATURAL DISASTER: Severe Drought! "
                                "Ponds dried up! Trees & Wildlife withering! Prices surge!";
                            auto& dItems = itemManager.getItems();
                            std::vector<Item> keptItems;
                            keptItems.reserve(dItems.size());
                            for (const auto& it : dItems) {
                                if (it.id == ITEM_TREE) { if ((rand() % 100) < 35) keptItems.push_back(it); }
                                else keptItems.push_back(it);
                            }
                            dItems = std::move(keptItems);
                            itemManager.rebuildSpatialGrid();
                            std::vector<std::unique_ptr<Animal>> keptAnimals;
                            for (auto& a : animals)
                                if ((rand() % 100) < 30) keptAnimals.push_back(std::move(a));
                            animals = std::move(keptAnimals);
                            applyDroughtMarketImpact(stockMarket, authManager);
                        } else {
                            disasterBannerMessage =
                                "[ADMIN EVENT] NATURAL DISASTER: Acute Diarrhea! "
                                "Raw food POISONOUS! Cooked food demand surging!";
                            applyDisasterFoodMarketImpact(stockMarket, authManager);
                        }
                        inventory.showBanner("[EVENT] Scheduled disaster activated: " + ev.type +
                                             " (" + std::to_string(ev.durationSeconds) + "s)");
                        std::cout << "[DISASTER POLL] Activated event #" << ev.eventID
                                  << " type=" << ev.type << " dur=" << ev.durationSeconds << "s\n";
                    }
                }
            }

            if (disasterActive) {
                disasterRemaining -= dt;
                if (!tutorialActive) {   // no disaster drain during tutorial
                if (currentDisasterType == "drought") {
                    // Severe drought accelerates dehydration
                    thirst = std::min(maxThirst, thirst + (2.5f * dt));
                } else {
                    // Severe diarrhea accelerates dehydration and hunger
                    thirst = std::min(maxThirst, thirst + (1.8f * dt));
                    hunger = std::min(maxHunger, hunger + (0.8f * dt));
                }
                } // end !tutorialActive

                if (disasterRemaining <= 0.f) {
                    disasterActive = false;
                    disasterRemaining = 0.f;
                    disasterBannerTimer = 5.f;
                    if (currentDisasterType == "drought") {
                        disasterBannerMessage = "The Drought has ended. Rain has returned, ponds are refilled, and prices have normalized.";
                        restoreDroughtMarketImpact(stockMarket, authManager);
                    } else {
                        disasterBannerMessage = "The Diarrhea outbreak has subsided. Meat prices have returned to normal.";
                        restoreDisasterFoodMarketImpact(stockMarket, authManager);
                    }

                    // Mark DB-scheduled event as completed (if applicable, async)
                    if (activeScheduledEventID >= 0) {
                        int eid = activeScheduledEventID;
                        std::thread([&authManager, eid]() {
                            try { authManager.markDisasterCompleted(eid); } catch (...) {}
                        }).detach();
                        std::cout << "[DISASTER POLL] Event #" << activeScheduledEventID << " completed.\n";
                        activeScheduledEventID = -1;
                    }
                }
            }

            if (disasterBannerTimer > 0.f) {
                disasterBannerTimer -= dt;
            }
        }

        updateSurvivalStatsAndQuests(
            dt,
            introActive,
            tutorialActive,
            isMenuOpen,
            spawnOverlayActive,
            hp,
            maxHP,
            hunger,
            maxHunger,
            hungerMultiplier,
            thirst,
            maxThirst,
            thirstMultiplier,
            level,
            exp,
            qInfo,
            availableQuests,
            currency,
            authManager,
            username,
            latestPlayerMsg,
            bubbleTimer,
            swordCrafting,
            swordCraftTimer,
            shieldCrafting,
            shieldCraftTimer,
            inventory,
            woodenSwordTex,
            shieldTex
        );

        bool swordEquipped = false;
        bool shieldEquipped = false;
        int eqSwordSlot = -1;
        int eqShieldSlot = -1;

        for (int i = 0; i < 20; i++) {
            if (inventory.isEquipped(i)) {
                if (inventory.getItemTexture(i) == &woodenSwordTex) {
                    swordEquipped = true;
                    eqSwordSlot = i;
                }
                else if (inventory.getItemTexture(i) == &shieldTex) {
                    shieldEquipped = true;
                    eqShieldSlot = i;
                }
                else {
                    inventory.setEquipped(i, false);
                }
            }
        }



        if (hp <= 0.f && !isDead)
        {
            isDead = true;
            deathClock.restart();

            // ── Permadeath: wipe all persistent player data ───────────────────
            // This clears inventory, structures, campfires, chests, and resets
            // stats in the database so the player starts fresh on next login.
            {
                AuthManager* amPtr = &authManager;
                std::string unameCopy = username;
                std::thread([amPtr, unameCopy]() {
                    try { amPtr->clearPlayerData(sql::SQLString(unameCopy.c_str())); }
                    catch (...) {}
                }).detach();
            }

            // Also clear in-memory state immediately
            inventory.clear();
            structures.clear();
            campfires.clear();
            currency = 50;
            level    = 1;
            exp      = 0;
            hunger   = 0.f;
            thirst   = 0.f;
            hungerMultiplier = 1;
            thirstMultiplier = 1;
        }

        if (inventoryOpen)
        {
            inventory.updateHover(sf::Mouse::getPosition(window), window);
            vitalsUI.update(
                window,
                inventory,
                shieldEquipped,
                eqShieldSlot,
                hp, maxHP,
                hunger, maxHunger, hungerMultiplier,
                thirst, maxThirst, thirstMultiplier,
                level, exp
            );
        }

        // Keep wrench button world positions synced with player and NPC nametags
        {
            sf::Text pNameMeasure(font, username, 18);
            float pW = pNameMeasure.getLocalBounds().size.x;
            playerWrenchBtn.setPosition({ sprite.getPosition().x + pW / 2.f + 5.f, sprite.getPosition().y - 280.f });

            if (npcFemale.sprite.has_value() && npcFemale.wrenchBtn.has_value()) {
                sf::Text fNameMeasure(font, npcFemale.name, 18);
                float fW = fNameMeasure.getLocalBounds().size.x;
                npcFemale.wrenchBtn->setPosition({ npcFemale.sprite->getPosition().x + fW / 2.f + 5.f, npcFemale.sprite->getPosition().y - 280.f });
            }
            if (npcMale.sprite.has_value() && npcMale.wrenchBtn.has_value()) {
                sf::Text mNameMeasure(font, npcMale.name, 18);
                float mW = mNameMeasure.getLocalBounds().size.x;
                npcMale.wrenchBtn->setPosition({ npcMale.sprite->getPosition().x + mW / 2.f + 5.f, npcMale.sprite->getPosition().y - 280.f });
            }
        }

        while (std::optional<sf::Event> event = window.pollEvent())
        {
            if (handleAdminPromptEvent(
                event.value(),
                window,
                adminPromptOpen,
                adminPromptIsBan,
                adminPromptTarget,
                adminPromptDuration,
                adminPromptMuteMinutes,
                adminReasonInput,
                authManager,
                username,
                latestPlayerMsg,
                bubbleTimer,
                adminPromptClock
            )) {
                continue;
            }
            if (handleQuantityPromptEvent(
                event.value(),
                window,
                quantityPromptOpen,
                quantityInput,
                quantityPromptIsCampfire,
                quantityPromptCampfireSlot,
                quantityPromptSourceSlot,
                quantityPromptTargetSlot,
                quantityPromptIsP2P,
                campfires,
                inventory,
                myP2PSlots,
                p2pOfferDirty,
                p2pMyReady,
                playerTradeSlots,
                isSellingToNPC,
                npcTradeGold,
                stockMarket,
                npcGoldInput,
                bargainAccepted,
                saplingPlacementMode,
                syncSaveLocalStructuresAndCampfires,
                &structures,
                quantityPromptIsChest,
                quantityPromptChestSlot,
                activeChest,
                quantityPromptMax,
                [&](int id) { return assets.getItemTexture(id); }
            )) {
                continue;
            }
            if (auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::F7 || key->scancode == sf::Keyboard::Scancode::F7) {
                    inventory.showBanner("[INFO] F7 hotkey is deprecated. Use chat command: /disaster diarrhea (or /disaster stop)");
                    latestPlayerMsg = "Notice: Hotkey F7 is deprecated. Please type /disaster diarrhea or /disaster stop in chat.";
                    bubbleTimer = 6.f;
                    continue;
                }
                if (adminSimOpen && (key->code == sf::Keyboard::Key::Escape || key->scancode == sf::Keyboard::Scancode::Escape)) {
                    adminSimOpen = false;
                    continue;
                }
            }

            bool clickHandled = false;
            if (isChatting) {
                chatInput.processEvent(event.value(), window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));
            }
            if (iapVerifyingOpen) {
                iapCodeInput.processEvent(event.value(), window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));
            }
            if (handleKeyAndTextInputs(
                event.value(),
                globalSettings,
                isChatting,
                chatActive,
                chatInput,
                username,
                authManager,
                otherPlayers,
                latestPlayerMsg,
                bubbleTimer,
                tutorialActive,
                tutorialStep,
                tutChatted,
                placementMode,
                placementRotation,
                saplingPlacementMode,
                saplingRotation,
                structures,
                sprite,
                playerFloorLevel,
                syncSaveLocalStructuresAndCampfires,
                stockPageOpen,
                marketSearchActive,
                marketSearchText,
                marketScrollOffset,
                playerGoldTyping,
                playerGoldInput,
                playerTradeGold,
                currency,
                p2pGoldTyping,
                myP2PGoldInput,
                myP2PGold,
                p2pOfferDirty,
                p2pOfferPushTimer,
                p2pMyReady,
                npcGoldTyping,
                customCommandHandler,
                &adminPromptOpen,
                &adminPromptIsBan,
                &adminPromptTarget,
                &adminPromptDuration,
                &adminPromptMuteMinutes,
                &adminReasonInput,
                &adminPromptClock
            )) {
                continue;
            }

            if (adminSimOpen) {
                if (auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                    adminSimScrollOffset -= (int)wheel->delta;
                    int maxScroll = std::max(0, (int)stockMarket.size() - 9);
                    adminSimScrollOffset = std::clamp(adminSimScrollOffset, 0, maxScroll);
                    continue;
                }
            }

            if (handleMouseDragAndScroll(
                event.value(),
                window,
                width,
                height,
                chatBoxX, chatBoxY, chatBoxWidth, chatBoxHeight,
                chatScroll,
                isDraggingChat,
                lastMousePosChat,
                stockPageOpen,
                marketScrollOffset,
                worldPos,
                campfires,
                inventory,
                username,
                structures,
                isSleeping,
                sleepFadeTimer,
                sleepTimeSkipped,
                placementMode,
                chestMenuOpen,
                activeChest,
                chatActive,
                tradeMenuOpen,
                p2pTradeOpen,
                tradeConfirmOpen,
                wrenchMenuOpen
            )) {
                continue;
            }

            if (handleGlobalKeyEvents(
                event.value(),
                window,
                globalSettings,
                inventoryOpen,
                invKeyHeld,
                tutorialActive,
                tutorialStep,
                tutInvOpened,
                tradeMenuOpen,
                tradeConfirmOpen,
                tradeBoxesTicked,
                playerTradeSlots,
                npcTradeSlots,
                inventory,
                authManager,
                username,
                myP2PSlots,
                p2pTradeID,
                hunger,
                thirst,
                currency,
                level,
                exp,
                elapsedGameTime,
                hungerMultiplier,
                thirstMultiplier,
                structures,
                campfires,
                chestMenuOpen,
                activeChest,
                wrenchMenuOpen,
                stockPageOpen,
                iapPageOpen,
                iapVerifyingOpen,
                chestSelectedInvSlot,
                &isLoggingOut,
                &logoutCloseWindow
            )) {
                continue;
            }

            if (auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouse->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f uiPos = window.mapPixelToCoords(mouse->position, GetUIView(&window));
                    sf::Vector2f mouseWorldPos = window.mapPixelToCoords(mouse->position);

                    if (adminSimOpen) {
                        handleAdminQuantitySimulatorClicks(uiPos, width, height, stockMarket, adminSimScrollOffset, adminSimOpen, authManager);
                        continue;
                    }

                    if (isAdmin) {
                        sf::FloatRect adminSimHudBtn({ (float)width - 440.f, 20.f }, { 130.f, 40.f });
                        if (adminSimHudBtn.contains(uiPos)) {
                            bool anyCampfireOpen = std::any_of(campfires.begin(), campfires.end(), [](const PlacedCampfire& f) { return f.menuOpen; });
                            bool anyFurnaceOpen = std::any_of(structures.begin(), structures.end(), [](const PlacedStructure& s) { return s.id == ITEM_FURNACE && s.menuOpen; });
                            bool isUndergoing = chestMenuOpen || anyCampfireOpen || anyFurnaceOpen || isSleeping || craftMenuOpen || tradeMenuOpen || p2pTradeOpen;
                            if (!isUndergoing) {
                                adminSimOpen = !adminSimOpen;
                                if (adminSimOpen) {
                                    stockPageOpen = false;
                                    chestMenuOpen = false;
                                    activeChest = nullptr;
                                    for (auto& f : campfires) f.menuOpen = false;
                                    for (auto& s : structures) s.menuOpen = false;
                                    isSleeping = false;
                                }
                            }
                            continue;
                        }

                        if (stockPageOpen) {
                            sf::FloatRect sBoxRect({ ((float)width - 920.f) / 2.f, ((float)height - 620.f) / 2.f }, { 920.f, 620.f });
                            sf::FloatRect simBtnRect({ sBoxRect.position.x + 700.f, sBoxRect.position.y + 18.f }, { 150.f, 36.f });
                            if (simBtnRect.contains(uiPos)) {
                                adminSimOpen = true;
                                stockPageOpen = false;
                                continue;
                            }
                        }
                    }

                    handleAllGameMouseButtonPressed(
                        uiPos,
                        mouseWorldPos,
                        mouse->position,
                        window,
                        width,
                        height,
                        deltaClock,
                        stockPageOpen,
                        wrenchMenuOpen,
                        craftMenuOpen,
                        placementMode,
                        tradeMenuOpen,
                        tradeConfirmOpen,
                        tradeConfirmMessage,
                        tradeConfirmIsP2P,
                        p2pTradeOpen,
                        isTargetNPC,
                        wrenchTargetPlayer,
                        wrenchMenuPos,
                        otherPlayers,
                        onlinePlayerDisplayPos,
                        onlinePlayerWrenchBounds,
                        playerWrenchBtn,
                        npcFemale,
                        npcMale,
                        iapPageOpen,
                        iapVerifyingOpen,
                        iapVerifyingGold,
                        iapVerifyingPrice,
                        iapVerifyingTier,
                        iapCodeInput,
                        username,
                        currency,
                        hunger,
                        thirst,
                        level,
                        exp,
                        elapsedGameTime,
                        hungerMultiplier,
                        thirstMultiplier,
                        marketSearchText,
                        marketSearchActive,
                        marketScrollOffset,
                        playerGoldTyping,
                        p2pGoldTyping,
                        selectionDone,
                        introActive,
                        currentIntroFrame,
                        introFrames,
                        introSprite,
                        tutorialActive,
                        tutorialStep,
                        tutorialSteps,
                        tutorialBox,
                        tutMoveDist,
                        tutSprinted,
                        tutInvOpened,
                        tutWoodGathered,
                        tutStoneGathered,
                        tutAttacked,
                        tutChatted,
                        authManager,
                        latestPlayerMsg,
                        bubbleTimer,
                        p2pInvitePopup,
                        p2pTradeID,
                        p2pAmInitiator,
                        p2pPartner,
                        p2pState,
                        p2pMyReady,
                        p2pPartnerReady,
                        p2pOfferDirty,
                        p2pStatusMsg,
                        myP2PSlots,
                        myP2PGold,
                        myP2PGoldInput,
                        partnerItems,
                        partnerGold,
                        tradeBoxesTicked,
                        playerTradeSlots,
                        npcTradeSlots,
                        playerTradeGold,
                        npcTradeGold,
                        hasBargained,
                        bargainAccepted,
                        bargainStatus,
                        bargainColor,
                        inventory,
                        stockMarket,
                        isSellingToNPC,
                        npcGoldInput,
                        playerGoldInput,
                        getItemPrice,
                        getItemName,
                        adjustMarketItem,
                        quantityPromptOpen,
                        quantityPromptMax,
                        quantityPromptTargetSlot,
                        quantityPromptSourceSlot,
                        quantityPromptIsP2P,
                        quantityPromptIsCampfire,
                        quantityPromptCampfireSlot,
                        quantityPromptIsChest,
                        quantityPromptChestSlot,
                        quantityInput,
                        qInfo,
                        availableQuests,
                        p2pPollTimer,
                        assets,
                        chestMenuOpen,
                        activeChest,
                        chestSelectedInvSlot,
                        saplingPlacementMode,
                        saplingPlacementPos,
                        saplingRotation,
                        saplingFollowingCursor,
                        saplingFromInventorySlot,
                        saplingTex,
                        plantedSaplings,
                        snapToGrid,
                        placementItemID,
                        placementFromInventorySlot,
                        placementPos,
                        placementRotation,
                        placementFollowingCursor,
                        campfires,
                        structures,
                        furnaceTex,
                        woodTileTex,
                        woodWallTex,
                        woodDoorTex,
                        woodRoofTex,
                        woodStairsTex,
                        bedTex,
                        chestTex,
                        [&]() { syncSaveLocalStructuresAndCampfires(); },
                        craftableItems,
                        swordCrafting,
                        swordCraftTimer,
                        shieldCrafting,
                        shieldCraftTimer,
                        characterSelection,
                        option,
                        inventoryOpen,
                        campfireFrames,
                        maxHunger,
                        MAX_MULTIPLIER,
                        hp,
                        disasterActive,
                        currentDisasterType,
                        &isLoggingOut,
                        &logoutCloseWindow
                    );
                }
            }
        }

        if (isLoggingOut)
        {
            // 1. Immediately close all menus & UI popups
            wrenchMenuOpen = false;
            craftMenuOpen = false;
            tradeMenuOpen = false;
            tradeConfirmOpen = false;
            p2pTradeOpen = false;
            p2pInvitePopup = false;
            chestMenuOpen = false;
            activeChest = nullptr;
            stockPageOpen = false;
            adminSimOpen = false;
            iapPageOpen = false;
            iapVerifyingOpen = false;
            quantityPromptOpen = false;
            inventoryOpen = false;

            // 2. Stop audio immediately
            bgMusic.stop();
            nightMusic.stop();
            currentActiveMusic = nullptr;

            // 3. Snapshot game data on the main thread safely before background save
            for (int i = 0; i < 4; i++) {
                if (playerTradeSlots[i].tex != nullptr) {
                    inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                    playerTradeSlots[i] = {};
                }
                if (myP2PSlots[i].tex != nullptr) {
                    inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
                    myP2PSlots[i] = {};
                }
            }

            std::vector<std::pair<int, int>> saveItems(20, { 0, 0 });
            for (int i = 0; i < 20; i++) {
                if (inventory.hasItem(i)) {
                    saveItems[i] = { inventory.getItemID(i), inventory.getItemCount(i) };
                }
            }

            std::vector<StoredStructure> saveStructData;
            for (auto& s : structures) {
                if (s.sprite.has_value() && s.owner == username) {
                    saveStructData.push_back({ s.id, s.sprite->getPosition().x, s.sprite->getPosition().y, s.health, s.rotation, s.isOpen, s.shearY, s.owner });
                }
            }

            std::vector<StoredCampfire> saveCampData;
            std::vector<StoredCampfire> allSaveCampData;
            for (auto& c : campfires) {
                if (c.owner == username) {
                    saveCampData.push_back({
                        c.position.x,
                        c.position.y,
                        c.timeRemaining,
                        c.isCrafted,
                        c.fuel,
                        c.inputItem,
                        c.inputCount,
                        c.outputItem,
                        c.outputCount
                    });
                }
                allSaveCampData.push_back({
                    c.position.x,
                    c.position.y,
                    c.timeRemaining,
                    c.isCrafted,
                    c.fuel,
                    c.inputItem,
                    c.inputCount,
                    c.outputItem,
                    c.outputCount,
                    c.owner
                });
            }

            auto renderSavingFrame = [&](const std::string& statusMessage, int dotsCount) {
                while (const std::optional ev = window.pollEvent()) {
                    if (ev->is<sf::Event::Closed>()) {
                        logoutCloseWindow = true;
                    }
                }

                std::string dots(dotsCount, '.');
                window.setView(GetUIView(&window));
                window.clear(sf::Color(16, 14, 22));

                const float CARD_W = 620.f;
                const float CARD_H = 260.f;
                float cx = (float)width / 2.f;
                float cy = (float)height / 2.f;

                // Card shadow
                sf::RectangleShape cardGlow({ CARD_W + 8.f, CARD_H + 8.f });
                cardGlow.setOrigin({ (CARD_W + 8.f) / 2.f, (CARD_H + 8.f) / 2.f });
                cardGlow.setPosition({ cx, cy + 2.f });
                cardGlow.setFillColor(sf::Color(0, 0, 0, 110));
                window.draw(cardGlow);

                // Card body
                sf::RectangleShape card({ CARD_W, CARD_H });
                card.setOrigin({ CARD_W / 2.f, CARD_H / 2.f });
                card.setPosition({ cx, cy });
                card.setFillColor(sf::Color(26, 22, 34));
                card.setOutlineThickness(2.f);
                card.setOutlineColor(sf::Color(120, 90, 180));
                window.draw(card);

                // Header bar
                sf::RectangleShape cardHeader({ CARD_W, 44.f });
                cardHeader.setOrigin({ CARD_W / 2.f, CARD_H / 2.f });
                cardHeader.setPosition({ cx, cy - CARD_H / 2.f + 22.f });
                cardHeader.setFillColor(sf::Color(44, 34, 60));
                window.draw(cardHeader);

                // Title
                sf::Text titleTxt(uiFont, logoutCloseWindow ? "Exiting Game..." : "Logging Out...", 22);
                titleTxt.setStyle(sf::Text::Bold);
                titleTxt.setFillColor(sf::Color(255, 215, 0));
                auto tb = titleTxt.getLocalBounds();
                titleTxt.setOrigin({ tb.size.x / 2.f, tb.size.y / 2.f });
                titleTxt.setPosition({ cx, cy - CARD_H / 2.f + 20.f });
                window.draw(titleTxt);

                // Status text
                sf::Text statusTxt(uiFont, statusMessage + dots, 16);
                statusTxt.setFillColor(sf::Color(225, 225, 235));
                auto sb = statusTxt.getLocalBounds();
                statusTxt.setOrigin({ sb.size.x / 2.f, sb.size.y / 2.f });
                statusTxt.setPosition({ cx, cy - 25.f });
                window.draw(statusTxt);

                // Prominent warning container banner
                sf::RectangleShape warnBox({ 570.f, 44.f });
                warnBox.setOrigin({ 285.f, 22.f });
                warnBox.setPosition({ cx, cy + 30.f });
                warnBox.setFillColor(sf::Color(70, 30, 20, 220));
                warnBox.setOutlineThickness(1.5f);
                warnBox.setOutlineColor(sf::Color(235, 115, 50));
                window.draw(warnBox);

                // User-requested critical warning text:
                // "Do not turn off the game while saving, it will cause loss of data."
                sf::Text warnTxt(uiFont, "Do not turn off the game while saving, it will cause loss of data.", 14);
                warnTxt.setStyle(sf::Text::Bold);
                warnTxt.setFillColor(sf::Color(255, 210, 80));
                auto wb = warnTxt.getLocalBounds();
                warnTxt.setOrigin({ wb.size.x / 2.f, wb.size.y / 2.f });
                warnTxt.setPosition({ cx, cy + 30.f });
                window.draw(warnTxt);

                // Footer hint text
                sf::Text hintTxt(uiFont, logoutCloseWindow ? "Safely saving all progress to database..." : "Returning to Main Menu shortly...", 13);
                hintTxt.setFillColor(sf::Color(155, 155, 180));
                auto hb = hintTxt.getLocalBounds();
                hintTxt.setOrigin({ hb.size.x / 2.f, hb.size.y / 2.f });
                hintTxt.setPosition({ cx, cy + 85.f });
                window.draw(hintTxt);

                window.display();
            };

            // 4. Wait for any in-flight background futures while rendering saving UI with warning
            sf::Clock dotClock;
            int dotCount = 0;
            sf::Clock waitClock;
            while ((syncInFlight && syncFuture.valid()) ||
                   (chatFetchInFlight && chatFuture.valid()) ||
                   (recentTradesFetchInFlight && recentTradesFuture.valid()))
            {
                if (syncInFlight && syncFuture.valid() && syncFuture.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
                    try { (void)syncFuture.get(); } catch (...) {}
                    syncInFlight = false;
                }
                if (chatFetchInFlight && chatFuture.valid() && chatFuture.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
                    try { (void)chatFuture.get(); } catch (...) {}
                    chatFetchInFlight = false;
                }
                if (recentTradesFetchInFlight && recentTradesFuture.valid() && recentTradesFuture.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
                    try { (void)recentTradesFuture.get(); } catch (...) {}
                    recentTradesFetchInFlight = false;
                }

                if (dotClock.getElapsedTime().asSeconds() > 0.35f) {
                    dotCount = (dotCount + 1) % 4;
                    dotClock.restart();
                }

                renderSavingFrame("Finalizing network synchronization", dotCount);

                if (waitClock.getElapsedTime().asSeconds() > 2.0f) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            // 5. Start background save thread with snapshot data
            std::atomic<bool> saveDone{ false };
            std::atomic<bool> saveSuccess{ false };
            float currentElapsed = elapsedGameTime;
            int p2pTradeIDCopy = p2pTradeID;

            std::thread saveThread([&authManager, username, p2pTradeIDCopy, saveItems, hunger, thirst, currency, level, exp, currentElapsed, hungerMultiplier, thirstMultiplier, saveStructData, saveCampData, allSaveCampData, &saveDone, &saveSuccess]() {
                try {
                    if (p2pTradeIDCopy > 0) {
                        authManager.cancelP2PTrade(p2pTradeIDCopy);
                    }
                    authManager.saveInventory(username.c_str(), saveItems);
                    authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, currentElapsed, hungerMultiplier, thirstMultiplier);
                    authManager.saveStructures(username.c_str(), saveStructData);
                    authManager.saveCampfires(username.c_str(), saveCampData);
                    for (const auto& c : allSaveCampData) {
                        authManager.updateCampfireCookingState(c.x, c.y, c.fuel, c.inputItem, c.inputCount, c.outputItem, c.outputCount, c.timeRemaining);
                    }
                    authManager.clearPlayerChatMessages(username.c_str());
                    authManager.setPlayerOffline(username.c_str());
                    saveSuccess.store(true);
                } catch (const std::exception& e) {
                    std::cerr << "[LOGOUT] Save failed: " << e.what() << "\n";
                    saveSuccess.store(false);
                } catch (...) {
                    std::cerr << "[LOGOUT] Save failed with unknown error.\n";
                    saveSuccess.store(false);
                }
                saveDone.store(true);
            });

            // 6. Active 60 FPS event pump & animated saving screen with safety timeout
            sf::Clock totalSaveClock;
            while (!saveDone.load())
            {
                if (dotClock.getElapsedTime().asSeconds() > 0.35f) {
                    dotCount = (dotCount + 1) % 4;
                    dotClock.restart();
                }

                renderSavingFrame("Saving player progress and structures", dotCount);

                // Safety timeout: if cloud database is unresponsive after 8 seconds, don't hang indefinitely
                if (totalSaveClock.getElapsedTime().asSeconds() > 8.0f) {
                    std::cerr << "[LOGOUT] Save safety timeout (8s) triggered. Resuming transition.\n";
                    saveDone.store(true);
                    saveSuccess.store(false);
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            if (saveThread.joinable()) {
                if (totalSaveClock.getElapsedTime().asSeconds() > 8.0f) {
                    saveThread.detach();
                } else {
                    saveThread.join();
                }
            }

            // 7. Brief confirmation display
            sf::Clock confirmClock;
            while (confirmClock.getElapsedTime().asSeconds() < 0.35f)
            {
                while (const std::optional ev = window.pollEvent()) {
                    if (ev->is<sf::Event::Closed>()) logoutCloseWindow = true;
                }
                window.setView(GetUIView(&window));
                window.clear(sf::Color(16, 14, 22));

                const float CARD_W = 620.f;
                const float CARD_H = 260.f;
                float cx = (float)width / 2.f;
                float cy = (float)height / 2.f;

                sf::RectangleShape card({ CARD_W, CARD_H });
                card.setOrigin({ CARD_W / 2.f, CARD_H / 2.f });
                card.setPosition({ cx, cy });
                card.setFillColor(sf::Color(26, 22, 34));
                card.setOutlineThickness(2.f);
                card.setOutlineColor(saveSuccess.load() ? sf::Color(70, 160, 90) : sf::Color(200, 150, 60));
                window.draw(card);

                sf::Text titleTxt(uiFont, saveSuccess.load() ? "Saved Successfully!" : "Save Completed", 22);
                titleTxt.setStyle(sf::Text::Bold);
                titleTxt.setFillColor(saveSuccess.load() ? sf::Color(100, 240, 120) : sf::Color(240, 190, 80));
                auto tb = titleTxt.getLocalBounds();
                titleTxt.setOrigin({ tb.size.x / 2.f, tb.size.y / 2.f });
                titleTxt.setPosition({ cx, cy - 30.f });
                window.draw(titleTxt);

                sf::Text subTxt(uiFont, logoutCloseWindow ? "Closing safely..." : "Returning to Main Menu...", 15);
                subTxt.setFillColor(sf::Color(210, 215, 225));
                auto sb = subTxt.getLocalBounds();
                subTxt.setOrigin({ sb.size.x / 2.f, sb.size.y / 2.f });
                subTxt.setPosition({ cx, cy + 25.f });
                window.draw(subTxt);

                window.display();
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            // 8. If user clicked the window [X], close window now. Otherwise, leave window open for MainMenu!
            if (logoutCloseWindow) {
                window.close();
            }

            return;
        }

    if (selectionDone && !animLoadedForSelection)
    {
        if (!handleCharacterSelectionHydration(
            window,
            width,
            height,
            option,
            username,
            dbStartupEnabled,
            authManager,
            font,
            sprite,
            animWalk,
            animWalkWithSword,
            animWalkWithShield,
            animWalkWithShieldAndSword,
            animAttackWithSword,
            animAttackWithShieldAndSword,
            FRAMES_PER_DIR,
            stats,
            hunger,
            thirst,
            currency,
            level,
            exp,
            timeOffset,
            elapsedGameTime,
            hungerMultiplier,
            thirstMultiplier,
            hungerMultiplierText,
            thirstMultiplierText,
            structures,
            campfires,
            inventory,
            assets,
            snapToGrid,
            npcFemale,
            npcMale,
            MAP_WIDTH,
            MAP_HEIGHT,
            texMaleOnline,
            texFemaleOnline
        )) {
            return;
        }
        animLoadedForSelection = true;
    }


    if (selectionDone && musicLoaded && !musicStarted)
    {
        bgMusic.setVolume(globalSettings.volume);
        currentActiveMusic = &bgMusic;
        bgMusic.play();
        musicStarted = true;
    }

    if (!selectionDone)
    {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
        characterSelection.updateHover(mousePos);

        window.clear(sf::Color(20, 20, 20));
        window.setView(window.getDefaultView());
        sf::Vector2u winSize = window.getSize();
        if (selBgSprite.getLocalBounds().size.x > 0 && selBgSprite.getLocalBounds().size.y > 0) {
            selBgSprite.setScale({
                (float)winSize.x / selBgSprite.getLocalBounds().size.x,
                (float)winSize.y / selBgSprite.getLocalBounds().size.y
            });
            selBgSprite.setPosition({ 0.f, 0.f });
            window.draw(selBgSprite);
        }
        window.setView(GetUIView(&window));
        characterSelection.draw(window);

        // Old campfire drawing logic removed

        if (!tracedFirstDisplay) {
            traceStartup("before_first_display");
            tracedFirstDisplay = true;
        }
        window.display();
        if (!tracedAfterFirstDisplay) {
            traceStartup("after_first_display");
            tracedAfterFirstDisplay = true;
        }
        continue;
    }

    if (!isDead && !introActive && !isMenuOpen && !spawnOverlayActive)
    {
        if (tutorialActive && tutorialStep == 5 && !tutAttacked) {
            // Ensure some targets are nearby
            float minDist = 99999.f;
            for (auto& a : animals) {
                float d = std::sqrt(std::pow(a->sprite.getPosition().x - sprite.getPosition().x, 2) + std::pow(a->sprite.getPosition().y - sprite.getPosition().y, 2));
                if (d < minDist) minDist = d;
            }
            for (auto& m : monsters) {
                float d = std::sqrt(std::pow(m->sprite.getPosition().x - sprite.getPosition().x, 2) + std::pow(m->sprite.getPosition().y - sprite.getPosition().y, 2));
                if (d < minDist) minDist = d;
            }

            if (minDist > 1000.f && animals.size() < 6) {
                // Spawn target chickens spaced apart around player (max 6 animals total)
                for (int i = 0; i < 2 && animals.size() < 6; i++) {
                    float angle = (i == 0) ? 0.f : 3.14159f;
                    sf::Vector2f p = sprite.getPosition() + sf::Vector2f(std::cos(angle) * 350.f, std::sin(angle) * 350.f);
                    animals.push_back(std::make_unique<Chicken>(p));
                }
            }
        }

        const float TILE_CONNECT_DIST = 260.f;
        std::vector<TileFloor> elevatedTileFloors = computeElevatedTileFloors(structures);

        auto isBlocked = [&](sf::Vector2f targetPos) {
            return isPositionBlocked(targetPos, structures, elevatedTileFloors, itemManager);
        };

        updateMonstersCombat(
            monsters,
            sprite,
            dt,
            structures,
            elevatedTileFloors,
            itemManager,
            introActive,
            tutorialActive,
            shieldEquipped,
            eqShieldSlot,
            inventory,
            hp,
            isPlayerHurt,
            playerHurtTimer,
            MAP_WIDTH,
            MAP_HEIGHT
        );

        handlePlayerMovementAndCombat(
            animWalk,
            animWalkWithSword,
            animWalkWithShield,
            animWalkWithShieldAndSword,
            animAttackWithSword,
            animAttackWithShieldAndSword,
            swordEquipped,
            shieldEquipped,
            eqSwordSlot,
            playerAttackCooldown,
            dt,
            globalSettings,
            isSleeping,
            sprite,
            currentDir,
            isBlocked,
            tutorialActive,
            tutorialStep,
            tutMoveDist,
            tutSprinted,
            tutLastPos,
            tutAttacked,
            tutWoodGathered,
            tutStoneGathered,
            animals,
            itemManager,
            monsters,
            structures,
            inventory,
            exp,
            username,
            &campfires
        );

        updateCampfiresAndCooking(campfires, structures, dt, [&]() { syncSaveLocalStructuresAndCampfires(); });

        updateSaplings(plantedSaplings, itemManager, treeTex, dt, structures, campfires);

        UpdateCamera(window, sprite, MAP_WIDTH, MAP_HEIGHT);


        if (!pondBoundsList.empty()) {
            sf::FloatRect pb = pondBoundsList[0];
            float centerX = pb.position.x + pb.size.x * 0.5f;
            float centerY = pb.position.y + pb.size.y * 0.5f;
        }

        bool isDrought = (disasterActive && currentDisasterType == "drought");
        nearPond = isDrought ? false : isInWater(sprite.getPosition(), pondBoundsList);

        window.clear(sf::Color::White);
        if ((debugFrameIndex % 300ull) == 0ull) traceStartup("hb_after_main_clear");
        window.draw(background);

        drawFloorGrid(window, MAP_WIDTH, MAP_HEIGHT);
        drawSecondFloorGrid(window, structures, playerFloorLevel);

        if (!isDrought) {
            for (auto& p : ponds) {
                window.draw(p);
            }
        }
        itemManager.draw(window);

        drawWorldCampfires(window, font, campfires, campfireFrames, wrenchTex, sprite, inventory);

        updateAndDrawPlacementPreview(
            window,
            placementMode,
            placementFollowingCursor,
            placementItemID,
            placementRotation,
            placementPos,
            worldPos,
            structures,
            snapToGrid,
            campfireFrames,
            furnaceTex,
            woodTileTex,
            woodWallTex,
            woodDoorTex,
            woodRoofTex,
            woodStairsTex,
            bedTex,
            chestTex
        );

        // --- HOUSE TRANSPARENCY & STAIRS / MULTI-FLOOR LOGIC ---
        renderWorldDepthScene(
            window,
            sprite,
            structures,
            elevatedTileFloors,
            playerFloorLevel,
            saplingPlacementMode,
            saplingFollowingCursor,
            saplingPlacementPos,
            saplingTex,
            plantedSaplings,
            font,
            isPlayerHurt,
            playerHurtTimer,
            dt,
            animals,
            monsters,
            animLoadedForSelection,
            npcFemale,
            npcMale,
            username,
            playerWrenchBtn,
            bubbleTimer,
            latestPlayerMsg,
            snapToGrid,
            stairOffset
        );

        drawWorldInteractions(
            window,
            font,
            sprite,
            playerFloorLevel,
            isSleeping,
            chestMenuOpen,
            structures,
            nearPond,
            vitalsUI.drinkText,
            wrenchTex,
            inventory
        );

        updateWorldEntitiesAndDrops(
            animals,
            monsters,
            structures,
            itemManager,
            sprite,
            dt,
            isBlocked,
            MAP_WIDTH,
            MAP_HEIGHT,
            woolTex,
            woodTex,
            saplingTex,
            stoneTex,
            coalTex,
            copperOreTex,
            ironOreTex,
            goldOreTex,
            diamondTex,
            tutorialActive,
            tutorialStep,
            tutStoneGathered,
            qInfo,
            authManager,
            username,
            syncSaveLocalStructuresAndCampfires,
            inventory,
            nearPond,
            thirst,
            thirstMultiplier,
            maxThirst,
            campfires,
            snapToGrid,
            globalSettings,
            font,
            window
        );

        syncAndRenderOnlinePlayers(
            window,
            authManager,
            dbStartupEnabled,
            username,
            sprite,
            dt,
            syncFuture,
            syncInFlight,
            otherPlayers,
            onlinePlayerTargets,
            onlinePlayerDisplayPos,
            onlinePlayerWrenchBounds,
            structures,
            campfires,
            serverEpochCache,
            texFemaleOnline,
            texMaleOnline,
            wrenchTex,
            furnaceTex,
            woodTileTex,
            woodWallTex,
            woodDoorTex,
            woodRoofTex,
            woodStairsTex,
            bedTex,
            chestTex,
            uiFont,
            latestPlayerMsg,
            bubbleTimer,
            p2pTradeID,
            p2pTradeOpen,
            p2pInvitePopup,
            p2pAmInitiator,
            p2pState,
            p2pPartner,
            p2pStatusMsg,
            snapToGrid,
            &selfBanInfo,
            &localMuteInfo,
            &serverDisasterInfo,
            &serverDisasterUpdated
        );

        // Immediate kick if banned online
        if (selfBanInfo.isBanned) {
            std::cout << "[BAN KICK] Player " << username << " has been banned while online. Disconnecting immediately.\n";
            if (outBanKick) {
                *outBanKick = selfBanInfo;
            }
            break;
        }

        // Keep chat input mute status up to date
        chatInput.setMuted(localMuteInfo.isMuted);

        // Process server disaster sync
        if (serverDisasterUpdated) {
            serverDisasterUpdated = false;
            if (serverDisasterInfo.active && serverDisasterInfo.remainingSeconds > 0.f) {
                if (!disasterActive || currentDisasterType != serverDisasterInfo.type) {
                    currentDisasterType = serverDisasterInfo.type;
                    disasterActive = true;
                    disasterRemaining = serverDisasterInfo.remainingSeconds;
                    activeScheduledEventID = serverDisasterInfo.eventID;
                    disasterBannerTimer = 7.f;
                    if (currentDisasterType == "drought") {
                        disasterBannerMessage = "NATURAL DISASTER: Severe Drought strikes! Ponds dried up! Flora & Fauna withering!";
                        auto& dItems = itemManager.getItems();
                        std::vector<Item> keptItems;
                        keptItems.reserve(dItems.size());
                        for (auto& it : dItems) {
                            if (it.id == ITEM_TREE) { if ((rand() % 100) < 65) continue; }
                            keptItems.push_back(it);
                        }
                        dItems = std::move(keptItems);
                        itemManager.rebuildSpatialGrid();
                        std::vector<std::unique_ptr<Animal>> keptAnimals;
                        for (auto& a : animals)
                            if ((rand() % 100) < 70) keptAnimals.push_back(std::move(a));
                        animals = std::move(keptAnimals);
                        applyDroughtMarketImpact(stockMarket, authManager);
                    } else {
                        disasterBannerMessage = "NATURAL DISASTER: Acute Diarrhea strikes! Raw food POISONOUS! Cooked food in demand!";
                        applyDisasterFoodMarketImpact(stockMarket, authManager);
                    }
                    inventory.showBanner("[DISASTER] " + currentDisasterType + " activated! (" + std::to_string((int)disasterRemaining) + "s)");
                } else {
                    if (std::abs(disasterRemaining - serverDisasterInfo.remainingSeconds) > 2.0f) {
                        disasterRemaining = serverDisasterInfo.remainingSeconds;
                    }
                }
            } else {
                if (disasterActive) {
                    disasterActive = false;
                    disasterRemaining = 0.f;
                    disasterBannerTimer = 5.f;
                    if (currentDisasterType == "drought") {
                        disasterBannerMessage = "The Drought has ended. Rain has returned, ponds are refilled, and prices normalized.";
                        restoreDroughtMarketImpact(stockMarket, authManager);
                    } else {
                        disasterBannerMessage = "The Diarrhea outbreak has subsided. Meat prices have returned to normal.";
                        restoreDisasterFoodMarketImpact(stockMarket, authManager);
                    }
                    inventory.showBanner("Natural Disaster ended. Prices restored to normal.");
                    activeScheduledEventID = -1;
                }
            }
        }
    }

    // Update Stock Market Prices only when market stats or admin simulator is opened (not in background when closed)
    if ((stockPageOpen || adminSimOpen) && stockUpdateClock.getElapsedTime().asSeconds() >= 1800.f) {
        stockUpdateClock.restart();
        updateStockMarketSimulation(stockMarket, authManager);
    }
        updateDayNightCycleAndMusic(
            window,
            serverEpochCache,
            sleepTimeOffset,
            elapsedGameTime,
            totalCycleTime,
            darkness,
            isNight,
            wasNight,
            monsters,
            bgMusic,
            nightMusic,
            nightMusicLoaded,
            globalSettings.volume,
            tutorialActive,
            gen,
            randX,
            randY
        );

        vitalsUI.draw(window, inventoryOpen, shieldEquipped, inventory);
        inventory.drawItemBanner(window);


    renderAllGameUIs(
        window,
        uiFont,
        font,
        width,
        height,
        globalSettings,
        wrenchMenuOpen,
        wrenchMenuPos,
        isTargetNPC,
        wrenchTargetPlayer,
        p2pInvitePopup,
        p2pTradeID,
        p2pState,
        p2pTradeOpen,
        p2pPartner,
        myP2PSlots,
        myP2PGoldInput,
        p2pGoldTyping,
        p2pMyReady,
        partnerItems,
        partnerGold,
        p2pPartnerReady,
        p2pStatusMsg,
        craftMenuOpen,
        craftableItems,
        inventory,
        woodTex,
        stoneTex,
        woolTex,
        saplingTex,
        cookedChickenTex,
        cookedPigTex,
        cookedSheepTex,
        woodenSwordTex,
        shieldTex,
        furnaceTex,
        woodTileTex,
        woodWallTex,
        woodDoorTex,
        woodRoofTex,
        woodStairsTex,
        bedTex,
        swordCrafting,
        shieldCrafting,
        placementMode,
        saplingPlacementMode,
        saplingRotation,
        tradeMenuOpen,
        playerTradeSlots,
        npcTradeSlots,
        isSellingToNPC,
        npcTradeGold,
        npcGoldInput,
        playerGoldInput,
        playerGoldTyping,
        currency,
        hasBargained,
        bargainAccepted,
        bargainStatus,
        bargainColor,
        tradeConfirmOpen,
        tradeConfirmMessage,
        stockPageOpen,
        dbStartupEnabled,
        recentTradesFetchInFlight,
        recentTradesClock,
        recentTradesFuture,
        cachedRecentTrades,
        stockMarket,
        marketSearchActive,
        marketSearchText,
        marketCursorBlink,
        marketScrollOffset,
        iapPageOpen,
        iapVerifyingOpen,
        iapVerifyingTier,
        iapVerifyingGold,
        iapVerifyingPrice,
        iapCodeInput,
        isDead,
        elapsedGameTime,
        timeOffset,
        totalCycleTime,
        selectionDone,
        chatActive,
        loginMessageIdOffset,
        chatFetchInFlight,
        chatFetchClock,
        chatFuture,
        chatMessages,
        chatBoxX, chatBoxY, chatBoxWidth, chatBoxHeight,
        chatScroll, isChatting, chatInput,
        goldTexLoaded, goldSprite,
        introActive, introSprite, currentIntroFrame, introTexts,
        tutorialActive, tutorialOverlay, tutorialBox,
        tutorialStep, tutorialSteps,
        tutMoveDist, tutSprinted, tutInvOpened, tutWoodGathered, tutStoneGathered, tutAttacked, tutChatted,
        spawnOverlayActive, spawnOverlayDone, spawnOverlayTotal,
        chestMenuOpen,
        activeChest,
        chestSelectedInvSlot,
        campfires,
        structures,
        quantityPromptOpen, quantityPromptIsCampfire, quantityPromptCampfireSlot, quantityPromptMax, quantityInput,
        quantityPromptIsChest, quantityPromptChestSlot, quantityPromptSourceSlot,
        isSleeping, sleepFadeTimer, sleepFadeDuration,
        getItemTexture,
        getItemName,
        getItemPrice,
        disasterActive,
        disasterRemaining,
        disasterBannerTimer,
        disasterBannerMessage,
        isAdmin,
        currentDisasterType
    );

    if (adminSimOpen) {
        MarketTextures simTex;
        simTex.woodTex = &woodTex;
        simTex.stoneTex = &stoneTex;
        simTex.saplingTex = &saplingTex;
        simTex.cookedChickenTex = &cookedChickenTex;
        simTex.cookedPigTex = &cookedPigTex;
        simTex.cookedSheepTex = &cookedSheepTex;
        simTex.woolTex = &woolTex;
        simTex.woodenSwordTex = &woodenSwordTex;
        simTex.shieldTex = &shieldTex;
        simTex.furnaceTex = &furnaceTex;
        simTex.woodTileTex = &woodTileTex;
        simTex.woodWallTex = &woodWallTex;
        simTex.woodDoorTex = &woodDoorTex;
        simTex.woodRoofTex = &woodRoofTex;
        simTex.woodStairsTex = &woodStairsTex;
        simTex.bedTex = &bedTex;
        simTex.coalTex = &coalTex;
        simTex.copperOreTex = &copperOreTex;
        simTex.copperBarTex = &copperBarTex;
        simTex.ironOreTex = &ironOreTex;
        simTex.ironBarTex = &ironBarTex;
        simTex.goldOreTex = &goldOreTex;
        simTex.goldBarTex = &goldBarTex;
        simTex.diamondTex = &diamondTex;

        drawAdminQuantitySimulator(
            window,
            uiFont,
            width,
            height,
            stockMarket,
            adminSimScrollOffset,
            simTex
        );
    }

    if (adminPromptOpen) {
        drawAdminActionPrompt(
            window,
            uiFont,
            width,
            height,
            adminPromptIsBan,
            adminPromptTarget,
            adminPromptDuration,
            adminPromptMuteMinutes,
            adminReasonInput
        );
    }

    drawBrightnessOverlay(window, width, height, static_cast<float>(globalSettings.brightness));
    window.display();

    // Spawning moved into the main loop (incremental spawnPlan) to avoid long stalls.
    }

    // When player leaves the game, do not save their chat messages/history in the server/online database
    authManager.clearPlayerChatMessages(username.c_str());

    bgMusic.stop();
    nightMusic.stop();
    currentActiveMusic = nullptr;
}

