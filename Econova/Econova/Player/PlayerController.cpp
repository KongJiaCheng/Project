#include "PlayerController.h"
#include "Game.h"
#include "ItemID.h"
#include "SpawnEntity.h"
#include <cmath>
#include <queue>
#include <map>
#include <algorithm>

std::vector<TileFloor> computeElevatedTileFloors(const std::vector<PlacedStructure>& structures) {
    std::vector<TileFloor> elevatedTileFloors;
    for (const auto& s : structures) {
        if (!s.sprite.has_value()) continue;
        if (s.id == ITEM_WOOD_TILE && s.shearY >= 1.f) {
            elevatedTileFloors.push_back({ s.sprite->getPosition(), (int)s.shearY });
        }
    }
    return elevatedTileFloors;
}

bool isPositionBlocked(
    sf::Vector2f targetPos,
    const std::vector<PlacedStructure>& structures,
    const std::vector<TileFloor>& elevatedTileFloors,
    const ItemManager& itemManager
) {
    const float TILE_CONNECT_DIST = 260.f;
    sf::Vector2f targetFeet = targetPos + sf::Vector2f(0.f, 250.f);

    bool targetElevated = false;
    for (const auto& s : structures) {
        if (s.id == ITEM_WOOD_STAIRS && s.sprite.has_value()) {
            sf::Vector2f sPos = s.sprite->getPosition();
            const float margin = 150.f;
            float zoneTop    = sPos.y - 250.f - margin;
            float zoneBottom = sPos.y + margin;
            float zoneLeft   = sPos.x - margin;
            float zoneRight  = sPos.x + 500.f + margin;
            if (targetPos.y >= zoneTop && targetPos.y <= zoneBottom &&
                targetPos.x >= zoneLeft && targetPos.x <= zoneRight)
            { targetElevated = true; break; }
        }
    }
    if (!targetElevated) {
        for (const auto& etf : elevatedTileFloors) {
            if (targetFeet.y >= etf.pos.y - 250.f - 40.f && targetFeet.y <= etf.pos.y + 40.f) {
                float xShift = etf.pos.y - targetFeet.y;
                if (targetFeet.x >= etf.pos.x + xShift - 40.f && targetFeet.x <= etf.pos.x + 500.f + xShift + 40.f) {
                    targetElevated = true;
                    break;
                }
            }
        }
    }

    // 1. Structures (Walls, Furnace, Bed, Door)
    const float STRUCT_CULL_SQ = 600.f * 600.f;
    for (const auto& s : structures) {
        if (!s.sprite.has_value()) continue;
        sf::Vector2f sPos = s.sprite->getPosition();
        float sdx = sPos.x - targetPos.x;
        float sdy = sPos.y - targetPos.y;
        if (sdx * sdx + sdy * sdy > STRUCT_CULL_SQ) continue;

        if (s.id == ITEM_WOOD_WALL || (s.id == ITEM_WOOD_DOOR && !s.isOpen)) {
            if (targetElevated) continue;
            sf::Vector2f A = sPos;
            sf::Vector2f B = (s.rotation == 90.f)
                ? sf::Vector2f(sPos.x + 250.f, sPos.y - 250.f)
                : sf::Vector2f(sPos.x + 500.f, sPos.y);
            if (distanceToSegment(targetPos, A, B) < 40.f) return true;
        }
        else if (s.id == ITEM_FURNACE || s.id == ITEM_BED) {
            if (targetElevated) continue;
            sf::FloatRect b = s.sprite->getGlobalBounds();
            b.position.x += b.size.x * 0.2f; b.size.x *= 0.6f;
            b.position.y += b.size.y * 0.7f; b.size.y *= 0.3f;
            if (b.contains(targetPos)) return true;
        }
    }
    // 2. Resources (Trees, Stones)
    if (itemManager.isResourceBlocking(targetPos)) {
        return true;
    }
    return false;
}

