#pragma once
#include "Food.h"

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
        texture.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Items\\PigMeat.png");
    }

};
