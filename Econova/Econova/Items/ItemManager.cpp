#include "ItemManager.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "ZombieFlesh.h"
#include "Food.h"
#include "SheepMeat.h"
#include "ViewUtils.h"

#include <cmath>
#include <random>
#include <limits>
#include <iostream>
#include <SFML/Graphics.hpp>

#include "Game.h"

// Snap helpers (tree=50x50, stone=25x25)
static sf::Vector2f snapTree(sf::Vector2f pos)  { return snapToGrid(pos); }
static sf::Vector2f snapStone(sf::Vector2f pos) { return snapToGrid(pos); }


// --------------------------------------------------
// Helpers
// --------------------------------------------------
ItemManager::ItemManager()
{
    if (!woodTexture.loadFromFile(resolvePath("Items/wood.png")))
    {
        std::cerr << "failed to load wood texture!\n";
    }
}
sf::Vector2f ItemManager::getSpriteCenterSafe(const sf::Sprite& s) const
{
    sf::Vector2f pos = s.getPosition();
    sf::Vector2f origin = s.getOrigin();
    sf::Vector2f scale = s.getScale();

    const sf::Texture* tex = &s.getTexture();
    if (!tex)
        return pos;

    sf::Vector2u ts = tex->getSize();
    sf::Vector2f size(
        static_cast<float>(ts.x) * scale.x,
        static_cast<float>(ts.y) * scale.y
    );

    return {
        pos.x - origin.x + size.x * 0.5f,
        pos.y - origin.y + size.y * 0.5f
    };
}

float ItemManager::getSpriteRadius(const sf::Sprite& s) const
{
    const sf::Texture* tex = &s.getTexture();
    if (!tex)
        return 0.f;

    sf::Vector2u ts = tex->getSize();
    sf::Vector2f scale = s.getScale();

    float w = ts.x * scale.x;
    float h = ts.y * scale.y;

    return std::sqrt(w * w + h * h) * 0.5f;
}

// --------------------------------------------------
// Core
// --------------------------------------------------

void ItemManager::setResourceTextures(
    const sf::Texture* wood,
    const sf::Texture* stone
)
{
    woodTex = wood;
    stoneTex = stone;
}

const std::vector<Item>& ItemManager::getItems() const
{
    return items;
}

std::vector<Item>& ItemManager::getItems()
{
    return items;
}

void ItemManager::addItem(int id, const sf::Texture& texture, sf::Vector2f pos, int quantity)
{
    if (id == ITEM_TREE) {
        pos = snapToGrid(pos);
    } else if (id == ITEM_STONE) {
        pos = snapStone(pos);
    }

    size_t idx = items.size();
    items.emplace_back(id, quantity, texture, pos);
    auto sz = texture.getSize();
    if (id == ITEM_TREE)
    {
        items.back().sprite.setOrigin({
            sz.x / 2.f,
            static_cast<float>(sz.y)
            });

        // Scale tree sprite so its base fits a 50×50 ground footprint
        float scaleX = (sz.x > 0) ? 50.f / static_cast<float>(sz.x) : 1.f;
        // Keep tree tall – use same uniform scale as before but proportional
        float scale  = (sz.x > 0) ? 960.f / static_cast<float>(sz.x) : 1.2f;
        items.back().sprite.setScale({ scale, scale });

        items.back().sprite.move({ 0.f, -160.f });
    }
    else if (id == ITEM_STONE)
    {
        items.back().sprite.setOrigin({
            sz.x / 2.f,
            static_cast<float>(sz.y)
            });

        // Scale stone sprite so it fits a 250×250 cell
        float scale = (sz.x > 0) ? 250.f / static_cast<float>(sz.x) : 0.5f;
        items.back().sprite.setScale({ scale, scale });

        items.back().sprite.move({ 0.f, -120.f });
    }
    else if (id == ITEM_SAPLING) {
        items.back().sprite.setScale({ 0.32f, 0.32f });
    }
    else if (id == ITEM_CHICKEN_MEAT || id == ITEM_PIG_MEAT || id == ITEM_ZOMBIE_FLESH || id == ITEM_COOKED_CHICKEN || id == ITEM_COOKED_PIG) {
        items.back().sprite.setScale({ 0.4f, 0.4f });
    }
    else if (id == ITEM_SHEEP_MEAT || id == ITEM_COOKED_SHEEP) {
        items.back().sprite.setScale({ 0.32f, 0.32f });
    }
    else if (id == ITEM_WOOL) {
        items.back().sprite.setScale({ 0.25f, 0.25f });
    }
    else if (id == ITEM_COAL || id == ITEM_COPPER_ORE || id == ITEM_COPPER_BAR ||
             id == ITEM_IRON_ORE || id == ITEM_IRON_BAR || id == ITEM_GOLD_ORE ||
             id == ITEM_GOLD_BAR || id == ITEM_DIAMOND || id == ITEM_DROP_STONE) {
        items.back().sprite.setScale({ 0.35f, 0.35f });
    }
    gridInsert(idx, pos);
}

