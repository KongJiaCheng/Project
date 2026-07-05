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
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <algorithm>
#include <cmath>
#include <queue>
#include <map>
#include <iostream>
#include <optional>

const float GRID_STEP_X = 200.f;
const float GRID_STEP_Y = 100.f;
const float GLOBAL_SHEAR = -0.5f;

inline sf::Vector2f snapToShearedGrid(sf::Vector2f pos) {
    float gx = std::round(pos.x / GRID_STEP_X);
    float gy = std::round((pos.y - pos.x * GLOBAL_SHEAR) / GRID_STEP_Y);
    pos.x = gx * GRID_STEP_X;
    pos.y = gx * GRID_STEP_X * GLOBAL_SHEAR + gy * GRID_STEP_Y;
    return pos;
}

inline float distanceToSegment(sf::Vector2f P, sf::Vector2f A, sf::Vector2f B) {
    float l2 = std::pow(A.x - B.x, 2) + std::pow(A.y - B.y, 2);
    if (l2 == 0.f) return std::sqrt(std::pow(P.x - A.x, 2) + std::pow(P.y - A.y, 2));
    float t = ((P.x - A.x) * (B.x - A.x) + (P.y - A.y) * (B.y - A.y)) / l2;
    t = std::max(0.f, std::min(1.f, t));
    sf::Vector2f projection = A + t * (B - A);
    return std::sqrt(std::pow(P.x - projection.x, 2) + std::pow(P.y - projection.y, 2));
}

inline int getFloorLevelFromY(float y) {
    int iy = (int)std::round(y);
    for (int L = 0; L <= 10; ++L) {
        if ((iy + L * 180) % 100 == 0) {
            return L;
        }
    }
    return 0;
}
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

bool isInWater(const sf::Vector2f& pos, const std::vector<sf::FloatRect>& waters)
{
    for (const auto& water : waters) {
        if (water.contains(pos)) return true;
    }
    return false;
}



struct AnimSet {
    Animation south, north, east, west;
    void load(const std::string& basePath, int frames) {
        south.loadFromFolder(basePath + "/South", frames);
        north.loadFromFolder(basePath + "/North", frames);
        east.loadFromFolder(basePath + "/East", frames);
        west.loadFromFolder(basePath + "/West", frames);
    }
    void update(float dt) {
        south.update(dt); north.update(dt); east.update(dt); west.update(dt);
    }
    void stop() {
        south.stopAtFirst(); north.stopAtFirst(); east.stopAtFirst(); west.stopAtFirst();
    }
    void setPlaying(bool play) {
        south.playing = north.playing = east.playing = west.playing = play;
    }
    void setDirPlaying(int dir, bool play) {
        if (dir == 0) south.playing = play;
        else if (dir == 1) north.playing = play;
        else if (dir == 2) east.playing = play;
        else if (dir == 3) west.playing = play;
    }
    const sf::Texture& getTexture(int dir) {
        if (dir == 0) return south.getCurrentTexture();
        else if (dir == 1) return north.getCurrentTexture();
        else if (dir == 2) return east.getCurrentTexture();
        return west.getCurrentTexture();
    }
};

struct StockItem {
    int id;
    std::string name;
    float currentPrice;
    float basePrice;
    std::vector<float> history;
    float predictedPrice;
    std::string trend;
    float volatility; // Volatility multiplier for price updates (e.g. 0.3 for low, 2.0 for high)
};

struct TradeSlot { int id = 0; const sf::Texture* tex = nullptr; int count = 0; int originalSlot = -1; };

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

    auto timedLoadPlayerStats = [&](const std::string& uname) -> PlayerStats {
        if (!dbStartupEnabled) {
            return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };
        }
        struct SharedState {
            PlayerStats value{ 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };
            std::atomic<bool> done{ false };
            std::mutex mutex;
        };
        auto state = std::make_shared<SharedState>();
        std::thread([&authManager, state, uname]() {
            PlayerStats local = authManager.loadPlayerStats(uname.c_str());
            {
                std::lock_guard<std::mutex> lk(state->mutex);
                state->value = local;
            }
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < STARTUP_DB_TIMEOUT_SECS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] loadPlayerStats timed out. Using defaults.\n";
            return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };
        }
        std::lock_guard<std::mutex> lk(state->mutex);
        return state->value;
        };

    auto timedGetMaxMessageID = [&]() -> int {
        if (!dbStartupEnabled) {
            return 0;
        }
        struct SharedState {
            int value = 0;
            std::atomic<bool> done{ false };
        };
        auto state = std::make_shared<SharedState>();
        std::thread([&authManager, state]() {
            state->value = authManager.getMaxMessageID();
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < STARTUP_DB_TIMEOUT_SECS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] getMaxMessageID timed out. Using fallback 0.\n";
            return 0;
        }
        return state->value;
        };

    auto timedGetQuestInfo = [&](const std::string& uname) -> QuestInfo {
        if (!dbStartupEnabled) {
            return { 0, 0, "2000-01-01", false };
        }
        struct SharedState {
            QuestInfo value{ 0, 0, "2000-01-01", false };
            std::atomic<bool> done{ false };
            std::mutex mutex;
        };
        auto state = std::make_shared<SharedState>();
        std::thread([&authManager, state, uname]() {
            QuestInfo local = authManager.getQuestInfo(uname.c_str());
            {
                std::lock_guard<std::mutex> lk(state->mutex);
                state->value = local;
            }
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < STARTUP_DB_TIMEOUT_SECS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] getQuestInfo timed out. Using fallback quest info.\n";
            return { 0, 0, "2000-01-01", false };
        }
        std::lock_guard<std::mutex> lk(state->mutex);
        return state->value;
        };

    auto timedLoadMarketPrice = [&](int itemId, float fallback) -> float {
        if (!dbStartupEnabled) {
            return fallback;
        }
        struct SharedState {
            float value = 0.f;
            std::atomic<bool> done{ false };
        };
        auto state = std::make_shared<SharedState>();
        std::thread([&authManager, state, itemId]() {
            state->value = authManager.loadMarketPrice(itemId);
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < STARTUP_DB_TIMEOUT_SECS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] loadMarketPrice timed out for item " << itemId << ".\n";
            return fallback;
        }
        return state->value;
        };

    PlayerStats stats = timedLoadPlayerStats(username);
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
    Animation animation;

    const int FRAMES_PER_DIR = 6;
    AnimSet animWalk;
    AnimSet animWalkWithSword;
    AnimSet animWalkWithShield;
    AnimSet animWalkWithShieldAndSword;
    AnimSet animAttackWithSword;
    AnimSet animAttackWithShieldAndSword;
    bool animLoadedForSelection = false;

    sf::Font font;
    bool fontLoaded = font.openFromFile("C:/Windows/Fonts/arial.ttf");
    if (!fontLoaded) fontLoaded = font.openFromFile("C:/Windows/Fonts/calibri.ttf");
    if (!fontLoaded) fontLoaded = font.openFromFile("C:/Windows/Fonts/segoeui.ttf");
    if (!fontLoaded) fontLoaded = font.openFromFile("C:/Windows/Fonts/tahoma.ttf");
    traceStartup("after_font");
    sf::Font& uiFont = font;

    sf::Texture selBgTexture;
    selBgTexture.loadFromFile(resolvePath("MainMenu/Background.png"));
    sf::Sprite selBgSprite(selBgTexture);
    if (selBgSprite.getLocalBounds().size.x > 0 && selBgSprite.getLocalBounds().size.y > 0) {
        selBgSprite.setScale({
            static_cast<float>(window.getSize().x) / selBgSprite.getLocalBounds().size.x,
            static_cast<float>(window.getSize().y) / selBgSprite.getLocalBounds().size.y
            });
    }

    sf::Text Option(font, "Choose Your Character", 80);
    Option.setFillColor(sf::Color::White);
    Option.setOutlineColor(sf::Color::Black);
    Option.setOutlineThickness(3.f);
    Option.setPosition({ (width - Option.getLocalBounds().size.x) / 2.f, 50.f });

    sf::RectangleShape button1({ 600.f, 200.f });
    button1.setFillColor(sf::Color::Green);
    button1.setPosition({ (width - 600.f) / 2.f, (height - 200.f) / 2.f });

    sf::Text buttonText1(font, "Male", 120);
    buttonText1.setFillColor(sf::Color::White);
    buttonText1.setPosition(button1.getPosition());

    sf::RectangleShape button2({ 600.f, 200.f });
    button2.setFillColor(sf::Color::Green);
    button2.setPosition({ (width - 600.f) / 2.f, ((height - 200.f) / 2.f) + 400.f });

    sf::Text buttonText2(font, "Female", 120);
    buttonText2.setFillColor(sf::Color::White);
    buttonText2.setPosition(button2.getPosition());

    sf::Texture bgTexture;
    (void)bgTexture.loadFromFile(resolvePath("Items/Background.jpg"));
    traceStartup("after_background");

    sf::Sprite background(bgTexture);
    float scaleX = float(MAP_WIDTH) / bgTexture.getSize().x;
    float scaleY = float(MAP_HEIGHT) / bgTexture.getSize().y;
    background.setScale({ scaleX, scaleY });
    background.setPosition({ 0.f, 0.f });

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
    float playerAttackCooldown = 0.f;
    bool isPlayerHurt = false;
    const float ZOMBIE_ATTACK_COOLDOWN = 0.8f;
    const int ZOMBIE_DAMAGE = 10;

    int eqShieldSlot = -1;
    int eqSwordSlot = -1;

    sf::Texture texMaleOnline, texFemaleOnline;
    (void)texMaleOnline.loadFromFile(resolvePath("Sprites/Male/Walk/South/frame_000.png"));
    (void)texFemaleOnline.loadFromFile(resolvePath("Sprites/Female/Walk/South/frame_000.png"));

    sf::Texture texSouth;
    sf::Sprite sprite(texSouth);
    sprite.setOrigin({ 32.f, 32.f });
    sprite.setPosition({ MAP_WIDTH / 2.f, MAP_HEIGHT / 2.f });



    sf::Texture hpIconTex, hungerIconTex, thirstIconTex;
    (void)hpIconTex.loadFromFile(resolvePath("Sprites/UI/hp.png"));
    (void)hungerIconTex.loadFromFile(resolvePath("Sprites/UI/hunger.png"));
    (void)thirstIconTex.loadFromFile(resolvePath("Sprites/UI/thirst.png"));
    traceStartup("after_ui_icons");

    sf::Texture wrenchTex;
    (void)wrenchTex.loadFromFile(resolvePath("Sprites/UI/wrench.png"));

    struct NPC {
        std::optional<sf::Sprite> sprite;
        std::string name;
        sf::Texture texture;
        std::optional<sf::Sprite> wrenchBtn;
        NPC(const sf::Texture& tex) {
            sprite.emplace(tex);
            wrenchBtn.emplace(tex);
        }
    } npcFemale(wrenchTex), npcMale(wrenchTex);
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
    struct P2PItemDisplay { int id = 0; int count = 0; };
    std::vector<P2PItemDisplay> partnerItems;
    int   partnerGold        = 0;
    std::string wrenchTargetPlayer = ""; // username of online player clicked

    bool saplingPlacementMode = false;
    bool saplingFollowingCursor = true;
    sf::Vector2f saplingPlacementPos;
    int saplingFromInventorySlot = -1;
    float saplingRotation = 0.f;

    struct PlacedCampfire {
        sf::Vector2f position;
        float timeRemaining;
        bool isCrafted;
        int currentFrame;
        float frameTimer;

        int fuel = 0;

        int inputItem = 0;
        int inputCount = 0;

        int outputItem = 0;
        int outputCount = 0;

        bool menuOpen = false;
        std::string owner = "";  // empty=self, otherwise other player username
    };

    struct PlantedSapling {
        sf::Vector2f position;
        float growTimer;
        bool grown;
        float rotation = 0.f;
    };

    std::vector<PlantedSapling> plantedSaplings;
    struct PlacedStructure {
        int id;
        float health;
        float maxHealth;
        bool isHurt = false;
        float hurtTimer = 0.f;
        std::optional<sf::Sprite> sprite;
        float rotation = 0.f;
        bool isOpen = false;
        float shearY = 0.f;
        std::string owner = "";

        PlacedStructure(int i, const sf::Texture& tex, float rot = 0.f, bool open = false, float sh = 0.f, const std::string& own = "")
            : id(i), rotation(rot), isOpen(open), shearY(sh), owner(own) {
            sprite.emplace(tex);
            maxHealth = 100.f;
            if (id == ITEM_WOOD_WALL) maxHealth = 200.f;
            else if (id == ITEM_FURNACE) maxHealth = 150.f;
            else if (id == ITEM_BED) maxHealth = 120.f;
            health = maxHealth;
        }
    };
    std::vector<PlacedStructure> structures;
    std::vector<PlacedCampfire> campfires;

    auto syncSaveLocalStructuresAndCampfires = [&]() {
        if (!dbStartupEnabled) return;
        std::vector<StoredStructure> structData;
        for (auto& s : structures)
        {
            if (s.sprite.has_value() &&
                (s.owner == username || s.owner == ""))
            {
                structData.push_back({
                    s.id,
                    s.sprite->getPosition().x,
                    s.sprite->getPosition().y,
                    s.health,
                    s.rotation,
                    s.isOpen,
                    s.shearY
                    });
            }
        }
        authManager.saveStructures(username.c_str(), structData);

        std::vector<StoredCampfire> campData;
        for (auto& c : campfires)
        {
            if (c.owner == username || c.owner == "") {
                campData.push_back({
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
        }
        authManager.saveCampfires(username.c_str(), campData);
    };
    sf::Texture campfireFrames[8];
    for (int i = 0; i < 8; i++) {
        campfireFrames[i].loadFromFile(resolvePath("Items/campfire/frame_" + std::to_string(i) + "_campfire.png"));
    }
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

    std::vector<StockItem> stockMarket = {
        { ITEM_WOOD, "Wood", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.4f },
        { ITEM_STONE, "Stone", 8.f, 8.f, {8.f}, 8.f, "STABLE", 0.3f },
        { ITEM_SAPLING, "Sapling", 12.f, 12.f, {12.f}, 12.f, "STABLE", 0.6f },
        { ITEM_CHICKEN_MEAT, "Chicken Meat", 15.f, 15.f, {15.f}, 15.f, "STABLE", 1.8f },
        { ITEM_PIG_MEAT, "Pig Meat", 25.f, 25.f, {25.f}, 25.f, "STABLE", 0.5f },
        { ITEM_SHEEP_MEAT, "Sheep Meat", 20.f, 20.f, {20.f}, 20.f, "STABLE", 0.5f },
        { ITEM_WOOL, "Wool", 15.f, 15.f, {15.f}, 15.f, "STABLE", 0.5f },
        { ITEM_ZOMBIE_FLESH, "Zombie Flesh", 10.f, 10.f, {10.f}, 10.f, "STABLE", 2.5f },
        { ITEM_COOKED_CHICKEN, "Cooked Chicken", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f },
        { ITEM_COOKED_PIG, "Cooked Pig", 50.f, 50.f, {50.f}, 50.f, "STABLE", 0.5f },
        { ITEM_COOKED_SHEEP, "Cooked Sheep", 40.f, 40.f, {40.f}, 40.f, "STABLE", 0.5f },
        { ITEM_WOODEN_SWORD, "Wooden Sword", 55.f, 55.f, {55.f}, 55.f, "STABLE", 0.5f },
        { ITEM_SHIELD, "Sturdy Shield", 115.f, 115.f, {115.f}, 115.f, "STABLE", 0.4f },
        { ITEM_FURNACE, "Furnace", 84.f, 84.f, {84.f}, 84.f, "STABLE", 0.3f },
        { ITEM_WOOD_TILE, "Wood Tile", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.5f },
        { ITEM_WOOD_WALL, "Wood Wall", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.5f },
        { ITEM_WOOD_DOOR, "Wood Door", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f },
        { ITEM_WOOD_ROOF, "Wood Roof", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f },
        { ITEM_WOOD_STAIRS, "Wood Stairs", 48.f, 48.f, {48.f}, 48.f, "STABLE", 0.5f },
        { ITEM_BED, "Bed", 80.f, 80.f, {80.f}, 80.f, "STABLE", 0.6f }
    };

    auto getItemPrice = [&](int id) -> int {
        // If it's a raw material in stock market, get its current price
        for (auto& sItem : stockMarket) {
            if (sItem.id == id) {
                return (int)sItem.currentPrice;
            }
        }

        // Otherwise, base it on the current ingredient values!
        int woodPrice = 5;
        int stonePrice = 8;
        for (auto& sItem : stockMarket) {
            if (sItem.id == ITEM_WOOD) woodPrice = (int)sItem.currentPrice;
            if (sItem.id == ITEM_STONE) stonePrice = (int)sItem.currentPrice;
        }

        switch (id) {
        case ITEM_WOODEN_SWORD: // 8 Wood
            return woodPrice * 8 + 15;
        case ITEM_SHIELD: // 10 Wood + 5 Stone
            return woodPrice * 10 + stonePrice * 5 + 25;
        case ITEM_FURNACE: // 8 Stone
            return stonePrice * 8 + 20;
        case ITEM_WOOD_TILE: // 1 Wood
            return woodPrice * 1;
        case ITEM_WOOD_WALL: // 1 Wood
            return woodPrice * 1;
        case ITEM_WOOD_DOOR: // 5 Wood
            return woodPrice * 5 + 5;
        case ITEM_WOOD_ROOF: // 5 Wood
            return woodPrice * 5 + 5;
        case ITEM_WOOD_STAIRS: // 8 Wood
            return woodPrice * 8 + 8;
        case ITEM_BED: // 10 Wood + 5 Wool
        {
            int woolPrice = 15;
            for (auto& sItem : stockMarket) {
                if (sItem.id == ITEM_WOOL) woolPrice = (int)sItem.currentPrice;
            }
            return woodPrice * 10 + woolPrice * 5 + 30;
        }
        default:
            return 10; // Default fallback
        }
        };

    auto getItemName = [&](int id) -> std::string {
        for (auto& sItem : stockMarket) {
            if (sItem.id == id) return sItem.name;
        }
        switch (id) {
        case ITEM_WOODEN_SWORD: return "Wooden Sword";
        case ITEM_SHIELD: return "Sturdy Shield";
        case ITEM_FURNACE: return "Furnace";
        case ITEM_WOOD_TILE: return "Wood Tile";
        case ITEM_WOOD_WALL: return "Wood Wall";
        case ITEM_WOOD_DOOR: return "Wood Door";
        case ITEM_WOOD_ROOF: return "Wood Roof";
        case ITEM_WOOD_STAIRS: return "Wood Stairs";
        case ITEM_BED: return "Bed";
        case ITEM_CHICKEN_MEAT: return "Chicken Meat";
        case ITEM_PIG_MEAT: return "Pig Meat";
        case ITEM_SHEEP_MEAT: return "Sheep Meat";
        case ITEM_WOOL: return "Wool";
        case ITEM_COOKED_CHICKEN: return "Cooked Chicken";
        case ITEM_COOKED_PIG: return "Cooked Pig";
        case ITEM_COOKED_SHEEP: return "Cooked Sheep";
        default: return "Unknown Item";
        }
        };

    auto adjustMarketItem = [&](int itemId, float percentChange) {
        for (auto& item : stockMarket) {
            if (item.id == itemId) {
                // Save current price to history before updating
                if (item.history.size() >= 10) {
                    item.history.erase(item.history.begin());
                }
                item.history.push_back(item.currentPrice);

                // Apply price change
                item.currentPrice *= (1.f + percentChange);

                // Clamp to volatility-adjusted min/max bounds
                float minP = item.basePrice * (item.volatility > 1.2f ? 0.2f : 0.5f);
                float maxP = item.basePrice * (item.volatility > 1.2f ? 4.0f : 1.8f);
                if (item.currentPrice < minP) item.currentPrice = minP;
                if (item.currentPrice > maxP) item.currentPrice = maxP;
                item.currentPrice = std::round(item.currentPrice);
                if (item.currentPrice < 1.f) item.currentPrice = 1.f;

                // Save updated price to DB
                authManager.saveMarketPrice(item.id, item.currentPrice);

                // Recalculate predicted price and trend
                if (!item.history.empty()) {
                    float lastPrice = item.history.back();
                    float diff = item.currentPrice - lastPrice;
                    item.predictedPrice = std::round(item.currentPrice + diff * 0.5f);
                    if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;

                    float trendThreshold = item.volatility > 1.2f ? 0.04f : 0.02f;
                    if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) item.trend = "UP";
                    else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) item.trend = "DOWN";
                    else item.trend = "STABLE";
                }
                break;
            }
        }
    };



    // Prepopulate history for beautiful stock charts on start
    for (int step = 0; step < 8; ++step) {
        for (auto& item : stockMarket) {
            item.history.push_back(item.currentPrice);

            // Scaled volatility: unstable items fluctuate up to 37.5%, stable items up to 4.5%
            float baseRange = 15.f * item.volatility;
            int rangeInt = (int)(baseRange * 2.f) + 1;
            float percentChange = (-(int)baseRange + (rand() % (rangeInt > 0 ? rangeInt : 1))) / 100.f;

            float dev = (item.currentPrice - item.basePrice) / item.basePrice;
            percentChange -= dev * 0.05f;
            item.currentPrice = std::round(item.currentPrice * (1.f + percentChange));

            // Bound price based on volatility
            float minP = item.basePrice * (item.volatility > 1.2f ? 0.2f : 0.5f);
            float maxP = item.basePrice * (item.volatility > 1.2f ? 4.0f : 1.8f);
            if (item.currentPrice < minP) item.currentPrice = minP;
            if (item.currentPrice > maxP) item.currentPrice = maxP;

            item.currentPrice = std::round(item.currentPrice);
            if (item.currentPrice < 1.f) item.currentPrice = 1.f;
        }
    }

    for (auto& item : stockMarket)
    {
        float savedPrice =
            timedLoadMarketPrice(item.id, item.currentPrice);

        if (savedPrice > 0.f)
        {
            item.currentPrice = savedPrice;
        }
    }

    // Set initial predictions and trends based on prepopulated history
    for (auto& item : stockMarket) {
        if (item.history.size() >= 2) {
            float lastPrice = item.history.back();
            float diff = item.currentPrice - lastPrice;
            float baseNoise = 8.f * item.volatility;
            int noiseRange = (int)(baseNoise * 2.f) + 1;
            float noise = (-(int)baseNoise + (rand() % (noiseRange > 0 ? noiseRange : 1))) / 100.f;
            item.predictedPrice = std::round(item.currentPrice + diff * 0.5f + item.currentPrice * noise);
            if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;

            // Highly volatile items need a larger prediction gap to be labeled UP/DOWN
            float trendThreshold = item.volatility > 1.2f ? 0.08f : 0.04f;
            if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) {
                item.trend = "UP";
            }
            else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) {
                item.trend = "DOWN";
            }
            else {
                item.trend = "STABLE";
            }
        }
        else {
            item.predictedPrice = item.currentPrice;
            item.trend = "STABLE";
        }
    }

    sf::Sprite playerWrenchBtn(wrenchTex);
    playerWrenchBtn.setScale({ 0.05f, 0.05f });
    npcFemale.wrenchBtn->setTexture(wrenchTex);
    npcFemale.wrenchBtn->setScale({ 0.05f, 0.05f });
    npcMale.wrenchBtn->setTexture(wrenchTex);
    npcMale.wrenchBtn->setScale({ 0.05f, 0.05f });

    sf::Sprite hpIcon(hpIconTex);
    sf::Sprite hungerIcon(hungerIconTex);
    sf::Sprite thirstIcon(thirstIconTex);

    hpIcon.setScale({ 0.6f, 0.6f });
    hungerIcon.setScale({ 0.6f, 0.6f });
    thirstIcon.setScale({ 0.6f, 0.6f });

    sf::Text hpText(font, "", 16);
    sf::Text hungerText(font, "", 16);
    sf::Text thirstText(font, "", 16);

    sf::Text hungerMultiplierText(font, "", 22);
    sf::Text thirstMultiplierText(font, "", 22);

    hungerMultiplierText.setFillColor(sf::Color::Yellow);
    thirstMultiplierText.setFillColor(sf::Color::Cyan);

    hungerMultiplierText.setOutlineThickness(2.f);
    thirstMultiplierText.setOutlineThickness(2.f);

    hungerMultiplierText.setOutlineColor(sf::Color::Black);
    thirstMultiplierText.setOutlineColor(sf::Color::Black);
    sf::Text defText(font, "", 16);
    sf::Text levelText(font, "", 20);

    sf::Text drinkText(font, "Press E to drink water", 28);
    drinkText.setFillColor(sf::Color::Yellow);

    hpText.setFillColor(sf::Color::Black);
    hungerText.setFillColor(sf::Color::Black);
    thirstText.setFillColor(sf::Color::Black);
    hungerMultiplierText.setString("X" + std::to_string(hungerMultiplier));
    thirstMultiplierText.setString("X" + std::to_string(thirstMultiplier));
    defText.setFillColor(sf::Color::Black);
    levelText.setFillColor(sf::Color::White);
    levelText.setOutlineColor(sf::Color::Black);
    levelText.setOutlineThickness(1.f);

    sf::RectangleShape hpBack({ BAR_WIDTH, BAR_HEIGHT });
    hpBack.setFillColor(sf::Color(60, 60, 60));

    sf::RectangleShape hpBar({ BAR_WIDTH, BAR_HEIGHT });
    hpBar.setFillColor(sf::Color::Red);

    sf::RectangleShape hungerBack = hpBack;
    sf::RectangleShape hungerBar({ BAR_WIDTH, BAR_HEIGHT });
    hungerBar.setFillColor(sf::Color(255, 165, 0));

    sf::RectangleShape thirstBack = hpBack;
    sf::RectangleShape thirstBar({ BAR_WIDTH, BAR_HEIGHT });
    thirstBar.setFillColor(sf::Color(0, 150, 255));

    sf::RectangleShape defBack = hpBack;
    sf::RectangleShape defBar({ BAR_WIDTH, BAR_HEIGHT });
    defBar.setFillColor(sf::Color(100, 100, 255));

    sf::RectangleShape expBack = hpBack;
    sf::RectangleShape expBar({ BAR_WIDTH, BAR_HEIGHT });
    expBar.setFillColor(sf::Color::Green);

    hpBar.setSize({ BAR_WIDTH * (hp / maxHP), BAR_HEIGHT });
    hpBar.setPosition(hpBack.getPosition());
    hungerBar.setSize({ BAR_WIDTH * (1.f - hunger / maxHunger), BAR_HEIGHT });
    hungerBar.setPosition(hungerBack.getPosition());
    thirstBar.setSize({ BAR_WIDTH * (1.f - thirst / maxThirst), BAR_HEIGHT });
    thirstBar.setPosition(thirstBack.getPosition());

    ChickenMeat::loadTexture();
    PigMeat::loadTexture();
    ZombieFlesh::loadTexture();
    SheepMeat::loadTexture();
    CookedSheepMeat::loadTexture();
    (void)Chicken::texture.loadFromFile(resolvePath("Sprites/Animals/Chicken/South.png"));
    (void)Pig::texture.loadFromFile(resolvePath("Sprites/Animals/Pig/South.png"));
    (void)Sheep::texture.loadFromFile(resolvePath("Sprites/Animals/Sheep/Walk/South/frame_000.png"));
    (void)Zombie::texture.loadFromFile(resolvePath("Sprites/Monsters/Zombie/South.png"));

    sf::Texture woodTex, treeTex, stoneTex, pondTex, saplingTex, goldTex, woodenSwordTex, shieldTex, wrenchItemTex;
    sf::Texture furnaceTex, woodTileTex, woodWallTex, woodDoorTex, woodRoofTex, woodStairsTex, bedTex;
    sf::Texture swordIconTex, shieldIconTex, tileIconTex, wallIconTex, doorIconTex, roofIconTex, stairsIconTex, bedIconTex;

    sf::Texture cookedChickenTex;
    sf::Texture cookedPigTex;
    sf::Texture cookedZombieTex;
    sf::Texture cookedSheepTex;
    sf::Texture woolTex;

    (void)woodTex.loadFromFile(resolvePath("Items/wood.png"));
    (void)treeTex.loadFromFile(resolvePath("Items/Tree.png"));
    (void)stoneTex.loadFromFile(resolvePath("Items/Stone.png"));
    (void)pondTex.loadFromFile(resolvePath("Items/pond.png"));
    (void)saplingTex.loadFromFile(resolvePath("Items/sapling.png"));
    (void)goldTex.loadFromFile(resolvePath("Items/gold.png"));
    (void)woodenSwordTex.loadFromFile(resolvePath("Items/wooden sword.png"));
    (void)shieldTex.loadFromFile(resolvePath("Items/shield.png"));
    (void)furnaceTex.loadFromFile(resolvePath("Items/furnance.png"));
    (void)woodTileTex.loadFromFile(resolvePath("Items/wood tile.png"));
    (void)woodWallTex.loadFromFile(resolvePath("Items/wood wall.png"));
    (void)woodDoorTex.loadFromFile(resolvePath("Items/wooden door.png"));
    (void)woodRoofTex.loadFromFile(resolvePath("Items/wooden roof.png"));
    (void)woodStairsTex.loadFromFile(resolvePath("Items/wooden stairs.png"));
    (void)bedTex.loadFromFile(resolvePath("Items/bed.png"));
    (void)cookedChickenTex.loadFromFile(resolvePath("Items/cooked chicken meat.png"));
    (void)cookedPigTex.loadFromFile(resolvePath("Items/cooked pig meat.png"));
    (void)cookedSheepTex.loadFromFile(resolvePath("Items/cooked sheep meat.png"));
    (void)woolTex.loadFromFile(resolvePath("Items/wool.png"));
    (void)wrenchItemTex.loadFromFile(resolvePath("Sprites/UI/wrench.png"));

    // Load high quality icons
    (void)swordIconTex.loadFromFile(resolvePath("Items/wooden_sword_shield_icons.png"));
    (void)shieldIconTex.loadFromFile(resolvePath("Items/wooden_sword_shield_icons.png"));
    (void)tileIconTex.loadFromFile(resolvePath("Items/building_parts_icons.png"));
    (void)wallIconTex.loadFromFile(resolvePath("Items/building_parts_icons.png"));
    (void)doorIconTex.loadFromFile(resolvePath("Items/building_parts_icons.png"));
    (void)roofIconTex.loadFromFile(resolvePath("Items/building_parts_icons.png"));
    (void)stairsIconTex.loadFromFile(resolvePath("Items/building_parts_icons.png"));
    (void)bedIconTex.loadFromFile(resolvePath("Items/bed.png"));

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
        if (id == ITEM_WOOD) return &woodTex;
        if (id == ITEM_STONE) return &stoneTex;
        if (id == ITEM_SAPLING) return &saplingTex;
        if (id == ITEM_CHICKEN_MEAT) return ChickenMeat::getTexture();
        if (id == ITEM_PIG_MEAT) return PigMeat::getTexture();
        if (id == ITEM_SHEEP_MEAT) return SheepMeat::getTexture();
        if (id == ITEM_COOKED_SHEEP) return &cookedSheepTex;
        if (id == ITEM_COOKED_CHICKEN) return &cookedChickenTex;
        if (id == ITEM_COOKED_PIG) return &cookedPigTex;
        if (id == ITEM_WOOL) return &woolTex;
        if (id == ITEM_ZOMBIE_FLESH) return ZombieFlesh::getTexture();
        if (id == ITEM_WOODEN_SWORD) return &woodenSwordTex;
        if (id == ITEM_SHIELD) return &shieldTex;
        if (id == ITEM_FURNACE) return &furnaceTex;
        if (id == ITEM_BED) return &bedTex;
        if (id == ITEM_WOOD_TILE) return &woodTileTex;
        if (id == ITEM_WOOD_WALL) return &woodWallTex;
        if (id == ITEM_WOOD_DOOR) return &woodDoorTex;
        if (id == ITEM_WOOD_ROOF) return &woodRoofTex;
        if (id == ITEM_WOOD_STAIRS) return &woodStairsTex;
        return nullptr;
    };

    sf::Clock animalRespawnClock;

    sf::Clock deathClock;

    const float totalCycleTime = 900.f;

    sf::Clock deltaClock;
    sf::Clock dayNightClock;

    UpdateViewToWindow(&window);

    enum class Dir { South, North, East, West };
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
    float chatBoxX = (width - chatBoxWidth) / 2.f;
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
    std::vector<sf::Texture> introFrames;

    // Load entity textures
    Chicken::texture.loadFromFile(resolvePath("Sprites/Animals/Chicken/Walk/South/frame_000.png"));
    Pig::texture.loadFromFile(resolvePath("Sprites/Animals/Pig/Walk/South/frame_000.png"));
    Sheep::texture.loadFromFile(resolvePath("Sprites/Animals/Sheep/Walk/South/frame_000.png"));
    Zombie::texture.loadFromFile(resolvePath("Sprites/Monsters/Zombie/Walk/South/frame_000.png"));
    ChickenMeat::loadTexture();
    PigMeat::loadTexture();
    ZombieFlesh::loadTexture();

    // We will initialize introSprite later to avoid default constructor issues in SFML 3.x
    std::optional<sf::Sprite> introSprite;

    if (introActive) {
        // Load story frames
        for (int i = 0; i < 10; i++) { // Increased limit to 100 frames
            std::ostringstream ss;
            ss << "Story/frame_" << std::setfill('0') << std::setw(3) << i << ".jpg";
            std::string path = resolvePath(ss.str());

            if (fileExists(path)) {
                sf::Texture tex;
                if (tex.loadFromFile(path)) {
                    introFrames.push_back(tex);
                }
                else {
                    break;
                }
            }
            else {
                break; // No more frames
            }
        }
        if (introFrames.empty()) {
            // Fallback story frames if files not found
            std::cout << "[INFO] No story frames found in ../Story/. Using fallback text intro.\n";
            introActive = true;
        }
        else {
            introSprite.emplace(introFrames[0]);
        }
    }

    std::vector<std::string> introTexts = {
        "The world as we knew it ended in a flash of terror.",
        "A mysterious plague swept across the continents, leaving only chaos.",
        "Cities that once buzzed with life became silent graveyards.",
        "The survivors were forced to flee into the wild, seeking refuge.",
        "But the wilderness is no longer safe; the infected are everywhere.",
        "Hunger and thirst will be your constant companions in this new world.",
        "You must scavenge for food and clean water to stay alive.",
        "Trees provide wood for tools and fire; stones are essential for building.",
        "Crafting is the key to your survival. Without tools, you are defenseless.",
        "Nightfall brings the most danger. Zombies emerge from the shadows.",
        "Build a campfire to keep the darkness at bay and stay warm.",
        "But remember, you are not alone. Others may be out there.",
        "Some might help you, while others will fight for the same resources.",
        "Every choice you make could be your last. Stay alert.",
        "This is a world of RAS - Ruins, Ashes, and Survival.",
        "Your story begins here. Can you endure the coming storms?"
    };

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

    struct TutorialStep {
        std::string title;
        std::string description;
        std::string goal;
    };
    std::vector<TutorialStep> tutorialSteps = {
        {"WELCOME SURVIVOR", "You have arrived in a dangerous world. Let's learn the basics.", "Click 'Next' to continue"},
        {"MOVEMENT", "Use [W, A, S, D] to move around the map.", "Walk a short distance"},
        {"SPRINTING", "Hold [Left Shift] while moving to sprint.", "Try sprinting now"},
        {"INVENTORY", "Press [I] to open your backpack. You can equip tools here.", "Open your inventory"},
        {"GATHERING", "Go near a tree and press [Space] to chop wood.\nGo near a stone and press [E] to pick it up.", "Gather 1 Wood and 1 Stone"},
        {"COMBAT", "Animals and Monsters are everywhere. Use [Space] to attack.", "Defend yourself!"},
        {"COMMUNICATION", "Press [T] to open the global chat and talk to others.", "Say 'Hello' in chat"},
        {"ALL SET!", "You are ready to survive. Good luck!", "Click 'Finish' to start playing"}
    };
    sf::RectangleShape tutorialBox({ 800.f, 400.f });
    tutorialBox.setFillColor(sf::Color(20, 20, 20, 240));
    tutorialBox.setOutlineColor(sf::Color::Cyan);
    tutorialBox.setOutlineThickness(3.f);
    tutorialBox.setPosition({ (width - 800.f) / 2.f, height - 450.f });

    // Daily Quest System
    struct DailyQuest {
        int id;
        std::string title;
        std::string description;
        int goal;
    };
    std::vector<DailyQuest> availableQuests = {
        {1, "Lumberjack", "Chop down 10 trees.", 10},
        {2, "Stone Collector", "Collect 8 stones.", 8},
        {3, "Keep Warm", "Craft 1 campfire.", 1},
        {4, "Hunter", "Collect 5 meat items.", 5}
    };

    struct CraftableItem {
        int id;
        std::string name;
        int woodReq;
        int stoneReq;
        int woolReq;
        const sf::Texture* tex;
    };
    std::vector<CraftableItem> craftableItems = {
        {ITEM_NONE, "Campfire", 10, 10, 0, &campfireFrames[0]},
        {ITEM_WOODEN_SWORD, "Wooden Sword", 8, 0, 0, &woodenSwordTex},
        {ITEM_SHIELD, "Sturdy Shield", 10, 5, 0, &shieldTex},
        {ITEM_FURNACE, "Stone Furnace", 0, 10, 0, &furnaceTex},
        {ITEM_WOOD_TILE, "Wood Tile", 1, 0, 0, &woodTileTex},
        {ITEM_WOOD_WALL, "Wood Wall", 1, 0, 0, &woodWallTex},
        {ITEM_WOOD_DOOR, "Wood Door", 5, 0, 0, &woodDoorTex},
        {ITEM_WOOD_ROOF, "Wood Roof", 5, 0, 0, &woodRoofTex},
        {ITEM_WOOD_STAIRS, "Wood Stairs", 8, 0, 0, &woodStairsTex},
        {ITEM_BED, "Comfortable Bed", 10, 0, 5, &bedTex}
    };
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

    auto applyStructureScale = [](sf::Sprite& spr, int id, float rotation, bool isOpen, float& outShearY) {
        // Target visual size = fits the grid cell: 200 x 100 world units.
        const float TARGET_W = 200.f;
        const float TARGET_H = 100.f;

        auto sz = spr.getTexture().getSize();
        float scaleX = TARGET_W / static_cast<float>(sz.x);
        float scaleY = TARGET_H / static_cast<float>(sz.y);

        outShearY = GLOBAL_SHEAR; // Default floor slant
        if (id == ITEM_WOOD_TILE) {
            spr.setScale({ scaleX, scaleY });
        }
        else if (id == ITEM_WOOD_WALL) {
            if (rotation == 90.f) {
                float scaleX_vert = 100.f / static_cast<float>(sz.x);
                float scaleY_vert = 180.f / static_cast<float>(sz.y);
                spr.setScale({ scaleX_vert, scaleY_vert });
                outShearY = 0.f; // vertical wall stands straight up
            }
            else {
                float scaleX_slant = 200.f / static_cast<float>(sz.x);
                float scaleY_slant = 180.f / static_cast<float>(sz.y);
                spr.setScale({ scaleX_slant, scaleY_slant });
                outShearY = GLOBAL_SHEAR;
            }
        }
        else if (id == ITEM_WOOD_DOOR) {
            if (isOpen) {
                if (rotation == 90.f) {
                    float scaleX_vert = 100.f / static_cast<float>(sz.x);
                    float scaleY_vert = 180.f / static_cast<float>(sz.y);
                    spr.setScale({ scaleX_vert * 0.11f, scaleY_vert });
                    outShearY = 0.f;
                }
                else {
                    float scaleX_slant = 200.f / static_cast<float>(sz.x);
                    float scaleY_slant = 180.f / static_cast<float>(sz.y);
                    spr.setScale({ scaleX_slant, scaleY_slant * 0.11f });
                    outShearY = GLOBAL_SHEAR;
                }
            }
            else {
                if (rotation == 90.f) {
                    float scaleX_vert = 100.f / static_cast<float>(sz.x);
                    float scaleY_vert = 180.f / static_cast<float>(sz.y);
                    spr.setScale({ scaleX_vert, scaleY_vert });
                    outShearY = 0.f;
                }
                else {
                    float scaleX_slant = 200.f / static_cast<float>(sz.x);
                    float scaleY_slant = 180.f / static_cast<float>(sz.y);
                    spr.setScale({ scaleX_slant, scaleY_slant });
                    outShearY = GLOBAL_SHEAR;
                }
            }
        }
        else if (id == ITEM_FURNACE) {
            // Furnace covers the grid (200 width) but stands out tall (150 height)
            float sY = (sz.y > 0) ? 150.f / static_cast<float>(sz.y) : scaleY;
            spr.setScale({ scaleX, sY });
        }
        else if (id == ITEM_BED) {
            // Bed covers the grid (200 width) but stands out tall (130 height)
            float sY = (sz.y > 0) ? 130.f / static_cast<float>(sz.y) : scaleY;
            spr.setScale({ scaleX, sY });
        }
        else if (id == ITEM_WOOD_STAIRS) {
            if (rotation == 90.f)        { spr.setScale({ scaleX, scaleY }); outShearY = 0.f; }
            else                          spr.setScale({ scaleX, scaleY });
        }
        else if (id == ITEM_WOOD_ROOF) {
            if (rotation == 90.f)        { spr.setScale({ scaleX, scaleY }); outShearY = 0.f; }
            else                          spr.setScale({ scaleX, scaleY });
        }
        else if (id == ITEM_NONE) spr.setScale({ scaleX * 1.3f, scaleY * 1.3f }); // campfire: 1.3x larger to stand out
        else spr.setScale({ scaleX, scaleY });
        };

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
    bool tracedLoopEntered = false;
    bool tracedFirstDisplay = false;
    bool tracedAfterFirstDisplay = false;
    std::uint64_t debugFrameIndex = 0;
    const sf::View defaultView = window.getDefaultView();

    // =====================================================
    // Incremental world spawns (avoid long stalls)
    // =====================================================
    struct SpawnPlan {
        bool active = false;
        bool done = false;

        // item spawns
        int nearTreesTotal = 80;
        int nearStonesTotal = 50;
        int globalTreesTotal = 4000;
        int globalStonesTotal = 3000;

        int nearTreesDone = 0;
        int nearStonesDone = 0;
        int globalTreesDone = 0;
        int globalStonesDone = 0;

        // mobs
        int localChickensTotal = 15;
        int localPigsTotal = 10;
        int localSheepsTotal = 10;
        int localZombiesTotal = 5;
        int globalChickensTotal = 200;
        int globalPigsTotal = 100;
        int globalSheepsTotal = 100;

        int localChickensDone = 0;
        int localPigsDone = 0;
        int localSheepsDone = 0;
        int localZombiesDone = 0;
        int globalChickensDone = 0;
        int globalPigsDone = 0;
        int globalSheepsDone = 0;

        void start() {
            active = true;
            done = false;
            nearTreesDone = nearStonesDone = globalTreesDone = globalStonesDone = 0;
            localChickensDone = localPigsDone = localSheepsDone = localZombiesDone = 0;
            globalChickensDone = globalPigsDone = globalSheepsDone = 0;
        }

        int totalWork() const {
            return nearTreesTotal + nearStonesTotal + globalTreesTotal + globalStonesTotal
                + localChickensTotal + localPigsTotal + localSheepsTotal + localZombiesTotal
                + globalChickensTotal + globalPigsTotal + globalSheepsTotal;
        }

        int doneWork() const {
            return nearTreesDone + nearStonesDone + globalTreesDone + globalStonesDone
                + localChickensDone + localPigsDone + localSheepsDone + localZombiesDone
                + globalChickensDone + globalPigsDone + globalSheepsDone;
        }
    };

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

        if (spawnPlan.active && !spawnPlan.done) {
            static int spawnerTick = 0;
            if (++spawnerTick % 5 == 0) {
                std::ostringstream debugSs;
                debugSs << "[Spawner Debug] doneWork: " << spawnPlan.doneWork() << "/" << spawnPlan.totalWork()
                        << " | nearTrees: " << spawnPlan.nearTreesDone << "/" << spawnPlan.nearTreesTotal
                        << " | nearStones: " << spawnPlan.nearStonesDone << "/" << spawnPlan.nearStonesTotal
                        << " | globalTrees: " << spawnPlan.globalTreesDone << "/" << spawnPlan.globalTreesTotal
                        << " | globalStones: " << spawnPlan.globalStonesDone << "/" << spawnPlan.globalStonesTotal
                        << " | localChic: " << spawnPlan.localChickensDone << "/" << spawnPlan.localChickensTotal
                        << " | localPig: " << spawnPlan.localPigsDone << "/" << spawnPlan.localPigsTotal
                        << " | localSheep: " << spawnPlan.localSheepsDone << "/" << spawnPlan.localSheepsTotal
                        << " | localZom: " << spawnPlan.localZombiesDone << "/" << spawnPlan.localZombiesTotal
                        << " | globalChic: " << spawnPlan.globalChickensDone << "/" << spawnPlan.globalChickensTotal
                        << " | globalPig: " << spawnPlan.globalPigsDone << "/" << spawnPlan.globalPigsTotal
                        << " | globalSheep: " << spawnPlan.globalSheepsDone << "/" << spawnPlan.globalSheepsTotal;
                traceStartup(debugSs.str());
            }
            const float MIN_DISTANCE = 120.f;

            // Budget per frame (keeps FPS responsive)
            const int ITEM_ATTEMPTS_BUDGET = 250;
            const int MOB_BUDGET = 15;

            int itemAttempts = 0;
            int mobCount = 0;

            auto snapGrid = [](sf::Vector2f pos) -> sf::Vector2f {
                return snapToShearedGrid(pos);
            };

            auto isNearPlaceable = [&](sf::Vector2f pos) -> bool {
                const float threshold = 120.f;
                for (auto& s : structures) {
                    if (s.sprite.has_value()) {
                        sf::Vector2f sPos = s.sprite->getPosition();
                        float dx = pos.x - sPos.x;
                        float dy = pos.y - sPos.y;
                        if (std::sqrt(dx * dx + dy * dy) < threshold) {
                            return true;
                        }
                    }
                }
                for (auto& c : campfires) {
                    float dx = pos.x - c.position.x;
                    float dy = pos.y - c.position.y;
                    if (std::sqrt(dx * dx + dy * dy) < threshold) {
                        return true;
                    }
                }
                return false;
            };

            auto trySpawnNear = [&](int itemId, sf::Texture& tex, int& done, int total, int radius) {
                while (done < total && itemAttempts < ITEM_ATTEMPTS_BUDGET) {
                    ++itemAttempts;
                    sf::Vector2f p(
                        MAP_WIDTH / 2.f + (rand() % (radius * 2) - radius),
                        MAP_HEIGHT / 2.f + (rand() % (radius * 2) - radius)
                    );
                    p = snapGrid(p);
                    if (itemId == ITEM_TREE) {
                        bool placed = false;
                        for (int attempt = 0; attempt < 30; ++attempt) {
                            if (!isInWater(p, pondBoundsList) && !isNearPlaceable(p)) {
                                itemManager.addItem(itemId, tex, p, 1);
                                ++done;
                                placed = true;
                                break;
                            }
                            p = sf::Vector2f(
                                MAP_WIDTH / 2.f + (rand() % (radius * 2) - radius),
                                MAP_HEIGHT / 2.f + (rand() % (radius * 2) - radius)
                            );
                            p = snapGrid(p);
                            ++itemAttempts;
                        }
                        if (!placed) {
                            if (!isNearPlaceable(p)) {
                                itemManager.addItem(itemId, tex, p, 1);
                                ++done;
                            }
                        }
                    }
                    else {
                        bool placed = false;
                        for (int attempt = 0; attempt < 30; ++attempt) {
                            if (!isNearPlaceable(p)) {
                                itemManager.addItem(itemId, tex, p, 1);
                                ++done;
                                placed = true;
                                break;
                            }
                            p = sf::Vector2f(
                                MAP_WIDTH / 2.f + (rand() % (radius * 2) - radius),
                                MAP_HEIGHT / 2.f + (rand() % (radius * 2) - radius)
                            );
                            p = snapGrid(p);
                            ++itemAttempts;
                        }
                        if (!placed) {
                            if (!isNearPlaceable(p)) {
                                itemManager.addItem(itemId, tex, p, 1);
                                ++done;
                            }
                        }
                    }
                }
            };

            auto trySpawnGlobal = [&](int itemId, sf::Texture& tex, int& done, int total) {
                while (done < total && itemAttempts < ITEM_ATTEMPTS_BUDGET) {
                    sf::Vector2f p(randX(gen), randY(gen));
                    p = snapGrid(p);
                    bool spotFound = false;
                    for (int attempt = 0; attempt < 30; ++attempt) {
                        ++itemAttempts;
                        p = sf::Vector2f(randX(gen), randY(gen));
                        p = snapGrid(p);
                        if (itemId == ITEM_TREE && isInWater(p, pondBoundsList)) {
                            continue;
                        }
                        if (isNearPlaceable(p)) {
                            continue;
                        }
                        if (itemManager.isAreaFreeExplicit(p, MIN_DISTANCE, &tex)) {
                            spotFound = true;
                            break;
                        }
                    }
                    if (!spotFound && itemId == ITEM_TREE) {
                        for (int attempt = 0; attempt < 20; ++attempt) {
                            p = sf::Vector2f(randX(gen), randY(gen));
                            p = snapGrid(p);
                            if (!isInWater(p, pondBoundsList) && !isNearPlaceable(p)) {
                                spotFound = true;
                                break;
                            }
                        }
                    }
                    if (spotFound || !isNearPlaceable(p)) {
                        itemManager.addItem(itemId, tex, p, 1);
                        ++done;
                    }
                }
            };

            // Phase order: near items -> global items -> local mobs -> global mobs
            if (spawnPlan.nearTreesDone < spawnPlan.nearTreesTotal)
                trySpawnNear(ITEM_TREE, treeTex, spawnPlan.nearTreesDone, spawnPlan.nearTreesTotal, 8000);

            if (spawnPlan.nearStonesDone < spawnPlan.nearStonesTotal)
                trySpawnNear(ITEM_STONE, stoneTex, spawnPlan.nearStonesDone, spawnPlan.nearStonesTotal, 6000);

            if (spawnPlan.globalTreesDone < spawnPlan.globalTreesTotal)
                trySpawnGlobal(ITEM_TREE, treeTex, spawnPlan.globalTreesDone, spawnPlan.globalTreesTotal);

            if (spawnPlan.globalStonesDone < spawnPlan.globalStonesTotal)
                trySpawnGlobal(ITEM_STONE, stoneTex, spawnPlan.globalStonesDone, spawnPlan.globalStonesTotal);

            // mobs (simple budget)
            while (mobCount < MOB_BUDGET && (
                spawnPlan.localChickensDone < spawnPlan.localChickensTotal ||
                spawnPlan.localPigsDone < spawnPlan.localPigsTotal ||
                spawnPlan.localSheepsDone < spawnPlan.localSheepsTotal ||
                spawnPlan.localZombiesDone < spawnPlan.localZombiesTotal ||
                spawnPlan.globalChickensDone < spawnPlan.globalChickensTotal ||
                spawnPlan.globalPigsDone < spawnPlan.globalPigsTotal ||
                spawnPlan.globalSheepsDone < spawnPlan.globalSheepsTotal
                ))
            {
                if (spawnPlan.localChickensDone < spawnPlan.localChickensTotal) {
                    sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 4000 - 2000), MAP_HEIGHT / 2.f + (rand() % 4000 - 2000));
                    animals.push_back(std::make_unique<Chicken>(p));
                    ++spawnPlan.localChickensDone;
                    ++mobCount;
                    continue;
                }
                if (spawnPlan.localPigsDone < spawnPlan.localPigsTotal) {
                    sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 4000 - 2000), MAP_HEIGHT / 2.f + (rand() % 4000 - 2000));
                    animals.push_back(std::make_unique<Pig>(p));
                    ++spawnPlan.localPigsDone;
                    ++mobCount;
                    continue;
                }
                if (spawnPlan.localSheepsDone < spawnPlan.localSheepsTotal) {
                    sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 4000 - 2000), MAP_HEIGHT / 2.f + (rand() % 4000 - 2000));
                    animals.push_back(std::make_unique<Sheep>(p));
                    ++spawnPlan.localSheepsDone;
                    ++mobCount;
                    continue;
                }
                if (spawnPlan.localZombiesDone < spawnPlan.localZombiesTotal) {
                    sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 4000 - 2000), MAP_HEIGHT / 2.f + (rand() % 4000 - 2000));
                    monsters.push_back(std::make_unique<Zombie>(p));
                    ++spawnPlan.localZombiesDone;
                    ++mobCount;
                    continue;
                }
                if (spawnPlan.globalChickensDone < spawnPlan.globalChickensTotal) {
                    animals.push_back(std::make_unique<Chicken>(sf::Vector2f(randX(gen), randY(gen))));
                    ++spawnPlan.globalChickensDone;
                    ++mobCount;
                    continue;
                }
                if (spawnPlan.globalPigsDone < spawnPlan.globalPigsTotal) {
                    animals.push_back(std::make_unique<Pig>(sf::Vector2f(randX(gen), randY(gen))));
                    ++spawnPlan.globalPigsDone;
                    ++mobCount;
                    continue;
                }
                if (spawnPlan.globalSheepsDone < spawnPlan.globalSheepsTotal) {
                    animals.push_back(std::make_unique<Sheep>(sf::Vector2f(randX(gen), randY(gen))));
                    ++spawnPlan.globalSheepsDone;
                    ++mobCount;
                    continue;
                }
            }

            // Save overlay state; draw it at end-of-frame so later clears don't erase it.
            spawnOverlayActive = true;
            spawnOverlayTotal = spawnPlan.totalWork();
            spawnOverlayDone = spawnPlan.doneWork();

            if (spawnPlan.doneWork() >= spawnPlan.totalWork()) {
                spawnPlan.done = true;
                itemSpawned = true;
                spawnOverlayActive = false;
                traceStartup("spawn_plan_done");
            }
        }

        sf::Vector2f playerPos = sprite.getPosition();


        // =====================================================
        // DESPAWN FAR ANIMALS / MONSTERS & SPAWN NEAR CAMERA (ONLY IF NOT IN LOADING SCREEN)
        // =====================================================
        if (!spawnOverlayActive)
        {
            // Use squared distance to avoid sqrt in despawn checks
            const float DESPAWN_RADIUS_SQ = DESPAWN_RADIUS * DESPAWN_RADIUS;
            animals.erase(
                std::remove_if(
                    animals.begin(),
                    animals.end(),
                    [&](const std::unique_ptr<Animal>& animal)
                    {
                        return getDistanceSq(animal->getPosition(), playerPos) > DESPAWN_RADIUS_SQ;
                    }),
                animals.end()
            );


            // =====================================================
            // DESPAWN FAR MONSTERS
            // =====================================================

            monsters.erase(
                std::remove_if(
                    monsters.begin(),
                    monsters.end(),
                    [&](const std::unique_ptr<Monster>& monster)
                    {
                        return getDistanceSq(monster->getPosition(), playerPos) > DESPAWN_RADIUS_SQ;
                    }),
                monsters.end()
            );


            // =====================================================
            // SPAWN ANIMALS NEAR CAMERA
            // =====================================================

            // Rate-limited spawning: at most 1 animal per tick to avoid CPU spike
            if (mobSpawnClock.getElapsedTime().asSeconds() > 0.15f)
            {
                if ((int)animals.size() < MAX_ANIMALS)
                {
                    float angle =
                        static_cast<float>(rand()) /
                        RAND_MAX * 6.28318f;

                    float distance =
                        500.f +
                        static_cast<float>(rand()) /
                        RAND_MAX * SPAWN_RADIUS;

                    sf::Vector2f spawnPos(
                        playerPos.x + std::cos(angle) * distance,
                        playerPos.y + std::sin(angle) * distance
                    );

                    spawnPos.x = std::clamp(spawnPos.x, 0.f, (float)MAP_WIDTH);
                    spawnPos.y = std::clamp(spawnPos.y, 0.f, (float)MAP_HEIGHT);

                    int type = rand() % 3;
                    if (type == 0)
                        animals.push_back(std::make_unique<Chicken>(spawnPos));
                    else if (type == 1)
                        animals.push_back(std::make_unique<Pig>(spawnPos));
                    else
                        animals.push_back(std::make_unique<Sheep>(spawnPos));
                }

                mobSpawnClock.restart();
            }


            // =====================================================
            // NIGHT ZOMBIE SPAWNING
            // =====================================================

            bool isNight =
                darkness.getFillColor().a > 150;

            // Rate-limited zombie spawning: at most 1 per tick
            if (isNight && monsterSpawnClock.getElapsedTime().asSeconds() > 0.4f)
            {
                if ((int)monsters.size() < MAX_MONSTERS)
                {
                    float angle =
                        static_cast<float>(rand()) /
                        RAND_MAX * 6.28318f;

                    float distance =
                        800.f +
                        static_cast<float>(rand()) /
                        RAND_MAX * SPAWN_RADIUS;

                    sf::Vector2f spawnPos(
                        playerPos.x + std::cos(angle) * distance,
                        playerPos.y + std::sin(angle) * distance
                    );

                    spawnPos.x = std::clamp(spawnPos.x, 0.f, (float)MAP_WIDTH);
                    spawnPos.y = std::clamp(spawnPos.y, 0.f, (float)MAP_HEIGHT);

                    monsters.push_back(std::make_unique<Zombie>(spawnPos));
                }

                monsterSpawnClock.restart();
            }
        }


        // Collision check with distance culling to skip far-away objects
        const float BLOCK_CULL_SQ = 600.f * 600.f; // only check objects within 600px
        auto isBlocked = [&](sf::Vector2f pos)
            {
                // =====================================================
                // STRUCTURE COLLISION (with distance culling)
                // =====================================================
                for (auto& structure : structures)
                {
                    if (structure.sprite.has_value())
                    {
                        sf::Vector2f sc = structure.sprite->getPosition();
                        float sdx = sc.x - pos.x, sdy = sc.y - pos.y;
                        if (sdx * sdx + sdy * sdy > BLOCK_CULL_SQ) continue;
                        
                        if (structure.id == ITEM_WOOD_WALL || (structure.id == ITEM_WOOD_DOOR && !structure.isOpen)) {
                            sf::Vector2f sPos = structure.sprite->getPosition();
                            sf::Vector2f A = sPos - sf::Vector2f(250.8f, 250.8f * structure.shearY);
                            sf::Vector2f B = sPos + sf::Vector2f(250.8f, 250.8f * structure.shearY);
                            if (distanceToSegment(pos, A, B) < 40.f) return true;
                        }
                        else if (structure.id == ITEM_FURNACE || structure.id == ITEM_BED) {
                            sf::FloatRect b = structure.sprite->getGlobalBounds();
                            b.position.x += b.size.x * 0.2f; b.size.x *= 0.6f;
                            b.position.y += b.size.y * 0.7f; b.size.y *= 0.3f;
                            if (b.contains(pos)) return true;
                        }
                    }
                }

                // =====================================================
                // POND COLLISION (with distance culling)
                // =====================================================
                for (auto& pond : ponds)
                {
                    sf::Vector2f pc = pond.getPosition();
                    float pdx = pc.x - pos.x, pdy = pc.y - pos.y;
                    if (pdx * pdx + pdy * pdy > BLOCK_CULL_SQ) continue;
                    if (pond.getGlobalBounds().contains(pos))
                        return true;
                }

                return false;
            };

        // Update nearby animals and monsters using squared-distance check
        const float UPDATE_RADIUS_SQ = 1800.f * 1800.f;
        if (!spawnOverlayActive)
        {
            for (auto& animal : animals)
            {
                if (getDistanceSq(animal->getPosition(), playerPos) < UPDATE_RADIUS_SQ)
                {
                    animal->update(dt, sprite.getPosition(), isBlocked);
                }
            }

            // =====================================================
            // UPDATE ONLY NEARBY MONSTERS
            // =====================================================

            for (auto& monster : monsters)
            {
                if (getDistanceSq(monster->getPosition(), playerPos) < UPDATE_RADIUS_SQ)
                {
                    monster->update(dt, sprite.getPosition(), isBlocked);
                }
            }
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
        bool isMenuOpen = (stockPageOpen || iapPageOpen || iapVerifyingOpen || tradeMenuOpen || tradeConfirmOpen || craftMenuOpen || p2pTradeOpen || anyCampfireMenuOpen);

        // =====================================================
        // P2P TRADE POLL LOOP (every 1 second)
        // =====================================================
        if (p2pTradeID > 0) {
            p2pPollTimer -= dt;
            p2pOfferPushTimer -= dt;
            if (p2pPollTimer <= 0.f) {
                p2pPollTimer = 1.0f;
                p2pState = authManager.getP2PTradeState(p2pTradeID);

                // Parse partner's offer from CSV
                bool amI = p2pAmInitiator;
                std::string theirCSV = amI ? p2pState.targetItems : p2pState.initiatorItems;
                partnerGold = amI ? p2pState.targetGold : p2pState.initiatorGold;
                p2pPartnerReady = amI ? p2pState.targetReady : p2pState.initiatorReady;
                p2pMyReady      = amI ? p2pState.initiatorReady : p2pState.targetReady;

                // Parse CSV "itemID:count,itemID:count"
                partnerItems.clear();
                if (!theirCSV.empty()) {
                    std::stringstream ss(theirCSV);
                    std::string token;
                    while (std::getline(ss, token, ',')) {
                        auto colon = token.find(':');
                        if (colon != std::string::npos) {
                            P2PItemDisplay d;
                            d.id    = std::stoi(token.substr(0, colon));
                            d.count = std::stoi(token.substr(colon + 1));
                            if (d.id > 0 && d.count > 0) partnerItems.push_back(d);
                        }
                    }
                }

                if (p2pState.status == "pending" && !p2pAmInitiator) {
                    // Target player sees invite popup
                    p2pInvitePopup = true;
                    p2pTradeOpen   = false;
                }
                else if (p2pState.status == "accepted") {
                    p2pTradeOpen   = true;
                    p2pInvitePopup = false;
                    if (p2pStatusMsg.find("Waiting") != std::string::npos)
                        p2pStatusMsg = "Trade open! Add items and gold, then press Ready.";
                }
                else if (p2pState.status == "completed") {
                    // Apply received items to local inventory
                    for (auto& d : partnerItems) {
                        const sf::Texture* tex = getItemTexture(d.id);
                        if (tex) inventory.addItem(d.id, *tex, d.count);
                        // Update market: buying pressure raises price
                        adjustMarketItem(d.id, 0.004f * d.count);

                        // Log BUY record for received item
                        std::string name = getItemName(d.id);
                        int uPrice = getItemPrice(d.id);
                        int totalVal = d.count * uPrice;
                        authManager.addTradeRecord(username.c_str(), "BUY", name, d.count, uPrice, totalVal);
                    }
                    currency += partnerGold;

                    // Log SELL records for items we gave to partner before we clear our slots
                    for (int i = 0; i < 4; i++) {
                        if (myP2PSlots[i].tex) {
                            int item_id = myP2PSlots[i].id;
                            int item_qty = myP2PSlots[i].count;
                            std::string name = getItemName(item_id);
                            int uPrice = getItemPrice(item_id);
                            int totalVal = item_qty * uPrice;
                            authManager.addTradeRecord(username.c_str(), "SELL", name, item_qty, uPrice, totalVal);
                        }
                    }

                    // Remove what I offered
                    for (int i = 0; i < 4; i++) myP2PSlots[i] = {};
                    currency -= myP2PGold;
                    myP2PGold = 0;
                    latestPlayerMsg = "P2P trade completed with " + p2pPartner + "!";
                    bubbleTimer = 4.f;
                    authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
                    p2pTradeOpen = false; p2pTradeID = -1; p2pPartner = "";
                    p2pMyReady = false; p2pPartnerReady = false; p2pStatusMsg = "";
                    partnerItems.clear(); partnerGold = 0;
                }
                else if (p2pState.status == "cancelled") {
                    // Return items
                    for (int i = 0; i < 4; i++) {
                        if (myP2PSlots[i].tex) {
                            inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
                            myP2PSlots[i] = {};
                        }
                    }
                    currency += myP2PGold; myP2PGold = 0;
                    p2pTradeOpen = false; p2pInvitePopup = false; p2pTradeID = -1;
                    p2pPartner = ""; p2pStatusMsg = "Trade cancelled.";
                    latestPlayerMsg = "P2P trade cancelled."; bubbleTimer = 3.f;
                }

                // Push my offer to DB if dirty
                if (p2pOfferDirty && p2pTradeID > 0 && p2pOfferPushTimer <= 0.f) {
                    std::string myCSV;
                    for (int i = 0; i < 4; i++) {
                        if (myP2PSlots[i].tex) {
                            if (!myCSV.empty()) myCSV += ",";
                            myCSV += std::to_string(myP2PSlots[i].id) + ":" + std::to_string(myP2PSlots[i].count);
                        }
                    }
                    authManager.updateP2POffer(p2pTradeID, username, myCSV, myP2PGold);
                    // Market: selling pressure lowers price for offered items
                    for (int i = 0; i < 4; i++) {
                        if (myP2PSlots[i].tex) {
                            adjustMarketItem(myP2PSlots[i].id, -0.003f * myP2PSlots[i].count);
                        }
                    }
                    p2pOfferDirty = false;
                    p2pOfferPushTimer = 1.f;
                }

                // Check if both ready → trigger completion
                if (p2pState.initiatorReady && p2pState.targetReady && p2pState.status == "accepted") {
                    authManager.completeP2PTrade(p2pTradeID);
                }
            }
        }

        if (!introActive && !tutorialActive && !isMenuOpen && !spawnOverlayActive)
        {
            // survival update
            hunger = std::min(
                maxHunger,
                hunger + (HUNGER_RATE / hungerMultiplier) * dt
            );

            thirst = std::min(
                maxThirst,
                thirst + (THIRST_RATE / thirstMultiplier) * dt
            );

            if (hunger >= maxHunger)
            {
                if (hungerMultiplier > 1)
                {
                    hungerMultiplier--;

                    // Restore hunger back to full
                    hunger = 0.f;
                }
                else
                {
                    // No multiplier left -> damage HP
                    hp = std::max(0.f, hp - DAMAGE_RATE * dt);
                }
            }

            if (thirst >= maxThirst)
            {
                if (thirstMultiplier > 1)
                {
                    thirstMultiplier--;

                    // Restore thirst back to full
                    thirst = 0.f;
                }
                else
                {
                    // No multiplier left -> damage HP
                    hp = std::max(0.f, hp - DAMAGE_RATE * dt);
                }
            }
            else if (hunger < maxHunger * 0.2f) // Hunger bar above 80% (hunger < 20% empty)
            {
                hp = std::min(maxHP, hp + 5.f * dt);
            }
        }

        // Level up check
        int nextLevelExp = level * 100;
        if (exp >= nextLevelExp) {
            exp -= nextLevelExp;
            level++;
            std::cout << "LEVEL UP! Now Level " << level << "\n";
        }

        // Check Daily Quest Completion
        if (!tutorialActive && qInfo.questID > 0 && !qInfo.isCompleted) {
            int goal = 0;
            if (qInfo.questID >= 1 && qInfo.questID <= (int)availableQuests.size()) {
                goal = availableQuests[qInfo.questID - 1].goal;
            }

            if (qInfo.progress >= goal && goal > 0) {
                qInfo.isCompleted = true;
                currency += 50; // Reward!
                authManager.updateQuestProgress(username.c_str(), qInfo.progress, true);
                latestPlayerMsg = "DAILY QUEST COMPLETED! +50 Gold Reward!";
                bubbleTimer = 5.0f;
            }
        }

        if (swordCrafting) {
            swordCraftTimer -= dt;
            if (swordCraftTimer <= 0.f) {
                swordCrafting = false;
                inventory.addItem(4, woodenSwordTex, 1);
            }
        }

        if (shieldCrafting) {
            shieldCraftTimer -= dt;
            if (shieldCraftTimer <= 0.f) {
                shieldCrafting = false;
                inventory.addItem(6, shieldTex, 1);
            }
        }

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

        sf::Vector2f lastSaveCameraPos = sprite.getPosition();
        const float SAVE_DISTANCE = 2000.f;

        float dx = sprite.getPosition().x - lastSaveCameraPos.x;
        float dy = sprite.getPosition().y - lastSaveCameraPos.y;

        float distanceMoved = std::sqrt(dx * dx + dy * dy);

        if (distanceMoved >= SAVE_DISTANCE &&
            saveCooldownClock.getElapsedTime().asSeconds() > 10.f)
        {
            std::vector<std::pair<int, int>> items(20, { 0, 0 });
            for (int i = 0; i < 20; i++) {
                if (inventory.hasItem(i)) {
                    items[i] = { inventory.getItemID(i), inventory.getItemCount(i) };
                }
            }
            authManager.saveInventory(username.c_str(), items);

            float currentElapsed = elapsedGameTime;

            authManager.savePlayerStats(
                username.c_str(),
                hunger,
                thirst,
                currency,
                level,
                exp,
                currentElapsed,
                hungerMultiplier,
                thirstMultiplier
            );

            std::vector<StoredStructure> structData;

            for (auto& s : structures)
            {
                if (s.sprite.has_value() &&
                    (s.owner == username || s.owner == ""))
                {
                    structData.push_back({
                        s.id,
                        s.sprite->getPosition().x,
                        s.sprite->getPosition().y,
                        s.health,
                        s.rotation,
                        s.isOpen,
                        s.shearY
                        });
                }
            }

            authManager.saveStructures(username.c_str(), structData);

            std::vector<StoredCampfire> campData;

            for (auto& c : campfires)
            {
                campData.push_back({
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

            authManager.saveCampfires(username.c_str(), campData);

            lastSaveCameraPos = sprite.getPosition();

            saveCooldownClock.restart();

            std::cout << "[CAMERA AUTO SAVE]\n";
        }

        if (hp <= 0.f && !isDead)
        {
            isDead = true;
            deathClock.restart();
        }

        if (inventoryOpen)
        {
            // === Inventory layout (must match Inventory.cpp) ===
            const float SLOT_W = 64.f;   // same as boxW
            const float SLOT_GAP = 4.f;
            const int SLOT_COUNT = 8;

            float invX = inventory.computeX(window);
            float invY = inventory.computeY(window);
            float invWidth = SLOT_COUNT * (SLOT_W + SLOT_GAP);

            float barWidth = invWidth; // Longer bars
            float barHeight = 22.f;
            float spacing = 80.f; // Increased spacing

            float startX = invX;
            hpBack.setSize({ barWidth, barHeight });
            hungerBack.setSize({ barWidth, barHeight });
            thirstBack.setSize({ barWidth, barHeight });
            expBack.setSize({ barWidth, barHeight });

            float numBars = shieldEquipped ? 5.f : 4.f; // HP, Hunger, Thirst, EXP + optional Shield
            float startY = invY - (barHeight * numBars) - (spacing * (numBars - 1.f));

            hpBack.setPosition({ startX, startY });
            hungerBack.setPosition({ startX, startY + barHeight + spacing });
            thirstBack.setPosition({ startX, startY + (barHeight + spacing) * 2.f });
            expBack.setPosition({ startX, startY + (barHeight + spacing) * 3.f });

            float iconPadding = 6.f;
            float textPadding = 4.f;

            if (shieldEquipped) {
                defBack.setSize({ barWidth, barHeight });
                defBack.setPosition({ startX, startY + (barHeight + spacing) * 4.f });

                float defVal = inventory.getDurability(eqShieldSlot);
                defBar.setSize({ barWidth * (defVal / 500.f), barHeight });
                defBar.setPosition(defBack.getPosition());

                defText.setString("Defense " + std::to_string((int)defVal) + " / 500");
                defText.setCharacterSize(16);
                defText.setPosition({
                    defBack.getPosition().x + textPadding,
                    defBack.getPosition().y + 2.f
                    });
            }

            hpBar.setSize({ barWidth * (hp / maxHP), barHeight });
            hungerBar.setSize({ barWidth * (1.f - hunger / maxHunger), barHeight });
            thirstBar.setSize({ barWidth * (1.f - thirst / maxThirst), barHeight });

            hpBar.setPosition(hpBack.getPosition());
            hungerBar.setPosition(hungerBack.getPosition());
            thirstBar.setPosition(thirstBack.getPosition());

            hungerMultiplierText.setString("X" + std::to_string(hungerMultiplier));
            hungerMultiplierText.setPosition({
                hungerBack.getPosition().x + barWidth + 20.f,
                hungerBack.getPosition().y - 2.f
                });

            thirstMultiplierText.setString("X" + std::to_string(thirstMultiplier));
            thirstMultiplierText.setPosition({
                thirstBack.getPosition().x + barWidth + 20.f,
                thirstBack.getPosition().y - 2.f
                });

            int nextLevelExp = level * 100;
            expBar.setSize({ barWidth * (static_cast<float>(exp) / nextLevelExp), barHeight });
            expBar.setPosition(expBack.getPosition());

            auto hpBounds = hpIcon.getGlobalBounds();
            auto hungerBounds = hungerIcon.getGlobalBounds();
            auto thirstBounds = thirstIcon.getGlobalBounds();

            hpIcon.setPosition({
                hpBack.getPosition().x - hpBounds.size.x - iconPadding,
                hpBack.getPosition().y + (barHeight - hpBounds.size.y) / 2.f
                });

            hungerIcon.setPosition({
                hungerBack.getPosition().x - hungerBounds.size.x - iconPadding,
                hungerBack.getPosition().y + (barHeight - hungerBounds.size.y) / 2.f
                });

            thirstIcon.setPosition({
                thirstBack.getPosition().x - thirstBounds.size.x - iconPadding,
                thirstBack.getPosition().y + (barHeight - thirstBounds.size.y) / 2.f
                });

            // TEXT CONTENT
            hpText.setString("HP " + std::to_string((int)hp) + " / " + std::to_string((int)maxHP));
            hungerText.setString("Hunger " + std::to_string((int)(maxHunger - hunger)));
            thirstText.setString("Thirst " + std::to_string((int)(maxThirst - thirst)));

            hpText.setCharacterSize(16);
            hungerText.setCharacterSize(16);
            thirstText.setCharacterSize(16);

            // TEXT POSITION (inside bar)
            hpText.setPosition({
                hpBack.getPosition().x + textPadding,
                hpBack.getPosition().y + 2.f
                });

            hungerText.setPosition({
                hungerBack.getPosition().x + textPadding,
                hungerBack.getPosition().y + 2.f
                });

            thirstText.setPosition({
                thirstBack.getPosition().x + textPadding,
                thirstBack.getPosition().y + 2.f
                });

            // Level Text
            levelText.setString("Lv." + std::to_string(level) + " EXP: " + std::to_string(exp) + "/" + std::to_string(level * 100));
            levelText.setPosition({ startX, startY - 40.f });
        }

        while (std::optional<sf::Event> event = window.pollEvent())
        {
            if (quantityPromptOpen) {
                quantityInput.processEvent(event.value(), window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));
                if (auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyEvent->code == sf::Keyboard::Key::Enter) {
                        int amt = 1;
                        try {
                            std::string text = quantityInput.getText();
                            if (!text.empty()) amt = std::stoi(text);
                        } catch (...) {
                            amt = 1;
                        }
                        if (amt < 1) amt = 1;
                        if (amt > quantityPromptMax) amt = quantityPromptMax;

                        if (quantityPromptIsCampfire) {
                            PlacedCampfire* activeFire = nullptr;
                            for (auto& f : campfires) {
                                if (f.menuOpen) {
                                    activeFire = &f;
                                    break;
                                }
                            }
                            if (activeFire) {
                                int itemID = inventory.getItemID(quantityPromptSourceSlot);
                                if (quantityPromptCampfireSlot == 1) { // Fuel
                                    if (itemID == ITEM_WOOD) {
                                        activeFire->fuel += amt * 4;
                                    } else if (itemID == ITEM_SAPLING) {
                                        activeFire->fuel += amt;
                                        saplingPlacementMode = false;
                                    }
                                } else if (quantityPromptCampfireSlot == 2) { // Raw Food
                                    activeFire->inputItem = itemID;
                                    activeFire->inputCount += amt;
                                }
                                for (int c = 0; c < amt; c++) {
                                    inventory.consumeItem(quantityPromptSourceSlot);
                                }
                                syncSaveLocalStructuresAndCampfires();
                            }
                        } else if (quantityPromptIsP2P) {
                            myP2PSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                            myP2PSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                            myP2PSlots[quantityPromptTargetSlot].count = amt;
                            myP2PSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                            for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                            p2pOfferDirty = true; p2pMyReady = false;
                        } else {
                            playerTradeSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                            playerTradeSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                            playerTradeSlots[quantityPromptTargetSlot].count = amt;
                            playerTradeSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                            for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                            isSellingToNPC = true;

                            npcTradeGold = 0;
                            for (int j = 0; j < 4; j++) {
                                if (playerTradeSlots[j].tex != nullptr) {
                                    float demandMul = 0.75f;
                                    for (auto& mkt : stockMarket) {
                                        if (mkt.id == playerTradeSlots[j].id) {
                                            demandMul = (mkt.trend == "UP") ? 0.90f : (mkt.trend == "DOWN") ? 0.60f : 0.75f;
                                            break;
                                        }
                                    }
                                    npcTradeGold += (int)(getItemPrice(playerTradeSlots[j].id) * demandMul) * playerTradeSlots[j].count;
                                }
                            }
                            npcGoldInput = std::to_string(npcTradeGold);
                            bargainAccepted = false;
                        }
                        quantityPromptOpen = false;
                    } else if (keyEvent->code == sf::Keyboard::Key::Escape) {
                        quantityPromptOpen = false;
                    }
                }
                if (auto* mouseButtonEvent = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mouseButtonEvent->button == sf::Mouse::Button::Left) {
                        sf::Vector2f uiPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
                        sf::Vector2f qBoxPos = { ((float)width - 400.f) / 2.f, ((float)height - 220.f) / 2.f };
                        sf::FloatRect confirmBtnRect({ qBoxPos.x + 30.f, qBoxPos.y + 150.f }, { 160.f, 40.f });
                        sf::FloatRect cancelBtnRect({ qBoxPos.x + 210.f, qBoxPos.y + 150.f }, { 160.f, 40.f });

                        if (confirmBtnRect.contains(uiPos)) {
                            int amt = 1;
                            try {
                                std::string text = quantityInput.getText();
                                if (!text.empty()) amt = std::stoi(text);
                            } catch (...) {
                                amt = 1;
                            }
                            if (amt < 1) amt = 1;
                            if (amt > quantityPromptMax) amt = quantityPromptMax;

                            if (quantityPromptIsCampfire) {
                                PlacedCampfire* activeFire = nullptr;
                                for (auto& f : campfires) {
                                    if (f.menuOpen) {
                                        activeFire = &f;
                                        break;
                                    }
                                }
                                if (activeFire) {
                                    int itemID = inventory.getItemID(quantityPromptSourceSlot);
                                    if (quantityPromptCampfireSlot == 1) { // Fuel
                                        if (itemID == ITEM_WOOD) {
                                            activeFire->fuel += amt * 4;
                                        } else if (itemID == ITEM_SAPLING) {
                                            activeFire->fuel += amt;
                                            saplingPlacementMode = false;
                                        }
                                    } else if (quantityPromptCampfireSlot == 2) { // Raw Food
                                        activeFire->inputItem = itemID;
                                        activeFire->inputCount += amt;
                                    }
                                    for (int c = 0; c < amt; c++) {
                                        inventory.consumeItem(quantityPromptSourceSlot);
                                    }
                                    syncSaveLocalStructuresAndCampfires();
                                }
                            } else if (quantityPromptIsP2P) {
                                myP2PSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                                myP2PSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                                myP2PSlots[quantityPromptTargetSlot].count = amt;
                                myP2PSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                                for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                                p2pOfferDirty = true; p2pMyReady = false;
                            } else {
                                playerTradeSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                                playerTradeSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                                playerTradeSlots[quantityPromptTargetSlot].count = amt;
                                playerTradeSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                                for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                                isSellingToNPC = true;

                                npcTradeGold = 0;
                                for (int j = 0; j < 4; j++) {
                                    if (playerTradeSlots[j].tex != nullptr) {
                                        float demandMul = 0.75f;
                                        for (auto& mkt : stockMarket) {
                                            if (mkt.id == playerTradeSlots[j].id) {
                                                demandMul = (mkt.trend == "UP") ? 0.90f : (mkt.trend == "DOWN") ? 0.60f : 0.75f;
                                                break;
                                            }
                                        }
                                        npcTradeGold += (int)(getItemPrice(playerTradeSlots[j].id) * demandMul) * playerTradeSlots[j].count;
                                    }
                                }
                                npcGoldInput = std::to_string(npcTradeGold);
                                bargainAccepted = false;
                            }
                            quantityPromptOpen = false;
                        } else if (cancelBtnRect.contains(uiPos)) {
                            quantityPromptOpen = false;
                        }
                    }
                }
                continue;
            }
            bool clickHandled = false;
            if (isChatting) {
                chatInput.processEvent(event.value(), window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));
            }
            if (iapVerifyingOpen) {
                iapCodeInput.processEvent(event.value(), window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));
            }

            if (auto* textEvent = event->getIf<sf::Event::TextEntered>())
            {
                uint32_t unicode = textEvent->unicode;

                if (playerGoldTyping)
                {
                    if (unicode >= '0' && unicode <= '9')
                    {
                        if (playerGoldInput == "0")
                            playerGoldInput.clear();

                        playerGoldInput += static_cast<char>(unicode);
                    }
                    else if (unicode == 8 && !playerGoldInput.empty())
                    {
                        playerGoldInput.pop_back();
                    }

                    if (playerGoldInput.empty())
                        playerGoldInput = "0";

                    playerTradeGold = std::stoi(playerGoldInput);

                    if (playerTradeGold > currency)
                    {
                        playerTradeGold = currency;
                        playerGoldInput = std::to_string(currency);
                    }
                }
                else if (p2pGoldTyping)
                {
                    if (unicode >= '0' && unicode <= '9')
                    {
                        if (myP2PGoldInput == "0")
                            myP2PGoldInput.clear();

                        myP2PGoldInput += static_cast<char>(unicode);
                    }
                    else if (unicode == 8 && !myP2PGoldInput.empty())
                    {
                        myP2PGoldInput.pop_back();
                    }

                    if (myP2PGoldInput.empty())
                        myP2PGoldInput = "0";

                    myP2PGold = std::stoi(myP2PGoldInput);

                    if (myP2PGold > currency)
                    {
                        myP2PGold = currency;
                        myP2PGoldInput = std::to_string(currency);
                    }
                    p2pOfferDirty = true;
                    p2pOfferPushTimer = 0.2f;
                    p2pMyReady = false;
                }

                // NPC gold is AI-controlled only
                npcGoldTyping = false;
            }
            if (auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == globalSettings.keyChat || key->scancode == sf::Keyboard::Scancode::T) {
                    if (!isChatting) {
                        chatActive = !chatActive;
                        if (!chatActive) {
                            isChatting = false;
                            chatInput.setFocus(false);
                            chatInput.clear();
                        }
                    }
                }
                else if (key->scancode == sf::Keyboard::Scancode::Enter) {
                    if (chatActive) {
                        if (!isChatting) {
                            isChatting = true;
                            chatInput.setFocus(true);
                        }
                        else {
                            std::string msg = chatInput.getText();
                            if (!msg.empty()) {
                                if (msg.find("/ban ") == 0) {
                                    std::string rest = msg.substr(5);
                                    size_t spacePos = rest.find(' ');
                                    if (spacePos == std::string::npos) {
                                        latestPlayerMsg = "Usage: /ban <username> <3|7|14|30|perm>";
                                        bubbleTimer = 5.f;
                                    }
                                    else {
                                        std::string target = rest.substr(0, spacePos);
                                        std::string duration = rest.substr(spacePos + 1);
                                        std::string executorRole = authManager.getUserRole(username.c_str());
                                        if (executorRole != "moderator") {
                                            latestPlayerMsg = "Error: Only moderators can ban.";
                                            bubbleTimer = 5.f;
                                        }
                                        else {
                                            std::string feedback;
                                            authManager.banUser(target, duration, feedback);
                                            latestPlayerMsg = feedback;
                                            bubbleTimer = 5.f;
                                        }
                                    }
                                }
                                else if (msg.find("/unban ") == 0) {
                                    std::string target = msg.substr(7);
                                    std::string executorRole = authManager.getUserRole(username.c_str());
                                    if (executorRole != "moderator") {
                                        latestPlayerMsg = "Error: Only moderators can unban.";
                                        bubbleTimer = 5.f;
                                    }
                                    else {
                                        std::string feedback;
                                        authManager.unbanUser(target, feedback);
                                        latestPlayerMsg = feedback;
                                        bubbleTimer = 5.f;
                                    }
                                }
                                else if (msg.find("/mute ") == 0) {
                                    std::string rest = msg.substr(6);
                                    size_t spacePos = rest.find(' ');
                                    std::string target = rest;
                                    int minutes = 60; // default 1 hour
                                    if (spacePos != std::string::npos) {
                                        target = rest.substr(0, spacePos);
                                        try {
                                            minutes = std::stoi(rest.substr(spacePos + 1));
                                        }
                                        catch (...) {
                                            minutes = 60;
                                        }
                                    }
                                    std::string executorRole = authManager.getUserRole(username.c_str());
                                    if (executorRole != "moderator") {
                                        latestPlayerMsg = "Error: Only moderators can mute.";
                                        bubbleTimer = 5.f;
                                    }
                                    else {
                                        std::string feedback;
                                        authManager.muteUser(target, minutes, feedback);
                                        latestPlayerMsg = feedback;
                                        bubbleTimer = 5.f;
                                    }
                                }
                                else if (msg.find("/unmute ") == 0) {
                                    std::string target = msg.substr(8);
                                    std::string executorRole = authManager.getUserRole(username.c_str());
                                    if (executorRole != "moderator") {
                                        latestPlayerMsg = "Error: Only moderators can unmute.";
                                        bubbleTimer = 5.f;
                                    }
                                    else {
                                        std::string feedback;
                                        authManager.unmuteUser(target, feedback);
                                        latestPlayerMsg = feedback;
                                        bubbleTimer = 5.f;
                                    }
                                }
                                else if (msg == "/makemoderator") {
                                    if (authManager.setUserRole(username.c_str(), "moderator")) {
                                        latestPlayerMsg = "You are now a moderator!";
                                    }
                                    else {
                                        latestPlayerMsg = "Failed to set role.";
                                    }
                                    bubbleTimer = 5.f;
                                }
                                else if (msg.find("/pull ") == 0) {
                                    std::string target = msg.substr(6);
                                    authManager.sendCommand(username.c_str(), target.c_str(), "pull");
                                    latestPlayerMsg = "Command Sent: /pull " + target;
                                    bubbleTimer = 3.f;
                                }
                                else if (msg == "/who") {
                                    std::string onlineList = "Players Online: ";
                                    for (size_t i = 0; i < otherPlayers.size(); ++i) {
                                        onlineList += otherPlayers[i].username;
                                        if (i < otherPlayers.size() - 1) onlineList += ", ";
                                    }
                                    if (otherPlayers.empty()) {
                                        onlineList += "Only you.";
                                    }
                                    latestPlayerMsg = onlineList;
                                    bubbleTimer = 5.f;
                                }
                                else if (msg == "/help" || msg == "/?") {
                                    std::string executorRole = authManager.getUserRole(username.c_str());
                                    if (executorRole == "moderator") {
                                        latestPlayerMsg = "Commands:\n/pull (name) - Pull player\n/who - See online players\n/ban (name) (3|7|14|30|perm)\n/unban (name)\n/mute (name) [minutes]\n/unmute (name)\n/makemoderator\n/help - Show commands";
                                    }
                                    else {
                                        latestPlayerMsg = "Commands:\n/pull (name) - Pull player\n/who - See online players\n/help - Show commands";
                                    }
                                    bubbleTimer = 10.f;
                                }
                                else {
                                    std::string chatErr;
                                    if (authManager.sendChatMessage(username.c_str(), msg.c_str(), chatErr)) {
                                        if (tutorialActive && tutorialStep == 6) tutChatted = true;
                                        latestPlayerMsg = msg;
                                        bubbleTimer = 5.0f;
                                    }
                                    else {
                                        latestPlayerMsg = chatErr.empty() ? "Failed to send message." : chatErr;
                                        bubbleTimer = 5.0f;
                                    }
                                }
                                chatInput.clear();
                            }
                            isChatting = false;
                            chatInput.setFocus(false);
                        }
                    }
                }
                else if (key->scancode == sf::Keyboard::Scancode::Escape && isChatting) {
                    isChatting = false;
                    chatInput.setFocus(false);
                    chatInput.clear();
                }
                else if (key->scancode == globalSettings.keyRotate) {
                    if (placementMode) {
                        placementRotation = (placementRotation == 0.f) ? 90.f : ((placementRotation == 90.f) ? 180.f : 0.f);
                    }
                    else if (saplingPlacementMode) {
                        saplingRotation += 90.f;
                        if (saplingRotation >= 360.f) saplingRotation -= 360.f;
                    }
                }
                else if (key->scancode == globalSettings.keyInteract && !isChatting && !placementMode) {
                    // Try to interact with Door
                    PlacedStructure* nearestDoor = nullptr;
                    float minDist = 300.f;
                    for (auto& s : structures) {
                        if (s.id == ITEM_WOOD_DOOR) {
                            sf::Vector2f dPos = s.sprite->getPosition();
                            sf::Vector2f pPos = sprite.getPosition();
                            int doorFloor = getFloorLevelFromY(dPos.y);
                            if (doorFloor == playerFloorLevel) {
                                float dy = (dPos.y + doorFloor * 180.f) - pPos.y;
                                float dx = dPos.x - pPos.x;
                                float d = std::sqrt(dx*dx + dy*dy);
                                if (d < minDist) {
                                    minDist = d;
                                    nearestDoor = &s;
                                }
                            }
                        }
                    }
                    if (nearestDoor) {
                        nearestDoor->isOpen = !nearestDoor->isOpen;
                        float ignore;
                        applyStructureScale(*nearestDoor->sprite, nearestDoor->id, nearestDoor->rotation, nearestDoor->isOpen, ignore);
                        syncSaveLocalStructuresAndCampfires();
                    }
                }
            }

            if (auto* te = event->getIf<sf::Event::TextEntered>()) {
                if (stockPageOpen && marketSearchActive) {
                    if (te->unicode == 8) { // Backspace
                        if (!marketSearchText.empty())
                            marketSearchText.pop_back();
                    }
                    else if (te->unicode >= 32 && te->unicode < 127) {
                        marketSearchText += static_cast<char>(te->unicode);
                    }
                    marketScrollOffset = 0; // Reset scroll on type
                }
            }

            if (auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (wheel->wheel == sf::Mouse::Wheel::Vertical) {
                    sf::Vector2f uiPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
                    if (sf::FloatRect({ chatBoxX, chatBoxY }, { chatBoxWidth, chatBoxHeight }).contains(uiPos)) {
                        chatScroll -= wheel->delta * 30.f;
                        if (chatScroll < 0.f) chatScroll = 0.f;
                    }
                    sf::FloatRect sBoxRect({ (width - 900.f) / 2.f, (height - 600.f) / 2.f }, { 900.f, 600.f });
                    if (stockPageOpen && sBoxRect.contains(uiPos)) {
                        marketScrollOffset -= (int)wheel->delta;
                        if (marketScrollOffset < 0) marketScrollOffset = 0;
                    }
                }
            }

            if (auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    for (auto& fire : campfires)
                    {
                        sf::FloatRect fireBounds({
                            fire.position.x - 50.f,
                            fire.position.y - 50.f },
                            { 100.f,
                            100.f }
                            );

                        if (fireBounds.contains(worldPos))
                        {
                            for (int i = 0; i < 20; i++)
                            {
                                if (inventory.hasItem(i))
                                {
                                    if (inventory.getItemID(i) == ITEM_WRENCH)
                                    {
                                        if (fire.owner.empty() || fire.owner == username)
                                            fire.menuOpen = true;
                                    }
                                }
                            }
                        }
                    }

                    sf::Vector2f uiPos = window.mapPixelToCoords(mouse->position, GetUIView(&window));
                    if (sf::FloatRect({ chatBoxX, chatBoxY }, { chatBoxWidth, chatBoxHeight }).contains(uiPos)) {
                        isDraggingChat = true;
                        lastMousePosChat = uiPos;
                    }
                    else {
                        // Check Campfire Menu Button Clicks
                        bool campfireMenuClickHandled = false;
                        for (auto& fire : campfires) {
                            if (fire.menuOpen) {
                                sf::Vector2f menuPos(
                                    (float)width / 2.f - 250.f,
                                    (float)height / 2.f - 150.f
                                );
                                
                                sf::FloatRect closeBtn(
                                    { menuPos.x + 440.f, menuPos.y + 10.f },
                                    { 40.f, 40.f }
                                );
                                sf::FloatRect fuelBtn(
                                    { menuPos.x + 60.f, menuPos.y + 110.f },
                                    { 80.f, 80.f }
                                );
                                sf::FloatRect cookBtn(
                                    { menuPos.x + 210.f, menuPos.y + 110.f },
                                    { 80.f, 80.f }
                                );
                                sf::FloatRect takeBtn(
                                    { menuPos.x + 360.f, menuPos.y + 110.f },
                                    { 80.f, 80.f }
                                );
                                
                                if (closeBtn.contains(uiPos)) {
                                    fire.menuOpen = false;
                                    clickHandled = true;
                                    campfireMenuClickHandled = true;
                                    break;
                                }
                                else if (fuelBtn.contains(uiPos)) {
                                    if (fire.fuel > 0) {
                                        // Retrieve fuel back to inventory
                                        int woodReturn = fire.fuel / 4;
                                        int saplingReturn = fire.fuel % 4;
                                        if (woodReturn > 0) {
                                            inventory.addItem(ITEM_WOOD, woodTex, woodReturn);
                                        }
                                        if (saplingReturn > 0) {
                                            inventory.addItem(ITEM_SAPLING, saplingTex, saplingReturn);
                                        }
                                        fire.fuel = 0;
                                        syncSaveLocalStructuresAndCampfires();
                                    } else {
                                        // Add fuel
                                        int selSlot = inventory.getSelectedSlot();
                                        if (selSlot != -1 && inventory.hasItem(selSlot)) {
                                            int itemID = inventory.getItemID(selSlot);
                                            if (itemID == ITEM_WOOD || itemID == ITEM_SAPLING) {
                                                int totalQty = inventory.getItemCount(selSlot);
                                                if (totalQty > 1) {
                                                    quantityPromptOpen = true;
                                                    quantityPromptMax = totalQty;
                                                    quantityPromptTargetSlot = 0;
                                                    quantityPromptSourceSlot = selSlot;
                                                    quantityPromptIsP2P = false;
                                                    quantityPromptIsCampfire = true;
                                                    quantityPromptCampfireSlot = 1; // 1 = Fuel
                                                    quantityInput.clear();
                                                    quantityInput.setFocus(true);
                                                } else {
                                                    if (itemID == ITEM_WOOD) {
                                                        fire.fuel += 4;
                                                    } else {
                                                        fire.fuel += 1;
                                                        if (itemID == ITEM_SAPLING) {
                                                            saplingPlacementMode = false;
                                                        }
                                                    }
                                                    inventory.consumeItem(selSlot);
                                                    syncSaveLocalStructuresAndCampfires();
                                                }
                                            }
                                        }
                                    }
                                    clickHandled = true;
                                    campfireMenuClickHandled = true;
                                    break;
                                }
                                else if (cookBtn.contains(uiPos)) {
                                    if (fire.inputCount > 0 && fire.inputItem != 0) {
                                        // Retrieve raw food back to inventory
                                        const sf::Texture* rTex = (fire.inputItem == ITEM_CHICKEN_MEAT) ? ChickenMeat::getTexture() : 
                                                                  (fire.inputItem == ITEM_PIG_MEAT) ? PigMeat::getTexture() : 
                                                                  (fire.inputItem == ITEM_SHEEP_MEAT) ? SheepMeat::getTexture() : nullptr;
                                        if (rTex) {
                                            inventory.addItem(fire.inputItem, *rTex, fire.inputCount);
                                        }
                                        fire.inputCount = 0;
                                        fire.inputItem = 0;
                                        syncSaveLocalStructuresAndCampfires();
                                    } else {
                                        // Add raw food
                                        int selSlot = inventory.getSelectedSlot();
                                        if (selSlot != -1 && inventory.hasItem(selSlot)) {
                                            int itemID = inventory.getItemID(selSlot);
                                            if (itemID == ITEM_CHICKEN_MEAT || itemID == ITEM_PIG_MEAT || itemID == ITEM_SHEEP_MEAT) {
                                                int totalQty = inventory.getItemCount(selSlot);
                                                if (totalQty > 1) {
                                                    quantityPromptOpen = true;
                                                    quantityPromptMax = totalQty;
                                                    quantityPromptTargetSlot = 0;
                                                    quantityPromptSourceSlot = selSlot;
                                                    quantityPromptIsP2P = false;
                                                    quantityPromptIsCampfire = true;
                                                    quantityPromptCampfireSlot = 2; // 2 = Raw Food
                                                    quantityInput.clear();
                                                    quantityInput.setFocus(true);
                                                } else {
                                                    fire.inputItem = itemID;
                                                    fire.inputCount = 1;
                                                    inventory.consumeItem(selSlot);
                                                    syncSaveLocalStructuresAndCampfires();
                                                }
                                            }
                                        }
                                    }
                                    clickHandled = true;
                                    campfireMenuClickHandled = true;
                                    break;
                                }
                                else if (takeBtn.contains(uiPos)) {
                                    if (fire.outputCount > 0) {
                                        if (fire.outputItem == ITEM_COOKED_CHICKEN) {
                                            inventory.addItem(ITEM_COOKED_CHICKEN, cookedChickenTex, fire.outputCount);
                                        }
                                        else if (fire.outputItem == ITEM_COOKED_PIG) {
                                            inventory.addItem(ITEM_COOKED_PIG, cookedPigTex, fire.outputCount);
                                        }
                                        else if (fire.outputItem == ITEM_COOKED_SHEEP) {
                                            inventory.addItem(ITEM_COOKED_SHEEP, cookedSheepTex, fire.outputCount);
                                        }
                                        fire.outputCount = 0;
                                        fire.outputItem = 0;
                                        syncSaveLocalStructuresAndCampfires();
                                    }
                                    clickHandled = true;
                                    campfireMenuClickHandled = true;
                                    break;
                                }
                            }
                        }

                        if (!campfireMenuClickHandled) {
                            // Check Campfire Wrench click in world coordinates (no distance check)
                            for (auto& fire : campfires) {
                                if (fire.isCrafted) {
                                    sf::FloatRect wrenchBtnBounds({ fire.position.x - 20.5f, fire.position.y - 480.f - 20.5f }, { 41.f, 41.f });
                                    if (wrenchBtnBounds.contains(worldPos)) {
                                        int selSlot = inventory.getSelectedSlot();
                                        bool hasWrench = false;
                                        if (selSlot != -1 && inventory.hasItem(selSlot) && inventory.getItemID(selSlot) == ITEM_WRENCH) {
                                            hasWrench = true;
                                        }
                                        for (int i = 0; i < 20; i++) {
                                            if (inventory.hasItem(i) && inventory.getItemID(i) == ITEM_WRENCH && inventory.isEquipped(i)) {
                                                hasWrench = true;
                                                break;
                                            }
                                        }
                                        if (hasWrench) {
                                            for (auto& f : campfires) f.menuOpen = false;
                                            if (fire.owner.empty() || fire.owner == username)
                                                fire.menuOpen = true;
                                            clickHandled = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        if (!clickHandled) {
                            // Check Door Interaction Button in world coordinates
                            PlacedStructure* nearestDoor = nullptr;
                            float minDist = 300.f;
                            for (auto& s : structures) {
                                if (s.id == ITEM_WOOD_DOOR) {
                                    sf::Vector2f dPos = s.sprite->getPosition();
                                    sf::Vector2f pPos = sprite.getPosition();
                                    int doorFloor = getFloorLevelFromY(dPos.y);
                                    if (doorFloor == playerFloorLevel) {
                                        float dy = (dPos.y + doorFloor * 180.f) - pPos.y;
                                        float dx = dPos.x - pPos.x;
                                        float d = std::sqrt(dx*dx + dy*dy);
                                        if (d < minDist) {
                                            minDist = d;
                                            nearestDoor = &s;
                                        }
                                    }
                                }
                            }
                            if (nearestDoor) {
                                sf::FloatRect doorBtnBounds({ nearestDoor->sprite->getPosition().x - 100.f, nearestDoor->sprite->getPosition().y - 120.f }, { 200.f, 60.f });
                                if (doorBtnBounds.contains(worldPos)) {
                                    nearestDoor->isOpen = !nearestDoor->isOpen;
                                    float ignore;
                                    applyStructureScale(*nearestDoor->sprite, nearestDoor->id, nearestDoor->rotation, nearestDoor->isOpen, ignore);
                                    clickHandled = true;
                                    syncSaveLocalStructuresAndCampfires();
                                }
                            }
                            // Check Bed Interaction Button in world coordinates
                            PlacedStructure* nearestBed = nullptr;
                            float minBedDist = 180.f;
                            for (auto& s : structures) {
                                if (s.id == ITEM_BED) {
                                    sf::Vector2f bPos = s.sprite->getPosition();
                                    sf::Vector2f pPos = sprite.getPosition();
                                    int bedFloor = getFloorLevelFromY(bPos.y);
                                    if (bedFloor == playerFloorLevel) {
                                        float dy = (bPos.y + bedFloor * 180.f) - pPos.y;
                                        float dx = bPos.x - pPos.x;
                                        float d = std::sqrt(dx*dx + dy*dy);
                                        if (d < minBedDist) {
                                            minBedDist = d;
                                            nearestBed = &s;
                                        }
                                    }
                                }
                            }
                            if (nearestBed && !isSleeping) {
                                sf::FloatRect sleepBtnBounds({ nearestBed->sprite->getPosition().x - 100.f, nearestBed->sprite->getPosition().y - 120.f }, { 200.f, 60.f });
                                if (sleepBtnBounds.contains(worldPos)) {
                                    isSleeping = true;
                                    sleepFadeTimer = 0.f;
                                    sleepTimeSkipped = false;
                                    clickHandled = true;
                                }
                            }
                        }
                    }
                }


            }
            if (auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    isDraggingChat = false;
                }
            }
            if (auto* move = event->getIf<sf::Event::MouseMoved>()) {
                if (isDraggingChat) {
                    sf::Vector2f uiPos = window.mapPixelToCoords(move->position, GetUIView(&window));
                    chatScroll -= (uiPos.y - lastMousePosChat.y);
                    if (chatScroll < 0.f) chatScroll = 0.f;
                    lastMousePosChat = uiPos;
                }
            }
            if (event->is<sf::Event::Closed>())
            {
                for (int i = 0; i < 4; i++) {
                    if (playerTradeSlots[i].tex != nullptr) {
                        inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                        playerTradeSlots[i] = {};
                    }
                }
                for (int i = 0; i < 4; i++) {
                    if (myP2PSlots[i].tex != nullptr) {
                        inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
                        myP2PSlots[i] = {};
                    }
                }
                if (p2pTradeID > 0) {
                    authManager.cancelP2PTrade(p2pTradeID);
                }

                std::vector<std::pair<int, int>> items(20, { 0, 0 });
                for (int i = 0; i < 20; i++) {
                    if (inventory.hasItem(i)) {
                        items[i] = { inventory.getItemID(i), inventory.getItemCount(i) };
                    }
                }
                authManager.saveInventory(username.c_str(), items);

                float currentElapsed = elapsedGameTime;
                authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, currentElapsed, hungerMultiplier, thirstMultiplier);

                std::vector<StoredStructure> structData;
                for (auto& s : structures) {
                    if (s.sprite.has_value() && (s.owner == username || s.owner == "")) {
                        structData.push_back({ s.id, s.sprite->getPosition().x, s.sprite->getPosition().y, s.health, s.rotation, s.isOpen, s.shearY });
                    }
                }
                authManager.saveStructures(username.c_str(), structData);

                std::vector<StoredCampfire> campData;
                for (auto& c : campfires) {
                    campData.push_back({
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
                authManager.saveCampfires(username.c_str(), campData);

                window.close();
            }

            if (auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape)
                {
                    if (tradeMenuOpen || tradeConfirmOpen) {
                        tradeConfirmOpen = false;
                        tradeMenuOpen = false;
                        tradeBoxesTicked = false;
                        for (int i = 0; i < 4; i++) {
                            if (playerTradeSlots[i].tex != nullptr) {
                                inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                                playerTradeSlots[i].tex = nullptr;
                                playerTradeSlots[i].count = 0;
                                playerTradeSlots[i].id = 0;
                            }
                            npcTradeSlots[i].tex = nullptr;
                            npcTradeSlots[i].count = 0;
                            npcTradeSlots[i].id = 0;
                        }
                        continue;
                    }
                    for (int i = 0; i < 4; i++) {
                        if (playerTradeSlots[i].tex != nullptr) {
                            inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                            playerTradeSlots[i] = {};
                        }
                    }
                    for (int i = 0; i < 4; i++) {
                        if (myP2PSlots[i].tex != nullptr) {
                            inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
                            myP2PSlots[i] = {};
                        }
                    }
                    if (p2pTradeID > 0) {
                        authManager.cancelP2PTrade(p2pTradeID);
                    }

                    std::vector<std::pair<int, int>> items(20, { 0, 0 });
                    for (int i = 0; i < 20; i++) {
                        if (inventory.hasItem(i)) {
                            items[i] = { inventory.getItemID(i), inventory.getItemCount(i) };
                        }
                    }
                    std::cout << "Saving inventory...\n";
                    authManager.saveInventory(username.c_str(), items);

                    float currentElapsed = elapsedGameTime;
                    authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, currentElapsed, hungerMultiplier, thirstMultiplier);

                    std::vector<StoredStructure> structData;
                    for (auto& s : structures) {
                        if (s.sprite.has_value() && (s.owner == username || s.owner == "")) {
                            structData.push_back({ s.id, s.sprite->getPosition().x, s.sprite->getPosition().y, s.health, s.rotation, s.isOpen, s.shearY });
                        }
                    }
                    authManager.saveStructures(username.c_str(), structData);

                    std::vector<StoredCampfire> campData;
                    for (auto& c : campfires) {
                        campData.push_back({
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
                    authManager.saveCampfires(username.c_str(), campData);

                    std::cout << "Saved!\n";

                    // ✅ CLOSE GAME (or break loop)
                    window.close();
                }

                if (key->scancode == globalSettings.keyInventory && !invKeyHeld)
                {
                    inventoryOpen = !inventoryOpen;

                    if (inventoryOpen && tutorialActive && tutorialStep == 3)
                        tutInvOpened = true;

                    invKeyHeld = true;
                }
            }

            if (auto* key = event->getIf<sf::Event::KeyReleased>())
            {
                if (key->scancode == globalSettings.keyInventory)
                    invKeyHeld = false;
            }

            if (auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                sf::Vector2f uiPos = window.mapPixelToCoords(mouse->position, GetUIView(&window));
                if (mouse->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f mpos((float)mouse->position.x, (float)mouse->position.y);

                    bool clickHandled = false;
                    playerGoldTyping = false;
                    p2pGoldTyping = false;

                    if (stockPageOpen) {
                        sf::FloatRect sBoxRect({ (width - 900.f) / 2.f, (height - 600.f) / 2.f }, { 900.f, 600.f });
                        sf::FloatRect closeBtnRect({ sBoxRect.position.x + 840.f, sBoxRect.position.y + 20.f }, { 40.f, 40.f });
                        sf::FloatRect searchBarRect({ sBoxRect.position.x + 40.f, sBoxRect.position.y + 65.f }, { 680.f, 32.f });
                        if (closeBtnRect.contains(uiPos)) {
                            stockPageOpen = false;
                            marketSearchText = "";
                            marketSearchActive = false;
                            marketScrollOffset = 0;
                        }
                        else {
                            marketSearchActive = searchBarRect.contains(uiPos);
                        }
                        clickHandled = true;
                        continue;
                    }


                    if (iapPageOpen && !iapVerifyingOpen) {
                        sf::FloatRect sBoxRect({ (width - 900.f) / 2.f, (height - 600.f) / 2.f }, { 900.f, 600.f });
                        sf::FloatRect closeBtnRect({ sBoxRect.position.x + 840.f, sBoxRect.position.y + 20.f }, { 40.f, 40.f });
                        if (closeBtnRect.contains(uiPos)) {
                            iapPageOpen = false;
                        }
                        else {
                            float cardW = 190.f;
                            float cardH = 360.f;
                            float cardY = sBoxRect.position.y + 160.f;
                            float cardGap = 20.f;
                            float startX = sBoxRect.position.x + 45.f;

                            for (int i = 0; i < 4; i++) {
                                sf::FloatRect buyBtnRect({ startX + i * (cardW + cardGap) + 15.f, cardY + cardH - 55.f }, { cardW - 30.f, 40.f });
                                if (buyBtnRect.contains(uiPos)) {
                                    int gold = 100;
                                    float price = 0.99f;
                                    std::string tierName = "Handful of Gold";
                                    if (i == 1) { gold = 1000; price = 9.99f; tierName = "Sack of Gold"; }
                                    else if (i == 2) { gold = 5000; price = 39.99f; tierName = "Chest of Gold"; }
                                    else if (i == 3) { gold = 10000; price = 79.99f; tierName = "Vault of Gold"; }

                                    iapVerifyingOpen = true;
                                    iapVerifyingGold = gold;
                                    iapVerifyingPrice = price;
                                    iapVerifyingTier = tierName;
                                    iapCodeInput.clear();
                                    iapCodeInput.setFocus(true);

#ifdef _WIN32
                                    bool useRealStripeGateway = true; // Toggle to false to use local PowerShell sandbox instead!

                                    std::string priceStr = std::to_string(price);
                                    priceStr = priceStr.substr(0, priceStr.find(".") + 3);

                                    std::string urlTier = tierName;
                                    size_t spacePos = urlTier.find(" ");
                                    while (spacePos != std::string::npos) {
                                        urlTier.replace(spacePos, 1, "%20");
                                        spacePos = urlTier.find(" ");
                                    }

                                    if (useRealStripeGateway) {
                                        // Open live hosted Node.js Express server which securely handles Stripe CheckoutSession creation & redirects
                                        std::string url = "https://econova-qfhu.onrender.com/checkout?user=" + username + "&gold=" + std::to_string(gold) + "&price=" + priceStr + "&tier=" + urlTier;
                                        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                                    }
                                    else {
                                        // 1. Launch PowerShell local server in background (hidden)
                                        ShellExecuteA(NULL, "open", "powershell.exe", "-WindowStyle Hidden -ExecutionPolicy Bypass -File ..\\payment_portal\\local_server.ps1", NULL, SW_HIDE);

                                        // 2. Open local payment portal with solved absolute path
                                        char fullPath[MAX_PATH] = { 0 };
                                        std::string url = "";
                                        if (_fullpath(fullPath, "payment_portal/index.html", MAX_PATH) != NULL) {
                                            std::string absPath = fullPath;
                                            std::replace(absPath.begin(), absPath.end(), '\\', '/');
                                            url = absPath + "?user=" + username + "&gold=" + std::to_string(gold) + "&price=" + priceStr + "&tier=" + urlTier;
                                        }
                                        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                                    }
#endif
                                }
                            }
                        }
                        clickHandled = true;
                        continue;
                    }

                    if (iapVerifyingOpen) {
                        sf::FloatRect vBoxRect({ (width - 500.f) / 2.f, (height - 320.f) / 2.f }, { 500.f, 320.f });

                        sf::FloatRect verifyBtnRect({ vBoxRect.position.x + 50.f, vBoxRect.position.y + 240.f }, { 190.f, 45.f });
                        sf::FloatRect cancelBtnRect({ vBoxRect.position.x + 260.f, vBoxRect.position.y + 240.f }, { 190.f, 45.f });

                        if (verifyBtnRect.contains(uiPos)) {
                            std::string enteredCode = iapCodeInput.getText();
                            bool success = false;

                            std::string prefix = "GOLD-" + std::to_string(iapVerifyingGold);
                            if (enteredCode.find(prefix) == 0) {
                                success = true;
                            }

                            if (!success && fileExists("payment_success.json")) {
                                std::ifstream file("payment_success.json");
                                if (file.is_open()) {
                                    std::string line;
                                    std::string verifiedUser = "";
                                    int verifiedGold = 0;
                                    while (std::getline(file, line)) {
                                        if (line.find("username") != std::string::npos) {
                                            size_t start = line.find("\"", line.find(":"));
                                            size_t end = line.find("\"", start + 1);
                                            if (start != std::string::npos && end != std::string::npos) {
                                                verifiedUser = line.substr(start + 1, end - start - 1);
                                            }
                                        }
                                        if (line.find("gold") != std::string::npos) {
                                            std::string goldStr = "";
                                            for (char c : line) {
                                                if (std::isdigit(c)) goldStr += c;
                                            }
                                            if (!goldStr.empty()) verifiedGold = std::stoi(goldStr);
                                        }
                                    }
                                    file.close();
                                    if (verifiedUser == username && verifiedGold == iapVerifyingGold) {
                                        success = true;
                                    }
                                }
                            }

                            if (success) {
                                currency += iapVerifyingGold;
                                float currentElapsed = elapsedGameTime;
                                authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, currentElapsed, hungerMultiplier, thirstMultiplier);
                                latestPlayerMsg = "Payment Verified! +" + std::to_string(iapVerifyingGold) + " Gold synced to account.";
                                iapVerifyingOpen = false;
                                iapPageOpen = false;

                                if (fileExists("payment_success.json")) {
                                    std::remove("payment_success.json");
                                }

#ifdef _WIN32
                                ShellExecuteA(NULL, "open", "taskkill.exe", "/F /IM powershell.exe", NULL, SW_HIDE);
#endif
                            }
                            else {
                                latestPlayerMsg = "Verification failed. Code invalid or payment incomplete.";
                            }
                        }
                        else if (cancelBtnRect.contains(uiPos)) {
                            iapVerifyingOpen = false;
                            latestPlayerMsg = "Transaction cancelled.";

#ifdef _WIN32
                            ShellExecuteA(NULL, "open", "taskkill.exe", "/F /IM powershell.exe", NULL, SW_HIDE);
#endif
                        }

                        clickHandled = true;
                        continue;
                    }

                    sf::FloatRect settingsBtnRect({ (float)width - 140.f, 20.f }, { 120.f, 40.f });
                    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && settingsBtnRect.contains(uiPos)) {
                        openSettingsWindow(window, width, height);
                        deltaClock.restart();
                        continue;
                    }

                    sf::FloatRect marketBtnRect({ (float)width - 300.f, 20.f }, { 150.f, 40.f });
                    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && marketBtnRect.contains(uiPos)) {
                        stockPageOpen = true;
                        continue;
                    }

                    // ✅ Online player click → open wrench with "Trade" option
                    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !p2pTradeOpen) {
                        for (const auto& op : otherPlayers) {
                            auto dispIt = onlinePlayerDisplayPos.find(op.username);
                            sf::Vector2f opPos = (dispIt != onlinePlayerDisplayPos.end()) ? dispIt->second : sf::Vector2f(op.x, op.y);
                            sf::FloatRect opBounds({ opPos.x - 30.f, opPos.y - 50.f }, { 60.f, 100.f });
                            bool hitWrench = false;
                            {
                                auto wbIt = onlinePlayerWrenchBounds.find(op.username);
                                if (wbIt != onlinePlayerWrenchBounds.end())
                                    hitWrench = wbIt->second.contains(worldPos);
                            }
                            if (opBounds.contains(worldPos) || hitWrench) {
                                wrenchMenuOpen = true;
                                isTargetNPC = false;
                                wrenchTargetPlayer = op.username;
                                wrenchMenuPos = uiPos;
                                clickHandled = true;
                                break;
                            }
                        }
                    }

                    // ✅ Allow wrench click only if no other menus are open
                    if (!clickHandled && !wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && playerWrenchBtn.getGlobalBounds().contains(worldPos)) {
                        wrenchMenuOpen = true;
                        isTargetNPC = false;
                        wrenchTargetPlayer = "";
                        wrenchMenuPos = uiPos;
                        continue;   // stop further click handling
                    }
                    else if (!clickHandled && !wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && npcFemale.wrenchBtn->getGlobalBounds().contains(worldPos)) {
                        wrenchMenuOpen = true;
                        isTargetNPC = true;
                        wrenchTargetPlayer = "";
                        wrenchMenuPos = uiPos;
                        continue;   // stop further click handling
                    }
                    else if (!clickHandled && !wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && npcMale.wrenchBtn->getGlobalBounds().contains(worldPos)) {
                        wrenchMenuOpen = true;
                        isTargetNPC = true;
                        wrenchTargetPlayer = "";
                        wrenchMenuPos = uiPos;
                        continue;   // stop further click handling
                    }

                    if (selectionDone && introActive) {
                        currentIntroFrame++;
                        if (currentIntroFrame >= introFrames.size() || (introFrames.empty() && currentIntroFrame >= 1)) {
                            introActive = false;
                            tutorialActive = true;
                            tutorialStep = 0;
                        }
                        else if (!introFrames.empty()) {
                            introSprite->setTexture(introFrames[currentIntroFrame]);
                        }
                        clickHandled = true;
                    }
                    else if (selectionDone && tutorialActive) {
                        bool goalMet = true;
                        if (tutorialStep == 1) goalMet = (tutMoveDist >= 500.f);
                        else if (tutorialStep == 2) goalMet = tutSprinted;
                        else if (tutorialStep == 3) goalMet = tutInvOpened;
                        else if (tutorialStep == 4) goalMet = (tutWoodGathered >= 1 && tutStoneGathered >= 1);
                        else if (tutorialStep == 5) goalMet = tutAttacked;
                        else if (tutorialStep == 6) goalMet = tutChatted;

                        sf::FloatRect nextBtn({ tutorialBox.getPosition().x + 650.f, tutorialBox.getPosition().y + 330.f }, { 120.f, 40.f });
                        if (goalMet && nextBtn.contains(uiPos)) {
                            tutorialStep++;
                            if (tutorialStep >= tutorialSteps.size()) {
                                tutorialActive = false;
                                authManager.setFirstTimeFinished(username.c_str());
                                latestPlayerMsg = "You can now get daily quests from the NPC!";
                                bubbleTimer = 5.0f;
                            }
                        }
                        clickHandled = true;
                    }
                    // =====================================================
                    // P2P INVITE POPUP (target player accepts/declines)
                    // =====================================================
                    else if (p2pInvitePopup) {
                        float pw = 400.f, ph = 160.f;
                        float px = (width - pw) / 2.f, py = (height - ph) / 2.f;
                        sf::FloatRect acceptP({ px + 40.f, py + 100.f }, { 120.f, 40.f });
                        sf::FloatRect declineP({ px + 240.f, py + 100.f }, { 120.f, 40.f });
                        if (acceptP.contains(uiPos)) {
                            authManager.acceptP2PTrade(p2pTradeID);
                            p2pInvitePopup  = false;
                            p2pTradeOpen    = true;
                            p2pAmInitiator  = false;
                            p2pPartner      = p2pState.initiator;
                            p2pMyReady      = false;
                            p2pPartnerReady = false;
                            p2pStatusMsg    = "Trade open! Add items and gold, then press Ready.";
                            for (int i = 0; i < 4; i++) myP2PSlots[i] = {};
                            myP2PGold = 0; myP2PGoldInput = "0";
                            partnerItems.clear(); partnerGold = 0;
                            clickHandled = true;
                        }
                        else if (declineP.contains(uiPos)) {
                            authManager.cancelP2PTrade(p2pTradeID);
                            p2pInvitePopup = false;
                            p2pTradeID = -1;
                            clickHandled = true;
                        }
                    }
                    // =====================================================
                    // P2P TRADE WINDOW CLICK HANDLER
                    // =====================================================
                    else if (p2pTradeOpen) {
                        float tW = 700.f, tH = 440.f;
                        sf::FloatRect tR({ (width - tW) / 2.f, (height - tH) / 2.f }, { tW, tH });

                        // My item slots (left side, 4 slots)
                        for (int i = 0; i < 4; i++) {
                            sf::FloatRect sr({ tR.position.x + 20.f + i * 74.f, tR.position.y + 80.f }, { 64.f, 64.f });
                            if (sr.contains(uiPos)) {
                                if (myP2PSlots[i].tex) {
                                    // Return to inventory
                                    inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
                                    myP2PSlots[i] = {};
                                } else {
                                    int sel = inventory.getSelectedSlot();
                                    if (sel != -1 && inventory.hasItem(sel)) {
                                        bool free = true;
                                        for (int j = 0; j < 4; j++) if (myP2PSlots[j].originalSlot == sel) { free = false; break; }
                                        if (free) {
                                            int totalQty = inventory.getItemCount(sel);
                                            if (totalQty > 1) {
                                                quantityPromptOpen = true;
                                                quantityPromptMax = totalQty;
                                                quantityPromptTargetSlot = i;
                                                quantityPromptSourceSlot = sel;
                                                quantityPromptIsP2P = true;
                                                quantityInput.clear();
                                                quantityInput.setFocus(true);
                                            } else {
                                                myP2PSlots[i].id = inventory.getItemID(sel);
                                                myP2PSlots[i].tex = inventory.getItemTexture(sel);
                                                myP2PSlots[i].count = 1;
                                                myP2PSlots[i].originalSlot = sel;
                                                inventory.clearSlot(sel);
                                                p2pOfferDirty = true;
                                                p2pMyReady = false;
                                            }
                                        }
                                    }
                                }
                                clickHandled = true; break;
                            }
                        }

                        // My gold input
                        sf::FloatRect goldBox({ tR.position.x + 20.f, tR.position.y + 165.f }, { 100.f, 34.f });
                        if (!clickHandled && goldBox.contains(uiPos)) { p2pGoldTyping = true; playerGoldTyping = false; clickHandled = true; }

                        // Ready button
                        sf::FloatRect readyBtn({ tR.position.x + 20.f, tR.position.y + 380.f }, { 120.f, 40.f });
                        if (!clickHandled && readyBtn.contains(uiPos) && !p2pMyReady) {
                            p2pMyReady = true;
                            authManager.setP2PReady(p2pTradeID, username, true);
                            // Push offer immediately
                            std::string myCSV;
                            for (int i = 0; i < 4; i++) {
                                if (myP2PSlots[i].tex) {
                                    if (!myCSV.empty()) myCSV += ",";
                                    myCSV += std::to_string(myP2PSlots[i].id) + ":" + std::to_string(myP2PSlots[i].count);
                                }
                            }
                            authManager.updateP2POffer(p2pTradeID, username, myCSV, myP2PGold);
                            p2pStatusMsg = "You are ready! Waiting for " + p2pPartner + "...";
                            clickHandled = true;
                        }

                        // Unready button
                        sf::FloatRect unreadyBtn({ tR.position.x + 155.f, tR.position.y + 380.f }, { 120.f, 40.f });
                        if (!clickHandled && unreadyBtn.contains(uiPos) && p2pMyReady) {
                            p2pMyReady = false;
                            authManager.setP2PReady(p2pTradeID, username, false);
                            p2pStatusMsg = "Not ready. Adjust your offer.";
                            clickHandled = true;
                        }

                        // Cancel button
                        sf::FloatRect cancelBtn({ tR.position.x + tW - 130.f, tR.position.y + 380.f }, { 120.f, 40.f });
                        if (!clickHandled && cancelBtn.contains(uiPos)) {
                            authManager.cancelP2PTrade(p2pTradeID);
                            for (int i = 0; i < 4; i++) {
                                if (myP2PSlots[i].tex) { inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count); myP2PSlots[i] = {}; }
                            }
                            p2pTradeOpen = false; p2pTradeID = -1; p2pPartner = "";
                            p2pMyReady = false; p2pPartnerReady = false;
                            partnerItems.clear(); partnerGold = 0;
                            clickHandled = true;
                        }

                        // Fall through to inventory for item selection
                        if (!clickHandled) { inventory.handleClick(mouse->position, window); clickHandled = true; }
                    }
                    else if (tradeConfirmOpen) {
                        sf::FloatRect yesRect({ (width - 250.f) / 2.f, height / 2.f }, { 100.f, 40.f });
                        sf::FloatRect noRect({ (width + 50.f) / 2.f, height / 2.f }, { 100.f, 40.f });
                        if (yesRect.contains(uiPos)) {
                            tradeConfirmOpen = false;
                            tradeMenuOpen = false;
                            tradeBoxesTicked = false;

                            // ✅ LOG PLAYER SOLD ITEMS (SELL) & UPDATE MARKET PRICES
                            for (int i = 0; i < 4; i++) {
                                if (playerTradeSlots[i].tex != nullptr && playerTradeSlots[i].count > 0) {
                                    std::string name = "";
                                    int price = 0;
                                    for (auto& item : stockMarket) {
                                        if (item.id == playerTradeSlots[i].id) {
                                            name = item.name;
                                            // Reduce price due to increased supply
                                            item.currentPrice *= (1.0f - (bargainAccepted ? 0.008f : 0.005f) * playerTradeSlots[i].count);
                                            float minP = item.basePrice * 0.4f;
                                            float maxP = item.basePrice * 2.5f;
                                            if (item.currentPrice < minP) item.currentPrice = minP;
                                            if (item.currentPrice > maxP) item.currentPrice = maxP;
                                            item.currentPrice = std::round(item.currentPrice);
                                            if (item.currentPrice < 1.f) item.currentPrice = 1.f;

                                            if (!item.history.empty()) {
                                                float lastPrice = item.history.back();
                                                float diff = item.currentPrice - lastPrice;
                                                item.predictedPrice = std::round(item.currentPrice + diff * 0.5f);
                                                if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;
                                                if (item.predictedPrice > item.currentPrice * 1.02f) item.trend = "UP";
                                                else if (item.predictedPrice < item.currentPrice * 0.98f) item.trend = "DOWN";
                                                else item.trend = "STABLE";
                                            }
                                            price = (int)item.currentPrice;
                                            break;
                                        }
                                    }
                                    if (name.empty()) {
                                        if (playerTradeSlots[i].id == ITEM_CHICKEN_MEAT) name = "Chicken Meat";
                                        else if (playerTradeSlots[i].id == ITEM_PIG_MEAT) name = "Pig Meat";
                                        else if (playerTradeSlots[i].id == ITEM_SHEEP_MEAT) name = "Sheep Meat";
                                        else if (playerTradeSlots[i].id == ITEM_WOOL) name = "Wool";
                                        else if (playerTradeSlots[i].id == ITEM_ZOMBIE_FLESH) name = "Zombie Flesh";
                                        else if (playerTradeSlots[i].id == ITEM_WOODEN_SWORD) name = "Wooden Sword";
                                        else if (playerTradeSlots[i].id == ITEM_SHIELD) name = "Shield";
                                        else name = "Unknown";
                                    }
                                    authManager.addTradeRecord(username.c_str(), "SELL", name, playerTradeSlots[i].count, price, playerTradeSlots[i].count * price);
                                }
                            }

                            // ✅ LOG PLAYER BOUGHT ITEMS (BUY) & UPDATE MARKET PRICES
                            for (int i = 0; i < 4; i++) {
                                if (npcTradeSlots[i].tex != nullptr && npcTradeSlots[i].count > 0) {
                                    std::string name = "";
                                    int price = 0;
                                    for (auto& item : stockMarket) {
                                        if (item.id == npcTradeSlots[i].id) {
                                            name = item.name;
                                            // Increase price due to demand, or decrease by 5% if bargained down
                                            if (bargainAccepted) {
                                                item.currentPrice *= 0.95f;
                                            }
                                            else {
                                                item.currentPrice *= (1.0f + 0.008f * npcTradeSlots[i].count);
                                            }
                                            float minP = item.basePrice * 0.4f;
                                            float maxP = item.basePrice * 2.5f;
                                            if (item.currentPrice < minP) item.currentPrice = minP;
                                            if (item.currentPrice > maxP) item.currentPrice = maxP;
                                            item.currentPrice = std::round(item.currentPrice);
                                            if (item.currentPrice < 1.f) item.currentPrice = 1.f;

                                            if (!item.history.empty()) {
                                                float lastPrice = item.history.back();
                                                float diff = item.currentPrice - lastPrice;
                                                item.predictedPrice = std::round(item.currentPrice + diff * 0.5f);
                                                if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;
                                                if (item.predictedPrice > item.currentPrice * 1.02f) item.trend = "UP";
                                                else if (item.predictedPrice < item.currentPrice * 0.98f) item.trend = "DOWN";
                                                else item.trend = "STABLE";
                                            }
                                            price = (int)item.currentPrice;
                                            break;
                                        }
                                    }
                                    if (name.empty()) {
                                        if (npcTradeSlots[i].id == ITEM_CHICKEN_MEAT) name = "Chicken Meat";
                                        else if (npcTradeSlots[i].id == ITEM_PIG_MEAT) name = "Pig Meat";
                                        else if (npcTradeSlots[i].id == ITEM_SHEEP_MEAT) name = "Sheep Meat";
                                        else if (npcTradeSlots[i].id == ITEM_WOOL) name = "Wool";
                                        else if (npcTradeSlots[i].id == ITEM_ZOMBIE_FLESH) name = "Zombie Flesh";
                                        else if (npcTradeSlots[i].id == ITEM_WOODEN_SWORD) name = "Wooden Sword";
                                        else if (npcTradeSlots[i].id == ITEM_SHIELD) name = "Shield";
                                        else name = "Unknown";
                                    }
                                    authManager.addTradeRecord(username.c_str(), "BUY", name, npcTradeSlots[i].count, price, npcTradeSlots[i].count * price);
                                }
                            }

                            // ✅ TRANSFER GOLD
                            // Player earns money from selling
                            currency += playerTradeGold;

                            // Player spends money buying
                            currency -= npcTradeGold;

                            playerTradeGold = 0;
                            npcTradeGold = 0;
                            hasBargained = false;
                            bargainAccepted = false;
                            bargainStatus = "";
                            bargainColor = sf::Color::White;

                            // TRANSFER NPC ITEMS TO PLAYER
                            for (int i = 0; i < 4; i++) {
                                if (npcTradeSlots[i].tex != nullptr && npcTradeSlots[i].count > 0) {
                                    inventory.addItem(npcTradeSlots[i].id, *npcTradeSlots[i].tex, npcTradeSlots[i].count);
                                    npcTradeSlots[i].tex = nullptr;
                                    npcTradeSlots[i].count = 0;
                                    npcTradeSlots[i].id = 0;
                                }
                                playerTradeSlots[i].tex = nullptr;
                                playerTradeSlots[i].count = 0;
                                playerTradeSlots[i].id = 0;
                            }

                            // ✅ SAVE ONCE
                            std::vector<std::pair<int, int>> items(20, { 0, 0 });
                            for (int i = 0; i < 20; i++) {
                                if (inventory.hasItem(i)) {
                                    items[i] = { inventory.getItemID(i), inventory.getItemCount(i) };
                                }
                            }
                            authManager.saveInventory(username.c_str(), items);

                            float currentElapsed = elapsedGameTime;
                            authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, currentElapsed, hungerMultiplier, thirstMultiplier);

                            clickHandled = true;
                        }
                        else if (noRect.contains(uiPos)) {
                            tradeConfirmOpen = false;
                            tradeMenuOpen = false;
                            tradeBoxesTicked = false;

                            // Cancel Trade! Return items to Inventory
                            for (int i = 0; i < 4; i++) {
                                if (playerTradeSlots[i].tex != nullptr) {
                                    inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                                    playerTradeSlots[i].tex = nullptr;
                                    playerTradeSlots[i].count = 0;
                                    playerTradeSlots[i].id = 0;
                                }
                                npcTradeSlots[i].tex = nullptr;
                                npcTradeSlots[i].count = 0;
                                npcTradeSlots[i].id = 0;
                            }
                            hasBargained = false;
                            bargainAccepted = false;
                            bargainStatus = "";
                            bargainColor = sf::Color::White;
                            clickHandled = true;
                        }
                    }

                    else if (tradeMenuOpen) {
                        sf::FloatRect tBoxRect({ (width - 640.f) / 2.f, (height - 420.f) / 2.f }, { 640.f, 420.f });

                        // --- Player trade slot hit zones (4 slots) ---
                        for (int i = 0; i < 4; i++) {
                            sf::FloatRect slotRect({ tBoxRect.position.x + 20.f + i * 74.f, tBoxRect.position.y + 70.f }, { 64.f, 64.f });
                            if (slotRect.contains(uiPos)) {
                                if (playerTradeSlots[i].tex != nullptr) {
                                    // Return item to inventory
                                    inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                                    playerTradeSlots[i] = {};
                                    // Recheck if any items left — if none, switch to buy mode
                                    bool anyLeft = false;
                                    for (int j = 0; j < 4; j++) if (playerTradeSlots[j].tex) anyLeft = true;
                                    if (!anyLeft) isSellingToNPC = false;
                                } else {
                                    // Move selected inventory item into this slot
                                    int selSlot = inventory.getSelectedSlot();
                                    if (selSlot != -1 && inventory.hasItem(selSlot)) {
                                        bool slotFree = true;
                                        for (int j = 0; j < 4; j++) {
                                            if (playerTradeSlots[j].originalSlot == selSlot) { slotFree = false; break; }
                                        }
                                        if (slotFree) {
                                            int totalQty = inventory.getItemCount(selSlot);
                                            if (totalQty > 1) {
                                                quantityPromptOpen = true;
                                                quantityPromptMax = totalQty;
                                                quantityPromptTargetSlot = i;
                                                quantityPromptSourceSlot = selSlot;
                                                quantityPromptIsP2P = false;
                                                quantityInput.clear();
                                                quantityInput.setFocus(true);
                                            } else {
                                                playerTradeSlots[i].id = inventory.getItemID(selSlot);
                                                playerTradeSlots[i].tex = inventory.getItemTexture(selSlot);
                                                playerTradeSlots[i].count = 1;
                                                playerTradeSlots[i].originalSlot = selSlot;
                                                inventory.clearSlot(selSlot);
                                                isSellingToNPC = true;
                                            }
                                        }
                                    }
                                }
                                // Auto-recalculate NPC sell offer
                                if (isSellingToNPC) {
                                    npcTradeGold = 0;
                                    for (int j = 0; j < 4; j++) {
                                        if (playerTradeSlots[j].tex != nullptr) {
                                            float demandMul = 0.75f;
                                            for (auto& mkt : stockMarket) {
                                                if (mkt.id == playerTradeSlots[j].id) {
                                                    demandMul = (mkt.trend == "UP") ? 0.90f : (mkt.trend == "DOWN") ? 0.60f : 0.75f;
                                                    break;
                                                }
                                            }
                                            npcTradeGold += (int)(getItemPrice(playerTradeSlots[j].id) * demandMul) * playerTradeSlots[j].count;
                                        }
                                    }
                                    npcGoldInput = std::to_string(npcTradeGold);
                                }
                                bargainAccepted = false;
                                hasBargained = false;
                                bargainStatus = "";
                                bargainColor = sf::Color::White;
                                clickHandled = true;
                                break;
                            }
                        }

                        // --- NPC item slots for buy mode (select/deselect) ---
                        if (!clickHandled && !isSellingToNPC) {
                            for (int i = 0; i < 4; i++) {
                                if (npcTradeSlots[i].tex == nullptr) continue;
                                sf::FloatRect nSlotRect({ tBoxRect.position.x + 20.f + i * 74.f, tBoxRect.position.y + 172.f }, { 64.f, 64.f });
                                if (nSlotRect.contains(uiPos)) {
                                    // Toggle selected (originalSlot 1=selected, 0=not)
                                    npcTradeSlots[i].originalSlot = (npcTradeSlots[i].originalSlot == 1) ? 0 : 1;
                                    bargainStatus = "";
                                    clickHandled = true;
                                    break;
                                }
                            }
                        }

                        // --- Player Gold input box ---
                        sf::FloatRect playerGoldBox({ tBoxRect.position.x + 360.f, tBoxRect.position.y + 85.f }, { 120.f, 35.f });
                        if (!clickHandled && playerGoldBox.contains(uiPos)) {
                            playerGoldTyping = true;
                            clickHandled = true;
                        }

                        // --- Stock Stats button ---
                        sf::FloatRect statsBtnRect({ tBoxRect.position.x + 360.f, tBoxRect.position.y + 200.f }, { 150.f, 36.f });
                        if (!clickHandled && statsBtnRect.contains(uiPos)) {
                            stockPageOpen = true;
                            clickHandled = true;
                        }

                        // --- Bargain button ---
                        sf::FloatRect bargainRect({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 355.f }, { 100.f, 40.f });
                        if (!clickHandled && bargainRect.contains(uiPos)) {
                            if (isSellingToNPC && npcTradeGold > 0 && !hasBargained && !bargainAccepted) {
                                // SELL bargain: NPC raises offer
                                if ((rand() % 100) < 40) {
                                    bargainAccepted = true;
                                    npcTradeGold = (int)(npcTradeGold * 1.15f);
                                    npcGoldInput = std::to_string(npcTradeGold);
                                    bargainStatus = "NPC: Fine! I'll add a little more.";
                                    bargainColor = sf::Color::Green;
                                } else {
                                    hasBargained = true;
                                    bargainStatus = "NPC: No deal! Take it or leave it.";
                                    bargainColor = sf::Color::Red;
                                }
                                clickHandled = true;
                            } else if (!isSellingToNPC && !hasBargained && !bargainAccepted) {
                                // BUY bargain: try to get 10% discount on selected items
                                bool anySelected = false;
                                for (int i = 0; i < 4; i++) if (npcTradeSlots[i].originalSlot == 1) anySelected = true;
                                if (anySelected) {
                                    if ((rand() % 100) < 35) {
                                        bargainAccepted = true;
                                        bargainStatus = "NPC: Alright, 10% off just for you!";
                                        bargainColor = sf::Color::Green;
                                    } else {
                                        hasBargained = true;
                                        bargainStatus = "NPC: Full price, take it or leave it!";
                                        bargainColor = sf::Color::Red;
                                    }
                                    clickHandled = true;
                                }
                            }
                        }

                        // --- Accept button ---
                        sf::FloatRect acceptRect({ tBoxRect.position.x + 500.f, tBoxRect.position.y + 355.f }, { 100.f, 40.f });
                        if (!clickHandled && acceptRect.contains(uiPos)) {
                            if (isSellingToNPC) {
                                // SELL: player receives NPC gold
                                bool hasItems = false;
                                for (int i = 0; i < 4; i++) if (playerTradeSlots[i].tex) hasItems = true;
                                if (hasItems && npcTradeGold > 0) {
                                    currency += npcTradeGold;
                                    for (int i = 0; i < 4; i++) {
                                        if (playerTradeSlots[i].tex) {
                                            int item_id = playerTradeSlots[i].id;
                                            int item_qty = playerTradeSlots[i].count;
                                            std::string name = getItemName(item_id);
                                            int uPrice = getItemPrice(item_id);
                                            int totalVal = item_qty * uPrice;

                                            // Directly affect market stats (supply decreases price)
                                            float percentChange = -0.005f * item_qty;
                                            adjustMarketItem(item_id, percentChange);

                                            // Log trade history record
                                            authManager.addTradeRecord(username.c_str(), "SELL", name, item_qty, uPrice, totalVal);

                                            playerTradeSlots[i] = {};
                                        }
                                    }
                                    latestPlayerMsg = "Sold! Received " + std::to_string(npcTradeGold) + " Gold.";
                                    bubbleTimer = 4.f;
                                    tradeMenuOpen = false;
                                    isSellingToNPC = false;
                                    npcTradeGold = 0; npcGoldInput = "0";
                                    playerTradeGold = 0; playerGoldInput = "0";
                                    bargainAccepted = false; hasBargained = false; bargainStatus = "";
                                    authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
                                } else {
                                    bargainStatus = "Add items to your trade slots first!";
                                    bargainColor = sf::Color::Red;
                                }
                            } else {
                                // BUY: calculate total cost of selected NPC items at 120% (or 108% if bargained)
                                float priceMul = bargainAccepted ? 1.08f : 1.20f;
                                int totalCost = 0;
                                bool anySelected = false;
                                for (int i = 0; i < 4; i++) {
                                    if (npcTradeSlots[i].originalSlot == 1 && npcTradeSlots[i].tex) {
                                        anySelected = true;
                                        totalCost += (int)(getItemPrice(npcTradeSlots[i].id) * npcTradeSlots[i].count * priceMul);
                                    }
                                }
                                if (!anySelected) {
                                    bargainStatus = "Select NPC items to buy first!";
                                    bargainColor = sf::Color::Red;
                                } else if (totalCost > currency) {
                                    bargainStatus = "Not enough gold! Need " + std::to_string(totalCost) + " G.";
                                    bargainColor = sf::Color::Red;
                                } else {
                                    // Deduct gold and give items to player
                                    currency -= totalCost;
                                    for (int i = 0; i < 4; i++) {
                                        if (npcTradeSlots[i].originalSlot == 1 && npcTradeSlots[i].tex) {
                                            inventory.addItem(npcTradeSlots[i].id, *npcTradeSlots[i].tex, npcTradeSlots[i].count);
                                            
                                            int item_id = npcTradeSlots[i].id;
                                            int item_qty = npcTradeSlots[i].count;
                                            std::string name = getItemName(item_id);
                                            int uPrice = getItemPrice(item_id);
                                            int totalVal = item_qty * uPrice;

                                            // Directly affect market stats (demand increases price)
                                            float percentChange = 0.005f * item_qty;
                                            adjustMarketItem(item_id, percentChange);

                                            // Log trade history record
                                            authManager.addTradeRecord(username.c_str(), "BUY", name, item_qty, uPrice, totalVal);

                                            npcTradeSlots[i] = {};
                                        }
                                    }
                                    latestPlayerMsg = "Bought items for " + std::to_string(totalCost) + " Gold.";
                                    bubbleTimer = 4.f;
                                    tradeMenuOpen = false;
                                    isSellingToNPC = false;
                                    playerTradeGold = 0; playerGoldInput = "0";
                                    npcTradeGold = 0; npcGoldInput = "0";
                                    bargainAccepted = false; hasBargained = false; bargainStatus = "";
                                    authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
                                }
                            }
                            clickHandled = true;
                        }

                        // --- Cancel button ---
                        sf::FloatRect cancelRect({ tBoxRect.position.x + 390.f, tBoxRect.position.y + 355.f }, { 100.f, 40.f });
                        if (!clickHandled && cancelRect.contains(uiPos)) {
                            for (int i = 0; i < 4; i++) {
                                if (playerTradeSlots[i].tex != nullptr) {
                                    inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                                    playerTradeSlots[i] = {};
                                }
                                npcTradeSlots[i].originalSlot = 0; // deselect all
                            }
                            tradeMenuOpen = false;
                            isSellingToNPC = false;
                            playerTradeGold = 0; playerGoldInput = "0";
                            npcTradeGold = 0; npcGoldInput = "0";
                            hasBargained = false; bargainAccepted = false; bargainStatus = "";
                            bargainColor = sf::Color::White;
                            clickHandled = true;
                        }

                        // Allow inventory clicks while trade open (to select items)
                        if (!clickHandled) {
                            inventory.handleClick(mouse->position, window);
                            clickHandled = true;
                        }
                    }
                    else if (wrenchMenuOpen) {

                    if (isTargetNPC) {
                        // ✅ TRADE & QUEST & STOCK STATS for NPC
                        sf::FloatRect tradeBound({ wrenchMenuPos.x, wrenchMenuPos.y }, { 150.f, 30.f });
                        sf::FloatRect questBound({ wrenchMenuPos.x, wrenchMenuPos.y + 30.f }, { 150.f, 30.f });
                        sf::FloatRect statsBound({ wrenchMenuPos.x, wrenchMenuPos.y + 60.f }, { 150.f, 30.f });
                        sf::FloatRect iapBound({ wrenchMenuPos.x, wrenchMenuPos.y + 90.f }, { 150.f, 30.f });



                        if (tradeBound.contains(uiPos)) {
                            tradeMenuOpen = true;
                            wrenchMenuOpen = false;

                            // Clear Trade Slots on Open
                            for (int i = 0; i < 4; i++) {
                                playerTradeSlots[i].tex = nullptr;
                                playerTradeSlots[i].count = 0;
                                playerTradeSlots[i].id = 0;
                                npcTradeSlots[i].tex = nullptr;
                                npcTradeSlots[i].count = 0;
                                npcTradeSlots[i].id = 0;
                            }
                            hasBargained = false;
                            bargainAccepted = false;
                            bargainStatus = "";
                            bargainColor = sf::Color::White;

                            // Populate NPC slots randomly with 1-3 items
                            int itemsCount = 1 + (rand() % 3);
                            std::vector<int> possibleIds = {
                                ITEM_WOOD, ITEM_STONE, ITEM_SAPLING,
                                ITEM_CHICKEN_MEAT, ITEM_PIG_MEAT, ITEM_SHEEP_MEAT, ITEM_WOOL, ITEM_ZOMBIE_FLESH,
                                ITEM_COOKED_CHICKEN, ITEM_COOKED_PIG, ITEM_COOKED_SHEEP,
                                ITEM_WOODEN_SWORD, ITEM_SHIELD, ITEM_FURNACE, ITEM_BED,
                                ITEM_WOOD_TILE, ITEM_WOOD_WALL, ITEM_WOOD_DOOR, ITEM_WOOD_ROOF, ITEM_WOOD_STAIRS
                            };
                            for (int i = 0; i < itemsCount; i++) {
                                int randIdx = rand() % possibleIds.size();
                                int id = possibleIds[randIdx];

                                const sf::Texture* tex = nullptr;
                                if (id == ITEM_WOOD) tex = &woodTex;
                                else if (id == ITEM_STONE) tex = &stoneTex;
                                else if (id == ITEM_SAPLING) tex = &saplingTex;
                                else if (id == ITEM_CHICKEN_MEAT) tex = ChickenMeat::getTexture();
                                else if (id == ITEM_PIG_MEAT) tex = PigMeat::getTexture();
                                else if (id == ITEM_SHEEP_MEAT) tex = SheepMeat::getTexture();
                                else if (id == ITEM_COOKED_CHICKEN) tex = &cookedChickenTex;
                                else if (id == ITEM_COOKED_PIG) tex = &cookedPigTex;
                                else if (id == ITEM_COOKED_SHEEP) tex = &cookedSheepTex;
                                else if (id == ITEM_WOOL) tex = &woolTex;
                                else if (id == ITEM_ZOMBIE_FLESH) tex = ZombieFlesh::getTexture();
                                else if (id == ITEM_WOODEN_SWORD) tex = &woodenSwordTex;
                                else if (id == ITEM_SHIELD) tex = &shieldTex;
                                else if (id == ITEM_FURNACE) tex = &furnaceTex;
                                else if (id == ITEM_BED) tex = &bedTex;
                                else if (id == ITEM_WOOD_TILE) tex = &woodTileTex;
                                else if (id == ITEM_WOOD_WALL) tex = &woodWallTex;
                                else if (id == ITEM_WOOD_DOOR) tex = &woodDoorTex;
                                else if (id == ITEM_WOOD_ROOF) tex = &woodRoofTex;
                                else if (id == ITEM_WOOD_STAIRS) tex = &woodStairsTex;

                                if (tex) {
                                    npcTradeSlots[i].id = id;
                                    npcTradeSlots[i].tex = tex;
                                    if (id == ITEM_WOODEN_SWORD || id == ITEM_SHIELD || id == ITEM_FURNACE || id == ITEM_BED) {
                                        npcTradeSlots[i].count = 1; // Only 1 equipment/furniture unit
                                    }
                                    else if (id == ITEM_WOOD_TILE || id == ITEM_WOOD_WALL || id == ITEM_WOOD_DOOR || id == ITEM_WOOD_ROOF || id == ITEM_WOOD_STAIRS) {
                                        npcTradeSlots[i].count = 2 + (rand() % 4); // 2 to 5 building parts
                                    }
                                    else {
                                        npcTradeSlots[i].count = 1 + (rand() % 8); // 1 to 8 raw materials
                                    }
                                }
                            }

                            tradeBoxesTicked = false;
                            playerTradeGold = 0;

                            npcTradeGold = 0;

                            for (int j = 0; j < 4; j++)
                            {
                                if (npcTradeSlots[j].tex != nullptr)
                                {
                                    npcTradeGold +=
                                        npcTradeSlots[j].count *
                                        getItemPrice(npcTradeSlots[j].id);
                                }
                            }

                            npcGoldInput = std::to_string(npcTradeGold);
                            playerGoldInput = "0";

                            clickHandled = true;
                        }
                        else if (questBound.contains(uiPos)) {
                            int questIdx = qInfo.questID - 1;
                            if (questIdx >= 0 && questIdx < (int)availableQuests.size()) {
                                auto& q = availableQuests[questIdx];
                                if (qInfo.isCompleted) {
                                    latestPlayerMsg = "Quest: " + q.title + " (COMPLETED!)";
                                }
                                else {
                                    latestPlayerMsg = "Quest: " + q.title + "\n" + q.description + " (" + std::to_string(qInfo.progress) + "/" + std::to_string(q.goal) + ")";
                                }
                            }
                            else {
                                latestPlayerMsg = "No quest available right now.";
                            }
                            bubbleTimer = 5.0f;
                            wrenchMenuOpen = false;
                            clickHandled = true;
                        }
                        else if (statsBound.contains(uiPos)) {
                            stockPageOpen = true;
                            wrenchMenuOpen = false;
                            clickHandled = true;
                        }
                        else if (iapBound.contains(uiPos)) {
                            iapPageOpen = true;
                            wrenchMenuOpen = false;
                            clickHandled = true;
                        }
                        else {
                            wrenchMenuOpen = false;
                            clickHandled = true;
                        }
                    }
                    else {
                        // Online player OR own wrench
                        if (!wrenchTargetPlayer.empty()) {
                            // Targeting an online player
                            sf::FloatRect tradeOnlineBound({ wrenchMenuPos.x, wrenchMenuPos.y }, { 150.f, 30.f });
                            sf::FloatRect craftItemBound2({ wrenchMenuPos.x, wrenchMenuPos.y + 30.f }, { 150.f, 30.f });
                            if (tradeOnlineBound.contains(uiPos)) {
                                // Send trade invite
                                p2pTradeID = authManager.createP2PTrade(username, wrenchTargetPlayer);
                                if (p2pTradeID > 0) {
                                    p2pTradeOpen   = true;
                                    p2pAmInitiator = true;
                                    p2pPartner     = wrenchTargetPlayer;
                                    p2pMyReady     = false;
                                    p2pPartnerReady= false;
                                    p2pStatusMsg   = "Waiting for " + wrenchTargetPlayer + " to accept...";
                                    for (int i = 0; i < 4; i++) myP2PSlots[i] = {};
                                    myP2PGold = 0; myP2PGoldInput = "0";
                                    partnerItems.clear(); partnerGold = 0;
                                    p2pPollTimer = 1.f;
                                    latestPlayerMsg = "Trade invite sent to " + wrenchTargetPlayer + "!";
                                    bubbleTimer = 3.f;
                                }
                                wrenchMenuOpen = false;
                                wrenchTargetPlayer = "";
                                clickHandled = true;
                            }
                            else if (craftItemBound2.contains(uiPos)) {
                                craftMenuOpen = true;
                                wrenchMenuOpen = false;
                                wrenchTargetPlayer = "";
                                clickHandled = true;
                            }
                            else {
                                wrenchMenuOpen = false;
                                wrenchTargetPlayer = "";
                                clickHandled = true;
                            }
                        }
                        else {
                            // Own wrench → Craft Items only
                            sf::FloatRect craftItemBound({ wrenchMenuPos.x, wrenchMenuPos.y }, { 150.f, 30.f });
                            if (craftItemBound.contains(uiPos)) {
                                std::cout << "Clicked Craft Items! craftMenuOpen is now true.\n";
                                craftMenuOpen = true;
                                wrenchMenuOpen = false;
                                clickHandled = true;
                            }
                            else {
                                std::cout << "Clicked outside craftItemBound! uiPos: " << uiPos.x << ", " << uiPos.y << " | Box: " << wrenchMenuPos.x << ", " << wrenchMenuPos.y << "\n";
                                wrenchMenuOpen = false;
                                clickHandled = true;
                            }
                        }
                    }
                }

                else if (saplingPlacementMode)
                {
                    float btnW = 220.f, btnH = 70.f, gap = 40.f;
                    float tWidth = btnW * 3.f + gap * 2.f;
                    float sX = (width - tWidth) / 2.f;
                    float yP = height - 110.f;
                    sf::FloatRect placeBtn({ sX, yP }, { btnW, btnH });
                    sf::FloatRect rotateBtn({ sX + btnW + gap, yP }, { btnW, btnH });
                    sf::FloatRect cancelBtn({ sX + (btnW + gap) * 2.f, yP }, { btnW, btnH });

                    if (placeBtn.contains(uiPos))
                    {
                        PlantedSapling s;
                        s.position = saplingPlacementPos;
                        s.growTimer = 40.f;
                        s.grown = false;
                        s.rotation = saplingRotation;

                        plantedSaplings.push_back(s);

                        if (saplingFromInventorySlot != -1) {
                            inventory.consumeItem(saplingFromInventorySlot);
                        } else {
                            for (int i = 0; i < 20; i++)
                            {
                                if (inventory.hasItem(i))
                                {
                                    if (inventory.getItemTexture(i) == &saplingTex)
                                    {
                                        inventory.consumeItem(i);
                                        break;
                                    }
                                }
                            }
                        }

                        saplingPlacementMode = false;
                        clickHandled = true;
                    }
                    else if (rotateBtn.contains(uiPos))
                    {
                        saplingRotation += 90.f;
                        if (saplingRotation >= 360.f) saplingRotation -= 360.f;
                        clickHandled = true;
                    }
                    else if (cancelBtn.contains(uiPos))
                    {
                        saplingPlacementMode = false;
                        clickHandled = true;
                    }
                    else
                    {
                        saplingFollowingCursor = !saplingFollowingCursor;
                        saplingPlacementPos = snapToShearedGrid(worldPos);
                        clickHandled = true;
                    }
                }

                else if (placementMode) {
                    float btnW = 220.f, btnH = 70.f, gap = 40.f;
                    float tWidth = btnW * 3.f + gap * 2.f;
                    float sX = (width - tWidth) / 2.f;
                    float yP = height - 110.f;
                    sf::FloatRect placeBtn({ sX, yP }, { btnW, btnH });
                    sf::FloatRect rotateBtn({ sX + btnW + gap, yP }, { btnW, btnH });
                    sf::FloatRect cancelBtn({ sX + (btnW + gap) * 2.f, yP }, { btnW, btnH });

                    if (placeBtn.contains(uiPos)) {
                        int totalWood = 0, totalStone = 0;
                        if (placementItemID == ITEM_NONE) {
                            if (placementFromInventorySlot != -1) {
                                inventory.consumeItem(placementFromInventorySlot);
                            }
                            PlacedCampfire pc;
                            pc.position = placementPos;
                            pc.timeRemaining = 90.f;
                            pc.isCrafted = false;
                            pc.currentFrame = 0;
                            pc.frameTimer = 0.f;
                            pc.owner = username;
                            campfires.push_back(pc);
                            syncSaveLocalStructuresAndCampfires();

                            if (!tutorialActive && qInfo.questID == 3 && qInfo.progress < 1) {
                                qInfo.progress++;
                                authManager.updateQuestProgress(username.c_str(), qInfo.progress, qInfo.isCompleted);
                            }

                            placementMode = false;
                        }
                        else {
                            if (placementFromInventorySlot != -1) {
                                inventory.consumeItem(placementFromInventorySlot);
                            }
                            // Remove conflicting/duplicate structures at the placement position
                            bool structureRemoved = false;
                            for (auto it = structures.begin(); it != structures.end(); ) {
                                if (it->sprite.has_value() && it->sprite->getPosition() == placementPos) {
                                    bool shouldRemove = false;
                                    if (placementItemID == ITEM_WOOD_DOOR || placementItemID == ITEM_WOOD_WALL) {
                                        if (it->id == ITEM_WOOD_WALL || it->id == ITEM_WOOD_DOOR) {
                                            shouldRemove = true;
                                        }
                                    } else if (it->id == placementItemID) {
                                        shouldRemove = true;
                                    }
                                    if (shouldRemove) {
                                        it = structures.erase(it);
                                        structureRemoved = true;
                                    } else {
                                        ++it;
                                    }
                                } else {
                                    ++it;
                                }
                            }
                            if (structureRemoved) {
                                syncSaveLocalStructuresAndCampfires();
                            }
                            if (placementItemID == ITEM_FURNACE) structures.emplace_back(ITEM_FURNACE, furnaceTex, placementRotation, false);
                            else if (placementItemID == ITEM_WOOD_TILE) structures.emplace_back(ITEM_WOOD_TILE, woodTileTex, placementRotation, false);
                            else if (placementItemID == ITEM_WOOD_WALL) structures.emplace_back(ITEM_WOOD_WALL, woodWallTex, placementRotation, false);
                            else if (placementItemID == ITEM_WOOD_DOOR) structures.emplace_back(ITEM_WOOD_DOOR, woodDoorTex, placementRotation, false);
                            else if (placementItemID == ITEM_WOOD_ROOF) structures.emplace_back(ITEM_WOOD_ROOF, woodRoofTex, placementRotation, false);
                            else if (placementItemID == ITEM_WOOD_STAIRS) structures.emplace_back(ITEM_WOOD_STAIRS, woodStairsTex, placementRotation, false);
                            else if (placementItemID == ITEM_BED) structures.emplace_back(ITEM_BED, bedTex, placementRotation, false);

                            auto& newS = structures.back();
                            sf::Vector2u ts = newS.sprite->getTexture().getSize();
                            newS.sprite->setOrigin({ ts.x / 2.f, (float)ts.y }); // Bottom center for 3D depth

                            applyStructureScale(*newS.sprite, placementItemID, placementRotation, false, newS.shearY);

                            newS.sprite->setPosition(placementPos);
                            syncSaveLocalStructuresAndCampfires();

                            placementMode = false;
                        }
                        clickHandled = true;
                    }
                    else if (rotateBtn.contains(uiPos)) {
                        placementRotation = (placementRotation == 0.f) ? 90.f : ((placementRotation == 90.f) ? 180.f : 0.f);
                        clickHandled = true;
                    }
                    else if (cancelBtn.contains(uiPos)) {
                        placementMode = false;
                        clickHandled = true;
                    }
                    else {
                        // Map Click: Toggle following
                        placementFollowingCursor = !placementFollowingCursor;
                        if (placementFollowingCursor) {
                            placementPos = worldPos;
                        }
                        else {
                            placementPos = worldPos;
                        }

                        // Snap to grid cell origin (floor, matching drawn grid)
                        placementPos = snapToShearedGrid(placementPos);

                        if (placementItemID == ITEM_WOOD_WALL) {
                            if (placementRotation == 90.f) {
                                placementPos = { placementPos.x - 100.f, placementPos.y + 50.f };
                            }
                        }
                        else if (placementItemID == ITEM_WOOD_DOOR) {
                            float bestDist = 150.f;
                            PlacedStructure* nearestWall = nullptr;
                            for (auto& s : structures) {
                                if (s.id == ITEM_WOOD_WALL && s.sprite.has_value()) {
                                    sf::Vector2f wallPos = s.sprite->getPosition();
                                    float dx = worldPos.x - wallPos.x;
                                    float dy = worldPos.y - wallPos.y;
                                    float dist = std::sqrt(dx * dx + dy * dy);
                                    if (dist < bestDist) {
                                        bestDist = dist;
                                        nearestWall = &s;
                                    }
                                }
                            }
                            if (nearestWall) {
                                placementPos = nearestWall->sprite->getPosition();
                                placementRotation = nearestWall->rotation;
                            } else {
                                if (placementRotation == 90.f) {
                                    placementPos = { placementPos.x - 100.f, placementPos.y + 50.f };
                                }
                            }
                        }
                        else if (placementItemID == ITEM_WOOD_ROOF) {
                            placementPos.y -= 180.f;
                        }
                        else if (placementItemID == ITEM_WOOD_STAIRS) {
                            placementPos.y -= 100.f;
                        }
                        clickHandled = true;
                    }
                }
                else if (craftMenuOpen) {
                    sf::FloatRect craftBox({ (width - 1300.f) / 2.f, (height - 950.f) / 2.f }, { 1300.f, 950.f });
                    sf::FloatRect closeBtn({ craftBox.position.x + 1220.f, craftBox.position.y + 10.f }, { 60.f, 60.f });


                    if (closeBtn.contains(uiPos)) {
                        craftMenuOpen = false;
                        clickHandled = true;
                    }
                    else {
                        int totalWood = 0, totalStone = 0, totalWool = 0;
                        for (int i = 0; i < 20; i++) {
                            if (inventory.hasItem(i)) {
                                if (inventory.getItemID(i) == ITEM_WOOD) totalWood += inventory.getItemCount(i);
                                if (inventory.getItemID(i) == ITEM_STONE) totalStone += inventory.getItemCount(i);
                                if (inventory.getItemID(i) == ITEM_WOOL) totalWool += inventory.getItemCount(i);
                            }
                        }

                        float startX = craftBox.position.x + 50.f;
                        float startY = craftBox.position.y + 110.f;
                        float itemW = 380.f, itemH = 190.f;
                        float gapX = 30.f, gapY = 20.f;

                        for (int i = 0; i < (int)craftableItems.size(); i++) {
                            int row = i / 3;
                            int col = i % 3;
                            sf::Vector2f pos(startX + col * (itemW + gapX), startY + row * (itemH + gapY));
                            sf::FloatRect btnRect({ pos.x + 210.f, pos.y + 130.f }, { 140.f, 45.f });
                            if (btnRect.contains(uiPos)) {
                                const auto& item = craftableItems[i];
                                if (totalWood >= item.woodReq && totalStone >= item.stoneReq && totalWool >= item.woolReq) {
                                    // Check if already crafting
                                    if (item.id == ITEM_WOODEN_SWORD && swordCrafting) continue;
                                    if (item.id == ITEM_SHIELD && shieldCrafting) continue;

                                    // Consume resources
                                    int wToC = item.woodReq, sToC = item.stoneReq, woolToC = item.woolReq;
                                    for (int j = 0; j < 20; j++) {
                                        if (inventory.hasItem(j)) {
                                            if (inventory.getItemID(j) == ITEM_WOOD) {
                                                while (wToC > 0 && inventory.getItemCount(j) > 0) { inventory.consumeItem(j); wToC--; }
                                            }
                                            if (inventory.getItemID(j) == ITEM_STONE) {
                                                while (sToC > 0 && inventory.getItemCount(j) > 0) { inventory.consumeItem(j); sToC--; }
                                            }
                                            if (inventory.getItemID(j) == ITEM_WOOL) {
                                                while (woolToC > 0 && inventory.getItemCount(j) > 0) { inventory.consumeItem(j); woolToC--; }
                                            }
                                        }
                                    }

                                    if (item.id == ITEM_WOODEN_SWORD) { swordCrafting = true; swordCraftTimer = 30.f; }
                                    else if (item.id == ITEM_SHIELD) { shieldCrafting = true; shieldCraftTimer = 60.f; }
                                    else {
                                        inventory.addItem(item.id, *item.tex, 1);
                                    }
                                    clickHandled = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                else if (!selectionDone)
                {
                    if (button1.getGlobalBounds().contains(uiPos))
                    {
                        option = 1;
                        selectionDone = true;
                        authManager.saveGender(username.c_str(), option);
                        clickHandled = true;
                    }
                    else if (button2.getGlobalBounds().contains(uiPos))
                    {
                        option = 2;
                        selectionDone = true;
                        authManager.saveGender(username.c_str(), option);
                        clickHandled = true;
                    }
                 }


                 if (mouse->button == sf::Mouse::Button::Left && !clickHandled && inventoryOpen)
                {
                    int clickedSlot = inventory.getSlotAt(uiPos);
                    if (clickedSlot != -1)
                    {
                        // Always allow slot selection
                        inventory.handleClick(mouse->position, window);

                        if (inventory.hasItem(clickedSlot))
                        {
                            const sf::Texture* tex = inventory.getItemTexture(clickedSlot);
                            bool campfireOpen = false;
                            for (auto& f : campfires) {
                                if (f.menuOpen) {
                                    campfireOpen = true;
                                    break;
                                }
                            }
                            // Only block consumption during trade or cooking
                            if (!tradeMenuOpen && !p2pTradeOpen && !campfireOpen)
                            {
                                if (tex == &saplingTex)
                                {
                                    saplingPlacementMode = true;
                                    saplingFollowingCursor = false;
                                    saplingFromInventorySlot = clickedSlot;
                                    saplingRotation = 0.f;
                                    inventoryOpen = false;
                                    clickHandled = true;
                                }

                                int itemId = inventory.getItemID(clickedSlot);
                                if ((itemId >= ITEM_FURNACE && itemId <= ITEM_BED) || (itemId == ITEM_NONE && tex == &campfireFrames[0])) {
                                    inventoryOpen = false;
                                    placementMode = true;
                                    placementFollowingCursor = false;
                                    placementItemID = itemId;
                                    placementFromInventorySlot = clickedSlot;
                                    clickHandled = true;
                                }

                                if (tex == ChickenMeat::getTexture())
                                {

                                    float foodRestore = 20.f;

                                    hunger -= foodRestore;

                                    while (hunger < 0.f && hungerMultiplier < MAX_MULTIPLIER)
                                    {
                                        hunger += maxHunger;
                                        hungerMultiplier++;
                                    }

                                    hunger = (std::max)(0.f, hunger);
                                    std::cout << "Hunger Multiplier = " << hungerMultiplier << "\n";

                                    inventory.consumeItem(clickedSlot);
                                    std::cout << "Eat Chicken\n";
                                }
                                else if (tex == PigMeat::getTexture())
                                {
                                    float foodRestore = 30.f;

                                    hunger -= foodRestore;

                                    while (hunger < 0.f && hungerMultiplier < MAX_MULTIPLIER)
                                    {
                                        hunger += maxHunger;
                                        hungerMultiplier++;
                                    }

                                    hunger = (std::max)(0.f, hunger);

                                    std::cout << "Hunger Multiplier = " << hungerMultiplier << "\n";

                                    inventory.consumeItem(clickedSlot);
                                    std::cout << "Eat Pig\n";
                                }
                                else if (tex == SheepMeat::getTexture())
                                {
                                    float foodRestore = 30.f;

                                    hunger -= foodRestore;

                                    while (hunger < 0.f && hungerMultiplier < MAX_MULTIPLIER)
                                    {
                                        hunger += maxHunger;
                                        hungerMultiplier++;
                                    }

                                    hunger = (std::max)(0.f, hunger);

                                    std::cout << "Hunger Multiplier = " << hungerMultiplier << "\n";

                                    inventory.consumeItem(clickedSlot);
                                    std::cout << "Eat Sheep\n";
                                }
                                else if (tex == ZombieFlesh::getTexture())
                                {

                                    float foodRestore = 30.f;

                                    hunger -= foodRestore;

                                    while (hunger < 0.f && hungerMultiplier < MAX_MULTIPLIER)
                                    {
                                        hunger += maxHunger;
                                        hungerMultiplier++;
                                    }

                                    hunger = (std::max)(0.f, hunger);

                                    hp = (std::max)(0.f, hp - 10.f);

                                    std::cout << "Hunger Multiplier = " << hungerMultiplier << "\n";

                                    inventory.consumeItem(clickedSlot);
                                    std::cout << "Eat Zombie\n";
                                }

                                // =====================================================
                                // COOKED FOOD
                                // =====================================================

                                else if (tex == &cookedChickenTex)
                                {

                                    float foodRestore = 40.f;

                                    // Directly increase multiplier
                                    if (hungerMultiplier < MAX_MULTIPLIER)
                                    {
                                        hungerMultiplier++;
                                    }

                                    // Restore hunger
                                    hunger -= foodRestore;

                                    // Overflow carry
                                    while (hunger < 0.f)
                                    {
                                        hunger += maxHunger;
                                    }

                                    hunger = (std::max)(0.f, hunger);
                                    std::cout << "Hunger Multiplier = " << hungerMultiplier << "\n";

                                    inventory.consumeItem(clickedSlot);

                                    std::cout << "Eat Cooked Chicken\n";
                                }
                                else if (tex == &cookedPigTex)
                                {

                                    float foodRestore = 60.f;

                                    // Directly increase multiplier
                                    if (hungerMultiplier < MAX_MULTIPLIER)
                                    {
                                        hungerMultiplier++;
                                    }

                                    // Restore hunger
                                    hunger -= foodRestore;

                                    // Overflow carry
                                    while (hunger < 0.f)
                                    {
                                        hunger += maxHunger;
                                    }

                                    hunger = (std::max)(0.f, hunger);

                                    std::cout << "Hunger Multiplier = " << hungerMultiplier << "\n";

                                    inventory.consumeItem(clickedSlot);

                                    std::cout << "Eat Cooked Pig\n";
                                }
                                else if (tex == &cookedSheepTex)
                                {
                                    float foodRestore = 60.f;

                                    // Directly increase multiplier
                                    if (hungerMultiplier < MAX_MULTIPLIER)
                                    {
                                        hungerMultiplier++;
                                    }

                                    // Restore hunger
                                    hunger -= foodRestore;

                                    // Overflow carry
                                    while (hunger < 0.f)
                                    {
                                        hunger += maxHunger;
                                    }

                                    hunger = (std::max)(0.f, hunger);

                                    std::cout << "Hunger Multiplier = " << hungerMultiplier << "\n";

                                    inventory.consumeItem(clickedSlot);

                                    std::cout << "Eat Cooked Sheep\n";
                                }
                            }
                        }
                        clickHandled = true;
                    }
                }
                }

            }
        }

    // Extra brace removed to extend while(window.isOpen()) loop all the way to the end of gameLoop

    if (selectionDone && !animLoadedForSelection)
    {
        if (!dbStartupEnabled) {
            // Fast-start fallback: skip heavy first-frame asset/world hydration.
            // This prevents transition stalls and lets gameplay render immediately.
            if (option == 1 && texMaleOnline.getSize().x > 0 && texMaleOnline.getSize().y > 0) {
                sprite.setTexture(texMaleOnline);
                sf::Vector2u ts = texMaleOnline.getSize();
                sprite.setOrigin({ ts.x / 2.f, ts.y / 2.f });
                sprite.setScale({ 500.f / ts.x, 500.f / ts.y });
            }
            else if (texFemaleOnline.getSize().x > 0 && texFemaleOnline.getSize().y > 0) {
                sprite.setTexture(texFemaleOnline);
                sf::Vector2u ts = texFemaleOnline.getSize();
                sprite.setOrigin({ ts.x / 2.f, ts.y / 2.f });
                sprite.setScale({ 500.f / ts.x, 500.f / ts.y });
            }

            hunger = stats.hunger;
            thirst = stats.thirst;
            currency = stats.currency;
            level = stats.level;
            exp = stats.exp;
            timeOffset = stats.elapsedTime;
            elapsedGameTime = timeOffset;
            hungerMultiplier = stats.hungerMultiplier;
            thirstMultiplier = stats.thirstMultiplier;
            hungerMultiplierText.setString("X" + std::to_string(hungerMultiplier));
            thirstMultiplierText.setString("X" + std::to_string(thirstMultiplier));

            animLoadedForSelection = true;
        }

        if (dbStartupEnabled) {

        if (option == 1)
        {
            animWalk.load("Sprites/Male/Walk", FRAMES_PER_DIR);
            animWalkWithSword.load("Sprites/Male/WalkWithSword", FRAMES_PER_DIR);
            animWalkWithShield.load("Sprites/Male/WalkWithShield", FRAMES_PER_DIR);
            animWalkWithShieldAndSword.load("Sprites/Male/WalkWithShieldAndSword", FRAMES_PER_DIR);
            animAttackWithSword.load("Sprites/Male/AttackWithSword", FRAMES_PER_DIR);
            animAttackWithShieldAndSword.load("Sprites/Male/AttackWithSwordAndShield", FRAMES_PER_DIR);
        }
        else
        {
            animWalk.load("Sprites/Female/Walk", FRAMES_PER_DIR);
            animWalkWithSword.load("Sprites/Female/WalkWithSword", FRAMES_PER_DIR);
            animWalkWithShield.load("Sprites/Female/WalkWithShield", FRAMES_PER_DIR);
            animWalkWithShieldAndSword.load("Sprites/Female/WalkWithShieldAndSword", FRAMES_PER_DIR);
            animAttackWithSword.load("Sprites/Female/AttackWithSword", FRAMES_PER_DIR);
            animAttackWithShieldAndSword.load("Sprites/Female/AttackWithSwordAndShield", FRAMES_PER_DIR);
        }

        sprite.setTexture(animWalk.south.getCurrentTexture());
        sf::Vector2u ts_init = sprite.getTexture().getSize();
        if (ts_init.x > 0 && ts_init.y > 0) {
            sprite.setOrigin({ ts_init.x / 2.f, ts_init.y / 2.f });
            sprite.setScale({ 500.f / ts_init.x, 500.f / ts_init.y });
        }


        // ---- Async DB load: fetch inventory/structures/campfires in background ----
        // Bundle all 5 DB calls into one future so the main thread stays responsive.
        struct LoadedGameData {
            std::vector<std::pair<int, int>>    inventory;
            std::vector<StoredStructure>       structures;
            std::vector<StoredStructure>       otherStructures;
            std::vector<StoredCampfire>        campfires;
            std::vector<StoredCampfire>        otherCampfires;
        };
        struct SharedLoadState {
            LoadedGameData data;
            std::atomic<bool> done{ false };
            std::atomic<bool> failed{ false };
            std::mutex dataMutex;
        };
        auto loadState = std::make_shared<SharedLoadState>();
        std::string loadUser = username;
        if (dbStartupEnabled) {
            std::thread([&authManager, loadState, loadUser]() {
                LoadedGameData d;
                try {
                    d.inventory = authManager.loadInventory(loadUser.c_str());
                    d.structures = authManager.loadStructures(loadUser.c_str());
                    d.otherStructures = authManager.loadOtherStructures(loadUser.c_str());
                    d.campfires = authManager.loadCampfires(loadUser.c_str());
                    d.otherCampfires = authManager.loadOtherCampfires(loadUser.c_str());
                }
                catch (...) {
                    loadState->failed = true;
                }
                {
                    std::lock_guard<std::mutex> lk(loadState->dataMutex);
                    loadState->data = std::move(d);
                }
                loadState->done = true;
                }).detach();
        }
        else {
            {
                std::lock_guard<std::mutex> lk(loadState->dataMutex);
                loadState->data.inventory.assign(20, { 0, 0 });
            }
            loadState->done = true;
        }

        bool loadTimedOut = false;
        // Spin the window event loop + draw a "Loading..." overlay while waiting
        {
            sf::Text loadTxt(font, "Loading world...", 60);
            loadTxt.setFillColor(sf::Color::White);
            loadTxt.setOutlineColor(sf::Color::Black);
            loadTxt.setOutlineThickness(3.f);
            sf::Clock loadDotClock;
            sf::Clock loadTimeoutClock;
            int loadDots = 0;
            const float LOAD_TIMEOUT = 30.0f; // 30 second timeout
            while (!loadState->done.load()) {
                if (loadTimeoutClock.getElapsedTime().asSeconds() > LOAD_TIMEOUT) {
                    loadTimedOut = true;
                    std::cerr << "[ERROR] Loading world timed out after " << LOAD_TIMEOUT << " seconds\n";
                    break;
                }
                while (const std::optional ev = window.pollEvent()) {
                    if (ev->is<sf::Event::Closed>()) { window.close(); goto skipGameLoad; }
                }
                if (loadDotClock.getElapsedTime().asSeconds() > 0.4f) {
                    loadDots = (loadDots + 1) % 4;
                    loadDotClock.restart();
                }
                loadTxt.setString(std::string("Loading world") + std::string(loadDots, '.'));
                sf::FloatRect lb = loadTxt.getLocalBounds();
                loadTxt.setPosition({ width / 2.f - lb.size.x / 2.f, height / 2.f - 40.f });
                window.clear(sf::Color(30, 30, 30));
                window.draw(loadTxt);
                window.display();
            }
        }
        {
            LoadedGameData loaded;
            if (!loadTimedOut && loadState->done.load()) {
                std::lock_guard<std::mutex> lk(loadState->dataMutex);
                loaded = loadState->data;
            }
            else {
                std::cerr << "[WARN] Proceeding with fallback local data after load timeout.\n";
                loaded = LoadedGameData{};
                loaded.inventory.assign(20, { 0, 0 });
            }
            auto& loadedItems = loaded.inventory;
            if (loadedItems.size() < 20) {
                loadedItems.resize(20, { 0, 0 });
            }
            bool hasWrenchInSlot0 = (!loadedItems.empty() && loadedItems[0].first == ITEM_WRENCH);
            if (!hasWrenchInSlot0) {
                for (int i = 19; i > 0; i--) {
                    loadedItems[i] = loadedItems[i - 1];
                }
                loadedItems[0] = { ITEM_WRENCH, 1 };
            }

            hunger = stats.hunger;
            thirst = stats.thirst;
            currency = stats.currency;
            level = stats.level;
            exp = stats.exp;
            timeOffset = stats.elapsedTime;
            elapsedGameTime = timeOffset;
            hungerMultiplier = stats.hungerMultiplier;
            thirstMultiplier = stats.thirstMultiplier;
            hungerMultiplierText.setString("X" + std::to_string(hungerMultiplier));
            thirstMultiplierText.setString("X" + std::to_string(thirstMultiplier));

            // Apply Structures (skip any that don't align with the sheared grid)
            for (auto& s : loaded.structures) {
                // Check grid alignment: position must be exactly at a cell origin
                sf::Vector2f snapped = snapToShearedGrid({s.x, s.y});
                if (std::abs(s.x - snapped.x) > 1.f || std::abs(s.y - snapped.y) > 1.f) {
                    continue; // doesn't fit the grid — discard
                }
                const sf::Texture* tex = nullptr;
                if (s.id == ITEM_FURNACE) tex = &furnaceTex;
                else if (s.id == ITEM_WOOD_TILE) tex = &woodTileTex;
                else if (s.id == ITEM_WOOD_WALL) tex = &woodWallTex;
                else if (s.id == ITEM_WOOD_DOOR) tex = &woodDoorTex;
                else if (s.id == ITEM_WOOD_ROOF) tex = &woodRoofTex;
                else if (s.id == ITEM_WOOD_STAIRS) tex = &woodStairsTex;
                else if (s.id == ITEM_BED) tex = &bedTex;
                if (tex) {
                    structures.emplace_back(s.id, *tex, s.rotation, s.isOpen, s.shearY, username);
                    auto& newS = structures.back();
                    newS.sprite->setPosition({ s.x, s.y });
                    newS.health = s.health;
                    sf::Vector2u ts = newS.sprite->getTexture().getSize();
                    newS.sprite->setOrigin({ ts.x / 2.f, (float)ts.y });
                    applyStructureScale(*newS.sprite, s.id, s.rotation, s.isOpen, newS.shearY);
                }
            }

            // Apply Other Players' Structures (skip any that don't align with the sheared grid)
            for (auto& s : loaded.otherStructures) {
                // Check grid alignment: position must be exactly at a cell origin
                sf::Vector2f snapped = snapToShearedGrid({s.x, s.y});
                if (std::abs(s.x - snapped.x) > 1.f || std::abs(s.y - snapped.y) > 1.f) {
                    continue; // doesn't fit the grid — discard
                }
                const sf::Texture* tex = nullptr;
                if (s.id == ITEM_FURNACE) tex = &furnaceTex;
                else if (s.id == ITEM_WOOD_TILE) tex = &woodTileTex;
                else if (s.id == ITEM_WOOD_WALL) tex = &woodWallTex;
                else if (s.id == ITEM_WOOD_DOOR) tex = &woodDoorTex;
                else if (s.id == ITEM_WOOD_ROOF) tex = &woodRoofTex;
                else if (s.id == ITEM_WOOD_STAIRS) tex = &woodStairsTex;
                else if (s.id == ITEM_BED) tex = &bedTex;
                if (tex) {
                    structures.emplace_back(s.id, *tex, s.rotation, s.isOpen, s.shearY, "other");
                    auto& newS = structures.back();
                    newS.sprite->setPosition({ s.x, s.y });
                    newS.health = s.health;
                    sf::Vector2u ts = newS.sprite->getTexture().getSize();
                    newS.sprite->setOrigin({ ts.x / 2.f, (float)ts.y });
                    applyStructureScale(*newS.sprite, s.id, s.rotation, s.isOpen, newS.shearY);
                }
            }

            // Apply Campfires
            for (auto& c : loaded.campfires) {
                PlacedCampfire pc;
                pc.position = { c.x, c.y };
                pc.timeRemaining = c.timeRemaining;
                pc.isCrafted = c.isCrafted;
                pc.currentFrame = 0;
                pc.frameTimer = 0.f;
                pc.fuel = c.fuel;
                pc.inputItem = c.inputItem;
                pc.inputCount = c.inputCount;
                pc.outputItem = c.outputItem;
                pc.outputCount = c.outputCount;
                pc.owner = username;  // own campfire
                campfires.push_back(pc);
            }

            // Apply Other Players' Campfires
            for (auto& c : loaded.otherCampfires) {
                PlacedCampfire pc;
                pc.position = { c.x, c.y };
                pc.timeRemaining = c.timeRemaining;
                pc.isCrafted = c.isCrafted;
                pc.currentFrame = 0;
                pc.frameTimer = 0.f;
                pc.fuel = c.fuel;
                pc.inputItem = c.inputItem;
                pc.inputCount = c.inputCount;
                pc.outputItem = c.outputItem;
                pc.outputCount = c.outputCount;
                pc.owner = c.owner;  // other player's campfire
                campfires.push_back(pc);
            }

            for (int i = 0; i < 20 && i < static_cast<int>(loadedItems.size()); i++)
            {
                int itemId = loadedItems[i].first;
                int count = loadedItems[i].second;
                if (count > 0)
                {
                    const sf::Texture* tex =
                        (itemId == ITEM_TREE) ? &treeTex :
                        (itemId == ITEM_STONE) ? &stoneTex :
                        (itemId == ITEM_WOOD) ? &woodTex :
                        (itemId == ITEM_WOODEN_SWORD) ? &woodenSwordTex :
                        (itemId == ITEM_SAPLING) ? &saplingTex :
                        (itemId == ITEM_SHIELD) ? &shieldTex :
                        (itemId == ITEM_FURNACE) ? &furnaceTex :
                        (itemId == ITEM_WOOD_TILE) ? &woodTileTex :
                        (itemId == ITEM_WOOD_WALL) ? &woodWallTex :
                        (itemId == ITEM_WOOD_DOOR) ? &woodDoorTex :
                        (itemId == ITEM_WOOD_ROOF) ? &woodRoofTex :
                        (itemId == ITEM_WOOD_STAIRS) ? &woodStairsTex :
                        (itemId == ITEM_BED) ? &bedTex :
                        (itemId == ITEM_WRENCH) ? &wrenchItemTex :
                        (itemId == ITEM_CHICKEN_MEAT) ? ChickenMeat::getTexture() :
                        (itemId == ITEM_PIG_MEAT) ? PigMeat::getTexture() :
                        (itemId == ITEM_SHEEP_MEAT) ? SheepMeat::getTexture() :
                        (itemId == ITEM_COOKED_SHEEP) ? &cookedSheepTex :
                        (itemId == ITEM_WOOL) ? &woolTex :
                        (itemId == ITEM_ZOMBIE_FLESH) ? ZombieFlesh::getTexture() :
                        (itemId == ITEM_COOKED_CHICKEN) ? &cookedChickenTex :
                        (itemId == ITEM_COOKED_PIG) ? &cookedPigTex : nullptr;

                    if (tex) {
                        inventory.setItem(i, itemId, *tex, count);
                    }
                }
            }
        } // end LoadedGameData block

        if (npcFemale.texture.loadFromFile(resolvePath("Sprites/NPCFemale/Walk/South/frame_000.png"))) {
            npcFemale.sprite->setTexture(npcFemale.texture);
            sf::Vector2u ts_npcF = npcFemale.texture.getSize();
            if (ts_npcF.x > 0 && ts_npcF.y > 0) {
                npcFemale.sprite->setOrigin({ ts_npcF.x / 2.f, ts_npcF.y / 2.f });
                npcFemale.sprite->setScale({ 500.f / ts_npcF.x, 500.f / ts_npcF.y });
            }
            npcFemale.sprite->setPosition({ width / 2.f + 150.f, height / 2.f - 50.f });
        }

        if (npcMale.texture.loadFromFile(resolvePath("Sprites/NPCMale/Walk/South/frame_000.png"))) {
            npcMale.sprite->setTexture(npcMale.texture);
            sf::Vector2u ts_npcM = npcMale.texture.getSize();
            if (ts_npcM.x > 0 && ts_npcM.y > 0) {
                npcMale.sprite->setOrigin({ ts_npcM.x / 2.f, ts_npcM.y / 2.f });
                npcMale.sprite->setScale({ 500.f / ts_npcM.x, 500.f / ts_npcM.y });
            }
            npcMale.sprite->setPosition({ MAP_WIDTH / 2.f - 150.f, MAP_HEIGHT / 2.f - 50.f });
        }
        if (npcFemale.sprite->getPosition().x == 0) { // Fallback if load failed or position not set
            npcFemale.sprite->setPosition({ MAP_WIDTH / 2.f + 150.f, MAP_HEIGHT / 2.f - 50.f });
            npcMale.sprite->setPosition({ MAP_WIDTH / 2.f - 150.f, MAP_HEIGHT / 2.f - 50.f });
        }

        animLoadedForSelection = true;
        }
    skipGameLoad:;
    }


    if (selectionDone && musicLoaded && !musicStarted)
    {
        bgMusic.play();
        musicStarted = true;
    }

    if (!selectionDone)
    {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

        bool hover1 = button1.getGlobalBounds().contains(mousePos);
        button1.setFillColor(hover1 ? sf::Color(46, 204, 113) : sf::Color(39, 174, 96));
        button1.setOutlineThickness(hover1 ? 4.f : 0.f);
        button1.setOutlineColor(sf::Color::White);

        bool hover2 = button2.getGlobalBounds().contains(mousePos);
        button2.setFillColor(hover2 ? sf::Color(46, 204, 113) : sf::Color(39, 174, 96));
        button2.setOutlineThickness(hover2 ? 4.f : 0.f);
        button2.setOutlineColor(sf::Color::White);

        window.clear(sf::Color::White);
        window.setView(GetUIView(&window));
        window.draw(selBgSprite);
        window.draw(Option);
        window.draw(button1);
        window.draw(buttonText1);
        window.draw(button2);
        window.draw(buttonText2);

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

        // --- MULTI-FLOOR: Compute elevated tile levels (BFS from stairs) ---
        const float TILE_CONNECT_DIST = 260.f;
        struct TileFloor { sf::Vector2f pos; int floorLevel; };
        std::vector<TileFloor> elevatedTileFloors;
        {
            struct StairInfo { sf::Vector2f pos; };
            std::vector<StairInfo> allStairs;
            std::vector<sf::Vector2f> allTilePos;
            for (auto& s : structures) {
                if (!s.sprite.has_value()) continue;
                if (s.id == ITEM_WOOD_STAIRS) allStairs.push_back({ s.sprite->getPosition() });
                if (s.id == ITEM_WOOD_TILE)   allTilePos.push_back(s.sprite->getPosition());
            }
            auto posKey = [](sf::Vector2f p) -> std::pair<int,int> {
                return { (int)std::round(p.x / 200.f), (int)std::round(p.y / 100.f) };
            };
            std::map<std::pair<int,int>, int> tileLevelMap;
            for (auto& tp : allTilePos) tileLevelMap[posKey(tp)] = 0;

            std::queue<std::pair<sf::Vector2f, int>> bfsQueue;
            for (auto& stair : allStairs) {
                for (auto& tp : allTilePos) {
                    float dx = tp.x - stair.pos.x, dy = tp.y - stair.pos.y;
                    // Stair is placed on a tile: tile is at or below stair (dy >= 0 in screen coords)
                    // or adjacent at same level. Accept any tile within distance (no dy restriction).
                    if (std::sqrt(dx*dx + dy*dy) < TILE_CONNECT_DIST) {
                        auto k = posKey(tp);
                        if (tileLevelMap[k] < 1) {
                            tileLevelMap[k] = 1;
                            bfsQueue.push({ tp, 1 });
                        }
                    }
                }
            }
            while (!bfsQueue.empty()) {
                auto [curPos, curLevel] = bfsQueue.front(); bfsQueue.pop();
                for (auto& tp : allTilePos) {
                    float dx = tp.x - curPos.x, dy = tp.y - curPos.y;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    if (dist < TILE_CONNECT_DIST && dist > 1.f) {
                        auto k = posKey(tp);
                        if (tileLevelMap[k] < curLevel) {
                            tileLevelMap[k] = curLevel;
                            bfsQueue.push({ tp, curLevel });
                        }
                    }
                }
                for (auto& stair : allStairs) {
                    float dx = stair.pos.x - curPos.x, dy = stair.pos.y - curPos.y;
                    if (std::sqrt(dx*dx + dy*dy) < TILE_CONNECT_DIST) {
                        for (auto& tp2 : allTilePos) {
                            float dx2 = tp2.x - stair.pos.x, dy2 = tp2.y - stair.pos.y;
                            if (std::sqrt(dx2*dx2 + dy2*dy2) < TILE_CONNECT_DIST && dy2 <= 0.f) {
                                auto k2 = posKey(tp2);
                                int nextLevel = curLevel + 1;
                                if (tileLevelMap[k2] < nextLevel) {
                                    tileLevelMap[k2] = nextLevel;
                                    bfsQueue.push({ tp2, nextLevel });
                                }
                            }
                        }
                    }
                }
            }
            for (auto& [key, level] : tileLevelMap) {
                if (level > 0) {
                    for (auto& tp : allTilePos) {
                        if (posKey(tp) == key) {
                            elevatedTileFloors.push_back({ tp, level });
                        }
                    }
                }
            }
        }

        // --- COLLISION CHECK LAMBDA ---
        auto isBlocked = [&](sf::Vector2f targetPos) {
            sf::Vector2f targetFeet = targetPos + sf::Vector2f(0.f, 250.f);

            // Check if on stairs OR on any elevated floor tile (any floor level)
            bool targetElevated = false;
            for (auto& s : structures) {
                if (s.id == ITEM_WOOD_STAIRS && s.sprite.has_value()) {
                    sf::FloatRect sb = s.sprite->getGlobalBounds();
                    sb.position.x -= 30.f; sb.size.x += 60.f;
                    sb.position.y -= 30.f; sb.size.y += 60.f;
                    if (sb.contains(targetFeet)) { targetElevated = true; break; }
                }
            }
            if (!targetElevated) {
                for (auto& etf : elevatedTileFloors) {
                    float dx = targetFeet.x - etf.pos.x;
                    float dy = targetFeet.y - etf.pos.y;
                    if (std::sqrt(dx*dx + dy*dy) < TILE_CONNECT_DIST) {
                        targetElevated = true;
                        break;
                    }
                }
            }

            // 1. Structures (Walls, Furnace, Bed, Door)
            for (auto& s : structures) {
                if (!s.sprite.has_value()) continue;
                if (s.id == ITEM_WOOD_WALL || (s.id == ITEM_WOOD_DOOR && !s.isOpen)) {
                    if (targetElevated) continue; // Ignore wall/door collision when on upper floor or stairs
                    sf::Vector2f sPos = s.sprite->getPosition();
                    sf::Vector2f A = sPos - sf::Vector2f(250.8f, 250.8f * s.shearY);
                    sf::Vector2f B = sPos + sf::Vector2f(250.8f, 250.8f * s.shearY);
                    if (distanceToSegment(targetPos, A, B) < 40.f) return true;
                }
                else if (s.id == ITEM_FURNACE || s.id == ITEM_BED) {
                    if (targetElevated) continue; // Skip furniture on lower floor
                    sf::FloatRect b = s.sprite->getGlobalBounds();
                    b.position.x += b.size.x * 0.2f; b.size.x *= 0.6f;
                    b.position.y += b.size.y * 0.7f; b.size.y *= 0.3f;
                    if (b.contains(targetPos)) return true;
                }
            }
            // 2. Resources (Trees, Stones)
            for (auto& it : itemManager.getItems()) {
                if (it.id == ITEM_TREE || it.id == ITEM_STONE) {
                    sf::FloatRect b = it.sprite.getGlobalBounds();
                    // Trees/Stones also collision at bottom
                    b.position.x += b.size.x * 0.3f; b.size.x *= 0.4f;
                    b.position.y += b.size.y * 0.7f; b.size.y *= 0.3f;
                    if (b.contains(targetPos)) return true;
                }
            }
            return false;
            };

        for (auto& m : monsters)
        {
            if (m->health > 0) {
                float dx = m->sprite.getPosition().x - sprite.getPosition().x;
                float dy = m->sprite.getPosition().y - sprite.getPosition().y;
                if (dx * dx + dy * dy < 3000000.f) {
                    m->update(dt, sprite.getPosition(), isBlocked);
                }
            }

            // Only attack if zombie is alive
            if (m->health > 0) {
                sf::FloatRect z = m->sprite.getGlobalBounds();
                sf::FloatRect p = sprite.getGlobalBounds();

                sf::Vector2f zCenter(z.position.x + z.size.x * 0.5f, z.position.y + z.size.y * 0.5f);
                sf::Vector2f pCenter(p.position.x + p.size.x * 0.5f, p.position.y + p.size.y * 0.5f);
                float dx = zCenter.x - pCenter.x;
                float dy = zCenter.y - pCenter.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                bool hit = dist < 60.f; // Increased contact threshold for better reliability
                if (!introActive && hit)
                {
                    if (m->attackTimer >= m->attackCooldown)
                    {
                        if (!tutorialActive) {
                            if (shieldEquipped && eqShieldSlot != -1) {
                                int shieldDur = inventory.getDurability(eqShieldSlot);
                                shieldDur -= (int)m->attackDamage;
                                if (shieldDur <= 0) {
                                    shieldDur = 0;
                                    inventory.consumeItem(eqShieldSlot);
                                    shieldEquipped = false;
                                }
                                else {
                                    inventory.setDurability(eqShieldSlot, shieldDur);
                                }
                            }
                            else {
                                hp -= m->attackDamage;
                            }
                        }
                        m->attackTimer = 0.f;
                        isPlayerHurt = true;
                        playerHurtTimer = 0.25f;

                        sf::Vector2f pushDir = sprite.getPosition() - m->sprite.getPosition();
                        float len = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                        if (len != 0.f) pushDir /= len;
                        sprite.move(pushDir * 30.f); // Slight knockback
                    }
                }
            }

            sf::FloatRect bounds = m->sprite.getGlobalBounds();
            sf::Vector2f pos = m->sprite.getPosition();

            if (bounds.position.x < 0.f)
                pos.x -= bounds.position.x;

            if (bounds.position.y < 0.f)
                pos.y -= bounds.position.y;

            if (bounds.position.x + bounds.size.x > MAP_WIDTH)
                pos.x -= (bounds.position.x + bounds.size.x - MAP_WIDTH);

            if (bounds.position.y + bounds.size.y > MAP_HEIGHT)
                pos.y -= (bounds.position.y + bounds.size.y - MAP_HEIGHT);

            m->sprite.setPosition(pos);
        }

        AnimSet* currentAnimSet = &animWalk;
        if (swordEquipped && shieldEquipped) currentAnimSet = &animWalkWithShieldAndSword;
        else if (swordEquipped) currentAnimSet = &animWalkWithSword;
        else if (shieldEquipped) currentAnimSet = &animWalkWithShield;

        if (playerAttackCooldown > 0.f) {
            if (swordEquipped && shieldEquipped) currentAnimSet = &animAttackWithShieldAndSword;
            else if (swordEquipped) currentAnimSet = &animAttackWithSword;
        }

        animWalk.setPlaying(false);
        animWalkWithSword.setPlaying(false);
        animWalkWithShield.setPlaying(false);
        animWalkWithShieldAndSword.setPlaying(false);
        animAttackWithSword.setPlaying(false);
        animAttackWithShieldAndSword.setPlaying(false);

        float currentSpeed = MOVE_SPEED;

        if (sf::Keyboard::isKeyPressed(globalSettings.keySprint))
        {
            currentSpeed *= 2.0f; // Sprinting increases speed by 100%
            if (tutorialActive && tutorialStep == 2) tutSprinted = true;
        }

        bool moveUp = !isSleeping && sf::Keyboard::isKeyPressed(globalSettings.keyUp);
        bool moveDown = !isSleeping && sf::Keyboard::isKeyPressed(globalSettings.keyDown);
        bool moveLeft = !isSleeping && sf::Keyboard::isKeyPressed(globalSettings.keyLeft);
        bool moveRight = !isSleeping && sf::Keyboard::isKeyPressed(globalSettings.keyRight);

        if (moveUp || moveDown || moveLeft || moveRight) {
            sf::Vector2f curPos = sprite.getPosition();
            float dx = curPos.x - tutLastPos.x;
            float dy = curPos.y - tutLastPos.y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (tutorialActive && tutorialStep == 1) tutMoveDist += d;
            tutLastPos = curPos;
            if (moveUp)
            {
                sf::Vector2f nextPos = sprite.getPosition() + sf::Vector2f(0.f, -currentSpeed * dt);
                if (!isBlocked(nextPos)) sprite.move({ 0.f, -currentSpeed * dt });
                currentDir = Dir::North;
                currentAnimSet->setDirPlaying(1, true);
            }
            else if (moveDown)
            {
                sf::Vector2f nextPos = sprite.getPosition() + sf::Vector2f(0.f, currentSpeed * dt);
                if (!isBlocked(nextPos)) sprite.move({ 0.f, currentSpeed * dt });
                currentDir = Dir::South;
                currentAnimSet->setDirPlaying(0, true);
            }

            if (moveLeft)
            {
                sf::Vector2f nextPos = sprite.getPosition() + sf::Vector2f(-currentSpeed * dt, 0.f);
                if (!isBlocked(nextPos)) sprite.move({ -currentSpeed * dt, 0.f });
                currentDir = Dir::West;
                currentAnimSet->setDirPlaying(3, true);
            }
            else if (moveRight)
            {
                sf::Vector2f nextPos = sprite.getPosition() + sf::Vector2f(currentSpeed * dt, 0.f);
                if (!isBlocked(nextPos)) sprite.move({ currentSpeed * dt, 0.f });
                currentDir = Dir::East;
                currentAnimSet->setDirPlaying(2, true);
            }
        }
        else
        {
            currentAnimSet->stop();
        }

        if (playerAttackCooldown > 0.f) {
            if (currentDir == Dir::South) currentAnimSet->setDirPlaying(0, true);
            else if (currentDir == Dir::North) currentAnimSet->setDirPlaying(1, true);
            else if (currentDir == Dir::East) currentAnimSet->setDirPlaying(2, true);
            else if (currentDir == Dir::West) currentAnimSet->setDirPlaying(3, true);
        }
        currentAnimSet->update(0.016f);

        if (currentDir == Dir::South) sprite.setTexture(currentAnimSet->getTexture(0), true);
        if (currentDir == Dir::North) sprite.setTexture(currentAnimSet->getTexture(1), true);
        if (currentDir == Dir::East)  sprite.setTexture(currentAnimSet->getTexture(2), true);
        if (currentDir == Dir::West)  sprite.setTexture(currentAnimSet->getTexture(3), true);

        sf::Vector2u ts_player = sprite.getTexture().getSize();
        if (ts_player.x > 0 && ts_player.y > 0) {
            sprite.setOrigin({ ts_player.x / 2.f, ts_player.y / 2.f });
        }

        if (playerAttackCooldown > 0.f) {
            playerAttackCooldown -= dt;
        }

        if (!isSleeping && (sf::Keyboard::isKeyPressed(globalSettings.keyAttack) || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Space)) && playerAttackCooldown <= 0.f)
        {
            playerAttackCooldown = 0.3f; // Attack cooldown

            float damage = swordEquipped ? 50.f : 25.f;
            bool hitSomething = false;

            float attackRadius = swordEquipped ? 150.f : 30.f;
            sf::FloatRect attackBox = sprite.getGlobalBounds();
            attackBox.position.x -= attackRadius;
            attackBox.position.y -= attackRadius;
            attackBox.size.x += attackRadius * 2.f;
            attackBox.size.y += attackRadius * 2.f;

            for (auto& a : animals)
            {
                sf::FloatRect b = a->sprite.getGlobalBounds();
                bool overlap =
                    attackBox.position.x < b.position.x + b.size.x &&
                    attackBox.position.x + attackBox.size.x > b.position.x &&
                    attackBox.position.y < b.position.y + b.size.y &&
                    attackBox.position.y + attackBox.size.y > b.position.y;

                if (overlap && a->health > 0)
                {
                    if (tutorialActive && tutorialStep == 5) tutAttacked = true;
                    a->health -= damage;
                    a->isHurt = true;
                    a->hurtTimer = 0.15f;
                    hitSomething = true;
                    exp += 10; // Gain EXP for attacking animals

                    // 🔥 KNOCKBACK GOES HERE
                    sf::Vector2f dir = a->sprite.getPosition() - sprite.getPosition();
                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len != 0.f) dir /= len;
                    a->sprite.move(dir * 40.f);
                }
            }

            for (auto& tree : itemManager.getItems())
            {
                if (tree.id != ITEM_TREE) continue;

                sf::FloatRect b = tree.sprite.getGlobalBounds();
                bool overlap =
                    attackBox.position.x < b.position.x + b.size.x &&
                    attackBox.position.x + attackBox.size.x > b.position.x &&
                    attackBox.position.y < b.position.y + b.size.y &&
                    attackBox.position.y + attackBox.size.y > b.position.y;

                if (overlap && tree.health > 0)
                {
                    if (tutorialActive && tutorialStep == 4) tutWoodGathered++;
                    tree.health -= damage;
                    tree.isHurt = true;
                    tree.hurtTimer = 0.15f;
                    hitSomething = true;
                    exp += 5; // Gain EXP for chopping wood
                }
            }

            for (auto& a : monsters)
            {
                sf::FloatRect b = a->sprite.getGlobalBounds();
                bool overlap =
                    attackBox.position.x < b.position.x + b.size.x &&
                    attackBox.position.x + attackBox.size.x > b.position.x &&
                    attackBox.position.y < b.position.y + b.size.y &&
                    attackBox.position.y + attackBox.size.y > b.position.y;

                if (overlap && a->health > 0)
                {
                    if (tutorialActive && tutorialStep == 5) tutAttacked = true;
                    a->health -= damage;
                    a->isHurt = true;
                    a->hurtTimer = 0.15f;
                    hitSomething = true;
                    exp += 20; // Gain EXP for attacking monsters

                    // 🔥 KNOCKBACK GOES HERE
                    sf::Vector2f dir = a->sprite.getPosition() - sprite.getPosition();
                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len != 0.f) dir /= len;
                    a->sprite.move(dir * 40.f);
                }
            }

            for (auto& s : structures)
            {
                if (s.id == ITEM_WOOD_TILE) continue; // Floors are indestructible? Or keep them destroyable?

                sf::FloatRect b = s.sprite->getGlobalBounds();
                bool overlap =
                    attackBox.position.x < b.position.x + b.size.x &&
                    attackBox.position.x + attackBox.size.x > b.position.x &&
                    attackBox.position.y < b.position.y + b.size.y &&
                    attackBox.position.y + attackBox.size.y > b.position.y;

                if (overlap && s.health > 0)
                {
                    s.health -= damage;
                    s.isHurt = true;
                    s.hurtTimer = 0.15f;
                    hitSomething = true;
                }
            }

            if (hitSomething && swordEquipped && eqSwordSlot != -1) {
                int dur = inventory.getDurability(eqSwordSlot);
                dur -= 1;
                inventory.setDurability(eqSwordSlot, dur);
                if (dur <= 0) {
                    inventory.consumeItem(eqSwordSlot);
                    inventory.setEquipped(eqSwordSlot, false);
                    swordEquipped = false;
                }
            }
        }

        for (auto& pc : campfires) {
            if (!pc.isCrafted) {
                pc.timeRemaining -= dt;
                if (pc.timeRemaining <= 0.f) {
                    pc.timeRemaining = 0.f;
                    pc.isCrafted = true;
                }
            }
            else {
                pc.frameTimer += dt;
                if (pc.frameTimer >= 0.1f) {
                    pc.frameTimer = 0.f;
                    pc.currentFrame = (pc.currentFrame + 1) % 8;
                }

                // Cooking logic over time
                if (pc.inputCount > 0 && pc.fuel > 0) {
                    if (pc.timeRemaining <= 0.f) {
                        pc.timeRemaining = 2.0f; // Start cooking this item (takes 2 seconds)
                    }
                    pc.timeRemaining -= dt;
                    if (pc.timeRemaining <= 0.f) {
                        pc.fuel--;
                        pc.inputCount--;
                        if (pc.inputItem == ITEM_CHICKEN_MEAT) pc.outputItem = ITEM_COOKED_CHICKEN;
                        else if (pc.inputItem == ITEM_PIG_MEAT) pc.outputItem = ITEM_COOKED_PIG;
                        else if (pc.inputItem == ITEM_SHEEP_MEAT) pc.outputItem = ITEM_COOKED_SHEEP;
                        pc.outputCount++;
                        if (pc.inputCount <= 0) {
                            pc.inputItem = 0;
                        }
                        syncSaveLocalStructuresAndCampfires();
                    }
                } else {
                    pc.timeRemaining = 0.f;
                }
            }
        }

        for (auto& s : plantedSaplings)
        {
            if (!s.grown)
            {
                s.growTimer -= dt;

                if (s.growTimer <= 0.f)
                {
                    s.growTimer = 0.f;
                    s.grown = true;

                    itemManager.addItem(
                        ITEM_TREE,
                        treeTex,
                        s.position,
                        1
                    );
                }
            }
        }

        plantedSaplings.erase(
            std::remove_if(
                plantedSaplings.begin(),
                plantedSaplings.end(),
                [](const PlantedSapling& s)
                {
                    return s.grown;
                }),
            plantedSaplings.end()
        );

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

        // -------------------------------------------------------------------------
        // PARALLELOGRAM FLOOR GRID — matches the wood tile visual footprint exactly.
        //
        // Wood tile: texture 1254x1254, scale {0.4, 0.2}, origin at bottom-center.
        //   halfW = 1254 * 0.4 / 2 = 250.8
        //   tileH = 1254 * 0.2     = 250.8
        // The SFML shear transform is:  y' = y + shearY * (x - px)
        // Snap step: 200 x 100 (grid density — cell anchors at gx*200, gy*100).
        // -------------------------------------------------------------------------
        {
            const float STEP_X = 200.f;
            const float STEP_Y = 100.f;

            // Hover cell — which anchor is closest to the mouse
            sf::Vector2i pixelMouse = sf::Mouse::getPosition(window);
            sf::Vector2f worldM     = window.mapPixelToCoords(pixelMouse);
            int hoverGx = (int)std::round(worldM.x / STEP_X);
            int hoverGy = (int)std::round((worldM.y - worldM.x * GLOBAL_SHEAR) / STEP_Y);

            // Viewport bounds for frustum culling
            sf::View  currentView = window.getView();
            sf::Vector2f vCenter  = currentView.getCenter();
            sf::Vector2f vSize    = currentView.getSize();
            float margin = 200.f; // generous margin

            float left   = vCenter.x - vSize.x * 0.5f - margin;
            float right  = vCenter.x + vSize.x * 0.5f + margin;
            float top    = vCenter.y - vSize.y * 0.5f - margin;
            float bottom = vCenter.y + vSize.y * 0.5f + margin;

            int minGx = (int)std::floor(left   / STEP_X);
            int maxGx = (int)std::ceil (right  / STEP_X);

            // Clamp to world
            int mapMaxGx = (int)std::round((float)MAP_WIDTH  / STEP_X);
            int mapMaxGy = (int)std::round((float)MAP_HEIGHT / STEP_Y);
            if (minGx < 0) minGx = 0;
            if (maxGx > mapMaxGx) maxGx = mapMaxGx;

            sf::VertexArray lines(sf::PrimitiveType::Lines);

            for (int gx = minGx; gx <= maxGx; ++gx) {
                // Compute gy range for this column to cover frustum Y
                float colYOffset = gx * STEP_X * GLOBAL_SHEAR;
                int minGy = (int)std::floor((top - colYOffset) / STEP_Y);
                int maxGy = (int)std::ceil ((bottom - colYOffset) / STEP_Y);

                if (minGy < 0) minGy = 0;
                if (maxGy > mapMaxGy) maxGy = mapMaxGy;

                for (int gy = minGy; gy <= maxGy; ++gy) {
                    // Anchor = bottom-center of the cell
                    float px = gx * STEP_X;
                    float py = colYOffset + gy * STEP_Y;

                    // Parallelogram corners (200x100 cell, sheared by -0.5f)
                    sf::Vector2f BL(px - 100.f, py + 50.f);
                    sf::Vector2f BR(px + 100.f, py - 50.f);
                    sf::Vector2f TR(px + 100.f, py - 150.f);
                    sf::Vector2f TL(px - 100.f, py - 50.f);

                    sf::Color col;
                    if (gx == hoverGx && gy == hoverGy) {
                        col = sf::Color(0, 255, 80, 200); // bright green hover
                    } else {
                        col = sf::Color(255, 255, 255, 70); // soft white grid
                    }

                    // Draw 4 edges of the parallelogram
                    lines.append(sf::Vertex{TL, col}); lines.append(sf::Vertex{TR, col}); // top
                    lines.append(sf::Vertex{TR, col}); lines.append(sf::Vertex{BR, col}); // right
                    lines.append(sf::Vertex{BR, col}); lines.append(sf::Vertex{BL, col}); // bottom
                    lines.append(sf::Vertex{BL, col}); lines.append(sf::Vertex{TL, col}); // left
                }
            }
            window.draw(lines);
        }
        for (auto& p : ponds) {
            window.draw(p);
        }
        itemManager.draw(window);

        for (auto& pc : campfires) {
            sf::Sprite cs(campfireFrames[pc.currentFrame]);
            sf::Vector2u cfs = cs.getTexture().getSize();
            cs.setOrigin({ cfs.x / 2.f, (float)cfs.y });
            cs.setScale({ 0.9f, 0.9f });
            cs.setPosition(pc.position);
            if (!pc.isCrafted) {
                cs.setColor(sf::Color(255, 255, 255, 128));
            }
            window.draw(cs);

            if (!pc.isCrafted) {
                sf::Text t(font, "Crafting: " + std::to_string((int)pc.timeRemaining) + "s", 16);
                t.setFillColor(sf::Color::White);
                t.setPosition({ pc.position.x - 40.f, pc.position.y - 40.f });
                window.draw(t);
            }
            else {
                // Draw floating wrench icon if player has a wrench in inventory (no distance check)
                float dx = pc.position.x - sprite.getPosition().x;
                float dy = pc.position.y - (sprite.getPosition().y + 250.f);
                float dist = std::sqrt(dx*dx + dy*dy);
                int selSlot = inventory.getSelectedSlot();
                bool hasWrench = false;
                if (selSlot != -1 && inventory.hasItem(selSlot) && inventory.getItemID(selSlot) == ITEM_WRENCH) {
                    hasWrench = true;
                }
                static int debugCounter = 0;
                if (debugCounter++ % 60 == 0) {
                    std::cout << "[Campfire Wrench Debug] dist: " << dist 
                              << " | selSlot: " << selSlot 
                              << " | hasItem: " << (selSlot != -1 ? inventory.hasItem(selSlot) : false)
                              << " | itemID: " << (selSlot != -1 ? inventory.getItemID(selSlot) : -1)
                              << " | hasWrench: " << hasWrench << std::endl;
                }
                for (int i = 0; i < 20; i++) {
                    if (inventory.hasItem(i) && inventory.getItemID(i) == ITEM_WRENCH && inventory.isEquipped(i)) {
                        hasWrench = true;
                        break;
                    }
                }
                if (hasWrench) {
                    sf::Sprite ws(wrenchTex);
                    sf::Vector2u wts = wrenchTex.getSize();
                    ws.setOrigin({ wts.x / 2.f, wts.y / 2.f });
                    ws.setScale({ 0.08f, 0.08f });
                    ws.setPosition({ pc.position.x, pc.position.y - 480.f });
                    window.draw(ws);
                }
            }
        }


        if (placementMode) {
            if (placementFollowingCursor) {
                placementPos = worldPos;
                // Snap to grid cell origin (floor, matching drawn grid)
                placementPos = snapToShearedGrid(placementPos);

                if (placementItemID == ITEM_WOOD_WALL) {
                    if (placementRotation == 90.f) {
                        placementPos = { placementPos.x - 100.f, placementPos.y + 50.f };
                    }
                }
                else if (placementItemID == ITEM_WOOD_DOOR) {
                    float bestDist = 150.f;
                    PlacedStructure* nearestWall = nullptr;
                    for (auto& s : structures) {
                        if (s.id == ITEM_WOOD_WALL && s.sprite.has_value()) {
                            sf::Vector2f wallPos = s.sprite->getPosition();
                            float dx = worldPos.x - wallPos.x;
                            float dy = worldPos.y - wallPos.y;
                            float dist = std::sqrt(dx * dx + dy * dy);
                            if (dist < bestDist) {
                                bestDist = dist;
                                nearestWall = &s;
                            }
                        }
                    }
                    if (nearestWall) {
                        placementPos = nearestWall->sprite->getPosition();
                        placementRotation = nearestWall->rotation;
                    } else {
                        if (placementRotation == 90.f) {
                            placementPos = { placementPos.x - 100.f, placementPos.y + 50.f };
                        }
                    }
                }
                else if (placementItemID == ITEM_WOOD_ROOF) {
                    placementPos.y -= 180.f;
                }
                else if (placementItemID == ITEM_WOOD_STAIRS) {
                    placementPos.y -= 100.f;
                }
            }

            sf::Sprite preview(campfireFrames[0]);
            if (placementItemID == ITEM_NONE) preview.setTexture(campfireFrames[0], true);
            else if (placementItemID == ITEM_FURNACE) preview.setTexture(furnaceTex, true);
            else if (placementItemID == ITEM_WOOD_TILE) preview.setTexture(woodTileTex, true);
            else if (placementItemID == ITEM_WOOD_WALL) preview.setTexture(woodWallTex, true);
            else if (placementItemID == ITEM_WOOD_DOOR) preview.setTexture(woodDoorTex, true);
            else if (placementItemID == ITEM_WOOD_ROOF) preview.setTexture(woodRoofTex, true);
            else if (placementItemID == ITEM_WOOD_STAIRS) preview.setTexture(woodStairsTex, true);
            else if (placementItemID == ITEM_BED) preview.setTexture(bedTex, true);

            sf::Vector2u pts = preview.getTexture().getSize();
            preview.setOrigin({ pts.x / 2.f, (float)pts.y });

            float previewShear = 0.f;
            applyStructureScale(preview, placementItemID, placementRotation, false, previewShear);

            preview.setColor(sf::Color(255, 255, 255, 180));
            preview.setPosition(placementPos);

            sf::Transform pTrans;
            pTrans.translate(placementPos);
            pTrans.combine(sf::Transform(1.f, 0.f, 0.f, previewShear, 1.f, 0.f, 0.f, 0.f, 1.f));
            pTrans.translate(-placementPos);
            window.draw(preview, pTrans);
        }

        // --- HOUSE TRANSPARENCY & STAIRS / MULTI-FLOOR LOGIC ---
        bool isUnderRoof = false;
        float stairOffset = 0.f;
        sf::Vector2f pPos = sprite.getPosition();
        sf::Vector2f pFeetGlobal = { pPos.x, pPos.y + 250.f };
        // elevatedTileFloors and TILE_CONNECT_DIST already computed above before isBlocked

        // --- Compute player floor level and stair offset ---
        playerFloorLevel = 0;
        bool playerOnStairs = false;

        // Check if player feet are on any elevated tile (already fully on upper floor)
        for (auto& etf : elevatedTileFloors) {
            float dx = pFeetGlobal.x - etf.pos.x;
            float dy = pFeetGlobal.y - etf.pos.y;
            if (std::sqrt(dx*dx + dy*dy) < TILE_CONNECT_DIST) {
                if (etf.floorLevel > playerFloorLevel) {
                    playerFloorLevel = etf.floorLevel;
                }
            }
        }

        // Check if player is on stairs  gives fractional climb offset on top of floor offset
        for (auto& s : structures) {
            if (s.id == ITEM_WOOD_STAIRS && s.sprite.has_value()) {
                sf::FloatRect sb = s.sprite->getGlobalBounds();
                sb.position.x -= 30.f; sb.size.x += 60.f;
                sb.position.y -= 30.f; sb.size.y += 60.f;
                if (sb.contains(pFeetGlobal)) {
                    playerOnStairs = true;
                    float progress = 0.f;
                    if (s.rotation == 90.f) {
                        progress = (pFeetGlobal.x - sb.position.x) / sb.size.x;
                    } else if (s.rotation == 180.f) {
                        progress = 1.0f - (pFeetGlobal.x - sb.position.x) / sb.size.x;
                    } else {
                        progress = 1.0f - (pFeetGlobal.y - sb.position.y) / sb.size.y;
                    }
                    if (progress < 0.f) progress = 0.f;
                    if (progress > 1.f) progress = 1.f;
                    // Stair climb interpolates toward next floor level
                    float baseOffset = -(float)playerFloorLevel * 180.f;
                    stairOffset = baseOffset + progress * -180.f;
                }
            }
        }

        // If fully on an elevated tile (not on stairs), use fixed floor offset
        if (!playerOnStairs && playerFloorLevel > 0) {
            stairOffset = -(float)playerFloorLevel * 180.f;
        }

        // --- isUnderRoof / isUnderElevatedTile check ---
        // Player is "under" a higher floor if there is an elevated tile of a higher floor
        // directly above (overlapping X, lower Y) the player
        bool isUnderElevatedFloor = false;
        for (auto& etf : elevatedTileFloors) {
            if (etf.floorLevel > playerFloorLevel) {
                float dx = pPos.x - etf.pos.x;
                float dy = pPos.y - etf.pos.y;
                // Check horizontal proximity (within tile footprint ~200x100)
                if (std::abs(dx) < 200.f && std::abs(dy) < 150.f) {
                    isUnderElevatedFloor = true;
                    break;
                }
            }
        }

        for (auto& s : structures) {
            if (s.id == ITEM_WOOD_ROOF || s.id == ITEM_WOOD_WALL) {
                sf::FloatRect rb = s.sprite->getGlobalBounds();
                rb.position.x -= 60.f; rb.size.x += 120.f;
                rb.position.y -= 60.f; rb.size.y += 120.f;
                if (rb.contains(pPos)) {
                    isUnderRoof = true;
                }
            }
        }

        // Apply transparency to buildings if player is inside or under elevated floor
        for (auto& s : structures) {
            if (s.id == ITEM_WOOD_ROOF || s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR) {
                if (s.rotation == 0.f && (s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR)) {
                    // Check if this horizontal structure is a back wall/door (has a wood tile directly below it on the same floor)
                    bool isBackWall = false;
                    if (s.sprite.has_value()) {
                        sf::Vector2f sPos = s.sprite->getPosition();
                        for (auto& t : structures) {
                            if (t.id == ITEM_WOOD_TILE && t.sprite.has_value()) {
                                sf::Vector2f tPos = t.sprite->getPosition();
                                if (std::abs(tPos.x - sPos.x) < 50.f && std::abs(tPos.y - (sPos.y + 100.f)) < 50.f) {
                                    isBackWall = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (isBackWall) {
                        // Back walls & doors: invisible when player is outside, semi-transparent when inside
                        if (isUnderRoof || isUnderElevatedFloor) s.sprite->setColor(sf::Color(255, 255, 255, 128));
                        else s.sprite->setColor(sf::Color(255, 255, 255, 0));
                    } else {
                        // Front walls & doors: semi-transparent when inside, fully visible when outside
                        if (isUnderRoof || isUnderElevatedFloor) s.sprite->setColor(sf::Color(255, 255, 255, 128));
                        else s.sprite->setColor(sf::Color::White);
                    }
                } else {
                    // Side walls and roofs: semi-transparent when inside, fully visible when outside
                    if (isUnderRoof || isUnderElevatedFloor) s.sprite->setColor(sf::Color(255, 255, 255, 128));
                    else s.sprite->setColor(sf::Color::White);
                }
            }
        }

        // Build set of elevated tile positions for fast lookup
        auto tileIsElevated = [&](sf::Vector2f tp) -> bool {
            for (auto& etf : elevatedTileFloors) {
                float dx = tp.x - etf.pos.x;
                float dy = tp.y - etf.pos.y;
                if (dx*dx + dy*dy < 1.f) return true;
            }
            return false;
        };

        // Apply semi-transparency to elevated tiles when player is below them
        for (auto& s : structures) {
            if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
                if (isUnderElevatedFloor && tileIsElevated(s.sprite->getPosition())) {
                    s.sprite->setColor(sf::Color(255, 255, 255, 128));
                } else {
                    s.sprite->setColor(sf::Color::White);
                }
            }
        }

        // 1. DRAW GROUND-FLOOR TILES FIRST (non-elevated)
        for (auto& s : structures) {
            if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
                if (tileIsElevated(s.sprite->getPosition())) continue; // skip, drawn in depth pass
                float dx = s.sprite->getPosition().x - sprite.getPosition().x;
                float dy = s.sprite->getPosition().y - sprite.getPosition().y;
                if (dx * dx + dy * dy < 3000000.f) {
                    sf::Transform trans;
                    trans.translate(s.sprite->getPosition());
                    trans.combine(sf::Transform(1.f, 0.f, 0.f, s.shearY, 1.f, 0.f, 0.f, 0.f, 1.f));
                    trans.translate(-s.sprite->getPosition());
                    window.draw(*s.sprite, trans);
                }
            }
        }

        // Helper lambda getFloorLevelFromY removed (using inline global function instead)

        // 2. PREPARE ELEVATED DRAWABLES (for depth sorting)
        struct Drawable { float groundY; float z; int priority; std::function<void()> draw; };
        std::vector<Drawable> depthDrawables;

        for (auto& s : structures) {
            if (!s.sprite.has_value()) continue;
            float dx = s.sprite->getPosition().x - sprite.getPosition().x;
            float dy = s.sprite->getPosition().y - sprite.getPosition().y;
            if (dx * dx + dy * dy >= 3000000.f) continue;

            float sPos_y = s.sprite->getPosition().y;
            int sFloor = getFloorLevelFromY(sPos_y);
            float sGroundY = sPos_y + sFloor * 180.f;
            float sZ = sFloor * 180.f;

            if (s.id == ITEM_WOOD_TILE) {
                if (tileIsElevated(s.sprite->getPosition())) {
                    depthDrawables.push_back({ sGroundY, sZ, 0, [&s, &window]() {
                        if (s.sprite.has_value()) {
                            sf::Transform trans;
                            trans.translate(s.sprite->getPosition());
                            trans.combine(sf::Transform(1.f, 0.f, 0.f, s.shearY, 1.f, 0.f, 0.f, 0.f, 1.f));
                            trans.translate(-s.sprite->getPosition());
                            window.draw(*s.sprite, trans);
                        }
                    } });
                }
            } else {
                int priority = (s.id == ITEM_WOOD_ROOF) ? 2 : 1;
                depthDrawables.push_back({ sGroundY, sZ, priority, [&s, &window]() {
                    if (s.sprite.has_value()) {
                        sf::Transform trans;
                        trans.translate(s.sprite->getPosition());
                        trans.combine(sf::Transform(1.f, 0.f, 0.f, s.shearY, 1.f, 0.f, 0.f, 0.f, 1.f));
                        trans.translate(-s.sprite->getPosition());
                        window.draw(*s.sprite, trans);
                    }
                } });
            }
        }

        if (saplingPlacementMode)
        {
            if (saplingFollowingCursor)
            {
                sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                sf::Vector2f worldM = window.mapPixelToCoords(pixelPos);
                saplingPlacementPos = snapToShearedGrid(worldM);
            }

            sf::Sprite ghost(saplingTex);

            ghost.setScale({ 0.2f, 0.2f });
            ghost.setColor(sf::Color(255, 255, 255, 128));
            ghost.setPosition(saplingPlacementPos);

            window.draw(ghost);
        }


        for (auto& s : plantedSaplings)
        {
            if (!s.grown)
            {
                sf::Sprite spr(saplingTex);

                spr.setScale({ 0.2f, 0.2f });
                spr.setPosition(s.position);

                window.draw(spr);

                sf::Text t(font,
                    "Growing: " + std::to_string((int)s.growTimer) + "s",
                    16);

                t.setFillColor(sf::Color::White);
                t.setPosition({
                    s.position.x - 30.f,
                    s.position.y - 40.f
                    });

                window.draw(t);
            }
        }

        // Add Player to depth sorting
        if (isPlayerHurt) {
            playerHurtTimer -= dt;
            if (playerHurtTimer <= 0.f) isPlayerHurt = false;
            sprite.setColor(sf::Color(255, 100, 100));
        }
        else {
            sprite.setColor(sf::Color::White);
        }
        sf::Vector2f originalPlayerPos = sprite.getPosition();
        sprite.move({ 0.f, stairOffset });
        depthDrawables.push_back({ originalPlayerPos.y, -stairOffset, 1, [&sprite, &window]() { window.draw(sprite); } });

        for (auto& a : animals) {
            if (a->health > 0) {
                float dx = a->sprite.getPosition().x - sprite.getPosition().x;
                float dy = a->sprite.getPosition().y - sprite.getPosition().y;
                if (dx * dx + dy * dy < 3000000.f) {
                    depthDrawables.push_back({ a->sprite.getPosition().y, 0.f, 1, [&a, &window]() { window.draw(a->sprite); } });
                }
            }
        }
        for (auto& m : monsters) {
            if (m->health > 0) {
                float dx = m->sprite.getPosition().x - sprite.getPosition().x;
                float dy = m->sprite.getPosition().y - sprite.getPosition().y;
                if (dx * dx + dy * dy < 3000000.f) {
                    depthDrawables.push_back({ m->sprite.getPosition().y, 0.f, 1, [&m, &window]() { window.draw(m->sprite); } });
                }
            }
        }
        std::sort(depthDrawables.begin(), depthDrawables.end(), [](const Drawable& a, const Drawable& b) {
            if (std::abs(a.groundY - b.groundY) > 0.001f) {
                return a.groundY < b.groundY;
            }
            if (std::abs(a.z - b.z) > 0.001f) {
                return a.z < b.z;
            }
            return a.priority < b.priority;
            });

        for (auto& d : depthDrawables) d.draw();

        // Restore player position for logic
        sprite.setPosition(originalPlayerPos);

        sprite.setColor(sf::Color::White);

        // Removed old playerSwordSprite drawing code
        if (animLoadedForSelection) {
            if (npcFemale.sprite.has_value()) window.draw(*npcFemale.sprite);
            if (npcMale.sprite.has_value()) window.draw(*npcMale.sprite);
        }

        // Draw speech bubble if active
        if (bubbleTimer > 0.f && !latestPlayerMsg.empty()) {
            sf::Text bubbleText(font, latestPlayerMsg, 24);
            bubbleText.setFillColor(sf::Color::Black);
            sf::FloatRect textBounds = bubbleText.getLocalBounds();

            float paddingX = 15.f;
            float paddingY = 10.f;
            sf::RectangleShape bubbleBg({ textBounds.size.x + paddingX * 2.f, textBounds.size.y + paddingY * 2.f });
            bubbleBg.setFillColor(sf::Color(255, 255, 255, 220));
            bubbleBg.setOutlineColor(sf::Color::Black);
            bubbleBg.setOutlineThickness(3.f);

            sf::Vector2f playerPos = sprite.getPosition();
            // Place bubble above the name tag
            bubbleBg.setPosition({ playerPos.x - bubbleBg.getSize().x / 2.f, playerPos.y - 360.f });
            bubbleText.setPosition({ bubbleBg.getPosition().x + paddingX, bubbleBg.getPosition().y + paddingY - 6.f });

            window.draw(bubbleBg);
            window.draw(bubbleText);
        }

        // Draw player name and wrench (above player)
        sf::Text nameText(font, username, 18);
        nameText.setFillColor(sf::Color::White);
        nameText.setOutlineColor(sf::Color::Black);
        nameText.setOutlineThickness(1.5f);
        nameText.setPosition({ sprite.getPosition().x - nameText.getLocalBounds().size.x / 2.f, sprite.getPosition().y - 280.f });
        playerWrenchBtn.setPosition({ nameText.getPosition().x + nameText.getLocalBounds().size.x + 5.f, nameText.getPosition().y });
        window.draw(nameText);
        window.draw(playerWrenchBtn);

        // Draw NPC name and wrench (above NPC)
        if (animLoadedForSelection) {
            if (npcFemale.sprite.has_value() && npcFemale.wrenchBtn.has_value()) {
                sf::Text npcFemaleNameText(font, npcFemale.name, 18);
                npcFemaleNameText.setFillColor(sf::Color::White);
                npcFemaleNameText.setOutlineColor(sf::Color::Black);
                npcFemaleNameText.setOutlineThickness(1.5f);
                npcFemaleNameText.setPosition({ npcFemale.sprite->getPosition().x - npcFemaleNameText.getLocalBounds().size.x / 2.f, npcFemale.sprite->getPosition().y - 280.f });
                npcFemale.wrenchBtn->setPosition({ npcFemaleNameText.getPosition().x + npcFemaleNameText.getLocalBounds().size.x + 5.f, npcFemaleNameText.getPosition().y });
                window.draw(npcFemaleNameText);
                window.draw(*npcFemale.wrenchBtn);
            }

            if (npcMale.sprite.has_value() && npcMale.wrenchBtn.has_value()) {
                sf::Text npcMaleNameText(font, npcMale.name, 18);
                npcMaleNameText.setFillColor(sf::Color::White);
                npcMaleNameText.setOutlineColor(sf::Color::Black);
                npcMaleNameText.setOutlineThickness(1.5f);
                npcMaleNameText.setPosition({ npcMale.sprite->getPosition().x - npcMaleNameText.getLocalBounds().size.x / 2.f, npcMale.sprite->getPosition().y - 280.f });
                npcMale.wrenchBtn->setPosition({ npcMaleNameText.getPosition().x + npcMaleNameText.getLocalBounds().size.x + 5.f, npcMaleNameText.getPosition().y });
                window.draw(npcMaleNameText);
                window.draw(*npcMale.wrenchBtn);
            }
        }

        if (nearPond)
        {
            drinkText.setPosition({
                sprite.getPosition().x - 160.f,
                sprite.getPosition().y - 100.f
                });

            window.draw(drinkText);
        }

        // Door Interaction UI
        PlacedStructure* nearestDoor = nullptr;
        float minDist = 300.f;
        for (auto& s : structures) {
            if (s.id == ITEM_WOOD_DOOR) {
                sf::Vector2f dPos = s.sprite->getPosition();
                sf::Vector2f pPos = sprite.getPosition();
                int doorFloor = getFloorLevelFromY(dPos.y);
                if (doorFloor == playerFloorLevel) {
                    float dy = (dPos.y + doorFloor * 180.f) - pPos.y;
                    float dx = dPos.x - pPos.x;
                    float d = std::sqrt(dx*dx + dy*dy);
                    if (d < minDist) {
                        minDist = d;
                        nearestDoor = &s;
                    }
                }
            }
        }

        if (nearestDoor) {
            sf::RectangleShape doorBtn({ 200.f, 60.f });
            doorBtn.setPosition({ nearestDoor->sprite->getPosition().x - 100.f, nearestDoor->sprite->getPosition().y - 120.f });

            sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            bool isHovered = doorBtn.getGlobalBounds().contains(mpos);
            doorBtn.setFillColor(isHovered ? sf::Color(80, 160, 220, 230) : sf::Color(60, 140, 200, 200));
            doorBtn.setOutlineThickness(3.f);
            doorBtn.setOutlineColor(isHovered ? sf::Color::White : sf::Color(255, 255, 255, 120));
            window.draw(doorBtn);

            sf::Text doorTxt(font, nearestDoor->isOpen ? "Close Door" : "Open Door", 28);
            sf::FloatRect b = doorTxt.getLocalBounds();
            doorTxt.setPosition({ doorBtn.getPosition().x + (200.f - b.size.x) / 2.f, doorBtn.getPosition().y + (60.f - b.size.y) / 2.f - 8.f });
            window.draw(doorTxt);
        }

        // Bed Interaction UI
        PlacedStructure* nearestBed = nullptr;
        float minBedDist = 180.f;
        for (auto& s : structures) {
            if (s.id == ITEM_BED) {
                sf::Vector2f bPos = s.sprite->getPosition();
                sf::Vector2f pPos = sprite.getPosition();
                int bedFloor = getFloorLevelFromY(bPos.y);
                if (bedFloor == playerFloorLevel) {
                    float dy = (bPos.y + bedFloor * 180.f) - pPos.y;
                    float dx = bPos.x - pPos.x;
                    float d = std::sqrt(dx*dx + dy*dy);
                    if (d < minBedDist) {
                        minBedDist = d;
                        nearestBed = &s;
                    }
                }
            }
        }

        if (nearestBed && !isSleeping) {
            sf::RectangleShape sleepBtn({ 200.f, 60.f });
            sleepBtn.setPosition({ nearestBed->sprite->getPosition().x - 100.f, nearestBed->sprite->getPosition().y - 120.f });

            sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            bool isHovered = sleepBtn.getGlobalBounds().contains(mpos);
            sleepBtn.setFillColor(isHovered ? sf::Color(80, 160, 220, 230) : sf::Color(60, 140, 200, 200));
            sleepBtn.setOutlineThickness(3.f);
            sleepBtn.setOutlineColor(isHovered ? sf::Color::White : sf::Color(255, 255, 255, 120));
            window.draw(sleepBtn);

            sf::Text sleepTxt(font, "Sleep", 28);
            sf::FloatRect b = sleepTxt.getLocalBounds();
            sleepTxt.setPosition({ sleepBtn.getPosition().x + (200.f - b.size.x) / 2.f, sleepBtn.getPosition().y + (60.f - b.size.y) / 2.f - 8.f });
            window.draw(sleepTxt);
        }

        for (auto& a : animals)
        {
            if (a->health > 0)
            {
                window.draw(a->sprite);

                if (a->health < a->maxHealth)
                {
                    sf::FloatRect bounds = a->sprite.getGlobalBounds();
                    sf::RectangleShape hpBg({ 40.f, 6.f });
                    hpBg.setFillColor(sf::Color::Red);
                    hpBg.setPosition({ bounds.position.x + bounds.size.x / 2.f - 20.f, bounds.position.y - 15.f });

                    sf::RectangleShape hpFg({ 40.f * (std::max(0.f, a->health) / a->maxHealth), 6.f });
                    hpFg.setFillColor(sf::Color::Green);
                    hpFg.setPosition(hpBg.getPosition());

                    window.draw(hpBg);
                    window.draw(hpFg);
                }
            }
        }

        for (auto& a : animals)
        {
            if (a->health > 0) {
                float dx = a->sprite.getPosition().x - sprite.getPosition().x;
                float dy = a->sprite.getPosition().y - sprite.getPosition().y;
                if (dx * dx + dy * dy < 3000000.f) {
                    a->update(dt, sprite.getPosition(), isBlocked);
                }
            }

            // ---- CLAMP ANIMAL TO MAP (FULL SPRITE) ----
            sf::FloatRect bounds = a->sprite.getGlobalBounds();
            sf::Vector2f pos = a->sprite.getPosition();

            if (bounds.position.x < 0.f)
                pos.x -= bounds.position.x;

            if (bounds.position.y < 0.f)
                pos.y -= bounds.position.y;

            if (bounds.position.x + bounds.size.x > MAP_WIDTH)
                pos.x -= (bounds.position.x + bounds.size.x - MAP_WIDTH);

            if (bounds.position.y + bounds.size.y > MAP_HEIGHT)
                pos.y -= (bounds.position.y + bounds.size.y - MAP_HEIGHT);

            a->sprite.setPosition(pos);
        }

        for (auto& a : animals)
        {
            if (a->health <= 0 && !a->droppedLoot)
            {
                itemManager.addItem(
                    a->getFoodItemID(),
                    *a->getFoodTexture(),
                    a->sprite.getPosition(),
                    a->getFoodDropCount()
                );

                if (a->getFoodItemID() == ITEM_SHEEP_MEAT)
                {
                    int woolDropCount = 1 + rand() % 2;
                    itemManager.addItem(
                        ITEM_WOOL,
                        woolTex,
                        a->sprite.getPosition(),
                        woolDropCount
                    );
                }

                a->droppedLoot = true;
            }
        }

        auto& items = itemManager.getItems();
        for (auto it = items.begin(); it != items.end(); )
        {
            if (it->id == ITEM_TREE && it->health <= 0)
            {
                sf::Vector2f pos = it->sprite.getPosition();
                it = items.erase(it);

                itemManager.addItem(ITEM_WOOD, woodTex, pos, 1);

                if (!tutorialActive && qInfo.questID == 1 && qInfo.progress < 10) {
                    qInfo.progress++;
                    authManager.updateQuestProgress(username.c_str(), qInfo.progress, qInfo.isCompleted);
                }

                int chance = rand() % 100;
                if (chance < 80)
                {
                    itemManager.addItem(ITEM_SAPLING, saplingTex, pos, 1);
                    std::cout << "Obtained Sapling!\n";
                }
            }
            else
            {
                ++it;
            }
        }



        bool structureDestroyed = false;
        for (auto it = structures.begin(); it != structures.end(); ) {
            if (it->health <= 0) {
                it = structures.erase(it);
                structureDestroyed = true;
            }
            else {
                if (it->isHurt) {
                    it->hurtTimer -= dt;
                    if (it->hurtTimer <= 0.f) it->isHurt = false;
                }
                ++it;
            }
        }
        if (structureDestroyed) {
            syncSaveLocalStructuresAndCampfires();
        }

        for (auto& m : monsters)
        {
            if (m->health <= 0 && !m->droppedLoot)
            {
                itemManager.addItem(
                    m->getFoodItemID(),
                    *m->getFoodTexture(),
                    m->sprite.getPosition(),
                    m->getFoodDropCount()
                );

                m->droppedLoot = true;
            }
        }

        for (auto& m : monsters)
        {
            if (m->health > 0)
            {
                window.draw(m->sprite);

                if (m->health < m->maxHealth)
                {
                    sf::FloatRect bounds = m->sprite.getGlobalBounds();
                    sf::RectangleShape hpBg({ 40.f, 6.f });
                    hpBg.setFillColor(sf::Color::Red);
                    hpBg.setPosition({ bounds.position.x + bounds.size.x / 2.f - 20.f, bounds.position.y - 15.f });

                    sf::RectangleShape hpFg({ 40.f * (std::max(0.f, m->health) / m->maxHealth), 6.f });
                    hpFg.setFillColor(sf::Color::Green);
                    hpFg.setPosition(hpBg.getPosition());

                    window.draw(hpBg);
                    window.draw(hpFg);
                }
            }
        }



        float nearestDist = itemManager.getNearestDistance(sprite);
        int nearestID = itemManager.getNearestItemID(sprite);
        const float RADIUS = 250.f;

        if (nearestDist < RADIUS)
        {
            sf::Text prompt(font, "", 32);
            prompt.setFillColor(sf::Color::Yellow);

            prompt.setString("Press E to pick up");

            prompt.setPosition({ sprite.getPosition().x - 120, sprite.getPosition().y - 80 });

            window.draw(prompt);
        }

        static sf::Clock drinkClock;

        if (sf::Keyboard::isKeyPressed(globalSettings.keyInteract))
        {
            if (tutorialActive && tutorialStep == 4 && nearestID == ITEM_STONE) {
                tutStoneGathered++;
            }

            if (!tutorialActive && nearestID == ITEM_STONE && nearestDist < 100.f && qInfo.questID == 2 && qInfo.progress < 8) {
                qInfo.progress++;
                authManager.updateQuestProgress(username.c_str(), qInfo.progress, qInfo.isCompleted);
            }

            if (!tutorialActive && (nearestID == ITEM_CHICKEN_MEAT || nearestID == ITEM_PIG_MEAT || nearestID == ITEM_ZOMBIE_FLESH) && nearestDist < 100.f && qInfo.questID == 4 && qInfo.progress < 5) {
                qInfo.progress++;
                authManager.updateQuestProgress(username.c_str(), qInfo.progress, qInfo.isCompleted);
            }

            itemManager.tryPickup(sprite, inventory);


            if (nearPond && drinkClock.getElapsedTime().asSeconds() > 1.f)
            {

                float waterRestore = 40.f;

                // Restore thirst
                thirst -= waterRestore;

                // Overflow system
                while (thirst < 0.f && thirstMultiplier < MAX_MULTIPLIER)
                {
                    thirst += maxThirst;
                    thirstMultiplier++;
                }

                thirst = (std::max)(0.f, thirst);
                std::cout << "Thirst Multiplier = " << thirstMultiplier << "\n";
                drinkClock.restart();
            }
        }

        static sf::Clock stoneRespawnClock;
        if (stoneRespawnClock.getElapsedTime().asSeconds() > 10.f)
        {
            int currentStones = 0;
            for (const auto& item : itemManager.getItems()) {
                if (item.id == ITEM_STONE) currentStones++;
            }

            if (currentStones < 15) // Maintain 15 stones max
            {
                for (int a = 0; a < 20; ++a)
                {
                    sf::Vector2f p(randX(gen), randY(gen));
                    // Snap to sheared grid
                    p = snapToShearedGrid(p);

                    // Skip if near a structure or campfire
                    bool nearPlaceable = false;
                    for (auto& s : structures) {
                        if (s.sprite.has_value()) {
                            sf::Vector2f sPos = s.sprite->getPosition();
                            float dx = p.x - sPos.x, dy = p.y - sPos.y;
                            if (std::sqrt(dx*dx + dy*dy) < 120.f) { nearPlaceable = true; break; }
                        }
                    }
                    if (!nearPlaceable) {
                        for (auto& c : campfires) {
                            float dx = p.x - c.position.x, dy = p.y - c.position.y;
                            if (std::sqrt(dx*dx + dy*dy) < 120.f) { nearPlaceable = true; break; }
                        }
                    }

                    if (!nearPlaceable && itemManager.isAreaFreeExplicit(p, 120.f, &stoneTex))
                    {
                        itemManager.addItem(ITEM_STONE, stoneTex, p, 1);
                        break;
                    }
                }
            }
            stoneRespawnClock.restart();
        }

        // --- Multiplayer Sync (skip in startup-safe mode to avoid frame stalls) ---
        if (dbStartupEnabled) {
            static sf::Clock syncClock;
            if (!syncInFlight && syncClock.getElapsedTime().asSeconds() > 0.25f)
            {
                syncInFlight = true;

                // Capture values BEFORE thread starts
                float px = sprite.getPosition().x;
                float py = sprite.getPosition().y;
                std::string uname = username;

                syncFuture = std::async(std::launch::async,
                    [&authManager, uname, px, py]() -> SyncResult
                    {
                        SyncResult result;

                        // Update player position online
                        authManager.updateOnlineStatus(uname.c_str(), px, py);

                        // Get all players
                        result.players = authManager.getOnlinePlayers(uname.c_str());

                        // Get self info
                        OnlinePlayer self = authManager.getSelfOnlineInfo(uname.c_str());

                        // Pull handling
                        if (!self.pullBy.empty())
                        {
                            result.hasPull = true;
                            result.pullByUsername = self.pullBy;

                            for (const auto& op : result.players)
                            {
                                if (op.username == self.pullBy)
                                {
                                    result.pullPosition = { op.x, op.y };
                                    break;
                                }
                            }

                            authManager.clearPull(uname.c_str());
                        }

                        // Server epoch for day/night sync
                        result.serverEpoch = authManager.getServerEpochSeconds();

                        // Periodic structure + campfire sync every ~5s (20 ticks x 0.25s)
                        static int structSyncTick = 0;
                        ++structSyncTick;
                        if (structSyncTick >= 20) {
                            result.otherStructures = authManager.loadOtherStructures(uname.c_str());
                            result.otherCampfires  = authManager.loadOtherCampfires(uname.c_str());
                            result.structuresUpdated = true;
                            structSyncTick = 0;
                        }

                        // P2P trade invite check
                        result.pendingTradeID = authManager.getPendingP2PTradeFor(uname);
                        if (result.pendingTradeID > 0) {
                            result.pendingTradeState = authManager.getP2PTradeState(result.pendingTradeID);
                        }

                        return result;
                    });

                syncClock.restart();
            }

            // Consume result when the async task is done (non-blocking check)
            if (syncInFlight && syncFuture.valid()) {
                try {
                    if (syncFuture.wait_for(std::chrono::milliseconds(0))
                        == std::future_status::ready)
                    {
                        SyncResult res = syncFuture.get();

                        otherPlayers = res.players;

                        // Cache server epoch for day/night sync
                        if (res.serverEpoch > 0) serverEpochCache = res.serverEpoch;

                        // Apply live other-player structure/campfire updates
                        if (res.structuresUpdated) {
                            structures.erase(
                                std::remove_if(structures.begin(), structures.end(),
                                    [&](const PlacedStructure& s) {
                                        return !s.owner.empty() && s.owner != username;
                                    }),
                                structures.end());
                            for (auto& s : res.otherStructures) {
                                // Discard if not on a grid cell origin
                                sf::Vector2f snapped = snapToShearedGrid({s.x, s.y});
                                if (std::abs(s.x - snapped.x) > 1.f || std::abs(s.y - snapped.y) > 1.f) {
                                    continue;
                                }
                                const sf::Texture* tex = nullptr;
                                if      (s.id == ITEM_FURNACE)     tex = &furnaceTex;
                                else if (s.id == ITEM_WOOD_TILE)   tex = &woodTileTex;
                                else if (s.id == ITEM_WOOD_WALL)   tex = &woodWallTex;
                                else if (s.id == ITEM_WOOD_DOOR)   tex = &woodDoorTex;
                                else if (s.id == ITEM_WOOD_ROOF)   tex = &woodRoofTex;
                                else if (s.id == ITEM_WOOD_STAIRS) tex = &woodStairsTex;
                                else if (s.id == ITEM_BED)         tex = &bedTex;
                                if (tex) {
                                    structures.emplace_back(s.id, *tex, s.rotation, s.isOpen, s.shearY, "other");
                                    auto& ns = structures.back();
                                    ns.sprite->setPosition({ s.x, s.y });
                                    ns.health = s.health;
                                    sf::Vector2u ts = ns.sprite->getTexture().getSize();
                                    ns.sprite->setOrigin({ ts.x / 2.f, (float)ts.y });
                                    applyStructureScale(*ns.sprite, s.id, s.rotation, s.isOpen, ns.shearY);
                                }
                            }
                            campfires.erase(
                                std::remove_if(campfires.begin(), campfires.end(),
                                    [&](const PlacedCampfire& c) {
                                        return !c.owner.empty() && c.owner != username;
                                    }),
                                campfires.end());
                            for (auto& c : res.otherCampfires) {
                                PlacedCampfire pc;
                                pc.position      = { c.x, c.y };
                                pc.timeRemaining = c.timeRemaining;
                                pc.isCrafted     = c.isCrafted;
                                pc.currentFrame  = 0;
                                pc.frameTimer    = 0.f;
                                pc.fuel          = c.fuel;
                                pc.inputItem     = c.inputItem;
                                pc.inputCount    = c.inputCount;
                                pc.outputItem    = c.outputItem;
                                pc.outputCount   = c.outputCount;
                                pc.owner         = c.owner;
                                campfires.push_back(pc);
                            }
                        }

                        // Update interpolation targets
                        for (const auto& op : otherPlayers)
                        {
                            sf::Vector2f target(op.x, op.y);

                            onlinePlayerTargets[op.username] = target;

                            if (onlinePlayerDisplayPos.find(op.username)
                                == onlinePlayerDisplayPos.end())
                            {
                                onlinePlayerDisplayPos[op.username] = target;
                            }
                        }
                        // Remove players who dropped offline
                        for (auto it = onlinePlayerDisplayPos.begin(); it != onlinePlayerDisplayPos.end(); ) {
                            bool found = false;
                            for (const auto& op : otherPlayers) if (op.username == it->first) { found = true; break; }
                            it = found ? std::next(it) : onlinePlayerDisplayPos.erase(it);
                        }

                        if (res.hasPull && !res.pullByUsername.empty()) {
                            sprite.setPosition(res.pullPosition);
                            latestPlayerMsg = "I've been pulled by " + res.pullByUsername + "!";
                            bubbleTimer = 4.f;
                        }

                        if (p2pTradeID <= 0 && !p2pTradeOpen && !p2pInvitePopup && res.pendingTradeID > 0) {
                            p2pTradeID = res.pendingTradeID;
                            p2pAmInitiator = false;
                            p2pState = res.pendingTradeState;
                            p2pPartner = p2pState.initiator;
                            p2pInvitePopup = true;
                            p2pStatusMsg = "Pending invite from " + p2pPartner;
                        }

                        syncInFlight = false;
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "[ERROR] Asynchronous sync failure: " << e.what() << "\n";
                    syncInFlight = false;
                }
                catch (...) {
                    std::cerr << "[ERROR] Asynchronous sync unknown failure\n";
                    syncInFlight = false;
                }
            }
        }

        // Smoothly interpolate displayed positions toward DB targets every frame
        {
            const float LERP_SPEED = 5.0f; // units per second factor
            for (auto& kv : onlinePlayerDisplayPos)
            {
                auto it = onlinePlayerTargets.find(kv.first);

                if (it != onlinePlayerTargets.end())
                {
                    sf::Vector2f& cur = kv.second;
                    const sf::Vector2f& tgt = it->second;

                    float lerpSpeed = 10.0f;

                    float factor = (std::min)(1.0f, static_cast<float>(lerpSpeed * dt));

                    cur.x += (tgt.x - cur.x) * factor;
                    cur.y += (tgt.y - cur.y) * factor;
                }
            }
        }

        // Render Online Players using interpolated (smooth) positions
        for (const auto& op : otherPlayers) {
            auto dispIt = onlinePlayerDisplayPos.find(op.username);
            sf::Vector2f renderPos = (dispIt != onlinePlayerDisplayPos.end())
                ? dispIt->second
                : sf::Vector2f(op.x, op.y);

            sf::Sprite opSprite(op.gender == 2 ? texFemaleOnline : texMaleOnline);
            sf::Vector2u ts_init = opSprite.getTexture().getSize();
            if (ts_init.x > 0 && ts_init.y > 0) {
                opSprite.setOrigin({ ts_init.x / 2.f, ts_init.y / 2.f });
                opSprite.setScale({ 500.f / ts_init.x, 500.f / ts_init.y });
            }

            opSprite.setPosition(renderPos);
            window.draw(opSprite);

            // Name tag above character (matching local player height)
            sf::Text opName(uiFont, op.username, 16);
            opName.setFillColor(sf::Color::White);
            opName.setOutlineColor(sf::Color::Black);
            opName.setOutlineThickness(1.f);
            sf::FloatRect nb = opName.getLocalBounds();
            opName.setPosition({ renderPos.x - nb.size.x / 2.f, renderPos.y - 280.f });
            window.draw(opName);

            // Wrench icon to the right of the name tag
            sf::Sprite opWrench(wrenchTex);
            opWrench.setScale({ 0.05f, 0.05f });
            float wIconX = opName.getPosition().x + opName.getLocalBounds().size.x + 5.f;
            float wIconY = opName.getPosition().y;
            opWrench.setPosition({ wIconX, wIconY });
            window.draw(opWrench);
            // Store wrench hitbox in world space for click detection
            onlinePlayerWrenchBounds[op.username] = sf::FloatRect(
                { wIconX, wIconY },
                { opWrench.getGlobalBounds().size.x, opWrench.getGlobalBounds().size.y });
        }


    }

    // Update Stock Market Prices every 60 seconds
    if (stockUpdateClock.getElapsedTime().asSeconds() >= 60.f) {
        stockUpdateClock.restart();
        for (auto& item : stockMarket) {
            if (item.history.size() >= 10) {
                item.history.erase(item.history.begin());
            }
            item.history.push_back(item.currentPrice);

            // Volatile stocks fluctuate more per step
            float baseRange = 15.f * item.volatility;
            int rangeInt = (int)(baseRange * 2.f) + 1;
            float percentChange = (-(int)baseRange + (rand() % (rangeInt > 0 ? rangeInt : 1))) / 100.f;

            float dev = (item.currentPrice - item.basePrice) / item.basePrice;
            percentChange -= dev * 0.05f;

            item.currentPrice = item.currentPrice * (1.f + percentChange);

            // Allow wider price range for volatile stocks, tighter bounds for stable ones
            float minP = item.basePrice * (item.volatility > 1.2f ? 0.2f : 0.5f);
            float maxP = item.basePrice * (item.volatility > 1.2f ? 4.0f : 1.8f);
            if (item.currentPrice < minP) item.currentPrice = minP;
            if (item.currentPrice > maxP) item.currentPrice = maxP;

            item.currentPrice = std::round(item.currentPrice);
            authManager.saveMarketPrice(item.id, item.currentPrice);
            if (item.currentPrice < 1.f) item.currentPrice = 1.f;

            if (item.history.size() >= 2) {
                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                float baseNoise = 8.f * item.volatility;
                int noiseRange = (int)(baseNoise * 2.f) + 1;
                float noise = (-(int)baseNoise + (rand() % (noiseRange > 0 ? noiseRange : 1))) / 100.f;
                item.predictedPrice = std::round(item.currentPrice + diff * 0.5f + item.currentPrice * noise);
                if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;

                // Volatility changes the threshold for UP/DOWN trends slightly
                float trendThreshold = item.volatility > 1.2f ? 0.08f : 0.04f;
                if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) {
                    item.trend = "UP";
                }
                else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) {
                    item.trend = "DOWN";
                }
                else {
                    item.trend = "STABLE";
                }
            }
            else {
                item.predictedPrice = item.currentPrice;
                item.trend = "STABLE";
            }
        }

        // Simulate a background market transaction by virtual players to keep statistics alive!
        if (rand() % 100 < 80) { // 80% chance every 60 seconds
            std::vector<std::string> virtualPlayers = { "MinerBob", "LumberjackJack", "HunterMax", "SwordSmith", "GoldDigger", "FarmerFred" };
            std::string vPlayer = virtualPlayers[rand() % virtualPlayers.size()];
            std::string vAction = (rand() % 2 == 0) ? "SELL" : "BUY";

            // Choose a random item (both raw materials & crafted gear!)
            std::vector<std::pair<int, std::string>> vItems = {
                { ITEM_WOOD, "Wood" },
                { ITEM_STONE, "Stone" },
                { ITEM_SAPLING, "Sapling" },
                { ITEM_CHICKEN_MEAT, "Chicken Meat" },
                { ITEM_PIG_MEAT, "Pig Meat" },
                { ITEM_SHEEP_MEAT, "Sheep Meat" },
                { ITEM_WOOL, "Wool" },
                { ITEM_ZOMBIE_FLESH, "Zombie Flesh" },
                { ITEM_COOKED_CHICKEN, "Cooked Chicken" },
                { ITEM_COOKED_PIG, "Cooked Pig" },
                { ITEM_COOKED_SHEEP, "Cooked Sheep" },
                { ITEM_WOODEN_SWORD, "Wooden Sword" },
                { ITEM_SHIELD, "Sturdy Shield" },
                { ITEM_FURNACE, "Furnace" },
                { ITEM_BED, "Bed" },
                { ITEM_WOOD_TILE, "Wood Tile" },
                { ITEM_WOOD_WALL, "Wood Wall" },
                { ITEM_WOOD_DOOR, "Wood Door" },
                { ITEM_WOOD_ROOF, "Wood Roof" },
                { ITEM_WOOD_STAIRS, "Wood Stairs" }
            };
            auto chosenItem = vItems[rand() % vItems.size()];
            int itemId = chosenItem.first;
            std::string itemName = chosenItem.second;

            int qty = 1;
            if (itemId == ITEM_WOOD || itemId == ITEM_STONE || itemId == ITEM_SAPLING) {
                qty = 5 + (rand() % 16); // 5 to 20
            }
            else if (itemId == ITEM_CHICKEN_MEAT || itemId == ITEM_PIG_MEAT || itemId == ITEM_SHEEP_MEAT || itemId == ITEM_WOOL || itemId == ITEM_ZOMBIE_FLESH ||
                     itemId == ITEM_COOKED_CHICKEN || itemId == ITEM_COOKED_PIG || itemId == ITEM_COOKED_SHEEP) {
                qty = 2 + (rand() % 9); // 2 to 10
            }
            else {
                qty = 1 + (rand() % 3); // 1 to 3
            }

            // If it is in the stock market, update its price according to virtual trade!
            for (auto& item : stockMarket) {
                if (item.id == itemId) {
                    if (vAction == "BUY") {
                        item.currentPrice *= (1.0f + 0.005f * qty * item.volatility); // Demand increases price (volatility-adjusted)
                    }
                    else {
                        item.currentPrice *= (1.0f - 0.003f * qty * item.volatility); // Supply decreases price (volatility-adjusted)
                    }
                    float minP = item.basePrice * (item.volatility > 1.2f ? 0.2f : 0.5f);
                    float maxP = item.basePrice * (item.volatility > 1.2f ? 4.0f : 1.8f);
                    if (item.currentPrice < minP) item.currentPrice = minP;
                    if (item.currentPrice > maxP) item.currentPrice = maxP;
                    item.currentPrice = std::round(item.currentPrice);
                    if (item.currentPrice < 1.f) item.currentPrice = 1.f;

                    if (!item.history.empty()) {
                        float lastPrice = item.history.back();
                        float diff = item.currentPrice - lastPrice;
                        item.predictedPrice = std::round(item.currentPrice + diff * 0.5f);
                        if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;

                        float trendThreshold = item.volatility > 1.2f ? 0.04f : 0.02f;
                        if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) item.trend = "UP";
                        else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) item.trend = "DOWN";
                        else item.trend = "STABLE";
                    }
                    break;
                }
            }

            int unitPrice = getItemPrice(itemId);
            int totalG = qty * unitPrice;

            authManager.addTradeRecord(vPlayer.c_str(), vAction, itemName, qty, unitPrice, totalG);
        }
    }
    // Use server epoch for synchronized day/night (all online players same phase)
    float cycleValue = (serverEpochCache > 0)
        ? static_cast<float>(serverEpochCache % static_cast<long long>(totalCycleTime))
        : std::fmod(elapsedGameTime, totalCycleTime);
    float t = cycleValue / totalCycleTime;
    float timeOfDay = (t < 0.5f) ? (t * 2.f) : (1.f - (t - 0.5f) * 2.f);
    int darknessLevel = static_cast<int>(timeOfDay * 200.f);
    darkness.setFillColor(sf::Color(0, 0, 0, darknessLevel));
    window.draw(darkness);

    isNight = (darknessLevel > 150);

    if (inventoryOpen)
    {
        window.setView(GetUIView(&window));

        window.draw(hpBack);
        window.draw(hpBar);
        window.draw(hpIcon);
        window.draw(hpText);

        window.draw(hungerBack);
        window.draw(hungerBar);
        window.draw(hungerIcon);
        window.draw(hungerText);

        window.draw(thirstBack);
        window.draw(thirstBar);
        window.draw(thirstIcon);
        window.draw(thirstText);

        window.draw(hungerMultiplierText);
        window.draw(thirstMultiplierText);

        window.draw(expBack);
        window.draw(expBar);

        window.draw(levelText);

        if (shieldEquipped) {
            window.draw(defBack);
            window.draw(defBar);
            window.draw(defText);
        }

        inventory.draw(window);
    }


    // NIGHT MUSIC LOGIC (FIXED)
    if (isNight && !wasNight)
    {
        wasNight = true;

        monsters.clear();

        const int ZOMBIE_COUNT = 60; // Increased for larger map

        for (int i = 0; i < ZOMBIE_COUNT; ++i)
        {
            monsters.push_back(std::make_unique<Zombie>(
                sf::Vector2f(randX(gen), randY(gen))
            ));
        }

        bgMusic.stop();                     // ✅ STOP DAY MUSIC

        if (!nightMusicLoaded)
        {
            if (nightMusic.openFromFile(
                "Music/Shirfine_-_Illusionary_Daytime_(mp3.pm).mp3"
            ))
            {
                nightMusic.setLooping(true);
                nightMusic.setVolume(globalSettings.volume);
                nightMusic.play();          // ✅ START PLAYING
                nightMusicLoaded = true;
            }
        }

        nightMusic.play();              // ✅ START PLAYING 
    }
    else if (!isNight && (wasNight || (!tutorialActive && !monsters.empty())))
    {
        wasNight = false;

        nightMusic.stop();               // ✅ STOP NIGHT MUSIC
        bgMusic.play();                     // ✅ START DAY MUSIC
        monsters.clear();
    }


    window.setView(GetUIView(&window));

    if (wrenchMenuOpen) {
        sf::RectangleShape menuBox({ 150.f, 30.f });
        menuBox.setPosition(wrenchMenuPos);
        menuBox.setFillColor(sf::Color(40, 40, 40, 240));
        menuBox.setOutlineColor(sf::Color::White);
        menuBox.setOutlineThickness(1.f);

        if (isTargetNPC) {
            // NPC → TRADE, QUEST, MARKET STATS, IN-APP PURCHASE
            menuBox.setSize({ 150.f, 120.f });
            window.draw(menuBox);
            sf::Text tradeTxt(uiFont, "1. Trade", 18);
            tradeTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 5.f });
            window.draw(tradeTxt);
            sf::Text questTxt(uiFont, "2. Daily Quest", 18);
            questTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 35.f });
            window.draw(questTxt);
            sf::Text statsTxt(uiFont, "3. Market Stats", 18);
            statsTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 65.f });
            window.draw(statsTxt);
            sf::Text iapTxt(uiFont, "4. Buy Gold", 18);
            iapTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 95.f });
            window.draw(iapTxt);
        }
        else if (!wrenchTargetPlayer.empty()) {
            // Online Player → TRADE + CRAFT
            menuBox.setSize({ 150.f, 60.f });
            window.draw(menuBox);
            sf::Text trdTxt(uiFont, "Trade with", 15);
            trdTxt.setFillColor(sf::Color(100, 220, 255));
            trdTxt.setPosition({ wrenchMenuPos.x + 6.f, wrenchMenuPos.y + 2.f });
            window.draw(trdTxt);
            sf::Text nameTxt(uiFont, wrenchTargetPlayer, 15);
            nameTxt.setFillColor(sf::Color::Yellow);
            nameTxt.setPosition({ wrenchMenuPos.x + 6.f, wrenchMenuPos.y + 18.f });
            window.draw(nameTxt);
            sf::Text craftTxt2(uiFont, "Craft Items", 18);
            craftTxt2.setPosition({ wrenchMenuPos.x + 6.f, wrenchMenuPos.y + 35.f });
            window.draw(craftTxt2);
        }
        else {
            // Own wrench → Craft only
            menuBox.setSize({ 150.f, 30.f });
            window.draw(menuBox);
            sf::Text craftTxt(uiFont, "Craft Items", 18);
            craftTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 5.f });
            window.draw(craftTxt);
        }
    }

    // =====================================================
    // P2P INVITE POPUP RENDER
    // =====================================================
    if (p2pInvitePopup && p2pTradeID > 0) {
        float pw = 400.f, ph = 160.f;
        float px2 = (width - pw) / 2.f, py2 = (height - ph) / 2.f;
        sf::RectangleShape popupBox({ pw, ph });
        popupBox.setPosition({ px2, py2 });
        popupBox.setFillColor(sf::Color(22, 22, 30, 250));
        popupBox.setOutlineThickness(2.f);
        popupBox.setOutlineColor(sf::Color(100, 200, 255));
        window.draw(popupBox);
        sf::Text pt1(uiFont, "Trade Request!", 22);
        pt1.setStyle(sf::Text::Bold);
        pt1.setFillColor(sf::Color(100, 220, 255));
        pt1.setPosition({ px2 + 20.f, py2 + 14.f });
        window.draw(pt1);
        sf::Text pt2(uiFont, p2pState.initiator + " wants to trade with you.", 16);
        pt2.setFillColor(sf::Color::White);
        pt2.setPosition({ px2 + 20.f, py2 + 50.f });
        window.draw(pt2);
        sf::RectangleShape acceptBtn({ 120.f, 40.f });
        acceptBtn.setPosition({ px2 + 40.f, py2 + 100.f });
        acceptBtn.setFillColor(sf::Color(40, 160, 70));
        window.draw(acceptBtn);
        sf::Text at(uiFont, "Accept", 16);
        at.setPosition({ px2 + 58.f, py2 + 110.f });
        window.draw(at);
        sf::RectangleShape declineBtn({ 120.f, 40.f });
        declineBtn.setPosition({ px2 + 240.f, py2 + 100.f });
        declineBtn.setFillColor(sf::Color(180, 50, 50));
        window.draw(declineBtn);
        sf::Text dt2(uiFont, "Decline", 16);
        dt2.setPosition({ px2 + 256.f, py2 + 110.f });
        window.draw(dt2);
    }

    // =====================================================
    // P2P TRADE WINDOW RENDER (barter + gold hybrid)
    // =====================================================
    if (p2pTradeOpen) {
        float tW = 700.f, tH = 440.f;
        sf::FloatRect tR({ (width - tW) / 2.f, (height - tH) / 2.f }, { tW, tH });

        sf::RectangleShape bg({ tW, tH });
        bg.setPosition(tR.position);
        bg.setFillColor(sf::Color(18, 18, 26, 252));
        bg.setOutlineThickness(2.f);
        bg.setOutlineColor(sf::Color(100, 200, 255));
        window.draw(bg);

        // Header
        sf::RectangleShape hdr({ tW, 48.f });
        hdr.setPosition(tR.position);
        hdr.setFillColor(sf::Color(30, 55, 100));
        window.draw(hdr);
        sf::Text htxt(uiFont, "P2P TRADE: You  ⇔  " + p2pPartner, 22);
        htxt.setStyle(sf::Text::Bold);
        htxt.setFillColor(sf::Color(180, 230, 255));
        htxt.setPosition({ tR.position.x + 16.f, tR.position.y + 10.f });
        window.draw(htxt);

        // Divider
        sf::RectangleShape div({ 2.f, tH - 48.f });
        div.setPosition({ tR.position.x + tW / 2.f, tR.position.y + 48.f });
        div.setFillColor(sf::Color(80, 120, 180, 160));
        window.draw(div);

        // ---- LEFT: MY OFFER ----
        sf::Text myLabel(uiFont, "YOUR OFFER", 16);
        myLabel.setFillColor(sf::Color(100, 220, 100));
        myLabel.setPosition({ tR.position.x + 20.f, tR.position.y + 54.f });
        window.draw(myLabel);

        for (int i = 0; i < 4; i++) {
            sf::Vector2f sp = { tR.position.x + 20.f + i * 74.f, tR.position.y + 80.f };
            sf::RectangleShape slot({ 64.f, 64.f });
            slot.setPosition(sp);
            slot.setFillColor(myP2PSlots[i].tex ? sf::Color(40, 70, 40) : sf::Color(45, 45, 55));
            slot.setOutlineThickness(2.f);
            slot.setOutlineColor(myP2PSlots[i].tex ? sf::Color(80, 220, 80) : sf::Color(70, 70, 90));
            window.draw(slot);
            if (myP2PSlots[i].tex) {
                sf::Sprite s(*myP2PSlots[i].tex);
                sf::Vector2u sz = s.getTexture().getSize();
                float sc = 48.f / std::max(sz.x, sz.y);
                s.setScale({ sc, sc });
                s.setPosition({ sp.x + 8.f, sp.y + 8.f });
                window.draw(s);
                sf::Text ct(uiFont, "x" + std::to_string(myP2PSlots[i].count), 13);
                ct.setFillColor(sf::Color::Yellow);
                ct.setPosition({ sp.x + 2.f, sp.y + 48.f });
                window.draw(ct);
                sf::Text nm(uiFont, getItemName(myP2PSlots[i].id), 11);
                nm.setFillColor(sf::Color(200, 200, 200));
                nm.setPosition({ sp.x, sp.y + 66.f });
                window.draw(nm);
            } else {
                sf::Text em(uiFont, "+", 26);
                em.setFillColor(sf::Color(60, 60, 80));
                em.setPosition({ sp.x + 22.f, sp.y + 16.f });
                window.draw(em);
            }
        }

        // My Gold offer
        sf::Text glbl(uiFont, "Gold offer:", 15);
        glbl.setFillColor(sf::Color(200, 200, 200));
        glbl.setPosition({ tR.position.x + 20.f, tR.position.y + 150.f });
        window.draw(glbl);
        sf::RectangleShape gbox({ 100.f, 34.f });
        gbox.setPosition({ tR.position.x + 20.f, tR.position.y + 168.f });
        gbox.setFillColor(sf::Color(30, 30, 30));
        gbox.setOutlineThickness(2.f);
        gbox.setOutlineColor(p2pGoldTyping ? sf::Color::Yellow : sf::Color(80, 120, 180));
        window.draw(gbox);
        sf::Text gtxt(uiFont, myP2PGoldInput.empty() ? "0" : myP2PGoldInput, 17);
        gtxt.setFillColor(sf::Color(255, 210, 50));
        gtxt.setPosition({ tR.position.x + 26.f, tR.position.y + 172.f });
        window.draw(gtxt);

        // My ready status
        sf::Text myReadyTxt(uiFont, p2pMyReady ? "YOU: READY" : "YOU: NOT READY", 14);
        myReadyTxt.setFillColor(p2pMyReady ? sf::Color(80, 220, 80) : sf::Color(180, 80, 80));
        myReadyTxt.setPosition({ tR.position.x + 20.f, tR.position.y + 215.f });
        window.draw(myReadyTxt);

        // ---- RIGHT: PARTNER'S OFFER ----
        float rx = tR.position.x + tW / 2.f + 20.f;
        sf::Text pLabel(uiFont, p2pPartner + "'s OFFER", 16);
        pLabel.setFillColor(sf::Color(100, 180, 255));
        pLabel.setPosition({ rx, tR.position.y + 54.f });
        window.draw(pLabel);

        for (int i = 0; i < (int)partnerItems.size() && i < 4; i++) {
            sf::Vector2f sp = { rx + i * 74.f, tR.position.y + 80.f };
            const sf::Texture* tex = getItemTexture(partnerItems[i].id);
            sf::RectangleShape slot({ 64.f, 64.f });
            slot.setPosition(sp);
            slot.setFillColor(tex ? sf::Color(40, 50, 80) : sf::Color(45, 45, 55));
            slot.setOutlineThickness(2.f);
            slot.setOutlineColor(sf::Color(80, 130, 220));
            window.draw(slot);
            if (tex) {
                sf::Sprite s(*tex);
                sf::Vector2u sz = s.getTexture().getSize();
                float sc = 48.f / std::max(sz.x, sz.y);
                s.setScale({ sc, sc });
                s.setPosition({ sp.x + 8.f, sp.y + 8.f });
                window.draw(s);
                sf::Text ct(uiFont, "x" + std::to_string(partnerItems[i].count), 13);
                ct.setFillColor(sf::Color::Yellow);
                ct.setPosition({ sp.x + 2.f, sp.y + 48.f });
                window.draw(ct);
                sf::Text nm(uiFont, getItemName(partnerItems[i].id), 11);
                nm.setFillColor(sf::Color(200, 200, 200));
                nm.setPosition({ sp.x, sp.y + 66.f });
                window.draw(nm);
            }
        }
        if (partnerItems.empty()) {
            sf::Text wait(uiFont, "(waiting for offer...)", 14);
            wait.setFillColor(sf::Color(120, 120, 140));
            wait.setPosition({ rx, tR.position.y + 100.f });
            window.draw(wait);
        }

        // Partner gold
        sf::Text pgld(uiFont, "Gold: " + std::to_string(partnerGold), 15);
        pgld.setFillColor(sf::Color(255, 210, 50));
        pgld.setPosition({ rx, tR.position.y + 168.f });
        window.draw(pgld);

        // Partner ready
        sf::Text prTxt(uiFont, p2pPartner + (p2pPartnerReady ? ": READY" : ": NOT READY"), 14);
        prTxt.setFillColor(p2pPartnerReady ? sf::Color(80, 220, 80) : sf::Color(180, 80, 80));
        prTxt.setPosition({ rx, tR.position.y + 215.f });
        window.draw(prTxt);

        // Status message
        if (!p2pStatusMsg.empty()) {
            sf::Text sm(uiFont, p2pStatusMsg, 14);
            sm.setFillColor(sf::Color(200, 200, 100));
            sm.setPosition({ tR.position.x + 20.f, tR.position.y + 340.f });
            window.draw(sm);
        }

        // Bottom buttons
        float bY = tR.position.y + 380.f;
        // Ready / Unready
        sf::RectangleShape readyBtnR({ 120.f, 40.f });
        readyBtnR.setPosition({ tR.position.x + 20.f, bY });
        readyBtnR.setFillColor(p2pMyReady ? sf::Color(70, 70, 70) : sf::Color(40, 160, 70));
        window.draw(readyBtnR);
        sf::Text rdTxt(uiFont, p2pMyReady ? "Unready" : "Ready!", 16);
        rdTxt.setPosition({ tR.position.x + 34.f, bY + 10.f });
        window.draw(rdTxt);

        // Cancel
        sf::RectangleShape cancelBtnR({ 120.f, 40.f });
        cancelBtnR.setPosition({ tR.position.x + tW - 130.f, bY });
        cancelBtnR.setFillColor(sf::Color(180, 50, 50));
        window.draw(cancelBtnR);
        sf::Text cTxt(uiFont, "Cancel", 16);
        cTxt.setPosition({ tR.position.x + tW - 110.f, bY + 10.f });
        window.draw(cTxt);

        // Both ready indicator
        if (p2pMyReady && p2pPartnerReady) {
            sf::Text both(uiFont, "Both ready! Trade executing...", 18);
            both.setStyle(sf::Text::Bold);
            both.setFillColor(sf::Color(80, 255, 80));
            both.setPosition({ tR.position.x + 20.f, tR.position.y + 308.f });
            window.draw(both);
        }
    }

    if (craftMenuOpen) {
        sf::RectangleShape craftBox({ 1300.f, 950.f });
        craftBox.setPosition({ (width - 1300.f) / 2.f, (height - 950.f) / 2.f });
        craftBox.setFillColor(sf::Color(30, 30, 35, 250));
        craftBox.setOutlineThickness(6.f);
        craftBox.setOutlineColor(sf::Color(80, 80, 90));
        window.draw(craftBox);

        // Header bar
        sf::RectangleShape header({ 1300.f, 80.f });
        header.setPosition(craftBox.getPosition());
        header.setFillColor(sf::Color(50, 50, 60));
        window.draw(header);

        sf::Text title(uiFont, "CRAFTING STATION", 48);
        title.setStyle(sf::Text::Bold);
        title.setFillColor(sf::Color(255, 220, 100));
        title.setPosition({ craftBox.getPosition().x + 40.f, craftBox.getPosition().y + 15.f });
        window.draw(title);

        sf::RectangleShape closeBtn({ 60.f, 60.f });
        closeBtn.setPosition({ craftBox.getPosition().x + 1220.f, craftBox.getPosition().y + 10.f });
        closeBtn.setFillColor(sf::Color(200, 50, 50));
        window.draw(closeBtn);
        sf::Text closeTxt(uiFont, "X", 36);
        closeTxt.setPosition({ closeBtn.getPosition().x + 18.f, closeBtn.getPosition().y + 8.f });
        window.draw(closeTxt);

        int totalWood = 0, totalStone = 0, totalWool = 0;
        for (int i = 0; i < 20; i++) {
            if (inventory.hasItem(i)) {
                if (inventory.getItemTexture(i) == &woodTex) totalWood += inventory.getItemCount(i);
                if (inventory.getItemTexture(i) == &stoneTex) totalStone += inventory.getItemCount(i);
                if (inventory.getItemTexture(i) == &woolTex) totalWool += inventory.getItemCount(i);
            }
        }

        // Draw resource overview
        sf::Text resTxt(uiFont, "Resources: " + std::to_string(totalWood) + " Wood, " + std::to_string(totalStone) + " Stone, " + std::to_string(totalWool) + " Wool", 24);
        resTxt.setFillColor(sf::Color(180, 180, 180));
        resTxt.setPosition({ craftBox.getPosition().x + 500.f, craftBox.getPosition().y + 30.f });
        window.draw(resTxt);

        float startX = craftBox.getPosition().x + 50.f;
        float startY = craftBox.getPosition().y + 110.f;
        float itemW = 380.f, itemH = 190.f;
        float gapX = 30.f, gapY = 20.f;

        for (int i = 0; i < (int)craftableItems.size(); i++) {
            int row = i / 3;
            int col = i % 3;
            sf::Vector2f pos(startX + col * (itemW + gapX), startY + row * (itemH + gapY));

            sf::RectangleShape itemCard({ itemW, itemH });
            itemCard.setPosition(pos);
            itemCard.setFillColor(sf::Color(45, 45, 55));
            itemCard.setOutlineThickness(2.f);
            itemCard.setOutlineColor(sf::Color(70, 70, 80));
            window.draw(itemCard);

            const auto& item = craftableItems[i];
            sf::Sprite s(*item.tex);
            sf::Vector2u ts = s.getTexture().getSize();
            float scale = 140.f / std::max(ts.x, ts.y);
            s.setScale({ scale, scale });
            s.setOrigin({ ts.x / 2.f, ts.y / 2.f });
            s.setPosition({ pos.x + 80.f, pos.y + itemH / 2.f });
            window.draw(s);

            sf::Text name(uiFont, item.name, 26);
            name.setStyle(sf::Text::Bold);
            name.setFillColor(sf::Color::White);
            name.setPosition({ pos.x + 160.f, pos.y + 20.f });
            window.draw(name);

            std::string reqs = "Cost:";
            if (item.woodReq > 0) reqs += "\n- " + std::to_string(item.woodReq) + " Wood";
            if (item.stoneReq > 0) reqs += "\n- " + std::to_string(item.stoneReq) + " Stone";
            if (item.woolReq > 0) reqs += "\n- " + std::to_string(item.woolReq) + " Wool";
            if (item.woodReq == 0 && item.stoneReq == 0 && item.woolReq == 0) reqs += "\nFree";

            sf::Text req(uiFont, reqs, 20);
            req.setFillColor(sf::Color(200, 200, 200));
            req.setPosition({ pos.x + 170.f, pos.y + 60.f });
            window.draw(req);

            sf::RectangleShape btn({ 140.f, 45.f });
            btn.setPosition({ pos.x + 210.f, pos.y + 130.f });

            bool canCraft = (totalWood >= item.woodReq && totalStone >= item.stoneReq && totalWool >= item.woolReq);
            bool inProgress = false;
            if (item.id == ITEM_WOODEN_SWORD && swordCrafting) inProgress = true;
            if (item.id == ITEM_SHIELD && shieldCrafting) inProgress = true;

            if (inProgress) {
                btn.setFillColor(sf::Color(120, 100, 0));
                window.draw(btn);
                sf::Text waitTxt(uiFont, "Crafting...", 18);
                waitTxt.setPosition({ btn.getPosition().x + 25.f, btn.getPosition().y + 10.f });
                window.draw(waitTxt);
            }
            else {
                btn.setFillColor(canCraft ? sf::Color(40, 160, 70) : sf::Color(80, 80, 80));
                window.draw(btn);
                sf::Text cTxt(uiFont, canCraft ? "CRAFT" : "LOCKED", 20);
                cTxt.setStyle(sf::Text::Bold);
                cTxt.setPosition({ btn.getPosition().x + (canCraft ? 38.f : 30.f), btn.getPosition().y + 10.f });
                window.draw(cTxt);
            }
        }
    }

    if (placementMode) {
        float btnW = 220.f, btnH = 70.f, gap = 40.f;
        float tWidth = btnW * 3.f + gap * 2.f;
        float sX = (width - tWidth) / 2.f;
        float yP = height - 110.f;

        sf::RectangleShape placeBtn({ btnW, btnH });
        placeBtn.setPosition({ sX, yP });

        int totalWood = 0, totalStone = 0;
        for (int i = 0; i < 20; i++) {
            if (inventory.hasItem(i)) {
                if (inventory.getItemTexture(i) == &woodTex) totalWood += inventory.getItemCount(i);
                if (inventory.getItemTexture(i) == &stoneTex) totalStone += inventory.getItemCount(i);
            }
        }
        if (totalWood >= 10 && totalStone >= 10) placeBtn.setFillColor(sf::Color(60, 180, 75));
        else placeBtn.setFillColor(sf::Color(100, 100, 100));
        placeBtn.setOutlineThickness(3.f);
        placeBtn.setOutlineColor(sf::Color::White);

        window.draw(placeBtn);
        sf::Text placeTxt(font, "Place", 36);
        sf::FloatRect pBounds = placeTxt.getLocalBounds();
        placeTxt.setPosition({ sX + (btnW - pBounds.size.x) / 2.f, yP + (btnH - pBounds.size.y) / 2.f - 8.f });
        window.draw(placeTxt);

        // ROTATE BUTTON
        sf::RectangleShape rotateBtn({ btnW, btnH });
        rotateBtn.setPosition({ sX + btnW + gap, yP });
        rotateBtn.setFillColor(sf::Color(60, 140, 200));
        rotateBtn.setOutlineThickness(3.f);
        rotateBtn.setOutlineColor(sf::Color::White);
        window.draw(rotateBtn);
        sf::Text rotateTxt(font, "Rotate", 36);
        sf::FloatRect rBounds = rotateTxt.getLocalBounds();
        rotateTxt.setPosition({ rotateBtn.getPosition().x + (btnW - rBounds.size.x) / 2.f, yP + (btnH - rBounds.size.y) / 2.f - 8.f });
        window.draw(rotateTxt);

        sf::RectangleShape cancelBtn({ btnW, btnH });
        cancelBtn.setPosition({ sX + (btnW + gap) * 2.f, yP });
        cancelBtn.setFillColor(sf::Color(200, 60, 60));
        cancelBtn.setOutlineThickness(3.f);
        cancelBtn.setOutlineColor(sf::Color::White);
        window.draw(cancelBtn);
        sf::Text cancelTxt(font, "Cancel", 36);
        sf::FloatRect cBounds = cancelTxt.getLocalBounds();
        cancelTxt.setPosition({ cancelBtn.getPosition().x + (btnW - cBounds.size.x) / 2.f, yP + (btnH - cBounds.size.y) / 2.f - 8.f });
        window.draw(cancelTxt);
    }


    if (saplingPlacementMode)
    {
        const float btnW = 220.f, btnH = 70.f, gap = 40.f;
        const float tWidth = btnW * 3.f + gap * 2.f;
        const float startX = (width - tWidth) / 2.f;
        const float yPos = height - 110.f;

        // PLACE BUTTON
        sf::RectangleShape placeBtn({ btnW, btnH });
        placeBtn.setPosition({ startX, yPos });
        placeBtn.setFillColor(sf::Color(60, 180, 75));
        placeBtn.setOutlineThickness(3.f);
        placeBtn.setOutlineColor(sf::Color::White);
        window.draw(placeBtn);
        sf::Text placeTxt(uiFont, "Place", 36);
        placeTxt.setFillColor(sf::Color::White);
        sf::FloatRect pBounds = placeTxt.getLocalBounds();
        placeTxt.setPosition({ startX + (btnW - pBounds.size.x) / 2.f, yPos + (btnH - pBounds.size.y) / 2.f - 8.f });
        window.draw(placeTxt);

        // ROTATE BUTTON
        sf::RectangleShape rotateBtn({ btnW, btnH });
        rotateBtn.setPosition({ startX + btnW + gap, yPos });
        rotateBtn.setFillColor(sf::Color(60, 140, 200));
        rotateBtn.setOutlineThickness(3.f);
        rotateBtn.setOutlineColor(sf::Color::White);
        window.draw(rotateBtn);
        sf::Text rotateTxt(uiFont, "Rotate (" + std::to_string((int)saplingRotation) + ")", 28);
        rotateTxt.setFillColor(sf::Color::White);
        sf::FloatRect rBounds = rotateTxt.getLocalBounds();
        rotateTxt.setPosition({ startX + btnW + gap + (btnW - rBounds.size.x) / 2.f, yPos + (btnH - rBounds.size.y) / 2.f - 8.f });
        window.draw(rotateTxt);

        // CANCEL BUTTON
        sf::RectangleShape cancelBtn({ btnW, btnH });
        cancelBtn.setPosition({ startX + (btnW + gap) * 2.f, yPos });
        cancelBtn.setFillColor(sf::Color(200, 60, 60));
        cancelBtn.setOutlineThickness(3.f);
        cancelBtn.setOutlineColor(sf::Color::White);
        window.draw(cancelBtn);
        sf::Text cancelTxt(uiFont, "Cancel", 36);
        cancelTxt.setFillColor(sf::Color::White);
        sf::FloatRect cBounds = cancelTxt.getLocalBounds();
        cancelTxt.setPosition({ cancelBtn.getPosition().x + (btnW - cBounds.size.x) / 2.f, yPos + (btnH - cBounds.size.y) / 2.f - 8.f });
        window.draw(cancelTxt);
    }

    if (tradeMenuOpen) {
        const float tW = 640.f, tH = 420.f;
        sf::FloatRect tBoxRect({ (width - tW) / 2.f, (height - tH) / 2.f }, { tW, tH });
        sf::RectangleShape tBox({ tW, tH });
        tBox.setPosition(tBoxRect.position);
        tBox.setFillColor(sf::Color(22, 22, 30, 248));
        tBox.setOutlineThickness(2.f);
        tBox.setOutlineColor(sf::Color(100, 180, 255));
        window.draw(tBox);

        // Header
        sf::RectangleShape header({ tW, 48.f });
        header.setPosition(tBoxRect.position);
        header.setFillColor(sf::Color(40, 60, 100));
        window.draw(header);
        sf::Text tTitle(uiFont, "TRADE WITH NPC", 26);
        tTitle.setStyle(sf::Text::Bold);
        tTitle.setFillColor(sf::Color(200, 230, 255));
        tTitle.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 8.f });
        window.draw(tTitle);

        // ─── PLAYER ITEMS section ───────────────────────────────
        sf::Text secLabel(uiFont, "Your Items  (click slot to add/remove)", 16);
        secLabel.setFillColor(sf::Color(160, 200, 255));
        secLabel.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 54.f });
        window.draw(secLabel);

        for (int i = 0; i < 4; i++) {
            sf::Vector2f slotPos = { tBoxRect.position.x + 20.f + i * 74.f, tBoxRect.position.y + 70.f };
            sf::RectangleShape slot({ 64.f, 64.f });
            slot.setPosition(slotPos);
            slot.setFillColor(playerTradeSlots[i].tex ? sf::Color(50, 80, 50) : sf::Color(50, 50, 60));
            slot.setOutlineThickness(2.f);
            slot.setOutlineColor(playerTradeSlots[i].tex ? sf::Color(80, 220, 80) : sf::Color(80, 80, 100));
            window.draw(slot);

            if (playerTradeSlots[i].tex) {
                sf::Sprite s(*playerTradeSlots[i].tex);
                sf::Vector2u sz = s.getTexture().getSize();
                float sc = 48.f / std::max(sz.x, sz.y);
                s.setScale({ sc, sc });
                s.setPosition({ slotPos.x + 8.f, slotPos.y + 8.f });
                window.draw(s);
                sf::Text ct(uiFont, "x" + std::to_string(playerTradeSlots[i].count), 14);
                ct.setFillColor(sf::Color::Yellow);
                ct.setPosition({ slotPos.x + 2.f, slotPos.y + 46.f });
                window.draw(ct);
            } else {
                sf::Text empty(uiFont, "+", 28);
                empty.setFillColor(sf::Color(80, 80, 100));
                empty.setPosition({ slotPos.x + 22.f, slotPos.y + 14.f });
                window.draw(empty);
            }
        }

        // ─── NPC ITEMS FOR SALE (buy mode — shown when no player items) ───
        if (!isSellingToNPC) {
            sf::Text npcSellLabel(uiFont, "NPC Items For Sale  (click to select)", 16);
            npcSellLabel.setFillColor(sf::Color(255, 220, 100));
            npcSellLabel.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 155.f });
            window.draw(npcSellLabel);

            // Calculate NPC item prices and total selected cost
            int totalCost = 0;
            for (int i = 0; i < 4; i++) {
                if (npcTradeSlots[i].tex == nullptr) continue;
                sf::Vector2f slotPos = { tBoxRect.position.x + 20.f + i * 74.f, tBoxRect.position.y + 172.f };

                // Highlight if selected for purchase
                bool selected = npcTradeSlots[i].originalSlot == 1; // reuse originalSlot as selected flag
                sf::RectangleShape nSlot({ 64.f, 64.f });
                nSlot.setPosition(slotPos);
                nSlot.setFillColor(selected ? sf::Color(80, 60, 20) : sf::Color(60, 50, 30));
                nSlot.setOutlineThickness(2.f);
                nSlot.setOutlineColor(selected ? sf::Color(255, 200, 50) : sf::Color(140, 120, 60));
                window.draw(nSlot);

                sf::Sprite s(*npcTradeSlots[i].tex);
                sf::Vector2u sz = s.getTexture().getSize();
                float sc = 48.f / std::max(sz.x, sz.y);
                s.setScale({ sc, sc });
                s.setPosition({ slotPos.x + 8.f, slotPos.y + 8.f });
                window.draw(s);

                sf::Text ct(uiFont, "x" + std::to_string(npcTradeSlots[i].count), 14);
                ct.setFillColor(sf::Color::Yellow);
                ct.setPosition({ slotPos.x + 2.f, slotPos.y + 46.f });
                window.draw(ct);

                // Price label
                int itemPrice = getItemPrice(npcTradeSlots[i].id) * npcTradeSlots[i].count;
                // NPC sells at 120% market
                itemPrice = (int)(itemPrice * 1.2f);
                sf::Text priceTxt(uiFont, std::to_string(itemPrice) + "G", 12);
                priceTxt.setFillColor(sf::Color(255, 220, 80));
                priceTxt.setPosition({ slotPos.x + 2.f, slotPos.y + 64.f });
                window.draw(priceTxt);

                if (selected) totalCost += itemPrice;
            }

            // Total cost line
            sf::Text costTxt(uiFont, "Selected Total: " + std::to_string(totalCost) + " G  |  You have: " + std::to_string(currency) + " G", 15);
            costTxt.setFillColor(totalCost <= currency ? sf::Color(100, 220, 100) : sf::Color(220, 80, 80));
            costTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 252.f });
            window.draw(costTxt);
        }

        // ─── RIGHT PANEL: Gold fields ────────────────────────────
        float rx = tBoxRect.position.x + 360.f;
        float ry = tBoxRect.position.y + 60.f;

        // NPC Offer (read-only)
        sf::Text npcLabel(uiFont, "NPC Offer (Gold):", 16);
        npcLabel.setFillColor(sf::Color(200, 200, 200));
        npcLabel.setPosition({ rx, ry });
        window.draw(npcLabel);

        sf::RectangleShape nGoldBox({ 120.f, 34.f });
        nGoldBox.setPosition({ rx, ry + 22.f });
        nGoldBox.setFillColor(sf::Color(30, 30, 30));
        nGoldBox.setOutlineThickness(2.f);
        nGoldBox.setOutlineColor(sf::Color(120, 120, 120));
        window.draw(nGoldBox);
        sf::Text nGoldTxt(uiFont, npcGoldInput.empty() ? "0" : npcGoldInput, 18);
        nGoldTxt.setFillColor(sf::Color(100, 255, 100));
        nGoldTxt.setPosition({ rx + 8.f, ry + 26.f });
        window.draw(nGoldTxt);

        // Player Gold (editable — for buying mode)
        sf::Text pgLabel(uiFont, "Your Gold Offer:", 16);
        pgLabel.setFillColor(sf::Color(200, 200, 200));
        pgLabel.setPosition({ rx, ry + 68.f });
        window.draw(pgLabel);

        sf::RectangleShape pGoldBox({ 120.f, 34.f });
        pGoldBox.setPosition({ rx, ry + 90.f });
        pGoldBox.setFillColor(sf::Color(30, 30, 30));
        pGoldBox.setOutlineThickness(2.f);
        pGoldBox.setOutlineColor(playerGoldTyping ? sf::Color::Yellow : sf::Color(80, 120, 200));
        window.draw(pGoldBox);
        sf::Text pGoldTxt(uiFont, playerGoldInput.empty() ? "0" : playerGoldInput, 18);
        pGoldTxt.setFillColor(sf::Color::White);
        pGoldTxt.setPosition({ rx + 8.f, ry + 94.f });
        window.draw(pGoldTxt);

        // Stock Stats button
        sf::RectangleShape statsBtn({ 150.f, 36.f });
        statsBtn.setPosition({ rx, ry + 138.f });
        statsBtn.setFillColor(sf::Color(40, 80, 130));
        statsBtn.setOutlineThickness(1.f);
        statsBtn.setOutlineColor(sf::Color::White);
        window.draw(statsBtn);
        sf::Text statsTxt(uiFont, "Stock Stats", 16);
        statsTxt.setPosition({ rx + 20.f, ry + 147.f });
        window.draw(statsTxt);

        // ─── Status message ───────────────────────────────────────
        if (!bargainStatus.empty()) {
            sf::Text stTxt(uiFont, bargainStatus, 15);
            stTxt.setFillColor(bargainColor);
            stTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 310.f });
            window.draw(stTxt);
        } else if (isSellingToNPC && npcTradeGold > 0) {
            sf::Text stTxt(uiFont, "NPC will pay " + std::to_string(npcTradeGold) + " Gold for your items.", 15);
            stTxt.setFillColor(sf::Color(100, 220, 100));
            stTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 310.f });
            window.draw(stTxt);
        } else if (!isSellingToNPC) {
            // Calculate selected NPC items cost
            float priceMul = bargainAccepted ? 1.08f : 1.20f;
            int totalCost = 0;
            for (int i = 0; i < 4; i++) {
                if (npcTradeSlots[i].originalSlot == 1 && npcTradeSlots[i].tex)
                    totalCost += (int)(getItemPrice(npcTradeSlots[i].id) * npcTradeSlots[i].count * priceMul);
            }
            if (totalCost > 0) {
                std::string msg = "Cost: " + std::to_string(totalCost) + " G" + (bargainAccepted ? "  [10% OFF!]" : "");
                sf::Text stTxt(uiFont, msg, 15);
                stTxt.setFillColor(totalCost <= currency ? sf::Color(100, 220, 100) : sf::Color(220, 80, 80));
                stTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 310.f });
                window.draw(stTxt);
            } else {
                sf::Text stTxt(uiFont, "Click NPC items below to select for purchase.", 15);
                stTxt.setFillColor(sf::Color(180, 180, 100));
                stTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 310.f });
                window.draw(stTxt);
            }
        }

        // ─── Bottom buttons ───────────────────────────────────────
        float bY = tBoxRect.position.y + 355.f;

        // Bargain (sell: NPC raises offer; buy: try to get discount)
        sf::RectangleShape bargainBtn({ 100.f, 40.f });
        bargainBtn.setPosition({ tBoxRect.position.x + 20.f, bY });
        bool canBargain = false;
        if (isSellingToNPC) canBargain = npcTradeGold > 0 && !hasBargained && !bargainAccepted;
        else {
            for (int i = 0; i < 4; i++) if (npcTradeSlots[i].originalSlot == 1) canBargain = true;
            if (hasBargained || bargainAccepted) canBargain = false;
        }
        bargainBtn.setFillColor(canBargain ? sf::Color(200, 130, 30) : sf::Color(70, 70, 70));
        bargainBtn.setOutlineThickness(1.f);
        bargainBtn.setOutlineColor(sf::Color::White);
        window.draw(bargainBtn);
        sf::Text bargainTxt(uiFont, "Bargain", 16);
        bargainTxt.setPosition({ tBoxRect.position.x + 28.f, bY + 10.f });
        window.draw(bargainTxt);

        // Cancel
        sf::RectangleShape cancelBtn({ 100.f, 40.f });
        cancelBtn.setPosition({ tBoxRect.position.x + 390.f, bY });
        cancelBtn.setFillColor(sf::Color(180, 50, 50));
        cancelBtn.setOutlineThickness(1.f);
        cancelBtn.setOutlineColor(sf::Color::White);
        window.draw(cancelBtn);
        sf::Text cancelTxt(uiFont, "Cancel", 16);
        cancelTxt.setPosition({ tBoxRect.position.x + 406.f, bY + 10.f });
        window.draw(cancelTxt);

        // Accept
        bool canAccept = false;
        if (isSellingToNPC) canAccept = npcTradeGold > 0;
        else { for (int i = 0; i < 4; i++) if (npcTradeSlots[i].originalSlot == 1 && npcTradeSlots[i].tex) canAccept = true; }
        sf::RectangleShape acceptBtn({ 100.f, 40.f });
        acceptBtn.setPosition({ tBoxRect.position.x + 500.f, bY });
        acceptBtn.setFillColor(canAccept ? sf::Color(40, 160, 70) : sf::Color(70, 70, 70));
        acceptBtn.setOutlineThickness(1.f);
        acceptBtn.setOutlineColor(sf::Color::White);
        window.draw(acceptBtn);
        sf::Text acceptTxt(uiFont, "Accept", 16);
        acceptTxt.setPosition({ tBoxRect.position.x + 516.f, bY + 10.f });
        window.draw(acceptTxt);

        // Mode label
        sf::Text modeTxt(uiFont, isSellingToNPC ? "MODE: SELLING" : "MODE: BUYING", 14);
        modeTxt.setFillColor(isSellingToNPC ? sf::Color(100, 255, 100) : sf::Color(255, 200, 50));
        modeTxt.setPosition({ tBoxRect.position.x + 20.f, tBoxRect.position.y + 338.f });
        window.draw(modeTxt);
    }

    if (tradeConfirmOpen) {
        sf::RectangleShape cBox({ 300.f, 150.f });
        cBox.setPosition({ (width - 300.f) / 2.f, height / 2.f - 75.f });
        cBox.setFillColor(sf::Color(20, 20, 20, 240));
        cBox.setOutlineColor(sf::Color::White);
        cBox.setOutlineThickness(2.f);
        window.draw(cBox);

        sf::Text cTxt(uiFont, "Confirm Trade?", 20);
        cTxt.setPosition({ cBox.getPosition().x + 70.f, cBox.getPosition().y + 20.f });
        window.draw(cTxt);

        sf::RectangleShape yBtn({ 100.f, 40.f });
        yBtn.setPosition({ (width - 250.f) / 2.f, height / 2.f });
        yBtn.setFillColor(sf::Color::Green);
        window.draw(yBtn);

        sf::Text yTxt(uiFont, "Yes", 18);
        yTxt.setPosition({ yBtn.getPosition().x + 30.f, yBtn.getPosition().y + 10.f });
        window.draw(yTxt);

        sf::RectangleShape nBtn({ 100.f, 40.f });
        nBtn.setPosition({ (width + 50.f) / 2.f, height / 2.f });
        nBtn.setFillColor(sf::Color::Red);
        window.draw(nBtn);

        sf::Text nTxt(uiFont, "No", 18);
        nTxt.setPosition({ nBtn.getPosition().x + 35.f, nBtn.getPosition().y + 10.f });
        window.draw(nTxt);
    }

    if (stockPageOpen) {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

        // ── Panel background ──────────────────────────────────────────────
        sf::RectangleShape sBox({ 920.f, 620.f });
        sBox.setPosition({ (width - 920.f) / 2.f, (height - 620.f) / 2.f });
        sBox.setFillColor(sf::Color(18, 18, 24, 250));
        sBox.setOutlineThickness(2.f);
        sBox.setOutlineColor(sf::Color(80, 80, 110));
        window.draw(sBox);

        float bx = sBox.getPosition().x;
        float by = sBox.getPosition().y;

        // ── Title ─────────────────────────────────────────────────────────
        sf::Text sTitle(uiFont, "STOCK MARKET PREDICTIONS", 26);
        sTitle.setStyle(sf::Text::Bold);
        sTitle.setFillColor(sf::Color(255, 215, 0));
        sTitle.setPosition({ bx + 30.f, by + 18.f });
        window.draw(sTitle);

        // ── Close button ──────────────────────────────────────────────────
        sf::RectangleShape closeBtn({ 36.f, 36.f });
        closeBtn.setPosition({ bx + 864.f, by + 18.f });
        bool closeHov = closeBtn.getGlobalBounds().contains(mousePos);
        closeBtn.setFillColor(closeHov ? sf::Color(240, 70, 70) : sf::Color(190, 45, 45));
        closeBtn.setOutlineThickness(closeHov ? 2.f : 0.f);
        closeBtn.setOutlineColor(sf::Color::White);
        window.draw(closeBtn);
        sf::Text closeTxt(uiFont, "X", 20);
        closeTxt.setStyle(sf::Text::Bold);
        closeTxt.setPosition({ bx + 875.f, by + 21.f });
        window.draw(closeTxt);

        // ── Search bar ────────────────────────────────────────────────────
        sf::FloatRect searchBarRect({ bx + 30.f, by + 62.f }, { 700.f, 32.f });
        sf::RectangleShape searchBg({ 700.f, 32.f });
        searchBg.setPosition({ bx + 30.f, by + 62.f });
        searchBg.setFillColor(marketSearchActive ? sf::Color(35, 38, 52) : sf::Color(28, 30, 42));
        searchBg.setOutlineThickness(marketSearchActive ? 2.f : 1.f);
        searchBg.setOutlineColor(marketSearchActive ? sf::Color(100, 160, 255) : sf::Color(60, 62, 80));
        window.draw(searchBg);

        // Magnifier icon text
        sf::Text searchIcon(uiFont, "  ", 16);
        searchIcon.setFillColor(sf::Color(120, 120, 150));
        searchIcon.setPosition({ bx + 36.f, by + 69.f });
        window.draw(searchIcon);

        std::string displaySearch = marketSearchText;
        bool cursorVisible = (marketCursorBlink.getElapsedTime().asMilliseconds() % 1000) < 500;
        if (marketSearchActive && cursorVisible) displaySearch += "|";
        else if (displaySearch.empty() && !marketSearchActive) displaySearch = "Search items...";

        sf::Text searchTxt(uiFont, displaySearch, 15);
        searchTxt.setFillColor(marketSearchText.empty() && !marketSearchActive
            ? sf::Color(80, 82, 100)
            : sf::Color(220, 220, 240));
        searchTxt.setPosition({ bx + 52.f, by + 69.f });
        window.draw(searchTxt);

        // ── Build full display list (raw + crafted) ───────────────────────
        struct DisplayItem {
            std::string name;
            int currentPrice;
            std::string trend;
            int predictedPrice;
            std::vector<float> history;
            const sf::Texture* tex = nullptr;
            bool isCrafted = false;
            float volatility = 1.0f;
        };

        std::vector<DisplayItem> allItems;

        // Raw and crafted stock market items
        for (auto& sm : stockMarket) {
            DisplayItem di;
            di.name = sm.name;
            di.currentPrice = (int)sm.currentPrice;
            di.predictedPrice = (int)sm.predictedPrice;
            di.trend = sm.trend;
            di.history = sm.history;
            di.isCrafted = (sm.id == ITEM_WOODEN_SWORD || sm.id == ITEM_SHIELD || sm.id == ITEM_FURNACE ||
                            sm.id == ITEM_WOOD_TILE || sm.id == ITEM_WOOD_WALL || sm.id == ITEM_WOOD_DOOR ||
                            sm.id == ITEM_WOOD_ROOF || sm.id == ITEM_WOOD_STAIRS || sm.id == ITEM_BED);
            di.volatility = sm.volatility;
            if (sm.id == ITEM_WOOD)               di.tex = &woodTex;
            else if (sm.id == ITEM_STONE)          di.tex = &stoneTex;
            else if (sm.id == ITEM_SAPLING)        di.tex = &saplingTex;
            else if (sm.id == ITEM_CHICKEN_MEAT)   di.tex = ChickenMeat::getTexture();
            else if (sm.id == ITEM_PIG_MEAT)       di.tex = PigMeat::getTexture();
            else if (sm.id == ITEM_SHEEP_MEAT)     di.tex = SheepMeat::getTexture();
            else if (sm.id == ITEM_COOKED_CHICKEN) di.tex = &cookedChickenTex;
            else if (sm.id == ITEM_COOKED_PIG)     di.tex = &cookedPigTex;
            else if (sm.id == ITEM_COOKED_SHEEP)   di.tex = &cookedSheepTex;
            else if (sm.id == ITEM_WOOL)           di.tex = &woolTex;
            else if (sm.id == ITEM_ZOMBIE_FLESH)   di.tex = ZombieFlesh::getTexture();
            else if (sm.id == ITEM_WOODEN_SWORD)   di.tex = &woodenSwordTex;
            else if (sm.id == ITEM_SHIELD)         di.tex = &shieldTex;
            else if (sm.id == ITEM_FURNACE)        di.tex = &furnaceTex;
            else if (sm.id == ITEM_WOOD_TILE)      di.tex = &woodTileTex;
            else if (sm.id == ITEM_WOOD_WALL)      di.tex = &woodWallTex;
            else if (sm.id == ITEM_WOOD_DOOR)      di.tex = &woodDoorTex;
            else if (sm.id == ITEM_WOOD_ROOF)      di.tex = &woodRoofTex;
            else if (sm.id == ITEM_WOOD_STAIRS)    di.tex = &woodStairsTex;
            else if (sm.id == ITEM_BED)            di.tex = &bedTex;
            allItems.push_back(di);
        }

        // ── Apply search filter ───────────────────────────────────────────
        std::vector<DisplayItem*> filtered;
        for (auto& di : allItems) {
            if (marketSearchText.empty()) {
                filtered.push_back(&di);
            }
            else {
                std::string nameLower = di.name;
                std::string queryLower = marketSearchText;
                for (char& c : nameLower)  c = (char)std::tolower((unsigned char)c);
                for (char& c : queryLower) c = (char)std::tolower((unsigned char)c);
                if (nameLower.find(queryLower) != std::string::npos)
                    filtered.push_back(&di);
            }
        }

        // ── Scroll clamping ───────────────────────────────────────────────
        const int maxVisible = 6;
        const float rowHeight = 72.f;
        int totalRows = (int)filtered.size();
        int maxScroll = std::max(0, totalRows - maxVisible);
        if (marketScrollOffset > maxScroll) marketScrollOffset = maxScroll;
        if (marketScrollOffset < 0) marketScrollOffset = 0;

        // ── Table headers ─────────────────────────────────────────────────
        float headerY = by + 104.f;
        auto makeHeader = [&](const std::string& label, float x) {
            sf::Text h(uiFont, label, 14);
            h.setFillColor(sf::Color(140, 140, 170));
            h.setPosition({ x, headerY });
            window.draw(h);
            };
        makeHeader("Item", bx + 30.f);
        makeHeader("Current Price", bx + 210.f);
        makeHeader("Trend", bx + 330.f);
        makeHeader("History Chart", bx + 460.f);
        makeHeader("Forecast / Signal", bx + 650.f);

        sf::RectangleShape hDivider({ 840.f, 1.f });
        hDivider.setPosition({ bx + 30.f, headerY + 22.f });
        hDivider.setFillColor(sf::Color(55, 55, 75));
        window.draw(hDivider);

        // ── Row clip region ───────────────────────────────────────────────
        float listTop = headerY + 28.f;
        float listH = maxVisible * rowHeight;

        // ── Draw rows ─────────────────────────────────────────────────────
        for (int vi = 0; vi < maxVisible; vi++) {
            int idx = vi + marketScrollOffset;
            if (idx >= totalRows) break;
            DisplayItem* item = filtered[idx];
            float y = listTop + vi * rowHeight;

            // Alternating row background
            sf::RectangleShape rBg({ 838.f, rowHeight - 4.f });
            rBg.setPosition({ bx + 30.f, y });
            rBg.setFillColor(sf::Color(vi % 2 == 0 ? 38 : 28, vi % 2 == 0 ? 38 : 28, vi % 2 == 0 ? 52 : 40, 180));
            window.draw(rBg);

            // Item icon (raw items have textures; crafted items use a "?" placeholder block)
            if (item->tex) {
                sf::Sprite icon(*item->tex);
                icon.setPosition({ bx + 36.f, y + 10.f });
                sf::Vector2u sz = item->tex->getSize();
                icon.setScale({ 34.f / sz.x, 34.f / sz.y });
                window.draw(icon);
            }
            else {
                // Crafted item placeholder block
                sf::RectangleShape placeholder({ 34.f, 34.f });
                placeholder.setPosition({ bx + 36.f, y + 10.f });
                placeholder.setFillColor(sf::Color(60, 60, 80));
                placeholder.setOutlineThickness(1.f);
                placeholder.setOutlineColor(sf::Color(100, 100, 130));
                window.draw(placeholder);
                sf::Text qMark(uiFont, "?", 18);
                qMark.setPosition({ bx + 47.f, y + 13.f });
                qMark.setFillColor(sf::Color(160, 160, 200));
                window.draw(qMark);
            }

            // Item name
            sf::Text nameTxt(uiFont, item->name, 16);
            nameTxt.setPosition({ bx + 80.f, y + 14.f });
            nameTxt.setFillColor(item->isCrafted ? sf::Color(200, 200, 220) : sf::Color::White);
            window.draw(nameTxt);

            // Crafted label badge or Volatility badge
            if (item->isCrafted) {
                sf::RectangleShape badge({ 50.f, 14.f });
                badge.setPosition({ bx + 80.f, y + 36.f });
                badge.setFillColor(sf::Color(70, 50, 100));
                window.draw(badge);
                sf::Text badgeTxt(uiFont, "CRAFTED", 9);
                badgeTxt.setFillColor(sf::Color(200, 160, 255));
                badgeTxt.setPosition({ bx + 82.f, y + 38.f });
                window.draw(badgeTxt);
            }
            else {
                sf::RectangleShape badge({ 55.f, 14.f });
                badge.setPosition({ bx + 80.f, y + 36.f });
                if (item->volatility > 1.2f) {
                    badge.setFillColor(sf::Color(140, 40, 40));
                    window.draw(badge);
                    sf::Text badgeTxt(uiFont, "UNSTABLE", 8);
                    badgeTxt.setFillColor(sf::Color(255, 140, 140));
                    badgeTxt.setPosition({ bx + 82.f, y + 38.f });
                    window.draw(badgeTxt);
                }
                else {
                    badge.setFillColor(sf::Color(30, 85, 80));
                    window.draw(badge);
                    sf::Text badgeTxt(uiFont, "STABLE", 8);
                    badgeTxt.setFillColor(sf::Color(140, 255, 230));
                    badgeTxt.setPosition({ bx + 86.f, y + 38.f });
                    window.draw(badgeTxt);
                }
            }

            // Current price
            sf::Text priceTxt(uiFont, std::to_string(item->currentPrice) + " G", 17);
            priceTxt.setFillColor(sf::Color(255, 230, 80));
            priceTxt.setPosition({ bx + 210.f, y + 18.f });
            window.draw(priceTxt);

            // Trend indicator
            sf::Text trendTxt(uiFont, "", 15);
            if (item->trend == "UP") {
                trendTxt.setString("^ UP");
                trendTxt.setFillColor(sf::Color(80, 220, 80));
            }
            else if (item->trend == "DOWN") {
                trendTxt.setString("v DOWN");
                trendTxt.setFillColor(sf::Color(220, 80, 80));
            }
            else {
                trendTxt.setString("~ STABLE");
                trendTxt.setFillColor(sf::Color(150, 150, 170));
            }
            trendTxt.setPosition({ bx + 330.f, y + 18.f });
            window.draw(trendTxt);

            // Mini history chart
            if (!item->history.empty()) {
                float minH = item->history[0], maxH = item->history[0];
                for (float hp : item->history) {
                    if (hp < minH) minH = hp;
                    if (hp > maxH) maxH = hp;
                }
                float range = maxH - minH;
                if (range < 1.f) range = 1.f;

                float graphW = 160.f, graphH = 38.f;
                float graphX = bx + 460.f, graphY = y + 10.f;

                sf::RectangleShape graphBg({ graphW, graphH });
                graphBg.setPosition({ graphX, graphY });
                graphBg.setFillColor(sf::Color(16, 16, 22, 210));
                graphBg.setOutlineThickness(1.f);
                graphBg.setOutlineColor(sf::Color(50, 50, 68));
                window.draw(graphBg);

                std::vector<float> pts = item->history;
                pts.push_back((float)item->currentPrice);

                sf::Color lineCol = (pts.back() >= pts.front())
                    ? sf::Color(80, 220, 80) : sf::Color(220, 80, 80);

                if (pts.size() >= 2) {
                    float stepX = graphW / (float)(pts.size() - 1);
                    for (size_t p = 0; p + 1 < pts.size(); p++) {
                        float x1 = graphX + p * stepX;
                        float y1 = graphY + graphH - 4.f - ((pts[p] - minH) / range) * (graphH - 8.f);
                        float x2 = graphX + (p + 1) * stepX;
                        float y2 = graphY + graphH - 4.f - ((pts[p + 1] - minH) / range) * (graphH - 8.f);
                        sf::RectangleShape seg;
                        float dist = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
                        seg.setSize({ dist, 2.f });
                        seg.setOrigin({ 0.f, 1.f });
                        seg.setPosition({ x1, y1 });
                        float angle = std::atan2(y2 - y1, x2 - x1) * 180.f / 3.14159f;
                        seg.setRotation(sf::degrees(angle));
                        seg.setFillColor(lineCol);
                        window.draw(seg);
                    }
                    // End dot
                    sf::RectangleShape dot({ 5.f, 5.f });
                    float dotX = graphX + (pts.size() - 1) * stepX - 2.f;
                    float dotY = graphY + graphH - 4.f - ((pts.back() - minH) / range) * (graphH - 8.f) - 2.f;
                    dot.setPosition({ dotX, dotY });
                    dot.setFillColor(sf::Color::White);
                    window.draw(dot);
                }
            }

            // Forecast & signal
            sf::Text predTxt(uiFont, "Est: " + std::to_string(item->predictedPrice) + " G", 14);
            predTxt.setFillColor(sf::Color(200, 200, 220));
            predTxt.setPosition({ bx + 650.f, y + 8.f });
            window.draw(predTxt);

            sf::Text sigTxt(uiFont, "", 13);
            sigTxt.setStyle(sf::Text::Bold);
            if (item->trend == "UP") {
                sigTxt.setString("SIGNAL: BUY");
                sigTxt.setFillColor(sf::Color(80, 220, 80));
            }
            else if (item->trend == "DOWN") {
                sigTxt.setString("SIGNAL: SELL");
                sigTxt.setFillColor(sf::Color(220, 80, 80));
            }
            else {
                sigTxt.setString("SIGNAL: HOLD");
                sigTxt.setFillColor(sf::Color(150, 150, 170));
            }
            sigTxt.setPosition({ bx + 650.f, y + 28.f });
            window.draw(sigTxt);

            // Row bottom divider
            sf::RectangleShape rowDiv({ 838.f, 1.f });
            rowDiv.setPosition({ bx + 30.f, y + rowHeight - 4.f });
            rowDiv.setFillColor(sf::Color(45, 45, 62));
            window.draw(rowDiv);
        }

        // ── Scrollbar ─────────────────────────────────────────────────────
        if (totalRows > maxVisible) {
            float trackX = bx + 882.f;
            float trackY = listTop;
            float trackH = listH;

            sf::RectangleShape track({ 8.f, trackH });
            track.setPosition({ trackX, trackY });
            track.setFillColor(sf::Color(40, 40, 55));
            track.setOutlineThickness(1.f);
            track.setOutlineColor(sf::Color(60, 60, 80));
            window.draw(track);

            float thumbRatio = (float)maxVisible / totalRows;
            float thumbH = std::max(20.f, trackH * thumbRatio);
            float thumbY = trackY + (trackH - thumbH) * ((float)marketScrollOffset / maxScroll);

            sf::RectangleShape thumb({ 8.f, thumbH });
            thumb.setPosition({ trackX, thumbY });
            thumb.setFillColor(sf::Color(100, 120, 200));
            window.draw(thumb);
        }

        // ── Empty search result notice ────────────────────────────────────
        if (filtered.empty()) {
            sf::Text noResult(uiFont, "No items match \"" + marketSearchText + "\"", 18);
            noResult.setFillColor(sf::Color(130, 130, 160));
            noResult.setPosition({ bx + 280.f, listTop + listH / 2.f - 10.f });
            window.draw(noResult);
        }

        // ── Item count label ──────────────────────────────────────────────
        sf::Text countTxt(uiFont,
            std::to_string(filtered.size()) + " / " + std::to_string(allItems.size()) + " items", 13);
        countTxt.setFillColor(sf::Color(100, 100, 130));
        countTxt.setPosition({ bx + 740.f, by + 68.f });
        window.draw(countTxt);

        // ── Scroll hint ───────────────────────────────────────────────────
        if (totalRows > maxVisible) {
            sf::Text hint(uiFont, "Scroll with mouse wheel to see more items", 13);
            hint.setFillColor(sf::Color(80, 80, 110));
            hint.setPosition({ bx + 30.f, by + 595.f });
            window.draw(hint);
        }

        // ── Recent trades ticker ──────────────────────────────────────────
        sf::RectangleShape tickerBg({ 880.f, 28.f });
        tickerBg.setPosition({ bx + 20.f, by + 578.f });
        tickerBg.setFillColor(sf::Color(28, 28, 38, 220));
        tickerBg.setOutlineThickness(1.f);
        tickerBg.setOutlineColor(sf::Color(60, 60, 90));
        window.draw(tickerBg);

        if (dbStartupEnabled) {
            if (!recentTradesFetchInFlight && (cachedRecentTrades.empty() || recentTradesClock.getElapsedTime().asSeconds() > 2.0f)) {
                recentTradesFuture = std::async(std::launch::async, []() {
                    return AuthManager::getInstance().getRecentTrades(3);
                    });
                recentTradesFetchInFlight = true;
                recentTradesClock.restart();
            }
            if (recentTradesFetchInFlight && recentTradesFuture.valid()) {
                try {
                    if (recentTradesFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                        cachedRecentTrades = recentTradesFuture.get();
                        recentTradesFetchInFlight = false;
                    }
                }
                catch (...) {
                    recentTradesFetchInFlight = false;
                }
            }
        }

        std::string tickerText = "  LIVE:  ";
        if (cachedRecentTrades.empty()) {
            tickerText += "No transactions recorded yet.";
        }
        else {
            bool firstEntry = true;
            for (size_t ri = 0; ri < cachedRecentTrades.size(); ri++) {
                auto& r = cachedRecentTrades[ri];
                // Skip entries with unknown or empty item names
                if (r.itemName.empty() || r.itemName == "Unknown Item" || r.itemName == "Unknown") continue;
                std::string uName = r.username.length() > 10 ? r.username.substr(0, 8) + ".." : r.username;
                if (!firstEntry) tickerText += "   |   ";
                tickerText += "[" + uName + "] " + (r.actionType == "SELL" ? "Sold" : "Bought")
                    + " " + std::to_string(r.quantity) + "x " + r.itemName
                    + " (" + std::to_string(r.totalGold) + "G)";
                firstEntry = false;
            }
            if (firstEntry) tickerText += "No transactions recorded yet.";
        }
        sf::Text tickerTxt(uiFont, tickerText, 13);
        tickerTxt.setFillColor(sf::Color(255, 215, 0));
        tickerTxt.setPosition({ tickerBg.getPosition().x + 10.f, tickerBg.getPosition().y + 5.f });
        window.draw(tickerTxt);
    }

    if (iapPageOpen) {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
        sf::RectangleShape sBox({ 900.f, 600.f });
        sf::Vector2f sBoxPos = { (width - 900.f) / 2.f, (height - 600.f) / 2.f };
        sBox.setPosition(sBoxPos);
        sBox.setFillColor(sf::Color(15, 20, 30, 248));
        sBox.setOutlineThickness(3.f);
        sBox.setOutlineColor(sf::Color(255, 215, 0, 180)); // Golden glowing border
        window.draw(sBox);

        // Title
        sf::Text sTitle(uiFont, "IN-APP PURCHASE STORE", 28);
        sTitle.setStyle(sf::Text::Bold);
        sTitle.setFillColor(sf::Color(255, 215, 0)); // Gold
        sTitle.setPosition({ sBoxPos.x + 40.f, sBoxPos.y + 20.f });
        window.draw(sTitle);

        // Subtitle
        sf::Text sSub(uiFont, "Get gold coins instantly to upgrade gear, craft items, and build structures!", 15);
        sSub.setFillColor(sf::Color(180, 180, 200));
        sSub.setPosition({ sBoxPos.x + 40.f, sBoxPos.y + 55.f });
        window.draw(sSub);

        // Gold Balance Display
        sf::RectangleShape balBox({ 300.f, 40.f });
        balBox.setPosition({ sBoxPos.x + 40.f, sBoxPos.y + 90.f });
        balBox.setFillColor(sf::Color(40, 45, 55, 200));
        balBox.setOutlineThickness(1.f);
        balBox.setOutlineColor(sf::Color(255, 215, 0, 80));
        window.draw(balBox);

        sf::Text balTxt(uiFont, "Your Gold Balance: " + std::to_string(currency) + " G", 16);
        balTxt.setStyle(sf::Text::Bold);
        balTxt.setFillColor(sf::Color(255, 225, 100));
        balTxt.setPosition({ balBox.getPosition().x + 15.f, balBox.getPosition().y + 8.f });
        window.draw(balTxt);

        // Close button (X)
        sf::RectangleShape closeBtn({ 40.f, 40.f });
        closeBtn.setPosition({ sBoxPos.x + 840.f, sBoxPos.y + 20.f });

        bool closeHover = closeBtn.getGlobalBounds().contains(mousePos);
        closeBtn.setFillColor(closeHover ? sf::Color(240, 70, 70) : sf::Color(200, 50, 50));
        closeBtn.setOutlineThickness(closeHover ? 2.f : 0.f);
        closeBtn.setOutlineColor(sf::Color::White);
        window.draw(closeBtn);

        sf::Text closeTxt(uiFont, "X", 22);
        closeTxt.setStyle(sf::Text::Bold);
        closeTxt.setPosition({ closeBtn.getPosition().x + 12.f, closeBtn.getPosition().y + 5.f });
        window.draw(closeTxt);

        // Draw the 4 cards!
        float cardW = 190.f;
        float cardH = 360.f;
        float cardY = sBoxPos.y + 160.f;
        float cardGap = 20.f;
        float startX = sBoxPos.x + 45.f;

        struct CardData { std::string title; std::string desc; int gold; float price; sf::Color color; };
        std::vector<CardData> cards = {
            { "Handful of Gold", "Great starter boost!", 100, 0.99f, sf::Color(205, 127, 50) },      // Bronze
            { "Sack of Gold", "Popular upgrades choice!", 1000, 9.99f, sf::Color(192, 192, 192) },    // Silver
            { "Chest of Gold", "Ideal for core builders!", 5000, 39.99f, sf::Color(255, 215, 0) },   // Gold
            { "Vault of Gold", "Tycoon class! Best value!", 10000, 79.99f, sf::Color(0, 242, 254) }  // Cyan neon
        };

        for (int i = 0; i < 4; i++) {
            float cX = startX + i * (cardW + cardGap);
            sf::FloatRect cardRect({ cX, cardY }, { cardW, cardH });
            bool hovered = cardRect.contains(mousePos);

            // Card container
            sf::RectangleShape card({ cardW, cardH });
            card.setPosition({ cX, cardY });
            card.setFillColor(hovered ? sf::Color(30, 35, 50, 230) : sf::Color(20, 22, 30, 220));
            card.setOutlineThickness(hovered ? 3.f : 1.f);
            card.setOutlineColor(hovered ? cards[i].color : sf::Color(255, 255, 255, 30));
            window.draw(card);

            // Card Title
            sf::Text cTitle(uiFont, cards[i].title, 16);
            cTitle.setStyle(sf::Text::Bold);
            cTitle.setFillColor(cards[i].color);
            cTitle.setPosition({ cX + 15.f, cardY + 20.f });
            window.draw(cTitle);

            // Gold Coins Icon / Graphics Placeholder
            sf::Text cCoins(uiFont, "COINS", 38);
            cCoins.setString("G");
            cCoins.setStyle(sf::Text::Bold);
            cCoins.setFillColor(cards[i].color);
            cCoins.setPosition({ cX + (cardW - cCoins.getGlobalBounds().size.x) / 2.f, cardY + 60.f });
            window.draw(cCoins);

            // Gold Value Text
            sf::Text cGold(uiFont, std::to_string(cards[i].gold) + " GOLD", 20);
            cGold.setStyle(sf::Text::Bold);
            cGold.setFillColor(sf::Color::White);
            cGold.setPosition({ cX + (cardW - cGold.getGlobalBounds().size.x) / 2.f, cardY + 130.f });
            window.draw(cGold);

            // Subtext / Description
            sf::Text cDesc(uiFont, cards[i].desc, 12);
            cDesc.setFillColor(sf::Color(150, 150, 160));
            cDesc.setPosition({ cX + (cardW - cDesc.getGlobalBounds().size.x) / 2.f, cardY + 175.f });
            window.draw(cDesc);

            // Divider line inside card
            sf::RectangleShape cDiv({ cardW - 40.f, 1.f });
            cDiv.setPosition({ cX + 20.f, cardY + 215.f });
            cDiv.setFillColor(sf::Color(255, 255, 255, 30));
            window.draw(cDiv);

            // Price display
            std::string priceStr = std::to_string(cards[i].price);
            priceStr = priceStr.substr(0, priceStr.find(".") + 3);
            sf::Text cPrice(uiFont, "$" + priceStr, 24);
            cPrice.setStyle(sf::Text::Bold);
            cPrice.setFillColor(sf::Color(100, 250, 150));
            cPrice.setPosition({ cX + (cardW - cPrice.getGlobalBounds().size.x) / 2.f, cardY + 230.f });
            window.draw(cPrice);

            // "BUY NOW" Button
            sf::FloatRect btnRect({ cX + 15.f, cardY + cardH - 55.f }, { cardW - 30.f, 40.f });
            bool btnHovered = btnRect.contains(mousePos);

            sf::RectangleShape buyBtn({ cardW - 30.f, 40.f });
            buyBtn.setPosition({ cX + 15.f, cardY + cardH - 55.f });
            buyBtn.setFillColor(btnHovered ? sf::Color(40, 180, 120) : sf::Color(30, 130, 90));
            buyBtn.setOutlineThickness(btnHovered ? 1.f : 0.f);
            buyBtn.setOutlineColor(sf::Color::White);
            window.draw(buyBtn);

            sf::Text buyTxt(uiFont, "BUY NOW", 14);
            buyTxt.setStyle(sf::Text::Bold);
            buyTxt.setFillColor(sf::Color::White);
            buyTxt.setPosition({ buyBtn.getPosition().x + (buyBtn.getSize().x - buyTxt.getGlobalBounds().size.x) / 2.f, buyBtn.getPosition().y + 11.f });
            window.draw(buyTxt);
        }
    }

    if (iapVerifyingOpen) {
        sf::RectangleShape vBox({ 500.f, 320.f });
        sf::Vector2f vBoxPos = { (width - 500.f) / 2.f, (height - 320.f) / 2.f };
        vBox.setPosition(vBoxPos);
        vBox.setFillColor(sf::Color(20, 25, 35, 250)); // Deep slate backgroundTest-NetConnection mysql-308a256a-econovadb.h.aivencloud.com -Port 22311
        vBox.setOutlineThickness(3.f);
        vBox.setOutlineColor(sf::Color(0, 242, 254, 180)); // Cyan glowing border
        window.draw(vBox);

        // Title
        sf::Text vTitle(uiFont, "SECURE CHECKOUT", 22);
        vTitle.setStyle(sf::Text::Bold);
        vTitle.setFillColor(sf::Color(0, 242, 254)); // Cyan
        vTitle.setPosition({ vBoxPos.x + 30.f, vBoxPos.y + 25.f });
        window.draw(vTitle);

        // Instruction Label
        sf::Text vLabel(uiFont, "A secure checkout page has been opened in your browser.\nPlease complete your payment of:", 13);
        vLabel.setFillColor(sf::Color(200, 200, 210));
        vLabel.setPosition({ vBoxPos.x + 30.f, vBoxPos.y + 65.f });
        window.draw(vLabel);

        // Gold package details
        std::string priceStr = std::to_string(iapVerifyingPrice);
        priceStr = priceStr.substr(0, priceStr.find(".") + 3);
        sf::Text vDetails(uiFont, iapVerifyingTier + " (" + std::to_string(iapVerifyingGold) + " Gold) for $" + priceStr + " USD", 15);
        vDetails.setStyle(sf::Text::Bold);
        vDetails.setFillColor(sf::Color(255, 215, 0)); // Gold text
        vDetails.setPosition({ vBoxPos.x + 30.f, vBoxPos.y + 110.f });
        window.draw(vDetails);

        // Paste Code Instruction
        sf::Text vCodeLbl(uiFont, "Paste your verification code below to sync your Gold balance:", 12);
        vCodeLbl.setFillColor(sf::Color(170, 170, 180));
        vCodeLbl.setPosition({ vBoxPos.x + 30.f, vBoxPos.y + 145.f });
        window.draw(vCodeLbl);

        // Draw Manual Verification Input field
        iapCodeInput.draw(window);

        // Buttons verifying and canceling
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

        sf::FloatRect verifyBtnRect({ vBoxPos.x + 50.f, vBoxPos.y + 240.f }, { 190.f, 45.f });
        bool verifyHover = verifyBtnRect.contains(mousePos);
        sf::RectangleShape verifyBtn({ 190.f, 45.f });
        verifyBtn.setPosition({ vBoxPos.x + 50.f, vBoxPos.y + 240.f });
        verifyBtn.setFillColor(verifyHover ? sf::Color(0, 200, 254) : sf::Color(0, 150, 200));
        window.draw(verifyBtn);

        sf::Text verifyTxt(uiFont, "Verify & Sync", 15);
        verifyTxt.setStyle(sf::Text::Bold);
        verifyTxt.setFillColor(sf::Color::White);
        verifyTxt.setPosition({ verifyBtn.getPosition().x + (verifyBtn.getSize().x - verifyTxt.getGlobalBounds().size.x) / 2.f, verifyBtn.getPosition().y + 12.f });
        window.draw(verifyTxt);

        sf::FloatRect cancelBtnRect({ vBoxPos.x + 260.f, vBoxPos.y + 240.f }, { 190.f, 45.f });
        bool cancelHover = cancelBtnRect.contains(mousePos);
        sf::RectangleShape cancelBtn({ 190.f, 45.f });
        cancelBtn.setPosition({ vBoxPos.x + 260.f, vBoxPos.y + 240.f });
        cancelBtn.setFillColor(cancelHover ? sf::Color(200, 60, 60) : sf::Color(150, 40, 40));
        window.draw(cancelBtn);

        sf::Text cancelTxt(uiFont, "Cancel", 15);
        cancelTxt.setStyle(sf::Text::Bold);
        cancelTxt.setFillColor(sf::Color::White);
        cancelTxt.setPosition({ cancelBtn.getPosition().x + (cancelBtn.getSize().x - cancelTxt.getGlobalBounds().size.x) / 2.f, cancelBtn.getPosition().y + 12.f });
        window.draw(cancelTxt);
    }

    // ✅ HUD Market Stats Button
    sf::RectangleShape marketBtnTopRight({ 150.f, 40.f });
    marketBtnTopRight.setPosition({ (float)width - 300.f, 20.f });
    marketBtnTopRight.setFillColor(sf::Color(40, 110, 90, 200)); // Premium emerald green
    marketBtnTopRight.setOutlineThickness(2.f);
    marketBtnTopRight.setOutlineColor(sf::Color(255, 215, 0)); // Gold outline
    window.draw(marketBtnTopRight);

    sf::Text marketBtnTxtTopRight(uiFont, "Market Stats", 18);
    marketBtnTxtTopRight.setPosition({ (float)width - 280.f, 28.f });
    window.draw(marketBtnTxtTopRight);

    sf::RectangleShape settingsBtnTopRight({ 120.f, 40.f });
    settingsBtnTopRight.setPosition({ (float)width - 140.f, 20.f });
    settingsBtnTopRight.setFillColor(sf::Color(80, 80, 80, 200));
    settingsBtnTopRight.setOutlineThickness(2.f);
    settingsBtnTopRight.setOutlineColor(sf::Color::White);
    window.draw(settingsBtnTopRight);

    sf::Text settingsBtnTxtTopRight(uiFont, "Settings", 20);
    settingsBtnTxtTopRight.setPosition({ (float)width - 115.f, 28.f });
    window.draw(settingsBtnTxtTopRight);

    if (isDead)
    {
        window.setView(GetUIView(&window));

        float survivedSeconds = elapsedGameTime - timeOffset;
        int daysSurvived = static_cast<int>(survivedSeconds / totalCycleTime);

        sf::RectangleShape overlay({ (float)width, (float)height });
        overlay.setFillColor(sf::Color(0, 0, 0, 200));
        window.draw(overlay);

        sf::Text deathText(font, "YOU DIED", 120);
        deathText.setFillColor(sf::Color::Red);
        deathText.setPosition({
            (width - deathText.getGlobalBounds().size.x) / 2.f,
            height / 2.f - 150.f
            });

        sf::Text daysText(
            font,
            "You survived " + std::to_string(daysSurvived) + " days",
            48
        );
        daysText.setFillColor(sf::Color::White);
        daysText.setPosition({
            (width - daysText.getGlobalBounds().size.x) / 2.f,
            height / 2.f
            });

        sf::Text exitText(font, "Press ESC to quit", 32);
        exitText.setFillColor(sf::Color::Yellow);
        exitText.setPosition({
            (width - exitText.getGlobalBounds().size.x) / 2.f,
            height / 2.f + 100.f
            });

        window.draw(deathText);
        window.draw(daysText);
        window.draw(exitText);
    }

    sf::RectangleShape brightnessOverlay({ (float)width, (float)height });
    brightnessOverlay.setFillColor(sf::Color(0, 0, 0, 255 - (globalSettings.brightness * 2.55f)));
    window.setView(GetUIView(&window));
    window.draw(brightnessOverlay);

    if (selectionDone) {
        if (chatActive) {
            // Async chat fetch: trigger every 2s, only messages after login
            if (dbStartupEnabled) {
                if (!chatFetchInFlight && chatFetchClock.getElapsedTime().asSeconds() > 2.0f) {
                    int offset = loginMessageIdOffset;
                    chatFuture = std::async(std::launch::async, [offset]() {
                        return AuthManager::getInstance().fetchChatMessages(50, offset);
                        });
                    chatFetchInFlight = true;
                    chatFetchClock.restart();
                }
                // Consume result non-blockingly
                if (chatFetchInFlight && chatFuture.valid()) {
                    try {
                        if (chatFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                            chatMessages = chatFuture.get();
                            chatFetchInFlight = false;
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "[ERROR] Async chat failure: " << e.what() << "\n";
                        chatFetchInFlight = false;
                    }
                    catch (...) {
                        std::cerr << "[ERROR] Async chat unknown failure\n";
                        chatFetchInFlight = false;
                    }
                }
            }

            // Draw chat background
            sf::RectangleShape chatBg({ chatBoxWidth, chatBoxHeight });
            chatBg.setPosition({ chatBoxX, chatBoxY });
            chatBg.setFillColor(sf::Color(0, 0, 0, 150));
            chatBg.setOutlineThickness(2.f);
            chatBg.setOutlineColor(sf::Color::White);
            window.draw(chatBg);

            // Draw chat messages with scroll and clipping
            float msgMinY = chatBoxY + 10.f;
            float msgMaxY = chatBoxY + chatBoxHeight - 60.f;
            float chatY = msgMinY - chatScroll;

            for (auto it = chatMessages.rbegin(); it != chatMessages.rend(); ++it) {
                sf::Text msgText(font, it->first + ": " + it->second, 24);
                float msgHeight = msgText.getLocalBounds().size.y + 10.f;

                // Only draw if within bounds (simple clipping)
                if (chatY + msgHeight > msgMinY && chatY < msgMaxY) {
                    msgText.setFillColor(sf::Color::White);
                    msgText.setOutlineColor(sf::Color::Black);
                    msgText.setOutlineThickness(1.5f);
                    msgText.setPosition({ chatBoxX + 15.f, chatY });
                    window.draw(msgText);
                }
                chatY += msgHeight;
            }

            if (isChatting) {
                chatInput.draw(window);
            }
        }

        if (goldTexLoaded) {
            goldSprite.setPosition({ 10.f, 10.f });
            window.draw(goldSprite);
        }

        sf::Text curText(font, std::to_string(currency), 24);
        curText.setFillColor(sf::Color::Yellow);
        curText.setOutlineColor(sf::Color::Black);
        curText.setOutlineThickness(2.f);
        curText.setPosition({ 10.f + (goldTexLoaded ? goldSprite.getGlobalBounds().size.x + 5.f : 0.f), 10.f });
        window.draw(curText);
    }

    if (introActive) {
        window.setView(GetUIView(&window));
        window.clear(sf::Color::Black);

        if (introSprite.has_value()) {
            // Center the intro sprite
            sf::Vector2u ts = introSprite->getTexture().getSize();
            introSprite->setScale({ (float)width / ts.x, (float)height / ts.y });
            window.draw(*introSprite);

            // Draw Story Text at the bottom
            if (currentIntroFrame < introTexts.size()) {
                sf::RectangleShape textBg({ (float)width - 200.f, 150.f });
                textBg.setFillColor(sf::Color(0, 0, 0, 180));
                textBg.setOutlineColor(sf::Color::White);
                textBg.setOutlineThickness(2.f);
                textBg.setPosition({ 100.f, (float)height - 300.f });
                window.draw(textBg);

                sf::Text storyLine(uiFont, introTexts[currentIntroFrame], 40);
                storyLine.setFillColor(sf::Color::White);
                // Center text in box
                sf::FloatRect textBounds = storyLine.getLocalBounds();
                storyLine.setPosition({
                    textBg.getPosition().x + (textBg.getSize().x - textBounds.size.x) / 2.f,
                    textBg.getPosition().y + (textBg.getSize().y - textBounds.size.y) / 2.f - 10.f
                    });
                window.draw(storyLine);
            }
        }
        else {
            // Fallback text if no images
            sf::Text story(uiFont, "Long ago, the world was vast and full of life...\nBut a darkness arrived, and survivors scattered.\nYou are one of the few left who can rebuild.\n\nClick to start your journey.", 30);
            story.setPosition({ 100.f, height / 2.f - 100.f });
            window.draw(story);
        }

        sf::Text skipText(uiFont, "Click to Continue...", 30);
        skipText.setPosition({ width / 2.f - 100.f, height - 100.f });
        window.draw(skipText);
    }

    if (tutorialActive) {
        window.setView(GetUIView(&window));
        window.draw(tutorialOverlay);
        window.draw(tutorialBox);

        sf::Text title(uiFont, tutorialSteps[tutorialStep].title, 34);
        title.setFillColor(sf::Color::Cyan);
        title.setPosition({ tutorialBox.getPosition().x + 40.f, tutorialBox.getPosition().y + 30.f });
        window.draw(title);

        sf::Text desc(uiFont, tutorialSteps[tutorialStep].description, 24);
        desc.setPosition({ tutorialBox.getPosition().x + 40.f, tutorialBox.getPosition().y + 100.f });
        window.draw(desc);

        bool goalMet = true;
        std::string goalProgress = tutorialSteps[tutorialStep].goal;
        if (tutorialStep == 1) {
            goalMet = (tutMoveDist >= 500.f);
            goalProgress += " (" + std::to_string((int)std::min(500.f, tutMoveDist)) + "/500)";
        }
        else if (tutorialStep == 2) goalMet = tutSprinted;
        else if (tutorialStep == 3) goalMet = tutInvOpened;
        else if (tutorialStep == 4) {
            goalMet = (tutWoodGathered >= 1 && tutStoneGathered >= 1);
            goalProgress += " (Wood: " + std::to_string(std::min(1, tutWoodGathered)) + "/1, Stone: " + std::to_string(std::min(1, tutStoneGathered)) + "/1)";
        }
        else if (tutorialStep == 5) goalMet = tutAttacked;
        else if (tutorialStep == 6) goalMet = tutChatted;

        sf::Text goal(uiFont, "Goal: " + goalProgress, 20);
        goal.setFillColor(goalMet ? sf::Color::Green : sf::Color::Yellow);
        goal.setPosition({ tutorialBox.getPosition().x + 40.f, tutorialBox.getPosition().y + 280.f });
        window.draw(goal);

        sf::RectangleShape nextBtn({ 120.f, 40.f });
        nextBtn.setPosition({ tutorialBox.getPosition().x + 650.f, tutorialBox.getPosition().y + 330.f });
        nextBtn.setFillColor(goalMet ? sf::Color(0, 150, 0) : sf::Color(100, 100, 100));
        window.draw(nextBtn);

        sf::Text nextTxt(uiFont, (tutorialStep == tutorialSteps.size() - 1) ? "Finish" : "Next", 20);
        nextTxt.setPosition({ nextBtn.getPosition().x + 35.f, nextBtn.getPosition().y + 8.f });
        window.draw(nextTxt);
    }

    if (spawnOverlayActive && spawnOverlayTotal > 0) {
        window.setView(GetUIView(&window));
        float pct = 100.f * (float)spawnOverlayDone / (float)spawnOverlayTotal;
        pct = std::clamp(pct, 0.f, 100.f);

        sf::RectangleShape panel({ 520.f, 90.f });
        panel.setFillColor(sf::Color(0, 0, 0, 160));
        panel.setPosition({ 30.f, 30.f });
        window.draw(panel);

        sf::Text t(
            uiFont,
            "Generating world... " + std::to_string((int)pct) + "% (" + std::to_string(spawnOverlayDone) + "/" + std::to_string(spawnOverlayTotal) + ")",
            22
        );
        t.setFillColor(sf::Color::White);
        t.setPosition({ 45.f, 45.f });
        window.draw(t);
    }

    for (auto& fire : campfires) {
        if (fire.menuOpen) {
            window.setView(GetUIView(&window));
            
            sf::RectangleShape bg({ 500.f, 300.f });
            bg.setFillColor(sf::Color(40, 40, 40, 240));
            bg.setOutlineThickness(3.f);
            bg.setOutlineColor(sf::Color::White);
            bg.setPosition({
                (float)width / 2.f - 250.f,
                (float)height / 2.f - 150.f
            });
            window.draw(bg);

            sf::Text title(uiFont, "Campfire Cooking", 28);
            title.setFillColor(sf::Color::Yellow);
            title.setPosition({
                bg.getPosition().x + 20.f,
                bg.getPosition().y + 10.f
            });
            window.draw(title);

            if (fire.inputCount > 0 || fire.outputCount > 0 || fire.fuel > 0) {
                sf::Text prompt(uiFont, "Continue cooking progress saved!", 12);
                prompt.setFillColor(sf::Color(0, 242, 254));
                prompt.setPosition({
                    bg.getPosition().x + 20.f,
                    bg.getPosition().y + 45.f
                });
                window.draw(prompt);
            }

            // Close button (X) in the top right corner
            sf::RectangleShape closeBtn({ 40.f, 40.f });
            closeBtn.setFillColor(sf::Color(200, 50, 50));
            closeBtn.setPosition({ bg.getPosition().x + 440.f, bg.getPosition().y + 10.f });
            window.draw(closeBtn);
            sf::Text closeTxt(uiFont, "X", 24);
            closeTxt.setFillColor(sf::Color::White);
            sf::FloatRect ctB = closeTxt.getLocalBounds();
            closeTxt.setPosition({ closeBtn.getPosition().x + (40.f - ctB.size.x)/2.f - 1.f, closeBtn.getPosition().y + (40.f - ctB.size.y)/2.f - 4.f });
            window.draw(closeTxt);

            // Slots labels
            sf::Text fuelLabel(uiFont, "Fuel", 18);
            fuelLabel.setFillColor(sf::Color::White);
            fuelLabel.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 80.f });
            window.draw(fuelLabel);

            sf::Text foodLabel(uiFont, "Raw Food", 18);
            foodLabel.setFillColor(sf::Color::White);
            foodLabel.setPosition({ bg.getPosition().x + 210.f, bg.getPosition().y + 80.f });
            window.draw(foodLabel);

            sf::Text outputLabel(uiFont, "Cooked Food", 18);
            outputLabel.setFillColor(sf::Color::White);
            outputLabel.setPosition({ bg.getPosition().x + 360.f, bg.getPosition().y + 80.f });
            window.draw(outputLabel);

            sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

            // 1. Fuel slot
            sf::RectangleShape fuelSlot({ 80.f, 80.f });
            fuelSlot.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 110.f });
            bool hoverFuel = fuelSlot.getGlobalBounds().contains(mpos);
            fuelSlot.setFillColor(sf::Color(30, 30, 30));
            fuelSlot.setOutlineThickness(2.f);
            fuelSlot.setOutlineColor(hoverFuel ? sf::Color::Yellow : sf::Color(150, 150, 150));
            window.draw(fuelSlot);

            if (fire.fuel > 0) {
                sf::Sprite fs(woodTex);
                sf::Vector2u ts = woodTex.getSize();
                float scale = 64.f / std::max(ts.x, ts.y);
                fs.setScale({ scale, scale });
                fs.setPosition({ fuelSlot.getPosition().x + 8.f, fuelSlot.getPosition().y + 8.f });
                window.draw(fs);

                sf::Text fsText(uiFont, "x" + std::to_string(fire.fuel), 16);
                fsText.setFillColor(sf::Color::White);
                fsText.setPosition({ fuelSlot.getPosition().x + 50.f, fuelSlot.getPosition().y + 55.f });
                window.draw(fsText);
            }

            // 2. Food Input slot
            sf::RectangleShape foodSlot({ 80.f, 80.f });
            foodSlot.setPosition({ bg.getPosition().x + 210.f, bg.getPosition().y + 110.f });
            bool hoverFood = foodSlot.getGlobalBounds().contains(mpos);
            foodSlot.setFillColor(sf::Color(30, 30, 30));
            foodSlot.setOutlineThickness(2.f);
            foodSlot.setOutlineColor(hoverFood ? sf::Color::Yellow : sf::Color(150, 150, 150));
            window.draw(foodSlot);

            if (fire.inputCount > 0) {
                const sf::Texture* rTex = (fire.inputItem == ITEM_CHICKEN_MEAT) ? ChickenMeat::getTexture() :
                                          (fire.inputItem == ITEM_PIG_MEAT) ? PigMeat::getTexture() :
                                          (fire.inputItem == ITEM_SHEEP_MEAT) ? SheepMeat::getTexture() : nullptr;
                if (rTex) {
                    sf::Sprite fs(*rTex);
                    sf::Vector2u ts = rTex->getSize();
                    float scale = 64.f / std::max(ts.x, ts.y);
                    fs.setScale({ scale, scale });
                    fs.setPosition({ foodSlot.getPosition().x + 8.f, foodSlot.getPosition().y + 8.f });
                    window.draw(fs);
                }

                sf::Text fsText(uiFont, "x" + std::to_string(fire.inputCount), 16);
                fsText.setFillColor(sf::Color::White);
                fsText.setPosition({ foodSlot.getPosition().x + 50.f, foodSlot.getPosition().y + 55.f });
                window.draw(fsText);
            }

            // 3. Cooked Food slot
            sf::RectangleShape outSlot({ 80.f, 80.f });
            outSlot.setPosition({ bg.getPosition().x + 360.f, bg.getPosition().y + 110.f });
            bool hoverOut = outSlot.getGlobalBounds().contains(mpos);
            outSlot.setFillColor(sf::Color(30, 30, 30));
            outSlot.setOutlineThickness(2.f);
            outSlot.setOutlineColor(hoverOut ? sf::Color::Yellow : sf::Color(150, 150, 150));
            window.draw(outSlot);

            if (fire.outputCount > 0) {
                const sf::Texture* oTex = (fire.outputItem == ITEM_COOKED_CHICKEN) ? &cookedChickenTex :
                                          (fire.outputItem == ITEM_COOKED_PIG) ? &cookedPigTex :
                                          (fire.outputItem == ITEM_COOKED_SHEEP) ? &cookedSheepTex : nullptr;
                if (oTex) {
                    sf::Sprite fs(*oTex);
                    sf::Vector2u ts = oTex->getSize();
                    float scale = 64.f / std::max(ts.x, ts.y);
                    fs.setScale({ scale, scale });
                    fs.setPosition({ outSlot.getPosition().x + 8.f, outSlot.getPosition().y + 8.f });
                    window.draw(fs);
                }

                sf::Text fsText(uiFont, "x" + std::to_string(fire.outputCount), 16);
                fsText.setFillColor(sf::Color::White);
                fsText.setPosition({ outSlot.getPosition().x + 50.f, outSlot.getPosition().y + 55.f });
                window.draw(fsText);
            }

            // Status message and cook progress
            std::string statusStr = "Insert Fuel & Raw Food";
            sf::Color statusColor(150, 150, 150);
            if (fire.inputCount > 0 && fire.fuel > 0) {
                statusStr = "Cooking: " + std::to_string(static_cast<int>(fire.timeRemaining + 0.99f)) + "s";
                statusColor = sf::Color::Yellow;
            }
            else if (fire.outputCount > 0) {
                statusStr = "Cooked food ready! Click to retrieve.";
                statusColor = sf::Color::Green;
            }
            sf::Text statusTxt(uiFont, statusStr, 20);
            statusTxt.setFillColor(statusColor);
            statusTxt.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 215.f });
            window.draw(statusTxt);
        }
    }

    if (quantityPromptOpen) {
        sf::View currentView = window.getView();
        window.setView(GetUIView(&window));

        sf::RectangleShape qBox({ 400.f, 220.f });
        sf::Vector2f qBoxPos = { ((float)width - 400.f) / 2.f, ((float)height - 220.f) / 2.f };
        qBox.setPosition(qBoxPos);
        qBox.setFillColor(sf::Color(20, 25, 35, 250));
        qBox.setOutlineThickness(3.f);
        qBox.setOutlineColor(sf::Color(0, 242, 254, 180));
        window.draw(qBox);

        sf::Text qTitle(uiFont, "ENTER AMOUNT", 20);
        qTitle.setStyle(sf::Text::Bold);
        qTitle.setFillColor(sf::Color(0, 242, 254));
        qTitle.setPosition({ qBoxPos.x + 30.f, qBoxPos.y + 25.f });
        window.draw(qTitle);

        std::string labelText = "Enter quantity to put in trade";
        if (quantityPromptIsCampfire) {
            if (quantityPromptCampfireSlot == 1) {
                labelText = "Enter quantity of fuel to place";
            } else {
                labelText = "Enter quantity of food to place";
            }
        }
        sf::Text qLabel(uiFont, labelText + " (Max: " + std::to_string(quantityPromptMax) + "):", 12);
        qLabel.setFillColor(sf::Color(200, 200, 210));
        qLabel.setPosition({ qBoxPos.x + 30.f, qBoxPos.y + 65.f });
        window.draw(qLabel);

        quantityInput.setPosition({ qBoxPos.x + 30.f, qBoxPos.y + 95.f });
        quantityInput.draw(window);

        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

        sf::FloatRect confirmBtnRect({ qBoxPos.x + 30.f, qBoxPos.y + 150.f }, { 160.f, 40.f });
        bool confirmHover = confirmBtnRect.contains(mousePos);
        sf::RectangleShape confirmBtn({ 160.f, 40.f });
        confirmBtn.setPosition({ qBoxPos.x + 30.f, qBoxPos.y + 150.f });
        confirmBtn.setFillColor(confirmHover ? sf::Color(0, 200, 254) : sf::Color(0, 150, 200));
        window.draw(confirmBtn);

        sf::Text confirmTxt(uiFont, "Confirm", 15);
        confirmTxt.setStyle(sf::Text::Bold);
        confirmTxt.setFillColor(sf::Color::White);
        confirmTxt.setPosition({ confirmBtn.getPosition().x + (confirmBtn.getSize().x - confirmTxt.getGlobalBounds().size.x) / 2.f, confirmBtn.getPosition().y + 10.f });
        window.draw(confirmTxt);

        sf::FloatRect cancelBtnRect({ qBoxPos.x + 210.f, qBoxPos.y + 150.f }, { 160.f, 40.f });
        bool cancelHover = cancelBtnRect.contains(mousePos);
        sf::RectangleShape cancelBtn({ 160.f, 40.f });
        cancelBtn.setPosition({ qBoxPos.x + 210.f, qBoxPos.y + 150.f });
        cancelBtn.setFillColor(cancelHover ? sf::Color(200, 60, 60) : sf::Color(150, 40, 40));
        window.draw(cancelBtn);

        sf::Text cancelTxt(uiFont, "Cancel", 15);
        cancelTxt.setStyle(sf::Text::Bold);
        cancelTxt.setFillColor(sf::Color::White);
        cancelTxt.setPosition({ cancelBtn.getPosition().x + (cancelBtn.getSize().x - cancelTxt.getGlobalBounds().size.x) / 2.f, cancelBtn.getPosition().y + 10.f });
        window.draw(cancelTxt);

        window.setView(currentView);
    }

    if (isSleeping) {
        sf::View currentView = window.getView();
        window.setView(GetUIView(&window));
        float alpha = (sleepFadeTimer / sleepFadeDuration) * 255.f;
        if (alpha > 255.f) alpha = 255.f;
        if (alpha < 0.f) alpha = 0.f;
        sf::RectangleShape fadeRect(sf::Vector2f((float)width, (float)height));
        fadeRect.setFillColor(sf::Color(0, 0, 0, static_cast<int>(alpha)));
        fadeRect.setPosition({0.f, 0.f});
        window.draw(fadeRect);
        window.setView(currentView);
    }

    if ((debugFrameIndex % 300ull) == 0ull) traceStartup("hb_before_final_display");
    traceStartup("before_final_display");
    window.display();
    traceStartup("after_final_display");

    // Spawning moved into the main loop (incremental spawnPlan) to avoid long stalls.
    }
}
