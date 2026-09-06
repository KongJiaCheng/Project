#include "PigMeat.h"
sf::Texture PigMeat::texture;

const sf::Texture* PigMeat::getTexture()
{
    if (texture.getSize().x == 0)
        texture.loadFromFile(resolvePath("Items/PigMeat.png"));

    return &texture;
}

