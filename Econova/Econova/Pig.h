#pragma once
#include "Animal.h"
#include "ItemID.h"
#include "PigMeat.h"

class Pig : public Animal
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
        aS.loadFromFolder("Sprites/Animals/Pig/Walk/South", 4);
        aN.loadFromFolder("Sprites/Animals/Pig/Walk/North", 4);
        aE.loadFromFolder("Sprites/Animals/Pig/Walk/East", 4);
        aW.loadFromFolder("Sprites/Animals/Pig/Walk/West", 4);
        sharedSouth = std::move(aS.frames);
        sharedNorth = std::move(aN.frames);
        sharedEast = std::move(aE.frames);
        sharedWest = std::move(aW.frames);
        sharedLoaded = true;
    }

    Pig(sf::Vector2f pos) : Animal(texture, pos, 50)
    {
        maxHealth = 60.f;
        health = maxHealth;
        speed = 30.f;
        attackDamage = 12.f;
        attackCooldown = 1.0f;

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
        return ITEM_PIG_MEAT;
    }

    const sf::Texture* getFoodTexture() const override
    {
        return PigMeat::getTexture();
    }

    int getFoodDropCount() const override
    {
        return 2 + rand() % 3;
    }
};