bool isWorldPositionBlocked(
    sf::Vector2f pos,
    const std::vector<PlacedStructure>& structures,
    const std::vector<sf::Sprite>& ponds
) {
    const float BLOCK_CULL_SQ = 600.f * 600.f;

    for (const auto& structure : structures)
    {
        if (structure.sprite.has_value())
        {
            sf::Vector2f sc = structure.sprite->getPosition();
            float sdx = sc.x - pos.x, sdy = sc.y - pos.y;
            if (sdx * sdx + sdy * sdy > BLOCK_CULL_SQ) continue;

            if (structure.id == ITEM_WOOD_WALL || (structure.id == ITEM_WOOD_DOOR && !structure.isOpen)) {
                sf::Vector2f sPos = structure.sprite->getPosition();
                // Primary segment along the wall face
                sf::Vector2f A = sPos;
                sf::Vector2f B = (structure.rotation == 90.f)
                    ? sf::Vector2f(sPos.x + 250.f, sPos.y - 250.f)
                    : sf::Vector2f(sPos.x + 500.f, sPos.y);
                // Secondary segment: wall has depth — block entities approaching from either side
                sf::Vector2f A2(A.x, A.y - 80.f);
                sf::Vector2f B2(B.x, B.y - 80.f);
                if (distanceToSegment(pos, A, B) < 80.f ||
                    distanceToSegment(pos, A2, B2) < 80.f) return true;
            }
            else if (structure.id == ITEM_FURNACE || structure.id == ITEM_BED) {
                sf::FloatRect b = structure.sprite->getGlobalBounds();
                b.position.x += b.size.x * 0.2f; b.size.x *= 0.6f;
                b.position.y += b.size.y * 0.7f; b.size.y *= 0.3f;
                if (b.contains(pos)) return true;
            }
        }
    }

    for (const auto& pond : ponds)
    {
        sf::Vector2f pc = pond.getPosition();
        float pdx = pc.x - pos.x, pdy = pc.y - pos.y;
        if (pdx * pdx + pdy * pdy > BLOCK_CULL_SQ) continue;
        if (pond.getGlobalBounds().contains(pos))
            return true;
    }

    return false;
}


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
) {
    auto isBlocked = [&](sf::Vector2f targetPos) {
        return isPositionBlocked(targetPos, structures, elevatedTileFloors, itemManager);
    };

    for (auto& m : monsters) {
        // NOTE: m->update() is called by updateNearbyEntities in Game.cpp — do NOT call it again here
        // (double update caused doubled monster speed and CPU usage)

        if (m->health > 0) {
            sf::FloatRect z = m->sprite.getGlobalBounds();
            sf::FloatRect p = playerSprite.getGlobalBounds();

            sf::Vector2f zCenter(z.position.x + z.size.x * 0.5f, z.position.y + z.size.y * 0.5f);
            sf::Vector2f pCenter(p.position.x + p.size.x * 0.5f, p.position.y + p.size.y * 0.5f);
            float dx = zCenter.x - pCenter.x;
            float dy = zCenter.y - pCenter.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            bool hit = dist < 120.f;
            if (!introActive && hit) {
                if (m->attackTimer >= m->attackCooldown) {
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
                    playerHurtTimer = 0.8f;

                    sf::Vector2f pushDir = playerSprite.getPosition() - m->sprite.getPosition();
                    float len = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                    if (len != 0.f) pushDir /= len;
                    playerSprite.move(pushDir * 220.f);
                }
            }
        }

        sf::FloatRect bounds = m->sprite.getGlobalBounds();
        sf::Vector2f pos = m->sprite.getPosition();

        if (bounds.position.x < 0.f) pos.x -= bounds.position.x;
        if (bounds.position.y < 0.f) pos.y -= bounds.position.y;
        if (bounds.position.x + bounds.size.x > mapWidth) pos.x -= (bounds.position.x + bounds.size.x - mapWidth);
        if (bounds.position.y + bounds.size.y > mapHeight) pos.y -= (bounds.position.y + bounds.size.y - mapHeight);

        m->sprite.setPosition(pos);
    }
}

void updateSaplings(
    std::vector<PlantedSapling>& plantedSaplings,
    ItemManager& itemManager,
    const sf::Texture& treeTex,
    float dt,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
) {
    for (auto& s : plantedSaplings) {
        if (!s.grown) {
            s.growTimer -= dt;
            if (s.growTimer <= 0.f) {
                s.growTimer = 0.f;
                s.grown = true;
                // Don't grow a tree inside or next to any structure/campfire
                if (!isPosNearStructureOrHouse(s.position, structures, campfires, 500.f)) {
                    itemManager.addItem(ITEM_TREE, treeTex, s.position, 1);
                }
            }
        }
    }

    plantedSaplings.erase(
        std::remove_if(
            plantedSaplings.begin(),
            plantedSaplings.end(),
            [](const PlantedSapling& s) {
                return s.grown;
            }),
        plantedSaplings.end()
    );
}

