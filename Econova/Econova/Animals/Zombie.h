#pragma once
#include "Monster.h"
#include "ItemID.h"
#include "ZombieFlesh.h"
#include <functional>

class Zombie : public Monster
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
        aS.loadFromFolder("Sprites/Monsters/Zombie/Walk/South", 4);
        aN.loadFromFolder("Sprites/Monsters/Zombie/Walk/North", 4);
        aE.loadFromFolder("Sprites/Monsters/Zombie/Walk/East", 4);
        aW.loadFromFolder("Sprites/Monsters/Zombie/Walk/West", 4);
        sharedSouth = std::move(aS.frames);
        sharedNorth = std::move(aN.frames);
        sharedEast = std::move(aE.frames);
        sharedWest = std::move(aW.frames);
        sharedLoaded = true;
    }

    Zombie(sf::Vector2f pos) : Monster(texture, pos, 50)
    {
        maxHealth = 500.f;
        health = maxHealth;
        speed = 80.f;
        attackDamage = 15.f;
        attackCooldown = 1.2f;

        loadSharedTextures();
        animSouth.sharedFrames = &sharedSouth;
        animNorth.sharedFrames = &sharedNorth;
        animEast.sharedFrames = &sharedEast;
        animWest.sharedFrames = &sharedWest;

        sprite.setTexture(animSouth.getCurrentTexture(), true);
        sprite.setOrigin({ 32.f, 32.f });

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

    void update(float dt, const sf::Vector2f& playerPos, std::function<bool(sf::Vector2f)> isBlocked = nullptr) override
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

        // Charge attack timer when within contact range (120px world units).
        // Cap (don't zero) when out of range — brief retreats don't reset full charge,
        // but cap at cooldown so zombies can't insta-hit on very first re-entry from far away.
        if (len <= 120.f)
            attackTimer += dt;
        else if (attackTimer > attackCooldown)
            attackTimer = attackCooldown;
    }
};
