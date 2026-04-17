#pragma once
#include "Food.h"

class ChickenMeat : public Food
{
public:
    static sf::Texture texture;

    ChickenMeat()
        : Food(ITEM_CHICKEN_MEAT, 25.f, &texture)
    {
    }

    static void loadTexture()
    {
        texture.loadFromFile("../Items/ChickenMeat.png");
    }

    static const sf::Texture* getTexture();

};
