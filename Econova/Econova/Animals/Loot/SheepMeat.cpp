#include "SheepMeat.h"

sf::Texture SheepMeat::texture;
sf::Texture CookedSheepMeat::texture;

void SheepMeat::loadTexture()
{
    (void)texture.loadFromFile(resolvePath("Items/sheep meat.png"));
}

const sf::Texture* SheepMeat::getTexture()
{
    if (texture.getSize().x == 0)
        (void)texture.loadFromFile(resolvePath("Items/sheep meat.png"));
    return &texture;
}

void CookedSheepMeat::loadTexture()
{
    (void)texture.loadFromFile(resolvePath("Items/cooked sheep meat.png"));
}

const sf::Texture* CookedSheepMeat::getTexture()
{
    if (texture.getSize().x == 0)
        (void)texture.loadFromFile(resolvePath("Items/cooked sheep meat.png"));
    return &texture;
}
