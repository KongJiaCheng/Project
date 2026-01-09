#include "ChickenMeat.h"

sf::Texture ChickenMeat::texture;

const sf::Texture* ChickenMeat::getTexture()
{
    if (texture.getSize().x == 0)
        texture.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Items\\ChickenMeat.png");

    return &texture;
}
