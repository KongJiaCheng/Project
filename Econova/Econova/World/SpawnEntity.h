#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include <functional>
#include "Animal.h"
#include "Monster.h"
#include "ItemManager.h"
#include "StructureSystem.h"

inline bool isInWater(const sf::Vector2f& pos, const std::vector<sf::FloatRect>& waters)
{
    for (const auto& water : waters) {
        if (water.contains(pos)) return true;
    }
    return false;
}

void despawnFarEntities(
    std::vector<std::unique_ptr<Animal>>& animals,
    std::vector<std::unique_ptr<Monster>>& monsters,
    const sf::FloatRect& cameraBounds,
    float margin = 600.f
);

void spawnNearbyAnimals(
    std::vector<std::unique_ptr<Animal>>& animals,
    sf::Vector2f playerPos,
    sf::Clock& mobSpawnClock,
    int maxAnimals,
    const sf::FloatRect& cameraBounds,
    int mapWidth,
    int mapHeight,
    const std::vector<PlacedStructure>& structures = {},
    const std::vector<PlacedCampfire>& campfires = {}
);

void spawnNightZombies(
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    sf::Clock& monsterSpawnClock,
    int maxMonsters,
    const sf::FloatRect& cameraBounds,
    int mapWidth,
    int mapHeight,
    bool isNight,
    const std::vector<PlacedStructure>& structures = {},
    const std::vector<PlacedCampfire>& campfires = {}
);

void updateNearbyEntities(
    std::vector<std::unique_ptr<Animal>>& animals,
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    float dt,
    std::function<bool(sf::Vector2f)> isBlocked,
    const sf::FloatRect& cameraBounds
);

#include <random>
#include <algorithm>

inline bool isPosNearStructureOrHouse(
    sf::Vector2f p,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires,
    float buffer = 500.f
) {
    for (const auto& s : structures) {
        if (!s.sprite.has_value()) continue;
        sf::Vector2f sPos = s.sprite->getPosition();
        // Envelope covering any orientation and origin (BL corner, center, sheared)
        float minX = sPos.x - 250.f;
        float maxX = sPos.x + 750.f;
        float minY = sPos.y - 500.f;
        float maxY = sPos.y + 100.f;

        float dx = 0.f;
        if (p.x < minX) dx = minX - p.x;
        else if (p.x > maxX) dx = p.x - maxX;

        float dy = 0.f;
        if (p.y < minY) dy = minY - p.y;
        else if (p.y > maxY) dy = p.y - maxY;

        if (dx * dx + dy * dy < buffer * buffer) {
            return true;
        }
    }

    for (const auto& c : campfires) {
        float minX = c.position.x - 250.f;
        float maxX = c.position.x + 250.f;
        float minY = c.position.y - 250.f;
        float maxY = c.position.y + 250.f;

        float dx = 0.f;
        if (p.x < minX) dx = minX - p.x;
        else if (p.x > maxX) dx = p.x - maxX;

        float dy = 0.f;
        if (p.y < minY) dy = minY - p.y;
        else if (p.y > maxY) dy = p.y - maxY;

        if (dx * dx + dy * dy < buffer * buffer) {
            return true;
        }
    }

    return false;
}

inline void clearItemsNearStructures(
    ItemManager& itemManager,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires,
    float buffer = 500.f
) {
    if (structures.empty() && campfires.empty()) return;
    auto& items = itemManager.getItems();
    size_t originalCount = items.size();
    items.erase(
        std::remove_if(items.begin(), items.end(), [&](const Item& it) {
            if (it.id == ITEM_TREE || it.id == ITEM_STONE) {
                return isPosNearStructureOrHouse(it.sprite.getPosition(), structures, campfires, buffer);
            }
            return false;
        }),
        items.end()
    );
    if (items.size() != originalCount) {
        itemManager.rebuildSpatialGrid();
    }
}

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

    // mobs: capped to 6 or less total animals at spawn point, 2 or less monsters
    int localChickensTotal = 2;
    int localPigsTotal = 2;
    int localSheepsTotal = 2;
    int localZombiesTotal = 2;
    int globalChickensTotal = 0;
    int globalPigsTotal = 0;
    int globalSheepsTotal = 0;

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

void runIncrementalWorldSpawnStep(
    SpawnPlan& spawnPlan,
    ItemManager& itemManager,
    sf::Texture& treeTex,
    sf::Texture& stoneTex,
    std::vector<std::unique_ptr<Animal>>& animals,
    std::vector<std::unique_ptr<Monster>>& monsters,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires,
    const std::vector<sf::FloatRect>& pondBoundsList,
    unsigned int mapWidth,
    unsigned int mapHeight,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc,
    std::mt19937& gen,
    bool& itemSpawned,
    bool& spawnOverlayActive,
    int& spawnOverlayDone,
    int& spawnOverlayTotal
);
