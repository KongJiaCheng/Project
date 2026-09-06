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

void gameLoop(sf::RenderWindow& window, unsigned int width, unsigned int height, const std::string& username, bool isFirstTimeFlag)
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
    bool hasBargained = false;
    bool bargainAccepted = false;
    std::string bargainStatus = "";
    sf::Color bargainColor = sf::Color::White;

    bool craftMenuOpen = false;
    bool placementMode = false;
    float placementRotation = 0.f; // 0: Vertical, 90: Horizontal
    int playerFloorLevel = 0;
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
    bool amountPopupOpen = false;
    int amountPopupSlot = -1;
    int amountSelected = 1;
    int amountMax = 1;
    std::vector<PlacedCampfire> campfires;

    auto syncSaveLocalStructuresAndCampfires = [&]() {
        syncSaveStructuresAndCampfires(authManager, username, dbStartupEnabled, structures, campfires);
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
    bool iapPageOpen = false;
    bool iapVerifyingOpen = false;
    int iapVerifyingGold = 0;
    float iapVerifyingPrice = 0.f;
    std::string iapVerifyingTier = "";
    sf::Clock stockUpdateClock;

    std::vector<StockItem> stockMarket;
    initStockMarket(stockMarket, timedLoadMarketPrice);

    auto getItemPrice = [&](int id) -> int {
        return getMarketItemPrice(id, stockMarket);
    };

    auto getItemName = [&](int id) -> std::string {
        return getMarketItemName(id, stockMarket);
    };

    auto adjustMarketItem = [&](int itemId, float percentChange) {
        ::adjustMarketItem(stockMarket, itemId, percentChange, authManager);
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
    TextInput quantityInput(font, { (width - 200.f) / 2.f, (height - 40.f) / 2.f + 10.f }, { 200.f, 40.f });
    bool isChatting = false;
    bool chatActive = false;
    std::vector<std::pair<std::string, std::string>> chatMessages;
    sf::Clock chatFetchClock;
    std::string latestPlayerMsg = "";
    float bubbleTimer = 0.f;
    float chatScroll = 0.f;
    bool isDraggingChat = false;
    sf::Vector2f lastMousePosChat;

    // --- Session-based chat: only show messages sent after login ---
    int loginMessageIdOffset = timedGetMaxMessageID();
    traceStartup("after_chat_offset");

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
    tutorialOverlay.setFillColor(sf::Color(0, 0, 0, 90));

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

    const int MAX_ANIMALS = 40;
    const int MAX_MONSTERS = 15;

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

    while (window.isOpen())
    {
        ++debugFrameIndex;
        if (!tracedLoopEntered) {
            traceStartup("loop_entered");
            tracedLoopEntered = true;
        }
        dt = deltaClock.restart().asSeconds();
        dt = std::min(dt, 0.016f);
        if (selectionDone && !spawnOverlayActive && !craftMenuOpen && !p2pTradeOpen) {
            if (isSleeping) {
                if (!sleepTimeSkipped) {
                    sleepFadeTimer += dt;
                    if (sleepFadeTimer >= sleepFadeDuration) {
                        sleepFadeTimer = sleepFadeDuration;
                        elapsedGameTime = (std::floor(elapsedGameTime / totalCycleTime) + 1.f) * totalCycleTime;
                        hp = maxHP;
                        authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
                        monsters.clear();
                        sleepTimeSkipped = true;
                    }
                } else {
                    sleepFadeTimer -= dt;
                    if (sleepFadeTimer <= 0.f) {
                        sleepFadeTimer = 0.f;
                        isSleeping = false;
                        sleepTimeSkipped = false;
                    }
                }
            } else {
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
        // DESPAWN FAR ANIMALS / MONSTERS & SPAWN NEAR CAMERA (ONLY IF NOT IN LOADING SCREEN)
        // =====================================================
        // DESPAWN / SPAWN ENTITIES (ONLY IF NOT IN LOADING SCREEN)
        // =====================================================
        if (!spawnOverlayActive)
        {
            despawnFarEntities(animals, monsters, playerPos, DESPAWN_RADIUS);
            spawnNearbyAnimals(animals, playerPos, mobSpawnClock, MAX_ANIMALS, SPAWN_RADIUS, MAP_WIDTH, MAP_HEIGHT);
            bool isNight = darkness.getFillColor().a > 150;
            spawnNightZombies(monsters, playerPos, monsterSpawnClock, MAX_MONSTERS, SPAWN_RADIUS, MAP_WIDTH, MAP_HEIGHT, isNight);
        }


        auto isBlocked = [&](sf::Vector2f pos) {
            return isWorldPositionBlocked(pos, structures, ponds);
        };

        // Update nearby animals and monsters
        if (!spawnOverlayActive)
        {
            updateNearbyEntities(animals, monsters, playerPos, dt, isBlocked, 1800.f);
        }

        UpdateCamera(window, sprite, MAP_WIDTH, MAP_HEIGHT);
        sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        bubbleTimer = std::max(0.f, bubbleTimer - dt);

        bool anyCampfireMenuOpen = false;
        for (auto& fire : campfires) {
            if (fire.menuOpen) {
                anyCampfireMenuOpen = true;
                break;
            }
        }
        bool isMenuOpen = (stockPageOpen || iapPageOpen || iapVerifyingOpen || tradeMenuOpen || tradeConfirmOpen || craftMenuOpen || p2pTradeOpen || anyCampfireMenuOpen || chestMenuOpen);

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
            authManager
        );

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

        checkAndPerformPeriodicAutoSave(
            sprite,
            lastSaveCameraPos,
            saveCooldownClock,
            inventory,
            authManager,
            username,
            hunger,
            thirst,
            currency,
            level,
            exp,
            elapsedGameTime,
            hungerMultiplier,
            thirstMultiplier,
            structures,
            campfires
        );

        if (hp <= 0.f && !isDead)
        {
            isDead = true;
            deathClock.restart();
        }

        if (inventoryOpen)
        {
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
                syncSaveLocalStructuresAndCampfires
            )) {
                continue;
            }
            bool clickHandled = false;
            if (isChatting) {
                chatInput.processEvent(event.value(), window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));
            }
            if (iapVerifyingOpen) {
                iapCodeInput.processEvent(event.value(), window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));
            }            if (handleKeyAndTextInputs(
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
                npcGoldTyping
            )) {
                continue;
            }

            handleMouseDragAndScroll(
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
                username
            );

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
                campfires
            )) {
                continue;
            }

            if (auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouse->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f uiPos = window.mapPixelToCoords(mouse->position, GetUIView(&window));
                    handleAllGameMouseButtonPressed(
                        uiPos,
                        worldPos,
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
                        quantityInput,
                        qInfo,
                        availableQuests,
                        p2pPollTimer,
                        assets,
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
                        hp
                    );
                }
            }
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
        bgMusic.play();
        musicStarted = true;
    }

    if (!selectionDone)
    {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
        characterSelection.updateHover(mousePos);

        window.clear(sf::Color::White);
        window.setView(GetUIView(&window));
        window.draw(selBgSprite);
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

            if (minDist > 1000.f) {
                // Spawn some targets right in front of player
                for (int i = 0; i < 3; i++) {
                    sf::Vector2f p = sprite.getPosition() + sf::Vector2f((rand() % 400 - 200), (rand() % 400 - 200));
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
            animals,
            itemManager,
            monsters,
            structures,
            inventory,
            exp
        );

        updateCampfiresAndCooking(campfires, dt, [&]() { syncSaveLocalStructuresAndCampfires(); });

        updateSaplings(plantedSaplings, itemManager, treeTex, dt);

        UpdateCamera(window, sprite, MAP_WIDTH, MAP_HEIGHT);


        if (!pondBoundsList.empty()) {
            sf::FloatRect pb = pondBoundsList[0];
            float centerX = pb.position.x + pb.size.x * 0.5f;
            float centerY = pb.position.y + pb.size.y * 0.5f;
        }

        nearPond = isInWater(sprite.getPosition(), pondBoundsList);

        window.clear(sf::Color::White);
        if ((debugFrameIndex % 300ull) == 0ull) traceStartup("hb_after_main_clear");
        window.draw(background);

        drawFloorGrid(window, MAP_WIDTH, MAP_HEIGHT);

        for (auto& p : ponds) {
            window.draw(p);
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
            snapToGrid
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
            vitalsUI.drinkText
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
            snapToGrid
        );
    }

    // Update Stock Market Prices every 60 seconds
    if (stockUpdateClock.getElapsedTime().asSeconds() >= 60.f) {
        stockUpdateClock.restart();
        updateStockMarketSimulation(stockMarket, authManager);
    }
        updateDayNightCycleAndMusic(
            window,
            serverEpochCache,
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
        campfires,
        quantityPromptOpen, quantityPromptIsCampfire, quantityPromptCampfireSlot, quantityPromptMax, quantityInput,
        isSleeping, sleepFadeTimer, sleepFadeDuration,
        getItemTexture,
        getItemName,
        getItemPrice
    );

    if ((debugFrameIndex % 300ull) == 0ull) traceStartup("hb_before_final_display");
    traceStartup("before_final_display");
    window.display();
    traceStartup("after_final_display");

    // Spawning moved into the main loop (incremental spawnPlan) to avoid long stalls.
    }
}

