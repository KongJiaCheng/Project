#include "ItemManager.h"
#include "ChickenMeat.h"
#include "PigMeat.h"

#include <cmath>
#include <random>
#include <limits>
#include <iostream>
#include <SFML/Graphics.hpp>

// --------------------------------------------------
// Helpers
// --------------------------------------------------
ItemManager::ItemManager()
{
    if (!woodTexture.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/wood.png"))
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

void ItemManager::addItem(int id, const sf::Texture& texture, sf::Vector2f pos, int quantity)
{
    items.emplace_back(id, quantity, texture, pos);
}

void ItemManager::draw(sf::RenderWindow& window)
{
    for (auto& i : items)
        window.draw(i.sprite);
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
        inventory.addItem(ITEM_WOOD, woodTexture, 1);
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
}

float ItemManager::getNearestDistance(const sf::Sprite& player) const
{
    if (items.empty())
        return std::numeric_limits<float>::infinity();

    sf::Vector2f ppos = getSpriteCenterSafe(player);
    float bestDist = std::numeric_limits<float>::max();

    for (const auto& it : items)
    {
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

    for (const auto& it : items)
    {
        RectF r = getSpriteBoundsExplicit(it.sprite);

        bool overlap =
            newX < r.x + r.w &&
            newX + newW > r.x &&
            newY < r.y + r.h &&
            newY + newH > r.y;

        if (overlap)
            return false;
    }

    return true;
}




bool ItemManager::isFood(int itemID) const
{
    return itemID == ITEM_CHICKEN_MEAT ||
        itemID == ITEM_PIG_MEAT;
}

Food ItemManager::getFoodData(int itemID) const
{
    switch (itemID)
    {
    case ITEM_CHICKEN_MEAT:
        return ChickenMeat();
    case ITEM_PIG_MEAT:
        return PigMeat();
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

            if (attempts < MAX_ATTEMPTS)
                items.emplace_back(rule.itemID, 1, *rule.texture, pos);
        }
    }
}
