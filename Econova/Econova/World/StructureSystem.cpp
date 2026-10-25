#include "StructureSystem.h"
#include "WorldRenderer.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "SheepMeat.h"
#include "ViewUtils.h"
#include "Game.h"
#include "Inventory.h"
#include <algorithm>
#include <thread>
#include <sstream>

void syncSaveStructuresAndCampfires(
    AuthManager& authManager,
    const std::string& username,
    bool dbStartupEnabled,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires,
    Inventory* inventory
) {
    if (!dbStartupEnabled) return;
    std::vector<StoredStructure> structData;
    for (const auto& s : structures)
    {
        if (s.sprite.has_value() && s.owner == username)
        {
            structData.push_back({
                s.id,
                s.sprite->getPosition().x,
                s.sprite->getPosition().y,
                s.health,
                s.rotation,
                s.isOpen,
                s.shearY,
                s.owner
                });
        }
    }

    std::vector<StoredCampfire> campData;
    for (const auto& c : campfires)
    {
        if (c.owner == username) {
            campData.push_back({
                c.position.x,
                c.position.y,
                c.timeRemaining,
                c.isCrafted,
                c.fuel,
                c.inputItem,
                c.inputCount,
                c.outputItem,
                c.outputCount
                });
        }
    }

    std::vector<StoredCampfire> allCampfires;
    for (const auto& c : campfires) {
        allCampfires.push_back({
            c.position.x,
            c.position.y,
            c.timeRemaining,
            c.isCrafted,
            c.fuel,
            c.inputItem,
            c.inputCount,
            c.outputItem,
            c.outputCount,
            c.owner
        });
    }

    std::vector<std::pair<int, int>> invData;
    if (inventory) {
        invData.resize(20, { 0, 0 });
        for (int i = 0; i < 20; ++i) {
            if (inventory->hasItem(i)) {
                invData[i] = { inventory->getItemID(i), inventory->getItemCount(i) };
            }
        }
    }

    std::thread([&authManager, username, structData = std::move(structData), campData = std::move(campData), allCampfires = std::move(allCampfires), invData = std::move(invData)]() {
        try {
            authManager.saveStructures(username.c_str(), structData);
            authManager.saveCampfires(username.c_str(), campData);
            for (const auto& c : allCampfires) {
                authManager.updateCampfireCookingState(c.x, c.y, c.fuel, c.inputItem, c.inputCount, c.outputItem, c.outputCount, c.timeRemaining);
            }
            if (!invData.empty()) {
                authManager.saveInventory(username.c_str(), invData);
            }
        }
        catch (...) {}
    }).detach();
}

void updateCampfiresSimulation(
    std::vector<PlacedCampfire>& campfires,
    float dt
) {
    for (auto& fire : campfires) {
        if (fire.inputCount > 0 && fire.fuel > 0) {
            fire.timeRemaining -= dt;
            if (fire.timeRemaining <= 0.f) {
                int outItem = 0;
                if (fire.inputItem == ITEM_CHICKEN_MEAT) outItem = ITEM_COOKED_CHICKEN;
                else if (fire.inputItem == ITEM_PIG_MEAT) outItem = ITEM_COOKED_PIG;
                else if (fire.inputItem == ITEM_SHEEP_MEAT) outItem = ITEM_COOKED_SHEEP;

                if (outItem != 0) {
                    if (fire.outputCount == 0 || fire.outputItem == outItem) {
                        fire.outputItem = outItem;
                        fire.outputCount++;
                        fire.inputCount--;
                        fire.fuel--;
                        if (fire.inputCount > 0 && fire.fuel > 0) {
                            fire.timeRemaining = 10.f;
                        } else {
                            fire.timeRemaining = 0.f;
                        }
                    }
                }
            }
        }
    }
}

void drawCampfireMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    PlacedCampfire& fire,
    const sf::Texture& woodTex,
    const sf::Texture& cookedChickenTex,
    const sf::Texture& cookedPigTex,
    const sf::Texture& cookedSheepTex,
    std::function<const sf::Texture*(int id)> getItemTexture
) {
    window.setView(GetUIView(&window));
    
    sf::RectangleShape bg({ 500.f, 300.f });
    bg.setFillColor(sf::Color(40, 40, 40, 240));
    bg.setOutlineThickness(3.f);
    bg.setOutlineColor(sf::Color::White);
    bg.setPosition({
        (float)width / 2.f - 250.f,
        (float)height / 2.f - 150.f
    });
    window.draw(bg);

    sf::Text title(uiFont, "Campfire Cooking", 28);
    title.setFillColor(sf::Color::Yellow);
    title.setPosition({
        bg.getPosition().x + 20.f,
        bg.getPosition().y + 10.f
    });
    window.draw(title);

    if (fire.inputCount > 0 || fire.outputCount > 0 || fire.fuel > 0) {
        sf::Text prompt(uiFont, "Continue cooking progress saved!", 12);
        prompt.setFillColor(sf::Color(0, 242, 254));
        prompt.setPosition({
            bg.getPosition().x + 20.f,
            bg.getPosition().y + 45.f
        });
        window.draw(prompt);
    }

    sf::RectangleShape closeBtn({ 40.f, 40.f });
    closeBtn.setFillColor(sf::Color(200, 50, 50));
    closeBtn.setPosition({ bg.getPosition().x + 440.f, bg.getPosition().y + 10.f });
    window.draw(closeBtn);
    sf::Text closeTxt(uiFont, "X", 24);
    closeTxt.setFillColor(sf::Color::White);
    sf::FloatRect ctB = closeTxt.getLocalBounds();
    closeTxt.setPosition({ closeBtn.getPosition().x + (40.f - ctB.size.x)/2.f - 1.f, closeBtn.getPosition().y + (40.f - ctB.size.y)/2.f - 4.f });
    window.draw(closeTxt);

    sf::Text fuelLabel(uiFont, "Fuel", 18);
    fuelLabel.setFillColor(sf::Color::White);
    fuelLabel.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 80.f });
    window.draw(fuelLabel);

    sf::Text foodLabel(uiFont, "Raw Food", 18);
    foodLabel.setFillColor(sf::Color::White);
    foodLabel.setPosition({ bg.getPosition().x + 210.f, bg.getPosition().y + 80.f });
    window.draw(foodLabel);

    sf::Text outputLabel(uiFont, "Cooked Food", 18);
    outputLabel.setFillColor(sf::Color::White);
    outputLabel.setPosition({ bg.getPosition().x + 360.f, bg.getPosition().y + 80.f });
    window.draw(outputLabel);

    sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    // 1. Fuel slot
    sf::RectangleShape fuelSlot({ 80.f, 80.f });
    fuelSlot.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 110.f });
    bool hoverFuel = fuelSlot.getGlobalBounds().contains(mpos);
    fuelSlot.setFillColor(sf::Color(30, 30, 30));
    fuelSlot.setOutlineThickness(2.f);
    fuelSlot.setOutlineColor(hoverFuel ? sf::Color::Yellow : sf::Color(150, 150, 150));
    window.draw(fuelSlot);

    if (fire.fuel > 0) {
        sf::Sprite fs(woodTex);
        sf::Vector2u ts = woodTex.getSize();
        float scale = 64.f / std::max(ts.x, ts.y);
        fs.setScale({ scale, scale });
        fs.setPosition({ fuelSlot.getPosition().x + 8.f, fuelSlot.getPosition().y + 8.f });
        window.draw(fs);

        sf::Text fsText(uiFont, "x" + std::to_string(fire.fuel), 16);
        fsText.setFillColor(sf::Color::White);
        fsText.setPosition({ fuelSlot.getPosition().x + 50.f, fuelSlot.getPosition().y + 55.f });
        window.draw(fsText);
    }

    // 2. Food Input slot
    sf::RectangleShape foodSlot({ 80.f, 80.f });
    foodSlot.setPosition({ bg.getPosition().x + 210.f, bg.getPosition().y + 110.f });
    bool hoverFood = foodSlot.getGlobalBounds().contains(mpos);
    foodSlot.setFillColor(sf::Color(30, 30, 30));
    foodSlot.setOutlineThickness(2.f);
    foodSlot.setOutlineColor(hoverFood ? sf::Color::Yellow : sf::Color(150, 150, 150));
    window.draw(foodSlot);

    if (fire.inputCount > 0 && fire.inputItem != 0) {
        const sf::Texture* rTex = (fire.inputItem == ITEM_CHICKEN_MEAT) ? ChickenMeat::getTexture() :
                                  (fire.inputItem == ITEM_PIG_MEAT) ? PigMeat::getTexture() :
                                  (fire.inputItem == ITEM_SHEEP_MEAT) ? SheepMeat::getTexture() : nullptr;
        if (!rTex && getItemTexture) rTex = getItemTexture(fire.inputItem);
        if (rTex) {
            sf::Sprite fs(*rTex);
            sf::Vector2u ts = rTex->getSize();
            float scale = 64.f / std::max(ts.x, ts.y);
            fs.setScale({ scale, scale });
            fs.setPosition({ foodSlot.getPosition().x + 8.f, foodSlot.getPosition().y + 8.f });
            window.draw(fs);
        }

        sf::Text fsText(uiFont, "x" + std::to_string(fire.inputCount), 16);
        fsText.setFillColor(sf::Color::White);
        fsText.setPosition({ foodSlot.getPosition().x + 50.f, foodSlot.getPosition().y + 55.f });
        window.draw(fsText);
    }

    // Direction arrow between food and cooked slot
    bool campfireActive = (fire.inputCount > 0 && fire.fuel > 0);
    sf::Text arrowTxt(uiFont, "->", 24);
    arrowTxt.setStyle(sf::Text::Bold);
    arrowTxt.setFillColor(campfireActive ? sf::Color::Yellow : sf::Color(160, 160, 160));
    arrowTxt.setPosition({ bg.getPosition().x + 312.f, bg.getPosition().y + 133.f });
    window.draw(arrowTxt);

    // 3. Cooked Food slot
    sf::RectangleShape outSlot({ 80.f, 80.f });
    outSlot.setPosition({ bg.getPosition().x + 360.f, bg.getPosition().y + 110.f });
    bool hoverOut = outSlot.getGlobalBounds().contains(mpos);
    outSlot.setFillColor(sf::Color(30, 30, 30));
    outSlot.setOutlineThickness(2.f);
    outSlot.setOutlineColor(hoverOut ? sf::Color::Yellow : sf::Color(150, 150, 150));
    window.draw(outSlot);

    if (fire.outputCount > 0 && fire.outputItem != 0) {
        const sf::Texture* oTex = (fire.outputItem == ITEM_COOKED_CHICKEN) ? &cookedChickenTex :
                                  (fire.outputItem == ITEM_COOKED_PIG) ? &cookedPigTex :
                                  (fire.outputItem == ITEM_COOKED_SHEEP) ? &cookedSheepTex : nullptr;
        if (!oTex && getItemTexture) oTex = getItemTexture(fire.outputItem);
        if (oTex) {
            sf::Sprite fs(*oTex);
            sf::Vector2u ts = oTex->getSize();
            float scale = 64.f / std::max(ts.x, ts.y);
            fs.setScale({ scale, scale });
            fs.setPosition({ outSlot.getPosition().x + 8.f, outSlot.getPosition().y + 8.f });
            window.draw(fs);
        }

        sf::Text fsText(uiFont, "x" + std::to_string(fire.outputCount), 16);
        fsText.setFillColor(sf::Color::White);
        fsText.setPosition({ outSlot.getPosition().x + 50.f, outSlot.getPosition().y + 55.f });
        window.draw(fsText);
    }

    // Status row
    std::string statusStr = "Insert Fuel & Raw Food";
    sf::Color statusColor(150, 150, 150);
    if (campfireActive) {
        statusStr = "Cooking...";
        statusColor = sf::Color::Yellow;
    }
    else if (fire.outputCount > 0) {
        statusStr = "Cooked food ready! Click output slot to retrieve.";
        statusColor = sf::Color::Green;
    }
    sf::Text statusTxt(uiFont, statusStr, 18);
    statusTxt.setFillColor(statusColor);
    statusTxt.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 211.f });
    window.draw(statusTxt);

    // Cooking progress bar
    {
        const float BAR_X = bg.getPosition().x + 60.f;
        const float BAR_Y = bg.getPosition().y + 241.f;
        const float BAR_W = 380.f;
        const float BAR_H = 18.f;

        sf::RectangleShape barBg({ BAR_W, BAR_H });
        barBg.setPosition({ BAR_X, BAR_Y });
        barBg.setFillColor(sf::Color(50, 50, 50));
        barBg.setOutlineThickness(1.f);
        barBg.setOutlineColor(sf::Color(90, 90, 90));
        window.draw(barBg);

        if (campfireActive) {
            float progress = 1.0f - (fire.timeRemaining / 2.0f);
            if (progress < 0.f) progress = 0.f;
            if (progress > 1.f) progress = 1.f;
            sf::RectangleShape barFill({ BAR_W * progress, BAR_H });
            barFill.setPosition({ BAR_X, BAR_Y });
            barFill.setFillColor(sf::Color(255, 140, 30));
            window.draw(barFill);
            sf::Text pct(uiFont, std::to_string((int)(progress * 100)) + "%", 12);
            pct.setFillColor(sf::Color::White);
            pct.setPosition({ BAR_X + BAR_W / 2.f - 12.f, BAR_Y + 2.f });
            window.draw(pct);
        } else if (fire.outputCount > 0) {
            sf::RectangleShape barFull({ BAR_W, BAR_H });
            barFull.setPosition({ BAR_X, BAR_Y });
            barFull.setFillColor(sf::Color(50, 180, 50));
            window.draw(barFull);
            sf::Text done(uiFont, "Done!", 12);
            done.setFillColor(sf::Color::White);
            done.setPosition({ BAR_X + BAR_W / 2.f - 14.f, BAR_Y + 2.f });
            window.draw(done);
        }
    }

    // Hint at bottom
    sf::Text hintTxt(uiFont, "Select item in inventory below, then click a slot  |  Click output to retrieve", 12);
    hintTxt.setFillColor(sf::Color(160, 160, 160));
    hintTxt.setPosition({ bg.getPosition().x + 30.f, bg.getPosition().y + 270.f });
    window.draw(hintTxt);
}

void drawFurnaceMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    PlacedStructure& furnace,
    const sf::Texture& woodTex,
    const sf::Texture& cookedChickenTex,
    const sf::Texture& cookedPigTex,
    const sf::Texture& cookedSheepTex,
    std::function<const sf::Texture*(int id)> getItemTexture
) {
    window.setView(GetUIView(&window));
    
    sf::RectangleShape bg({ 500.f, 300.f });
    bg.setFillColor(sf::Color(40, 40, 40, 240));
    bg.setOutlineThickness(3.f);
    bg.setOutlineColor(sf::Color::White);
    bg.setPosition({
        (float)width / 2.f - 250.f,
        (float)height / 2.f - 150.f
    });
    window.draw(bg);

    sf::Text title(uiFont, "Stone Furnace", 28);
    title.setFillColor(sf::Color::Yellow);
    title.setPosition({
        bg.getPosition().x + 20.f,
        bg.getPosition().y + 10.f
    });
    window.draw(title);

    if (furnace.inputCount > 0 || furnace.outputCount > 0 || furnace.fuel > 0) {
        sf::Text prompt(uiFont, "Smelting and cooking progress saved!", 12);
        prompt.setFillColor(sf::Color(0, 242, 254));
        prompt.setPosition({
            bg.getPosition().x + 20.f,
            bg.getPosition().y + 45.f
        });
        window.draw(prompt);
    }

    sf::RectangleShape closeBtn({ 40.f, 40.f });
    closeBtn.setFillColor(sf::Color(200, 50, 50));
    closeBtn.setPosition({ bg.getPosition().x + 440.f, bg.getPosition().y + 10.f });
    window.draw(closeBtn);
    sf::Text closeTxt(uiFont, "X", 24);
    closeTxt.setFillColor(sf::Color::White);
    sf::FloatRect ctB = closeTxt.getLocalBounds();
    closeTxt.setPosition({ closeBtn.getPosition().x + (40.f - ctB.size.x)/2.f - 1.f, closeBtn.getPosition().y + (40.f - ctB.size.y)/2.f - 4.f });
    window.draw(closeTxt);

    sf::Text fuelLabel(uiFont, "Fuel", 18);
    fuelLabel.setFillColor(sf::Color::White);
    fuelLabel.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 80.f });
    window.draw(fuelLabel);

    sf::Text foodLabel(uiFont, "Ore / Food", 18);
    foodLabel.setFillColor(sf::Color::White);
    foodLabel.setPosition({ bg.getPosition().x + 205.f, bg.getPosition().y + 80.f });
    window.draw(foodLabel);

    sf::Text outputLabel(uiFont, "Bar / Cooked", 18);
    outputLabel.setFillColor(sf::Color::White);
    outputLabel.setPosition({ bg.getPosition().x + 355.f, bg.getPosition().y + 80.f });
    window.draw(outputLabel);

    sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    // 1. Fuel slot
    sf::RectangleShape fuelSlot({ 80.f, 80.f });
    fuelSlot.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 110.f });
    bool hoverFuel = fuelSlot.getGlobalBounds().contains(mpos);
    fuelSlot.setFillColor(sf::Color(30, 30, 30));
    fuelSlot.setOutlineThickness(2.f);
    fuelSlot.setOutlineColor(hoverFuel ? sf::Color::Yellow : sf::Color(150, 150, 150));
    window.draw(fuelSlot);

    if (furnace.fuel > 0) {
        sf::Sprite fs(woodTex);
        sf::Vector2u ts = woodTex.getSize();
        float scale = 64.f / std::max(ts.x, ts.y);
        fs.setScale({ scale, scale });
        fs.setPosition({ fuelSlot.getPosition().x + 8.f, fuelSlot.getPosition().y + 8.f });
        window.draw(fs);

        sf::Text fsText(uiFont, "x" + std::to_string(furnace.fuel), 16);
        fsText.setFillColor(sf::Color::White);
        fsText.setPosition({ fuelSlot.getPosition().x + 50.f, fuelSlot.getPosition().y + 55.f });
        window.draw(fsText);
    }

    // 2. Input slot
    sf::RectangleShape inputSlot({ 80.f, 80.f });
    inputSlot.setPosition({ bg.getPosition().x + 210.f, bg.getPosition().y + 110.f });
    bool hoverInput = inputSlot.getGlobalBounds().contains(mpos);
    inputSlot.setFillColor(sf::Color(30, 30, 30));
    inputSlot.setOutlineThickness(2.f);
    inputSlot.setOutlineColor(hoverInput ? sf::Color::Yellow : sf::Color(150, 150, 150));
    window.draw(inputSlot);

    if (furnace.inputCount > 0 && furnace.inputItem != 0) {
        const sf::Texture* rTex = getItemTexture ? getItemTexture(furnace.inputItem) : nullptr;
        if (!rTex) {
            if (furnace.inputItem == ITEM_CHICKEN_MEAT) rTex = ChickenMeat::getTexture();
            else if (furnace.inputItem == ITEM_PIG_MEAT) rTex = PigMeat::getTexture();
            else if (furnace.inputItem == ITEM_SHEEP_MEAT) rTex = SheepMeat::getTexture();
        }
        if (rTex) {
            sf::Sprite ispr(*rTex);
            sf::Vector2u ts = rTex->getSize();
            float scale = 64.f / std::max(ts.x, ts.y);
            ispr.setScale({ scale, scale });
            ispr.setPosition({ inputSlot.getPosition().x + 8.f, inputSlot.getPosition().y + 8.f });
            window.draw(ispr);

            sf::Text isText(uiFont, "x" + std::to_string(furnace.inputCount), 16);
            isText.setFillColor(sf::Color::White);
            isText.setPosition({ inputSlot.getPosition().x + 50.f, inputSlot.getPosition().y + 55.f });
            window.draw(isText);
        }
    }

    // Direction arrow between input and output slot
    bool furnaceActive = (furnace.inputCount > 0 && furnace.fuel > 0);
    sf::Text arrowTxt(uiFont, "->", 24);
    arrowTxt.setStyle(sf::Text::Bold);
    arrowTxt.setFillColor(furnaceActive ? sf::Color::Yellow : sf::Color(160, 160, 160));
    arrowTxt.setPosition({ bg.getPosition().x + 312.f, bg.getPosition().y + 133.f });
    window.draw(arrowTxt);

    // 3. Output slot
    sf::RectangleShape outputSlot({ 80.f, 80.f });
    outputSlot.setPosition({ bg.getPosition().x + 360.f, bg.getPosition().y + 110.f });
    bool hoverOutput = outputSlot.getGlobalBounds().contains(mpos);
    outputSlot.setFillColor(sf::Color(30, 30, 30));
    outputSlot.setOutlineThickness(2.f);
    outputSlot.setOutlineColor(hoverOutput ? sf::Color::Yellow : sf::Color(150, 150, 150));
    window.draw(outputSlot);

    if (furnace.outputCount > 0 && furnace.outputItem != 0) {
        const sf::Texture* oTex = getItemTexture ? getItemTexture(furnace.outputItem) : nullptr;
        if (!oTex) {
            if (furnace.outputItem == ITEM_COOKED_CHICKEN) oTex = &cookedChickenTex;
            else if (furnace.outputItem == ITEM_COOKED_PIG) oTex = &cookedPigTex;
            else if (furnace.outputItem == ITEM_COOKED_SHEEP) oTex = &cookedSheepTex;
        }
        if (oTex) {
            sf::Sprite ospr(*oTex);
            sf::Vector2u ts = oTex->getSize();
            float scale = 64.f / std::max(ts.x, ts.y);
            ospr.setScale({ scale, scale });
            ospr.setPosition({ outputSlot.getPosition().x + 8.f, outputSlot.getPosition().y + 8.f });
            window.draw(ospr);

            sf::Text osText(uiFont, "x" + std::to_string(furnace.outputCount), 16);
            osText.setFillColor(sf::Color::White);
            osText.setPosition({ outputSlot.getPosition().x + 50.f, outputSlot.getPosition().y + 55.f });
            window.draw(osText);
        }
    }

    std::string statusStr = "Insert Fuel & Ore / Food";
    sf::Color statusColor(150, 150, 150);
    if (furnaceActive) {
        bool isOre = (furnace.inputItem == ITEM_COPPER_ORE || furnace.inputItem == ITEM_IRON_ORE || furnace.inputItem == ITEM_GOLD_ORE);
        statusStr = isOre ? "Smelting..." : "Cooking...";
        statusColor = sf::Color::Yellow;
    }
    else if (furnace.outputCount > 0) {
        bool isBar = (furnace.outputItem == ITEM_COPPER_BAR || furnace.outputItem == ITEM_IRON_BAR || furnace.outputItem == ITEM_GOLD_BAR);
        statusStr = isBar ? "Metal bar ready! Click output slot to retrieve." : "Cooked food ready! Click output slot to retrieve.";
        statusColor = sf::Color::Green;
    }
    sf::Text statusTxt(uiFont, statusStr, 18);
    statusTxt.setFillColor(statusColor);
    statusTxt.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 211.f });
    window.draw(statusTxt);

    // Smelting/cooking progress bar
    {
        const float BAR_X = bg.getPosition().x + 60.f;
        const float BAR_Y = bg.getPosition().y + 241.f;
        const float BAR_W = 380.f;
        const float BAR_H = 18.f;

        sf::RectangleShape barBg({ BAR_W, BAR_H });
        barBg.setPosition({ BAR_X, BAR_Y });
        barBg.setFillColor(sf::Color(50, 50, 50));
        barBg.setOutlineThickness(1.f);
        barBg.setOutlineColor(sf::Color(90, 90, 90));
        window.draw(barBg);

        if (furnaceActive) {
            float progress = 1.0f - (furnace.timeRemaining / 2.0f);
            if (progress < 0.f) progress = 0.f;
            if (progress > 1.f) progress = 1.f;
            bool isOre2 = (furnace.inputItem == ITEM_COPPER_ORE || furnace.inputItem == ITEM_IRON_ORE || furnace.inputItem == ITEM_GOLD_ORE);
            sf::RectangleShape barFill({ BAR_W * progress, BAR_H });
            barFill.setPosition({ BAR_X, BAR_Y });
            barFill.setFillColor(isOre2 ? sf::Color(200, 110, 30) : sf::Color(255, 140, 30));
            window.draw(barFill);
            sf::Text pct(uiFont, std::to_string((int)(progress * 100)) + "%", 12);
            pct.setFillColor(sf::Color::White);
            pct.setPosition({ BAR_X + BAR_W / 2.f - 12.f, BAR_Y + 2.f });
            window.draw(pct);
        } else if (furnace.outputCount > 0) {
            sf::RectangleShape barFull({ BAR_W, BAR_H });
            barFull.setPosition({ BAR_X, BAR_Y });
            barFull.setFillColor(sf::Color(50, 180, 50));
            window.draw(barFull);
            sf::Text done(uiFont, "Done!", 12);
            done.setFillColor(sf::Color::White);
            done.setPosition({ BAR_X + BAR_W / 2.f - 14.f, BAR_Y + 2.f });
            window.draw(done);
        }
    }

    // Hint at bottom
    sf::Text hintTxt(uiFont, "Select ore/food in inventory below, then click a slot  |  Click output to retrieve", 12);
    hintTxt.setFillColor(sf::Color(160, 160, 160));
    hintTxt.setPosition({ bg.getPosition().x + 30.f, bg.getPosition().y + 270.f });
    window.draw(hintTxt);
}

