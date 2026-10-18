#include "Inventory.h"
#include "ItemID.h"
#include "ViewUtils.h"
#include <iostream>
#include <algorithm> // std::min

Inventory::Inventory()
{
    // Load font with fallbacks
    bool fontLoaded = font.openFromFile("C:/Windows/Fonts/arial.ttf");
    if (!fontLoaded) fontLoaded = font.openFromFile("C:/Windows/Fonts/calibri.ttf");
    if (!fontLoaded) fontLoaded = font.openFromFile("C:/Windows/Fonts/segoeui.ttf");
    if (!fontLoaded) {
        std::cerr << "Warning: failed to load font\n";
    }

    // countText will be reused for drawing counts (constructed here)
    countText = std::make_unique<sf::Text>(font, "", 18);
    countText->setFillColor(sf::Color::White);

    // initialize slot boxes and positions
    float totalWidth = 10 * (boxW + 4.f);
    float xStart = (1920.f - totalWidth) / 2.f;
    float totalHeight = 2 * (boxH + 4.f);
    float yStart = 1080.f - (totalHeight + 40.f);

    for (int i = 0; i < SLOT_COUNT; ++i)
    {
        int row = i / 10;
        int col = i % 10;
        float px = xStart + col * (boxW + 4.f);
        float py = yStart + row * (boxH + 4.f);

        slotBox[i].setSize({ boxW, boxH });
        slotBox[i].setPosition({ px, py });
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

    // Automatically update hovered slot directly each frame
    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
    sf::Vector2f uiMouse = window.mapPixelToCoords(mousePixel, GetUIView(&window));
    hoveredSlot = getSlotAt(uiMouse);

    // ---- Tooltip for hovered slot (or selected slot if not hovering another slot) ----
    int tipSlot = hoveredSlot;
    if ((tipSlot < 0 || tipSlot >= SLOT_COUNT || !slots[tipSlot].occupied) &&
        (selectedSlot >= 0 && selectedSlot < SLOT_COUNT && slots[selectedSlot].occupied))
    {
        tipSlot = selectedSlot;
    }

    if (tipSlot >= 0 && tipSlot < SLOT_COUNT && slots[tipSlot].occupied)
    {
        std::string name = getItemName(slots[tipSlot].id);
        if (slots[tipSlot].count > 1) {
            name += " (x" + std::to_string(slots[tipSlot].count) + ")";
        }
        if (slots[tipSlot].id == 4 || slots[tipSlot].id == 6) {
            name += " [" + std::to_string(slotDurability[tipSlot]) + "/" + std::to_string(slotMaxDurability[tipSlot]) + "]";
        }

        if (!name.empty())
        {
            sf::Text tipText(font, name, 22);
            tipText.setFillColor(sf::Color::White);
            tipText.setStyle(sf::Text::Bold);

            sf::FloatRect tb = tipText.getLocalBounds();
            const float padX = 18.f, padY = 10.f;
            float tw = tb.size.x + padX * 2.f;
            float th = 36.f;

            // Position tooltip above the column containing the slot so it never covers any slot
            int col = tipSlot % 10;
            float colX = xStart + col * (boxW + 4.f);
            float tx = colX + boxW / 2.f - tw / 2.f;
            float ty = yStart - th - 12.f;

            // Clamp to screen edges
            float screenW = 1920.f;
            if (tx < 10.f) tx = 10.f;
            if (tx + tw > screenW - 10.f) tx = screenW - 10.f - tw;

            // Background pill
            sf::RectangleShape tipBg({ tw, th });
            tipBg.setPosition({ tx, ty });
            tipBg.setFillColor(sf::Color(20, 20, 30, 230));
            tipBg.setOutlineThickness(2.f);
            tipBg.setOutlineColor(sf::Color(120, 200, 255, 220));
            window.draw(tipBg);

            // Accent bar on left edge
            sf::RectangleShape accent({ 4.f, th });
            accent.setPosition({ tx, ty });
            accent.setFillColor(sf::Color(100, 200, 255, 240));
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

void Inventory::setSelectedSlot(int slot)
{
    if (slot >= -1 && slot < SLOT_COUNT) {
        selectedSlot = slot;
        if (slot >= 0 && slots[slot].occupied) {
            bannerText = getItemName(slots[slot].id);
            if (slots[slot].count > 1) {
                bannerText += " (x" + std::to_string(slots[slot].count) + ")";
            }
            if (slots[slot].id == 4 || slots[slot].id == 6) {
                bannerText += " [" + std::to_string(slotDurability[slot]) + "/" + std::to_string(slotMaxDurability[slot]) + "]";
            }
            bannerTimer = 2.5f;
            bannerClock.restart();
        } else {
            bannerText = "";
            bannerTimer = 0.f;
        }
    }
}

void Inventory::drawItemBanner(sf::RenderWindow& window)
{
    if (bannerTimer <= 0.f || bannerText.empty()) return;

    float dt = bannerClock.restart().asSeconds();
    bannerTimer -= dt;
    if (bannerTimer <= 0.f) return;

    sf::View old = window.getView();
    window.setView(GetUIView(&window));

    sf::Text bText(font, bannerText, 24);
    bText.setStyle(sf::Text::Bold);
    sf::FloatRect tb = bText.getLocalBounds();

    const float padX = 24.f;
    float bw = tb.size.x + padX * 2.f;
    float bh = 44.f;
    float bx = (1920.f - bw) / 2.f;
    float by = 1080.f - 160.f;

    float alpha = 240.f;
    if (bannerTimer < 0.5f) {
        alpha = (bannerTimer / 0.5f) * 240.f;
    }
    std::uint8_t a = static_cast<std::uint8_t>(std::clamp(alpha, 0.f, 255.f));

    sf::RectangleShape bg({ bw, bh });
    bg.setPosition({ bx, by });
    bg.setFillColor(sf::Color(18, 22, 32, a));
    bg.setOutlineThickness(2.f);
    bg.setOutlineColor(sf::Color(100, 200, 255, a));
    window.draw(bg);

    sf::RectangleShape acc({ 4.f, bh });
    acc.setPosition({ bx, by });
    acc.setFillColor(sf::Color(120, 220, 255, a));
    window.draw(acc);

    bText.setFillColor(sf::Color(255, 255, 255, a));
    bText.setPosition({
        bx + padX,
        by + (bh - tb.size.y) / 2.f - tb.position.y
    });
    window.draw(bText);

    window.setView(old);
}

void Inventory::handleClick(const sf::Vector2i& mousePos, sf::RenderWindow& window)
{
    sf::Vector2f uiPos = window.mapPixelToCoords(mousePos, GetUIView(&window));

    int slot = getSlotAt(uiPos);
    if (slot != -1)
    {
        if (selectedSlot == slot && doubleClickClock.getElapsedTime().asSeconds() > 0.5f) {
            setSelectedSlot(-1);
        } else {
            setSelectedSlot(slot);
        }

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
    case ITEM_DROP_STONE:    return "Stone";
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
    case ITEM_CHEST:         return "Chest";
    case ITEM_CHICKEN_MEAT:  return "Chicken Meat";
    case ITEM_PIG_MEAT:      return "Pig Meat";
    case ITEM_ZOMBIE_FLESH:  return "Zombie Flesh";
    case ITEM_COOKED_CHICKEN:return "Cooked Chicken";
    case ITEM_COOKED_PIG:    return "Cooked Pork";
    case ITEM_SHEEP_MEAT:    return "Sheep Meat";
    case ITEM_COOKED_SHEEP:  return "Cooked Lamb";
    case ITEM_WOOL:          return "Wool";
    case ITEM_COAL:          return "Coal";
    case ITEM_COPPER_ORE:    return "Copper Ore";
    case ITEM_COPPER_BAR:    return "Copper Bar";
    case ITEM_IRON_ORE:      return "Iron Ore";
    case ITEM_IRON_BAR:      return "Iron Bar";
    case ITEM_GOLD_ORE:      return "Gold Ore";
    case ITEM_GOLD_BAR:      return "Gold Bar";
    case ITEM_DIAMOND:       return "Diamond";
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
    float& hp,
    bool disasterActive,
    bool furnaceMenuOpen,
    bool chestMenuOpen,
    bool quantityPromptOpen
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

            bool isInteractingWithMenu = tradeMenuOpen || p2pTradeOpen || campfireOpen
                                       || furnaceMenuOpen || chestMenuOpen || quantityPromptOpen;

            if (!isInteractingWithMenu)
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
                if ((itemId >= ITEM_FURNACE && itemId <= ITEM_BED) || itemId == ITEM_CHEST || (itemId == ITEM_NONE && tex == &campfireFrames[0])) {
                    inventoryOpen = false;
                    placementMode = true;
                    placementFollowingCursor = true;   // preview follows cursor immediately
                    placementItemID = itemId;
                    placementFromInventorySlot = clickedSlot;
                    return true;
                }

                if (consumeFoodItem(itemId, hunger, hungerMultiplier, maxHunger, MAX_MULTIPLIER, hp, disasterActive)) {
                    if (disasterActive && (itemId == ITEM_CHICKEN_MEAT || itemId == ITEM_PIG_MEAT || itemId == ITEM_SHEEP_MEAT || itemId == ITEM_ZOMBIE_FLESH)) {
                        inventory.showBanner("POISONED! Consuming raw meat during diarrhea epidemic damages HP!");
                    }
                    inventory.consumeItem(clickedSlot);
                    return true;
                }
            }
        }
        return true;
    }
    return false;
}

