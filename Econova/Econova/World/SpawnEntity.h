#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include <functional>
#include "Animal.h"
#include "Monster.h"

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
    sf::Vector2f playerPos,
    float despawnRadius
);

void spawnNearbyAnimals(
    std::vector<std::unique_ptr<Animal>>& animals,
    sf::Vector2f playerPos,
    sf::Clock& mobSpawnClock,
    int maxAnimals,
    float spawnRadius,
    int mapWidth,
    int mapHeight
);

void spawnNightZombies(
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    sf::Clock& monsterSpawnClock,
    int maxMonsters,
    float spawnRadius,
    int mapWidth,
    int mapHeight,
    bool isNight
);

void updateNearbyEntities(
    std::vector<std::unique_ptr<Animal>>& animals,
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    float dt,
    std::function<bool(sf::Vector2f)> isBlocked,
    float updateRadius
);

#include "ItemManager.h"
#include "StructureSystem.h"
#include <random>

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