// ─── Chest Menu (two-panel: inventory left, chest right) ──────────────────────
void drawChestMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    PlacedStructure& chest,
    Inventory& inventory,
    int chestSelectedInvSlot,
    std::function<const sf::Texture*(int id)> getItemTexture
) {
    window.setView(GetUIView(&window));
    sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    // ── Layout ────────────────────────────────────────────────────────────────
    const int   COLS      = 5;
    const int   ROWS      = 4;
    const float SLOT_S    = 66.f;
    const float GAP       = 7.f;
    const float GRID_W    = COLS * SLOT_S + (COLS - 1) * GAP;   // 358
    const float GRID_H    = ROWS * SLOT_S + (ROWS - 1) * GAP;   // 285
    const float PAD       = 14.f;
    const float HEADER_H  = 48.f;
    const float LABEL_H   = 20.f;
    const float HINT_H    = 26.f;
    const float DIV_W     = 2.f;
    const float COL_GAP   = 20.f;
    const float LEFT_W    = PAD + GRID_W + PAD;          // 14+358+14 = 386
    const float RIGHT_W   = PAD + GRID_W + PAD + 40.f;  // +40 for close btn = 426
    const float PANEL_W   = LEFT_W + DIV_W + COL_GAP + RIGHT_W; // 834
    const float PANEL_H   = HEADER_H + LABEL_H + GAP + GRID_H + PAD + HINT_H;

    float ppx = (float)width  / 2.f - PANEL_W / 2.f;
    float ppy = (float)height / 2.f - PANEL_H / 2.f;

    // ── Shadow ────────────────────────────────────────────────────────────────
    sf::RectangleShape shadow({ PANEL_W + 8.f, PANEL_H + 8.f });
    shadow.setPosition({ ppx - 4.f, ppy + 4.f });
    shadow.setFillColor(sf::Color(0, 0, 0, 110));
    window.draw(shadow);

    // ── Panel body ────────────────────────────────────────────────────────────
    sf::RectangleShape panel({ PANEL_W, PANEL_H });
    panel.setPosition({ ppx, ppy });
    panel.setFillColor(sf::Color(28, 22, 16));
    panel.setOutlineThickness(2.f);
    panel.setOutlineColor(sf::Color(120, 90, 50));
    window.draw(panel);

    // ── Header bar ────────────────────────────────────────────────────────────
    sf::RectangleShape header({ PANEL_W, HEADER_H });
    header.setPosition({ ppx, ppy });
    header.setFillColor(sf::Color(60, 42, 22));
    window.draw(header);
    sf::RectangleShape hDiv({ PANEL_W, 2.f });
    hDiv.setPosition({ ppx, ppy + HEADER_H });
    hDiv.setFillColor(sf::Color(150, 110, 55));
    window.draw(hDiv);

    sf::Text title(uiFont, "Chest Storage", 20);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(sf::Color(240, 200, 90));
    title.setPosition({ ppx + 14.f, ppy + (HEADER_H - 24.f) / 2.f });
    window.draw(title);

    // ── Close button ──────────────────────────────────────────────────────────
    sf::FloatRect closeBtnRect({ ppx + PANEL_W - 42.f, ppy + 6.f }, { 36.f, 36.f });
    bool closeHov = closeBtnRect.contains(mpos);
    sf::RectangleShape closeBtn({ 36.f, 36.f });
    closeBtn.setPosition(closeBtnRect.position);
    closeBtn.setFillColor(closeHov ? sf::Color(220, 60, 50) : sf::Color(160, 45, 35));
    closeBtn.setOutlineThickness(1.f);
    closeBtn.setOutlineColor(sf::Color(220, 100, 90));
    window.draw(closeBtn);
    sf::Text closeTxt(uiFont, "X", 16);
    closeTxt.setStyle(sf::Text::Bold);
    closeTxt.setFillColor(sf::Color::White);
    auto ctb = closeTxt.getLocalBounds();
    closeTxt.setPosition({ closeBtnRect.position.x + (36.f - ctb.size.x) / 2.f - 1.f,
                           closeBtnRect.position.y + (36.f - ctb.size.y) / 2.f - 4.f });
    window.draw(closeTxt);

    // ── Vertical divider ──────────────────────────────────────────────────────
    sf::RectangleShape vDiv({ DIV_W, PANEL_H - HEADER_H });
    vDiv.setPosition({ ppx + LEFT_W, ppy + HEADER_H });
    vDiv.setFillColor(sf::Color(100, 78, 45));
    window.draw(vDiv);

    // ── Section labels ────────────────────────────────────────────────────────
    const float CONTENT_Y  = ppy + HEADER_H;
    const float LEFT_GX    = ppx + PAD;
    const float LEFT_GY    = CONTENT_Y + LABEL_H + GAP;
    const float RIGHT_GX   = ppx + LEFT_W + DIV_W + COL_GAP + PAD;
    const float RIGHT_GY   = LEFT_GY;

    sf::Text invLabel(uiFont, "Your Inventory  (click item to select)", 12);
    invLabel.setFillColor(sf::Color(180, 160, 100));
    invLabel.setPosition({ LEFT_GX, CONTENT_Y + 3.f });
    window.draw(invLabel);

    int usedSlots = 0;
    for (int i = 0; i < CHEST_SLOT_COUNT; i++)
        if (chest.chestSlots[i].id != 0 && chest.chestSlots[i].count > 0) usedSlots++;
    sf::Text chestLabel(uiFont,
        "Chest Slots  (" + std::to_string(usedSlots) + "/" + std::to_string(CHEST_SLOT_COUNT) + " used)", 12);
    chestLabel.setFillColor(sf::Color(180, 160, 100));
    chestLabel.setPosition({ RIGHT_GX, CONTENT_Y + 3.f });
    window.draw(chestLabel);

    // ── LEFT PANEL: Inventory slots ───────────────────────────────────────────
    int invSelectedSlot = inventory.getSelectedSlot();
    // Effective selection: explicit chest selection overrides hotbar selection
    int effectiveSrc = (chestSelectedInvSlot >= 0) ? chestSelectedInvSlot : invSelectedSlot;

    for (int i = 0; i < 20 && i < CHEST_SLOT_COUNT; i++) {
        int col = i % COLS, row = i / COLS;
        float sx = LEFT_GX + col * (SLOT_S + GAP);
        float sy = LEFT_GY + row * (SLOT_S + GAP);
        sf::FloatRect slotBounds({ sx, sy }, { SLOT_S, SLOT_S });
        bool hover    = slotBounds.contains(mpos);
        bool selected = (i == effectiveSrc);
        bool hasItem  = inventory.hasItem(i);

        sf::RectangleShape slot({ SLOT_S, SLOT_S });
        slot.setPosition({ sx, sy });
        if (selected && hasItem)
            slot.setFillColor(sf::Color(55, 90, 155));
        else if (hover && hasItem)
            slot.setFillColor(sf::Color(60, 55, 38));
        else if (hasItem)
            slot.setFillColor(sf::Color(42, 36, 22));
        else
            slot.setFillColor(sf::Color(28, 24, 16));
        slot.setOutlineThickness(selected ? 2.5f : 1.5f);
        slot.setOutlineColor(selected ? sf::Color(100, 160, 255) :
                             hover ? sf::Color(200, 160, 60) :
                             sf::Color(68, 55, 38));
        window.draw(slot);

        if (hasItem) {
            const sf::Texture* tex = inventory.getItemTexture(i);
            if (tex) {
                sf::Sprite sp(*tex);
                sf::Vector2u ts = tex->getSize();
                float sc = (SLOT_S - 12.f) / (float)std::max(ts.x, ts.y);
                sp.setScale({ sc, sc });
                sp.setPosition({ sx + 6.f, sy + 6.f });
                window.draw(sp);
            }
            int cnt = inventory.getItemCount(i);
            if (cnt > 1) {
                std::string qs = std::to_string(cnt);
                sf::RectangleShape badge({ (float)(qs.size() * 8 + 4), 15.f });
                badge.setFillColor(sf::Color(0, 0, 0, 190));
                badge.setPosition({ sx + SLOT_S - badge.getSize().x - 2.f, sy + SLOT_S - 17.f });
                window.draw(badge);
                sf::Text ct(uiFont, qs, 11);
                ct.setFillColor(sf::Color::White);
                ct.setPosition({ badge.getPosition().x + 2.f, badge.getPosition().y + 1.f });
                window.draw(ct);
            }
        }
        // Slot number
        sf::Text sn(uiFont, std::to_string(i + 1), 9);
        sn.setFillColor(sf::Color(80, 70, 50));
        sn.setPosition({ sx + 2.f, sy + 2.f });
        window.draw(sn);
    }

    // ── RIGHT PANEL: Chest slots ──────────────────────────────────────────────
    std::string tooltipText;
    sf::Vector2f tooltipPos;

    for (int i = 0; i < CHEST_SLOT_COUNT; i++) {
        int col = i % COLS, row = i / COLS;
        float sx = RIGHT_GX + col * (SLOT_S + GAP);
        float sy = RIGHT_GY + row * (SLOT_S + GAP);
        sf::FloatRect slotBounds({ sx, sy }, { SLOT_S, SLOT_S });
        bool hover    = slotBounds.contains(mpos);
        const ChestSlot& cs = chest.chestSlots[i];
        bool occupied = (cs.id != 0 && cs.count > 0);

        // Show store indicator if inv item selected and hovering
        bool srcHasItem  = (effectiveSrc >= 0 && inventory.hasItem(effectiveSrc));
        bool canStore    = srcHasItem && (!occupied ||
                           (cs.id == inventory.getItemID(effectiveSrc)));

        sf::RectangleShape slot({ SLOT_S, SLOT_S });
        slot.setPosition({ sx, sy });
        if (hover && canStore && !occupied)
            slot.setFillColor(sf::Color(30, 70, 30));
        else if (hover && occupied)
            slot.setFillColor(sf::Color(80, 60, 35));
        else if (hover)
            slot.setFillColor(sf::Color(52, 46, 32));
        else if (occupied)
            slot.setFillColor(sf::Color(45, 38, 25));
        else
            slot.setFillColor(sf::Color(30, 25, 16));
        slot.setOutlineThickness(2.f);
        slot.setOutlineColor(hover && canStore ? sf::Color(80, 200, 80) :
                             hover ? sf::Color(220, 180, 70) :
                             occupied ? sf::Color(140, 110, 55) :
                             sf::Color(65, 52, 35));
        window.draw(slot);

        if (occupied) {
            const sf::Texture* tex = cs.tex ? cs.tex : (getItemTexture ? getItemTexture(cs.id) : nullptr);
            if (tex) {
                sf::Sprite sp(*tex);
                sf::Vector2u ts = tex->getSize();
                float sc = (SLOT_S - 12.f) / (float)std::max(ts.x, ts.y);
                sp.setScale({ sc, sc });
                sp.setPosition({ sx + 6.f, sy + 6.f });
                window.draw(sp);
            }
            std::string qstr = std::to_string(cs.count);
            sf::RectangleShape badge({ (float)(qstr.size() * 8 + 4), 15.f });
            badge.setFillColor(sf::Color(0, 0, 0, 190));
            badge.setPosition({ sx + SLOT_S - badge.getSize().x - 2.f, sy + SLOT_S - 17.f });
            window.draw(badge);
            sf::Text cnt(uiFont, qstr, 11);
            cnt.setFillColor(sf::Color(255, 230, 120));
            cnt.setPosition({ badge.getPosition().x + 2.f, badge.getPosition().y + 1.f });
            window.draw(cnt);
            if (hover) {
                tooltipText = srcHasItem && canStore ?
                    "Click to STORE into this slot" :
                    "Click to RETRIEVE " + qstr + " item(s)";
                tooltipPos = { sx, sy - 22.f };
            }
        } else if (hover) {
            if (srcHasItem) {
                sf::Text storeHint(uiFont, "\u25BC", 22);
                storeHint.setFillColor(sf::Color(80, 200, 80, 200));
                auto eb = storeHint.getLocalBounds();
                storeHint.setPosition({ sx + (SLOT_S - eb.size.x) / 2.f,
                                        sy + (SLOT_S - eb.size.y) / 2.f - 4.f });
                window.draw(storeHint);
                tooltipText = "Click to STORE here";
                tooltipPos  = { sx, sy - 22.f };
            } else {
                sf::Text emptyHint(uiFont, "+", 26);
                emptyHint.setFillColor(sf::Color(100, 85, 55, 140));
                auto eb = emptyHint.getLocalBounds();
                emptyHint.setPosition({ sx + (SLOT_S - eb.size.x) / 2.f - 2.f,
                                        sy + (SLOT_S - eb.size.y) / 2.f - 5.f });
                window.draw(emptyHint);
            }
        }
    }

    // ── Tooltip ───────────────────────────────────────────────────────────────
    if (!tooltipText.empty()) {
        sf::Text tip(uiFont, tooltipText, 12);
        sf::FloatRect tipB = tip.getLocalBounds();
        sf::RectangleShape tipBg({ tipB.size.x + 12.f, 22.f });
        tipBg.setFillColor(sf::Color(20, 15, 10, 220));
        tipBg.setOutlineThickness(1.f);
        tipBg.setOutlineColor(sf::Color(150, 120, 60));
        float tx = std::min(tooltipPos.x, ppx + PANEL_W - tipBg.getSize().x - 4.f);
        float ty = tooltipPos.y - 4.f;
        if (ty < ppy + HEADER_H + 2.f) ty = tooltipPos.y + SLOT_S + 4.f;
        tipBg.setPosition({ tx, ty });
        tip.setPosition({ tx + 6.f, ty + 3.f });
        window.draw(tipBg);
        window.draw(tip);
    }

    // ── Bottom hint bar ───────────────────────────────────────────────────────
    float hintY = ppy + PANEL_H - HINT_H;
    sf::RectangleShape hintBar({ PANEL_W, HINT_H });
    hintBar.setPosition({ ppx, hintY });
    hintBar.setFillColor(sf::Color(18, 14, 9));
    window.draw(hintBar);
    sf::Text hint(uiFont,
        "  Click inv slot to STORE  |  Click chest slot to RETRIEVE  |  [ESC]/[X] Close", 11);
    hint.setFillColor(sf::Color(130, 115, 80));
    hint.setPosition({ ppx + 8.f, hintY + 6.f });
    window.draw(hint);
}

