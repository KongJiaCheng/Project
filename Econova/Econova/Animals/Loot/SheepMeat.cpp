#include "SheepMeat.h"

sf::Texture SheepMeat::texture;
sf::Texture CookedSheepMeat::texture;

void SheepMeat::loadTexture()
{
    loadTextureSafe(texture, "Items/sheep meat.png");
}

const sf::Texture* SheepMeat::getTexture()
{
    if (texture.getSize().x == 0)
        loadTextureSafe(texture, "Items/sheep meat.png");
    return &texture;
}

void CookedSheepMeat::loadTexture()
{
    loadTextureSafe(texture, "Items/cooked sheep meat.png");
}

const sf::Texture* CookedSheepMeat::getTexture()
{
    if (texture.getSize().x == 0)
        loadTextureSafe(texture, "Items/cooked sheep meat.png");
    return &texture;
}
