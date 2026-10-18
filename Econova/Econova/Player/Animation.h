#pragma once
#include <SFML/Graphics.hpp>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include "ViewUtils.h"

class Animation
{
public:
    std::vector<sf::Texture> frames;
    const std::vector<sf::Texture>* sharedFrames = nullptr;
    float frameTime = 0.12f;
    float timer = 0.f;
    int current = 0;
    bool playing = false;

    Animation() = default;

    bool loadFromFolder(const std::string& folder, int frameCount, float fTime = 0.12f);
    void update(float dt);
    const sf::Texture& getCurrentTexture() const;
    void stopAtFirst();
};

struct AnimSet {
    Animation south, north, east, west;
    void load(const std::string& basePath, int frames);
    void update(float dt);
    void stop();
    void setPlaying(bool play);
    void setDirPlaying(int dir, bool play);
    const sf::Texture& getTexture(int dir);
};

void constructMaleAnimation(
    AnimSet& animWalk,
    AnimSet& animWalkWithSword,
    AnimSet& animWalkWithShield,
    AnimSet& animWalkWithShieldAndSword,
    AnimSet& animAttackWithSword,
    AnimSet& animAttackWithShieldAndSword,
    int frames = 6
);

void constructFemaleAnimation(
    AnimSet& animWalk,
    AnimSet& animWalkWithSword,
    AnimSet& animWalkWithShield,
    AnimSet& animWalkWithShieldAndSword,
    AnimSet& animAttackWithSword,
    AnimSet& animAttackWithShieldAndSword,
    int frames = 6
);