void drawQuantityPrompt(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    bool isCampfire,
    int campfireSlot,
    int maxAmount,
    TextInput& quantityInput,
    bool isChest,
    int chestSlotIdx,
    bool isRetrieve
) {
    sf::View currentView = window.getView();
    window.setView(GetUIView(&window));

    const float uiW = 1920.f;
    const float uiH = 1080.f;

    // Fullscreen dimmed backdrop
    sf::RectangleShape backdrop({ uiW, uiH });
    backdrop.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(backdrop);

    const float boxW = 480.f;
    const float boxH = 260.f;
    const float boxX = (uiW - boxW) / 2.f;
    const float boxY = (uiH - boxH) / 2.f;

    // Shadow
    sf::RectangleShape shadow({ boxW + 8.f, boxH + 8.f });
    shadow.setPosition({ boxX - 4.f, boxY + 4.f });
    shadow.setFillColor(sf::Color(0, 0, 0, 140));
    window.draw(shadow);

    // Box body
    sf::RectangleShape qBox({ boxW, boxH });
    qBox.setPosition({ boxX, boxY });
    qBox.setFillColor(sf::Color(22, 26, 34, 252));
    qBox.setOutlineThickness(3.f);
    qBox.setOutlineColor(isRetrieve ? sf::Color(255, 180, 50) : sf::Color(0, 220, 240));
    window.draw(qBox);

    // Header bar
    sf::RectangleShape header({ boxW, 44.f });
    header.setPosition({ boxX, boxY });
    header.setFillColor(isRetrieve ? sf::Color(65, 45, 15) : sf::Color(15, 55, 65));
    window.draw(header);

    sf::Text qTitle(uiFont, isRetrieve ? "RETRIEVE QUANTITY" : "STORE / PLACE QUANTITY", 18);
    qTitle.setStyle(sf::Text::Bold);
    qTitle.setFillColor(isRetrieve ? sf::Color(255, 205, 80) : sf::Color(0, 242, 254));
    auto tb = qTitle.getLocalBounds();
    qTitle.setPosition({ boxX + (boxW - tb.size.x) / 2.f, boxY + 10.f });
    window.draw(qTitle);

    // Context label
    std::string labelText;
    if (isChest && isRetrieve) {
        labelText = "Quantity to retrieve from chest";
    } else if (isChest) {
        labelText = "Quantity to store in chest";
    } else if (isCampfire && campfireSlot == 3) {
        labelText = "Quantity of output to retrieve";
    } else if (isCampfire && isRetrieve) {
        labelText = (campfireSlot == 1) ? "Quantity of fuel to withdraw" : "Quantity of input to withdraw";
    } else if (isCampfire) {
        labelText = (campfireSlot == 1) ? "Quantity of fuel to add" : "Quantity of items to cook/smelt";
    } else {
        labelText = "Quantity to put in trade";
    }
    sf::Text qLabel(uiFont, labelText + " (Max: " + std::to_string(maxAmount) + "):", 13);
    qLabel.setFillColor(sf::Color(210, 215, 225));
    qLabel.setPosition({ boxX + 30.f, boxY + 56.f });
    window.draw(qLabel);

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    // [-] Decrement button
    const float minusW = 44.f, minusH = 38.f;
    const float minusX = boxX + 30.f, inputY = boxY + 84.f;
    sf::FloatRect minusRect({ minusX, inputY }, { minusW, minusH });
    bool minusHov = minusRect.contains(mousePos);
    sf::RectangleShape minusBtn({ minusW, minusH });
    minusBtn.setPosition({ minusX, inputY });
    minusBtn.setFillColor(minusHov ? sf::Color(65, 75, 95) : sf::Color(42, 48, 62));
    minusBtn.setOutlineThickness(1.5f);
    minusBtn.setOutlineColor(sf::Color(100, 115, 140));
    window.draw(minusBtn);
    sf::Text minusTxt(uiFont, "-", 20);
    minusTxt.setStyle(sf::Text::Bold);
    minusTxt.setFillColor(sf::Color::White);
    auto mb = minusTxt.getLocalBounds();
    minusTxt.setPosition({ minusX + (minusW - mb.size.x) / 2.f, inputY + (minusH - mb.size.y) / 2.f - 4.f });
    window.draw(minusTxt);

    // Text Input box (centered between - and +)
    const float inputW = boxW - 60.f - (minusW * 2.f) - 16.f;
    const float inputX = minusX + minusW + 8.f;
    quantityInput.setFocus(true);
    quantityInput.setPosition({ inputX, inputY });
    quantityInput.draw(window);

    // [+] Increment button
    const float plusX = inputX + inputW + 8.f;
    sf::FloatRect plusRect({ plusX, inputY }, { minusW, minusH });
    bool plusHov = plusRect.contains(mousePos);
    sf::RectangleShape plusBtn({ minusW, minusH });
    plusBtn.setPosition({ plusX, inputY });
    plusBtn.setFillColor(plusHov ? sf::Color(65, 75, 95) : sf::Color(42, 48, 62));
    plusBtn.setOutlineThickness(1.5f);
    plusBtn.setOutlineColor(sf::Color(100, 115, 140));
    window.draw(plusBtn);
    sf::Text plusTxt(uiFont, "+", 20);
    plusTxt.setStyle(sf::Text::Bold);
    plusTxt.setFillColor(sf::Color::White);
    auto pb = plusTxt.getLocalBounds();
    plusTxt.setPosition({ plusX + (minusW - pb.size.x) / 2.f, inputY + (minusH - pb.size.y) / 2.f - 4.f });
    window.draw(plusTxt);

    // Quick preset buttons row: [1]  [Half]  [All]
    const float preY = boxY + 132.f;
    const float preW = (boxW - 60.f - 16.f) / 3.f;
    const float preH = 32.f;

    // [1]
    sf::FloatRect btn1Rect({ boxX + 30.f, preY }, { preW, preH });
    bool hov1 = btn1Rect.contains(mousePos);
    sf::RectangleShape btn1({ preW, preH });
    btn1.setPosition({ boxX + 30.f, preY });
    btn1.setFillColor(hov1 ? sf::Color(55, 75, 95) : sf::Color(35, 48, 62));
    btn1.setOutlineThickness(1.f);
    btn1.setOutlineColor(sf::Color(80, 110, 140));
    window.draw(btn1);
    sf::Text txt1(uiFont, "1", 14);
    txt1.setStyle(sf::Text::Bold);
    txt1.setFillColor(sf::Color::White);
    auto b1b = txt1.getLocalBounds();
    txt1.setPosition({ boxX + 30.f + (preW - b1b.size.x) / 2.f, preY + (preH - b1b.size.y) / 2.f - 3.f });
    window.draw(txt1);

    // [Half]
    sf::FloatRect btnHRect({ boxX + 30.f + preW + 8.f, preY }, { preW, preH });
    bool hovH = btnHRect.contains(mousePos);
    sf::RectangleShape btnH({ preW, preH });
    btnH.setPosition({ boxX + 30.f + preW + 8.f, preY });
    btnH.setFillColor(hovH ? sf::Color(55, 75, 95) : sf::Color(35, 48, 62));
    btnH.setOutlineThickness(1.f);
    btnH.setOutlineColor(sf::Color(80, 110, 140));
    window.draw(btnH);
    int halfVal = std::max(1, maxAmount / 2);
    sf::Text txtH(uiFont, "Half (" + std::to_string(halfVal) + ")", 14);
    txtH.setStyle(sf::Text::Bold);
    txtH.setFillColor(sf::Color::White);
    auto bhb = txtH.getLocalBounds();
    txtH.setPosition({ boxX + 30.f + preW + 8.f + (preW - bhb.size.x) / 2.f, preY + (preH - bhb.size.y) / 2.f - 3.f });
    window.draw(txtH);

    // [All]
    sf::FloatRect btnARect({ boxX + 30.f + (preW + 8.f) * 2.f, preY }, { preW, preH });
    bool hovA = btnARect.contains(mousePos);
    sf::RectangleShape btnA({ preW, preH });
    btnA.setPosition({ boxX + 30.f + (preW + 8.f) * 2.f, preY });
    btnA.setFillColor(hovA ? sf::Color(55, 75, 95) : sf::Color(35, 48, 62));
    btnA.setOutlineThickness(1.f);
    btnA.setOutlineColor(sf::Color(80, 110, 140));
    window.draw(btnA);
    sf::Text txtA(uiFont, "All (" + std::to_string(maxAmount) + ")", 14);
    txtA.setStyle(sf::Text::Bold);
    txtA.setFillColor(sf::Color::White);
    auto bab = txtA.getLocalBounds();
    txtA.setPosition({ boxX + 30.f + (preW + 8.f) * 2.f + (preW - bab.size.x) / 2.f, preY + (preH - bab.size.y) / 2.f - 3.f });
    window.draw(txtA);

    // Bottom Action buttons: [Confirm / Retrieve]  [Cancel]
    const float actY = boxY + 180.f;
    const float actW = (boxW - 60.f - 16.f) / 2.f;
    const float actH = 42.f;

    // Confirm button
    sf::FloatRect confirmBtnRect({ boxX + 30.f, actY }, { actW, actH });
    bool confirmHover = confirmBtnRect.contains(mousePos);
    sf::RectangleShape confirmBtn({ actW, actH });
    confirmBtn.setPosition({ boxX + 30.f, actY });
    if (isRetrieve) {
        confirmBtn.setFillColor(confirmHover ? sf::Color(230, 150, 30) : sf::Color(190, 120, 20));
        confirmBtn.setOutlineColor(confirmHover ? sf::Color(255, 200, 80) : sf::Color(210, 140, 30));
    } else {
        confirmBtn.setFillColor(confirmHover ? sf::Color(0, 190, 230) : sf::Color(0, 150, 190));
        confirmBtn.setOutlineColor(confirmHover ? sf::Color(100, 230, 255) : sf::Color(0, 180, 220));
    }
    confirmBtn.setOutlineThickness(1.5f);
    window.draw(confirmBtn);

    sf::Text confirmTxt(uiFont, isRetrieve ? "Retrieve" : "Confirm", 16);
    confirmTxt.setStyle(sf::Text::Bold);
    confirmTxt.setFillColor(sf::Color::White);
    auto cb = confirmTxt.getLocalBounds();
    confirmTxt.setPosition({ boxX + 30.f + (actW - cb.size.x) / 2.f, actY + (actH - cb.size.y) / 2.f - 3.f });
    window.draw(confirmTxt);

    // Cancel button
    sf::FloatRect cancelBtnRect({ boxX + 30.f + actW + 16.f, actY }, { actW, actH });
    bool cancelHover = cancelBtnRect.contains(mousePos);
    sf::RectangleShape cancelBtn({ actW, actH });
    cancelBtn.setPosition({ boxX + 30.f + actW + 16.f, actY });
    cancelBtn.setFillColor(cancelHover ? sf::Color(180, 50, 50) : sf::Color(140, 40, 40));
    cancelBtn.setOutlineThickness(1.5f);
    cancelBtn.setOutlineColor(cancelHover ? sf::Color(230, 90, 90) : sf::Color(170, 50, 50));
    window.draw(cancelBtn);

    sf::Text cancelTxt(uiFont, "Cancel", 16);
    cancelTxt.setStyle(sf::Text::Bold);
    cancelTxt.setFillColor(sf::Color::White);
    auto cnb = cancelTxt.getLocalBounds();
    cancelTxt.setPosition({ boxX + 30.f + actW + 16.f + (actW - cnb.size.x) / 2.f, actY + (actH - cnb.size.y) / 2.f - 3.f });
    window.draw(cancelTxt);

    window.setView(currentView);
}


