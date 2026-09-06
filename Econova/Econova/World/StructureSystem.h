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
};

struct PlantedSapling {
    sf::Vector2f position;
    float growTimer = 0.f;
    bool grown = false;
    float rotation = 0.f;
};

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
        else if (id == ITEM_CHEST) maxHealth = 150.f;
        health = maxHealth;
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

void drawQuantityPrompt(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    bool isCampfire,
    int campfireSlot,
    int maxAmount,
    TextInput& quantityInput
);

void drawTradeConfirmPopup(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height
);

void applyStructureScale(
    sf::Sprite& spr,
    int id,
    float rotation,
    bool isOpen,
    float& outShearY
);

class Inventory;
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