#include "AuthManager.h"
#include "GameAssets.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>

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
) {
    if (!dbStartupEnabled) {
        return true;
    }

    if (option == 1) {
        constructMaleAnimation(animWalk, animWalkWithSword, animWalkWithShield, animWalkWithShieldAndSword, animAttackWithSword, animAttackWithShieldAndSword, FRAMES_PER_DIR);
    }
    else {
        constructFemaleAnimation(animWalk, animWalkWithSword, animWalkWithShield, animWalkWithShieldAndSword, animAttackWithSword, animAttackWithShieldAndSword, FRAMES_PER_DIR);
    }

    sprite.setTexture(animWalk.south.getCurrentTexture());
    sf::Vector2u ts_init = sprite.getTexture().getSize();
    if (ts_init.x > 0 && ts_init.y > 0) {
        sprite.setOrigin({ ts_init.x / 2.f, ts_init.y / 2.f });
        sprite.setScale({ 500.f / ts_init.x, 500.f / ts_init.y });
    }

    struct LoadedGameData {
        std::vector<std::pair<int, int>>  inventory;
        std::vector<StoredStructure>     structures;
        std::vector<StoredStructure>     otherStructures;
        std::vector<StoredCampfire>      campfires;
        std::vector<StoredCampfire>      otherCampfires;
    };
    struct SharedLoadState {
        LoadedGameData data;
        std::atomic<bool> done{ false };
        std::atomic<bool> failed{ false };
        std::mutex dataMutex;
    };
    auto loadState = std::make_shared<SharedLoadState>();
    std::string loadUser = username;
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

    bool loadTimedOut = false;
    {
        sf::Text loadTxt(font, "Loading world...", 60);
        loadTxt.setFillColor(sf::Color::White);
        loadTxt.setOutlineColor(sf::Color::Black);
        loadTxt.setOutlineThickness(3.f);
        sf::Clock loadDotClock;
        sf::Clock loadTimeoutClock;
        int loadDots = 0;
        const float LOAD_TIMEOUT = 30.0f;
        while (!loadState->done.load()) {
            if (loadTimeoutClock.getElapsedTime().asSeconds() > LOAD_TIMEOUT) {
                loadTimedOut = true;
                std::cerr << "[ERROR] Loading world timed out after " << LOAD_TIMEOUT << " seconds\n";
                break;
            }
            while (const std::optional ev = window.pollEvent()) {
                if (ev->is<sf::Event::Closed>()) {
                    window.close();
                    return false;
                }
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
    bool hasWrenchInSlot0 = (!loadedItems.empty() && loadedItems[0].first == ITEM_WRENCH && loadedItems[0].second > 0);
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
    playerExp = stats.exp;
    timeOffset = stats.elapsedTime;
    elapsedGameTime = timeOffset;
    hungerMultiplier = stats.hungerMultiplier;
    thirstMultiplier = stats.thirstMultiplier;
    hungerMultiplierText.setString("X" + std::to_string(hungerMultiplier));
    thirstMultiplierText.setString("X" + std::to_string(thirstMultiplier));

    // Apply Structures
    for (auto& s : loaded.structures) {
        const sf::Texture* tex = assets.getItemTexture(s.id);
        if (tex) {
            structures.emplace_back(s.id, *tex, s.rotation, s.isOpen, s.shearY, username);
            auto& newS = structures.back();
            newS.sprite->setPosition({ s.x, s.y });
            newS.health = s.health;
            sf::Vector2u ts = newS.sprite->getTexture().getSize();
            if (s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR || s.id == ITEM_WOOD_STAIRS) {
                newS.sprite->setOrigin({ 0.f, (float)ts.y });
            } else {
                newS.sprite->setOrigin({ ts.x / 2.f, (float)ts.y });
            }
            applyStructureScale(*newS.sprite, s.id, s.rotation, s.isOpen, newS.shearY);
        }
    }

    // Apply Other Players' Structures
    for (auto& s : loaded.otherStructures) {
        const sf::Texture* tex = assets.getItemTexture(s.id);
        if (tex) {
            structures.emplace_back(s.id, *tex, s.rotation, s.isOpen, s.shearY, s.owner.empty() ? "other" : s.owner);
            auto& newS = structures.back();
            newS.sprite->setPosition({ s.x, s.y });
            newS.health = s.health;
            sf::Vector2u ts = newS.sprite->getTexture().getSize();
            if (s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR || s.id == ITEM_WOOD_STAIRS) {
                newS.sprite->setOrigin({ 0.f, (float)ts.y });
            } else {
                newS.sprite->setOrigin({ ts.x / 2.f, (float)ts.y });
            }
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
        pc.owner = username;
        campfires.push_back(pc);
    }

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
        pc.owner = c.owner;
        campfires.push_back(pc);
    }

    for (int i = 0; i < 20 && i < static_cast<int>(loadedItems.size()); i++) {
        int itemId = loadedItems[i].first;
        int count = loadedItems[i].second;
        if (count > 0) {
            const sf::Texture* tex = assets.getItemTexture(itemId);
            if (tex) {
                inventory.setItem(i, itemId, *tex, count);
            }
        }
    }

    if (!inventory.hasItem(0) || inventory.getItemID(0) != ITEM_WRENCH) {
        inventory.setItem(0, ITEM_WRENCH, assets.wrenchTex, 1);
    }

    if (npcFemale.texture.loadFromFile(resolvePath("Sprites/NPCFemale/Walk/South/frame_000.png"))) {
        npcFemale.sprite->setTexture(npcFemale.texture);
        sf::Vector2u ts_npcF = npcFemale.texture.getSize();
        if (ts_npcF.x > 0 && ts_npcF.y > 0) {
            npcFemale.sprite->setOrigin({ ts_npcF.x / 2.f, ts_npcF.y / 2.f });
            npcFemale.sprite->setScale({ 500.f / ts_npcF.x, 500.f / ts_npcF.y });
        }
        npcFemale.sprite->setPosition({ mapWidth / 2.f + 150.f, mapHeight / 2.f - 50.f });
    }

    if (npcMale.texture.loadFromFile(resolvePath("Sprites/NPCMale/Walk/South/frame_000.png"))) {
        npcMale.sprite->setTexture(npcMale.texture);
        sf::Vector2u ts_npcM = npcMale.texture.getSize();
        if (ts_npcM.x > 0 && ts_npcM.y > 0) {
            npcMale.sprite->setOrigin({ ts_npcM.x / 2.f, ts_npcM.y / 2.f });
            npcMale.sprite->setScale({ 500.f / ts_npcM.x, 500.f / ts_npcM.y });
        }
        npcMale.sprite->setPosition({ mapWidth / 2.f - 150.f, mapHeight / 2.f - 50.f });
    }

    if (npcFemale.sprite->getPosition().x == 0) {
        npcFemale.sprite->setPosition({ mapWidth / 2.f + 150.f, mapHeight / 2.f - 50.f });
        npcMale.sprite->setPosition({ mapWidth / 2.f - 150.f, mapHeight / 2.f - 50.f });
    }

    return true;
}

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
    int& tutStoneGathered,
    std::vector<std::unique_ptr<Animal>>& animals,
    ItemManager& itemManager,
    std::vector<std::unique_ptr<Monster>>& monsters,
    std::vector<PlacedStructure>& structures,
    Inventory& inventory,
    int& playerExp,
    const std::string& username,
    std::vector<PlacedCampfire>* campfires
) {
    const float MOVE_SPEED = 250.f;
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

    if (sf::Keyboard::isKeyPressed(globalSettings.keySprint)) {
        currentSpeed *= 2.0f;
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
        if (moveUp) {
            sf::Vector2f nextPos = sprite.getPosition() + sf::Vector2f(0.f, -currentSpeed * dt);
            if (!isBlocked(nextPos)) sprite.move({ 0.f, -currentSpeed * dt });
            currentDir = Dir::North;
            currentAnimSet->setDirPlaying(1, true);
        }
        else if (moveDown) {
            sf::Vector2f nextPos = sprite.getPosition() + sf::Vector2f(0.f, currentSpeed * dt);
            if (!isBlocked(nextPos)) sprite.move({ 0.f, currentSpeed * dt });
            currentDir = Dir::South;
            currentAnimSet->setDirPlaying(0, true);
        }

        if (moveLeft) {
            sf::Vector2f nextPos = sprite.getPosition() + sf::Vector2f(-currentSpeed * dt, 0.f);
            if (!isBlocked(nextPos)) sprite.move({ -currentSpeed * dt, 0.f });
            currentDir = Dir::West;
            currentAnimSet->setDirPlaying(3, true);
        }
        else if (moveRight) {
            sf::Vector2f nextPos = sprite.getPosition() + sf::Vector2f(currentSpeed * dt, 0.f);
            if (!isBlocked(nextPos)) sprite.move({ currentSpeed * dt, 0.f });
            currentDir = Dir::East;
            currentAnimSet->setDirPlaying(2, true);
        }
    }
    else {
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

    if (!isSleeping && (sf::Keyboard::isKeyPressed(globalSettings.keyAttack) || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Space)) && playerAttackCooldown <= 0.f) {
        playerAttackCooldown = 0.3f;

        float damage = swordEquipped ? 50.f : 25.f;
        bool hitSomething = false;

        sf::FloatRect attackBox;
        sf::Vector2f playerPos = sprite.getPosition();

        if (swordEquipped) {
            // Sword sweep: generous 280x280 box centered on player (140px reach in all directions)
            const float swordReach = 140.f;
            attackBox = sf::FloatRect(
                { playerPos.x - swordReach, playerPos.y - swordReach },
                { swordReach * 2.f, swordReach * 2.f }
            );
        } else {
            // Bare hands: tight directional reach right in front of player (must approach item)
            const float forwardReach = 135.f;
            const float sideHalfWidth = 65.f;
            const float backPadding = 25.f; // Small overlap into player's center to avoid dead zones

            switch (currentDir) {
            case Dir::North: // Facing Up (-Y)
                attackBox = sf::FloatRect(
                    { playerPos.x - sideHalfWidth, playerPos.y - forwardReach },
                    { sideHalfWidth * 2.f, forwardReach + backPadding }
                );
                break;
            case Dir::South: // Facing Down (+Y)
                attackBox = sf::FloatRect(
                    { playerPos.x - sideHalfWidth, playerPos.y - backPadding },
                    { sideHalfWidth * 2.f, forwardReach + backPadding }
                );
                break;
            case Dir::East: // Facing Right (+X)
                attackBox = sf::FloatRect(
                    { playerPos.x - backPadding, playerPos.y - sideHalfWidth },
                    { forwardReach + backPadding, sideHalfWidth * 2.f }
                );
                break;
            case Dir::West: // Facing Left (-X)
                attackBox = sf::FloatRect(
                    { playerPos.x - forwardReach, playerPos.y - sideHalfWidth },
                    { forwardReach + backPadding, sideHalfWidth * 2.f }
                );
                break;
            }
        }

        auto checkOverlap = [&](const sf::FloatRect& b) {
            return attackBox.position.x < b.position.x + b.size.x &&
                   attackBox.position.x + attackBox.size.x > b.position.x &&
                   attackBox.position.y < b.position.y + b.size.y &&
                   attackBox.position.y + attackBox.size.y > b.position.y;
        };

        auto getDistSq = [&](sf::Vector2f targetPos) {
            float dx = targetPos.x - playerPos.x;
            float dy = targetPos.y - playerPos.y;
            return dx * dx + dy * dy;
        };

        // For bare hands: track the single closest item/entity approached in front of the player
        Animal* bestAnimal = nullptr;
        Item* bestTree = nullptr;
        Item* bestStone = nullptr;
        Monster* bestMonster = nullptr;
        PlacedStructure* bestStructure = nullptr;
        PlacedCampfire* bestCampfire = nullptr;
        float bestDist = 999999.f;

        // 1. Animals
        for (auto& a : animals) {
            if (a->health <= 0) continue;
            sf::FloatRect b = a->sprite.getGlobalBounds();
            if (checkOverlap(b)) {
                if (swordEquipped) {
                    if (tutorialActive && tutorialStep == 5) tutAttacked = true;
                    a->health -= damage;
                    a->isHurt = true;
                    a->hurtTimer = 0.15f;
                    hitSomething = true;
                    playerExp += 10;

                    sf::Vector2f dir = a->sprite.getPosition() - sprite.getPosition();
                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len != 0.f) dir /= len;
                    a->sprite.move(dir * 40.f);
                } else {
                    float d = getDistSq(a->sprite.getPosition());
                    if (d < bestDist) {
                        bestDist = d;
                        bestAnimal = a.get();
                        bestTree = nullptr; bestStone = nullptr; bestMonster = nullptr; bestStructure = nullptr; bestCampfire = nullptr;
                    }
                }
            }
        }

        // 2. Trees (checked by base trunk footprint on the ground)
        for (auto& tree : itemManager.getItems()) {
            if (tree.id != ITEM_TREE || tree.health <= 0) continue;

            sf::FloatRect tb = tree.sprite.getGlobalBounds();
            sf::FloatRect b(
                { tb.position.x + tb.size.x * 0.25f, tb.position.y + tb.size.y * 0.65f },
                { tb.size.x * 0.50f, tb.size.y * 0.35f }
            );
            sf::Vector2f treeCenter(b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f);

            if (checkOverlap(b)) {
                if (swordEquipped) {
                    if (tutorialActive && tutorialStep == 4) tutWoodGathered++;
                    tree.health -= damage;
                    tree.isHurt = true;
                    tree.hurtTimer = 0.15f;
                    hitSomething = true;
                    playerExp += 5;
                } else {
                    float d = getDistSq(treeCenter);
                    if (d < bestDist) {
                        bestDist = d;
                        bestTree = &tree;
                        bestAnimal = nullptr; bestStone = nullptr; bestMonster = nullptr; bestStructure = nullptr; bestCampfire = nullptr;
                    }
                }
            }
        }

        // 3. Stones (checked by physical rock footprint on the ground)
        for (auto& stone : itemManager.getItems()) {
            if (stone.id != ITEM_STONE || stone.health <= 0) continue;

            sf::FloatRect sb = stone.sprite.getGlobalBounds();
            sf::FloatRect b(
                { sb.position.x - 15.f, sb.position.y - 15.f },
                { sb.size.x + 30.f, sb.size.y + 30.f }
            );
            sf::Vector2f stoneCenter(sb.position.x + sb.size.x * 0.5f, sb.position.y + sb.size.y * 0.5f);

            if (checkOverlap(b)) {
                if (swordEquipped) {
                    if (tutorialActive && tutorialStep == 4) tutStoneGathered++;
                    stone.health -= damage;
                    stone.isHurt = true;
                    stone.hurtTimer = 0.15f;
                    hitSomething = true;
                    playerExp += 5;
                } else {
                    float d = getDistSq(stoneCenter);
                    if (d < bestDist) {
                        bestDist = d;
                        bestStone = &stone;
                        bestAnimal = nullptr; bestTree = nullptr; bestMonster = nullptr; bestStructure = nullptr; bestCampfire = nullptr;
                    }
                }
            }
        }

        // 4. Monsters
        for (auto& a : monsters) {
            if (a->health <= 0) continue;
            sf::FloatRect b = a->sprite.getGlobalBounds();
            if (checkOverlap(b)) {
                if (swordEquipped) {
                    if (tutorialActive && tutorialStep == 5) tutAttacked = true;
                    a->health -= damage;
                    a->isHurt = true;
                    a->hurtTimer = 0.15f;
                    hitSomething = true;
                    playerExp += 20;

                    sf::Vector2f dir = a->sprite.getPosition() - sprite.getPosition();
                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len != 0.f) dir /= len;
                    a->sprite.move(dir * 40.f);
                } else {
                    float d = getDistSq(a->sprite.getPosition());
                    if (d < bestDist) {
                        bestDist = d;
                        bestMonster = a.get();
                        bestAnimal = nullptr; bestTree = nullptr; bestStone = nullptr; bestStructure = nullptr; bestCampfire = nullptr;
                    }
                }
            }
        }

        int curSlot = inventory.getSelectedSlot();
        bool isHoldingWrench = (curSlot != -1 && inventory.hasItem(curSlot) && inventory.getItemID(curSlot) == ITEM_WRENCH);

        // 5. Structures (including wood tiles)
        for (auto& s : structures) {
            if (!s.sprite.has_value() || s.health <= 0) continue;
            sf::FloatRect b = s.sprite->getGlobalBounds();
            if (checkOverlap(b)) {
                if (!s.owner.empty() && s.owner != username) {
                    if (isHoldingWrench) {
                        inventory.showBanner("This belongs to " + s.owner + "! You cannot wrench other players' items.");
                    } else {
                        inventory.showBanner("This belongs to " + s.owner + "! You cannot destroy other players' items.");
                    }
                    continue;
                }
                if (swordEquipped) {
                    s.health -= damage;
                    s.isHurt = true;
                    s.hurtTimer = 0.15f;
                    hitSomething = true;
                } else {
                    float d = getDistSq(s.sprite->getPosition());
                    if (d < bestDist) {
                        bestDist = d;
                        bestStructure = &s;
                        bestAnimal = nullptr; bestTree = nullptr; bestStone = nullptr; bestMonster = nullptr; bestCampfire = nullptr;
                    }
                }
            }
        }

        // 6. Campfires
        if (campfires) {
            for (auto& c : *campfires) {
                if (c.health <= 0) continue;
                sf::FloatRect cb({ c.position.x - 50.f, c.position.y - 70.f }, { 100.f, 80.f });
                if (checkOverlap(cb)) {
                    if (!c.owner.empty() && c.owner != username) {
                        if (isHoldingWrench) {
                            inventory.showBanner("This belongs to " + c.owner + "! You cannot wrench other players' items.");
                        } else {
                            inventory.showBanner("This belongs to " + c.owner + "! You cannot destroy other players' items.");
                        }
                        continue;
                    }
                    if (swordEquipped) {
                        c.health -= damage;
                        c.isHurt = true;
                        c.hurtTimer = 0.15f;
                        hitSomething = true;
                    } else {
                        float d = getDistSq(c.position);
                        if (d < bestDist) {
                            bestDist = d;
                            bestCampfire = &c;
                            bestAnimal = nullptr; bestTree = nullptr; bestStone = nullptr; bestMonster = nullptr; bestStructure = nullptr;
                        }
                    }
                }
            }
        }

        // Apply bare hands attack exclusively to the single closest approached item
        if (!swordEquipped) {
            if (bestAnimal) {
                if (tutorialActive && tutorialStep == 5) tutAttacked = true;
                bestAnimal->health -= damage;
                bestAnimal->isHurt = true;
                bestAnimal->hurtTimer = 0.15f;
                hitSomething = true;
                playerExp += 10;

                sf::Vector2f dir = bestAnimal->sprite.getPosition() - sprite.getPosition();
                float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                if (len != 0.f) dir /= len;
                bestAnimal->sprite.move(dir * 40.f);
            }
            else if (bestTree) {
                if (tutorialActive && tutorialStep == 4) tutWoodGathered++;
                bestTree->health -= damage;
                bestTree->isHurt = true;
                bestTree->hurtTimer = 0.15f;
                hitSomething = true;
                playerExp += 5;
            }
            else if (bestStone) {
                if (tutorialActive && tutorialStep == 4) tutStoneGathered++;
                bestStone->health -= damage;
                bestStone->isHurt = true;
                bestStone->hurtTimer = 0.15f;
                hitSomething = true;
                playerExp += 5;
            }
            else if (bestMonster) {
                if (tutorialActive && tutorialStep == 5) tutAttacked = true;
                bestMonster->health -= damage;
                bestMonster->isHurt = true;
                bestMonster->hurtTimer = 0.15f;
                hitSomething = true;
                playerExp += 20;

                sf::Vector2f dir = bestMonster->sprite.getPosition() - sprite.getPosition();
                float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                if (len != 0.f) dir /= len;
                bestMonster->sprite.move(dir * 40.f);
            }
            else if (bestStructure) {
                if (!bestStructure->owner.empty() && bestStructure->owner != username) {
                    if (isHoldingWrench) {
                        inventory.showBanner("This belongs to " + bestStructure->owner + "! You cannot wrench other players' items.");
                    } else {
                        inventory.showBanner("This belongs to " + bestStructure->owner + "! You cannot destroy other players' items.");
                    }
                } else {
                    bestStructure->health -= damage;
                    bestStructure->isHurt = true;
                    bestStructure->hurtTimer = 0.15f;
                    hitSomething = true;
                }
            }
            else if (bestCampfire) {
                if (!bestCampfire->owner.empty() && bestCampfire->owner != username) {
                    if (isHoldingWrench) {
                        inventory.showBanner("This belongs to " + bestCampfire->owner + "! You cannot wrench other players' items.");
                    } else {
                        inventory.showBanner("This belongs to " + bestCampfire->owner + "! You cannot destroy other players' items.");
                    }
                } else {
                    bestCampfire->health -= damage;
                    bestCampfire->isHurt = true;
                    bestCampfire->hurtTimer = 0.15f;
                    hitSomething = true;
                }
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
}

#include <random>

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
    const sf::Texture& coalTex,
    const sf::Texture& copperOreTex,
    const sf::Texture& ironOreTex,
    const sf::Texture& goldOreTex,
    const sf::Texture& diamondTex,
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
    std::vector<PlacedCampfire>& campfires,
    const std::function<sf::Vector2f(sf::Vector2f)>& snapGridFunc,
    const GameSettings& globalSettings,
    const sf::Font& font,
    sf::RenderWindow& window
) {
    const int MAX_MULTIPLIER = 10;

    for (auto& a : animals) {
        sf::FloatRect bounds = a->sprite.getGlobalBounds();
        sf::Vector2f pos = a->sprite.getPosition();

        if (bounds.position.x < 0.f)
            pos.x -= bounds.position.x;
        if (bounds.position.y < 0.f)
            pos.y -= bounds.position.y;
        if (bounds.position.x + bounds.size.x > mapWidth)
            pos.x -= (bounds.position.x + bounds.size.x - mapWidth);
        if (bounds.position.y + bounds.size.y > mapHeight)
            pos.y -= (bounds.position.y + bounds.size.y - mapHeight);

        a->sprite.setPosition(pos);
    }

    for (auto& a : animals) {
        if (a->health <= 0 && !a->droppedLoot) {
            itemManager.addItem(
                a->getFoodItemID(),
                *a->getFoodTexture(),
                a->sprite.getPosition(),
                a->getFoodDropCount()
            );

            if (a->getFoodItemID() == ITEM_SHEEP_MEAT) {
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

    struct PendingDrop {
        int id;
        const sf::Texture* texture;
        sf::Vector2f pos;
        int quantity;
    };

    std::vector<PendingDrop> pendingDrops;

    auto& items = itemManager.getItems();

    bool itemsChanged = false;
    for (auto it = items.begin(); it != items.end(); ) {

        // TREE DESTROYED
        if (it->id == ITEM_TREE && it->health <= 0) {
            sf::Vector2f pos = it->sprite.getPosition();

            it = items.erase(it);
            itemsChanged = true;

            pendingDrops.push_back({
                ITEM_WOOD,
                &woodTex,
                pos,
                1
                });

            if (!tutorialActive && qInfo.questID == 1 && qInfo.progress < 10) {
                qInfo.progress++;
                std::thread([&authManager, username, prog = qInfo.progress, comp = qInfo.isCompleted]() {
                    try { authManager.updateQuestProgress(username.c_str(), prog, comp); } catch (...) {}
                }).detach();
            }

            int chance = rand() % 100;

            if (chance < 80) {
                pendingDrops.push_back({
                    ITEM_SAPLING,
                    &saplingTex,
                    pos,
                    1
                    });

                std::cout << "Obtained Sapling!\n";
            }

            continue;
        }

        // STONE DESTROYED
        if (it->id == ITEM_STONE && it->health <= 0) {
            sf::Vector2f pos = it->sprite.getPosition();

            it = items.erase(it);
            itemsChanged = true;

            if (tutorialActive && tutorialStep == 4) tutStoneGathered++;

            if (!tutorialActive && qInfo.questID == 2 && qInfo.progress < 8) {
                qInfo.progress++;
                std::thread([&authManager, username, prog = qInfo.progress, comp = qInfo.isCompleted]() {
                    try { authManager.updateQuestProgress(username.c_str(), prog, comp); } catch (...) {}
                }).detach();
            }

            int chance = rand() % 100000;

            if (chance < 10000) { // 10% Stone (drastically reduced from 40%)
                pendingDrops.push_back({
                    ITEM_DROP_STONE,
                    &stoneTex,
                    pos,
                    1
                    });

                std::cout << "Obtained Stone!\n";
            }
            else if (chance < 45000) { // 35% Coal (increased from 30%)
                pendingDrops.push_back({
                    ITEM_COAL,
                    &coalTex,
                    pos,
                    1
                    });

                std::cout << "Obtained Coal!\n";
            }
            else if (chance < 75000) { // 30% Copper Ore (increased from 20%)
                pendingDrops.push_back({
                    ITEM_COPPER_ORE,
                    &copperOreTex,
                    pos,
                    1
                    });

                std::cout << "Obtained Copper Ore!\n";
            }
            else if (chance < 97000) { // 22% Iron Ore (more than doubled from 9.6%)
                pendingDrops.push_back({
                    ITEM_IRON_ORE,
                    &ironOreTex,
                    pos,
                    1
                    });

                std::cout << "Obtained Iron Ore!\n";
            }
            else if (chance < 99000) { // 2.0% Gold Ore (increased from 0.3%)
                pendingDrops.push_back({
                    ITEM_GOLD_ORE,
                    &goldOreTex,
                    pos,
                    1
                    });

                std::cout << "Obtained Gold Ore!\n";
            }
            else { // 1.0% Diamond (increased from 0.1%)
                pendingDrops.push_back({
                    ITEM_DIAMOND,
                    &diamondTex,
                    pos,
                    1
                    });

                std::cout << "Obtained Diamond!\n";
            }

            continue;
        }

        ++it;
    }

    // ADD ALL NEW DROPS AFTER FINISHING THE LOOP
    if (!pendingDrops.empty()) {
        for (const auto& drop : pendingDrops) {
            itemManager.addItem(
                drop.id,
                *drop.texture,
                drop.pos,
                drop.quantity
            );
        }
        itemsChanged = true;
    }

    // Rebuild indexes ONLY if items were actually destroyed or added
    if (itemsChanged) {
        itemManager.rebuildSpatialGrid();
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

    bool campfireDestroyed = false;
    for (auto it = campfires.begin(); it != campfires.end(); ) {
        if (it->health <= 0) {
            it = campfires.erase(it);
            campfireDestroyed = true;
        }
        else {
            if (it->isHurt) {
                it->hurtTimer -= dt;
                if (it->hurtTimer <= 0.f) it->isHurt = false;
            }
            ++it;
        }
    }

    if ((structureDestroyed || campfireDestroyed) && syncSaveFunc) {
        syncSaveFunc();
    }

    for (auto& m : monsters) {
        if (m->health <= 0 && !m->droppedLoot) {
            itemManager.addItem(
                m->getFoodItemID(),
                *m->getFoodTexture(),
                m->sprite.getPosition(),
                m->getFoodDropCount()
            );

            m->droppedLoot = true;
        }
    }

    std::pair<float, int> nearestPickup = itemManager.getNearestPickupItem(sprite, 350.f);
    float nearestDist = nearestPickup.first;
    const float RADIUS = 250.f;
    int nearestID = nearestPickup.second;

    if (nearestDist < RADIUS) {
        sf::Text prompt(font, "Press E to pick up", 32);
        prompt.setFillColor(sf::Color::Yellow);
        prompt.setPosition({ sprite.getPosition().x - 120, sprite.getPosition().y - 80 });
        window.draw(prompt);
    }

    static sf::Clock drinkClock;

    if (sf::Keyboard::isKeyPressed(globalSettings.keyInteract)) {
        if (tutorialActive && tutorialStep == 4 && nearestID == ITEM_STONE) {
            tutStoneGathered++;
        }

        if (!tutorialActive && nearestID == ITEM_STONE && nearestDist < 100.f && qInfo.questID == 2 && qInfo.progress < 8) {
            qInfo.progress++;
            std::thread([&authManager, username, prog = qInfo.progress, comp = qInfo.isCompleted]() {
                try { authManager.updateQuestProgress(username.c_str(), prog, comp); } catch (...) {}
            }).detach();
        }

        if (!tutorialActive && (nearestID == ITEM_CHICKEN_MEAT || nearestID == ITEM_PIG_MEAT || nearestID == ITEM_ZOMBIE_FLESH) && nearestDist < 100.f && qInfo.questID == 4 && qInfo.progress < 5) {
            qInfo.progress++;
            std::thread([&authManager, username, prog = qInfo.progress, comp = qInfo.isCompleted]() {
                try { authManager.updateQuestProgress(username.c_str(), prog, comp); } catch (...) {}
            }).detach();
        }

        itemManager.tryPickup(sprite, inventory);

        if (nearPond && drinkClock.getElapsedTime().asSeconds() > 1.f) {
            float waterRestore = 40.f;
            thirst -= waterRestore;
            while (thirst < 0.f && thirstMultiplier < MAX_MULTIPLIER) {
                thirst += maxThirst;
                thirstMultiplier++;
            }
            thirst = (std::max)(0.f, thirst);
            std::cout << "Thirst Multiplier = " << thirstMultiplier << "\n";
            drinkClock.restart();
        }
    }

    static sf::Clock stoneRespawnClock;
    if (stoneRespawnClock.getElapsedTime().asSeconds() > 10.f) {
        int currentStones = 0;
        for (const auto& item : itemManager.getItems()) {
            if (item.id == ITEM_STONE) currentStones++;
        }

        if (currentStones < 15) {
            static std::mt19937 gen(std::random_device{}());
            std::uniform_real_distribution<float> randX(0.f, (float)mapWidth);
            std::uniform_real_distribution<float> randY(0.f, (float)mapHeight);

            for (int a = 0; a < 20; ++a) {
                sf::Vector2f p(randX(gen), randY(gen));
                p = snapGridFunc(p);

                if (!isPosNearStructureOrHouse(p, structures, campfires, 500.f) &&
                    itemManager.isAreaFreeExplicit(p, 300.f, &stoneTex)) {
                    itemManager.addItem(ITEM_STONE, stoneTex, p, 1);
                    break;
                }
            }
        }
        stoneRespawnClock.restart();
    }
}

#include "GameAssets.h"
#include "AuthManager.h"

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
) {
    const float HUNGER_RATE = 1.f;
    const float THIRST_RATE = 1.f;
    const float DAMAGE_RATE = 10.f;

    if (!introActive && !tutorialActive && !isMenuOpen && !spawnOverlayActive)
    {
        hunger += (HUNGER_RATE / hungerMultiplier) * dt;

        if (hunger >= maxHunger)
        {
            hunger = maxHunger;

            if (hungerMultiplier > 1)
            {
                hungerMultiplier--;
                hunger = 0.f;
            }
            else
            {
                hp = std::max(0.f, hp - DAMAGE_RATE * dt);
            }
        }

        thirst += (THIRST_RATE / thirstMultiplier) * dt;

        if (thirst >= maxThirst)
        {
            thirst = maxThirst;

            if (thirstMultiplier > 1)
            {
                thirstMultiplier--;
                thirst = 0.f;
            }
            else
            {
                hp = std::max(0.f, hp - DAMAGE_RATE * dt);
            }
        }
        else if (hunger < maxHunger * 0.2f)
        {
            hp = std::min(maxHP, hp + 5.f * dt);
        }
    }

    int nextLevelExp = level * 100;
    if (exp >= nextLevelExp) {
        exp -= nextLevelExp;
        level++;
        std::cout << "LEVEL UP! Now Level " << level << "\n";
    }

    if (!tutorialActive && qInfo.questID > 0 && !qInfo.isCompleted) {
        int goal = 0;
        if (qInfo.questID >= 1 && qInfo.questID <= (int)availableQuests.size()) {
            goal = availableQuests[qInfo.questID - 1].goal;
        }

        if (qInfo.progress >= goal && goal > 0) {
            qInfo.isCompleted = true;
            currency += 50;
            std::thread([&authManager, username, prog = qInfo.progress]() {
                try { authManager.updateQuestProgress(username.c_str(), prog, true); } catch (...) {}
            }).detach();
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
}



