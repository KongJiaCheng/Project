#include "Inventory.h"
#include <iostream>
#include <algorithm> // std::min

Inventory::Inventory()
{
    // Load font first
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        std::cerr << "Warning: failed to load font\n";
    }

    // countText will be reused for drawing counts (constructed here)
    countText = std::make_unique<sf::Text>(font, "", 18);
    countText->setFillColor(sf::Color::White);

    // initialize slot boxes and positions
    for (int i = 0; i < SLOT_COUNT; ++i)
    {
        slotBox[i].setSize({ boxW, boxH });
        slotBox[i].setFillColor(sf::Color(50, 50, 50, 200));
        slotBox[i].setOutlineThickness(2.f);
        slotBox[i].setOutlineColor(sf::Color::Black);

        slots[i].icon = nullptr;
        slots[i].count = 0;
        slots[i].occupied = false;
    }
}

void Inventory::setItem(int index, const sf::Texture& texture, int count)
{
    if (index < 0 || index >= SLOT_COUNT) return;

    // create sprite (sf::Sprite requires a texture on construction in SFML3)
    slots[index].icon = std::make_unique<sf::Sprite>(texture);

    // position and optionally scale the icon to fit into slot
    slots[index].icon->setPosition({
        slotBox[index].getPosition().x + 8.f,
        slotBox[index].getPosition().y + 8.f
        });

    // optional scale to fit inside box (assumes texture size > 0)
    sf::Vector2u ts = texture.getSize();
    if (ts.x > 0 && ts.y > 0) {
        float scaleX = (boxW - 16.f) / static_cast<float>(ts.x);
        float scaleY = (boxH - 16.f) / static_cast<float>(ts.y);
        float scale = std::min(scaleX, scaleY);
        slots[index].icon->setScale({ scale, scale });
    }

    slots[index].count = count;
    slots[index].occupied = true;
}

void Inventory::draw(sf::RenderWindow& window)
{
    sf::View old = window.getView();
    window.setView(window.getDefaultView());

    float xStart = computeX(window);
    float yPos = computeY(window);

    // Update slot positions every frame
    for (int i = 0; i < SLOT_COUNT; ++i)
    {
        float px = xStart + i * (boxW + 4.f);
        slotBox[i].setPosition({ px, yPos });

        if (slots[i].occupied && slots[i].icon)
        {
            slots[i].icon->setPosition({ px + 8, yPos + 8 });
        }
    }

    // Draw slots & icons
    for (int i = 0; i < SLOT_COUNT; ++i)
    {
        window.draw(slotBox[i]);

        if (slots[i].occupied && slots[i].icon)
        {
            window.draw(*slots[i].icon);

            countText->setString(std::to_string(slots[i].count));
            countText->setPosition({
                slotBox[i].getPosition().x + boxW - 28,
                slotBox[i].getPosition().y + boxH - 28
                });
            window.draw(*countText);
        }
    }

    window.setView(old);
}


void Inventory::handleClick(const sf::Vector2i& mousePos, sf::RenderWindow& window)
{
    sf::Vector2f worldPos = window.mapPixelToCoords(mousePos);

    int slot = getSlotAt(worldPos);
    if (slot != -1)
    {
        selectedSlot = slot;
    }
}


int Inventory::getSlotAt(sf::Vector2f mousePos)
{
    // Loop through all slot boxes
    for (int i = 0; i < SLOT_COUNT; ++i)
    {
        if (slotBox[i].getGlobalBounds().contains(mousePos))
        {
            return i;   // return slot index
        }
    }

    return -1; // no slot clicked
}

void Inventory::addItem(int id, const sf::Texture& texture, int amount)
{
    // 1) Try stacking in a slot with the same texture
    for (int i = 0; i < SLOT_COUNT; i++)
    {
        if (slots[i].occupied && slots[i].icon)
        {
            // Compare texture pointers to detect same item type
            if (&slots[i].icon->getTexture() == &texture)
            {
                slots[i].count += amount;
                return;
            }
        }
    }

    // 2) Place in first empty slot
    for (int i = 0; i < SLOT_COUNT; i++)
    {
        if (!slots[i].occupied)
        {
            slots[i].icon = std::make_unique<sf::Sprite>(texture);
            slots[i].icon->setPosition({
                slotBox[i].getPosition().x + 8,
                slotBox[i].getPosition().y + 8
                });

            // Scale to fit
            sf::Vector2u ts = texture.getSize();
            if (ts.x > 0 && ts.y > 0)
            {
                float scaleX = (boxW - 16.f) / static_cast<float>(ts.x);
                float scaleY = (boxH - 16.f) / static_cast<float>(ts.y);
                float scale = std::min(scaleX, scaleY);
                slots[i].icon->setScale({ scale, scale });
            }

            slots[i].count = amount;   // show count = 1
            slots[i].occupied = true;
            return;
        }
    }

    std::cout << "Inventory full — cannot pick up item\n";
}


// Will be updated every frame to lock to window
float Inventory::computeX(sf::RenderWindow& window)
{
    float totalWidth = SLOT_COUNT * (boxW + 4.f);
    return (window.getSize().x - totalWidth) / 2.f;
}

float Inventory::computeY(sf::RenderWindow& window)
{
    return window.getSize().y - (boxH + 40.f); // 40px above bottom
}


