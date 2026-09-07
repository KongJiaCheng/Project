#include "StructureSystem.h"
#include "WorldRenderer.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "SheepMeat.h"
#include "ViewUtils.h"
#include "Game.h"
#include <algorithm>

void syncSaveStructuresAndCampfires(
    AuthManager& authManager,
    const std::string& username,
    bool dbStartupEnabled,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
) {
    if (!dbStartupEnabled) return;
    std::vector<StoredStructure> structData;
    for (const auto& s : structures)
    {
        if (s.sprite.has_value() &&
            (s.owner == username || s.owner == ""))
        {
            structData.push_back({
                s.id,
                s.sprite->getPosition().x,
                s.sprite->getPosition().y,
                s.health,
                s.rotation,
                s.isOpen,
                s.shearY
                });
        }
    }
    authManager.saveStructures(username.c_str(), structData);

    std::vector<StoredCampfire> campData;
    for (const auto& c : campfires)
    {
        if (c.owner == username || c.owner == "") {
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
    authManager.saveCampfires(username.c_str(), campData);
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
    const sf::Texture& cookedSheepTex
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

    if (fire.inputCount > 0) {
        const sf::Texture* rTex = (fire.inputItem == ITEM_CHICKEN_MEAT) ? ChickenMeat::getTexture() :
                                  (fire.inputItem == ITEM_PIG_MEAT) ? PigMeat::getTexture() :
                                  (fire.inputItem == ITEM_SHEEP_MEAT) ? SheepMeat::getTexture() : nullptr;
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

    // 3. Cooked Food slot
    sf::RectangleShape outSlot({ 80.f, 80.f });
    outSlot.setPosition({ bg.getPosition().x + 360.f, bg.getPosition().y + 110.f });
    bool hoverOut = outSlot.getGlobalBounds().contains(mpos);
    outSlot.setFillColor(sf::Color(30, 30, 30));
    outSlot.setOutlineThickness(2.f);
    outSlot.setOutlineColor(hoverOut ? sf::Color::Yellow : sf::Color(150, 150, 150));
    window.draw(outSlot);

    if (fire.outputCount > 0) {
        const sf::Texture* oTex = (fire.outputItem == ITEM_COOKED_CHICKEN) ? &cookedChickenTex :
                                  (fire.outputItem == ITEM_COOKED_PIG) ? &cookedPigTex :
                                  (fire.outputItem == ITEM_COOKED_SHEEP) ? &cookedSheepTex : nullptr;
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

    std::string statusStr = "Insert Fuel & Raw Food";
    sf::Color statusColor(150, 150, 150);
    if (fire.inputCount > 0 && fire.fuel > 0) {
        statusStr = "Cooking: " + std::to_string(static_cast<int>(fire.timeRemaining + 0.99f)) + "s";
        statusColor = sf::Color::Yellow;
    }
    else if (fire.outputCount > 0) {
        statusStr = "Cooked food ready! Click to retrieve.";
        statusColor = sf::Color::Green;
    }
    sf::Text statusTxt(uiFont, statusStr, 20);
    statusTxt.setFillColor(statusColor);
    statusTxt.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 215.f });
    window.draw(statusTxt);
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
    const sf::Texture& cookedSheepTex
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
        const sf::Texture* rTex = (furnace.inputItem == ITEM_CHICKEN_MEAT) ? ChickenMeat::getTexture() : 
                                  (furnace.inputItem == ITEM_PIG_MEAT) ? PigMeat::getTexture() : 
                                  (furnace.inputItem == ITEM_SHEEP_MEAT) ? SheepMeat::getTexture() : nullptr;
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

    // 3. Output slot
    sf::RectangleShape outputSlot({ 80.f, 80.f });
    outputSlot.setPosition({ bg.getPosition().x + 360.f, bg.getPosition().y + 110.f });
    bool hoverOutput = outputSlot.getGlobalBounds().contains(mpos);
    outputSlot.setFillColor(sf::Color(30, 30, 30));
    outputSlot.setOutlineThickness(2.f);
    outputSlot.setOutlineColor(hoverOutput ? sf::Color::Yellow : sf::Color(150, 150, 150));
    window.draw(outputSlot);

    if (furnace.outputCount > 0 && furnace.outputItem != 0) {
        const sf::Texture* oTex = (furnace.outputItem == ITEM_COOKED_CHICKEN) ? &cookedChickenTex :
                                  (furnace.outputItem == ITEM_COOKED_PIG) ? &cookedPigTex :
                                  (furnace.outputItem == ITEM_COOKED_SHEEP) ? &cookedSheepTex : nullptr;
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

    std::string statusStr = "Insert Fuel & Raw Food";
    sf::Color statusColor(150, 150, 150);
    if (furnace.inputCount > 0 && furnace.fuel > 0) {
        statusStr = "Smelting: " + std::to_string(static_cast<int>(furnace.timeRemaining + 0.99f)) + "s";
        statusColor = sf::Color::Yellow;
    }
    else if (furnace.outputCount > 0) {
        statusStr = "Cooked food ready! Click to retrieve.";
        statusColor = sf::Color::Green;
    }
    sf::Text statusTxt(uiFont, statusStr, 20);
    statusTxt.setFillColor(statusColor);
    statusTxt.setPosition({ bg.getPosition().x + 60.f, bg.getPosition().y + 215.f });
    window.draw(statusTxt);
}

void drawQuantityPrompt(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    bool isCampfire,
    int campfireSlot,
    int maxAmount,
    TextInput& quantityInput
) {
    sf::View currentView = window.getView();
    window.setView(GetUIView(&window));

    sf::RectangleShape qBox({ 400.f, 220.f });
    sf::Vector2f qBoxPos = { ((float)width - 400.f) / 2.f, ((float)height - 220.f) / 2.f };
    qBox.setPosition(qBoxPos);
    qBox.setFillColor(sf::Color(20, 25, 35, 250));
    qBox.setOutlineThickness(3.f);
    qBox.setOutlineColor(sf::Color(0, 242, 254, 180));
    window.draw(qBox);

    sf::Text qTitle(uiFont, "ENTER AMOUNT", 20);
    qTitle.setStyle(sf::Text::Bold);
    qTitle.setFillColor(sf::Color(0, 242, 254));
    qTitle.setPosition({ qBoxPos.x + 30.f, qBoxPos.y + 25.f });
    window.draw(qTitle);

    std::string labelText = "Enter quantity to put in trade";
    if (isCampfire) {
        if (campfireSlot == 1) {
            labelText = "Enter quantity of fuel to place";
        } else {
            labelText = "Enter quantity of food to place";
        }
    }
    sf::Text qLabel(uiFont, labelText + " (Max: " + std::to_string(maxAmount) + "):", 12);
    qLabel.setFillColor(sf::Color(200, 200, 210));
    qLabel.setPosition({ qBoxPos.x + 30.f, qBoxPos.y + 65.f });
    window.draw(qLabel);

    quantityInput.setPosition({ qBoxPos.x + 30.f, qBoxPos.y + 95.f });
    quantityInput.draw(window);

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    sf::FloatRect confirmBtnRect({ qBoxPos.x + 30.f, qBoxPos.y + 150.f }, { 160.f, 40.f });
    bool confirmHover = confirmBtnRect.contains(mousePos);
    sf::RectangleShape confirmBtn({ 160.f, 40.f });
    confirmBtn.setPosition({ qBoxPos.x + 30.f, qBoxPos.y + 150.f });
    confirmBtn.setFillColor(confirmHover ? sf::Color(0, 200, 254) : sf::Color(0, 150, 200));
    window.draw(confirmBtn);

    sf::Text confirmTxt(uiFont, "Confirm", 15);
    confirmTxt.setStyle(sf::Text::Bold);
    confirmTxt.setFillColor(sf::Color::White);
    confirmTxt.setPosition({ confirmBtn.getPosition().x + (confirmBtn.getSize().x - confirmTxt.getGlobalBounds().size.x) / 2.f, confirmBtn.getPosition().y + 10.f });
    window.draw(confirmTxt);

    sf::FloatRect cancelBtnRect({ qBoxPos.x + 210.f, qBoxPos.y + 150.f }, { 160.f, 40.f });
    bool cancelHover = cancelBtnRect.contains(mousePos);
    sf::RectangleShape cancelBtn({ 160.f, 40.f });
    cancelBtn.setPosition({ qBoxPos.x + 210.f, qBoxPos.y + 150.f });
    cancelBtn.setFillColor(cancelHover ? sf::Color(200, 60, 60) : sf::Color(150, 40, 40));
    window.draw(cancelBtn);

    sf::Text cancelTxt(uiFont, "Cancel", 15);
    cancelTxt.setStyle(sf::Text::Bold);
    cancelTxt.setFillColor(sf::Color::White);
    cancelTxt.setPosition({ cancelBtn.getPosition().x + (cancelBtn.getSize().x - cancelTxt.getGlobalBounds().size.x) / 2.f, cancelBtn.getPosition().y + 10.f });
    window.draw(cancelTxt);

    window.setView(currentView);
}

void drawTradeConfirmPopup(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height
) {
    sf::RectangleShape cBox({ 300.f, 150.f });
    cBox.setPosition({ (width - 300.f) / 2.f, height / 2.f - 75.f });
    cBox.setFillColor(sf::Color(20, 20, 20, 240));
    cBox.setOutlineColor(sf::Color::White);
    cBox.setOutlineThickness(2.f);
    window.draw(cBox);

    sf::Text cTxt(uiFont, "Confirm Trade?", 20);
    cTxt.setPosition({ cBox.getPosition().x + 70.f, cBox.getPosition().y + 20.f });
    window.draw(cTxt);

    sf::RectangleShape yBtn({ 100.f, 40.f });
    yBtn.setPosition({ (width - 250.f) / 2.f, height / 2.f });
    yBtn.setFillColor(sf::Color::Green);
    window.draw(yBtn);

    sf::Text yTxt(uiFont, "Yes", 18);
    yTxt.setPosition({ yBtn.getPosition().x + 30.f, yBtn.getPosition().y + 10.f });
    window.draw(yTxt);

    sf::RectangleShape nBtn({ 100.f, 40.f });
    nBtn.setPosition({ (width + 50.f) / 2.f, height / 2.f });
    nBtn.setFillColor(sf::Color::Red);
    window.draw(nBtn);

    sf::Text nTxt(uiFont, "No", 18);
    nTxt.setPosition({ nBtn.getPosition().x + 35.f, nBtn.getPosition().y + 10.f });
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
            if (placementFromInventorySlot != -1) {
                inventory.consumeItem(placementFromInventorySlot);
            }
            PlacedCampfire pc;
            pc.position = placementPos;
            pc.timeRemaining = 90.f;
            pc.isCrafted = false;
            pc.currentFrame = 0;
            pc.frameTimer = 0.f;
            pc.owner = username;
            campfires.push_back(pc);
            syncSaveCallback();

            if (!tutorialActive && qInfo.questID == 3 && qInfo.progress < 1) {
                qInfo.progress++;
                authManager.updateQuestProgress(username.c_str(), qInfo.progress, qInfo.isCompleted);
            }
            placementMode = false;
        }
        else {
            if (placementFromInventorySlot != -1) {
                inventory.consumeItem(placementFromInventorySlot);
            }
            bool structureRemoved = false;

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

            for (auto it = structures.begin(); it != structures.end(); ) {
                if (it->sprite.has_value() && it->sprite->getPosition() == placementPos) {
                    bool shouldRemove = false;
                    if (placementItemID == ITEM_WOOD_DOOR || placementItemID == ITEM_WOOD_WALL) {
                        if (it->id == ITEM_WOOD_WALL || it->id == ITEM_WOOD_DOOR) {
                            shouldRemove = true;
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
            sf::Vector2u ts = newS.sprite->getTexture().getSize();
            // Wall and door: origin = bottom-left so the sprite's left edge
            // aligns precisely with the rhombus cell's BL corner (snapToGrid result).
            // All other structures: origin = bottom-center.
            if (placementItemID == ITEM_WOOD_WALL || placementItemID == ITEM_WOOD_DOOR) {
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

    // Check Campfire Wrench click in world coordinates
    for (auto& fire : campfires) {
        if (fire.isCrafted) {
            sf::FloatRect wrenchBtnBounds({ fire.position.x - 20.5f, fire.position.y - 480.f - 20.5f }, { 41.f, 41.f });
            sf::FloatRect fireBounds({ fire.position.x - 50.f, fire.position.y - 50.f }, { 100.f, 100.f });
            if (wrenchBtnBounds.contains(worldPos) || fireBounds.contains(worldPos)) {
                if (hasWrench) {
                    for (auto& f : campfires) f.menuOpen = false;
                    for (auto& s : structures) s.menuOpen = false;
                    if (fire.owner.empty() || fire.owner == username)
                        fire.menuOpen = true;
                    return true;
                }
            }
        }
    }

    // Check Bed Interaction Button & Wrench Click
    for (auto& s : structures) {
        if (s.id == ITEM_BED && s.sprite.has_value()) {
            sf::Vector2f bPos = s.sprite->getPosition();
            sf::FloatRect wrenchBtnBounds({ bPos.x - 25.f, bPos.y - 480.f - 25.f }, { 50.f, 50.f });
            sf::FloatRect bedBounds({ bPos.x - 100.f, bPos.y - 120.f }, { 200.f, 120.f });
            if (wrenchBtnBounds.contains(worldPos) || bedBounds.contains(worldPos)) {
                if (hasWrench && !isSleeping) {
                    isSleeping = true;
                    sleepFadeTimer = 0.f;
                    sleepTimeSkipped = false;
                    return true;
                }
            }
        }
    }

    // Check Furnace Wrench Click
    for (auto& s : structures) {
        if (s.id == ITEM_FURNACE && s.sprite.has_value()) {
            sf::Vector2f fPos = s.sprite->getPosition();
            sf::FloatRect wrenchBtnBounds({ fPos.x - 25.f, fPos.y - 480.f - 25.f }, { 50.f, 50.f });
            sf::FloatRect furnaceBounds({ fPos.x - 100.f, fPos.y - 120.f }, { 200.f, 120.f });
            if (wrenchBtnBounds.contains(worldPos) || furnaceBounds.contains(worldPos)) {
                if (hasWrench) {
                    for (auto& f : campfires) f.menuOpen = false;
                    for (auto& st : structures) st.menuOpen = false;
                    s.menuOpen = true;
                    return true;
                }
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
                if (pc.timeRemaining <= 0.f) {
                    pc.timeRemaining = 2.0f;
                }
                pc.timeRemaining -= dt;
                if (pc.timeRemaining <= 0.f) {
                    pc.fuel--;
                    pc.inputCount--;
                    if (pc.inputItem == ITEM_CHICKEN_MEAT) pc.outputItem = ITEM_COOKED_CHICKEN;
                    else if (pc.inputItem == ITEM_PIG_MEAT) pc.outputItem = ITEM_COOKED_PIG;
                    else if (pc.inputItem == ITEM_SHEEP_MEAT) pc.outputItem = ITEM_COOKED_SHEEP;
                    pc.outputCount++;
                    if (pc.inputCount <= 0) {
                        pc.inputItem = 0;
                    }
                    syncSaveCallback();
                }
            } else {
                pc.timeRemaining = 0.f;
            }
        }
    }

    for (auto& s : structures) {
        if (s.id == ITEM_FURNACE) {
            if (s.inputCount > 0 && s.fuel > 0) {
                if (s.timeRemaining <= 0.f) {
                    s.timeRemaining = 2.0f;
                }
                s.timeRemaining -= dt;
                if (s.timeRemaining <= 0.f) {
                    s.fuel--;
                    s.inputCount--;
                    if (s.inputItem == ITEM_CHICKEN_MEAT) s.outputItem = ITEM_COOKED_CHICKEN;
                    else if (s.inputItem == ITEM_PIG_MEAT) s.outputItem = ITEM_COOKED_PIG;
                    else if (s.inputItem == ITEM_SHEEP_MEAT) s.outputItem = ITEM_COOKED_SHEEP;
                    s.outputCount++;
                    if (s.inputCount <= 0) {
                        s.inputItem = 0;
                    }
                    syncSaveCallback();
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

    drawPlacementPreview(window, placementItemID, placementRotation, placementPos, campfireFrames, furnaceTex, woodTileTex, woodWallTex, woodDoorTex, woodRoofTex, woodStairsTex, bedTex, chestTex);
}





