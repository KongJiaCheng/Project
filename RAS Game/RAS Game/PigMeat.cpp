#include "PigMeat.h"

sf::Texture PigMeat::texture;

const sf::Texture* PigMeat::getTexture()
{
    if (texture.getSize().x == 0)
        texture.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Items\\PigMeat.png");

    return &texture;
}

