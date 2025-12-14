#pragma once
#include <SFML/Graphics.hpp>

class Item
{
public:
    int id;
    int count;
    sf::Sprite sprite;  // must be constructed with texture

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
