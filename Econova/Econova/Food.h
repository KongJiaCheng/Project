#pragma once
#include <SFML/Graphics.hpp>
#include "ItemID.h"
#include "ViewUtils.h"

struct Food
{
    int itemID;
    float hungerRestore;
    const sf::Texture* texture;

    Food(
        int id = 0,
        float hunger = 0.f,
        const sf::Texture* tex = nullptr
    )
        : itemID(id),
        hungerRestore(hunger),
        texture(tex)
    {
    }
};

class SheepMeat : public Food
{
public:
    inline static sf::Texture texture;

    SheepMeat()
        : Food(ITEM_SHEEP_MEAT, 30.f, &texture)
    {
    }

    static void loadTexture()
    {
        (void)texture.loadFromFile(resolvePath("Items/sheep meat.png"));
    }

    static const sf::Texture* getTexture()
    {
        if (texture.getSize().x == 0)
            (void)texture.loadFromFile(resolvePath("Items/sheep meat.png"));
        return &texture;
    }
};

class CookedSheepMeat : public Food
{
public:
    inline static sf::Texture texture;

    CookedSheepMeat()
        : Food(ITEM_COOKED_SHEEP, 60.f, &texture)
    {
    }

    static void loadTexture()
    {
        (void)texture.loadFromFile(resolvePath("Items/cooked sheep meat.png"));
    }

    static const sf::Texture* getTexture()
    {
        if (texture.getSize().x == 0)
            (void)texture.loadFromFile(resolvePath("Items/cooked sheep meat.png"));
        return &texture;
    }
};
