#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <random>
#include <iostream>
#include <limits>

#include "Item.h"
#include "Inventory.h"

struct SpawnRule
{
    int itemID;
    const sf::Texture* texture;
    int minCount;
    int maxCount;
    float rarity;
};

class ItemManager
{
private:
    std::vector<Item> items;

    // -------------------------
    // Sprite helpers
    // -------------------------
    sf::Vector2f getSpriteCenterSafe(const sf::Sprite& s) const
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

    float getSpriteRadius(const sf::Sprite& s) const
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

    sf::FloatRect getSpriteBoundsAt(const sf::Texture& tex, sf::Vector2f pos) const
    {
        sf::FloatRect r;
        r.position = pos;
        r.size = {
            static_cast<float>(tex.getSize().x),
            static_cast<float>(tex.getSize().y)
        };
        return r;
    }

public:
    static constexpr int WOOD_ID = 2;

    const std::vector<Item>& getItems() const
    {
        return items;
    }

    void addItem(int id, const sf::Texture& texture, sf::Vector2f pos, int quantity)
    {
        items.emplace_back(id, quantity, texture, pos);
    }

    void draw(sf::RenderWindow& window)
    {
        for (auto& i : items)
            window.draw(i.sprite);
    }

    void spawnRandomItems(
        const std::vector<SpawnRule>& rules,
        int mapWidth,
        int mapHeight
    );

    // -------------------------
    // Pickup logic
    // -------------------------
    void tryPickup(
        const sf::Sprite& player,
        Inventory& inventory,
        const sf::Texture& woodTex
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
                bestIdx = static_cast<int>(i);
            }
        }

        constexpr float PICKUP_RADIUS = 250.f;
        if (bestIdx == -1 || bestDist >= PICKUP_RADIUS)
            return;

        // -------------------------
        // FIX: no respawn here
        // -------------------------
        int pickedID = items[bestIdx].id;

        if (pickedID == 0)
        {
            // Tree → give wood
            inventory.addItem(WOOD_ID, woodTex, 1);
        }
        else
        {
            const sf::Texture& tex = items[bestIdx].sprite.getTexture();
            inventory.addItem(pickedID, tex, items[bestIdx].count);
        }

        // Remove from world
        items.erase(items.begin() + bestIdx);
    }

    // -------------------------
    // Nearest item queries
    // -------------------------
    float getNearestDistance(const sf::Sprite& player) const
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

    int getNearestItemID(const sf::Sprite& player) const
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

    // -------------------------
    // Explicit bounds helpers
    // -------------------------
    struct RectF
    {
        float x;
        float y;
        float w;
        float h;
    };

    RectF getSpriteBoundsAtExplicit(
        const sf::Sprite& s,
        const sf::Vector2f& pos
    ) const
    {
        const sf::Texture* tex = &s.getTexture();
        if (!tex)
            return { pos.x, pos.y, 0.f, 0.f };

        sf::Vector2f origin = s.getOrigin();
        sf::Vector2f scale = s.getScale();
        sf::Vector2u ts = tex->getSize();

        float w = static_cast<float>(ts.x) * scale.x;
        float h = static_cast<float>(ts.y) * scale.y;

        float left = pos.x - origin.x;
        float top = pos.y - origin.y;

        return { left, top, w, h };
    }

    RectF getSpriteBoundsExplicit(const sf::Sprite& s) const
    {
        return getSpriteBoundsAtExplicit(s, s.getPosition());
    }

    bool isAreaFreeExplicit(
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
};
