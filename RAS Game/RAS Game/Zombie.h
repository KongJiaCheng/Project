#pragma once
#include "Monster.h"
#include "ItemID.h"
#include "ZombieFlesh.h"

class Zombie : public Monster
{
public:
    static sf::Texture texture;
    Zombie(sf::Vector2f pos) : Monster(texture, pos, 50)
    {
        maxHealth = 500.f;
        health = maxHealth;
        speed = 30.f;
        attackDamage = 12.f;
        attackCooldown = 1.0f;

        loadAnimations(
            "C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Monsters\\Zombie\\Walk\\South",
            "C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Monsters\\Zombie\\Walk\\North",
            "C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Monsters\\Zombie\\Walk\\East",
            "C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Monsters\\Zombie\\Walk\\West",
            4
        );

        sprite.setPosition(pos);
    }

    int getFoodItemID() const override
    {
        return ITEM_ZOMBIE_FLESH;
    }

    const sf::Texture* getFoodTexture() const override
    {
        return &ZombieFlesh::texture;
    }

    int getFoodDropCount() const override
    {
        return 2 + rand() % 3;
    }
};
