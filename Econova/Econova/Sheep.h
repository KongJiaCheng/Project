#pragma once
#include "Animal.h"
#include "ItemID.h"
#include "SheepMeat.h"

class Sheep : public Animal
{
public:
    static sf::Texture texture;
    static std::vector<sf::Texture> sharedSouth;
    static std::vector<sf::Texture> sharedNorth;
    static std::vector<sf::Texture> sharedEast;
    static std::vector<sf::Texture> sharedWest;
    static bool sharedLoaded;

    static void loadSharedTextures();

    Sheep(sf::Vector2f pos);

    int getFoodItemID() const override
    {
        return ITEM_SHEEP_MEAT;
    }

    const sf::Texture* getFoodTexture() const override;

    int getFoodDropCount() const override
    {
        return 1 + rand() % 2;
    }
};
