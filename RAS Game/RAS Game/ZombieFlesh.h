#pragma once
#include "Food.h"

class ZombieFlesh : public Food
{
public:
    static sf::Texture texture;

    ZombieFlesh()
        : Food(ITEM_ZOMBIE_FLESH, 40.f, &texture)
    {
    }

    static void loadTexture()
    {
        texture.loadFromFile("../Items/ZombieFlesh.png");
    }

    static const sf::Texture* getTexture();

};
