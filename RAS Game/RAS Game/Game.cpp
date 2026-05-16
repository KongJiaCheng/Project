#define _CRT_SECURE_NO_WARNINGS
#include "Game.h"
#include "ViewUtils.h"
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
#include "AuthManager.h"
#include "Settings.h"
#include "TextInput.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <filesystem>
#include <iomanip>
#include <sstream>

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

void gameLoop(sf::RenderWindow& window, unsigned int width, unsigned int height, const std::string& username, bool isFirstTimeFlag)
{
    std::cout << "[DEBUG] Entering gameLoop. isFirstTime: " << isFirstTimeFlag << "\n";
    int option = 0;
    bool selectionDone = false;
    bool inventoryOpen = false;
    bool invKeyHeld = false;
    bool itemSpawned = false;
    bool musicLoaded = false;
    bool musicStarted = false;
    bool nearPond = false;
    bool isDead = false;
    bool wasNight = false;

    sf::Music nightMusic;
    bool nightMusicLoaded = false;
    bool nightMusicPlaying = false;

    std::vector<std::unique_ptr<Animal>> animals;
    std::vector<std::unique_ptr<Monster>> monsters;

    sf::RectangleShape darkness;
    darkness.setSize(sf::Vector2f((float)MAP_WIDTH, (float)MAP_HEIGHT));
    darkness.setFillColor(sf::Color(0, 0, 0, 0));

    Inventory inventory;
    AuthManager& authManager = AuthManager::getInstance();
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
    (void)font.openFromFile("C:/Windows/Fonts/arial.ttf");
    sf::Font& uiFont = font;

    sf::Texture selBgTexture;
    selBgTexture.loadFromFile("../Main Menu/Background.png");
    sf::Sprite selBgSprite(selBgTexture);
    if (selBgSprite.getLocalBounds().size.x > 0 && selBgSprite.getLocalBounds().size.y > 0) {
        selBgSprite.setScale({ width / selBgSprite.getLocalBounds().size.x, height / selBgSprite.getLocalBounds().size.y });
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
    bgTexture.loadFromFile("../Items/Background.jpg");

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

    int currency = 0;
    int level = 1;
    int exp = 0;

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

    sf::Texture texSouth;
    sf::Sprite sprite(texSouth);
    sprite.setOrigin({ 32.f, 32.f });
    sprite.setPosition({ MAP_WIDTH / 2.f, MAP_HEIGHT / 2.f });

    sf::Texture woodTex;
    sf::Texture treeTex;
    sf::Texture stoneTex;
    sf::Texture pondTex;
	sf::Texture saplingTex;

    sf::Texture hpIconTex, hungerIconTex, thirstIconTex;
    hpIconTex.loadFromFile("../Sprites/UI/hp.png");
    hungerIconTex.loadFromFile("../Sprites/UI/hunger.png");
    thirstIconTex.loadFromFile("../Sprites/UI/thirst.png");

    sf::Texture wrenchTex;
    wrenchTex.loadFromFile("../Sprites/UI/wrench.png");

    struct NPC {
        sf::Sprite sprite;
        std::string name;
        sf::Texture texture;
        sf::Sprite wrenchBtn;
        NPC(const sf::Texture& tex) : sprite(tex), wrenchBtn(tex) {}
    } npcFemale(wrenchTex), npcMale(wrenchTex);
    npcFemale.name = "Female NPC";
    npcMale.name = "Male NPC";

    bool wrenchMenuOpen = false;
    sf::Vector2f wrenchMenuPos;
    bool tradeMenuOpen = false;
    bool tradeBoxesTicked = false;
    bool tradeConfirmOpen = false;

    bool craftMenuOpen = false;
    bool placementMode = false;
    bool placementFollowingCursor = true;

    sf::Vector2f placementPos;

    bool saplingPlacementMode = false;
    bool saplingFollowingCursor = true;
    sf::Vector2f saplingPlacementPos;

    struct PlacedCampfire {
        sf::Vector2f position;
        float timeRemaining;
        bool isCrafted;
        int currentFrame;
        float frameTimer;
    };

    struct PlantedSapling {
        sf::Vector2f position;
        float growTimer;
        bool grown;
    };

    std::vector<PlantedSapling> plantedSaplings;
    std::vector<PlacedCampfire> campfires;
    sf::Texture campfireFrames[8];
    for (int i = 0; i < 8; i++) {
        campfireFrames[i].loadFromFile("../Items/campfire/frame_" + std::to_string(i) + "_campfire.png");
    }

    bool isTargetNPC = false;
    struct TradeSlot { const sf::Texture* tex = nullptr; int count = 0; };
    TradeSlot playerTradeSlots[4];
    TradeSlot npcTradeSlots[4];

    sf::Sprite playerWrenchBtn(wrenchTex);
    playerWrenchBtn.setScale({ 0.05f, 0.05f });
    npcFemale.wrenchBtn.setTexture(wrenchTex);
    npcFemale.wrenchBtn.setScale({ 0.05f, 0.05f });
    npcMale.wrenchBtn.setTexture(wrenchTex);
    npcMale.wrenchBtn.setScale({ 0.05f, 0.05f });

    sf::Sprite hpIcon(hpIconTex);
    sf::Sprite hungerIcon(hungerIconTex);
    sf::Sprite thirstIcon(thirstIconTex);

    hpIcon.setScale({ 0.6f, 0.6f });
    hungerIcon.setScale({ 0.6f, 0.6f });
    thirstIcon.setScale({ 0.6f, 0.6f });

    sf::Text hpText(font, "", 16);
    sf::Text hungerText(font, "", 16);
    sf::Text thirstText(font, "", 16);
    sf::Text defText(font, "", 16);
    sf::Text levelText(font, "", 20);

    sf::Text drinkText(font, "Press E to drink water", 28);
    drinkText.setFillColor(sf::Color::Yellow);

    hpText.setFillColor(sf::Color::Black);
    hungerText.setFillColor(sf::Color::Black);
    thirstText.setFillColor(sf::Color::Black);
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
    Chicken::texture.loadFromFile("../Sprites/Animals/Chicken/South.png");
    Pig::texture.loadFromFile("../Sprites/Animals/Pig/South.png");
    Zombie::texture.loadFromFile("../Sprites/Monsters/Zombie/South.png");

    woodTex.loadFromFile("../Items/wood.png");
    treeTex.loadFromFile("../Items/Tree.png");
    stoneTex.loadFromFile("../Items/Stone.png");
    pondTex.loadFromFile("../Items/Pond.png");
	saplingTex.loadFromFile("../Items/Sapling.png");

    sf::Texture goldTex;
    bool goldTexLoaded = goldTex.loadFromFile("../Items/gold.png");
    sf::Sprite goldSprite(goldTex);
    goldSprite.setScale({ 0.5f, 0.5f }); // Scale down if it's too large

    sf::Texture woodenSwordTex;
    woodenSwordTex.loadFromFile("../Items/wooden sword.png");

    sf::Texture shieldTex;
    shieldTex.loadFromFile("../Items/shield.png");

    bool shieldCrafting = false;
    float shieldCraftTimer = 0.f;

    bool swordCrafting = false;
    float swordCraftTimer = 0.f;
    sf::Sprite playerSwordSprite(woodenSwordTex);
    playerSwordSprite.setScale({0.5f, 0.5f});
    sf::Sprite pond(pondTex);
    pond.setScale({ 10.0f, 10.0f }); 

    itemManager.setResourceTextures(&woodTex, &stoneTex);

    sf::Clock animalRespawnClock;
    const int MAX_ANIMALS = 200; // Increased for larger map

    sf::Clock deathClock;

    const float totalCycleTime = 900.f;

    sf::Clock deltaClock;
    sf::Clock dayNightClock;

    UpdateViewToWindow(&window);

    enum class Dir { South, North, East, West };
    Dir currentDir = Dir::South;

    const float MOVE_SPEED = 250.f;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> randX(0.f, (float)MAP_WIDTH);
    std::uniform_real_distribution<float> randY(0.f, (float)MAP_HEIGHT);

    if (animalRespawnClock.getElapsedTime().asSeconds() > 10.f &&
        animals.size() < MAX_ANIMALS)
    {
        animals.push_back(std::make_unique<Chicken>(
            sf::Vector2f(randX(gen), randY(gen))
        ));

        animalRespawnClock.restart();
    }

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
    if (bgMusic.openFromFile("../Music/Locally Sourced - Jason Farnham.mp3"))
    {
        bgMusic.setLooping(true);
        bgMusic.setVolume(globalSettings.volume); // 0–100
        musicLoaded = true;
    }
    else
    {
        std::cerr << "Failed to load background music\n";
    }

    float chatBoxWidth = 800.f;
    float chatBoxHeight = 400.f;
    float chatBoxX = (width - chatBoxWidth) / 2.f;
    float chatBoxY = 20.f;
    TextInput chatInput(font, { chatBoxX + 10.f, chatBoxY + chatBoxHeight - 50.f }, { chatBoxWidth - 20.f, 40.f });
    bool isChatting = false;
    std::vector<std::pair<std::string, std::string>> chatMessages;
    sf::Clock chatFetchClock;
    std::string latestPlayerMsg = "";
    float bubbleTimer = 0.f;
    float chatScroll = 0.f;
    bool isDraggingChat = false;
    sf::Vector2f lastMousePosChat;
    
    // Story Cinematic State
    bool introActive = isFirstTimeFlag;
    int currentIntroFrame = 0;
    std::vector<sf::Texture> introFrames;
    
    // Load entity textures
    Chicken::texture.loadFromFile("../Sprites/Animals/Chicken/Walk/South/frame_000.png");
    Pig::texture.loadFromFile("../Sprites/Animals/Pig/Walk/South/frame_000.png");
    Zombie::texture.loadFromFile("../Sprites/Monsters/Zombie/Walk/South/frame_000.png");
    ChickenMeat::loadTexture();
    PigMeat::loadTexture();
    ZombieFlesh::loadTexture();

    // We will initialize introSprite later to avoid default constructor issues in SFML 3.x
    std::optional<sf::Sprite> introSprite;
    
    if (introActive) {
        // Load story frames
        for (int i = 0; i < 100; i++) { // Increased limit to 100 frames
            std::ostringstream ss;
            ss << "../Story/frame_" << std::setfill('0') << std::setw(3) << i << ".jpg";
            std::string path = ss.str();
            
            if (std::filesystem::exists(path)) {
                sf::Texture tex;
                if (tex.loadFromFile(path)) {
                    introFrames.push_back(tex);
                } else {
                    break;
                }
            } else {
                break; // No more frames
            }
        }
        if (introFrames.empty()) {
            // Fallback story frames if files not found
            std::cout << "[INFO] No story frames found in ../Story/. Using fallback text intro.\n";
            introActive = true; 
        } else {
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
        "This is a world of RAS—Ruins, Ashes, and Survival.",
        "Your story begins here. Can you endure the coming storms?"
    };

    bool tutorialActive = false;
    int tutorialStep = 0;
    sf::RectangleShape tutorialOverlay({ (float)width, (float)height });
    tutorialOverlay.setFillColor(sf::Color(0, 0, 0, 180));

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
    
    QuestInfo qInfo = authManager.getQuestInfo(username.c_str());
    
    time_t tnow = time(0);
    struct tm tstruct;
#ifdef _WIN32
    localtime_s(&tstruct, &tnow);
#else
    tstruct = *localtime(&tnow);
#endif
    char tbuf[80];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d", &tstruct);
    std::string today(tbuf);
    
    if (qInfo.lastReset != today) {
        int newQuestID = (rand() % (availableQuests.size())) + 1;
        authManager.resetQuest(username.c_str(), newQuestID, today.c_str());
        qInfo.questID = newQuestID;
        qInfo.progress = 0;
        qInfo.lastReset = today;
    }

    while (window.isOpen())
    {
        float dt = deltaClock.restart().asSeconds();

        bubbleTimer = std::max(0.f, bubbleTimer - dt);

        if (!introActive && !tutorialActive)
        {
            // survival update
            hunger = std::min(maxHunger, hunger + HUNGER_RATE * dt);
            thirst = std::min(maxThirst, thirst + THIRST_RATE * dt);

            if (hunger >= maxHunger || thirst >= maxThirst)
            {
                hp = std::max(0.f, hp - DAMAGE_RATE * dt);
            }
            else if (hunger <= 0.01f)
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

        for (int i = 0; i < 8; i++) {
            if (inventory.isEquipped(i)) {
                if (inventory.getItemTexture(i) == &woodenSwordTex) {
                    swordEquipped = true;
                    eqSwordSlot = i;
                } else if (inventory.getItemTexture(i) == &shieldTex) {
                    shieldEquipped = true;
                    eqShieldSlot = i;
                } else {
                    inventory.setEquipped(i, false);
                }
            }
        }

        // ✅ AUTO SAVE EVERY 5 SECONDS
        static sf::Clock saveClock;

        if (saveClock.getElapsedTime().asSeconds() > 5.f)
        {
            std::vector<std::pair<int, int>> items(8, { 0,0 });

            for (int i = 0; i < 8; i++)
            {
                if (inventory.hasItem(i))
                {
                    const sf::Texture* tex = inventory.getItemTexture(i);
                    int count = inventory.getItemCount(i);

                    int itemId = 0;

                    if (tex == ChickenMeat::getTexture()) itemId = 1;
                    else if (tex == PigMeat::getTexture()) itemId = 2;
                    else if (tex == ZombieFlesh::getTexture()) itemId = 3;
                    else if (tex == &woodenSwordTex) itemId = 4;
                    else if (tex == &shieldTex) itemId = 6;

                    items[i] = { itemId, count };
                }
            }

            authManager.saveInventory(username.c_str(), items);
            authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp);

            std::cout << "[AUTO SAVE]\n";

            saveClock.restart();
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
            if (isChatting) {
                chatInput.processEvent(event.value(), window.mapPixelToCoords(sf::Mouse::getPosition(window), window.getDefaultView()));
            }

            if (auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Enter || key->scancode == globalSettings.keyChat) {
                    if (!isChatting) {
                        isChatting = true;
                        chatInput.setFocus(true);
                    } else if (key->scancode == sf::Keyboard::Scancode::Enter) {
                        std::string msg = chatInput.getText();
                        if (!msg.empty()) {
                            if (msg.find("/pull ") == 0) {
                                std::string target = msg.substr(6);
                                authManager.sendCommand(username.c_str(), target.c_str(), "pull");
                                latestPlayerMsg = "Command Sent: /pull " + target;
                                bubbleTimer = 3.f;
                            } else if (msg == "/help" || msg == "/?") {
                                latestPlayerMsg = "Commands:\n/pull (name) - Pull player\n/help - Show commands";
                                bubbleTimer = 8.f;
                            } else {
                                authManager.sendChatMessage(username.c_str(), msg.c_str());
                                if (tutorialActive && tutorialStep == 6) tutChatted = true;
                                latestPlayerMsg = msg;
                                bubbleTimer = 5.0f;
                            }
                            chatInput.clear();
                        }
                        isChatting = false;
                        chatInput.setFocus(false);
                    }
                } else if (key->scancode == sf::Keyboard::Scancode::Escape && isChatting) {
                    isChatting = false;
                    chatInput.setFocus(false);
                    chatInput.clear();
                }
            }

            if (auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (wheel->wheel == sf::Mouse::Wheel::Vertical) {
                    sf::Vector2f uiPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), window.getDefaultView());
                    if (sf::FloatRect({chatBoxX, chatBoxY}, {chatBoxWidth, chatBoxHeight}).contains(uiPos)) {
                        chatScroll -= wheel->delta * 30.f;
                        if (chatScroll < 0.f) chatScroll = 0.f;
                    }
                }
            }
            if (auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    sf::Vector2f uiPos = window.mapPixelToCoords(mouse->position, window.getDefaultView());
                    if (sf::FloatRect({chatBoxX, chatBoxY}, {chatBoxWidth, chatBoxHeight}).contains(uiPos)) {
                        isDraggingChat = true;
                        lastMousePosChat = uiPos;
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
                    sf::Vector2f uiPos = window.mapPixelToCoords(move->position, window.getDefaultView());
                    chatScroll -= (uiPos.y - lastMousePosChat.y);
                    if (chatScroll < 0.f) chatScroll = 0.f;
                    lastMousePosChat = uiPos;
                }
            }

            if (event->is<sf::Event::Closed>())
            {
                std::vector<std::pair<int, int>> items(8, { 0, 0 });
                for (int i = 0; i < 8; i++)
                {
                    if (inventory.hasItem(i))
                    {
                        const sf::Texture* tex = inventory.getItemTexture(i);
                        int count = inventory.getItemCount(i);
                        int itemId = 0;
                        if (tex == ChickenMeat::getTexture()) itemId = 1;
                        else if (tex == PigMeat::getTexture()) itemId = 2;
                        else if (tex == ZombieFlesh::getTexture()) itemId = 3;
                        else if (tex == &woodenSwordTex) itemId = 4;
                        else if (tex == &shieldTex) itemId = 6;
                        items[i] = { itemId, count };
                    }
                }
                authManager.saveInventory(username.c_str(), items);
                authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp);
                
                window.close();
            }

            if (auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape)
                {
                    std::vector<std::pair<int, int>> items(8, { 0, 0 });

                    for (int i = 0; i < 8; i++)
                    {
                        if (inventory.hasItem(i))
                        {
                            const sf::Texture* tex = inventory.getItemTexture(i);
                            int count = inventory.getItemCount(i);

                            int itemId = 0;

                            if (tex == ChickenMeat::getTexture()) itemId = 1;
                            else if (tex == PigMeat::getTexture()) itemId = 2;
                            else if (tex == ZombieFlesh::getTexture()) itemId = 3;
                            else if (tex == &woodenSwordTex) itemId = 4;
                            else if (tex == &shieldTex) itemId = 6;

                            items[i] = { itemId, count };
                        }
                    }

                    std::cout << "Saving inventory...\n";
                    authManager.saveInventory(username.c_str(), items);
                    authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp);
                    std::cout << "Saved!\n";

                    // ✅ CLOSE GAME (or break loop)
                    window.close();
                }

                if (key->scancode == globalSettings.keyInventory && !invKeyHeld)
                {
                    inventoryOpen = !inventoryOpen;
                    if (inventoryOpen && tutorialActive && tutorialStep == 3) tutInvOpened = true;
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
                if (mouse->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f mpos((float)mouse->position.x, (float)mouse->position.y);
                    sf::Vector2f uiPos = window.mapPixelToCoords(mouse->position, window.getDefaultView());
                    
                    // Temporarily restore the Game View so mapPixelToCoords maps to the world correctly
                    UpdateCamera(window, sprite, MAP_WIDTH, MAP_HEIGHT);
                    sf::Vector2f worldPos = window.mapPixelToCoords(mouse->position);

                    bool clickHandled = false;

                    sf::FloatRect settingsBtnRect({ (float)width - 140.f, 20.f }, { 120.f, 40.f });
                    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && settingsBtnRect.contains(uiPos)) {
                        openSettingsWindow(window, width, height);
                        deltaClock.restart();
                        continue;
                    }

                    // ✅ Allow wrench click only if no other menus are open
                    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && playerWrenchBtn.getGlobalBounds().contains(worldPos)) {
                        wrenchMenuOpen = true;
                        isTargetNPC = false;
                        wrenchMenuPos = uiPos;
                        continue;   // stop further click handling
                    }
                    else if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && npcFemale.wrenchBtn.getGlobalBounds().contains(worldPos)) {
                        wrenchMenuOpen = true;
                        isTargetNPC = true;
                        wrenchMenuPos = uiPos;
                        continue;   // stop further click handling
                    }
                    else if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && npcMale.wrenchBtn.getGlobalBounds().contains(worldPos)) {
                        wrenchMenuOpen = true;
                        isTargetNPC = true;
                        wrenchMenuPos = uiPos;
                        continue;   // stop further click handling
                    }

                    if (selectionDone && introActive) {
                        currentIntroFrame++;
                        if (currentIntroFrame >= introFrames.size() || (introFrames.empty() && currentIntroFrame >= 1)) {
                            introActive = false;
                            tutorialActive = true;
                            tutorialStep = 0;
                        } else if (!introFrames.empty()) {
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
                    else if (tradeConfirmOpen) {
                        sf::FloatRect yesRect({ (width - 250.f) / 2.f, height / 2.f }, { 100.f, 40.f });
                        sf::FloatRect noRect({ (width + 50.f) / 2.f, height / 2.f }, { 100.f, 40.f });
                        if (yesRect.contains(uiPos)) {
                            tradeConfirmOpen = false;
                            tradeMenuOpen = false;
                            tradeBoxesTicked = false;

                            // TRANSFER NPC ITEMS TO PLAYER
                            for(int i = 0; i < 4; i++) {
                                if (npcTradeSlots[i].tex != nullptr && npcTradeSlots[i].count > 0) {
                                    inventory.addItem(0, *npcTradeSlots[i].tex, npcTradeSlots[i].count);
                                    npcTradeSlots[i].tex = nullptr;
                                    npcTradeSlots[i].count = 0;
                                }
                                playerTradeSlots[i].tex = nullptr;
                                playerTradeSlots[i].count = 0;
                            }

                            // ✅ SAVE ONCE
                            std::vector<std::pair<int, int>> items(8, { 0,0 });
                            for (int i = 0; i < 8; i++) {
                                if (inventory.hasItem(i)) {
                                    const sf::Texture* tex = inventory.getItemTexture(i);
                                    int count = inventory.getItemCount(i);
                                    int itemId = 0;
                                    if (tex == ChickenMeat::getTexture()) itemId = 1;
                                    else if (tex == PigMeat::getTexture()) itemId = 2;
                                    else if (tex == ZombieFlesh::getTexture()) itemId = 3;
                                    else if (tex == &woodenSwordTex) itemId = 4;
                                    else if (tex == &shieldTex) itemId = 6;
                                    items[i] = { itemId, count };
                                }
                            }
                            authManager.saveInventory(username.c_str(), items);
                            clickHandled = true;
                        } else if (noRect.contains(uiPos)) {
                            tradeConfirmOpen = false;
                            tradeMenuOpen = false;
                            tradeBoxesTicked = false;
                            
                            // Cancel Trade! Return items to Inventory
                            for(int i=0; i<4; i++) {
                                if (playerTradeSlots[i].tex != nullptr) {
                                    inventory.addItem(0, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                                    playerTradeSlots[i].tex = nullptr;
                                    playerTradeSlots[i].count = 0;
                                }
                                npcTradeSlots[i].tex = nullptr;
                                npcTradeSlots[i].count = 0;
                            }
                            clickHandled = true;
                        }
                    }

                    else if (tradeMenuOpen) {
                        inventory.handleClick(mouse->position, window);

                        sf::FloatRect acceptRect({ (width + 600.f) / 2.f - 120.f, (height + 400.f) / 2.f - 60.f }, { 100.f, 40.f });
                        if (acceptRect.contains(uiPos)) {
                            tradeBoxesTicked = true;
                            tradeConfirmOpen = true;
                            clickHandled = true;
                        }

                        // Check if player clicked a Player Trade Box to transfer from inventory
                        sf::Vector2f tBoxPos((width - 600.f) / 2.f, (height - 400.f) / 2.f);
                        for(int i=0; i<4; i++) {
                            sf::FloatRect pBoxRect({ tBoxPos.x + 50.f + i * 70.f, tBoxPos.y + 100.f }, { 64.f, 64.f });
                            if (pBoxRect.contains(uiPos)) {
                                int selSlot = inventory.getSelectedSlot();
                                if (selSlot != -1 && inventory.hasItem(selSlot) && playerTradeSlots[i].tex == nullptr) {
                                    playerTradeSlots[i].tex = inventory.getItemTexture(selSlot);
                                    playerTradeSlots[i].count = inventory.getItemCount(selSlot);
                                    
                                    // Use original consumeItem in a loop to bypass Intellisense caching issues
                                    int countToConsume = playerTradeSlots[i].count;
                                    for (int c = 0; c < countToConsume; c++) {
                                        inventory.consumeItem(selSlot);
                                    }
                                }
                                clickHandled = true;
                            }
                        }
                    }
                    else if (wrenchMenuOpen) {

                        if (isTargetNPC) {
                            // ✅ TRADE & QUEST for NPC
                            sf::FloatRect tradeBound({ wrenchMenuPos.x, wrenchMenuPos.y }, { 150.f, 30.f });
                            sf::FloatRect questBound({ wrenchMenuPos.x, wrenchMenuPos.y + 30.f }, { 150.f, 30.f });

                            if (tradeBound.contains(uiPos)) {
                                tradeMenuOpen = true;
                                wrenchMenuOpen = false;

                                // Clear Trade Slots on Open
                                for (int i = 0; i < 4; i++) {
                                    playerTradeSlots[i].tex = nullptr;
                                    playerTradeSlots[i].count = 0;
                                    npcTradeSlots[i].tex = nullptr;
                                    npcTradeSlots[i].count = 0;
                                }

                                // Mock NPC items
                                npcTradeSlots[0].tex = ChickenMeat::getTexture();
                                npcTradeSlots[0].count = 5;

                                tradeBoxesTicked = false;
                                clickHandled = true;
                            }
                            else if (questBound.contains(uiPos)) {
                                int questIdx = qInfo.questID - 1;
                                if (questIdx >= 0 && questIdx < (int)availableQuests.size()) {
                                    auto& q = availableQuests[questIdx];
                                    if (qInfo.isCompleted) {
                                        latestPlayerMsg = "Quest: " + q.title + " (COMPLETED!)";
                                    } else {
                                        latestPlayerMsg = "Quest: " + q.title + "\n" + q.description + " (" + std::to_string(qInfo.progress) + "/" + std::to_string(q.goal) + ")";
                                    }
                                } else {
                                    latestPlayerMsg = "No quest available right now.";
                                }
                                bubbleTimer = 5.0f;
                                wrenchMenuOpen = false;
                                clickHandled = true;
                            }
                            else {
                                wrenchMenuOpen = false;
                                clickHandled = true;
                            }
                        }
                        else {
                            // ✅ PLAYER → only Craft
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

                    else if (saplingPlacementMode)
                    {
                        float btnW = 220.f, btnH = 70.f, gap = 40.f;
                        float tWidth = btnW * 2.f + gap;
                        float sX = (width - tWidth) / 2.f;
                        float yP = height - 110.f;
                        sf::FloatRect placeBtn({ sX, yP }, { btnW, btnH });
                        sf::FloatRect cancelBtn({ sX + btnW + gap, yP }, { btnW, btnH });

                        if (placeBtn.contains(uiPos))
                        {
                            PlantedSapling s;
                            s.position = saplingPlacementPos;
                            s.growTimer = 40.f;
                            s.grown = false;

                            plantedSaplings.push_back(s);

                            for (int i = 0; i < 8; i++)
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

                            saplingPlacementMode = false;

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

                            if (!saplingFollowingCursor)
                                saplingPlacementPos = worldPos;

                            clickHandled = true;
                        }
                        }

                    else if (placementMode) {
                        float btnW = 220.f, btnH = 70.f, gap = 40.f;
                        float tWidth = btnW * 2.f + gap;
                        float sX = (width - tWidth) / 2.f;
                        float yP = height - 110.f;
                        sf::FloatRect placeBtn({ sX, yP }, { btnW, btnH });
                        sf::FloatRect cancelBtn({ sX + btnW + gap, yP }, { btnW, btnH });
                        
                        if (placeBtn.contains(uiPos)) {
                            int totalWood = 0, totalStone = 0;
                            for (int i = 0; i < 8; i++) {
                                if (inventory.hasItem(i)) {
                                    if (inventory.getItemTexture(i) == &woodTex) totalWood += inventory.getItemCount(i);
                                    if (inventory.getItemTexture(i) == &stoneTex) totalStone += inventory.getItemCount(i);
                                }
                            }
                            if (totalWood >= 10 && totalStone >= 10) {
                                int woodToConsume = 10, stoneToConsume = 10;
                                for (int i = 0; i < 8; i++) {
                                    if (inventory.hasItem(i)) {
                                        if (inventory.getItemTexture(i) == &woodTex) {
                                            while(woodToConsume > 0 && inventory.getItemCount(i) > 0) { inventory.consumeItem(i); woodToConsume--; }
                                        }
                                        if (inventory.getItemTexture(i) == &stoneTex) {
                                            while(stoneToConsume > 0 && inventory.getItemCount(i) > 0) { inventory.consumeItem(i); stoneToConsume--; }
                                        }
                                    }
                                }
                                PlacedCampfire pc;
                                pc.position = placementPos;
                                pc.timeRemaining = 90.f;
                                pc.isCrafted = false;
                                pc.currentFrame = 0;
                                pc.frameTimer = 0.f;
                                campfires.push_back(pc);
                                placementMode = false;
                            }
                            clickHandled = true;
                        } else if (cancelBtn.contains(uiPos)) {
                            placementMode = false;
                            clickHandled = true;
                        } else {
                            placementFollowingCursor = !placementFollowingCursor;
                            if (!placementFollowingCursor) {
                                placementPos = worldPos;
                            }
                            clickHandled = true;
                        }
                    }
                    else if (craftMenuOpen) {
                        sf::FloatRect craftBox({ (width - 1200.f) / 2.f, (height - 650.f) / 2.f }, { 1200.f, 650.f });
                        sf::FloatRect closeBtn({ craftBox.position.x + 1120.f, craftBox.position.y + 15.f }, { 50.f, 50.f });
                        sf::FloatRect craftBtn({ craftBox.position.x + 60.f, craftBox.position.y + 520.f }, { 200.f, 80.f });
                        sf::FloatRect craftSwordBtn({ craftBox.position.x + 460.f, craftBox.position.y + 520.f }, { 200.f, 80.f });
                        sf::FloatRect craftShieldBtn({ craftBox.position.x + 860.f, craftBox.position.y + 520.f }, { 200.f, 80.f });
                        
                        if (closeBtn.contains(uiPos)) {
                            craftMenuOpen = false;
                            clickHandled = true;
                        } else if (craftBtn.contains(uiPos)) {
                            int totalWood = 0, totalStone = 0;
                            for (int i = 0; i < 8; i++) {
                                if (inventory.hasItem(i)) {
                                    if (inventory.getItemTexture(i) == &woodTex) totalWood += inventory.getItemCount(i);
                                    if (inventory.getItemTexture(i) == &stoneTex) totalStone += inventory.getItemCount(i);
                                }
                            }
                            if (totalWood >= 10 && totalStone >= 10) {
                                craftMenuOpen = false;
                                placementMode = true;
                                placementFollowingCursor = true;
                                
                                if (!tutorialActive && qInfo.questID == 3 && qInfo.progress < 1) {
                                    qInfo.progress++;
                                    authManager.updateQuestProgress(username.c_str(), qInfo.progress, qInfo.isCompleted);
                                }
                            }
                            clickHandled = true;
                        } else if (craftSwordBtn.contains(uiPos) && !swordCrafting) {
                            int totalWood = 0;
                            for (int i = 0; i < 8; i++) {
                                if (inventory.hasItem(i) && inventory.getItemTexture(i) == &woodTex) {
                                    totalWood += inventory.getItemCount(i);
                                }
                            }
                            if (totalWood >= 8) {
                                int woodToConsume = 8;
                                for (int i = 0; i < 8; i++) {
                                    if (inventory.hasItem(i) && inventory.getItemTexture(i) == &woodTex) {
                                        while(woodToConsume > 0 && inventory.getItemCount(i) > 0) { inventory.consumeItem(i); woodToConsume--; }
                                    }
                                }
                                swordCrafting = true;
                                swordCraftTimer = 30.f;
                            }
                            clickHandled = true;
                        } else if (craftShieldBtn.contains(uiPos) && !shieldCrafting) {
                            int totalWood = 0, totalStone = 0;
                            for (int i = 0; i < 8; i++) {
                                if (inventory.hasItem(i)) {
                                    if (inventory.getItemTexture(i) == &woodTex) totalWood += inventory.getItemCount(i);
                                    if (inventory.getItemTexture(i) == &stoneTex) totalStone += inventory.getItemCount(i);
                                }
                            }
                            if (totalWood >= 10 && totalStone >= 5) {
                                int woodToConsume = 10;
                                int stoneToConsume = 5;
                                for (int i = 0; i < 8; i++) {
                                    if (inventory.hasItem(i)) {
                                        if (inventory.getItemTexture(i) == &woodTex) {
                                            while(woodToConsume > 0 && inventory.getItemCount(i) > 0) { inventory.consumeItem(i); woodToConsume--; }
                                        }
                                        if (inventory.getItemTexture(i) == &stoneTex) {
                                            while(stoneToConsume > 0 && inventory.getItemCount(i) > 0) { inventory.consumeItem(i); stoneToConsume--; }
                                        }
                                    }
                                }
                                shieldCrafting = true;
                                shieldCraftTimer = 60.f; // 1 minute
                            }
                            clickHandled = true;
                        }
                    }
                    else if (!selectionDone)
                    {
                        if (button1.getGlobalBounds().contains(mpos))
                        {
                            option = 1;
                            selectionDone = true;
                            clickHandled = true;
                        }
                        else if (button2.getGlobalBounds().contains(mpos))
                        {
                            option = 2;
                            selectionDone = true;
                            clickHandled = true;
                        }
                    }

                    if (!clickHandled && inventoryOpen)
                    {
                        int clickedSlot = inventory.getSlotAt(uiPos);
                        if (clickedSlot != -1)
                        {
                            // Always allow slot selection
                            inventory.handleClick(mouse->position, window);

                            if (inventory.hasItem(clickedSlot))
                            {
                                const sf::Texture* tex = inventory.getItemTexture(clickedSlot);
                                // Only block consumption during trade
                                if (!tradeMenuOpen)
                                {
                                    if (tex == &saplingTex)
                                    {
                                        saplingPlacementMode = true;
                                        saplingFollowingCursor = true;
                                    }

                                    if (tex == ChickenMeat::getTexture())
                                    {
                                        hunger = std::max(0.f, hunger - 30.f);
                                        inventory.consumeItem(clickedSlot);
                                        std::cout << "Eat Chicken\n";
                                    }
                                    else if (tex == PigMeat::getTexture())
                                    {
                                        hunger = std::max(0.f, hunger - 45.f);
                                        inventory.consumeItem(clickedSlot);
                                        std::cout << "Eat Pig\n";
                                    }
                                    else if (tex == ZombieFlesh::getTexture())
                                    {
                                        hunger = std::max(0.f, hunger - 20.f);
                                        hp = std::max(0.f, hp - 10.f);
                                        inventory.consumeItem(clickedSlot);
                                        std::cout << "Eat Zombie\n";
                                    }
                                }
                            }
                            clickHandled = true;
                        }
                    }
                    
                }
            }
        }

        if (selectionDone && !animLoadedForSelection)
        {

            if (option == 1)
            {
                animWalk.load("../Sprites/Male/Walk", FRAMES_PER_DIR);
                animWalkWithSword.load("../Sprites/Male/WalkWithSword", FRAMES_PER_DIR);
                animWalkWithShield.load("../Sprites/Male/WalkWithShield", FRAMES_PER_DIR);
                animWalkWithShieldAndSword.load("../Sprites/Male/WalkWithShieldAndSword", FRAMES_PER_DIR);
                animAttackWithSword.load("../Sprites/Male/AttackWithSword", FRAMES_PER_DIR);
                animAttackWithShieldAndSword.load("../Sprites/Male/AttackWithSwordAndShield", FRAMES_PER_DIR);
            }
            else
            {
                animWalk.load("../Sprites/Female/Walk", FRAMES_PER_DIR);
                animWalkWithSword.load("../Sprites/Female/WalkWithSword", FRAMES_PER_DIR);
                animWalkWithShield.load("../Sprites/Female/WalkWithShield", FRAMES_PER_DIR);
                animWalkWithShieldAndSword.load("../Sprites/Female/WalkWithShieldAndSword", FRAMES_PER_DIR);
                animAttackWithSword.load("../Sprites/Female/AttackWithSword", FRAMES_PER_DIR);
                animAttackWithShieldAndSword.load("../Sprites/Female/AttackWithSwordAndShield", FRAMES_PER_DIR);
            }

            sprite.setTexture(animWalk.south.getCurrentTexture());
            sf::Vector2u ts_init = sprite.getTexture().getSize();
            if (ts_init.x > 0 && ts_init.y > 0) {
                sprite.setOrigin({ ts_init.x / 2.f, ts_init.y / 2.f });
                sprite.setScale({ 500.f / ts_init.x, 500.f / ts_init.y });
            }
            

            // ✅ LOAD INVENTORY FROM DATABASE
            auto loadedItems = authManager.loadInventory(username.c_str());
            PlayerStats stats = authManager.loadPlayerStats(username.c_str());
            hunger = stats.hunger;
            thirst = stats.thirst;
            currency = stats.currency;
            level = stats.level;
            exp = stats.exp;

            for (int i = 0; i < 8; i++)
            {
                int itemId = loadedItems[i].first;
                int count = loadedItems[i].second;

                if (count > 0)
                {
                    if (itemId == 1)
                        inventory.setItem(i, *ChickenMeat::getTexture(), count);
                    else if (itemId == 2)
                        inventory.setItem(i, *PigMeat::getTexture(), count);
                    else if (itemId == 3)
                        inventory.setItem(i, *ZombieFlesh::getTexture(), count);
                    else if (itemId == 4)
                        inventory.setItem(i, woodenSwordTex, count);
                    else if (itemId == 6)
                        inventory.setItem(i, shieldTex, count);
                }
            }

            if (npcFemale.texture.loadFromFile("../Sprites/NPCFemale/Walk/South/frame_000.png")) {
                npcFemale.sprite.setTexture(npcFemale.texture);
                sf::Vector2u ts_npcF = npcFemale.texture.getSize();
                if (ts_npcF.x > 0 && ts_npcF.y > 0) {
                    npcFemale.sprite.setOrigin({ ts_npcF.x / 2.f, ts_npcF.y / 2.f });
                    npcFemale.sprite.setScale({ 500.f / ts_npcF.x, 500.f / ts_npcF.y });
                }
                npcFemale.sprite.setPosition({ width / 2.f + 150.f, height / 2.f - 50.f });
            }
            
            if (npcMale.texture.loadFromFile("../Sprites/NPCMale/Walk/South/frame_000.png")) {
                npcMale.sprite.setTexture(npcMale.texture);
                sf::Vector2u ts_npcM = npcMale.texture.getSize();
                if (ts_npcM.x > 0 && ts_npcM.y > 0) {
                    npcMale.sprite.setOrigin({ ts_npcM.x / 2.f, ts_npcM.y / 2.f });
                    npcMale.sprite.setScale({ 500.f / ts_npcM.x, 500.f / ts_npcM.y });
                }
                npcMale.sprite.setPosition({ MAP_WIDTH / 2.f - 150.f, MAP_HEIGHT / 2.f - 50.f });
            }
            if (npcFemale.sprite.getPosition().x == 0) { // Fallback if load failed or position not set
                 npcFemale.sprite.setPosition({ MAP_WIDTH / 2.f + 150.f, MAP_HEIGHT / 2.f - 50.f });
                 npcMale.sprite.setPosition({ MAP_WIDTH / 2.f - 150.f, MAP_HEIGHT / 2.f - 50.f });
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
            window.clear(sf::Color::White);
            window.setView(window.getDefaultView());
            window.draw(selBgSprite);
            window.draw(Option);
            window.draw(button1);
            window.draw(buttonText1);
            window.draw(button2);
            window.draw(buttonText2);

            window.display();
            continue;
        }

        if (!isDead && !introActive)
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

            for (auto& m : monsters)
            {
                if (m->health > 0)
                    m->update(dt, sprite.getPosition());

                sf::FloatRect zombieBounds = m->sprite.getGlobalBounds();
                sf::FloatRect playerBounds = sprite.getGlobalBounds();

                sf::FloatRect z = m->sprite.getGlobalBounds();
                sf::FloatRect p = sprite.getGlobalBounds();

                sf::Vector2f zCenter(z.position.x + z.size.x * 0.5f, z.position.y + z.size.y * 0.5f);
                sf::Vector2f pCenter(p.position.x + p.size.x * 0.5f, p.position.y + p.size.y * 0.5f);
                float dx = zCenter.x - pCenter.x;
                float dy = zCenter.y - pCenter.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                bool hit = dist < 35.f; // Closer contact threshold
                if (!introActive && hit)
                {
                    zombieAttackTimer += dt;

                    if (zombieAttackTimer >= ZOMBIE_ATTACK_COOLDOWN)
                    {
                        if (!tutorialActive) {
                            if (shieldEquipped && eqShieldSlot != -1) {
                                int shieldDur = inventory.getDurability(eqShieldSlot);
                                shieldDur -= ZOMBIE_DAMAGE;
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
                                hp -= ZOMBIE_DAMAGE;
                            }
                        }
                        zombieAttackTimer = 0.f;
                        isPlayerHurt = true;
                        playerHurtTimer = 0.25f;

                        sf::Vector2f dir = sprite.getPosition() - m->sprite.getPosition();
                        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (len != 0.f) dir /= len;
                        sprite.move(dir * 50.f);
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

			bool moveUp = sf::Keyboard::isKeyPressed(globalSettings.keyUp);
			bool moveDown = sf::Keyboard::isKeyPressed(globalSettings.keyDown);
			bool moveLeft = sf::Keyboard::isKeyPressed(globalSettings.keyLeft);
			bool moveRight = sf::Keyboard::isKeyPressed(globalSettings.keyRight);

            if (moveUp || moveDown || moveLeft || moveRight) {
                sf::Vector2f curPos = sprite.getPosition();
                float dx = curPos.x - tutLastPos.x;
                float dy = curPos.y - tutLastPos.y;
                float d = std::sqrt(dx*dx + dy*dy);
                if (tutorialActive && tutorialStep == 1) tutMoveDist += d;
                tutLastPos = curPos;

                if (moveUp)
                {
                    sprite.move({ 0.f, -currentSpeed * dt });
                    currentDir = Dir::North;
                    currentAnimSet->setDirPlaying(1, true);
                }
                else if (moveDown)
                {
                    sprite.move({ 0.f, currentSpeed * dt });
                    currentDir = Dir::South;
                    currentAnimSet->setDirPlaying(0, true);
                }
                else if (moveLeft)
                {
                    sprite.move({ -currentSpeed * dt, 0.f });
                    currentDir = Dir::West;
                    currentAnimSet->setDirPlaying(3, true);
                }
                else if (moveRight)
                {
                    sprite.move({ currentSpeed * dt, 0.f });
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

            currentAnimSet->update(dt);

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

            if ((sf::Keyboard::isKeyPressed(globalSettings.keyAttack) || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Space)) && playerAttackCooldown <= 0.f)
            {
                playerAttackCooldown = 0.3f; // Attack cooldown

                float damage = swordEquipped ? 50.f : 25.f;
                bool hitSomething = false;

                sf::FloatRect attackBox = sprite.getGlobalBounds();
                attackBox.position.x -= 150.f;
                attackBox.position.y -= 150.f;
                attackBox.size.x += 300.f;
                attackBox.size.y += 300.f;

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
                } else {
                    pc.frameTimer += dt;
                    if (pc.frameTimer >= 0.1f) {
                        pc.frameTimer = 0.f;
                        pc.currentFrame = (pc.currentFrame + 1) % 8;
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

            sf::Vector2f pondCenter(
                pondBounds.position.x + pondBounds.size.x * 0.5f,
                pondBounds.position.y + pondBounds.size.y * 0.5f
            );

            nearPond = pondBounds.contains(sprite.getPosition());


            window.clear(sf::Color::White);
            window.draw(background);
            window.draw(pond);
            itemManager.draw(window);

            for (auto& pc : campfires) {
                sf::Sprite cs(campfireFrames[pc.currentFrame]);
                cs.setOrigin({ cs.getGlobalBounds().size.x / 2.f, cs.getGlobalBounds().size.y / 2.f });
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
            }

            if (placementMode) {
                if (placementFollowingCursor) {
                    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                    placementPos = window.mapPixelToCoords(pixelPos);
                }
                
                sf::Sprite ghost(campfireFrames[0]);
                ghost.setOrigin({ ghost.getGlobalBounds().size.x / 2.f, ghost.getGlobalBounds().size.y / 2.f });
                ghost.setPosition(placementPos);
                ghost.setColor(sf::Color(255, 255, 255, 128));
                window.draw(ghost);
            }

            if (saplingPlacementMode)
            {
                if (saplingFollowingCursor)
                {
                    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                    saplingPlacementPos = window.mapPixelToCoords(pixelPos);
                }

                sf::Sprite ghost(saplingTex);

                ghost.setScale({ 0.2f, 0.2f });
                ghost.setColor(sf::Color(255, 255, 255, 128));
                ghost.setPosition(saplingPlacementPos);

                window.draw(ghost);
            }

            if (isPlayerHurt) {
                playerHurtTimer -= dt;
                if (playerHurtTimer <= 0.f) isPlayerHurt = false;
                sprite.setColor(sf::Color(255, 100, 100));
            } else {
                sprite.setColor(sf::Color::White);
            }

            for (auto& s : plantedSaplings)
            {
                if (!s.grown)
                {
                    sf::Sprite spr(saplingTex);

                    spr.setScale({ 0.1f, 0.1f });
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

            window.draw(sprite);
            sprite.setColor(sf::Color::White);

            // Removed old playerSwordSprite drawing code
            if (animLoadedForSelection) {
                window.draw(npcFemale.sprite);
                window.draw(npcMale.sprite);
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
                sf::Text npcFemaleNameText(font, npcFemale.name, 18);
                npcFemaleNameText.setFillColor(sf::Color::White);
                npcFemaleNameText.setOutlineColor(sf::Color::Black);
                npcFemaleNameText.setOutlineThickness(1.5f);
                npcFemaleNameText.setPosition({ npcFemale.sprite.getPosition().x - npcFemaleNameText.getLocalBounds().size.x / 2.f, npcFemale.sprite.getPosition().y - 280.f });
                npcFemale.wrenchBtn.setPosition({ npcFemaleNameText.getPosition().x + npcFemaleNameText.getLocalBounds().size.x + 5.f, npcFemaleNameText.getPosition().y });
                window.draw(npcFemaleNameText);
                window.draw(npcFemale.wrenchBtn);

                sf::Text npcMaleNameText(font, npcMale.name, 18);
                npcMaleNameText.setFillColor(sf::Color::White);
                npcMaleNameText.setOutlineColor(sf::Color::Black);
                npcMaleNameText.setOutlineThickness(1.5f);
                npcMaleNameText.setPosition({ npcMale.sprite.getPosition().x - npcMaleNameText.getLocalBounds().size.x / 2.f, npcMale.sprite.getPosition().y - 280.f });
                npcMale.wrenchBtn.setPosition({ npcMaleNameText.getPosition().x + npcMaleNameText.getLocalBounds().size.x + 5.f, npcMaleNameText.getPosition().y });
                window.draw(npcMaleNameText);
                window.draw(npcMale.wrenchBtn);
            }

            if (nearPond)
            {
                drinkText.setPosition({
                    sprite.getPosition().x - 160.f,
                    sprite.getPosition().y - 100.f
                    });

                window.draw(drinkText);
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
                if (a->health > 0)
                    a->update(dt, sprite.getPosition());

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
                    thirst = std::max(0.f, thirst - 40.f);
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

        // --- Multiplayer Sync ---
        static sf::Clock syncClock;
        static std::vector<OnlinePlayer> otherPlayers;
        if (syncClock.getElapsedTime().asSeconds() > 0.2f) {
            authManager.updateOnlineStatus(username.c_str(), sprite.getPosition().x, sprite.getPosition().y);
            otherPlayers = authManager.getOnlinePlayers(username.c_str());
            
            // Check self for pull
            OnlinePlayer self = authManager.getSelfOnlineInfo(username.c_str());
            if (!self.pullBy.empty()) {
                for (const auto& op : otherPlayers) {
                    if (op.username == self.pullBy) {
                        sprite.setPosition({ op.x, op.y });
                        authManager.clearPull(username.c_str());
                        latestPlayerMsg = "I've been pulled by " + op.username + "!";
                        bubbleTimer = 4.f;
                        break;
                    }
                }
            }
            syncClock.restart();
        }

        // Render Online Players
        for (const auto& op : otherPlayers) {
            sf::Sprite opSprite = sprite; // Reuse local player's visual
            opSprite.setColor(sf::Color(255, 200, 200, 255)); // Tint slightly red
            opSprite.setPosition({ op.x, op.y });
            window.draw(opSprite);

            sf::Text opName(uiFont, op.username, 16);
            opName.setFillColor(sf::Color::White);
            opName.setOutlineColor(sf::Color::Black);
            opName.setOutlineThickness(1.f);
            sf::FloatRect nb = opName.getLocalBounds();
            opName.setPosition({ op.x - nb.size.x / 2.f + 16.f, op.y - 40.f });
            window.draw(opName);
        }

                if (currentStones < 15) // Maintain 15 stones max
                {
                    for (int a = 0; a < 20; ++a)
                    {
                        sf::Vector2f p(randX(gen), randY(gen));
                        if (itemManager.isAreaFreeExplicit(p, 120.f, &stoneTex))
                        {
                            itemManager.addItem(ITEM_STONE, stoneTex, p, 1);
                            break;
                        }
                    }
                }
                stoneRespawnClock.restart();
            }
        }
        const float totalCycleTime = 900.f;
        float cycleValue = std::fmod(dayNightClock.getElapsedTime().asSeconds(), totalCycleTime);
        float t = cycleValue / totalCycleTime;
        float timeOfDay = (t < 0.5f) ? (t * 2.f) : (1.f - (t - 0.5f) * 2.f);
        int darknessLevel = static_cast<int>(timeOfDay * 200.f);
        darkness.setFillColor(sf::Color(0, 0, 0, darknessLevel));
        window.draw(darkness);

        bool isNight = (darknessLevel > 80);

        if (inventoryOpen)
        {
            window.setView(window.getDefaultView());

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
                    "../Music/Shirfine_-_Illusionary_Daytime_(mp3.pm).mp3"
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


        window.setView(window.getDefaultView());

        if (wrenchMenuOpen) {
            float boxHeight = 30.f;

            sf::RectangleShape menuBox({ 150.f, boxHeight });
            menuBox.setPosition(wrenchMenuPos);
            menuBox.setFillColor(sf::Color(40, 40, 40, 240));
            menuBox.setOutlineColor(sf::Color::White);
            menuBox.setOutlineThickness(1.f);

            window.draw(menuBox);

            if (isTargetNPC) {
                // ✅ NPC → TRADE & QUEST
                sf::Text tradeTxt(uiFont, "1. Trade", 18);
                tradeTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 5.f });
                window.draw(tradeTxt);

                sf::Text questTxt(uiFont, "2. Daily Quest", 18);
                questTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 35.f });
                window.draw(questTxt);
                
                // Adjust menu height for two items
                menuBox.setSize({ 150.f, 60.f });
            }
            else {
                // ✅ PLAYER → ONLY CRAFT
                sf::Text craftTxt(uiFont, "Craft Items", 18);
                craftTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 5.f });
                window.draw(craftTxt);
            }
        }

        if (craftMenuOpen) {
            static sf::Clock dbgClock;
            if (dbgClock.getElapsedTime().asSeconds() > 1.f) {
                std::cout << "Rendering Craft Menu Box! width=" << width << " height=" << height << "\n";
                dbgClock.restart();
            }
            sf::RectangleShape craftBox({ 1200.f, 650.f });
            craftBox.setPosition({ (width - 1200.f) / 2.f, (height - 650.f) / 2.f });
            craftBox.setFillColor(sf::Color(40, 40, 40, 245));
            craftBox.setOutlineThickness(4.f);
            craftBox.setOutlineColor(sf::Color(120, 120, 120));
            window.draw(craftBox);

            sf::Text title(uiFont, "CRAFT MENU", 42);
			title.setStyle(sf::Text::Bold);
			title.setFillColor(sf::Color::White);
            title.setPosition({ craftBox.getPosition().x + 20.f, craftBox.getPosition().y + 10.f });
            window.draw(title);

            sf::RectangleShape closeBtn({ 50.f, 50.f });
            closeBtn.setPosition({ craftBox.getPosition().x + 1120.f, craftBox.getPosition().y + 15.f });
            closeBtn.setFillColor(sf::Color::Red);
            window.draw(closeBtn);
            sf::Text closeTxt(uiFont, "X", 32);
            closeTxt.setPosition({ closeBtn.getPosition().x + 14.f, closeBtn.getPosition().y + 4.f });
            window.draw(closeTxt);

            // ---- CAMPFIRE (LEFT) ----
            sf::Sprite preview(campfireFrames[0]);
            sf::Vector2u ts = preview.getTexture().getSize();
            preview.setScale({ 300.f / ts.x, 300.f / ts.y });
            preview.setPosition({ craftBox.getPosition().x + 60.f, craftBox.getPosition().y + 100.f });
            window.draw(preview);

            sf::Text reqTxt(uiFont, "Requires:\n10 Wood\n10 Stone", 30);
            reqTxt.setPosition({ craftBox.getPosition().x + 60.f, craftBox.getPosition().y + 420.f });
            window.draw(reqTxt);

            sf::RectangleShape craftBtn({ 200.f, 80.f });
            craftBtn.setPosition({ craftBox.getPosition().x + 60.f, craftBox.getPosition().y + 520.f });
            
            int totalWood = 0, totalStone = 0;
            for (int i = 0; i < 8; i++) {
                if (inventory.hasItem(i)) {
                    if (inventory.getItemTexture(i) == &woodTex) totalWood += inventory.getItemCount(i);
                    if (inventory.getItemTexture(i) == &stoneTex) totalStone += inventory.getItemCount(i);
                }
            }
            if (totalWood >= 10 && totalStone >= 10) craftBtn.setFillColor(sf::Color::Green);
            else craftBtn.setFillColor(sf::Color(100, 100, 100));
            
            window.draw(craftBtn);
            sf::Text craftTxtBtn(uiFont, "Craft", 35);
            craftTxtBtn.setPosition({ craftBtn.getPosition().x + 55.f, craftBtn.getPosition().y + 15.f });
            window.draw(craftTxtBtn);

            // ---- WOODEN SWORD (MIDDLE) ----
            sf::Sprite previewSword(woodenSwordTex);
            sf::Vector2u tsw = previewSword.getTexture().getSize();
            if (tsw.x > 0 && tsw.y > 0) previewSword.setScale({ 300.f / tsw.x, 300.f / tsw.y });
            previewSword.setPosition({ craftBox.getPosition().x + 460.f, craftBox.getPosition().y + 100.f });
            window.draw(previewSword);

            sf::Text reqSwordTxt(uiFont, "Requires:\n8 Wood", 30);
            reqSwordTxt.setPosition({ craftBox.getPosition().x + 460.f, craftBox.getPosition().y + 420.f });
            window.draw(reqSwordTxt);

            if (swordCrafting) {
                sf::Text timerTxt(uiFont, "Crafting: " + std::to_string((int)swordCraftTimer) + "s", 30);
                timerTxt.setFillColor(sf::Color::Yellow);
                timerTxt.setPosition({ craftBox.getPosition().x + 460.f, craftBox.getPosition().y + 540.f });
                window.draw(timerTxt);
            } else {
                sf::RectangleShape craftSwordBtn({ 200.f, 80.f });
                craftSwordBtn.setPosition({ craftBox.getPosition().x + 460.f, craftBox.getPosition().y + 520.f });
                
                if (totalWood >= 8) craftSwordBtn.setFillColor(sf::Color::Green);
                else craftSwordBtn.setFillColor(sf::Color(100, 100, 100));
                
                window.draw(craftSwordBtn);
                sf::Text craftSwordTxtBtn(uiFont, "Craft", 35);
                craftSwordTxtBtn.setPosition({ craftSwordBtn.getPosition().x + 55.f, craftSwordBtn.getPosition().y + 15.f });
                window.draw(craftSwordTxtBtn);
            }

            // ---- SHIELD (RIGHT) ----
            sf::Sprite previewShield(shieldTex);
            sf::Vector2u tsh = previewShield.getTexture().getSize();
            if (tsh.x > 0 && tsh.y > 0) previewShield.setScale({ 300.f / tsh.x, 300.f / tsh.y });
            previewShield.setPosition({ craftBox.getPosition().x + 860.f, craftBox.getPosition().y + 100.f });
            window.draw(previewShield);

            sf::Text reqShieldTxt(uiFont, "Requires:\n10 Wood\n5 Stone", 30);
            reqShieldTxt.setPosition({ craftBox.getPosition().x + 860.f, craftBox.getPosition().y + 420.f });
            window.draw(reqShieldTxt);

            if (shieldCrafting) {
                sf::Text timerShieldTxt(uiFont, "Crafting: " + std::to_string((int)shieldCraftTimer) + "s", 30);
                timerShieldTxt.setFillColor(sf::Color::Yellow);
                timerShieldTxt.setPosition({ craftBox.getPosition().x + 860.f, craftBox.getPosition().y + 540.f });
                window.draw(timerShieldTxt);
            } else {
                sf::RectangleShape craftShieldBtn({ 200.f, 80.f });
                craftShieldBtn.setPosition({ craftBox.getPosition().x + 860.f, craftBox.getPosition().y + 520.f });
                
                if (totalWood >= 10 && totalStone >= 5) craftShieldBtn.setFillColor(sf::Color::Green);
                else craftShieldBtn.setFillColor(sf::Color(100, 100, 100));
                
                window.draw(craftShieldBtn);
                sf::Text craftShieldTxtBtn(uiFont, "Craft", 35);
                craftShieldTxtBtn.setPosition({ craftShieldBtn.getPosition().x + 55.f, craftShieldBtn.getPosition().y + 15.f });
                window.draw(craftShieldTxtBtn);
            }
        }

        if (placementMode) {
            float btnW = 220.f, btnH = 70.f, gap = 40.f;
            float tWidth = btnW * 2.f + gap;
            float sX = (width - tWidth) / 2.f;
            float yP = height - 110.f;

            sf::RectangleShape placeBtn({ btnW, btnH });
            placeBtn.setPosition({ sX, yP });
            
            int totalWood = 0, totalStone = 0;
            for (int i = 0; i < 8; i++) {
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

            sf::RectangleShape cancelBtn({ btnW, btnH });
            cancelBtn.setPosition({ sX + btnW + gap, yP });
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
            const float btnWidth = 220.f;
            const float btnHeight = 70.f;
            const float gap = 40.f;

            float totalWidth = btnWidth * 2.f + gap;
            float startX = (width - totalWidth) / 2.f;
            float yPos = height - 110.f;

            // PLACE BUTTON
            sf::RectangleShape placeBtn({ btnWidth, btnHeight });
            placeBtn.setPosition({ startX, yPos });
            placeBtn.setFillColor(sf::Color(60, 180, 75));
            placeBtn.setOutlineThickness(3.f);
            placeBtn.setOutlineColor(sf::Color::White);

            window.draw(placeBtn);

            sf::Text placeTxt(uiFont, "Place", 36);
            placeTxt.setFillColor(sf::Color::White);

            sf::FloatRect pBounds = placeTxt.getLocalBounds();

            placeTxt.setPosition({
                startX + (btnWidth - pBounds.size.x) / 2.f,
                yPos + (btnHeight - pBounds.size.y) / 2.f - 8.f
                });

            window.draw(placeTxt);

            // CANCEL BUTTON
            sf::RectangleShape cancelBtn({ btnWidth, btnHeight });
            cancelBtn.setPosition({ startX + btnWidth + gap, yPos });
            cancelBtn.setFillColor(sf::Color(200, 60, 60));
            cancelBtn.setOutlineThickness(3.f);
            cancelBtn.setOutlineColor(sf::Color::White);

            window.draw(cancelBtn);

            sf::Text cancelTxt(uiFont, "Cancel", 36);
            cancelTxt.setFillColor(sf::Color::White);

            sf::FloatRect cBounds = cancelTxt.getLocalBounds();

            cancelTxt.setPosition({
                cancelBtn.getPosition().x + (btnWidth - cBounds.size.x) / 2.f,
                yPos + (btnHeight - cBounds.size.y) / 2.f - 8.f
                });

            window.draw(cancelTxt);
        }

        if (tradeMenuOpen) {
            sf::RectangleShape tBox({600.f, 400.f});
            tBox.setPosition({ (width - 600.f) / 2.f, (height - 400.f) / 2.f });
            tBox.setFillColor(sf::Color(30,30,30, 240));
            tBox.setOutlineThickness(2.f);
            tBox.setOutlineColor(sf::Color::White);
            window.draw(tBox);

            sf::Text tTitle(uiFont, "TRADE", 24);
            tTitle.setPosition({ tBox.getPosition().x + 20.f, tBox.getPosition().y + 10.f });
            window.draw(tTitle);

            // Draw player boxes
            for(int i=0; i<4; ++i) {
                sf::RectangleShape b({64.f, 64.f});
                b.setFillColor(sf::Color(80,80,80));
                b.setPosition({tBox.getPosition().x + 50.f + i * 70.f, tBox.getPosition().y + 100.f});
                window.draw(b);

                if (playerTradeSlots[i].tex != nullptr) {
                    sf::Sprite s(*playerTradeSlots[i].tex);
                    s.setPosition({ b.getPosition().x + 8.f, b.getPosition().y + 8.f });
                    sf::Vector2u sz = s.getTexture().getSize();
                    s.setScale({ 48.f / sz.x, 48.f / sz.y });
                    window.draw(s);
                    
                    sf::Text cText(uiFont, std::to_string(playerTradeSlots[i].count), 16);
                    cText.setPosition({ b.getPosition().x + 40.f, b.getPosition().y + 40.f });
                    window.draw(cText);
                }

                if (tradeBoxesTicked) {
                    sf::Text tick(uiFont, "V", 30);
                    tick.setFillColor(sf::Color::Green);
                    tick.setPosition({ b.getPosition().x + 20.f, b.getPosition().y + 10.f });
                    window.draw(tick);
                }
            }

            // Draw target boxes
            for(int i=0; i<4; ++i) {
                sf::RectangleShape b({64.f, 64.f});
                b.setFillColor(sf::Color(80,80,80));
                b.setPosition({tBox.getPosition().x + 50.f + i * 70.f, tBox.getPosition().y + 200.f});
                window.draw(b);

                if (npcTradeSlots[i].tex != nullptr) {
                    sf::Sprite s(*npcTradeSlots[i].tex);
                    s.setPosition({ b.getPosition().x + 8.f, b.getPosition().y + 8.f });
                    sf::Vector2u sz = s.getTexture().getSize();
                    s.setScale({ 48.f / sz.x, 48.f / sz.y });
                    window.draw(s);
                    
                    sf::Text cText(uiFont, std::to_string(npcTradeSlots[i].count), 16);
                    cText.setPosition({ b.getPosition().x + 40.f, b.getPosition().y + 40.f });
                    window.draw(cText);
                }

                if (tradeBoxesTicked) {
                    sf::Text tick(uiFont, "V", 30);
                    tick.setFillColor(sf::Color::Green);
                    tick.setPosition({ b.getPosition().x + 20.f, b.getPosition().y + 10.f });
                    window.draw(tick);
                }
            }

            sf::RectangleShape aBtn({100.f, 40.f});
            aBtn.setPosition({ tBox.getPosition().x + tBox.getGlobalBounds().size.x - 120.f, 
                               tBox.getPosition().y + tBox.getGlobalBounds().size.y - 60.f });
            aBtn.setFillColor(sf::Color::Green);
            window.draw(aBtn);

            sf::Text aTxt(uiFont, "Accept", 18);
            aTxt.setPosition({ aBtn.getPosition().x + 15.f, aBtn.getPosition().y + 10.f });
            window.draw(aTxt);
        }

        if (tradeConfirmOpen) {
            sf::RectangleShape cBox({300.f, 150.f});
            cBox.setPosition({ (width - 300.f) / 2.f, height / 2.f - 75.f });
            cBox.setFillColor(sf::Color(20,20,20, 240));
            cBox.setOutlineColor(sf::Color::White);
            cBox.setOutlineThickness(2.f);
            window.draw(cBox);

            sf::Text cTxt(uiFont, "Confirm Trade?", 20);
            cTxt.setPosition({ cBox.getPosition().x + 70.f, cBox.getPosition().y + 20.f });
            window.draw(cTxt);

            sf::RectangleShape yBtn({100.f, 40.f});
            yBtn.setPosition({ (width - 250.f) / 2.f, height / 2.f });
            yBtn.setFillColor(sf::Color::Green);
            window.draw(yBtn);

            sf::Text yTxt(uiFont, "Yes", 18);
            yTxt.setPosition({ yBtn.getPosition().x + 30.f, yBtn.getPosition().y + 10.f });
            window.draw(yTxt);

            sf::RectangleShape nBtn({100.f, 40.f});
            nBtn.setPosition({ (width + 50.f) / 2.f, height / 2.f });
            nBtn.setFillColor(sf::Color::Red);
            window.draw(nBtn);

            sf::Text nTxt(uiFont, "No", 18);
            nTxt.setPosition({ nBtn.getPosition().x + 35.f, nBtn.getPosition().y + 10.f });
            window.draw(nTxt);
        }

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
            window.setView(window.getDefaultView());

            float survivedSeconds = dayNightClock.getElapsedTime().asSeconds();
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

        sf::RectangleShape brightnessOverlay({(float)width, (float)height});
        brightnessOverlay.setFillColor(sf::Color(0, 0, 0, 255 - (globalSettings.brightness * 2.55f)));
        window.setView(window.getDefaultView());
        window.draw(brightnessOverlay);

        if (selectionDone) {
            if (chatFetchClock.getElapsedTime().asSeconds() > 2.0f) {
                chatMessages = authManager.fetchChatMessages(50);
                chatFetchClock.restart();
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
            window.setView(window.getDefaultView());
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
            } else {
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
            window.setView(window.getDefaultView());
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

        window.display();

        if (selectionDone && !itemSpawned)
        {
            const float MIN_DISTANCE = 120.f;

            // SPAWN NEAR PLAYER (Map Center)
            for (int i = 0; i < 80; ++i) {
                sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 16000 - 8000), MAP_HEIGHT / 2.f + (rand() % 16000 - 8000));
                if (!isInWater(p, pondBoundsList)) itemManager.addItem(ITEM_TREE, treeTex, p, 1);
            }
            for (int i = 0; i < 50; ++i) {
                sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 12000 - 6000), MAP_HEIGHT / 2.f + (rand() % 12000 - 6000));
                itemManager.addItem(ITEM_STONE, stoneTex, p, 1);
            }

            // GLOBAL SPAWNS (Thousands for 50k map)
            for (int i = 0; i < 4000; ++i)
            {
                sf::Vector2f p(randX(gen), randY(gen));
                if (!isInWater(p, pondBoundsList) && itemManager.isAreaFreeExplicit(p, MIN_DISTANCE, &treeTex))
                {
                    itemManager.addItem(ITEM_TREE, treeTex, p, 1);
                }
            }

            for (int i = 0; i < 3000; ++i)
            {
                sf::Vector2f p(randX(gen), randY(gen));
                if (itemManager.isAreaFreeExplicit(p, MIN_DISTANCE, &stoneTex))
                {
                    itemManager.addItem(ITEM_STONE, stoneTex, p, 1);
                }
            }

            itemSpawned = true;

            // --- LOCAL SPAWNS NEAR PLAYER FOR TUTORIAL ---
            for (int i = 0; i < 15; ++i) {
                sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 4000 - 2000), MAP_HEIGHT / 2.f + (rand() % 4000 - 2000));
                animals.push_back(std::make_unique<Chicken>(p));
            }
            for (int i = 0; i < 10; ++i) {
                sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 4000 - 2000), MAP_HEIGHT / 2.f + (rand() % 4000 - 2000));
                animals.push_back(std::make_unique<Pig>(p));
            }
            for (int i = 0; i < 5; ++i) {
                sf::Vector2f p(MAP_WIDTH / 2.f + (rand() % 4000 - 2000), MAP_HEIGHT / 2.f + (rand() % 4000 - 2000));
                monsters.push_back(std::make_unique<Zombie>(p));
            }

            // --- GLOBAL SPAWNS ---
            const int CHICKEN_COUNT = 200;
            const int PIG_COUNT = 100;

            for (int i = 0; i < CHICKEN_COUNT; ++i)
            {
                animals.push_back(std::make_unique<Chicken>(
                    sf::Vector2f(randX(gen), randY(gen))
                ));
            }

            for (int i = 0; i < PIG_COUNT; ++i)
            {
                animals.push_back(std::make_unique<Pig>(
                    sf::Vector2f(randX(gen), randY(gen))
                ));
            }
        }
    }
}