void drawTradeConfirmPopup(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::string& warningMessage
) {
    // Backdrop shadow to dim background and draw focus to confirmation
    sf::RectangleShape backdrop({ static_cast<float>(width), static_cast<float>(height) });
    backdrop.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(backdrop);

    float boxW = 560.f;
    float boxH = 260.f;
    float boxX = (static_cast<float>(width) - boxW) / 2.f;
    float boxY = (static_cast<float>(height) - boxH) / 2.f;

    sf::RectangleShape cBox({ boxW, boxH });
    cBox.setPosition({ boxX, boxY });
    cBox.setFillColor(sf::Color(22, 25, 34, 250));
    cBox.setOutlineColor(warningMessage.empty() ? sf::Color(160, 160, 175) : sf::Color(255, 190, 40));
    cBox.setOutlineThickness(3.f);
    window.draw(cBox);

    // Title / Header
    sf::Text titleTxt(uiFont, warningMessage.empty() ? "CONFIRM TRADE" : "! ECONOMIC TRADE ADVISORY !", 18);
    titleTxt.setStyle(sf::Text::Bold);
    titleTxt.setFillColor(warningMessage.empty() ? sf::Color::White : sf::Color(255, 205, 50));
    titleTxt.setPosition({ boxX + 24.f, boxY + 16.f });
    window.draw(titleTxt);

    sf::RectangleShape divLine({ boxW - 48.f, 1.5f });
    divLine.setPosition({ boxX + 24.f, boxY + 46.f });
    divLine.setFillColor(sf::Color(70, 75, 90));
    window.draw(divLine);

    // Warning message with word wrapping
    std::string textToDisplay = warningMessage.empty() ? "Are you sure you want to proceed with this trade?" : warningMessage;
    std::vector<std::string> lines;
    std::stringstream ss(textToDisplay);
    std::string word;
    std::string currentLine = "";
    while (ss >> word) {
        if (!currentLine.empty() && currentLine.length() + 1 + word.length() > 48) {
            lines.push_back(currentLine);
            currentLine = word;
        } else {
            if (!currentLine.empty()) currentLine += " ";
            currentLine += word;
        }
    }
    if (!currentLine.empty()) lines.push_back(currentLine);

    float textY = boxY + 62.f;
    for (const auto& line : lines) {
        sf::Text lineTxt(uiFont, line, 16);
        lineTxt.setFillColor(warningMessage.empty() ? sf::Color(220, 220, 230) : sf::Color(255, 240, 210));
        lineTxt.setPosition({ boxX + 26.f, textY });
        window.draw(lineTxt);
        textY += 24.f;
    }

    // Accept button (Yes)
    sf::RectangleShape yBtn({ 140.f, 42.f });
    yBtn.setPosition({ boxX + 90.f, boxY + 195.f });
    yBtn.setFillColor(sf::Color(38, 145, 65));
    yBtn.setOutlineColor(sf::Color(75, 210, 110));
    yBtn.setOutlineThickness(1.5f);
    window.draw(yBtn);

    sf::Text yTxt(uiFont, "Accept Trade", 16);
    yTxt.setStyle(sf::Text::Bold);
    yTxt.setFillColor(sf::Color::White);
    yTxt.setPosition({ yBtn.getPosition().x + 20.f, yBtn.getPosition().y + 10.f });
    window.draw(yTxt);

    // Reject button (No)
    sf::RectangleShape nBtn({ 140.f, 42.f });
    nBtn.setPosition({ boxX + 330.f, boxY + 195.f });
    nBtn.setFillColor(sf::Color(175, 45, 45));
    nBtn.setOutlineColor(sf::Color(235, 80, 80));
    nBtn.setOutlineThickness(1.5f);
    window.draw(nBtn);

    sf::Text nTxt(uiFont, "Reject Trade", 16);
    nTxt.setStyle(sf::Text::Bold);
    nTxt.setFillColor(sf::Color::White);
    nTxt.setPosition({ nBtn.getPosition().x + 22.f, nBtn.getPosition().y + 10.f });
    window.draw(nTxt);
}

