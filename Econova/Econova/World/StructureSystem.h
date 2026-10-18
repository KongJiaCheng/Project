#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <optional>
#include "TextInput.h"
#include "AuthManager.h"
#include "ItemID.h"

struct PlacedCampfire {
    sf::Vector2f position;
    float timeRemaining = 0.f;
    bool isCrafted = false;
    int currentFrame = 0;
    float frameTimer = 0.f;
    int fuel = 0;
    int inputItem = 0;
    int inputCount = 0;
    int outputItem = 0;
    int outputCount = 0;
    bool menuOpen = false;
    std::string owner = "";
    float health, maxHealth;
    bool isHurt = false;
    float hurtTimer = 0.f;
    std::optional < sf::Sprite> sprite;
    int id;
    PlacedCampfire()
        : id(0), owner(""),
          fuel(0), inputItem(0), inputCount(0), outputItem(0), outputCount(0),
          timeRemaining(0.f), menuOpen(false), health(100.f), maxHealth(100.f) {}

    PlacedCampfire(int i, const sf::Texture& tex, float rot = 0.f, bool open = false, float sh = 0.f, const std::string& own = "")
        : id(i), owner(own),
        fuel(0), inputItem(0), inputCount(0), outputItem(0), outputCount(0), timeRemaining(0.f), menuOpen(false) {
        sprite.emplace(tex);
        maxHealth = 100.f;
        if (id == ITEM_CAMPFIRE) maxHealth = 150.f;
        health = maxHealth;
    }

};

struct PlantedSapling {
    sf::Vector2f position;
    float growTimer = 0.f;
    bool grown = false;
    float rotation = 0.f;
};


// A single slot in a placed chest
struct ChestSlot {
    int id = 0;
    int count = 0;
    const sf::Texture* tex = nullptr;  // non-owning pointer; valid as long as game assets live
};

static constexpr int CHEST_SLOT_COUNT = 20;

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
    int fuel = 0;
    int inputItem = 0;
    int inputCount = 0;
    int outputItem = 0;
    int outputCount = 0;
    float timeRemaining = 0.f;
    bool menuOpen = false;
    sf::Vector2f position;
    ChestSlot chestSlots[CHEST_SLOT_COUNT] = {};   // chest storage slots

    PlacedStructure(int i, const sf::Texture& tex, float rot = 0.f, bool open = false, float sh = 0.f, const std::string& own = "")
        : id(i), rotation(rot), isOpen(open), shearY(sh), owner(own),
          fuel(0), inputItem(0), inputCount(0), outputItem(0), outputCount(0), timeRemaining(0.f), menuOpen(false) {
        sprite.emplace(tex);
        maxHealth = 100.f;
        if (id == ITEM_WOOD_WALL) maxHealth = 200.f;
        else if (id == ITEM_FURNACE) maxHealth = 150.f;
        else if (id == ITEM_BED) maxHealth = 120.f;
        else if (id == ITEM_CHEST) maxHealth = 150.f;
        health = maxHealth;
        for (auto& slot : chestSlots) { slot.id = 0; slot.count = 0; slot.tex = nullptr; }
    }
};

void syncSaveStructuresAndCampfires(
    AuthManager& authManager,
    const std::string& username,
    bool dbStartupEnabled,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
);

void updateCampfiresSimulation(
    std::vector<PlacedCampfire>& campfires,
    float dt
);

void drawCampfireMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    PlacedCampfire& fire,
    const sf::Texture& woodTex,
    const sf::Texture& cookedChickenTex,
    const sf::Texture& cookedPigTex,
    const sf::Texture& cookedSheepTex
);

void drawFurnaceMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    PlacedStructure& furnace,
    const sf::Texture& woodTex,
    const sf::Texture& cookedChickenTex,
    const sf::Texture& cookedPigTex,
    const sf::Texture& cookedSheepTex,
    std::function<const sf::Texture*(int id)> getItemTexture = nullptr
);

class Inventory;

void drawChestMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    PlacedStructure& chest,
    Inventory& inventory,
    int chestSelectedInvSlot,
    std::function<const sf::Texture*(int id)> getItemTexture
);

void drawQuantityPrompt(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    bool isCampfire,
    int campfireSlot,
    int maxAmount,
    TextInput& quantityInput,
    bool isChest = false,
    int chestSlotIdx = -1,
    bool isChestRetrieve = false
);

void drawTradeConfirmPopup(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::string& warningMessage = ""
);

void applyStructureScale(
    sf::Sprite& spr,
    int id,
    float rotation,
    bool isOpen,
    float& outShearY
);

struct QuestInfo;

bool handleSaplingPlacementClicks(
    sf::Vector2f uiPos,
    sf::Vector2f worldPos,
    unsigned int width,
    unsigned int height,
    bool& saplingPlacementMode,
    sf::Vector2f& saplingPlacementPos,
    float& saplingRotation,
    bool& saplingFollowingCursor,
    int saplingFromInventorySlot,
    const sf::Texture& saplingTex,
    std::vector<PlantedSapling>& plantedSaplings,
    Inventory& inventory,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc
);

bool handleStructurePlacementClicks(
    sf::Vector2f uiPos,
    sf::Vector2f worldPos,
    unsigned int width,
    unsigned int height,
    bool& placementMode,
    int placementItemID,
    int placementFromInventorySlot,
    sf::Vector2f& placementPos,
    float& placementRotation,
    bool& placementFollowingCursor,
    const std::string& username,
    std::vector<PlacedCampfire>& campfires,
    std::vector<PlacedStructure>& structures,
    Inventory& inventory,
    QuestInfo& qInfo,
    bool tutorialActive,
    AuthManager& authManager,
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Texture& chestTex,
    std::function<void()> syncSaveCallback,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc
);

bool handleWorldInteractionClicks(
    sf::Vector2f worldPos,
    Inventory& inventory,
    std::vector<PlacedCampfire>& campfires,
    const std::string& username,
    std::vector<PlacedStructure>& structures,
    const sf::Sprite& playerSprite,
    int playerFloorLevel,
    bool& isSleeping,
    float& sleepFadeTimer,
    bool& sleepTimeSkipped,
    bool& chestMenuOpen,
    PlacedStructure*& activeChest,
    std::function<void()> syncSaveFunc
);

void updateCampfiresAndCooking(
    std::vector<PlacedCampfire>& campfires,
    std::vector<PlacedStructure>& structures,
    float dt,
    std::function<void()> syncSaveCallback
);

void updateAndDrawPlacementPreview(
    sf::RenderWindow& window,
    bool placementMode,
    bool placementFollowingCursor,
    int placementItemID,
    float& placementRotation,
    sf::Vector2f& placementPos,
    sf::Vector2f worldPos,
    const std::vector<PlacedStructure>& structures,
    std::function<sf::Vector2f(sf::Vector2f)> snapToShearedGrid,
    const sf::Texture campfireFrames[8],
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Texture& chestTex
);



