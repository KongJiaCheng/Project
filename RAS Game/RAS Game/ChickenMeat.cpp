#include "ChickenMeat.h"

sf::Texture ChickenMeat::texture;

const sf::Texture* ChickenMeat::getTexture()
{
    if (texture.getSize().x == 0)
        texture.loadFromFile("../Items/ChickenMeat.png");

    return &texture;
}
