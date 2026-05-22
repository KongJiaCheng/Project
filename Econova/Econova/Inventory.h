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

    // NEW FUNCTIONS (must appear here!)
    float computeX(sf::RenderWindow& window);
    float computeY(sf::RenderWindow& window);
    int getSelectedSlot() const { return selectedSlot; }
    bool isEquipped(int slot) const { return slotEquipped[slot]; }
    void toggleEquipped(int slot) { slotEquipped[slot] = !slotEquipped[slot]; }
    void setEquipped(int slot, bool eq) { slotEquipped[slot] = eq; }
    int getDurability(int slot) const { return slotDurability[slot]; }
    void setDurability(int slot, int val) { slotDurability[slot] = val; }
    bool hasItem(int slot) const { return slots[slot].occupied; }
    const sf::Texture* getItemTexture(int slot) const
    {
        return slots[slot].icon ? &slots[slot].icon->getTexture() : nullptr;
    }
    int getItemCount(int slot) const { return slots[slot].count; }
    int getItemID(int slot) const { return slots[slot].id; }
    void clear();
    void clearSlot(int slot);
    void consumeItem(int slot)
    {
        if (slots[slot].count > 0)
        {
            slots[slot].count--;
        }
        
        if (slots[slot].count <= 0)
        {
            slots[slot].occupied = false;
            slots[slot].icon.reset();
            slots[slot].count = 0;
            if (slotEquipped[slot]) {
                slotEquipped[slot] = false;
            }
        }
    }

private:
    // Layout settings
    static const int SLOT_COUNT = 20;
    float boxW = 100.f;
    float boxH = 100.f;

    // UI elements
    sf::RectangleShape slotBox[SLOT_COUNT];
    std::unique_ptr<sf::Text> countText;

    sf::Font font;
    int selectedSlot = -1;
    bool slotEquipped[SLOT_COUNT];
    int slotDurability[SLOT_COUNT];
    int slotMaxDurability[SLOT_COUNT];
    
    sf::Clock doubleClickClock;
    int lastClickedSlot = -1;

    struct Slot
    {
        int id = 0;
        std::unique_ptr<sf::Sprite> icon;
        int count = 0;
        bool occupied = false;
    };

    Slot slots[SLOT_COUNT];


};