void applyStructureScale(
    sf::Sprite& spr,
    int id,
    float rotation,
    bool isOpen,
    float& outShearY
) {
    outShearY = 0.f;  // No shear in square-grid system
    auto sz = spr.getTexture().getSize();
    if (sz.x == 0 || sz.y == 0) return;

    float tw = static_cast<float>(sz.x);
    float th = static_cast<float>(sz.y);

    if (id == ITEM_WOOD_TILE) {
        // 500×500 footprint
        spr.setScale({ 500.f / tw, 500.f / th });
    }
    else if (id == ITEM_WOOD_WALL) {
        spr.setScale({ 500.f / tw, 500.f / th });
    }
    else if (id == ITEM_WOOD_DOOR) {
        if (isOpen) {
            // Open: thin strip 500×50
            spr.setScale({ 500.f / tw, 50.f / th });
        } else {
            // Closed: 500 wide × 500 tall (same length and width as wood wall)
            spr.setScale({ 500.f / tw, 500.f / th });
        }
    }
    else if (id == ITEM_FURNACE) {
        // 500×500
        spr.setScale({ 500.f / tw, 500.f / th });
    }
    else if (id == ITEM_BED) {
        // 500×500
        spr.setScale({ 500.f / tw, 500.f / th });
    }
    else if (id == ITEM_WOOD_STAIRS) {
        // 500×500
        spr.setScale({ 500.f / tw, 500.f / th });
    }
    else if (id == ITEM_WOOD_ROOF) {
        // 500×500
        spr.setScale({ 500.f / tw, 500.f / th });
    }
    else if (id == ITEM_NONE) {
        // Campfire: 500×500
        spr.setScale({ 500.f / tw, 500.f / th });
    }
    else {
        // Default: 250×250
        spr.setScale({ 250.f / tw, 250.f / th });
    }
}


#include "Inventory.h"
#include "HUD.h"
#include <cmath>

bool handleSaplingPlacementClicks(
    sf::Vector2f uiPos,
    sf::Vector2f worldPos,
    unsigned int width,
    unsigned int height,
    bool& saplingPlacementMode,
    sf::Vector2f& saplingPlacementPos,
    float& saplingRotation,
    bool& saplingFollowingCursor,
    int saplingFromInventorySlot,
    const sf::Texture& saplingTex,
    std::vector<PlantedSapling>& plantedSaplings,
    Inventory& inventory,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc
) {
    float btnW = 220.f, btnH = 70.f, gap = 40.f;
    float tWidth = btnW * 3.f + gap * 2.f;
    float sX = (width - tWidth) / 2.f;
    float yP = height - 110.f;
    sf::FloatRect placeBtn({ sX, yP }, { btnW, btnH });
    sf::FloatRect rotateBtn({ sX + btnW + gap, yP }, { btnW, btnH });
    sf::FloatRect cancelBtn({ sX + (btnW + gap) * 2.f, yP }, { btnW, btnH });

    if (placeBtn.contains(uiPos)) {
        PlantedSapling s;
        s.position = saplingPlacementPos;
        s.growTimer = 40.f;
        s.grown = false;
        s.rotation = saplingRotation;
        plantedSaplings.push_back(s);

        if (saplingFromInventorySlot != -1) {
            inventory.consumeItem(saplingFromInventorySlot);
        } else {
            for (int i = 0; i < 20; i++) {
                if (inventory.hasItem(i)) {
                    if (inventory.getItemTexture(i) == &saplingTex) {
                        inventory.consumeItem(i);
                        break;
                    }
                }
            }
        }
        saplingPlacementMode = false;
        return true;
    }
    else if (rotateBtn.contains(uiPos)) {
        saplingRotation += 90.f;
        if (saplingRotation >= 360.f) saplingRotation -= 360.f;
        return true;
    }
    else if (cancelBtn.contains(uiPos)) {
        saplingPlacementMode = false;
        return true;
    }
    else {
        saplingFollowingCursor = !saplingFollowingCursor;
        saplingPlacementPos = snapGridFunc(worldPos);
        return true;
    }
}

