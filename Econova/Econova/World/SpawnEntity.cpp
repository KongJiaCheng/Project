#include "SpawnEntity.h"
#include "Chicken.h"
#include "Pig.h"
#include "Sheep.h"
#include "Zombie.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

static inline float getDistanceSq(const sf::Vector2f& a, const sf::Vector2f& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

void despawnFarEntities(
    std::vector<std::unique_ptr<Animal>>& animals,
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    float despawnRadius
) {
    const float DESPAWN_RADIUS_SQ = despawnRadius * despawnRadius;

    animals.erase(
        std::remove_if(
            animals.begin(),
            animals.end(),
            [&](const std::unique_ptr<Animal>& animal) {
                return getDistanceSq(animal->getPosition(), playerPos) > DESPAWN_RADIUS_SQ;
            }),
        animals.end()
    );

    monsters.erase(
        std::remove_if(
            monsters.begin(),
            monsters.end(),
            [&](const std::unique_ptr<Monster>& monster) {
                return getDistanceSq(monster->getPosition(), playerPos) > DESPAWN_RADIUS_SQ;
            }),
        monsters.end()
    );
}

void spawnNearbyAnimals(
    std::vector<std::unique_ptr<Animal>>& animals,
    sf::Vector2f playerPos,
    sf::Clock& mobSpawnClock,
    int maxAnimals,
    float spawnRadius,
    int mapWidth,
    int mapHeight
) {
    if (mobSpawnClock.getElapsedTime().asSeconds() > 0.15f) {
        if ((int)animals.size() < maxAnimals) {
            float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
            float distance = 500.f + static_cast<float>(rand()) / RAND_MAX * spawnRadius;

            sf::Vector2f spawnPos(
                playerPos.x + std::cos(angle) * distance,
                playerPos.y + std::sin(angle) * distance
            );

            spawnPos.x = std::clamp(spawnPos.x, 0.f, (float)mapWidth);
            spawnPos.y = std::clamp(spawnPos.y, 0.f, (float)mapHeight);

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
}

void spawnNightZombies(
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    sf::Clock& monsterSpawnClock,
    int maxMonsters,
    float spawnRadius,
    int mapWidth,
    int mapHeight,
    bool isNight
) {
    if (isNight && monsterSpawnClock.getElapsedTime().asSeconds() > 0.4f) {
        if ((int)monsters.size() < maxMonsters) {
            float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
            float distance = 800.f + static_cast<float>(rand()) / RAND_MAX * spawnRadius;

            sf::Vector2f spawnPos(
                playerPos.x + std::cos(angle) * distance,
                playerPos.y + std::sin(angle) * distance
            );

            spawnPos.x = std::clamp(spawnPos.x, 0.f, (float)mapWidth);
            spawnPos.y = std::clamp(spawnPos.y, 0.f, (float)mapHeight);

            monsters.push_back(std::make_unique<Zombie>(spawnPos));
        }
        monsterSpawnClock.restart();
    }
}

void updateNearbyEntities(
    std::vector<std::unique_ptr<Animal>>& animals,
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    float dt,
    std::function<bool(sf::Vector2f)> isBlocked,
    float updateRadius
) {
    const float UPDATE_RADIUS_SQ = updateRadius * updateRadius;

    for (auto& animal : animals) {
        if (getDistanceSq(animal->getPosition(), playerPos) < UPDATE_RADIUS_SQ) {
            animal->update(dt, playerPos, isBlocked);
        }
    }

    for (auto& monster : monsters) {
        if (getDistanceSq(monster->getPosition(), playerPos) < UPDATE_RADIUS_SQ) {
            monster->update(dt, playerPos, isBlocked);
        }
    }
}

#include "ItemID.h"
#include <iostream>
#include <sstream>

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
) {
    if (!spawnPlan.active || spawnPlan.done) return;

    const float MIN_DISTANCE = 300.f;
    const int ITEM_ATTEMPTS_BUDGET = 250;
    const int MOB_BUDGET = 15;

    int itemAttempts = 0;
    int mobCount = 0;

    std::uniform_real_distribution<float> randX(0.f, static_cast<float>(mapWidth));
    std::uniform_real_distribution<float> randY(0.f, static_cast<float>(mapHeight));

    auto isNearPlaceable = [&](sf::Vector2f pos) -> bool {
        const float threshold = 300.f;
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
                mapWidth / 2.f + (rand() % (radius * 2) - radius),
                mapHeight / 2.f + (rand() % (radius * 2) - radius)
            );
            p = snapGridFunc(p);
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
                        mapWidth / 2.f + (rand() % (radius * 2) - radius),
                        mapHeight / 2.f + (rand() % (radius * 2) - radius)
                    );
                    p = snapGridFunc(p);
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
                        mapWidth / 2.f + (rand() % (radius * 2) - radius),
                        mapHeight / 2.f + (rand() % (radius * 2) - radius)
                    );
                    p = snapGridFunc(p);
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
            p = snapGridFunc(p);
            bool spotFound = false;
            for (int attempt = 0; attempt < 30; ++attempt) {
                ++itemAttempts;
                p = sf::Vector2f(randX(gen), randY(gen));
                p = snapGridFunc(p);
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
                    p = snapGridFunc(p);
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

    if (spawnPlan.nearTreesDone < spawnPlan.nearTreesTotal)
        trySpawnNear(ITEM_TREE, treeTex, spawnPlan.nearTreesDone, spawnPlan.nearTreesTotal, 8000);

    if (spawnPlan.nearStonesDone < spawnPlan.nearStonesTotal)
        trySpawnNear(ITEM_STONE, stoneTex, spawnPlan.nearStonesDone, spawnPlan.nearStonesTotal, 6000);

    if (spawnPlan.globalTreesDone < spawnPlan.globalTreesTotal)
        trySpawnGlobal(ITEM_TREE, treeTex, spawnPlan.globalTreesDone, spawnPlan.globalTreesTotal);

    if (spawnPlan.globalStonesDone < spawnPlan.globalStonesTotal)
        trySpawnGlobal(ITEM_STONE, stoneTex, spawnPlan.globalStonesDone, spawnPlan.globalStonesTotal);

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
            sf::Vector2f p(mapWidth / 2.f + (rand() % 4000 - 2000), mapHeight / 2.f + (rand() % 4000 - 2000));
            animals.push_back(std::make_unique<Chicken>(p));
            ++spawnPlan.localChickensDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.localPigsDone < spawnPlan.localPigsTotal) {
            sf::Vector2f p(mapWidth / 2.f + (rand() % 4000 - 2000), mapHeight / 2.f + (rand() % 4000 - 2000));
            animals.push_back(std::make_unique<Pig>(p));
            ++spawnPlan.localPigsDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.localSheepsDone < spawnPlan.localSheepsTotal) {
            sf::Vector2f p(mapWidth / 2.f + (rand() % 4000 - 2000), mapHeight / 2.f + (rand() % 4000 - 2000));
            animals.push_back(std::make_unique<Sheep>(p));
            ++spawnPlan.localSheepsDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.localZombiesDone < spawnPlan.localZombiesTotal) {
            sf::Vector2f p(mapWidth / 2.f + (rand() % 4000 - 2000), mapHeight / 2.f + (rand() % 4000 - 2000));
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

    spawnOverlayActive = true;
    spawnOverlayTotal = spawnPlan.totalWork();
    spawnOverlayDone = spawnPlan.doneWork();

    if (spawnPlan.doneWork() >= spawnPlan.totalWork()) {
        spawnPlan.done = true;
        itemSpawned = true;
        spawnOverlayActive = false;
    }
}