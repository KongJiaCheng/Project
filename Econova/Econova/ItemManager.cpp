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

const float GLOBAL_SHEAR = -0.5f;

static sf::Vector2f snapToShearedGrid(sf::Vector2f pos) {
    const float STEP_X = 200.f;
    const float STEP_Y = 100.f;
    float gx = std::round(pos.x / STEP_X);
    float gy = std::round((pos.y - pos.x * GLOBAL_SHEAR) / STEP_Y);
    
    float snappedX = gx * STEP_X;
    float snappedY = gx * STEP_X * GLOBAL_SHEAR + gy * STEP_Y;
    return { snappedX, snappedY };
}

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
    if (id == ITEM_TREE || id == ITEM_STONE) {
        sf::Vector2f snapped = snapToShearedGrid(pos);
        if (std::abs(pos.x - snapped.x) > 1.f || std::abs(pos.y - snapped.y) > 1.f) {
            return; // doesn't fit the grid — discard / remove
        }
    }

    size_t idx = items.size();
    items.emplace_back(id, quantity, texture, pos);
    // Natural resources fill exactly one grid cell (footprint: 200 x 100).
    auto sz = texture.getSize();
    if (id == ITEM_TREE || id == ITEM_STONE) {
        items.back().sprite.setOrigin({ sz.x / 2.f, static_cast<float>(sz.y) });
        float cellScaleX = (sz.x > 0) ? 200.f / static_cast<float>(sz.x) : 1.f;
        float cellScaleY = (sz.y > 0) ? 100.f / static_cast<float>(sz.y) : 1.f;
        items.back().sprite.setScale({ cellScaleX, cellScaleY });
    }
    else if (id == ITEM_SAPLING) {
        items.back().sprite.setScale({ 0.2f, 0.2f });
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
    gridInsert(idx, pos);
}

void ItemManager::draw(sf::RenderWindow& window)
{
    // A small delta time mock or just use fixed decrement per frame if we don't have dt here
    // A better way is to pass dt to draw or update, but for now we just decrement it slightly each draw call
    for (auto& i : items)
    {
        if (i.isHurt)
        {
            i.sprite.setColor(sf::Color(255, 100, 100));
            i.hurtTimer -= 0.016f; // approximate 60fps
            if (i.hurtTimer <= 0.f)
            {
                i.isHurt = false;
                i.sprite.setColor(sf::Color::White);
            }
        }
        else
        {
            i.sprite.setColor(sf::Color::White);
        }

        if (i.id == ITEM_TREE || i.id == ITEM_STONE)
        {
            sf::Transform trans;
            trans.translate(i.sprite.getPosition());
            trans.combine(sf::Transform(1.f, 0.f, 0.f, GLOBAL_SHEAR, 1.f, 0.f, 0.f, 0.f, 1.f));
            trans.translate(-i.sprite.getPosition());
            window.draw(i.sprite, trans);
        }
        else
        {
            window.draw(i.sprite);
        }

        if (i.id == ITEM_TREE && i.health < i.maxHealth)
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

// --------------------------------------------------
// World logic
// --------------------------------------------------

void ItemManager::tryPickup(
    const sf::Sprite& player,
    Inventory& inventory
)
{
    if (items.empty())
        return;

    sf::Vector2f ppos = getSpriteCenterSafe(player);

    int bestIdx = -1;
    float bestDist = std::numeric_limits<float>::max();

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (items[i].id == ITEM_TREE) continue;

        sf::Vector2f ipos = getSpriteCenterSafe(items[i].sprite);
        float dx = ipos.x - ppos.x;
        float dy = ipos.y - ppos.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < bestDist)
        {
            bestDist = dist;
            bestIdx = (int)i;
        }
    }

    if (bestIdx == -1 || bestDist > 250.f)
        return;

    int pickedID = items[bestIdx].id;

    if (pickedID == ITEM_TREE)
    {
        inventory.addItem(ITEM_WOOD, *woodTex, 1);
    }
    else
    {
        inventory.addItem(
            pickedID,
            items[bestIdx].sprite.getTexture(),
            items[bestIdx].count
        );
    }

    items.erase(items.begin() + bestIdx);
    rebuildGrid();   // indices shifted after erase � must rebuild
}

float ItemManager::getNearestDistance(const sf::Sprite& player) const
{
    if (items.empty())
        return std::numeric_limits<float>::infinity();

    sf::Vector2f ppos = getSpriteCenterSafe(player);
    float bestDist = std::numeric_limits<float>::max();

    for (const auto& it : items)
    {
        if (it.id == ITEM_TREE) continue;

        sf::Vector2f ipos = getSpriteCenterSafe(it.sprite);

        float dx = ipos.x - ppos.x;
        float dy = ipos.y - ppos.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < bestDist)
            bestDist = dist;
    }

    return bestDist;
}

int ItemManager::getNearestItemID(const sf::Sprite& player) const
{
    if (items.empty())
        return -1;

    sf::Vector2f ppos = getSpriteCenterSafe(player);
    float bestDist = std::numeric_limits<float>::max();
    int bestID = -1;

    for (const auto& it : items)
    {
        if (it.id == ITEM_TREE) continue;

        sf::Vector2f ipos = getSpriteCenterSafe(it.sprite);

        float dx = ipos.x - ppos.x;
        float dy = ipos.y - ppos.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < bestDist)
        {
            bestDist = dist;
            bestID = it.id;
        }
    }

    return bestID;
}

bool ItemManager::isAreaFreeExplicit(
    sf::Vector2f pos,
    float minDistance,
    const sf::Texture* newTex
) const
{
    if (!newTex)
        return false;

    float w = static_cast<float>(newTex->getSize().x);
    float h = static_cast<float>(newTex->getSize().y);

    float newX = pos.x - minDistance;
    float newY = pos.y - minDistance;
    float newW = w + minDistance * 2.f;
    float newH = h + minDistance * 2.f;

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
                RectF r = getSpriteBoundsExplicit(items[idx].sprite);

                bool overlap =
                    newX < r.x + r.w &&
                    newX + newW > r.x &&
                    newY < r.y + r.h &&
                    newY + newH > r.y;

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
