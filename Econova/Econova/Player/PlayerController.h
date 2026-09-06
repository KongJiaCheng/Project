#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include "Animal.h"
#include "Monster.h"
#include "ItemManager.h"
#include "StructureSystem.h"
#include "Inventory.h"
#include "Animation.h"
#include "Settings.h"

struct TileFloor {
    sf::Vector2f pos;
    int floorLevel;
};

std::vector<TileFloor> computeElevatedTileFloors(const std::vector<PlacedStructure>& structures);

bool isPositionBlocked(
    sf::Vector2f targetPos,
    const std::vector<PlacedStructure>& structures,
    const std::vector<TileFloor>& elevatedTileFloors,
    const ItemManager& itemManager
);

bool isWorldPositionBlocked(
    sf::Vector2f pos,
    const std::vector<PlacedStructure>& structures,
    const std::vector<sf::Sprite>& ponds
);


void updateMonstersCombat(
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Sprite& playerSprite,
    float dt,
    const std::vector<PlacedStructure>& structures,
    const std::vector<TileFloor>& elevatedTileFloors,
    const ItemManager& itemManager,
    bool introActive,
    bool tutorialActive,
    bool& shieldEquipped,
    int eqShieldSlot,
    Inventory& inventory,
    float& hp,
    bool& isPlayerHurt,
    float& playerHurtTimer,
    unsigned int mapWidth,
    unsigned int mapHeight
);

void updateSaplings(
    std::vector<PlantedSapling>& plantedSaplings,
    ItemManager& itemManager,
    const sf::Texture& treeTex,
    float dt
);

struct DailyQuest;
struct QuestInfo;
class AuthManager;

void updateSurvivalStatsAndQuests(
    float dt,
    bool introActive,
    bool tutorialActive,
    bool isMenuOpen,
    bool spawnOverlayActive,
    float& hp,
    float maxHP,
    float& hunger,
    float maxHunger,
    int& hungerMultiplier,
    float& thirst,
    float maxThirst,
    int& thirstMultiplier,
    int& level,
    int& exp,
    QuestInfo& qInfo,
    const std::vector<DailyQuest>& availableQuests,
    int& currency,
    AuthManager& authManager,
    const std::string& username,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    bool& swordCrafting,
    float& swordCraftTimer,
    bool& shieldCrafting,
    float& shieldCraftTimer,
    Inventory& inventory,
    const sf::Texture& woodenSwordTex,
    const sf::Texture& shieldTex
);


struct NPC {
    std::optional<sf::Sprite> sprite;
    std::string name;
    sf::Texture texture;
    std::optional<sf::Sprite> wrenchBtn;
    NPC(const sf::Texture& tex) {
        sprite.emplace(tex);
        wrenchBtn.emplace(tex);
    }
};

struct GameAssets;
struct PlayerStats;
class AuthManager;

bool hydrateWorldAndPlayerData(
    sf::RenderWindow& window,
    unsigned int width,
    unsigned int height,
    int option,
    const std::string& username,
    bool dbStartupEnabled,
    AuthManager& authManager,
    const sf::Font& font,
    sf::Sprite& sprite,
    AnimSet& animWalk,
    AnimSet& animWalkWithSword,
    AnimSet& animWalkWithShield,
    AnimSet& animWalkWithShieldAndSword,
    AnimSet& animAttackWithSword,
    AnimSet& animAttackWithShieldAndSword,
    const int FRAMES_PER_DIR,
    const PlayerStats& stats,
    float& hunger,
    float& thirst,
    int& currency,
    int& level,
    int& playerExp,
    float& timeOffset,
    float& elapsedGameTime,
    int& hungerMultiplier,
    int& thirstMultiplier,
    sf::Text& hungerMultiplierText,
    sf::Text& thirstMultiplierText,
    std::vector<PlacedStructure>& structures,
    std::vector<PlacedCampfire>& campfires,
    Inventory& inventory,
    const GameAssets& assets,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc,
    NPC& npcFemale,
    NPC& npcMale,
    unsigned int mapWidth,
    unsigned int mapHeight
);

enum class Dir { South, North, East, West };

void handlePlayerMovementAndCombat(
    AnimSet& animWalk,
    AnimSet& animWalkWithSword,
    AnimSet& animWalkWithShield,
    AnimSet& animWalkWithShieldAndSword,
    AnimSet& animAttackWithSword,
    AnimSet& animAttackWithShieldAndSword,
    bool& swordEquipped,
    bool shieldEquipped,
    int eqSwordSlot,
    float& playerAttackCooldown,
    float dt,
    const GameSettings& globalSettings,
    bool isSleeping,
    sf::Sprite& sprite,
    Dir& currentDir,
    const std::function<bool(sf::Vector2f)>& isBlocked,
    bool tutorialActive,
    int tutorialStep,
    float& tutMoveDist,
    bool& tutSprinted,
    sf::Vector2f& tutLastPos,
    bool& tutAttacked,
    int& tutWoodGathered,
    std::vector<std::unique_ptr<Animal>>& animals,
    ItemManager& itemManager,
    std::vector<std::unique_ptr<Monster>>& monsters,
    std::vector<PlacedStructure>& structures,
    Inventory& inventory,
    int& playerExp
);

void updateWorldEntitiesAndDrops(
    std::vector<std::unique_ptr<Animal>>& animals,
    std::vector<std::unique_ptr<Monster>>& monsters,
    std::vector<PlacedStructure>& structures,
    ItemManager& itemManager,
    sf::Sprite& sprite,
    float dt,
    const std::function<bool(sf::Vector2f)>& isBlocked,
    unsigned int mapWidth,
    unsigned int mapHeight,
    const sf::Texture& woolTex,
    const sf::Texture& woodTex,
    const sf::Texture& saplingTex,
    const sf::Texture& stoneTex,
    bool tutorialActive,
    int tutorialStep,
    int& tutStoneGathered,
    QuestInfo& qInfo,
    AuthManager& authManager,
    const std::string& username,
    const std::function<void()>& syncSaveFunc,
    Inventory& inventory,
    bool nearPond,
    float& thirst,
    int& thirstMultiplier,
    float maxThirst,
    const std::vector<PlacedCampfire>& campfires,
    const std::function<sf::Vector2f(sf::Vector2f)>& snapGridFunc,
    const GameSettings& globalSettings,
    const sf::Font& font,
    sf::RenderWindow& window
);
