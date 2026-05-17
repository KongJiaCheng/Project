#pragma once
#include "Food.h"
#include "ViewUtils.h"

class PigMeat : public Food
{
public:
    static sf::Texture texture;

    PigMeat()
        : Food(ITEM_PIG_MEAT, 40.f, &texture)
    {
    }

    static void loadTexture()
    {
        (void)texture.loadFromFile(resolvePath("../Items/PigMeat.png"));
    }

    static const sf::Texture* getTexture();

};
