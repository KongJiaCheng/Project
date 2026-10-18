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

    float wanderTimer = 0.f;
    sf::Vector2f wanderDir = { 0.f, 0.f };
    bool isWandering = false;

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
        sf::Vector2f toPlayer = pos - playerPos;
        float distToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

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

        sf::Vector2f moveDir(0.f, 0.f);
        float currentSpeed = speed;

        // Flee if hurt or if the player is very close (< 180px)
        if (isHurt || distToPlayer < 180.f)
        {
            if (distToPlayer > 0.01f)
            {
                moveDir = toPlayer / distToPlayer;
                currentSpeed = speed * 1.5f; // sprint when fleeing
            }
        }
        else
        {
            // Gentle wandering & idling when peaceful
            wanderTimer -= dt;
            if (wanderTimer <= 0.f)
            {
                // 50% chance idle, 50% chance wander
                if ((rand() % 100) < 50)
                {
                    isWandering = false;
                    wanderDir = { 0.f, 0.f };
                    wanderTimer = 2.0f + static_cast<float>(rand() % 30) / 10.f; // idle 2-5s
                }
                else
                {
                    isWandering = true;
                    float angle = (static_cast<float>(rand()) / RAND_MAX) * 6.2831853f;
                    wanderDir = { std::cos(angle), std::sin(angle) };
                    wanderTimer = 1.5f + static_cast<float>(rand() % 25) / 10.f; // wander 1.5-4s
                }
            }

            if (isWandering)
            {
                moveDir = wanderDir;
                currentSpeed = speed * 0.4f; // walk slowly
            }
        }

        float moveLen = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
        if (moveLen > 0.01f)
        {
            moveDir /= moveLen;
            sf::Vector2f moveVec = moveDir * currentSpeed * dt;

            sf::Vector2f nextPos = pos + moveVec;
            if (!isBlocked || !isBlocked(nextPos)) {
                sprite.move(moveVec);
            }
            else {
                // Try sliding
                if (!isBlocked({ pos.x + moveVec.x, pos.y })) sprite.move({ moveVec.x, 0.f });
                else if (!isBlocked({ pos.x, pos.y + moveVec.y })) sprite.move({ 0.f, moveVec.y });
                else {
                    wanderDir = -wanderDir;
                }
            }

            if (std::abs(moveDir.x) > std::abs(moveDir.y))
                currentDir = (moveDir.x > 0) ? Dir::East : Dir::West;
            else
                currentDir = (moveDir.y > 0) ? Dir::South : Dir::North;

            animSouth.playing = animNorth.playing =
                animEast.playing = animWest.playing = true;
        }
        else
        {
            animSouth.playing = animNorth.playing =
                animEast.playing = animWest.playing = false;
        }

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

    sf::Vector2f getPosition() const
    {
        return sprite.getPosition();
    }

};
