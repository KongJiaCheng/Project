#pragma once
#include "Animal.h"
#include "ItemID.h"
#include "PigMeat.h"

class Pig : public Animal
{
public:
    static sf::Texture texture;
    Pig(sf::Vector2f pos) : Animal(texture, pos, 50)
    {
        maxHealth = 60.f;
        health = maxHealth;
        speed = 30.f;
        attackDamage = 12.f;
        attackCooldown = 1.0f;

        loadAnimations(
            "../Sprites/Animals/Pig/Walk/South",
            "../Sprites/Animals/Pig/Walk/North",
            "../Sprites/Animals/Pig/Walk/East",
            "../Sprites/Animals/Pig/Walk/West",
            4
        );

        sprite.setPosition(pos);
        sprite.setScale({ 1.5f, 1.5f });
    }

    int getFoodItemID() const override
    {
        return ITEM_PIG_MEAT;
    }

    const sf::Texture* getFoodTexture() const override
    {
        return &PigMeat::texture;
    }

    int getFoodDropCount() const override
    {
        return 2 + rand() % 3;
    }
};
