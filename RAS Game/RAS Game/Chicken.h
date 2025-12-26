#pragma once
#include "Animal.h"
#include "ItemID.h"
#include "ChickenMeat.h"

class Chicken : public Animal
{
public:
    static sf::Texture texture;
    Chicken(sf::Vector2f pos) : Animal(texture, pos, 50)
    {
        maxHealth = 20.f;
        health = maxHealth;
        speed = 70.f;
        attackDamage = 4.f;
        attackCooldown = 1.6f;

        loadAnimations(
            "C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Animals\\Chicken\\Walk\\South",
            "C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Animals\\Chicken\\Walk\\North",
            "C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Animals\\Chicken\\Walk\\East",
            "C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Animals\\Chicken\\Walk\\West",
            4
        );

        sprite.setPosition(pos);
    }

    int getFoodItemID() const override
    {
        return ITEM_CHICKEN_MEAT;
    }

    const sf::Texture* getFoodTexture() const override
    {
        return &ChickenMeat::texture;
    }

    int getFoodDropCount() const override
    {
        return 1 + rand() % 2;
    }
};