void ItemManager::draw(sf::RenderWindow& window)
{
    // View frustum culling using spatial grid to only check on-screen cells
    sf::View view = window.getView();
    sf::Vector2f center = view.getCenter();
    sf::Vector2f size = view.getSize();
    const float margin = 450.f; // generous margin for tall trees & wide objects
    float minX = center.x - size.x * 0.5f - margin;
    float maxX = center.x + size.x * 0.5f + margin;
    float minY = center.y - size.y * 0.5f - margin;
    float maxY = center.y + size.y * 0.5f + margin;

    int cxMin = static_cast<int>(std::floor(minX / GRID_CELL));
    int cxMax = static_cast<int>(std::floor(maxX / GRID_CELL));
    int cyMin = static_cast<int>(std::floor(minY / GRID_CELL));
    int cyMax = static_cast<int>(std::floor(maxY / GRID_CELL));

    for (int cx = cxMin; cx <= cxMax; ++cx) {
        for (int cy = cyMin; cy <= cyMax; ++cy) {
            auto it = spatialGrid.find(cellKey(cx, cy));
            if (it == spatialGrid.end()) continue;

            for (size_t idx : it->second) {
                if (idx >= items.size()) continue;
                auto& i = items[idx];

                if (i.isHurt)
                {
                    i.hurtTimer -= 0.016f; // approximate 60fps
                    if (i.hurtTimer <= 0.f)
                    {
                        i.isHurt = false;
                        i.sprite.setColor(sf::Color::White);
                    }
                    else
                    {
                        i.sprite.setColor(sf::Color(255, 100, 100));
                    }
                }

                sf::Vector2f pos = i.sprite.getPosition();
                // Skip items that are off-screen
                if (pos.x < minX || pos.x > maxX || pos.y < minY || pos.y > maxY)
                {
                    continue;
                }

                window.draw(i.sprite);

                if ((i.id == ITEM_TREE || i.id == ITEM_STONE) && i.health < i.maxHealth)
                {
                    sf::FloatRect bounds = i.sprite.getGlobalBounds();
                    sf::RectangleShape hpBg({ 40.f, 6.f });
                    hpBg.setFillColor(sf::Color::Red);
                    hpBg.setPosition({ bounds.position.x + bounds.size.x / 2.f - 20.f, bounds.position.y - 15.f });

                    sf::RectangleShape hpFg({ 40.f * (std::max(0.f, i.health) / i.maxHealth), 6.f });
                    hpFg.setFillColor(sf::Color::Green);
                    hpFg.setPosition(hpBg.getPosition());

                    window.draw(hpBg);
                    window.draw(hpFg);
                }
            }
        }
    }
}

// --------------------------------------------------
// World logic
// --------------------------------------------------

bool ItemManager::isResourceBlocking(sf::Vector2f targetPos) const
{
    const float RES_CULL = 350.f;
    const float RES_CULL_SQ = RES_CULL * RES_CULL;

    int cxMin = static_cast<int>(std::floor((targetPos.x - RES_CULL) / GRID_CELL));
    int cxMax = static_cast<int>(std::floor((targetPos.x + RES_CULL) / GRID_CELL));
    int cyMin = static_cast<int>(std::floor((targetPos.y - RES_CULL) / GRID_CELL));
    int cyMax = static_cast<int>(std::floor((targetPos.y + RES_CULL) / GRID_CELL));

    for (int cx = cxMin; cx <= cxMax; ++cx) {
        for (int cy = cyMin; cy <= cyMax; ++cy) {
            auto it = spatialGrid.find(cellKey(cx, cy));
            if (it == spatialGrid.end()) continue;

            for (size_t idx : it->second) {
                if (idx >= items.size()) continue;
                const auto& item = items[idx];
                if (item.id == ITEM_TREE || item.id == ITEM_STONE) {
                    sf::Vector2f ipos = item.sprite.getPosition();
                    float idxDist = ipos.x - targetPos.x;
                    float idyDist = ipos.y - targetPos.y;
                    if (idxDist * idxDist + idyDist * idyDist > RES_CULL_SQ) continue;

                    sf::FloatRect b = item.sprite.getGlobalBounds();
                    b.position.x += b.size.x * 0.3f; b.size.x *= 0.4f;
                    b.position.y += b.size.y * 0.7f; b.size.y *= 0.3f;
                    if (b.contains(targetPos)) return true;
                }
            }
        }
    }
    return false;
}