bool handleStructurePlacementClicks(
    sf::Vector2f uiPos,
    sf::Vector2f worldPos,
    unsigned int width,
    unsigned int height,
    bool& placementMode,
    int placementItemID,
    int placementFromInventorySlot,
    sf::Vector2f& placementPos,
    float& placementRotation,
    bool& placementFollowingCursor,
    const std::string& username,
    std::vector<PlacedCampfire>& campfires,
    std::vector<PlacedStructure>& structures,
    Inventory& inventory,
    QuestInfo& qInfo,
    bool tutorialActive,
    AuthManager& authManager,
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Texture& chestTex,
    std::function<void()> syncSaveCallback,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc
) {
    float btnW = 220.f, btnH = 70.f, gap = 40.f;
    float tWidth = btnW * 3.f + gap * 2.f;
    float sX = (width - tWidth) / 2.f;
    float yP = height - 110.f;
    sf::FloatRect placeBtn({ sX, yP }, { btnW, btnH });
    sf::FloatRect rotateBtn({ sX + btnW + gap, yP }, { btnW, btnH });
    sf::FloatRect cancelBtn({ sX + (btnW + gap) * 2.f, yP }, { btnW, btnH });

    if (placeBtn.contains(uiPos)) {
        if (placementItemID == ITEM_NONE) {
            for (const auto& it : structures) {
                if (it.sprite.has_value() && it.sprite->getPosition() == placementPos) {
                    if (!it.owner.empty() && it.owner != username) {
                        inventory.showBanner("This belongs to " + it.owner + "! You cannot build over other players' structures.");
                        return true;
                    }
                }
            }
            for (const auto& c : campfires) {
                if (std::hypot(c.position.x - placementPos.x, c.position.y - placementPos.y) < 50.f) {
                    if (!c.owner.empty() && c.owner != username) {
                        inventory.showBanner("This belongs to " + c.owner + "! You cannot build over other players' campfires.");
                        return true;
                    }
                }
            }
            if (placementFromInventorySlot != -1) {
                inventory.consumeItem(placementFromInventorySlot);
            }
            PlacedCampfire pc;
            pc.position = placementPos;
            pc.timeRemaining = 0.f;
            pc.isCrafted = true;   // immediately usable — wrench works right away
            pc.currentFrame = 0;
            pc.frameTimer = 0.f;
            pc.owner = username;
            campfires.push_back(pc);
            syncSaveCallback();

            if (!tutorialActive && qInfo.questID == 3 && qInfo.progress < 1) {
                qInfo.progress++;
                std::thread([&authManager, username, prog = qInfo.progress, comp = qInfo.isCompleted]() {
                    try { authManager.updateQuestProgress(username.c_str(), prog, comp); } catch (...) {}
                }).detach();
            }
            placementMode = false;
        }
        else {
            // ── Overlap guard for wall / door ─────────────────────────────────
            // Wall and door are mutually exclusive: never place one on top of the other.
            if (placementItemID == ITEM_WOOD_WALL || placementItemID == ITEM_WOOD_DOOR) {
                for (const auto& it : structures) {
                    if (!it.sprite.has_value()) continue;
                    if (it.sprite->getPosition() != placementPos) continue;
                    if (it.id == ITEM_WOOD_WALL || it.id == ITEM_WOOD_DOOR) {
                        // Cell already occupied — refuse placement
                        return true;
                    }
                }
            }

            // ── Guard for wooden stairs: must be placed on top of an existing wood tile ──
            if (placementItemID == ITEM_WOOD_STAIRS) {
                bool hasTileUnder = false;
                for (const auto& it : structures) {
                    if (it.id == ITEM_WOOD_TILE && it.sprite.has_value()) {
                        if (std::abs(it.sprite->getPosition().x - placementPos.x) < 1.f &&
                            std::abs(it.sprite->getPosition().y - placementPos.y) < 1.f) {
                            hasTileUnder = true;
                            break;
                        }
                    }
                }
                if (!hasTileUnder) {
                    // Cannot place stairs without a wood tile underneath!
                    return true;
                }
            }

            // ── Second-floor wood tile logic ──────────────────────────────────
            float targetFloor = 0.f;
            if (placementItemID == ITEM_WOOD_TILE) {
                bool hasStairsBuilt = false;
                bool hasGroundTileAtPos = false;
                bool hasSecondFloorTileAtPos = false;
                for (const auto& s : structures) {
                    if (s.id == ITEM_WOOD_STAIRS) hasStairsBuilt = true;
                    if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
                        if (std::abs(s.sprite->getPosition().x - placementPos.x) < 1.f &&
                            std::abs(s.sprite->getPosition().y - placementPos.y) < 1.f) {
                            if (s.shearY >= 1.f) hasSecondFloorTileAtPos = true;
                            else hasGroundTileAtPos = true;
                        }
                    }
                }
                if (hasStairsBuilt && hasGroundTileAtPos) {
                    if (hasSecondFloorTileAtPos) {
                        // Both ground and second floor tiles already exist here
                        return true;
                    }
                    targetFloor = 1.f;
                }
            }

            for (const auto& it : structures) {
                if (it.sprite.has_value() && it.sprite->getPosition() == placementPos) {
                    if (!it.owner.empty() && it.owner != username) {
                        inventory.showBanner("This belongs to " + it.owner + "! You cannot build over other players' structures.");
                        return true;
                    }
                }
            }
            for (const auto& c : campfires) {
                if (std::hypot(c.position.x - placementPos.x, c.position.y - placementPos.y) < 50.f) {
                    if (!c.owner.empty() && c.owner != username) {
                        inventory.showBanner("This belongs to " + c.owner + "! You cannot build over other players' campfires.");
                        return true;
                    }
                }
            }

            if (placementFromInventorySlot != -1) {
                inventory.consumeItem(placementFromInventorySlot);
            }
            bool structureRemoved = false;

            for (auto it = structures.begin(); it != structures.end(); ) {
                if (it->sprite.has_value() && it->sprite->getPosition() == placementPos) {
                    bool shouldRemove = false;
                    if (placementItemID == ITEM_WOOD_DOOR || placementItemID == ITEM_WOOD_WALL) {
                        if (it->id == ITEM_WOOD_WALL || it->id == ITEM_WOOD_DOOR) {
                            shouldRemove = true;
                        }
                    } else if (placementItemID == ITEM_WOOD_TILE) {
                        // Only remove tile on the same floor level
                        if (it->id == ITEM_WOOD_TILE) {
                            bool itIs2ndFloor = (it->shearY >= 1.f);
                            bool targetIs2ndFloor = (targetFloor >= 1.f);
                            if (itIs2ndFloor == targetIs2ndFloor) {
                                shouldRemove = true;
                            }
                        }
                    } else if (it->id == placementItemID) {
                        shouldRemove = true;
                    }
                    if (shouldRemove) {
                        it = structures.erase(it);
                        structureRemoved = true;
                    } else {
                        ++it;
                    }
                } else {
                    ++it;
                }
            }
            if (structureRemoved) {
                syncSaveCallback();
            }
            if (placementItemID == ITEM_FURNACE) structures.emplace_back(ITEM_FURNACE, furnaceTex, placementRotation, false);
            else if (placementItemID == ITEM_WOOD_TILE) structures.emplace_back(ITEM_WOOD_TILE, woodTileTex, placementRotation, false);
            else if (placementItemID == ITEM_WOOD_WALL) structures.emplace_back(ITEM_WOOD_WALL, woodWallTex, placementRotation, false);
            else if (placementItemID == ITEM_WOOD_DOOR) structures.emplace_back(ITEM_WOOD_DOOR, woodDoorTex, placementRotation, false);
            else if (placementItemID == ITEM_WOOD_ROOF) structures.emplace_back(ITEM_WOOD_ROOF, woodRoofTex, placementRotation, false);
            else if (placementItemID == ITEM_WOOD_STAIRS) structures.emplace_back(ITEM_WOOD_STAIRS, woodStairsTex, placementRotation, false);
            else if (placementItemID == ITEM_BED) structures.emplace_back(ITEM_BED, bedTex, placementRotation, false);
            else if (placementItemID == ITEM_CHEST) structures.emplace_back(ITEM_CHEST, chestTex, placementRotation, false);

            auto& newS = structures.back();
            newS.owner = username;
            newS.shearY = targetFloor;
            sf::Vector2u ts = newS.sprite->getTexture().getSize();
            // Wall, door, and stairs: origin = bottom-left so the sprite's left edge
            // aligns precisely with the rhombus cell's BL corner (snapToGrid result).
            // All other structures: origin = bottom-center.
            if (placementItemID == ITEM_WOOD_WALL || placementItemID == ITEM_WOOD_DOOR || placementItemID == ITEM_WOOD_STAIRS) {
                newS.sprite->setOrigin({ 0.f, (float)ts.y });
            } else {
                newS.sprite->setOrigin({ ts.x / 2.f, (float)ts.y });
            }
            applyStructureScale(*newS.sprite, placementItemID, placementRotation, false, newS.shearY);
            newS.sprite->setPosition(placementPos);
            syncSaveCallback();
            placementMode = false;
        }
        return true;
    }
    else if (rotateBtn.contains(uiPos)) {
        if (placementItemID == ITEM_WOOD_WALL || placementItemID == ITEM_WOOD_DOOR) {
            // Wall and door: two orientations only (horizontal / diagonal)
            placementRotation = (placementRotation == 0.f) ? 90.f : 0.f;
        } else {
            placementRotation = (placementRotation == 0.f) ? 90.f : ((placementRotation == 90.f) ? 180.f : 0.f);
        }
        return true;
    }
    else if (cancelBtn.contains(uiPos)) {
        placementMode = false;
        return true;
    }
    else {
        placementFollowingCursor = !placementFollowingCursor;

        // Snap placement position based on item footprint
        if (placementItemID == ITEM_NONE) {
            // Campfire: place inside the grid cell, not at the border
            sf::Vector2f corner = snapToGrid(worldPos);
            placementPos = { corner.x + 375.f, corner.y - 125.f };
        }
        else {
            // All structures snap to grid cell corner
            placementPos = snapToGrid(worldPos);
        }
        return true;
    }
}



