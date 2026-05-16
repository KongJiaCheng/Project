#pragma once
#include <SFML/Graphics.hpp>
#include "Animation.h"
#include <cmath>
#include <functional>

class Animal
{
public:
    enum class Dir { South, North, East, West };

    sf::Sprite sprite;

    Animation animSouth;
    Animation animNorth;
    Animation animEast;
    Animation animWest;

    Dir currentDir = Dir::South;

    float health = 30.f;
    float maxHealth = 30.f;
    float speed = 40.f;

    float attackDamage = 8.f;
    float attackCooldown = 1.2f;
    float attackTimer = 0.f;

    bool droppedLoot = false;
    bool isHurt = false;
    float hurtTimer = 0.f;

    Animal() = default;

    Animal(const sf::Texture& tex, sf::Vector2f pos, float hp)
        : health(hp), sprite(tex)
    {
        sprite.setPosition(pos);
    }

    virtual ~Animal() = default;

    void loadAnimations(
        const std::string& south,
        const std::string& north,
        const std::string& east,
        const std::string& west,
        int framesPerDir)
    {
        animSouth.loadFromFolder(south, framesPerDir);
        animNorth.loadFromFolder(north, framesPerDir);
        animEast.loadFromFolder(east, framesPerDir);
        animWest.loadFromFolder(west, framesPerDir);

        sprite.setTexture(animSouth.getCurrentTexture(), true);
        sprite.setOrigin({ 32.f, 32.f });
    }

    virtual void update(float dt, const sf::Vector2f& playerPos, std::function<bool(sf::Vector2f)> isBlocked = nullptr)
    {
        sf::Vector2f pos = sprite.getPosition();
        sf::Vector2f dir = pos - playerPos;

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

        if (len > 0.01f)
        {
            dir /= len;
            sf::Vector2f moveVec = dir * speed * dt;
            
            sf::Vector2f nextPos = pos + moveVec;
            if (!isBlocked || !isBlocked(nextPos)) {
                sprite.move(moveVec);
            }
            else {
                // Try sliding
                if (!isBlocked({ pos.x + moveVec.x, pos.y })) sprite.move({ moveVec.x, 0.f });
                else if (!isBlocked({ pos.x, pos.y + moveVec.y })) sprite.move({ 0.f, moveVec.y });
            }

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

    void applyCurrentTexture()
    {
        switch (currentDir)
        {
        case Dir::South: sprite.setTexture(animSouth.getCurrentTexture(), true); break;
        case Dir::North: sprite.setTexture(animNorth.getCurrentTexture(), true); break;
        case Dir::East:  sprite.setTexture(animEast.getCurrentTexture(), true); break;
        case Dir::West:  sprite.setTexture(animWest.getCurrentTexture(), true); break;
        }
    }

    virtual int getFoodItemID() const = 0;
    virtual const sf::Texture* getFoodTexture() const = 0;
    virtual int getFoodDropCount() const = 0;
};
