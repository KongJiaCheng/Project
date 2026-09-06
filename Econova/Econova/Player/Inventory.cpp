#include "Inventory.h"
#include "ItemID.h"
#include "ViewUtils.h"
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
        slotEquipped[i] = false;
        slotDurability[i] = 100;
        slotMaxDurability[i] = 100;
    }
}

void Inventory::setItem(int index, int id, const sf::Texture& texture, int count)
{
    if (index < 0 || index >= SLOT_COUNT) return;

    slots[index].id = id;
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
    window.setView(GetUIView(&window));

    float xStart = computeX(window);
    float yStart = computeY(window);

    // Draw slots & icons in 2 rows of 10
    for (int i = 0; i < SLOT_COUNT; ++i)
    {
        int row = i / 10;
        int col = i % 10;
        float px = xStart + col * (boxW + 4.f);
        float py = yStart + row * (boxH + 4.f);

        slotBox[i].setPosition({ px, py });
        if (i == selectedSlot) {
            slotBox[i].setOutlineColor(sf::Color::Yellow);
            slotBox[i].setOutlineThickness(3.f);
        } else {
            slotBox[i].setOutlineColor(sf::Color::Black);
            slotBox[i].setOutlineThickness(2.f);
        }
        window.draw(slotBox[i]);

        if (slots[i].occupied && slots[i].icon)
        {
            slots[i].icon->setPosition({ px + 5.f, py + 5.f });
            
            // Re-scale if needed (though setItem/addItem does it, this is safe)
            sf::Vector2u ts = slots[i].icon->getTexture().getSize();
            float scaleX = (boxW - 10.f) / static_cast<float>(ts.x);
            float scaleY = (boxH - 10.f) / static_cast<float>(ts.y);
            float scale = std::min(scaleX, scaleY);
            slots[i].icon->setScale({ scale, scale });

            window.draw(*slots[i].icon);

            countText->setString(std::to_string(slots[i].count));
            countText->setPosition({
                px + boxW - 25,
                py + boxH - 25
                });
            window.draw(*countText);

            if (slotEquipped[i]) {
                sf::Text tickText(font, "V", 24);
                tickText.setFillColor(sf::Color::Green);
                tickText.setStyle(sf::Text::Bold);
                tickText.setPosition({ px + 5, py + 2 });
                window.draw(tickText);

                // Draw durability bar
                sf::RectangleShape durBg({ boxW - 10, 5 });
                durBg.setFillColor(sf::Color::Red);
                durBg.setPosition({ px + 5, py + boxH - 10 });

                sf::RectangleShape durFg({ (boxW - 10) * ((float)slotDurability[i] / (float)slotMaxDurability[i]), 5 });
                durFg.setFillColor(sf::Color::Green);
                durFg.setPosition(durBg.getPosition());

                window.draw(durBg);
                window.draw(durFg);
            }
        }
    }

    // ---- Tooltip for hovered slot ----
    if (hoveredSlot >= 0 && hoveredSlot < SLOT_COUNT &&
        slots[hoveredSlot].occupied)
    {
        std::string name = getItemName(slots[hoveredSlot].id);
        if (!name.empty())
        {
            sf::Text tipText(font, name, 22);
            tipText.setFillColor(sf::Color::White);
            tipText.setStyle(sf::Text::Bold);

            sf::FloatRect tb = tipText.getLocalBounds();
            const float padX = 18.f, padY = 10.f;
            float tw = tb.size.x + padX * 2.f;
            float th = 34.f;

            sf::Vector2f slotPos = slotBox[hoveredSlot].getPosition();
            float tx = slotPos.x + boxW / 2.f - tw / 2.f;
            float ty = slotPos.y - th - 10.f;

            // Clamp to screen edges
            float screenW = 1920.f;
            if (tx < 4.f) tx = 4.f;
            if (tx + tw > screenW - 4.f) tx = screenW - 4.f - tw;

            // Background pill
            sf::RectangleShape tipBg({ tw, th });
            tipBg.setPosition({ tx, ty });
            tipBg.setFillColor(sf::Color(20, 20, 30, 220));
            tipBg.setOutlineThickness(2.f);
            tipBg.setOutlineColor(sf::Color(120, 200, 255, 200));
            window.draw(tipBg);

            // Accent bar on left edge
            sf::RectangleShape accent({ 4.f, th });
            accent.setPosition({ tx, ty });
            accent.setFillColor(sf::Color(100, 200, 255, 230));
            window.draw(accent);

            // Text centred in pill
            tipText.setPosition({
                tx + padX,
                ty + (th - tb.size.y) / 2.f - tb.position.y
            });
            window.draw(tipText);
        }
    }

    window.setView(old);
}