bool handleWorldInteractionClicks(
    sf::Vector2f worldPos,
    Inventory& inventory,
    std::vector<PlacedCampfire>& campfires,
    const std::string& username,
    std::vector<PlacedStructure>& structures,
    const sf::Sprite& playerSprite,
    int playerFloorLevel,
    bool& isSleeping,
    float& sleepFadeTimer,
    bool& sleepTimeSkipped,
    bool& chestMenuOpen,
    PlacedStructure*& activeChest,
    std::function<void()> syncSaveFunc
) {
    int selSlot = inventory.getSelectedSlot();
    bool hasWrench = (selSlot != -1 && inventory.hasItem(selSlot) && inventory.getItemID(selSlot) == ITEM_WRENCH);

    // Check Campfire Wrench — wrench icon only, not campfire body click
    for (auto& fire : campfires) {
        sf::FloatRect wrenchBtnBounds({ fire.position.x - 25.f, fire.position.y - 100.f }, { 50.f, 50.f });
        if (wrenchBtnBounds.contains(worldPos)) {
            if (!fire.owner.empty() && fire.owner != username) {
                inventory.showBanner("This belongs to " + fire.owner + "! You cannot wrench other players' items.");
                return true;
            }
            for (auto& f : campfires) f.menuOpen = false;
            for (auto& s : structures) s.menuOpen = false;
            chestMenuOpen = false;
            fire.menuOpen = true;
            return true;
        }
    }

    // Check Bed Interaction Button & Wrench Click
    for (auto& s : structures) {
        if (s.id == ITEM_BED && s.sprite.has_value()) {
            sf::Vector2f bPos = s.sprite->getPosition();
            sf::FloatRect wrenchBtnBounds({ bPos.x - 25.f, bPos.y - 105.f }, { 50.f, 50.f });
            sf::FloatRect bedBounds({ bPos.x - 100.f, bPos.y - 120.f }, { 200.f, 120.f });
            if (wrenchBtnBounds.contains(worldPos) || bedBounds.contains(worldPos)) {
                if (!s.owner.empty() && s.owner != username) {
                    inventory.showBanner("This belongs to " + s.owner + "! You cannot wrench other players' items.");
                    return true;
                }
                if (!isSleeping) {
                    isSleeping = true;
                    sleepFadeTimer = 0.f;
                    sleepTimeSkipped = false;
                    return true;
                }
            }
        }
    }

    // Check Furnace Wrench — wrench icon only, not furnace body click
    for (auto& s : structures) {
        if (s.id == ITEM_FURNACE && s.sprite.has_value()) {
            sf::Vector2f fPos = s.sprite->getPosition();
            sf::FloatRect wrenchBtnBounds({ fPos.x - 25.f, fPos.y - 175.f }, { 50.f, 50.f });
            if (wrenchBtnBounds.contains(worldPos)) {
                if (!s.owner.empty() && s.owner != username) {
                    inventory.showBanner("This belongs to " + s.owner + "! You cannot wrench other players' items.");
                    return true;
                }
                for (auto& f : campfires) f.menuOpen = false;
                for (auto& st : structures) st.menuOpen = false;
                chestMenuOpen = false;
                s.menuOpen = true;
                return true;
            }
        }
    }

    // Check Door Interaction Button
    PlacedStructure* nearestDoor = nullptr;
    float minDist = 300.f;
    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_DOOR) {
            sf::Vector2f dPos = s.sprite->getPosition();
            sf::Vector2f pPos = playerSprite.getPosition();
            int doorFloor = getFloorLevelFromY(dPos.y);
            if (doorFloor == playerFloorLevel) {
                float dy = (dPos.y + doorFloor * 180.f) - pPos.y;
                float dx = dPos.x - pPos.x;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < minDist) {
                    minDist = d;
                    nearestDoor = &s;
                }
            }
        }
    }
    if (nearestDoor) {
        sf::FloatRect doorBtnBounds({ nearestDoor->sprite->getPosition().x - 100.f, nearestDoor->sprite->getPosition().y - 120.f }, { 200.f, 60.f });
        if (doorBtnBounds.contains(worldPos)) {
            if (!nearestDoor->owner.empty() && nearestDoor->owner != username) {
                inventory.showBanner("This belongs to " + nearestDoor->owner + "! You cannot wrench other players' items.");
                return true;
            }
            nearestDoor->isOpen = !nearestDoor->isOpen;
            float ignore;
            applyStructureScale(*nearestDoor->sprite, nearestDoor->id, nearestDoor->rotation, nearestDoor->isOpen, ignore);
            syncSaveFunc();
            return true;
        }
    }

    // Check Chest Interaction Button
    PlacedStructure* nearestChest = nullptr;
    float minChestDist = 200.f;
    for (auto& s : structures) {
        if (s.id == ITEM_CHEST) {
            sf::Vector2f cPos = s.sprite->getPosition();
            sf::Vector2f pPos = playerSprite.getPosition();
            int chestFloor = getFloorLevelFromY(cPos.y);
            if (chestFloor == playerFloorLevel) {
                float dy = (cPos.y + chestFloor * 200.f) - pPos.y;
                float dx = cPos.x - pPos.x;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < minChestDist) {
                    minChestDist = d;
                    nearestChest = &s;
                }
            }
        }
    }
    if (nearestChest && !chestMenuOpen) {
        sf::FloatRect chestBtnBounds({ nearestChest->sprite->getPosition().x - 100.f, nearestChest->sprite->getPosition().y - 120.f }, { 200.f, 60.f });
        if (chestBtnBounds.contains(worldPos)) {
            if (!nearestChest->owner.empty() && nearestChest->owner != username) {
                inventory.showBanner("This belongs to " + nearestChest->owner + "! You cannot wrench other players' items.");
                return true;
            }
            chestMenuOpen = true;
            activeChest = nearestChest;
            return true;
        }
    }

    return false;
}

void updateCampfiresAndCooking(
    std::vector<PlacedCampfire>& campfires,
    std::vector<PlacedStructure>& structures,
    float dt,
    std::function<void()> syncSaveCallback
) {
    for (auto& pc : campfires) {
        if (!pc.isCrafted) {
            pc.timeRemaining -= dt;
            if (pc.timeRemaining <= 0.f) {
                pc.timeRemaining = 0.f;
                pc.isCrafted = true;
            }
        }
        else {
            pc.frameTimer += dt;
            if (pc.frameTimer >= 0.1f) {
                pc.frameTimer = 0.f;
                pc.currentFrame = (pc.currentFrame + 1) % 8;
            }

            if (pc.inputCount > 0 && pc.fuel > 0) {
                int expectedOutput = 0;
                if (pc.inputItem == ITEM_CHICKEN_MEAT) expectedOutput = ITEM_COOKED_CHICKEN;
                else if (pc.inputItem == ITEM_PIG_MEAT) expectedOutput = ITEM_COOKED_PIG;
                else if (pc.inputItem == ITEM_SHEEP_MEAT) expectedOutput = ITEM_COOKED_SHEEP;

                if (expectedOutput != 0 && (pc.outputCount == 0 || pc.outputItem == expectedOutput)) {
                    if (pc.timeRemaining <= 0.f) {
                        pc.timeRemaining = 2.0f;
                    }
                    pc.timeRemaining -= dt;
                    if (pc.timeRemaining <= 0.f) {
                        pc.fuel--;
                        pc.inputCount--;
                        pc.outputItem = expectedOutput;
                        pc.outputCount++;
                        if (pc.inputCount <= 0) {
                            pc.inputItem = 0;
                        }
                        syncSaveCallback();
                    }
                }
            } else {
                pc.timeRemaining = 0.f;
            }
        }
    }

    for (auto& s : structures) {
        if (s.id == ITEM_FURNACE) {
            if (s.inputCount > 0 && s.fuel > 0) {
                int expectedOutput = 0;
                if (s.inputItem == ITEM_COPPER_ORE) expectedOutput = ITEM_COPPER_BAR;
                else if (s.inputItem == ITEM_IRON_ORE) expectedOutput = ITEM_IRON_BAR;
                else if (s.inputItem == ITEM_GOLD_ORE) expectedOutput = ITEM_GOLD_BAR;
                else if (s.inputItem == ITEM_CHICKEN_MEAT) expectedOutput = ITEM_COOKED_CHICKEN;
                else if (s.inputItem == ITEM_PIG_MEAT) expectedOutput = ITEM_COOKED_PIG;
                else if (s.inputItem == ITEM_SHEEP_MEAT) expectedOutput = ITEM_COOKED_SHEEP;

                if (expectedOutput != 0 && (s.outputCount == 0 || s.outputItem == expectedOutput)) {
                    if (s.timeRemaining <= 0.f) {
                        s.timeRemaining = 2.0f;
                    }
                    s.timeRemaining -= dt;
                    if (s.timeRemaining <= 0.f) {
                        s.fuel--;
                        s.inputCount--;
                        s.outputItem = expectedOutput;
                        s.outputCount++;
                        if (s.inputCount <= 0) {
                            s.inputItem = 0;
                        }
                        syncSaveCallback();
                    }
                }
            } else {
                s.timeRemaining = 0.f;
            }
        }
    }
}

void updateAndDrawPlacementPreview(
    sf::RenderWindow& window,
    bool placementMode,
    bool placementFollowingCursor,
    int placementItemID,
    float& placementRotation,
    sf::Vector2f& placementPos,
    sf::Vector2f worldPos,
    const std::vector<PlacedStructure>& structures,
    std::function<sf::Vector2f(sf::Vector2f)> snapToShearedGrid,
    const sf::Texture campfireFrames[8],
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Texture& chestTex
) {
    if (!placementMode) return;

    if (placementFollowingCursor) {
        if (placementItemID == ITEM_NONE) {
            // Campfire: place inside the grid cell, not at the border
            sf::Vector2f corner = snapToGrid(worldPos);
            placementPos = { corner.x + 375.f, corner.y - 125.f };
        }
        else {
            placementPos = snapToGrid(worldPos);
        }
    }

    drawPlacementPreview(window, placementItemID, placementRotation, placementPos, campfireFrames, furnaceTex, woodTileTex, woodWallTex, woodDoorTex, woodRoofTex, woodStairsTex, bedTex, chestTex, structures);
}