std::vector<int> ItemManager::tryPickup(
    const sf::Sprite& player,
    Inventory& inventory,
    float maxRadius
)
{
    std::vector<int> pickedIds;
    if (items.empty())
        return pickedIds;

    sf::Vector2f ppos = getSpriteCenterSafe(player);
    float maxRadSq = maxRadius * maxRadius;

    int cxMin = static_cast<int>(std::floor((ppos.x - maxRadius) / GRID_CELL));
    int cxMax = static_cast<int>(std::floor((ppos.x + maxRadius) / GRID_CELL));
    int cyMin = static_cast<int>(std::floor((ppos.y - maxRadius) / GRID_CELL));
    int cyMax = static_cast<int>(std::floor((ppos.y + maxRadius) / GRID_CELL));

    std::vector<size_t> candidateIndices;
    for (int cx = cxMin; cx <= cxMax; ++cx) {
        for (int cy = cyMin; cy <= cyMax; ++cy) {
            auto it = spatialGrid.find(cellKey(cx, cy));
            if (it == spatialGrid.end()) continue;
            for (size_t idx : it->second) {
                if (idx < items.size() && items[idx].id != ITEM_TREE && items[idx].id != ITEM_STONE) {
                    candidateIndices.push_back(idx);
                }
            }
        }
    }

    if (candidateIndices.empty()) return pickedIds;

    std::sort(candidateIndices.begin(), candidateIndices.end());
    candidateIndices.erase(std::unique(candidateIndices.begin(), candidateIndices.end()), candidateIndices.end());

    std::vector<size_t> toErase;
    for (size_t idx : candidateIndices) {
        sf::Vector2f ipos = getSpriteCenterSafe(items[idx].sprite);
        float dx = ipos.x - ppos.x;
        float dy = ipos.y - ppos.y;
        if (dx * dx + dy * dy < maxRadSq) {
            int pickedID = items[idx].id;
            int inventoryID = (pickedID == ITEM_DROP_STONE) ? ITEM_STONE : pickedID;
            const sf::Texture* itTex = nullptr;
            try {
                itTex = &items[idx].sprite.getTexture();
            } catch (...) {
                itTex = nullptr;
            }
            if (itTex) {
                inventory.addItem(
                    inventoryID,
                    *itTex,
                    items[idx].count
                );
                pickedIds.push_back(pickedID);
                toErase.push_back(idx);
            }
        }
    }

    if (!toErase.empty()) {
        std::sort(toErase.begin(), toErase.end(), std::greater<size_t>());
        for (size_t idx : toErase) {
            if (idx < items.size()) {
                items.erase(items.begin() + idx);
            }
        }
        rebuildGrid();
    }

    return pickedIds;
}

std::pair<float, int> ItemManager::getNearestPickupItem(const sf::Sprite& player, float maxQueryRadius) const
{
    if (items.empty())
        return { std::numeric_limits<float>::infinity(), -1 };

    sf::Vector2f ppos = player.getPosition();
    float maxDistSq = maxQueryRadius * maxQueryRadius;
    float bestDistSq = maxDistSq;
    int bestID = -1;

    int cxMin = static_cast<int>(std::floor((ppos.x - maxQueryRadius) / GRID_CELL));
    int cxMax = static_cast<int>(std::floor((ppos.x + maxQueryRadius) / GRID_CELL));
    int cyMin = static_cast<int>(std::floor((ppos.y - maxQueryRadius) / GRID_CELL));
    int cyMax = static_cast<int>(std::floor((ppos.y + maxQueryRadius) / GRID_CELL));

    for (int cx = cxMin; cx <= cxMax; ++cx) {
        for (int cy = cyMin; cy <= cyMax; ++cy) {
            auto it = spatialGrid.find(cellKey(cx, cy));
            if (it == spatialGrid.end()) continue;

            for (size_t idx : it->second) {
                if (idx >= items.size()) continue;
                const auto& item = items[idx];
                if (item.id == ITEM_TREE || item.id == ITEM_STONE) continue;

                sf::Vector2f ipos = item.sprite.getPosition();
                float dx = ipos.x - ppos.x;
                float dy = ipos.y - ppos.y;
                float distSq = dx * dx + dy * dy;

                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestID = (item.id == ITEM_DROP_STONE) ? ITEM_STONE : item.id;
                }
            }
        }
    }

    if (bestID != -1) {
        return { std::sqrt(bestDistSq), bestID };
    }
    return { std::numeric_limits<float>::infinity(), -1 };
}

