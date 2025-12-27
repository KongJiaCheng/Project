#pragma once
#include <SFML/Graphics.hpp>
#include <memory>

class Inventory
{
public:
    Inventory();
    void draw(sf::RenderWindow& window);
    void handleClick(const sf::Vector2i& mousePos, sf::RenderWindow& window);
    void setItem(int index, const sf::Texture& texture, int count);
    int getSlotAt(sf::Vector2f mousePos);

    void addItem(int id, const sf::Texture& texture, int count);

private:
    // Layout settings
    static const int SLOT_COUNT = 10;
    float boxW = 200.f;
    float boxH = 200.f;

    // UI elements
    sf::RectangleShape slotBox[SLOT_COUNT];
    std::unique_ptr<sf::Text> countText;

    sf::Font font;
    int selectedSlot = -1;

    struct Slot
    {
        std::unique_ptr<sf::Sprite> icon;
        int count = 0;
        bool occupied = false;
    };

    Slot slots[SLOT_COUNT];

    // NEW FUNCTIONS (must appear here!)
    float computeX(sf::RenderWindow& window);
    float computeY(sf::RenderWindow& window);
};
