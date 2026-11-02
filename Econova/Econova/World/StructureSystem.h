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

constexpr float FLOOR_HEIGHT = 500.f;

inline std::pair<sf::Vector2f, sf::Vector2f> getWallSideCells(sf::Vector2f pos, float rotation) {
    constexpr float CELL = 500.f, SX = 250.f, SY = 250.f;
    if (rotation == 90.f) {
        return { pos, sf::Vector2f(pos.x - CELL, pos.y) };
    } else if (rotation == 180.f) {
        return { sf::Vector2f(pos.x - CELL, pos.y), sf::Vector2f(pos.x - CELL - SX, pos.y + SY) };
    } else if (rotation == 270.f) {
        return { sf::Vector2f(pos.x - SX, pos.y + SY), sf::Vector2f(pos.x - CELL - SX, pos.y + SY) };
    } else { // 0.f
        return { pos, sf::Vector2f(pos.x - SX, pos.y + SY) };
    }
}

inline bool isStaircaseOpening(sf::Vector2f cellPos, int targetFloor, const std::vector<PlacedStructure>& structures) {
    if (targetFloor < 1) return false;
    for (const auto& s : structures) {
        if (s.id == ITEM_WOOD_STAIRS && s.sprite.has_value() && (int)s.shearY == targetFloor - 1) {
            if (std::abs(s.sprite->getPosition().x - cellPos.x) < 1.f &&
                std::abs(s.sprite->getPosition().y - cellPos.y) < 1.f) {
                return true;
            }
        }
    }
    return false;
}

inline std::vector<sf::Vector2f> getSupportedCellsForFloor(int targetFloor, const std::vector<PlacedStructure>& structures) {
    std::vector<sf::Vector2f> supported;
    if (targetFloor < 1) return supported;
    int lowerFloor = targetFloor - 1;

    auto addUnique = [&](sf::Vector2f p) {
        for (const auto& c : supported) {
            if (std::abs(c.x - p.x) < 1.f && std::abs(c.y - p.y) < 1.f) return;
        }
        supported.push_back(p);
    };

    for (const auto& s : structures) {
        if (!s.sprite.has_value() || (int)s.shearY != lowerFloor) continue;

        if (s.id == ITEM_WOOD_TILE) {
            addUnique(s.sprite->getPosition());
        } else if (s.id == ITEM_WOOD_STAIRS) {
            constexpr float SX = 250.f, SY = 250.f;
            addUnique(s.sprite->getPosition());
            addUnique(sf::Vector2f(s.sprite->getPosition().x + SX, s.sprite->getPosition().y - SY));
        } else if (s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR) {
            auto sides = getWallSideCells(s.sprite->getPosition(), s.rotation);
            addUnique(sides.first);
            addUnique(sides.second);
        }
    }
    return supported;
}

inline bool isCellSupportedOnFloor(sf::Vector2f cellPos, int targetFloor, const std::vector<PlacedStructure>& structures) {
    if (targetFloor < 1) return true;
    auto cells = getSupportedCellsForFloor(targetFloor, structures);
    for (const auto& c : cells) {
        if (std::abs(c.x - cellPos.x) < 1.f && std::abs(c.y - cellPos.y) < 1.f) return true;
    }
    return false;
}
inline bool isRoofSupported(sf::Vector2f cellPos, int targetFloor, const std::vector<PlacedStructure>& structures) {
    constexpr float CELL = 500.f, SX = 250.f, SY = 250.f;
    for (const auto& s : structures) {
        if (!s.sprite.has_value()) continue;
        int sf = (int)s.shearY;
        if (sf != targetFloor) continue;
        sf::Vector2f sp = s.sprite->getPosition();

        if (s.id == ITEM_WOOD_TILE) {
            if (std::abs(sp.x - cellPos.x) < 1.f && std::abs(sp.y - cellPos.y) < 1.f) return true;
        } else if (s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR) {
            if (std::abs(sp.x - cellPos.x) < 1.f && std::abs(sp.y - cellPos.y) < 1.f) return true;
            if (std::abs(sp.x - (cellPos.x + CELL)) < 1.f && std::abs(sp.y - cellPos.y) < 1.f) return true;
            if (std::abs(sp.x - (cellPos.x + SX)) < 1.f && std::abs(sp.y - (cellPos.y - SY)) < 1.f) return true;
            if (std::abs(sp.x - (cellPos.x + CELL + SX)) < 1.f && std::abs(sp.y - (cellPos.y - SY)) < 1.f) return true;
        }
    }
    if (targetFloor >= 1 && isCellSupportedOnFloor(cellPos, targetFloor, structures)) {
        return true;
    }
    return false;
}

class Inventory;

void syncSaveStructuresAndCampfires(
    AuthManager& authManager,
    const std::string& username,
    bool dbStartupEnabled,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires,
    Inventory* inventory = nullptr
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
    const sf::Texture& cookedSheepTex,
    std::function<const sf::Texture*(int id)> getItemTexture = nullptr
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
    bool isRetrieve = false
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
    float& placementFloor,
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
    float& placementFloor,
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



