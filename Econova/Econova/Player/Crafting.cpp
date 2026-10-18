#include "Crafting.h"
#include "ItemID.h"
#include <algorithm>

void drawCraftMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::vector<CraftableItem>& craftableItems,
    const Inventory& inventory,
    const sf::Texture& woodTex,
    const sf::Texture& stoneTex,
    const sf::Texture& woolTex,
    bool swordCrafting,
    bool shieldCrafting
) {
    sf::RectangleShape craftBox({ 1300.f, 950.f });
    craftBox.setPosition({ (width - 1300.f) / 2.f, (height - 950.f) / 2.f });
    craftBox.setFillColor(sf::Color(30, 30, 35, 250));
    craftBox.setOutlineThickness(6.f);
    craftBox.setOutlineColor(sf::Color(80, 80, 90));
    window.draw(craftBox);

    // Header bar
    sf::RectangleShape header({ 1300.f, 80.f });
    header.setPosition(craftBox.getPosition());
    header.setFillColor(sf::Color(50, 50, 60));
    window.draw(header);

    sf::Text title(uiFont, "CRAFTING STATION", 48);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(sf::Color(255, 220, 100));
    title.setPosition({ craftBox.getPosition().x + 40.f, craftBox.getPosition().y + 15.f });
    window.draw(title);

    sf::RectangleShape closeBtn({ 60.f, 60.f });
    closeBtn.setPosition({ craftBox.getPosition().x + 1220.f, craftBox.getPosition().y + 10.f });
    closeBtn.setFillColor(sf::Color(200, 50, 50));
    window.draw(closeBtn);
    sf::Text closeTxt(uiFont, "X", 36);
    closeTxt.setPosition({ closeBtn.getPosition().x + 18.f, closeBtn.getPosition().y + 8.f });
    window.draw(closeTxt);

    int totalWood = 0, totalStone = 0, totalWool = 0;
    for (int i = 0; i < 20; i++) {
        if (inventory.hasItem(i)) {
            if (inventory.getItemTexture(i) == &woodTex) totalWood += inventory.getItemCount(i);
            if (inventory.getItemTexture(i) == &stoneTex) totalStone += inventory.getItemCount(i);
            if (inventory.getItemTexture(i) == &woolTex) totalWool += inventory.getItemCount(i);
        }
    }

    // Draw resource overview
    sf::Text resTxt(uiFont, "Resources: " + std::to_string(totalWood) + " Wood, " + std::to_string(totalStone) + " Stone, " + std::to_string(totalWool) + " Wool", 24);
    resTxt.setFillColor(sf::Color(180, 180, 180));
    resTxt.setPosition({ craftBox.getPosition().x + 500.f, craftBox.getPosition().y + 30.f });
    window.draw(resTxt);

    float startX = craftBox.getPosition().x + 50.f;
    float startY = craftBox.getPosition().y + 110.f;
    float itemW = 380.f, itemH = 190.f;
    float gapX = 30.f, gapY = 20.f;

    for (int i = 0; i < (int)craftableItems.size(); i++) {
        int row = i / 3;
        int col = i % 3;
        sf::Vector2f pos(startX + col * (itemW + gapX), startY + row * (itemH + gapY));

        sf::RectangleShape itemCard({ itemW, itemH });
        itemCard.setPosition(pos);
        itemCard.setFillColor(sf::Color(45, 45, 55));
        itemCard.setOutlineThickness(2.f);
        itemCard.setOutlineColor(sf::Color(70, 70, 80));
        window.draw(itemCard);

        const auto& item = craftableItems[i];
        if (item.tex) {
            sf::Sprite s(*item.tex);
            sf::Vector2u ts = s.getTexture().getSize();
            float scale = 140.f / std::max(ts.x, ts.y);
            s.setScale({ scale, scale });
            s.setOrigin({ ts.x / 2.f, ts.y / 2.f });
            s.setPosition({ pos.x + 80.f, pos.y + itemH / 2.f });
            window.draw(s);
        }

        sf::Text name(uiFont, item.name, 26);
        name.setStyle(sf::Text::Bold);
        name.setFillColor(sf::Color::White);
        name.setPosition({ pos.x + 160.f, pos.y + 20.f });
        window.draw(name);

        std::string reqs = "Cost:";
        if (item.woodReq > 0) reqs += "\n- " + std::to_string(item.woodReq) + " Wood";
        if (item.stoneReq > 0) reqs += "\n- " + std::to_string(item.stoneReq) + " Stone";
        if (item.woolReq > 0) reqs += "\n- " + std::to_string(item.woolReq) + " Wool";
        if (item.woodReq == 0 && item.stoneReq == 0 && item.woolReq == 0) reqs += "\nFree";

        sf::Text req(uiFont, reqs, 20);
        req.setFillColor(sf::Color(200, 200, 200));
        req.setPosition({ pos.x + 170.f, pos.y + 60.f });
        window.draw(req);

        sf::RectangleShape btn({ 140.f, 45.f });
        btn.setPosition({ pos.x + 210.f, pos.y + 130.f });

        bool canCraft = (totalWood >= item.woodReq && totalStone >= item.stoneReq && totalWool >= item.woolReq);
        bool inProgress = false;
        if (item.id == ITEM_WOODEN_SWORD && swordCrafting) inProgress = true;
        if (item.id == ITEM_SHIELD && shieldCrafting) inProgress = true;

        if (inProgress) {
            btn.setFillColor(sf::Color(120, 100, 0));
            window.draw(btn);
            sf::Text waitTxt(uiFont, "Crafting...", 18);
            waitTxt.setPosition({ btn.getPosition().x + 25.f, btn.getPosition().y + 10.f });
            window.draw(waitTxt);
        }
        else {
            btn.setFillColor(canCraft ? sf::Color(40, 160, 70) : sf::Color(80, 80, 80));
            window.draw(btn);
            sf::Text cTxt(uiFont, canCraft ? "CRAFT" : "LOCKED", 20);
            cTxt.setStyle(sf::Text::Bold);
            cTxt.setPosition({ btn.getPosition().x + (canCraft ? 38.f : 30.f), btn.getPosition().y + 10.f });
            window.draw(cTxt);
        }
    }
}

