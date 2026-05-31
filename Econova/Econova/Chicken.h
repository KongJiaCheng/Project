#pragma once
#include "Animal.h"
#include "ItemID.h"
#include "ChickenMeat.h"

class Chicken : public Animal
{
public:
    static sf::Texture texture;
    static std::vector<sf::Texture> sharedSouth;
    static std::vector<sf::Texture> sharedNorth;
    static std::vector<sf::Texture> sharedEast;
    static std::vector<sf::Texture> sharedWest;
    static bool sharedLoaded;

    static void loadSharedTextures() {
        if (sharedLoaded) return;
        Animation aS, aN, aE, aW;
        aS.loadFromFolder("Sprites/Animals/Chicken/Walk/South", 4);
        aN.loadFromFolder("Sprites/Animals/Chicken/Walk/North", 4);
        aE.loadFromFolder("Sprites/Animals/Chicken/Walk/East", 4);
        aW.loadFromFolder("Sprites/Animals/Chicken/Walk/West", 4);
        sharedSouth = std::move(aS.frames);
        sharedNorth = std::move(aN.frames);
        sharedEast = std::move(aE.frames);
        sharedWest = std::move(aW.frames);
        sharedLoaded = true;
    }

    Chicken(sf::Vector2f pos) : Animal(texture, pos, 50)
    {
        maxHealth = 20.f;
        health = maxHealth;
        speed = 70.f;
        attackDamage = 4.f;
        attackCooldown = 1.6f;

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

    int getFoodItemID() const override
    {
        return ITEM_CHICKEN_MEAT;
    }

    const sf::Texture* getFoodTexture() const override
    {
        return ChickenMeat::getTexture();
    }

    int getFoodDropCount() const override
    {
        return 1 + rand() % 2;
    }
};
