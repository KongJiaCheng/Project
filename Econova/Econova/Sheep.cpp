#include "Sheep.h"

sf::Texture Sheep::texture;
std::vector<sf::Texture> Sheep::sharedSouth;
std::vector<sf::Texture> Sheep::sharedNorth;
std::vector<sf::Texture> Sheep::sharedEast;
std::vector<sf::Texture> Sheep::sharedWest;
bool Sheep::sharedLoaded = false;

void Sheep::loadSharedTextures() {
    if (sharedLoaded) return;
    Animation aS, aN, aE, aW;
    aS.loadFromFolder("Sprites/Animals/Sheep/Walk/South", 6);
    aN.loadFromFolder("Sprites/Animals/Sheep/Walk/North", 6);
    aE.loadFromFolder("Sprites/Animals/Sheep/Walk/East", 6);
    aW.loadFromFolder("Sprites/Animals/Sheep/Walk/West", 6);
    sharedSouth = std::move(aS.frames);
    sharedNorth = std::move(aN.frames);
    sharedEast = std::move(aE.frames);
    sharedWest = std::move(aW.frames);
    sharedLoaded = true;
}

Sheep::Sheep(sf::Vector2f pos) : Animal(texture, pos, 40)
{
    maxHealth = 40.f;
    health = maxHealth;
    speed = 35.f;
    attackDamage = 6.f;
    attackCooldown = 1.4f;

    loadSharedTextures();
    animSouth.sharedFrames = &sharedSouth;
    animNorth.sharedFrames = &sharedNorth;
    animEast.sharedFrames = &sharedEast;
    animWest.sharedFrames = &sharedWest;

    sprite.setTexture(animSouth.getCurrentTexture(), true);
    sprite.setOrigin({ 32.f, 32.f });

    sprite.setPosition(pos);
    sprite.setScale({ 1.5f, 1.5f });
}

const sf::Texture* Sheep::getFoodTexture() const
{
    return SheepMeat::getTexture();
}