void drawPlacementControls(
    sf::RenderWindow& window,
    const sf::Font& font,
    unsigned int width,
    unsigned int height,
    const Inventory& inventory,
    const sf::Texture& woodTex,
    const sf::Texture& stoneTex
) {
    float btnW = 220.f, btnH = 70.f, gap = 40.f;
    float tWidth = btnW * 3.f + gap * 2.f;
    float sX = (width - tWidth) / 2.f;
    float yP = height - 110.f;

    sf::RectangleShape placeBtn({ btnW, btnH });
    placeBtn.setPosition({ sX, yP });

    int totalWood = 0, totalStone = 0;
    for (int i = 0; i < 20; i++) {
        if (inventory.hasItem(i)) {
            if (inventory.getItemTexture(i) == &woodTex) totalWood += inventory.getItemCount(i);
            if (inventory.getItemTexture(i) == &stoneTex) totalStone += inventory.getItemCount(i);
        }
    }
    if (totalWood >= 10 && totalStone >= 10) placeBtn.setFillColor(sf::Color(60, 180, 75));
    else placeBtn.setFillColor(sf::Color(100, 100, 100));
    placeBtn.setOutlineThickness(3.f);
    placeBtn.setOutlineColor(sf::Color::White);

    window.draw(placeBtn);
    sf::Text placeTxt(font, "Place", 36);
    sf::FloatRect pBounds = placeTxt.getLocalBounds();
    placeTxt.setPosition({ sX + (btnW - pBounds.size.x) / 2.f, yP + (btnH - pBounds.size.y) / 2.f - 8.f });
    window.draw(placeTxt);

    // ROTATE BUTTON
    sf::RectangleShape rotateBtn({ btnW, btnH });
    rotateBtn.setPosition({ sX + btnW + gap, yP });
    rotateBtn.setFillColor(sf::Color(60, 140, 200));
    rotateBtn.setOutlineThickness(3.f);
    rotateBtn.setOutlineColor(sf::Color::White);
    window.draw(rotateBtn);
    sf::Text rotateTxt(font, "Rotate", 36);
    sf::FloatRect rBounds = rotateTxt.getLocalBounds();
    rotateTxt.setPosition({ rotateBtn.getPosition().x + (btnW - rBounds.size.x) / 2.f, yP + (btnH - rBounds.size.y) / 2.f - 8.f });
    window.draw(rotateTxt);

    sf::RectangleShape cancelBtn({ btnW, btnH });
    cancelBtn.setPosition({ sX + (btnW + gap) * 2.f, yP });
    cancelBtn.setFillColor(sf::Color(200, 60, 60));
    cancelBtn.setOutlineThickness(3.f);
    cancelBtn.setOutlineColor(sf::Color::White);
    window.draw(cancelBtn);
    sf::Text cancelTxt(font, "Cancel", 36);
    sf::FloatRect cBounds = cancelTxt.getLocalBounds();
    cancelTxt.setPosition({ cancelBtn.getPosition().x + (btnW - cBounds.size.x) / 2.f, yP + (btnH - cBounds.size.y) / 2.f - 8.f });
    window.draw(cancelTxt);
}