void Inventory::handleClick(const sf::Vector2i& mousePos, sf::RenderWindow& window)
{
    sf::Vector2f uiPos = window.mapPixelToCoords(mousePos, GetUIView(&window));

    int slot = getSlotAt(uiPos);
    if (slot != -1)
    {
        selectedSlot = slot;

        // check double click
        if (slot == lastClickedSlot && doubleClickClock.getElapsedTime().asSeconds() < 0.5f) {
            if (slots[slot].occupied) {
                int id = slots[slot].id;
                if (id == 4 || id == 6 || id == 14) { // ITEM_WOODEN_SWORD, ITEM_SHIELD, or ITEM_WRENCH (14)
                    bool newEquip = !slotEquipped[slot];
                    if (newEquip) {
                        for (int j = 0; j < SLOT_COUNT; j++) {
                            if (slots[j].occupied && slots[j].id == id) {
                                slotEquipped[j] = false;
                            }
                        }
                    }
                    slotEquipped[slot] = newEquip;
                }
            }
        }
        
        lastClickedSlot = slot;
        doubleClickClock.restart();
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

void Inventory::updateHover(const sf::Vector2i& mousePos, sf::RenderWindow& window)
{
    sf::Vector2f uiPos = window.mapPixelToCoords(mousePos, GetUIView(&window));
    hoveredSlot = getSlotAt(uiPos);
}
// Item display name lookup
std::string Inventory::getItemName(int id)
{
    switch (id)
    {
    case ITEM_NONE:          return "";
    case ITEM_TREE:          return "Tree";
    case ITEM_STONE:         return "Stone";
    case ITEM_WOOD:          return "Wood";
    case ITEM_WOODEN_SWORD:  return "Wooden Sword";
    case ITEM_SAPLING:       return "Sapling";
    case ITEM_SHIELD:        return "Shield";
    case ITEM_FURNACE:       return "Furnace";
    case ITEM_WOOD_TILE:     return "Wood Tile";
    case ITEM_WOOD_WALL:     return "Wood Wall";
    case ITEM_WOOD_DOOR:     return "Wood Door";
    case ITEM_WOOD_ROOF:     return "Wood Roof";
    case ITEM_WOOD_STAIRS:   return "Wood Stairs";
    case ITEM_BED:           return "Bed";
    case ITEM_WRENCH:        return "Wrench";
    case ITEM_CHICKEN_MEAT:  return "Chicken Meat";
    case ITEM_PIG_MEAT:      return "Pig Meat";
    case ITEM_ZOMBIE_FLESH:  return "Zombie Flesh";
    case ITEM_COOKED_CHICKEN:return "Cooked Chicken";
    case ITEM_COOKED_PIG:    return "Cooked Pork";
    case ITEM_SHEEP_MEAT:    return "Sheep Meat";
    case ITEM_COOKED_SHEEP:  return "Cooked Lamb";
    case ITEM_WOOL:          return "Wool";
    default:                 return "Unknown Item";
    }
}


void Inventory::addItem(int id, const sf::Texture& texture, int amount)
{
    // 1) Try stacking in a slot with the same ID
    for (int i = 0; i < SLOT_COUNT; i++)
    {
        if (slots[i].occupied && slots[i].id == id)
        {
            slots[i].count += amount;
            return;
        }
    }

    // 2) Place in first empty slot
    for (int i = 0; i < SLOT_COUNT; i++)
    {
        if (!slots[i].occupied)
        {
            slots[i].id = id;
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
            if (id == 4) { // ITEM_WOODEN_SWORD
                slotDurability[i] = 100;
                slotMaxDurability[i] = 100;
            } else if (id == 6) { // ITEM_SHIELD
                slotDurability[i] = 500;
                slotMaxDurability[i] = 500;
            }
            return;
        }
    }

    std::cout << "Inventory full — cannot pick up item\n";
}


// Will be updated every frame to lock to window
float Inventory::computeX(sf::RenderWindow& window)
{
    float totalWidth = 10 * (boxW + 4.f);
    return (1920.f - totalWidth) / 2.f;
}

float Inventory::computeY(sf::RenderWindow& window)
{
    float totalHeight = 2 * (boxH + 4.f);
    return 1080.f - (totalHeight + 40.f); // 40px above bottom
}

// ----------------------------------------------------
// New Inventory Persistence Helper Functions
// ----------------------------------------------------

void Inventory::clear()
{
    for (int i = 0; i < SLOT_COUNT; ++i)
    {
        slots[i].count = 0;
        slots[i].occupied = false;
        slots[i].icon.reset();
    }
}

void Inventory::clearSlot(int slot)
{
    if (slot >= 0 && slot < SLOT_COUNT) {
        slots[slot].count = 0;
        slots[slot].occupied = false;
        slots[slot].icon.reset();
    }
}

#include "Food.h"
#include "StructureSystem.h"

bool handleInventoryInteractionClick(
    sf::Vector2f uiPos,
    sf::Vector2i mousePixelPos,
    sf::RenderWindow& window,
    Inventory& inventory,
    bool& inventoryOpen,
    bool tradeMenuOpen,
    bool p2pTradeOpen,
    const std::vector<PlacedCampfire>& campfires,
    const sf::Texture& saplingTex,
    const sf::Texture campfireFrames[8],
    bool& saplingPlacementMode,
    bool& saplingFollowingCursor,
    int& saplingFromInventorySlot,
    float& saplingRotation,
    bool& placementMode,
    bool& placementFollowingCursor,
    int& placementItemID,
    int& placementFromInventorySlot,
    float& hunger,
    int& hungerMultiplier,
    float maxHunger,
    int MAX_MULTIPLIER,
    float& hp
) {
    if (!inventoryOpen) return false;

    int clickedSlot = inventory.getSlotAt(uiPos);
    if (clickedSlot != -1)
    {
        inventory.handleClick(mousePixelPos, window);

        if (inventory.hasItem(clickedSlot))
        {
            const sf::Texture* tex = inventory.getItemTexture(clickedSlot);
            bool campfireOpen = false;
            for (const auto& f : campfires) {
                if (f.menuOpen) {
                    campfireOpen = true;
                    break;
                }
            }

            if (!tradeMenuOpen && !p2pTradeOpen && !campfireOpen)
            {
                if (tex == &saplingTex)
                {
                    saplingPlacementMode = true;
                    saplingFollowingCursor = false;
                    saplingFromInventorySlot = clickedSlot;
                    saplingRotation = 0.f;
                    inventoryOpen = false;
                    return true;
                }

                int itemId = inventory.getItemID(clickedSlot);
                if ((itemId >= ITEM_FURNACE && itemId <= ITEM_BED) || (itemId == ITEM_NONE && tex == &campfireFrames[0])) {
                    inventoryOpen = false;
                    placementMode = true;
                    placementFollowingCursor = false;
                    placementItemID = itemId;
                    placementFromInventorySlot = clickedSlot;
                    return true;
                }

                if (consumeFoodItem(itemId, hunger, hungerMultiplier, maxHunger, MAX_MULTIPLIER, hp)) {
                    inventory.consumeItem(clickedSlot);
                    return true;
                }
            }
        }
        return true;
    }
    return false;
}

