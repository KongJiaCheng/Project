#pragma once
#include <SFML/Graphics.hpp>
#include "ItemID.h"

struct Food
{
    int itemID;
    float hungerRestore;
    const sf::Texture* texture;

    Food(
        int id = 0,
        float hunger = 0.f,
        const sf::Texture* tex = nullptr
    )
        : itemID(id),
        hungerRestore(hunger),
        texture(tex)
    {
    }
};

bool consumeFoodItem(int itemID, float& hunger, int& hungerMultiplier, float maxHunger, int maxMultiplier, float& hp, bool disasterActive = false);
