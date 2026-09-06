#pragma once
#include "Food.h"
#include "ViewUtils.h"

class SheepMeat : public Food
{
public:
    static sf::Texture texture;

    SheepMeat()
        : Food(ITEM_SHEEP_MEAT, 30.f, &texture)
    {
    }

    static void loadTexture();
    static const sf::Texture* getTexture();
};

class CookedSheepMeat : public Food
{
public:
    static sf::Texture texture;

    CookedSheepMeat()
        : Food(ITEM_COOKED_SHEEP, 60.f, &texture)
    {
    }

    static void loadTexture();
    static const sf::Texture* getTexture();
};
