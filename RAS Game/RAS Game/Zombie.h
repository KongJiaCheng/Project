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
        speed = 80.f;
        attackDamage = 15.f;
        attackCooldown = 1.2f;

        loadAnimations(
            "../Sprites/Monsters/Zombie/Walk/South",
            "../Sprites/Monsters/Zombie/Walk/North",
            "../Sprites/Monsters/Zombie/Walk/East",
            "../Sprites/Monsters/Zombie/Walk/West",
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
        return ZombieFlesh::getTexture();
    }

    int getFoodDropCount() const override
    {
        return 2 + rand() % 3;
    }

    void update(float dt, const sf::Vector2f& playerPos) override
    {
        sf::Vector2f pos = sprite.getPosition();
        sf::Vector2f dir = playerPos - pos; // Vector from zombie to player

        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);

        if (isHurt)
        {
            hurtTimer -= dt;
            if (hurtTimer <= 0.f)
                isHurt = false;
        }

        if (isHurt)
            sprite.setColor(sf::Color(255, 100, 100));
        else
            sprite.setColor(sf::Color::White);

        // Auto detect from anywhere
        if (len > 10.0f) 
        {
            dir /= len;
            sprite.move(dir * speed * dt);

            if (std::abs(dir.x) > std::abs(dir.y))
                currentDir = (dir.x > 0) ? Dir::East : Dir::West;
            else
                currentDir = (dir.y > 0) ? Dir::South : Dir::North;
        }

        animSouth.playing = animNorth.playing =
            animEast.playing = animWest.playing = true;

        animSouth.update(dt);
        animNorth.update(dt);
        animEast.update(dt);
        animWest.update(dt);

        applyCurrentTexture();

        attackTimer += dt;
    }
};
