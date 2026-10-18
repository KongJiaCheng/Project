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
    const sf::FloatRect& cameraBounds,
    float margin
) {
    sf::FloatRect despawnBounds(
        { cameraBounds.position.x - margin, cameraBounds.position.y - margin },
        { cameraBounds.size.x + margin * 2.f, cameraBounds.size.y + margin * 2.f }
    );

    animals.erase(
        std::remove_if(
            animals.begin(),
            animals.end(),
            [&](const std::unique_ptr<Animal>& animal) {
                return !despawnBounds.contains(animal->getPosition());
            }),
        animals.end()
    );

    monsters.erase(
        std::remove_if(
            monsters.begin(),
            monsters.end(),
            [&](const std::unique_ptr<Monster>& monster) {
                return !despawnBounds.contains(monster->getPosition());
            }),
        monsters.end()
    );
}

void spawnNearbyAnimals(
    std::vector<std::unique_ptr<Animal>>& animals,
    sf::Vector2f playerPos,
    sf::Clock& mobSpawnClock,
    int maxAnimals,
    const sf::FloatRect& cameraBounds,
    int mapWidth,
    int mapHeight,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
) {
    // Hard cap at 6 animals or less in each place within the camera
    maxAnimals = std::min(maxAnimals, 6);

    // Count animals currently inside the camera bounds
    int animalsInCamera = 0;
    for (const auto& a : animals) {
        if (cameraBounds.contains(a->getPosition())) {
            animalsInCamera++;
        }
    }
    if (animalsInCamera >= maxAnimals || (int)animals.size() >= maxAnimals) {
        return;
    }

    // Natural spawn pacing: give breathing room so animals don't all appear at once
    float spawnCooldown = (animalsInCamera == 0) ? 0.6f : 1.8f;
    if (mobSpawnClock.getElapsedTime().asSeconds() < spawnCooldown) {
        return;
    }

    const float margin = 80.f;
    float spawnAreaW = cameraBounds.size.x - margin * 2.f;
    float spawnAreaH = cameraBounds.size.y - margin * 2.f;
    if (spawnAreaW <= 0.f || spawnAreaH <= 0.f) return;

    bool foundPos = false;
    sf::Vector2f spawnPos;

    // Up to 16 attempts to find a well-distributed, non-concentrated spawn location
    for (int attempt = 0; attempt < 16; ++attempt) {
        float rx = cameraBounds.position.x + margin + (static_cast<float>(rand()) / RAND_MAX) * spawnAreaW;
        float ry = cameraBounds.position.y + margin + (static_cast<float>(rand()) / RAND_MAX) * spawnAreaH;

        rx = std::clamp(rx, 0.f, (float)mapWidth);
        ry = std::clamp(ry, 0.f, (float)mapHeight);

        sf::Vector2f candidate(rx, ry);

        if (!cameraBounds.contains(candidate)) continue;

        // 1. Don't spawn immediately near player (>= 250px) or spawn center (>= 450px)
        if (getDistanceSq(candidate, playerPos) < 250.f * 250.f) continue;
        sf::Vector2f mapSpawnCenter(mapWidth / 2.f, mapHeight / 2.f);
        if (getDistanceSq(candidate, mapSpawnCenter) < 450.f * 450.f) continue;

        // 2. Anti-concentration: Don't spawn close to another animal (minimum 280px separation)
        bool tooClose = false;
        for (const auto& a : animals) {
            if (getDistanceSq(candidate, a->getPosition()) < 280.f * 280.f) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;

        // 3. Local density limit: within 500px radius, no more than 1 other animal
        int localNearbyCount = 0;
        for (const auto& a : animals) {
            if (getDistanceSq(candidate, a->getPosition()) < 500.f * 500.f) {
                localNearbyCount++;
            }
        }
        if (localNearbyCount >= 2) continue;

        // 4. Avoid structures and campfires
        if (!structures.empty() || !campfires.empty()) {
            if (isPosNearStructureOrHouse(candidate, structures, campfires, 400.f)) {
                continue;
            }
        }

        spawnPos = candidate;
        foundPos = true;
        break;
    }

    if (foundPos) {
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

void spawnNightZombies(
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    sf::Clock& monsterSpawnClock,
    int maxMonsters,
    const sf::FloatRect& cameraBounds,
    int mapWidth,
    int mapHeight,
    bool isNight,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
) {
    if (!isNight) return;

    // Hard cap at 6 monsters or less within camera
    maxMonsters = std::min(maxMonsters, 6);

    int monstersInCamera = 0;
    for (const auto& m : monsters) {
        if (cameraBounds.contains(m->getPosition())) {
            monstersInCamera++;
        }
    }
    if (monstersInCamera >= maxMonsters || (int)monsters.size() >= maxMonsters) {
        return;
    }

    float spawnCooldown = (monstersInCamera == 0) ? 1.0f : 2.5f;
    if (monsterSpawnClock.getElapsedTime().asSeconds() < spawnCooldown) {
        return;
    }

    const float margin = 80.f;
    float spawnAreaW = cameraBounds.size.x - margin * 2.f;
    float spawnAreaH = cameraBounds.size.y - margin * 2.f;
    if (spawnAreaW <= 0.f || spawnAreaH <= 0.f) return;

    bool foundPos = false;
    sf::Vector2f spawnPos;

    for (int attempt = 0; attempt < 16; ++attempt) {
        float rx = cameraBounds.position.x + margin + (static_cast<float>(rand()) / RAND_MAX) * spawnAreaW;
        float ry = cameraBounds.position.y + margin + (static_cast<float>(rand()) / RAND_MAX) * spawnAreaH;

        rx = std::clamp(rx, 0.f, (float)mapWidth);
        ry = std::clamp(ry, 0.f, (float)mapHeight);

        sf::Vector2f candidate(rx, ry);

        if (!cameraBounds.contains(candidate)) continue;

        // Keep 300px away from player so they don't immediately attack on spawn, and keep 450px away from spawn center
        if (getDistanceSq(candidate, playerPos) < 300.f * 300.f) continue;
        sf::Vector2f mapSpawnCenter(mapWidth / 2.f, mapHeight / 2.f);
        if (getDistanceSq(candidate, mapSpawnCenter) < 450.f * 450.f) continue;

        // Don't spawn right on top of another monster
        bool tooClose = false;
        for (const auto& m : monsters) {
            if (getDistanceSq(candidate, m->getPosition()) < 250.f * 250.f) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;

        // Local cluster density limit: within 500px radius, no more than 1 other monster
        int localNearbyCount = 0;
        for (const auto& m : monsters) {
            if (getDistanceSq(candidate, m->getPosition()) < 500.f * 500.f) {
                localNearbyCount++;
            }
        }
        if (localNearbyCount >= 2) continue;

        // Avoid structures and campfires
        if (!structures.empty() || !campfires.empty()) {
            if (isPosNearStructureOrHouse(candidate, structures, campfires, 400.f)) {
                continue;
            }
        }

        spawnPos = candidate;
        foundPos = true;
        break;
    }

    if (foundPos) {
        monsters.push_back(std::make_unique<Zombie>(spawnPos));
    }

    monsterSpawnClock.restart();
}

void updateNearbyEntities(
    std::vector<std::unique_ptr<Animal>>& animals,
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Vector2f playerPos,
    float dt,
    std::function<bool(sf::Vector2f)> isBlocked,
    const sf::FloatRect& cameraBounds
) {
    for (auto& animal : animals) {
        if (cameraBounds.contains(animal->getPosition())) {
            animal->update(dt, playerPos, isBlocked);
        }
    }

    for (auto& monster : monsters) {
        if (cameraBounds.contains(monster->getPosition())) {
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

    const float MIN_DISTANCE = 120.f;
    const int ITEM_ATTEMPTS_BUDGET = 250;
    const int MOB_BUDGET = 15;

    int itemAttempts = 0;
    int mobCount = 0;

    std::uniform_real_distribution<float> randX(0.f, static_cast<float>(mapWidth));
    std::uniform_real_distribution<float> randY(0.f, static_cast<float>(mapHeight));

    sf::Vector2f spawnCenter(mapWidth / 2.f, mapHeight / 2.f);
    const float SPAWN_CLEARANCE_SQ = 450.f * 450.f;

    auto isNearPlaceable = [&](sf::Vector2f pos) -> bool {
        return isPosNearStructureOrHouse(pos, structures, campfires, 500.f);
    };

    auto trySpawnNear = [&](int itemId, sf::Texture& tex, int& done, int total, int radius) {
        while (done < total && itemAttempts < ITEM_ATTEMPTS_BUDGET) {
            ++itemAttempts;
            sf::Vector2f p(
                mapWidth / 2.f + (rand() % (radius * 2) - radius),
                mapHeight / 2.f + (rand() % (radius * 2) - radius)
            );
            p = snapGridFunc(p);
            bool placed = false;
            for (int attempt = 0; attempt < 30; ++attempt) {
                bool insideSpawn = (getDistanceSq(p, spawnCenter) < SPAWN_CLEARANCE_SQ);
                bool blocked = insideSpawn || isNearPlaceable(p) || (itemId == ITEM_TREE && isInWater(p, pondBoundsList));
                if (!blocked && itemManager.isAreaFreeExplicit(p, 120.f, &tex)) {
                    itemManager.addItem(itemId, tex, p, 1);
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
            // Always increment done so the plan can complete even if no valid spot found
            ++done;
            (void)placed;
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
                if (getDistanceSq(p, spawnCenter) < SPAWN_CLEARANCE_SQ) {
                    continue;
                }
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
            if (spotFound) {
                itemManager.addItem(itemId, tex, p, 1);
            }
            // Always increment done so the plan can complete
            ++done;
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



    auto findDispersedSpawnPos = [&](bool isMonster) -> std::pair<bool, sf::Vector2f> {
        for (int attempt = 0; attempt < 30; ++attempt) {
            float angle = (static_cast<float>(rand()) / RAND_MAX) * 2.f * 3.14159265f;
            float minDist = isMonster ? 450.f : 300.f;
            float maxDist = 1800.f;
            float dist = minDist + (static_cast<float>(rand()) / RAND_MAX) * (maxDist - minDist);

            sf::Vector2f candidate(
                spawnCenter.x + std::cos(angle) * dist,
                spawnCenter.y + std::sin(angle) * dist
            );

            if (candidate.x < 100.f || candidate.x > static_cast<float>(mapWidth) - 100.f ||
                candidate.y < 100.f || candidate.y > static_cast<float>(mapHeight) - 100.f)
                continue;

            if (isInWater(candidate, pondBoundsList)) continue;
            if (isNearPlaceable(candidate)) continue;
            if (getDistanceSq(candidate, spawnCenter) < minDist * minDist) continue;

            // Enforce minimum distance from any existing animal (>= 280px)
            bool tooClose = false;
            for (const auto& a : animals) {
                if (getDistanceSq(candidate, a->getPosition()) < 280.f * 280.f) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;

            // Enforce minimum distance from any existing monster (>= 250px)
            for (const auto& m : monsters) {
                if (getDistanceSq(candidate, m->getPosition()) < 250.f * 250.f) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;

            // Cluster density check: no more than 1 other entity within 500px radius
            int localCount = 0;
            for (const auto& a : animals) {
                if (getDistanceSq(candidate, a->getPosition()) < 500.f * 500.f) localCount++;
            }
            for (const auto& m : monsters) {
                if (getDistanceSq(candidate, m->getPosition()) < 500.f * 500.f) localCount++;
            }
            if (localCount >= 2) continue;

            return { true, candidate };
        }
        return { false, sf::Vector2f(0.f, 0.f) };
    };

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
            if (animals.size() < 6) {
                auto res = findDispersedSpawnPos(false);
                if (res.first) {
                    animals.push_back(std::make_unique<Chicken>(res.second));
                }
            }
            ++spawnPlan.localChickensDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.localPigsDone < spawnPlan.localPigsTotal) {
            if (animals.size() < 6) {
                auto res = findDispersedSpawnPos(false);
                if (res.first) {
                    animals.push_back(std::make_unique<Pig>(res.second));
                }
            }
            ++spawnPlan.localPigsDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.localSheepsDone < spawnPlan.localSheepsTotal) {
            if (animals.size() < 6) {
                auto res = findDispersedSpawnPos(false);
                if (res.first) {
                    animals.push_back(std::make_unique<Sheep>(res.second));
                }
            }
            ++spawnPlan.localSheepsDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.localZombiesDone < spawnPlan.localZombiesTotal) {
            if (monsters.size() < 6) {
                auto res = findDispersedSpawnPos(true);
                if (res.first) {
                    monsters.push_back(std::make_unique<Zombie>(res.second));
                }
            }
            ++spawnPlan.localZombiesDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.globalChickensDone < spawnPlan.globalChickensTotal) {
            if (animals.size() < 6) {
                auto res = findDispersedSpawnPos(false);
                if (res.first) {
                    animals.push_back(std::make_unique<Chicken>(res.second));
                }
            }
            ++spawnPlan.globalChickensDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.globalPigsDone < spawnPlan.globalPigsTotal) {
            if (animals.size() < 6) {
                auto res = findDispersedSpawnPos(false);
                if (res.first) {
                    animals.push_back(std::make_unique<Pig>(res.second));
                }
            }
            ++spawnPlan.globalPigsDone;
            ++mobCount;
            continue;
        }
        if (spawnPlan.globalSheepsDone < spawnPlan.globalSheepsTotal) {
            if (animals.size() < 6) {
                auto res = findDispersedSpawnPos(false);
                if (res.first) {
                    animals.push_back(std::make_unique<Sheep>(res.second));
                }
            }
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
        clearItemsNearStructures(itemManager, structures, campfires, 500.f);
    }
}