float ItemManager::getNearestDistance(const sf::Sprite& player) const
{
    return getNearestPickupItem(player, 400.f).first;
}

int ItemManager::getNearestItemID(const sf::Sprite& player) const
{
    return getNearestPickupItem(player, 400.f).second;
}

bool ItemManager::isAreaFreeExplicit(
    sf::Vector2f pos,
    float minDistance,
    const sf::Texture* newTex
) const
{
    if (!newTex)
        return false;

    // Use a realistic ground footprint (trees: ~80x80, stones: ~100x100)
    // rather than using raw unscaled 960x960 texture bounds, which caused 64-cell spatial queries
    // and false collision rejections.
    float footW = 80.f;
    float footH = 80.f;
    if (newTex->getSize().x < 200 && newTex->getSize().y < 200) {
        footW = static_cast<float>(newTex->getSize().x);
        footH = static_cast<float>(newTex->getSize().y);
    }

    float newX = pos.x - footW * 0.5f - minDistance;
    float newY = pos.y - footH * 0.5f - minDistance;
    float newW = footW + minDistance * 2.f;
    float newH = footH + minDistance * 2.f;

    // Determine which grid cells the query AABB overlaps
    int cxMin = static_cast<int>(std::floor(newX / GRID_CELL));
    int cyMin = static_cast<int>(std::floor(newY / GRID_CELL));
    int cxMax = static_cast<int>(std::floor((newX + newW) / GRID_CELL));
    int cyMax = static_cast<int>(std::floor((newY + newH) / GRID_CELL));

    for (int cx = cxMin; cx <= cxMax; ++cx) {
        for (int cy = cyMin; cy <= cyMax; ++cy) {
            auto it = spatialGrid.find(cellKey(cx, cy));
            if (it == spatialGrid.end()) continue;

            for (size_t idx : it->second) {
                if (idx >= items.size()) continue;
                sf::Vector2f ipos = items[idx].sprite.getPosition();
                float itemFoot = (items[idx].id == ITEM_TREE) ? 60.f : 80.f;

                bool overlap =
                    newX < (ipos.x + itemFoot * 0.5f) &&
                    (newX + newW) > (ipos.x - itemFoot * 0.5f) &&
                    newY < (ipos.y + itemFoot * 0.5f) &&
                    (newY + newH) > (ipos.y - itemFoot * 0.5f);

                if (overlap)
                    return false;
            }
        }
    }

    return true;
}




bool ItemManager::isFood(int itemID) const
{
    return itemID == ITEM_CHICKEN_MEAT ||
        itemID == ITEM_PIG_MEAT ||
        itemID == ITEM_SHEEP_MEAT ||
        itemID == ITEM_COOKED_SHEEP ||
        itemID == ITEM_COOKED_CHICKEN ||
        itemID == ITEM_COOKED_PIG ||
        itemID == ITEM_ZOMBIE_FLESH;
}

Food ItemManager::getFoodData(int itemID) const
{
    switch (itemID)
    {
    case ITEM_CHICKEN_MEAT:
        return ChickenMeat();
    case ITEM_PIG_MEAT:
        return PigMeat();
    case ITEM_SHEEP_MEAT:
        return SheepMeat();
    case ITEM_COOKED_SHEEP:
        return CookedSheepMeat();
    case ITEM_ZOMBIE_FLESH:
        return ZombieFlesh();
    default:
        return Food();
    }
}

void ItemManager::spawnRandomItems(const std::vector<SpawnRule>& rules, int mapWidth, int mapHeight)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> randX(0.f, (float)mapWidth);
    std::uniform_real_distribution<float> randY(0.f, (float)mapHeight);

    for (const auto& rule : rules)
    {
        if (!rule.texture) continue;

        std::uniform_int_distribution<int> amountDist(rule.minCount, rule.maxCount);
        int totalItems = amountDist(gen);

        const float MIN_DISTANCE = 150.f;   // separation between items

        for (int i = 0; i < totalItems; i++)
        {
            sf::Vector2f pos;
            int attempts = 0;
            const int MAX_ATTEMPTS = 40;

            do {
                pos = { randX(gen), randY(gen) };
                attempts++;
            } while (!isAreaFreeExplicit(pos, MIN_DISTANCE, rule.texture) && attempts < MAX_ATTEMPTS);

            if (attempts < MAX_ATTEMPTS) {
                size_t idx = items.size();
                items.emplace_back(rule.itemID, 1, *rule.texture, pos);
                gridInsert(idx, pos);   // keep spatial grid in sync
            }
        }
    }
}

