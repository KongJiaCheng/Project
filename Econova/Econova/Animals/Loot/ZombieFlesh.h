#pragma once
#include "Food.h"
#include "ViewUtils.h"

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
        (void)texture.loadFromFile(resolvePath("Items/ZombieFlesh.png"));
    }

    static const sf::Texture* getTexture();

};
