#pragma once
#include "Food.h"
#include "ViewUtils.h"

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
        (void)texture.loadFromFile(resolvePath("Items/ChickenMeat.png"));
    }

    static const sf::Texture* getTexture();

};