void drawSaplingPlacementControls(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    float saplingRotation
) {
    const float btnW = 220.f, btnH = 70.f, gap = 40.f;
    const float tWidth = btnW * 3.f + gap * 2.f;
    const float startX = (width - tWidth) / 2.f;
    const float yPos = height - 110.f;

    // PLACE BUTTON
    sf::RectangleShape placeBtn({ btnW, btnH });
    placeBtn.setPosition({ startX, yPos });
    placeBtn.setFillColor(sf::Color(60, 180, 75));
    placeBtn.setOutlineThickness(3.f);
    placeBtn.setOutlineColor(sf::Color::White);
    window.draw(placeBtn);
    sf::Text placeTxt(uiFont, "Place", 36);
    placeTxt.setFillColor(sf::Color::White);
    sf::FloatRect pBounds = placeTxt.getLocalBounds();
    placeTxt.setPosition({ startX + (btnW - pBounds.size.x) / 2.f, yPos + (btnH - pBounds.size.y) / 2.f - 8.f });
    window.draw(placeTxt);

    // ROTATE BUTTON
    sf::RectangleShape rotateBtn({ btnW, btnH });
    rotateBtn.setPosition({ startX + btnW + gap, yPos });
    rotateBtn.setFillColor(sf::Color(60, 140, 200));
    rotateBtn.setOutlineThickness(3.f);
    rotateBtn.setOutlineColor(sf::Color::White);
    window.draw(rotateBtn);
    sf::Text rotateTxt(uiFont, "Rotate (" + std::to_string((int)saplingRotation) + ")", 28);
    rotateTxt.setFillColor(sf::Color::White);
    sf::FloatRect rBounds = rotateTxt.getLocalBounds();
    rotateTxt.setPosition({ startX + btnW + gap + (btnW - rBounds.size.x) / 2.f, yPos + (btnH - rBounds.size.y) / 2.f - 8.f });
    window.draw(rotateTxt);

    // CANCEL BUTTON
    sf::RectangleShape cancelBtn({ btnW, btnH });
    cancelBtn.setPosition({ startX + (btnW + gap) * 2.f, yPos });
    cancelBtn.setFillColor(sf::Color(200, 60, 60));
    cancelBtn.setOutlineThickness(3.f);
    cancelBtn.setOutlineColor(sf::Color::White);
    window.draw(cancelBtn);
    sf::Text cancelTxt(uiFont, "Cancel", 36);
    cancelTxt.setFillColor(sf::Color::White);
    sf::FloatRect cBounds = cancelTxt.getLocalBounds();
    cancelTxt.setPosition({ cancelBtn.getPosition().x + (btnW - cBounds.size.x) / 2.f, yPos + (btnH - cBounds.size.y) / 2.f - 8.f });
    window.draw(cancelTxt);
}

#include "ItemID.h"

bool handleCraftMenuClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& craftMenuOpen,
    const std::vector<CraftableItem>& craftableItems,
    Inventory& inventory,
    bool& swordCrafting,
    float& swordCraftTimer,
    bool& shieldCrafting,
    float& shieldCraftTimer
) {
    sf::FloatRect craftBox({ (width - 1300.f) / 2.f, (height - 950.f) / 2.f }, { 1300.f, 950.f });
    sf::FloatRect closeBtn({ craftBox.position.x + 1220.f, craftBox.position.y + 10.f }, { 60.f, 60.f });

    if (closeBtn.contains(uiPos)) {
        craftMenuOpen = false;
        return true;
    }

    int totalWood = 0, totalStone = 0, totalWool = 0;
    for (int i = 0; i < 20; i++) {
        if (inventory.hasItem(i)) {
            if (inventory.getItemID(i) == ITEM_WOOD) totalWood += inventory.getItemCount(i);
            if (inventory.getItemID(i) == ITEM_STONE) totalStone += inventory.getItemCount(i);
            if (inventory.getItemID(i) == ITEM_WOOL) totalWool += inventory.getItemCount(i);
        }
    }

    float startX = craftBox.position.x + 50.f;
    float startY = craftBox.position.y + 110.f;
    float itemW = 380.f, itemH = 190.f;
    float gapX = 30.f, gapY = 20.f;

    for (int i = 0; i < (int)craftableItems.size(); i++) {
        int row = i / 3;
        int col = i % 3;
        sf::Vector2f pos(startX + col * (itemW + gapX), startY + row * (itemH + gapY));
        sf::FloatRect btnRect({ pos.x + 210.f, pos.y + 130.f }, { 140.f, 45.f });
        if (btnRect.contains(uiPos)) {
            const auto& item = craftableItems[i];
            if (totalWood >= item.woodReq && totalStone >= item.stoneReq && totalWool >= item.woolReq) {
                if (item.id == ITEM_WOODEN_SWORD && swordCrafting) return true;
                if (item.id == ITEM_SHIELD && shieldCrafting) return true;

                int wToC = item.woodReq, sToC = item.stoneReq, woolToC = item.woolReq;
                for (int j = 0; j < 20; j++) {
                    if (inventory.hasItem(j)) {
                        if (inventory.getItemID(j) == ITEM_WOOD) {
                            while (wToC > 0 && inventory.getItemCount(j) > 0) { inventory.consumeItem(j); wToC--; }
                        }
                        if (inventory.getItemID(j) == ITEM_STONE) {
                            while (sToC > 0 && inventory.getItemCount(j) > 0) { inventory.consumeItem(j); sToC--; }
                        }
                        if (inventory.getItemID(j) == ITEM_WOOL) {
                            while (woolToC > 0 && inventory.getItemCount(j) > 0) { inventory.consumeItem(j); woolToC--; }
                        }
                    }
                }

                if (item.id == ITEM_WOODEN_SWORD) { swordCrafting = true; swordCraftTimer = 30.f; }
                else if (item.id == ITEM_SHIELD) { shieldCrafting = true; shieldCraftTimer = 60.f; }
                else {
                    inventory.addItem(item.id, *item.tex, 1);
                }
                return true;
            }
        }
    }
    return true;
}

