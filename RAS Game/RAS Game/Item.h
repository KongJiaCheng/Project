#pragma once
#include <SFML/Graphics.hpp>

class Item
{
public:
    int id;
    int count;
    sf::Sprite sprite;  // must be constructed with texture

    float health = 100.f;
    float maxHealth = 100.f;
    bool isHurt = false;
    float hurtTimer = 0.f;

    Item(int itemID, int amount, const sf::Texture& tex, sf::Vector2f pos)
        : id(itemID), count(amount), sprite(tex)
    {
        sprite.setPosition(pos);
    }

    const sf::FloatRect getBounds() const
    {
        return sprite.getGlobalBounds();
    }
};
