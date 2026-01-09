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
        texture.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Items\\ZombieFlesh.png");
    }

    static const sf::Texture* getTexture();

};
