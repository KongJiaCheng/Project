#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <unordered_map>
#include <limits>
#include <cmath>

#include "Item.h"
#include "Inventory.h"
#include "Food.h"
#include "ItemID.h"

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
    sf::Texture woodTexture;

    const sf::Texture* woodTex = nullptr;
    const sf::Texture* stoneTex = nullptr;

    std::vector<Item> items;

    // ---- Spatial grid for fast isAreaFreeExplicit ----
    // Cell size should be >= the largest minDistance you ever pass (120-150 px).
    static constexpr float GRID_CELL = 200.f;
    // Key = (cellX << 20) | cellY  (fits maps up to ~200k units wide)
    std::unordered_map<uint64_t, std::vector<size_t>> spatialGrid;

    uint64_t cellKey(int cx, int cy) const {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32)
            | static_cast<uint64_t>(static_cast<uint32_t>(cy));
    }
    void gridInsert(size_t idx, sf::Vector2f pos) {
        int cx = static_cast<int>(std::floor(pos.x / GRID_CELL));
        int cy = static_cast<int>(std::floor(pos.y / GRID_CELL));
        spatialGrid[cellKey(cx, cy)].push_back(idx);
    }
    // Rebuild the whole grid (called after bulk operations)
    void rebuildGrid() {
        spatialGrid.clear();
        for (size_t i = 0; i < items.size(); ++i)
            gridInsert(i, items[i].sprite.getPosition());
    }
    // ---- end spatial grid ----

    sf::Vector2f getSpriteCenterSafe(const sf::Sprite& s) const;
    float getSpriteRadius(const sf::Sprite& s) const;

public:
    ItemManager();

    void setResourceTextures(
        const sf::Texture* wood,
        const sf::Texture* stone
    );

    std::vector<Item>& getItems();
    const std::vector<Item>& getItems() const;

    void addItem(int id, const sf::Texture& texture, sf::Vector2f pos, int quantity);
    void rebuildSpatialGrid() { rebuildGrid(); }
    void draw(sf::RenderWindow& window);

    void spawnRandomItems(
        const std::vector<SpawnRule>& rules,
        int mapWidth,
        int mapHeight
    );

    void tryPickup(
        const sf::Sprite& player,
        Inventory& inventory
    );

    float getNearestDistance(const sf::Sprite& player) const;
    int getNearestItemID(const sf::Sprite& player) const;

    struct RectF
    {
        float x;
        float y;
        float w;
        float h;
    };

    RectF getSpriteBoundsExplicit(const sf::Sprite& s) const
    {
        return getSpriteBoundsAtExplicit(s, s.getPosition());
    }

    RectF getSpriteBoundsAtExplicit(
        const sf::Sprite& s,
        const sf::Vector2f& pos
    ) const
    {
        const sf::Texture* tex = &s.getTexture();
        if (!tex)
        {
            RectF r;
            r.x = pos.x;
            r.y = pos.y;
            r.w = 0.f;
            r.h = 0.f;
            return r;
        }

        sf::Vector2f origin = s.getOrigin();
        sf::Vector2f scale = s.getScale();
        sf::Vector2u ts = tex->getSize();

        RectF r;
        r.w = static_cast<float>(ts.x) * scale.x;
        r.h = static_cast<float>(ts.y) * scale.y;
        r.x = pos.x - origin.x * scale.x;
        r.y = pos.y - origin.y * scale.y;

        return r;
    }

    bool isAreaFreeExplicit(
        sf::Vector2f pos,
        float minDistance,
        const sf::Texture* newTex
    ) const;

    bool isFood(int itemID) const;
    Food getFoodData(int itemID) const;
};
