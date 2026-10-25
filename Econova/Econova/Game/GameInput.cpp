#include "GameInput.h"
#include "ViewUtils.h"
#include "ItemID.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "SheepMeat.h"
#include "Server.h"
#include "Game.h"
#include <thread>

bool handleQuantityPromptEvent(
    const sf::Event& event,
    sf::RenderWindow& window,
    bool& quantityPromptOpen,
    TextInput& quantityInput,
    bool& quantityPromptIsCampfire,
    int quantityPromptCampfireSlot,
    int quantityPromptSourceSlot,
    int quantityPromptTargetSlot,
    bool quantityPromptIsP2P,
    std::vector<PlacedCampfire>& campfires,
    Inventory& inventory,
    TradeSlot myP2PSlots[4],
    bool& p2pOfferDirty,
    bool& p2pMyReady,
    TradeSlot playerTradeSlots[4],
    bool& isSellingToNPC,
    int& npcTradeGold,
    const std::vector<StockItem>& stockMarket,
    std::string& npcGoldInput,
    bool& bargainAccepted,
    bool& saplingPlacementMode,
    std::function<void()> syncSaveCallback,
    std::vector<PlacedStructure>* structures,
    bool& quantityPromptIsChest,
    int quantityPromptChestSlot,
    PlacedStructure* activeChest,
    int quantityPromptMax,
    std::function<const sf::Texture*(int id)> getItemTexture
) {
    if (!quantityPromptOpen) return false;

    // Helper lambda to safely execute the confirmation logic across Enter key and Confirm button
    auto applyQuantityConfirm = [&]() -> bool {
        int amt = 1;
        try {
            std::string textVal = quantityInput.getText();
            if (!textVal.empty()) amt = std::stoi(textVal);
        } catch (...) {
            amt = 1;
        }
        if (amt < 1) amt = 1;
        if (quantityPromptMax > 0 && amt > quantityPromptMax) amt = quantityPromptMax;

        if (quantityPromptIsCampfire) {
            PlacedCampfire* activeFire = nullptr;
            for (auto& f : campfires) {
                if (f.menuOpen) {
                    activeFire = &f;
                    break;
                }
            }
            PlacedStructure* activeFurnace = nullptr;
            if (!activeFire && structures) {
                for (auto& s : *structures) {
                    if (s.id == ITEM_FURNACE && s.menuOpen) {
                        activeFurnace = &s;
                        break;
                    }
                }
            }

            if (activeFire) {
                if (quantityPromptSourceSlot >= 0 && quantityPromptSourceSlot < 20) {
                    // STORE into campfire
                    int available = inventory.getItemCount(quantityPromptSourceSlot);
                    amt = std::clamp(amt, 1, available);
                    int itemID = inventory.getItemID(quantityPromptSourceSlot);
                    if (quantityPromptCampfireSlot == 1) { // Fuel
                        if (itemID == ITEM_WOOD) {
                            activeFire->fuel += amt * 4;
                        } else if (itemID == ITEM_SAPLING) {
                            activeFire->fuel += amt;
                            saplingPlacementMode = false;
                        }
                        if (activeFire->inputCount > 0 && activeFire->timeRemaining <= 0.f) {
                            activeFire->timeRemaining = 2.0f;
                        }
                        inventory.showBanner("Added " + std::to_string(amt) + "x " + Inventory::getItemName(itemID) + " as fuel!");
                    } else if (quantityPromptCampfireSlot == 2) { // Raw Food
                        activeFire->inputItem = itemID;
                        activeFire->inputCount += amt;
                        if (activeFire->timeRemaining <= 0.f) {
                            activeFire->timeRemaining = 2.0f;
                        }
                        inventory.showBanner("Added " + std::to_string(amt) + "x " + Inventory::getItemName(itemID) + " to campfire!");
                    }
                    for (int c = 0; c < amt; c++) {
                        inventory.consumeItem(quantityPromptSourceSlot);
                    }
                    if (syncSaveCallback) syncSaveCallback();
                } else if (quantityPromptSourceSlot == -1) {
                    // RETRIEVE / WITHDRAW from campfire
                    if (quantityPromptCampfireSlot == 1) { // Withdraw fuel
                        int availWood = activeFire->fuel / 4;
                        int woodToTake = std::clamp(amt, 1, std::max(1, availWood));
                        const sf::Texture* tex = getItemTexture ? getItemTexture(ITEM_WOOD) : nullptr;
                        if (woodToTake > 0) {
                            if (tex) inventory.addItem(ITEM_WOOD, *tex, woodToTake);
                            activeFire->fuel -= woodToTake * 4;
                            if (activeFire->fuel < 0) activeFire->fuel = 0;
                            inventory.showBanner("Withdrew " + std::to_string(woodToTake) + "x Wood from campfire fuel.");
                        }
                    } else if (quantityPromptCampfireSlot == 2) { // Withdraw raw input
                        int actualAmt = std::clamp(amt, 1, activeFire->inputCount);
                        if (actualAmt > 0 && activeFire->inputItem != 0) {
                            const sf::Texture* rTex = getItemTexture ? getItemTexture(activeFire->inputItem) : nullptr;
                            if (rTex) {
                                inventory.addItem(activeFire->inputItem, *rTex, actualAmt);
                            }
                            activeFire->inputCount -= actualAmt;
                            if (activeFire->inputCount <= 0) {
                                activeFire->inputCount = 0;
                                activeFire->inputItem = 0;
                                activeFire->timeRemaining = 0.f;
                            }
                            inventory.showBanner("Withdrew " + std::to_string(actualAmt) + "x " + Inventory::getItemName(activeFire->inputItem) + ".");
                        }
                    } else if (quantityPromptCampfireSlot == 3) { // Retrieve cooked output
                        int actualAmt = std::clamp(amt, 1, activeFire->outputCount);
                        if (actualAmt > 0 && activeFire->outputItem != 0) {
                            const sf::Texture* oTex = getItemTexture ? getItemTexture(activeFire->outputItem) : nullptr;
                            if (oTex) {
                                inventory.addItem(activeFire->outputItem, *oTex, actualAmt);
                            }
                            activeFire->outputCount -= actualAmt;
                            if (activeFire->outputCount <= 0) {
                                activeFire->outputCount = 0;
                                activeFire->outputItem = 0;
                            }
                            inventory.showBanner("Retrieved " + std::to_string(actualAmt) + "x " + Inventory::getItemName(activeFire->outputItem) + "!");
                        }
                    }
                    if (syncSaveCallback) syncSaveCallback();
                }
            } else if (activeFurnace) {
                if (quantityPromptSourceSlot >= 0 && quantityPromptSourceSlot < 20) {
                    // STORE into furnace
                    int available = inventory.getItemCount(quantityPromptSourceSlot);
                    amt = std::clamp(amt, 1, available);
                    int itemID = inventory.getItemID(quantityPromptSourceSlot);
                    if (quantityPromptCampfireSlot == 1) { // Fuel
                        if (itemID == ITEM_COAL) {
                            activeFurnace->fuel += amt * 8;
                        } else if (itemID == ITEM_WOOD) {
                            activeFurnace->fuel += amt * 4;
                        } else if (itemID == ITEM_SAPLING) {
                            activeFurnace->fuel += amt;
                            saplingPlacementMode = false;
                        }
                        if (activeFurnace->inputCount > 0 && activeFurnace->timeRemaining <= 0.f) {
                            activeFurnace->timeRemaining = 2.0f;
                        }
                        inventory.showBanner("Added " + std::to_string(amt) + "x " + Inventory::getItemName(itemID) + " as fuel!");
                    } else if (quantityPromptCampfireSlot == 2) { // Ore or Raw Food
                        activeFurnace->inputItem = itemID;
                        activeFurnace->inputCount += amt;
                        if (activeFurnace->timeRemaining <= 0.f) {
                            activeFurnace->timeRemaining = 2.0f;
                        }
                        inventory.showBanner("Added " + std::to_string(amt) + "x " + Inventory::getItemName(itemID) + " to furnace!");
                    }
                    for (int c = 0; c < amt; c++) {
                        inventory.consumeItem(quantityPromptSourceSlot);
                    }
                    if (syncSaveCallback) syncSaveCallback();
                } else if (quantityPromptSourceSlot == -1) {
                    // RETRIEVE / WITHDRAW from furnace
                    if (quantityPromptCampfireSlot == 1) { // Withdraw fuel
                        int availCoal = activeFurnace->fuel / 8;
                        int availWood = (activeFurnace->fuel % 8) / 4;
                        if (availCoal > 0) {
                            int coalToTake = std::clamp(amt, 1, availCoal);
                            const sf::Texture* cTex = getItemTexture ? getItemTexture(ITEM_COAL) : nullptr;
                            if (cTex) {
                                inventory.addItem(ITEM_COAL, *cTex, coalToTake);
                                activeFurnace->fuel -= coalToTake * 8;
                                inventory.showBanner("Withdrew " + std::to_string(coalToTake) + "x Coal from furnace fuel.");
                            }
                        } else if (availWood > 0) {
                            int woodToTake = std::clamp(amt, 1, availWood);
                            const sf::Texture* wTex = getItemTexture ? getItemTexture(ITEM_WOOD) : nullptr;
                            if (wTex) {
                                inventory.addItem(ITEM_WOOD, *wTex, woodToTake);
                                activeFurnace->fuel -= woodToTake * 4;
                                inventory.showBanner("Withdrew " + std::to_string(woodToTake) + "x Wood from furnace fuel.");
                            }
                        }
                        if (activeFurnace->fuel < 0) activeFurnace->fuel = 0;
                    } else if (quantityPromptCampfireSlot == 2) { // Withdraw input
                        int actualAmt = std::clamp(amt, 1, activeFurnace->inputCount);
                        if (actualAmt > 0 && activeFurnace->inputItem != 0) {
                            const sf::Texture* rTex = getItemTexture ? getItemTexture(activeFurnace->inputItem) : nullptr;
                            if (rTex) {
                                inventory.addItem(activeFurnace->inputItem, *rTex, actualAmt);
                            }
                            activeFurnace->inputCount -= actualAmt;
                            if (activeFurnace->inputCount <= 0) {
                                activeFurnace->inputCount = 0;
                                activeFurnace->inputItem = 0;
                                activeFurnace->timeRemaining = 0.f;
                            }
                            inventory.showBanner("Withdrew " + std::to_string(actualAmt) + "x " + Inventory::getItemName(activeFurnace->inputItem) + ".");
                        }
                    } else if (quantityPromptCampfireSlot == 3) { // Retrieve smelted output
                        int actualAmt = std::clamp(amt, 1, activeFurnace->outputCount);
                        if (actualAmt > 0 && activeFurnace->outputItem != 0) {
                            const sf::Texture* oTex = getItemTexture ? getItemTexture(activeFurnace->outputItem) : nullptr;
                            if (oTex) {
                                inventory.addItem(activeFurnace->outputItem, *oTex, actualAmt);
                            }
                            activeFurnace->outputCount -= actualAmt;
                            if (activeFurnace->outputCount <= 0) {
                                activeFurnace->outputCount = 0;
                                activeFurnace->outputItem = 0;
                            }
                            inventory.showBanner("Retrieved " + std::to_string(actualAmt) + "x " + Inventory::getItemName(activeFurnace->outputItem) + "!");
                        }
                    }
                    if (syncSaveCallback) syncSaveCallback();
                }
            }
        } else if (quantityPromptIsP2P) {
            if (quantityPromptTargetSlot >= 0 && quantityPromptTargetSlot < 4 &&
                quantityPromptSourceSlot >= 0 && quantityPromptSourceSlot < 20 &&
                inventory.hasItem(quantityPromptSourceSlot))
            {
                int available = inventory.getItemCount(quantityPromptSourceSlot);
                amt = std::clamp(amt, 1, available);
                myP2PSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                myP2PSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                myP2PSlots[quantityPromptTargetSlot].count = amt;
                myP2PSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                p2pOfferDirty = true;
                p2pMyReady = false;
            }
        } else if (quantityPromptIsChest) {
            if (activeChest) {
                if (quantityPromptSourceSlot == -1) {
                    // RETRIEVE: move items from chest slot to inventory
                    if (quantityPromptChestSlot >= 0 && quantityPromptChestSlot < CHEST_SLOT_COUNT) {
                        ChestSlot& cs = activeChest->chestSlots[quantityPromptChestSlot];
                        int actualAmt = std::min(amt, cs.count);
                        if (actualAmt > 0 && cs.tex) {
                            inventory.addItem(cs.id, *cs.tex, actualAmt);
                            cs.count -= actualAmt;
                            if (cs.count <= 0) cs = ChestSlot{};
                            inventory.showBanner("Retrieved " + std::to_string(actualAmt) + "x " + Inventory::getItemName(cs.id) + " from chest.");
                        }
                    }
                } else if (quantityPromptSourceSlot >= 0 && quantityPromptSourceSlot < 20) {
                    // STORE: move items from inventory slot to chest slot (supports stacking)
                    int actualAmt = std::min(amt, inventory.getItemCount(quantityPromptSourceSlot));
                    if (actualAmt > 0) {
                        int itemID = inventory.getItemID(quantityPromptSourceSlot);
                        const sf::Texture* itemTex = inventory.getItemTexture(quantityPromptSourceSlot);
                        if (!itemTex && getItemTexture) itemTex = getItemTexture(itemID);

                        int targetSlot = quantityPromptChestSlot;
                        if (targetSlot < 0 || targetSlot >= CHEST_SLOT_COUNT ||
                            (activeChest->chestSlots[targetSlot].id != 0 && activeChest->chestSlots[targetSlot].id != itemID)) {
                            targetSlot = -1;
                            for (int j = 0; j < CHEST_SLOT_COUNT; j++) {
                                if (activeChest->chestSlots[j].id == itemID && activeChest->chestSlots[j].count > 0) {
                                    targetSlot = j;
                                    break;
                                }
                            }
                            if (targetSlot == -1) {
                                for (int j = 0; j < CHEST_SLOT_COUNT; j++) {
                                    if (activeChest->chestSlots[j].id == 0 || activeChest->chestSlots[j].count == 0) {
                                        targetSlot = j;
                                        break;
                                    }
                                }
                            }
                        }

                        if (targetSlot >= 0 && targetSlot < CHEST_SLOT_COUNT) {
                            ChestSlot& cs = activeChest->chestSlots[targetSlot];
                            if (cs.id == itemID && cs.count > 0) {
                                cs.count += actualAmt;
                            } else {
                                cs.id    = itemID;
                                cs.tex   = itemTex;
                                cs.count = actualAmt;
                            }
                            for (int c = 0; c < actualAmt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                            inventory.showBanner("Stored " + std::to_string(actualAmt) + "x " + Inventory::getItemName(itemID) + " into chest.");
                        } else {
                            inventory.showBanner("Chest is full!");
                        }
                    }
                }
                if (syncSaveCallback) syncSaveCallback();
            }
        } else {
            // NPC Trade slot assignment
            if (quantityPromptTargetSlot >= 0 && quantityPromptTargetSlot < 4 &&
                quantityPromptSourceSlot >= 0 && quantityPromptSourceSlot < 20 &&
                inventory.hasItem(quantityPromptSourceSlot))
            {
                int available = inventory.getItemCount(quantityPromptSourceSlot);
                amt = std::clamp(amt, 1, available);
                int itemID = inventory.getItemID(quantityPromptSourceSlot);
                const sf::Texture* itemTex = inventory.getItemTexture(quantityPromptSourceSlot);

                playerTradeSlots[quantityPromptTargetSlot].id = itemID;
                playerTradeSlots[quantityPromptTargetSlot].tex = itemTex;
                playerTradeSlots[quantityPromptTargetSlot].count = amt;
                playerTradeSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                for (int c = 0; c < amt; c++) {
                    inventory.consumeItem(quantityPromptSourceSlot);
                }
                isSellingToNPC = true;

                npcTradeGold = 0;
                for (int j = 0; j < 4; j++) {
                    if (playerTradeSlots[j].tex != nullptr && playerTradeSlots[j].count > 0) {
                        float demandMul = 0.75f;
                        for (auto& mkt : stockMarket) {
                            if (mkt.id == playerTradeSlots[j].id) {
                                demandMul = (mkt.trend == "UP") ? 0.90f : (mkt.trend == "DOWN") ? 0.60f : 0.75f;
                                break;
                            }
                        }
                        npcTradeGold += (int)(getMarketItemPrice(playerTradeSlots[j].id, stockMarket) * demandMul) * playerTradeSlots[j].count;
                    }
                }
                npcGoldInput = std::to_string(npcTradeGold);
                bargainAccepted = false;
            }
        }

        quantityPromptOpen = false;
        quantityPromptIsChest = false;
        quantityPromptIsCampfire = false;
        quantityInput.clear();
        quantityInput.setFocus(false);
        return true;
    };

    const float boxW = 480.f;
    const float boxH = 260.f;
    const float boxX = (1920.f - boxW) / 2.f;
    const float boxY = (1080.f - boxH) / 2.f;

    const float minusW = 44.f, minusH = 38.f;
    const float minusX = boxX + 30.f, inputY = boxY + 84.f;
    sf::FloatRect minusRect({ minusX, inputY }, { minusW, minusH });

    const float inputW = boxW - 60.f - (minusW * 2.f) - 16.f;
    const float inputX = minusX + minusW + 8.f;
    sf::FloatRect inputRect({ inputX, inputY }, { inputW, minusH });

    const float plusX = inputX + inputW + 8.f;
    sf::FloatRect plusRect({ plusX, inputY }, { minusW, minusH });

    const float preY = boxY + 132.f;
    const float preW = (boxW - 60.f - 16.f) / 3.f;
    const float preH = 32.f;
    sf::FloatRect btn1Rect({ boxX + 30.f, preY }, { preW, preH });
    sf::FloatRect btnHRect({ boxX + 30.f + preW + 8.f, preY }, { preW, preH });
    sf::FloatRect btnARect({ boxX + 30.f + (preW + 8.f) * 2.f, preY }, { preW, preH });

    const float actY = boxY + 180.f;
    const float actW = (boxW - 60.f - 16.f) / 2.f;
    const float actH = 42.f;
    sf::FloatRect confirmBtnRect({ boxX + 30.f, actY }, { actW, actH });
    sf::FloatRect cancelBtnRect({ boxX + 30.f + actW + 16.f, actY }, { actW, actH });

    quantityInput.processEvent(event, window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));

    if (auto* textEvent = event.getIf<sf::Event::TextEntered>()) {
        if (textEvent->unicode == 13) { // Enter pressed
            return applyQuantityConfirm();
        }
        if (textEvent->unicode >= '0' && textEvent->unicode <= '9') {
            std::string s = quantityInput.getText();
            try {
                int val = std::stoi(s);
                if (quantityPromptMax > 0 && val > quantityPromptMax) {
                    quantityInput.setText(std::to_string(quantityPromptMax));
                }
            } catch (...) {}
        }
        // Consume all other text entries so they do not leak to chat or other inputs
        return true;
    }

    if (auto* keyEvent = event.getIf<sf::Event::KeyPressed>()) {
        if (keyEvent->code == sf::Keyboard::Key::Escape ||
            keyEvent->scancode == sf::Keyboard::Scancode::Escape) {
            quantityPromptOpen = false;
            quantityPromptIsChest = false;
            quantityPromptIsCampfire = false;
            quantityInput.clear();
            quantityInput.setFocus(false);
            return true;
        }
        if (keyEvent->code == sf::Keyboard::Key::Enter ||
            keyEvent->scancode == sf::Keyboard::Scancode::Enter) {
            return applyQuantityConfirm();
        }
        if (keyEvent->code == sf::Keyboard::Key::Up ||
            keyEvent->scancode == sf::Keyboard::Scancode::Up) {
            int cur = 1;
            try { cur = std::stoi(quantityInput.getText()); } catch (...) { cur = 1; }
            cur = std::min(quantityPromptMax > 0 ? quantityPromptMax : 99, cur + 1);
            quantityInput.setText(std::to_string(cur));
            return true;
        }
        if (keyEvent->code == sf::Keyboard::Key::Down ||
            keyEvent->scancode == sf::Keyboard::Scancode::Down) {
            int cur = 1;
            try { cur = std::stoi(quantityInput.getText()); } catch (...) { cur = 1; }
            cur = std::max(1, cur - 1);
            quantityInput.setText(std::to_string(cur));
            return true;
        }
        // Consume keys while modal prompt is open
        return true;
    }

    if (auto* mouseButtonEvent = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouseButtonEvent->button == sf::Mouse::Button::Left) {
            sf::Vector2f uiPos = window.mapPixelToCoords(mouseButtonEvent->position, GetUIView(&window));

            // Decrement button [-]
            if (minusRect.contains(uiPos)) {
                int cur = 1;
                try { cur = std::stoi(quantityInput.getText()); } catch (...) { cur = 1; }
                cur = std::max(1, cur - 1);
                quantityInput.setText(std::to_string(cur));
                return true;
            }

            // Increment button [+]
            if (plusRect.contains(uiPos)) {
                int cur = 1;
                try { cur = std::stoi(quantityInput.getText()); } catch (...) { cur = 1; }
                cur = std::min(quantityPromptMax > 0 ? quantityPromptMax : 99, cur + 1);
                quantityInput.setText(std::to_string(cur));
                return true;
            }

            // Quick preset [ 1 ]
            if (btn1Rect.contains(uiPos)) {
                quantityInput.setText("1");
                return true;
            }

            // Quick preset [ Half ]
            if (btnHRect.contains(uiPos)) {
                int halfVal = std::max(1, quantityPromptMax / 2);
                quantityInput.setText(std::to_string(halfVal));
                return true;
            }

            // Quick preset [ All ]
            if (btnARect.contains(uiPos)) {
                quantityInput.setText(std::to_string(quantityPromptMax));
                return true;
            }

            // Confirm button
            if (confirmBtnRect.contains(uiPos)) {
                return applyQuantityConfirm();
            }

            // Cancel button
            if (cancelBtnRect.contains(uiPos)) {
                quantityPromptOpen = false;
                quantityPromptIsChest = false;
                quantityPromptIsCampfire = false;
                quantityInput.clear();
                quantityInput.setFocus(false);
                return true;
            }

            // Click outside dialog panel cancels cleanly
            sf::FloatRect panelRect({ boxX, boxY }, { boxW, boxH });
            if (!panelRect.contains(uiPos)) {
                quantityPromptOpen = false;
                quantityPromptIsChest = false;
                quantityPromptIsCampfire = false;
                quantityInput.clear();
                quantityInput.setFocus(false);
                return true;
            }
            return true;
        }
        return true;
    }

    if (event.is<sf::Event::MouseWheelScrolled>()) {
        return true;
    }

    return false;
}

bool handleCampfireMenuClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    std::vector<PlacedCampfire>& campfires,
    Inventory& inventory,
    const sf::Texture& woodTex,
    const sf::Texture& saplingTex,
    const sf::Texture& cookedChickenTex,
    const sf::Texture& cookedPigTex,
    const sf::Texture& cookedSheepTex,
    bool& quantityPromptOpen,
    int& quantityPromptMax,
    int& quantityPromptTargetSlot,
    int& quantityPromptSourceSlot,
    bool& quantityPromptIsP2P,
    bool& quantityPromptIsCampfire,
    int& quantityPromptCampfireSlot,
    TextInput& quantityInput,
    bool& saplingPlacementMode,
    std::function<void()> syncSaveCallback,
    std::function<const sf::Texture* (int id)> getItemTexture
) {
    for (auto& fire : campfires) {
        if (fire.menuOpen) {
            sf::Vector2f menuPos(
                (float)width / 2.f - 250.f,
                (float)height / 2.f - 150.f
            );

            sf::FloatRect menuRect(menuPos, { 500.f, 300.f });
            sf::FloatRect closeBtn(
                { menuPos.x + 440.f, menuPos.y + 10.f },
                { 40.f, 40.f }
            );

            // 1) Clicking inventory at bottom of screen selects that slot without closing campfire menu
            int invSlot = inventory.getSlotAt(uiPos);
            if (invSlot != -1) {
                inventory.setSelectedSlot(invSlot);
                if (inventory.hasItem(invSlot)) {
                    inventory.showBanner("Selected: " + Inventory::getItemName(inventory.getItemID(invSlot)));
                }
                return true;
            }

            // 2) Close menu if clicking close button or clicking outside menu
            if (closeBtn.contains(uiPos) || !menuRect.contains(uiPos)) {
                fire.menuOpen = false;
                return true;
            }

            sf::FloatRect fuelBtn(
                { menuPos.x + 60.f, menuPos.y + 110.f },
                { 80.f, 80.f }
            );
            sf::FloatRect cookBtn(
                { menuPos.x + 210.f, menuPos.y + 110.f },
                { 80.f, 80.f }
            );
            sf::FloatRect takeBtn(
                { menuPos.x + 290.f, menuPos.y + 100.f },
                { 160.f, 100.f }
            );

            // 3) Fuel button clicked
            if (fuelBtn.contains(uiPos)) {
                int sel = inventory.getSelectedSlot();
                bool selIsFuel = (sel != -1 && inventory.hasItem(sel) &&
                    (inventory.getItemID(sel) == ITEM_WOOD || inventory.getItemID(sel) == ITEM_SAPLING));

                if (selIsFuel) {
                    // Holding fuel: ADD fuel to campfire
                    int itemID = inventory.getItemID(sel);
                    int totalQty = inventory.getItemCount(sel);
                    if (totalQty > 1) {
                        quantityPromptOpen = true;
                        quantityPromptMax = totalQty;
                        quantityPromptTargetSlot = 0;
                        quantityPromptSourceSlot = sel;
                        quantityPromptIsP2P = false;
                        quantityPromptIsCampfire = true;
                        quantityPromptCampfireSlot = 1;
                        quantityInput.clear();
                        quantityInput.setText(std::to_string(totalQty));
                        quantityInput.setFocus(true);
                    } else {
                        if (itemID == ITEM_WOOD) {
                            fire.fuel += 4;
                        } else {
                            fire.fuel += 1;
                            saplingPlacementMode = false;
                        }
                        inventory.consumeItem(sel);
                        if (fire.inputCount > 0 && fire.timeRemaining <= 0.f) {
                            fire.timeRemaining = 2.0f;
                        }
                        if (syncSaveCallback) syncSaveCallback();
                        inventory.showBanner("Added fuel (+ " + std::string(itemID == ITEM_WOOD ? "4" : "1") + " fuel)!");
                    }
                } else {
                    // Not holding fuel: WITHDRAW fuel from campfire
                    int availWood = fire.fuel / 4;
                    if (availWood > 0) {
                        if (availWood > 1) {
                            quantityPromptOpen = true;
                            quantityPromptMax = availWood;
                            quantityPromptTargetSlot = 0;
                            quantityPromptSourceSlot = -1; // RETRIEVE mode
                            quantityPromptIsP2P = false;
                            quantityPromptIsCampfire = true;
                            quantityPromptCampfireSlot = 1;
                            quantityInput.clear();
                            quantityInput.setText(std::to_string(availWood));
                            quantityInput.setFocus(true);
                        } else {
                            inventory.addItem(ITEM_WOOD, woodTex, 1);
                            fire.fuel -= 4;
                            if (syncSaveCallback) syncSaveCallback();
                            inventory.showBanner("Withdrew 1x Wood from campfire fuel.");
                        }
                    } else if (fire.fuel > 0) {
                        // Leftover saplings
                        inventory.addItem(ITEM_SAPLING, saplingTex, fire.fuel);
                        inventory.showBanner("Withdrew " + std::to_string(fire.fuel) + "x Sapling from campfire fuel.");
                        fire.fuel = 0;
                        if (syncSaveCallback) syncSaveCallback();
                    } else {
                        inventory.showBanner("No fuel in campfire to withdraw! Select Wood / Sapling in bag to add fuel.");
                    }
                }
                return true;
            }
            // 4) Cook/Food button clicked (Campfires can ONLY cook raw meats)
            else if (cookBtn.contains(uiPos)) {
                int sel = inventory.getSelectedSlot();
                if (sel != -1 && inventory.hasItem(sel)) {
                    int selID = inventory.getItemID(sel);
                    if (selID == ITEM_COPPER_ORE || selID == ITEM_IRON_ORE || selID == ITEM_GOLD_ORE) {
                        inventory.showBanner("Campfires can only cook food! Use a furnace to smelt ores.");
                        return true;
                    }
                }

                auto isRawMeat = [](int id) {
                    return id == ITEM_CHICKEN_MEAT || id == ITEM_PIG_MEAT || id == ITEM_SHEEP_MEAT;
                };

                bool selIsMeat = (sel != -1 && inventory.hasItem(sel) && isRawMeat(inventory.getItemID(sel)));

                if (selIsMeat) {
                    int selID = inventory.getItemID(sel);
                    if (fire.inputCount > 0 && fire.inputItem != selID) {
                        inventory.showBanner("Campfire has " + Inventory::getItemName(fire.inputItem) + "! Withdraw it first.");
                        return true;
                    }
                    int totalQty = inventory.getItemCount(sel);
                    if (totalQty > 1) {
                        quantityPromptOpen = true;
                        quantityPromptMax = totalQty;
                        quantityPromptTargetSlot = 0;
                        quantityPromptSourceSlot = sel;
                        quantityPromptIsP2P = false;
                        quantityPromptIsCampfire = true;
                        quantityPromptCampfireSlot = 2;
                        quantityInput.clear();
                        quantityInput.setText(std::to_string(totalQty));
                        quantityInput.setFocus(true);
                    } else {
                        fire.inputItem = selID;
                        fire.inputCount += 1;
                        if (fire.timeRemaining <= 0.f) {
                            fire.timeRemaining = 2.0f;
                        }
                        inventory.consumeItem(sel);
                        if (syncSaveCallback) syncSaveCallback();
                        inventory.showBanner("Added 1x " + Inventory::getItemName(selID) + " to campfire!");
                    }
                } else {
                    // Not holding meat: WITHDRAW input meat from campfire
                    if (fire.inputCount > 0 && fire.inputItem != 0) {
                        if (fire.inputCount > 1) {
                            quantityPromptOpen = true;
                            quantityPromptMax = fire.inputCount;
                            quantityPromptTargetSlot = 0;
                            quantityPromptSourceSlot = -1; // RETRIEVE mode
                            quantityPromptIsP2P = false;
                            quantityPromptIsCampfire = true;
                            quantityPromptCampfireSlot = 2;
                            quantityInput.clear();
                            quantityInput.setText(std::to_string(fire.inputCount));
                            quantityInput.setFocus(true);
                        } else {
                            const sf::Texture* rTex = (fire.inputItem == ITEM_CHICKEN_MEAT) ? ChickenMeat::getTexture() : 
                                                      (fire.inputItem == ITEM_PIG_MEAT) ? PigMeat::getTexture() : 
                                                      (fire.inputItem == ITEM_SHEEP_MEAT) ? SheepMeat::getTexture() : nullptr;
                            if (!rTex && getItemTexture) rTex = getItemTexture(fire.inputItem);
                            inventory.addItem(fire.inputItem, rTex ? *rTex : woodTex, 1);
                            inventory.showBanner("Withdrew 1x " + Inventory::getItemName(fire.inputItem) + ".");
                            fire.inputCount = 0;
                            fire.inputItem = 0;
                            fire.timeRemaining = 0.f;
                            if (syncSaveCallback) syncSaveCallback();
                        }
                    } else {
                        inventory.showBanner("Campfire input is empty! Select raw meat in bag to cook.");
                    }
                }
                return true;
            }
            // 5) Retrieve output button clicked
            else if (takeBtn.contains(uiPos)) {
                if (fire.outputCount > 0 && fire.outputItem != 0) {
                    bool hasRoom = false;
                    for (int i = 0; i < 20; ++i) {
                        if (!inventory.hasItem(i) || (inventory.getItemID(i) == fire.outputItem && inventory.getItemCount(i) < 99)) {
                            hasRoom = true;
                            break;
                        }
                    }
                    if (!hasRoom) {
                        inventory.showBanner("Inventory is full!");
                        return true;
                    }

                    if (fire.outputCount > 1) {
                        quantityPromptOpen = true;
                        quantityPromptMax = fire.outputCount;
                        quantityPromptTargetSlot = 0;
                        quantityPromptSourceSlot = -1; // RETRIEVE mode
                        quantityPromptIsP2P = false;
                        quantityPromptIsCampfire = true;
                        quantityPromptCampfireSlot = 3; // output slot
                        quantityInput.clear();
                        quantityInput.setText(std::to_string(fire.outputCount));
                        quantityInput.setFocus(true);
                    } else {
                        const sf::Texture* oTex = nullptr;
                        if (fire.outputItem == ITEM_COOKED_CHICKEN) oTex = &cookedChickenTex;
                        else if (fire.outputItem == ITEM_COOKED_PIG) oTex = &cookedPigTex;
                        else if (fire.outputItem == ITEM_COOKED_SHEEP) oTex = &cookedSheepTex;
                        if (!oTex && getItemTexture) oTex = getItemTexture(fire.outputItem);

                        int item = fire.outputItem;
                        inventory.addItem(item, oTex ? *oTex : cookedChickenTex, 1);
                        inventory.showBanner("Retrieved 1x " + Inventory::getItemName(item) + "!");
                        fire.outputCount = 0;
                        fire.outputItem = 0;
                        if (syncSaveCallback) syncSaveCallback();
                    }
                } else {
                    inventory.showBanner("No cooked food ready to retrieve yet.");
                }
                return true;
            }

            return true; // Consume any other click inside menu window
        }
    }
    return false;
}

bool handleFurnaceMenuClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    std::vector<PlacedStructure>& structures,
    Inventory& inventory,
    const sf::Texture& woodTex,
    const sf::Texture& saplingTex,
    const sf::Texture& cookedChickenTex,
    const sf::Texture& cookedPigTex,
    const sf::Texture& cookedSheepTex,
    bool& quantityPromptOpen,
    int& quantityPromptMax,
    int& quantityPromptTargetSlot,
    int& quantityPromptSourceSlot,
    bool& quantityPromptIsP2P,
    bool& quantityPromptIsCampfire,
    int& quantityPromptCampfireSlot,
    TextInput& quantityInput,
    bool& saplingPlacementMode,
    std::function<void()> syncSaveCallback,
    std::function<const sf::Texture* (int id)> getItemTexture
) {
    for (auto& s : structures) {
        if (s.id == ITEM_FURNACE && s.menuOpen) {
            sf::Vector2f menuPos(
                (float)width / 2.f - 250.f,
                (float)height / 2.f - 150.f
            );
            
            sf::FloatRect menuRect(menuPos, { 500.f, 300.f });
            sf::FloatRect closeBtn(
                { menuPos.x + 440.f, menuPos.y + 10.f },
                { 40.f, 40.f }
            );

            // 1) Clicking inventory at bottom of screen selects that slot without closing furnace menu
            int invSlot = inventory.getSlotAt(uiPos);
            if (invSlot != -1) {
                inventory.setSelectedSlot(invSlot);
                if (inventory.hasItem(invSlot)) {
                    inventory.showBanner("Selected: " + Inventory::getItemName(inventory.getItemID(invSlot)));
                }
                return true;
            }

            // 2) Close menu if clicking close button or clicking outside menu
            if (closeBtn.contains(uiPos) || !menuRect.contains(uiPos)) {
                s.menuOpen = false;
                return true;
            }

            sf::FloatRect fuelBtn(
                { menuPos.x + 60.f, menuPos.y + 110.f },
                { 80.f, 80.f }
            );
            sf::FloatRect cookBtn(
                { menuPos.x + 210.f, menuPos.y + 110.f },
                { 80.f, 80.f }
            );
            sf::FloatRect takeBtn(
                { menuPos.x + 290.f, menuPos.y + 100.f },
                { 160.f, 100.f }
            );

            // 3) Fuel button clicked
            if (fuelBtn.contains(uiPos)) {
                int sel = inventory.getSelectedSlot();
                bool selIsFuel = (sel != -1 && inventory.hasItem(sel) &&
                    (inventory.getItemID(sel) == ITEM_COAL || inventory.getItemID(sel) == ITEM_WOOD || inventory.getItemID(sel) == ITEM_SAPLING));

                if (selIsFuel) {
                    int itemID = inventory.getItemID(sel);
                    int totalQty = inventory.getItemCount(sel);
                    if (totalQty > 1) {
                        quantityPromptOpen = true;
                        quantityPromptMax = totalQty;
                        quantityPromptTargetSlot = 0;
                        quantityPromptSourceSlot = sel;
                        quantityPromptIsP2P = false;
                        quantityPromptIsCampfire = true;
                        quantityPromptCampfireSlot = 1;
                        quantityInput.clear();
                        quantityInput.setText(std::to_string(totalQty));
                        quantityInput.setFocus(true);
                    } else {
                        if (itemID == ITEM_COAL) {
                            s.fuel += 8;
                        } else if (itemID == ITEM_WOOD) {
                            s.fuel += 4;
                        } else {
                            s.fuel += 1;
                            saplingPlacementMode = false;
                        }
                        inventory.consumeItem(sel);
                        if (s.inputCount > 0 && s.timeRemaining <= 0.f) {
                            s.timeRemaining = 2.0f;
                        }
                        if (syncSaveCallback) syncSaveCallback();
                        inventory.showBanner("Added fuel to furnace!");
                    }
                } else {
                    // WITHDRAW fuel from furnace
                    int coalReturn = s.fuel / 8;
                    int rem = s.fuel % 8;
                    int woodReturn = rem / 4;
                    if (coalReturn > 0) {
                        if (coalReturn > 1) {
                            quantityPromptOpen = true;
                            quantityPromptMax = coalReturn;
                            quantityPromptTargetSlot = 0;
                            quantityPromptSourceSlot = -1; // RETRIEVE mode
                            quantityPromptIsP2P = false;
                            quantityPromptIsCampfire = true;
                            quantityPromptCampfireSlot = 1;
                            quantityInput.clear();
                            quantityInput.setText(std::to_string(coalReturn));
                            quantityInput.setFocus(true);
                        } else {
                            const sf::Texture* cTex = getItemTexture ? getItemTexture(ITEM_COAL) : &woodTex;
                            inventory.addItem(ITEM_COAL, cTex ? *cTex : woodTex, 1);
                            s.fuel -= 8;
                            if (syncSaveCallback) syncSaveCallback();
                            inventory.showBanner("Withdrew 1x Coal from furnace fuel.");
                        }
                    } else if (woodReturn > 0) {
                        if (woodReturn > 1) {
                            quantityPromptOpen = true;
                            quantityPromptMax = woodReturn;
                            quantityPromptTargetSlot = 0;
                            quantityPromptSourceSlot = -1; // RETRIEVE mode
                            quantityPromptIsP2P = false;
                            quantityPromptIsCampfire = true;
                            quantityPromptCampfireSlot = 1;
                            quantityInput.clear();
                            quantityInput.setText(std::to_string(woodReturn));
                            quantityInput.setFocus(true);
                        } else {
                            inventory.addItem(ITEM_WOOD, woodTex, 1);
                            s.fuel -= 4;
                            if (syncSaveCallback) syncSaveCallback();
                            inventory.showBanner("Withdrew 1x Wood from furnace fuel.");
                        }
                    } else if (s.fuel > 0) {
                        inventory.addItem(ITEM_SAPLING, saplingTex, s.fuel);
                        inventory.showBanner("Withdrew " + std::to_string(s.fuel) + "x Sapling from furnace fuel.");
                        s.fuel = 0;
                        if (syncSaveCallback) syncSaveCallback();
                    } else {
                        inventory.showBanner("No fuel in furnace to withdraw! Select Coal / Wood / Sapling in bag to add fuel.");
                    }
                }
                return true;
            }
            // 4) Cook/Smelt button clicked (Furnace accepts ores AND raw food)
            else if (cookBtn.contains(uiPos)) {
                auto isFurnaceInput = [](int id) {
                    return id == ITEM_COPPER_ORE || id == ITEM_IRON_ORE || id == ITEM_GOLD_ORE ||
                           id == ITEM_CHICKEN_MEAT || id == ITEM_PIG_MEAT || id == ITEM_SHEEP_MEAT;
                };

                int sel = inventory.getSelectedSlot();
                bool selIsInput = (sel != -1 && inventory.hasItem(sel) && isFurnaceInput(inventory.getItemID(sel)));

                if (selIsInput) {
                    int selID = inventory.getItemID(sel);
                    if (s.inputCount > 0 && s.inputItem != selID) {
                        inventory.showBanner("Furnace has " + Inventory::getItemName(s.inputItem) + "! Withdraw it first.");
                        return true;
                    }
                    int totalQty = inventory.getItemCount(sel);
                    if (totalQty > 1) {
                        quantityPromptOpen = true;
                        quantityPromptMax = totalQty;
                        quantityPromptTargetSlot = 0;
                        quantityPromptSourceSlot = sel;
                        quantityPromptIsP2P = false;
                        quantityPromptIsCampfire = true;
                        quantityPromptCampfireSlot = 2;
                        quantityInput.clear();
                        quantityInput.setText(std::to_string(totalQty));
                        quantityInput.setFocus(true);
                    } else {
                        s.inputItem = selID;
                        s.inputCount += 1;
                        if (s.timeRemaining <= 0.f) {
                            s.timeRemaining = 2.0f;
                        }
                        inventory.consumeItem(sel);
                        if (syncSaveCallback) syncSaveCallback();
                        inventory.showBanner("Added 1x " + Inventory::getItemName(selID) + " to furnace!");
                    }
                } else {
                    // WITHDRAW input from furnace
                    if (s.inputCount > 0 && s.inputItem != 0) {
                        if (s.inputCount > 1) {
                            quantityPromptOpen = true;
                            quantityPromptMax = s.inputCount;
                            quantityPromptTargetSlot = 0;
                            quantityPromptSourceSlot = -1; // RETRIEVE mode
                            quantityPromptIsP2P = false;
                            quantityPromptIsCampfire = true;
                            quantityPromptCampfireSlot = 2;
                            quantityInput.clear();
                            quantityInput.setText(std::to_string(s.inputCount));
                            quantityInput.setFocus(true);
                        } else {
                            const sf::Texture* rTex = getItemTexture ? getItemTexture(s.inputItem) : nullptr;
                            inventory.addItem(s.inputItem, rTex ? *rTex : woodTex, 1);
                            inventory.showBanner("Withdrew 1x " + Inventory::getItemName(s.inputItem) + ".");
                            s.inputCount = 0;
                            s.inputItem = 0;
                            s.timeRemaining = 0.f;
                            if (syncSaveCallback) syncSaveCallback();
                        }
                    } else {
                        inventory.showBanner("Furnace input is empty! Select ores or raw food in bag to smelt/cook.");
                    }
                }
                return true;
            }
            // 5) Retrieve output button clicked
            else if (takeBtn.contains(uiPos)) {
                if (s.outputCount > 0 && s.outputItem != 0) {
                    bool hasRoom = false;
                    for (int i = 0; i < 20; ++i) {
                        if (!inventory.hasItem(i) || (inventory.getItemID(i) == s.outputItem && inventory.getItemCount(i) < 99)) {
                            hasRoom = true;
                            break;
                        }
                    }
                    if (!hasRoom) {
                        inventory.showBanner("Inventory is full!");
                        return true;
                    }

                    if (s.outputCount > 1) {
                        quantityPromptOpen = true;
                        quantityPromptMax = s.outputCount;
                        quantityPromptTargetSlot = 0;
                        quantityPromptSourceSlot = -1; // RETRIEVE mode
                        quantityPromptIsP2P = false;
                        quantityPromptIsCampfire = true;
                        quantityPromptCampfireSlot = 3; // output slot
                        quantityInput.clear();
                        quantityInput.setText(std::to_string(s.outputCount));
                        quantityInput.setFocus(true);
                    } else {
                        const sf::Texture* oTex = getItemTexture ? getItemTexture(s.outputItem) : nullptr;
                        int item = s.outputItem;
                        inventory.addItem(item, oTex ? *oTex : cookedChickenTex, 1);
                        inventory.showBanner("Retrieved 1x " + Inventory::getItemName(item) + "!");
                        s.outputCount = 0;
                        s.outputItem = 0;
                        if (syncSaveCallback) syncSaveCallback();
                    }
                } else {
                    inventory.showBanner("Nothing ready to retrieve yet.");
                }
                return true;
            }

            return true; // Consume any other click inside menu window
        }
    }
    return false;
}

bool handleChatCommandsAndInput(
    const std::string& msg,
    const std::string& username,
    AuthManager& authManager,
    const std::vector<OnlinePlayer>& otherPlayers,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    bool tutorialActive,
    int tutorialStep,
    bool& tutChatted,
    std::function<bool(const std::string& cmd, const std::string& args, std::string& feedback)> customCommandHandler,
    bool* adminPromptOpen,
    bool* adminPromptIsBan,
    std::string* adminPromptTarget,
    std::string* adminPromptDuration,
    int* adminPromptMuteMinutes,
    TextInput* adminReasonInput,
    sf::Clock* adminPromptClock
) {
    if (msg.empty()) return false;

    // Fulfill tutorial quest step 6 ("COMMUNICATION - Say 'Hello' in chat") immediately upon sending chat
    if (tutorialActive && tutorialStep == 6) {
        tutChatted = true;
    }

    auto isStaffRole = [](const std::string& r) {
        std::string s = r;
        for (char& c : s) c = (char)::tolower((unsigned char)c);
        return (s == "admin" || s == "administrator" || s == "moderator" ||
                s.find("admin") != std::string::npos || s.find("moderator") != std::string::npos);
    };

    if (msg == "/ban" || msg.rfind("/ban ", 0) == 0) {
        std::string rest = (msg.length() > 5) ? msg.substr(5) : "";
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.erase(rest.begin());
        while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t')) rest.pop_back();

        if (rest.empty()) {
            latestPlayerMsg = "Usage: /ban <username> [3|7|14|30|perm]";
            bubbleTimer = 6.f;
            return true;
        }

        std::string executorRole = authManager.getUserRole(username.c_str());
        if (!isStaffRole(executorRole) && !AuthManager::isTargetedAdmin(username, "")) {
            latestPlayerMsg = "Error: Only moderators or administrators can ban.";
            bubbleTimer = 5.f;
            return true;
        }

        std::string target = rest;
        std::string duration = "30";
        std::string initialReason = "";

        size_t lastSpace = rest.rfind(' ');
        if (lastSpace != std::string::npos) {
            std::string lastWord = rest.substr(lastSpace + 1);
            std::string lowerLast = lastWord;
            for (char& c : lowerLast) c = (char)::tolower((unsigned char)c);
            if (lowerLast == "3" || lowerLast == "7" || lowerLast == "14" || lowerLast == "30" || lowerLast == "perm" || lowerLast == "permanent") {
                target = rest.substr(0, lastSpace);
                duration = lowerLast;
            } else {
                size_t firstSpace = rest.find(' ');
                std::string firstToken = rest.substr(0, firstSpace);
                std::string rem = rest.substr(firstSpace + 1);
                size_t secondSpace = rem.find(' ');
                std::string secondToken = (secondSpace != std::string::npos) ? rem.substr(0, secondSpace) : rem;
                std::string lowerSecond = secondToken;
                for (char& c : lowerSecond) c = (char)::tolower((unsigned char)c);
                if (lowerSecond == "3" || lowerSecond == "7" || lowerSecond == "14" || lowerSecond == "30" || lowerSecond == "perm" || lowerSecond == "permanent") {
                    target = firstToken;
                    duration = lowerSecond;
                    if (secondSpace != std::string::npos) {
                        initialReason = rem.substr(secondSpace + 1);
                    }
                }
            }
        }

        while (target.size() >= 2 && ((target.front() == '"' && target.back() == '"') || (target.front() == '\'' && target.back() == '\''))) {
            target = target.substr(1, target.size() - 2);
        }
        while (!target.empty() && (target.front() == ' ' || target.front() == '\t')) target.erase(target.begin());
        while (!target.empty() && (target.back() == ' ' || target.back() == '\t')) target.pop_back();

        if (adminPromptOpen && adminPromptIsBan && adminPromptTarget && adminPromptDuration && adminReasonInput) {
            *adminPromptOpen = true;
            *adminPromptIsBan = true;
            *adminPromptTarget = target;
            *adminPromptDuration = duration;
            adminReasonInput->clear();
            if (!initialReason.empty()) {
                adminReasonInput->setText(initialReason);
            }
            adminReasonInput->setFocus(true);
            if (adminPromptClock) adminPromptClock->restart();
            latestPlayerMsg = "Admin Action: Write ban reason in prompt box.";
            bubbleTimer = 4.f;
        } else {
            std::string feedback;
            authManager.banUser(target, duration, feedback, initialReason, username);
            latestPlayerMsg = feedback;
            bubbleTimer = 5.f;
        }
        return true;
    }
    else if (msg == "/unban" || msg.rfind("/unban ", 0) == 0) {
        std::string target = (msg.length() > 7) ? msg.substr(7) : "";
        while (!target.empty() && (target.front() == ' ' || target.front() == '\t')) target.erase(target.begin());
        while (!target.empty() && (target.back() == ' ' || target.back() == '\t')) target.pop_back();
        while (target.size() >= 2 && ((target.front() == '"' && target.back() == '"') || (target.front() == '\'' && target.back() == '\''))) {
            target = target.substr(1, target.size() - 2);
        }

        std::string executorRole = authManager.getUserRole(username.c_str());
        if (!isStaffRole(executorRole) && !AuthManager::isTargetedAdmin(username, "")) {
            latestPlayerMsg = "Error: Only moderators or administrators can unban.";
            bubbleTimer = 5.f;
        }
        else {
            std::string feedback;
            authManager.unbanUser(target, feedback);
            latestPlayerMsg = feedback;
            bubbleTimer = 5.f;
        }
    }
    else if (msg == "/mute" || msg.rfind("/mute ", 0) == 0) {
        std::string rest = (msg.length() > 6) ? msg.substr(6) : "";
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.erase(rest.begin());
        while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t')) rest.pop_back();

        if (rest.empty()) {
            latestPlayerMsg = "Usage: /mute <username> [minutes]";
            bubbleTimer = 6.f;
            return true;
        }

        std::string executorRole = authManager.getUserRole(username.c_str());
        if (!isStaffRole(executorRole) && !AuthManager::isTargetedAdmin(username, "")) {
            latestPlayerMsg = "Error: Only moderators or administrators can mute.";
            bubbleTimer = 5.f;
            return true;
        }

        std::string target = rest;
        int minutes = 60;
        std::string initialReason = "";

        size_t lastSpace = rest.rfind(' ');
        if (lastSpace != std::string::npos) {
            std::string lastWord = rest.substr(lastSpace + 1);
            try {
                size_t idx = 0;
                int m = std::stoi(lastWord, &idx);
                if (idx == lastWord.size() && m > 0) {
                    target = rest.substr(0, lastSpace);
                    minutes = m;
                }
            } catch (...) {}
        }

        size_t firstSpace = rest.find(' ');
        if (firstSpace != std::string::npos) {
            std::string firstToken = rest.substr(0, firstSpace);
            std::string rem = rest.substr(firstSpace + 1);
            size_t secondSpace = rem.find(' ');
            std::string secondToken = (secondSpace != std::string::npos) ? rem.substr(0, secondSpace) : rem;
            try {
                size_t idx = 0;
                int m = std::stoi(secondToken, &idx);
                if (idx == secondToken.size() && m > 0) {
                    target = firstToken;
                    minutes = m;
                    if (secondSpace != std::string::npos) {
                        initialReason = rem.substr(secondSpace + 1);
                    }
                }
            } catch (...) {}
        }

        while (target.size() >= 2 && ((target.front() == '"' && target.back() == '"') || (target.front() == '\'' && target.back() == '\''))) {
            target = target.substr(1, target.size() - 2);
        }
        while (!target.empty() && (target.front() == ' ' || target.front() == '\t')) target.erase(target.begin());
        while (!target.empty() && (target.back() == ' ' || target.back() == '\t')) target.pop_back();

        if (adminPromptOpen && adminPromptIsBan && adminPromptTarget && adminPromptMuteMinutes && adminReasonInput) {
            *adminPromptOpen = true;
            *adminPromptIsBan = false;
            *adminPromptTarget = target;
            *adminPromptMuteMinutes = minutes;
            adminReasonInput->clear();
            if (!initialReason.empty()) {
                adminReasonInput->setText(initialReason);
            }
            adminReasonInput->setFocus(true);
            if (adminPromptClock) adminPromptClock->restart();
            latestPlayerMsg = "Admin Action: Write mute reason in prompt box.";
            bubbleTimer = 4.f;
        } else {
            std::string feedback;
            authManager.muteUser(target, minutes, feedback, initialReason, username);
            latestPlayerMsg = feedback;
            bubbleTimer = 5.f;
        }
        return true;
    }
    else if (msg == "/unmute" || msg.rfind("/unmute ", 0) == 0) {
        std::string target = (msg.length() > 8) ? msg.substr(8) : "";
        while (!target.empty() && (target.front() == ' ' || target.front() == '\t')) target.erase(target.begin());
        while (!target.empty() && (target.back() == ' ' || target.back() == '\t')) target.pop_back();
        while (target.size() >= 2 && ((target.front() == '"' && target.back() == '"') || (target.front() == '\'' && target.back() == '\''))) {
            target = target.substr(1, target.size() - 2);
        }

        std::string executorRole = authManager.getUserRole(username.c_str());
        if (!isStaffRole(executorRole) && !AuthManager::isTargetedAdmin(username, "")) {
            latestPlayerMsg = "Error: Only moderators or administrators can unmute.";
            bubbleTimer = 5.f;
        }
        else {
            std::string feedback;
            authManager.unmuteUser(target, feedback);
            latestPlayerMsg = feedback;
            bubbleTimer = 5.f;
        }
    }
    else if (msg == "/makeadmin" || msg == "/makemoderator") {
        latestPlayerMsg = "Error: Promotion commands are disabled. Admin role is granted only to targeted accounts at registration.";
        bubbleTimer = 6.f;
    }
    else if (msg == "/disaster" || msg.rfind("/disaster ", 0) == 0) {
        std::string executorRole = authManager.getUserRole(username.c_str());
        bool isStaff = isStaffRole(executorRole) || AuthManager::isTargetedAdmin(username, "");

        if (!isStaff) {
            latestPlayerMsg = "Error: Only administrators or moderators can trigger natural disasters.";
            bubbleTimer = 5.f;
            return true;
        }

        std::string sub = (msg.length() > 9) ? msg.substr(10) : "";
        while (!sub.empty() && sub.front() == ' ') sub.erase(sub.begin());
        while (!sub.empty() && sub.back() == ' ') sub.pop_back();

        if (sub.empty() || sub == "help" || sub == "list") {
            latestPlayerMsg = "Disasters: /disaster diarrhea (outbreak), /disaster drought (scorching drought), /disaster stop (end disaster).";
            bubbleTimer = 8.f;
            return true;
        }

        if (customCommandHandler) {
            std::string feedback;
            customCommandHandler("disaster", sub, feedback);
            latestPlayerMsg = feedback;
            bubbleTimer = 6.f;
        }
        else {
            latestPlayerMsg = "Disaster command system is not ready.";
            bubbleTimer = 5.f;
        }
        return true;
    }
    else if (msg == "/sim" || msg == "/simulator") {
        std::string executorRole = authManager.getUserRole(username.c_str());
        std::string lowerRole = executorRole;
        for (char& c : lowerRole) c = (char)::tolower((unsigned char)c);
        bool isAdmin = (lowerRole == "admin" || lowerRole == "administrator" || AuthManager::isTargetedAdmin(username, ""));

        if (!isAdmin) {
            latestPlayerMsg = "Error: Access Denied. Simulator is restricted to authorized administrators.";
            bubbleTimer = 5.f;
            return true;
        }

        if (customCommandHandler) {
            std::string feedback;
            customCommandHandler("sim", "", feedback);
            latestPlayerMsg = feedback;
            bubbleTimer = 4.f;
        }
        return true;
    }
    else if (msg.find("/pull ") == 0) {
        std::string target = msg.substr(6);
        authManager.sendCommand(username.c_str(), target.c_str(), "pull");
        latestPlayerMsg = "Command Sent: /pull " + target;
        bubbleTimer = 3.f;
    }
    else if (msg == "/who") {
        std::string onlineList = "Players Online: ";
        for (size_t i = 0; i < otherPlayers.size(); ++i) {
            onlineList += otherPlayers[i].username;
            if (i < otherPlayers.size() - 1) onlineList += ", ";
        }
        if (otherPlayers.empty()) {
            onlineList += "Only you.";
        }
        latestPlayerMsg = onlineList;
        bubbleTimer = 5.f;
    }
    else if (msg == "/help" || msg == "/?") {
        std::string executorRole = authManager.getUserRole(username.c_str());
        if (isStaffRole(executorRole) || AuthManager::isTargetedAdmin(username, "")) {
            latestPlayerMsg = "Commands:\n/pull (name) - Pull player\n/who - See online players\n/ban (name) (3|7|14|30|perm)\n/unban (name)\n/mute (name) [minutes]\n/unmute (name)\n/disaster <diarrhea|drought|stop|list> - Natural Disaster\n/sim - Admin Quantity Simulator\n/help - Show commands";
        }
        else {
            latestPlayerMsg = "Commands:\n/pull (name) - Pull player\n/who - See online players\n/help - Show commands";
        }
        bubbleTimer = 10.f;
    }
    else {
        // Optimistically display speech bubble above player immediately
        latestPlayerMsg = msg;
        bubbleTimer = 5.0f;

        // Send to database asynchronously so main render loop never freezes or triggers "Not Responding"
        std::string sendUser = username;
        std::string sendMsg = msg;
        std::thread([sendUser, sendMsg]() {
            try {
                std::string chatErr;
                bool ok = AuthManager::getInstance().sendChatMessage(sendUser.c_str(), sendMsg.c_str(), chatErr);
                if (!ok && !chatErr.empty()) {
                    AuthManager::getInstance().setPendingChatNotification(chatErr);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[CHAT] Background send error: " << e.what() << "\n";
            }
            catch (...) {}
        }).detach();
    }
    return true;
}

#include "GameAssets.h"
#include "AuthManager.h"

bool handleWrenchMenuClicks(
    sf::Vector2f uiPos,
    bool isTargetNPC,
    sf::Vector2f wrenchMenuPos,
    bool& wrenchMenuOpen,
    std::string& wrenchTargetPlayer,
    bool& tradeMenuOpen,
    bool& craftMenuOpen,
    bool& stockPageOpen,
    bool& iapPageOpen,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    sf::Color& bargainColor,
    int& playerTradeGold,
    int& npcTradeGold,
    std::string& npcGoldInput,
    std::string& playerGoldInput,
    bool& tradeBoxesTicked,
    const QuestInfo& qInfo,
    const std::vector<DailyQuest>& availableQuests,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    int& p2pTradeID,
    const std::string& username,
    bool& p2pTradeOpen,
    bool& p2pAmInitiator,
    std::string& p2pPartner,
    bool& p2pMyReady,
    bool& p2pPartnerReady,
    std::string& p2pStatusMsg,
    TradeSlot myP2PSlots[4],
    int& myP2PGold,
    std::string& myP2PGoldInput,
    std::vector<P2PItemDisplay>& partnerItems,
    int& partnerGold,
    float& p2pPollTimer,
    AuthManager& authManager,
    const GameAssets& assets,
    std::function<int(int id)> getItemPrice
) {
    if (isTargetNPC) {
        sf::FloatRect tradeBound({ wrenchMenuPos.x, wrenchMenuPos.y }, { 160.f, 30.f });
        sf::FloatRect questBound({ wrenchMenuPos.x, wrenchMenuPos.y + 30.f }, { 160.f, 30.f });
        sf::FloatRect statsBound({ wrenchMenuPos.x, wrenchMenuPos.y + 60.f }, { 160.f, 30.f });
        sf::FloatRect iapBound({ wrenchMenuPos.x, wrenchMenuPos.y + 90.f }, { 160.f, 30.f });

        if (tradeBound.contains(uiPos)) {
            tradeMenuOpen = true;
            wrenchMenuOpen = false;

            for (int i = 0; i < 4; i++) {
                playerTradeSlots[i].tex = nullptr;
                playerTradeSlots[i].count = 0;
                playerTradeSlots[i].id = 0;
                npcTradeSlots[i].tex = nullptr;
                npcTradeSlots[i].count = 0;
                npcTradeSlots[i].id = 0;
            }
            hasBargained = false;
            bargainAccepted = false;
            bargainStatus = "";
            bargainColor = sf::Color::White;

            int itemsCount = 1 + (rand() % 3);
            std::vector<int> possibleIds = {
                ITEM_WOOD, ITEM_STONE, ITEM_SAPLING,
                ITEM_CHICKEN_MEAT, ITEM_PIG_MEAT, ITEM_SHEEP_MEAT, ITEM_WOOL, ITEM_ZOMBIE_FLESH,
                ITEM_COOKED_CHICKEN, ITEM_COOKED_PIG, ITEM_COOKED_SHEEP,
                ITEM_WOODEN_SWORD, ITEM_SHIELD, ITEM_FURNACE, ITEM_BED, ITEM_CHEST,
                ITEM_WOOD_TILE, ITEM_WOOD_WALL, ITEM_WOOD_DOOR, ITEM_WOOD_ROOF, ITEM_WOOD_STAIRS
            };
            for (int i = 0; i < itemsCount; i++) {
                int randIdx = rand() % possibleIds.size();
                int id = possibleIds[randIdx];
                const sf::Texture* tex = assets.getItemTexture(id);

                if (tex) {
                    npcTradeSlots[i].id = id;
                    npcTradeSlots[i].tex = tex;
                    if (id == ITEM_WOODEN_SWORD || id == ITEM_SHIELD || id == ITEM_FURNACE || id == ITEM_BED || id == ITEM_CHEST) {
                        npcTradeSlots[i].count = 1;
                    }
                    else if (id == ITEM_WOOD_TILE || id == ITEM_WOOD_WALL || id == ITEM_WOOD_DOOR || id == ITEM_WOOD_ROOF || id == ITEM_WOOD_STAIRS) {
                        npcTradeSlots[i].count = 2 + (rand() % 4);
                    }
                    else {
                        npcTradeSlots[i].count = 1 + (rand() % 8);
                    }
                }
            }

            tradeBoxesTicked = false;
            playerTradeGold = 0;
            npcTradeGold = 0;

            for (int j = 0; j < 4; j++) {
                if (npcTradeSlots[j].tex != nullptr) {
                    npcTradeGold += npcTradeSlots[j].count * getItemPrice(npcTradeSlots[j].id);
                }
            }

            npcGoldInput = std::to_string(npcTradeGold);
            playerGoldInput = "0";
            return true;
        }
        else if (questBound.contains(uiPos)) {
            int questIdx = qInfo.questID - 1;
            if (questIdx >= 0 && questIdx < (int)availableQuests.size()) {
                auto& q = availableQuests[questIdx];
                if (qInfo.isCompleted) {
                    latestPlayerMsg = "Quest: " + q.title + " (COMPLETED!)";
                }
                else {
                    latestPlayerMsg = "Quest: " + q.title + "\n" + q.description + " (" + std::to_string(qInfo.progress) + "/" + std::to_string(q.goal) + ")";
                }
            }
            else {
                latestPlayerMsg = "No quest available right now.";
            }
            bubbleTimer = 5.0f;
            wrenchMenuOpen = false;
            return true;
        }
        else if (statsBound.contains(uiPos)) {
            stockPageOpen = true;
            wrenchMenuOpen = false;
            return true;
        }
        else if (iapBound.contains(uiPos)) {
            iapPageOpen = true;
            wrenchMenuOpen = false;
            return true;
        }
        else {
            wrenchMenuOpen = false;
            return true;
        }
    }
    else {
        if (!wrenchTargetPlayer.empty()) {
            sf::FloatRect tradeOnlineBound({ wrenchMenuPos.x, wrenchMenuPos.y }, { 160.f, 30.f });
            sf::FloatRect craftItemBound2({ wrenchMenuPos.x, wrenchMenuPos.y + 30.f }, { 160.f, 30.f });
            if (tradeOnlineBound.contains(uiPos)) {
                p2pTradeID = authManager.createP2PTrade(username, wrenchTargetPlayer);
                if (p2pTradeID > 0) {
                    p2pTradeOpen   = true;
                    p2pAmInitiator = true;
                    p2pPartner     = wrenchTargetPlayer;
                    p2pMyReady     = false;
                    p2pPartnerReady= false;
                    p2pStatusMsg   = "Waiting for " + wrenchTargetPlayer + " to accept...";
                    for (int i = 0; i < 4; i++) myP2PSlots[i] = {};
                    myP2PGold = 0; myP2PGoldInput = "0";
                    partnerItems.clear(); partnerGold = 0;
                    p2pPollTimer = 1.f;
                    latestPlayerMsg = "Trade invite sent to " + wrenchTargetPlayer + "!";
                    bubbleTimer = 3.f;
                }
                wrenchMenuOpen = false;
                wrenchTargetPlayer = "";
                return true;
            }
            else if (craftItemBound2.contains(uiPos)) {
                craftMenuOpen = true;
                wrenchMenuOpen = false;
                wrenchTargetPlayer = "";
                return true;
            }
            else {
                wrenchMenuOpen = false;
                wrenchTargetPlayer = "";
                return true;
            }
        }
        else {
            sf::FloatRect craftItemBound({ wrenchMenuPos.x, wrenchMenuPos.y }, { 160.f, 34.f });
            if (craftItemBound.contains(uiPos)) {
                craftMenuOpen = true;
                wrenchMenuOpen = false;
                return true;
            }
            else {
                wrenchMenuOpen = false;
                return true;
            }
        }
    }
}

bool handleKeyAndTextInputs(
    const sf::Event& event,
    const GameSettings& globalSettings,
    bool& isChatting,
    bool& chatActive,
    TextInput& chatInput,
    const std::string& username,
    AuthManager& authManager,
    const std::vector<OnlinePlayer>& otherPlayers,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    bool tutorialActive,
    int tutorialStep,
    bool& tutChatted,
    bool& placementMode,
    float& placementRotation,
    bool& saplingPlacementMode,
    float& saplingRotation,
    std::vector<PlacedStructure>& structures,
    sf::Sprite& playerSprite,
    int playerFloorLevel,
    std::function<void()> syncSaveCallback,
    bool stockPageOpen,
    bool marketSearchActive,
    std::string& marketSearchText,
    int& marketScrollOffset,
    bool& playerGoldTyping,
    std::string& playerGoldInput,
    int& playerTradeGold,
    int currency,
    bool& p2pGoldTyping,
    std::string& myP2PGoldInput,
    int& myP2PGold,
    bool& p2pOfferDirty,
    float& p2pOfferPushTimer,
    bool& p2pMyReady,
    bool& npcGoldTyping,
    std::function<bool(const std::string& cmd, const std::string& args, std::string& feedback)> customCommandHandler,
    bool* adminPromptOpen,
    bool* adminPromptIsBan,
    std::string* adminPromptTarget,
    std::string* adminPromptDuration,
    int* adminPromptMuteMinutes,
    TextInput* adminReasonInput,
    sf::Clock* adminPromptClock
) {
    if (auto* textEvent = event.getIf<sf::Event::TextEntered>())
    {
        uint32_t unicode = textEvent->unicode;

        if (playerGoldTyping)
        {
            if (unicode >= '0' && unicode <= '9')
            {
                if (playerGoldInput == "0")
                    playerGoldInput.clear();

                playerGoldInput += static_cast<char>(unicode);
            }
            else if (unicode == 8 && !playerGoldInput.empty())
            {
                playerGoldInput.pop_back();
            }
            else if (unicode == 13) // Enter key commits gold typing
            {
                playerGoldTyping = false;
                return true;
            }

            if (playerGoldInput.empty())
                playerGoldInput = "0";

            try {
                playerTradeGold = std::stoi(playerGoldInput);
            } catch (...) {
                playerTradeGold = 0;
            }

            if (playerTradeGold > currency)
            {
                playerTradeGold = currency;
                playerGoldInput = std::to_string(currency);
            }
            return true;
        }
        else if (p2pGoldTyping)
        {
            if (unicode >= '0' && unicode <= '9')
            {
                if (myP2PGoldInput == "0")
                    myP2PGoldInput.clear();

                myP2PGoldInput += static_cast<char>(unicode);
            }
            else if (unicode == 8 && !myP2PGoldInput.empty())
            {
                myP2PGoldInput.pop_back();
            }
            else if (unicode == 13) // Enter key commits gold typing
            {
                p2pGoldTyping = false;
                return true;
            }

            if (myP2PGoldInput.empty())
                myP2PGoldInput = "0";

            try {
                myP2PGold = std::stoi(myP2PGoldInput);
            } catch (...) {
                myP2PGold = 0;
            }

            if (myP2PGold > currency)
            {
                myP2PGold = currency;
                myP2PGoldInput = std::to_string(currency);
            }
            p2pOfferDirty = true;
            p2pOfferPushTimer = 0.1f;
            p2pMyReady = false;
            return true;
        }

        npcGoldTyping = false;

        if (stockPageOpen && marketSearchActive) {
            if (unicode == 8) {
                if (!marketSearchText.empty())
                    marketSearchText.pop_back();
            }
            else if (unicode >= 32 && unicode < 127) {
                marketSearchText += static_cast<char>(unicode);
            }
            marketScrollOffset = 0;
            return true;
        }
    }

    if (auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->scancode == globalSettings.keyChat || key->scancode == sf::Keyboard::Scancode::T) {
            if (!chatActive) {
                chatActive = true;
                isChatting = true;
                chatInput.setFocus(true);
                chatInput.clear();
            }
            else {
                // If chat is open, pressing T again closes it unless user is actively typing a message
                std::string cur = chatInput.getText();
                if (!isChatting || cur.empty() || cur == "t" || cur == "T") {
                    chatActive = false;
                    isChatting = false;
                    chatInput.setFocus(false);
                    chatInput.clear();
                }
            }
            return true;
        }
        else if (key->scancode == sf::Keyboard::Scancode::Enter) {
            if (chatActive) {
                if (!isChatting) {
                    isChatting = true;
                    chatInput.setFocus(true);
                }
                else {
                    std::string msg = chatInput.getText();
                    if (!msg.empty()) {
                        handleChatCommandsAndInput(
                            msg,
                            username,
                            authManager,
                            otherPlayers,
                            latestPlayerMsg,
                            bubbleTimer,
                            tutorialActive,
                            tutorialStep,
                            tutChatted,
                            customCommandHandler,
                            adminPromptOpen,
                            adminPromptIsBan,
                            adminPromptTarget,
                            adminPromptDuration,
                            adminPromptMuteMinutes,
                            adminReasonInput,
                            adminPromptClock
                        );
                        chatInput.clear();
                    }
                    isChatting = false;
                    chatInput.setFocus(false);
                    chatActive = false; // Close chatbox when sending message
                }
            }
            return true;
        }
        else if (key->scancode == sf::Keyboard::Scancode::Escape) {
            if (playerGoldTyping || p2pGoldTyping) {
                playerGoldTyping = false;
                p2pGoldTyping = false;
                return true;
            }
            if (isChatting || chatActive) {
                chatActive = false;
                isChatting = false;
                chatInput.setFocus(false);
                chatInput.clear();
                return true;
            }
        }
        else if (key->scancode == globalSettings.keyRotate) {
            if (placementMode) {
                placementRotation = (placementRotation == 0.f) ? 90.f : ((placementRotation == 90.f) ? 180.f : 0.f);
            }
            else if (saplingPlacementMode) {
                saplingRotation += 90.f;
                if (saplingRotation >= 360.f) saplingRotation -= 360.f;
            }
            return true;
        }
        else if (key->scancode == globalSettings.keyInteract && !isChatting && !placementMode) {
            PlacedStructure* nearestDoor = nullptr;
            float minDist = 300.f;
            for (auto& s : structures) {
                if (s.id == ITEM_WOOD_DOOR && s.sprite.has_value()) {
                    sf::Vector2f dPos = s.sprite->getPosition();
                    sf::Vector2f pPos = playerSprite.getPosition();
                    int doorFloor = getFloorLevelFromY(dPos.y);
                    if (doorFloor == playerFloorLevel) {
                        float dy = (dPos.y + doorFloor * 180.f) - pPos.y;
                        float dx = dPos.x - pPos.x;
                        float d = std::sqrt(dx*dx + dy*dy);
                        if (d < minDist) {
                            minDist = d;
                            nearestDoor = &s;
                        }
                    }
                }
            }
            if (nearestDoor && nearestDoor->sprite.has_value()) {
                nearestDoor->isOpen = !nearestDoor->isOpen;
                float ignore;
                applyStructureScale(*nearestDoor->sprite, nearestDoor->id, nearestDoor->rotation, nearestDoor->isOpen, ignore);
                if (syncSaveCallback) syncSaveCallback();
            }
            return true;
        }
    }
    return false;
}

bool handleMouseDragAndScroll(
    const sf::Event& event,
    sf::RenderWindow& window,
    unsigned int width,
    unsigned int height,
    float chatBoxX, float chatBoxY, float chatBoxWidth, float chatBoxHeight,
    float& chatScroll,
    bool& isDraggingChat,
    sf::Vector2f& lastMousePosChat,
    bool stockPageOpen,
    int& marketScrollOffset,
    const sf::Vector2f& worldPos,
    std::vector<PlacedCampfire>& campfires,
    Inventory& inventory,
    const std::string& username,
    std::vector<PlacedStructure>& structures,
    bool& isSleeping,
    float& sleepFadeTimer,
    bool& sleepTimeSkipped,
    bool placementMode,
    bool& chestMenuOpen,
    PlacedStructure*& activeChest,
    bool chatActive,
    bool tradeMenuOpen,
    bool p2pTradeOpen,
    bool tradeConfirmOpen,
    bool wrenchMenuOpen
) {
    if (auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (wheel->wheel == sf::Mouse::Wheel::Vertical) {
            sf::Vector2f uiPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
            if (chatActive && sf::FloatRect({ chatBoxX, chatBoxY }, { chatBoxWidth, chatBoxHeight }).contains(uiPos)) {
                chatScroll -= wheel->delta * 30.f;
                if (chatScroll < 0.f) chatScroll = 0.f;
                return true;
            }
            sf::FloatRect sBoxRect({ (width - 900.f) / 2.f, (height - 600.f) / 2.f }, { 900.f, 600.f });
            if (stockPageOpen && sBoxRect.contains(uiPos)) {
                marketScrollOffset -= (int)wheel->delta;
                if (marketScrollOffset < 0) marketScrollOffset = 0;
                return true;
            }
            else if (!placementMode) {
                // Slide through items with mouse wheel
                int cur = inventory.getSelectedSlot();
                if (wheel->delta > 0.f) {
                    if (cur <= 0) cur = 19;
                    else cur--;
                    inventory.setSelectedSlot(cur);
                } else if (wheel->delta < 0.f) {
                    if (cur < 0 || cur >= 19) cur = 0;
                    else cur++;
                    inventory.setSelectedSlot(cur);
                }
                return true;
            }
        }
    }

    if (auto* mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouse->button == sf::Mouse::Button::Left) {
            bool anyCampfireOpen = std::any_of(campfires.begin(), campfires.end(), [](const PlacedCampfire& f) { return f.menuOpen; });
            bool anyFurnaceOpen = std::any_of(structures.begin(), structures.end(), [](const PlacedStructure& s) { return s.id == ITEM_FURNACE && s.menuOpen; });

            bool isSectionUndergoing = chestMenuOpen || anyCampfireOpen || anyFurnaceOpen || isSleeping || stockPageOpen || tradeMenuOpen || p2pTradeOpen || tradeConfirmOpen || wrenchMenuOpen;

            // Don't fire world interactions while in placement mode or when any menu is open
            if (!placementMode && !isSectionUndergoing) {
                // Use the actual click position (not the frame-level worldPos)
                sf::Vector2f curWorldPos = window.mapPixelToCoords(mouse->position);

                // Structure interactions — mutually exclusive, campfire/furnace on wrench icon or body
                bool hitWorldStructure = false;

                // Campfire — wrench icon area or campfire body
                for (auto& fire : campfires)
                {
                    if (hitWorldStructure) break;
                    sf::FloatRect wrenchBtnBounds({ fire.position.x - 35.f, fire.position.y - 110.f }, { 70.f, 70.f });
                    sf::FloatRect fireBounds({ fire.position.x - 50.f, fire.position.y - 70.f }, { 100.f, 80.f });
                    if (wrenchBtnBounds.contains(curWorldPos) || fireBounds.contains(curWorldPos))
                    {
                        if (!fire.owner.empty() && fire.owner != username) {
                            inventory.showBanner("This belongs to " + fire.owner + "! You cannot wrench other players' items.");
                            hitWorldStructure = true;
                            break;
                        }
                        for (auto& f : campfires) f.menuOpen = false;
                        for (auto& st : structures) st.menuOpen = false;
                        chestMenuOpen = false;
                        activeChest = nullptr;
                        isSleeping = false;
                        fire.menuOpen = true;
                        hitWorldStructure = true;
                    }
                }

                // Furnace — wrench icon area or furnace body
                if (!hitWorldStructure) {
                    for (auto& s : structures)
                    {
                        if (hitWorldStructure) break;
                        if (s.id == ITEM_FURNACE && s.sprite.has_value()) {
                            sf::Vector2f fPos = s.sprite->getPosition();
                            sf::FloatRect wrenchBtnBounds({ fPos.x - 35.f, fPos.y - 185.f }, { 70.f, 70.f });
                            sf::FloatRect furnaceBounds({ fPos.x - 80.f, fPos.y - 150.f }, { 160.f, 160.f });
                            if (wrenchBtnBounds.contains(curWorldPos) || furnaceBounds.contains(curWorldPos)) {
                                if (!s.owner.empty() && s.owner != username) {
                                    inventory.showBanner("This belongs to " + s.owner + "! You cannot wrench other players' items.");
                                    hitWorldStructure = true;
                                    break;
                                }
                                for (auto& f : campfires) f.menuOpen = false;
                                for (auto& st : structures) st.menuOpen = false;
                                chestMenuOpen = false;
                                activeChest = nullptr;
                                isSleeping = false;
                                s.menuOpen = true;
                                hitWorldStructure = true;
                            }
                        }
                    }
                }

                // Bed — wrench icon or body (bed has no dedicated menu, just triggers sleep)
                if (!hitWorldStructure) {
                    for (auto& s : structures)
                    {
                        if (hitWorldStructure) break;
                        if (s.id == ITEM_BED && s.sprite.has_value()) {
                            sf::Vector2f bPos = s.sprite->getPosition();
                            sf::FloatRect wrenchBtnBounds({ bPos.x - 35.f, bPos.y - 115.f }, { 70.f, 70.f });
                            sf::FloatRect bedBounds({ bPos.x - 100.f, bPos.y - 120.f }, { 200.f, 120.f });
                            if (wrenchBtnBounds.contains(curWorldPos) || bedBounds.contains(curWorldPos)) {
                                if (!s.owner.empty() && s.owner != username) {
                                    inventory.showBanner("This belongs to " + s.owner + "! You cannot wrench other players' items.");
                                    hitWorldStructure = true;
                                    break;
                                }
                                if (!isSleeping) {
                                    for (auto& f : campfires) f.menuOpen = false;
                                    for (auto& st : structures) st.menuOpen = false;
                                    chestMenuOpen = false;
                                    activeChest = nullptr;
                                    isSleeping = true;
                                    sleepFadeTimer = 0.f;
                                    sleepTimeSkipped = false;
                                    hitWorldStructure = true;
                                }
                            }
                        }
                    }
                }

                // Check Door Interaction Button & Door Click
                if (!hitWorldStructure) {
                    for (auto& s : structures) {
                        if (s.id == ITEM_WOOD_DOOR && s.sprite.has_value()) {
                            float doorCenterX = s.sprite->getPosition().x + (s.rotation == 90.f ? 125.f : 250.f);
                            float doorCenterY = s.sprite->getPosition().y - (s.rotation == 90.f ? 125.f : 0.f);
                            sf::FloatRect doorBtnBounds({ doorCenterX - 100.f, doorCenterY - 120.f }, { 200.f, 60.f });
                            sf::FloatRect doorBounds;
                            if (s.rotation == 90.f) {
                                doorBounds = sf::FloatRect({ s.sprite->getPosition().x, s.sprite->getPosition().y - 500.f }, { 250.f, 500.f });
                            } else {
                                doorBounds = sf::FloatRect({ s.sprite->getPosition().x, s.sprite->getPosition().y - 500.f }, { 500.f, 500.f });
                            }
                            if (doorBtnBounds.contains(curWorldPos) || doorBounds.contains(curWorldPos)) {
                                if (!s.owner.empty() && s.owner != username) {
                                    inventory.showBanner("This belongs to " + s.owner + "! You cannot wrench other players' items.");
                                    hitWorldStructure = true;
                                    break;
                                }
                                s.isOpen = !s.isOpen;
                                float ignore;
                                applyStructureScale(*s.sprite, s.id, s.rotation, s.isOpen, ignore);
                                hitWorldStructure = true;
                                break;
                            }
                        }
                    }
                }

                // Check Chest — open the chest menu when clicking it
                if (!hitWorldStructure && !chestMenuOpen) {
                    for (auto& s : structures) {
                        if (s.id == ITEM_CHEST && s.sprite.has_value()) {
                            sf::Vector2f cPos = s.sprite->getPosition();
                            sf::FloatRect chestBounds({ cPos.x - 150.f, cPos.y - 200.f }, { 300.f, 250.f });
                            if (chestBounds.contains(curWorldPos)) {
                                if (!s.owner.empty() && s.owner != username) {
                                    inventory.showBanner("This belongs to " + s.owner + "! You cannot wrench other players' items.");
                                    hitWorldStructure = true;
                                    break;
                                }
                                for (auto& f : campfires) f.menuOpen = false;
                                for (auto& st : structures) st.menuOpen = false;
                                isSleeping = false;
                                chestMenuOpen = true;
                                activeChest = &s;
                                hitWorldStructure = true;
                                break;
                            }
                        }
                    }
                }

                if (hitWorldStructure) {
                    return true;
                }
            } // end !placementMode && !isSectionUndergoing

            if (chatActive) {
                sf::Vector2f uiPos = window.mapPixelToCoords(mouse->position, GetUIView(&window));
                if (sf::FloatRect({ chatBoxX, chatBoxY }, { chatBoxWidth, chatBoxHeight }).contains(uiPos)) {
                    isDraggingChat = true;
                    lastMousePosChat = uiPos;
                    return true;
                }
            }
        }
    }

    if (auto* mouse = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (mouse->button == sf::Mouse::Button::Left) {
            if (chatActive && isDraggingChat) {
                isDraggingChat = false;
                return true;
            }
        }
    }

    if (auto* move = event.getIf<sf::Event::MouseMoved>()) {
        if (chatActive && isDraggingChat) {
            sf::Vector2f uiPos = window.mapPixelToCoords(move->position, GetUIView(&window));
            chatScroll -= (uiPos.y - lastMousePosChat.y);
            if (chatScroll < 0.f) chatScroll = 0.f;
            lastMousePosChat = uiPos;
            return true;
        }
    }
    return false;
}

bool handleTopBarAndWrenchClicks(
    sf::Vector2f uiPos,
    sf::Vector2f worldPos,
    unsigned int width,
    unsigned int height,
    sf::RenderWindow& window,
    sf::Clock& deltaClock,
    bool& stockPageOpen,
    bool& wrenchMenuOpen,
    bool& craftMenuOpen,
    bool& placementMode,
    bool& tradeMenuOpen,
    bool& tradeConfirmOpen,
    bool& p2pTradeOpen,
    bool isAnyBlockingMenuOpen,
    bool& isTargetNPC,
    std::string& wrenchTargetPlayer,
    sf::Vector2f& wrenchMenuPos,
    const std::vector<OnlinePlayer>& otherPlayers,
    const std::map<std::string, sf::Vector2f>& onlinePlayerDisplayPos,
    const std::map<std::string, sf::FloatRect>& onlinePlayerWrenchBounds,
    const sf::Sprite& playerWrenchBtn,
    const NPC& npcFemale,
    const NPC& npcMale,
    std::function<void()> logoutCallback
) {
    sf::FloatRect settingsBtnRect({ (float)width - 140.f, 20.f }, { 120.f, 40.f });
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && settingsBtnRect.contains(uiPos)) {
        openSettingsWindow(window, width, height);
        updateActiveMusicVolume();
        deltaClock.restart();
        return true;
    }

    // Logout button click — save all data then close window (returns to main menu)
    sf::FloatRect logoutBtnRect({ (float)width - 140.f, 70.f }, { 110.f, 40.f });
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && logoutBtnRect.contains(uiPos)) {
        if (logoutCallback) logoutCallback();
        return true;
    }

    sf::FloatRect marketBtnRect({ (float)width - 300.f, 20.f }, { 150.f, 40.f });
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && marketBtnRect.contains(uiPos)) {
        stockPageOpen = true;
        return true;
    }

    // Online player click -> open wrench with "Trade" option
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !p2pTradeOpen && !isAnyBlockingMenuOpen) {
        for (const auto& op : otherPlayers) {
            bool hitWrench = false;
            auto wbIt = onlinePlayerWrenchBounds.find(op.username);
            if (wbIt != onlinePlayerWrenchBounds.end()) {
                sf::FloatRect wb = wbIt->second;
                sf::FloatRect wbHit({ wb.position.x - 20.f, wb.position.y - 20.f }, { wb.size.x + 40.f, wb.size.y + 40.f });
                sf::FloatRect wNameHit({ wb.position.x - 200.f, wb.position.y - 15.f }, { 210.f, wb.size.y + 30.f });
                hitWrench = wbHit.contains(worldPos) || wNameHit.contains(worldPos);
            }
            if (!hitWrench) {
                auto dispIt = onlinePlayerDisplayPos.find(op.username);
                sf::Vector2f opPos = (dispIt != onlinePlayerDisplayPos.end()) ? dispIt->second : sf::Vector2f(op.x, op.y);
                sf::FloatRect opBounds({ opPos.x - 120.f, opPos.y - 280.f }, { 240.f, 320.f });
                hitWrench = opBounds.contains(worldPos);
            }
            if (hitWrench) {
                wrenchMenuOpen = true;
                isTargetNPC = false;
                wrenchTargetPlayer = op.username;
                wrenchMenuPos = uiPos;
                wrenchMenuPos.x = std::clamp(wrenchMenuPos.x, 10.f, 1920.f - 170.f);
                wrenchMenuPos.y = std::clamp(wrenchMenuPos.y, 10.f, 1080.f - 140.f);
                return true;
            }
        }
    }

    // Allow wrench click only if no other menus are open
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && !isAnyBlockingMenuOpen) {
        // Player wrench button, nametag, or body
        sf::FloatRect pb = playerWrenchBtn.getGlobalBounds();
        sf::FloatRect pbHit({ pb.position.x - 20.f, pb.position.y - 20.f }, { pb.size.x + 40.f, pb.size.y + 40.f });
        sf::FloatRect pNameHit({ pb.position.x - 200.f, pb.position.y - 15.f }, { 210.f, pb.size.y + 30.f });
        sf::FloatRect pBodyHit({ pb.position.x - 140.f, pb.position.y + 30.f }, { 200.f, 300.f });
        if (pbHit.contains(worldPos) || pNameHit.contains(worldPos) || pBodyHit.contains(worldPos)) {
            wrenchMenuOpen = true;
            isTargetNPC = false;
            wrenchTargetPlayer = "";
            wrenchMenuPos = uiPos;
            wrenchMenuPos.x = std::clamp(wrenchMenuPos.x, 10.f, 1920.f - 170.f);
            wrenchMenuPos.y = std::clamp(wrenchMenuPos.y, 10.f, 1080.f - 140.f);
            return true;
        }

        // Female NPC wrench button, nametag, or body
        bool hitFemale = false;
        if (npcFemale.wrenchBtn.has_value()) {
            sf::FloatRect fb = npcFemale.wrenchBtn->getGlobalBounds();
            sf::FloatRect fbHit({ fb.position.x - 20.f, fb.position.y - 20.f }, { fb.size.x + 40.f, fb.size.y + 40.f });
            sf::FloatRect fNameHit({ fb.position.x - 200.f, fb.position.y - 15.f }, { 210.f, fb.size.y + 30.f });
            hitFemale = fbHit.contains(worldPos) || fNameHit.contains(worldPos);
        }
        if (!hitFemale && npcFemale.sprite.has_value()) {
            sf::FloatRect fBody = npcFemale.sprite->getGlobalBounds();
            hitFemale = fBody.contains(worldPos);
        }
        if (hitFemale) {
            wrenchMenuOpen = true;
            isTargetNPC = true;
            wrenchTargetPlayer = "";
            wrenchMenuPos = uiPos;
            wrenchMenuPos.x = std::clamp(wrenchMenuPos.x, 10.f, 1920.f - 170.f);
            wrenchMenuPos.y = std::clamp(wrenchMenuPos.y, 10.f, 1080.f - 140.f);
            return true;
        }

        // Male NPC wrench button, nametag, or body
        bool hitMale = false;
        if (npcMale.wrenchBtn.has_value()) {
            sf::FloatRect mb = npcMale.wrenchBtn->getGlobalBounds();
            sf::FloatRect mbHit({ mb.position.x - 20.f, mb.position.y - 20.f }, { mb.size.x + 40.f, mb.size.y + 40.f });
            sf::FloatRect mNameHit({ mb.position.x - 200.f, mb.position.y - 15.f }, { 210.f, mb.size.y + 30.f });
            hitMale = mbHit.contains(worldPos) || mNameHit.contains(worldPos);
        }
        if (!hitMale && npcMale.sprite.has_value()) {
            sf::FloatRect mBody = npcMale.sprite->getGlobalBounds();
            hitMale = mBody.contains(worldPos);
        }
        if (hitMale) {
            wrenchMenuOpen = true;
            isTargetNPC = true;
            wrenchTargetPlayer = "";
            wrenchMenuPos = uiPos;
            wrenchMenuPos.x = std::clamp(wrenchMenuPos.x, 10.f, 1920.f - 170.f);
            wrenchMenuPos.y = std::clamp(wrenchMenuPos.y, 10.f, 1080.f - 140.f);
            return true;
        }
    }

    return false;
}

void handleAllGameMouseButtonPressed(
    sf::Vector2f uiPos,
    sf::Vector2f worldPos,
    sf::Vector2i mousePixelPos,
    sf::RenderWindow& window,
    unsigned int width,
    unsigned int height,
    sf::Clock& deltaClock,
    bool& stockPageOpen,
    bool& wrenchMenuOpen,
    bool& craftMenuOpen,
    bool& placementMode,
    bool& tradeMenuOpen,
    bool& tradeConfirmOpen,
    std::string& tradeConfirmMessage,
    bool& tradeConfirmIsP2P,
    bool& p2pTradeOpen,
    bool& isTargetNPC,
    std::string& wrenchTargetPlayer,
    sf::Vector2f& wrenchMenuPos,
    const std::vector<OnlinePlayer>& otherPlayers,
    const std::map<std::string, sf::Vector2f>& onlinePlayerDisplayPos,
    const std::map<std::string, sf::FloatRect>& onlinePlayerWrenchBounds,
    const sf::Sprite& playerWrenchBtn,
    const NPC& npcFemale,
    const NPC& npcMale,
    bool& iapPageOpen,
    bool& iapVerifyingOpen,
    int& iapVerifyingGold,
    float& iapVerifyingPrice,
    std::string& iapVerifyingTier,
    TextInput& iapCodeInput,
    const std::string& username,
    int& currency,
    float& hunger,
    float& thirst,
    int level,
    int exp,
    float elapsedGameTime,
    int& hungerMultiplier,
    int& thirstMultiplier,
    std::string& marketSearchText,
    bool& marketSearchActive,
    int& marketScrollOffset,
    bool& playerGoldTyping,
    bool& p2pGoldTyping,
    bool& selectionDone,
    bool& introActive,
    int& currentIntroFrame,
    const std::vector<sf::Texture>& introFrames,
    std::optional<sf::Sprite>& introSprite,
    bool& tutorialActive,
    int& tutorialStep,
    const std::vector<TutorialStep>& tutorialSteps,
    const sf::RectangleShape& tutorialBox,
    float tutMoveDist,
    bool tutSprinted,
    bool tutInvOpened,
    int tutWoodGathered,
    int tutStoneGathered,
    bool tutAttacked,
    bool tutChatted,
    AuthManager& authManager,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    bool& p2pInvitePopup,
    int& p2pTradeID,
    bool& p2pAmInitiator,
    std::string& p2pPartner,
    P2PTradeState& p2pState,
    bool& p2pMyReady,
    bool& p2pPartnerReady,
    bool& p2pOfferDirty,
    std::string& p2pStatusMsg,
    TradeSlot myP2PSlots[4],
    int& myP2PGold,
    std::string& myP2PGoldInput,
    std::vector<P2PItemDisplay>& partnerItems,
    int& partnerGold,
    bool& tradeBoxesTicked,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    int& playerTradeGold,
    int& npcTradeGold,
    bool& hasBargained,
    bool& bargainAccepted,
    std::string& bargainStatus,
    sf::Color& bargainColor,
    Inventory& inventory,
    std::vector<StockItem>& stockMarket,
    bool& isSellingToNPC,
    std::string& npcGoldInput,
    std::string& playerGoldInput,
    std::function<int(int)> getItemPrice,
    std::function<std::string(int)> getItemName,
    std::function<void(int, float)> adjustMarketItem,
    bool& quantityPromptOpen,
    int& quantityPromptMax,
    int& quantityPromptTargetSlot,
    int& quantityPromptSourceSlot,
    bool& quantityPromptIsP2P,
    bool& quantityPromptIsCampfire,
    int& quantityPromptCampfireSlot,
    bool& quantityPromptIsChest,
    int& quantityPromptChestSlot,
    TextInput& quantityInput,
    QuestInfo& qInfo,
    const std::vector<DailyQuest>& availableQuests,
    float& p2pPollTimer,
    const GameAssets& assets,
    bool& chestMenuOpen,
    PlacedStructure*& activeChest,
    int& chestSelectedInvSlot,
    bool& saplingPlacementMode,
    sf::Vector2f& saplingPlacementPos,
    float& saplingRotation,
    bool& saplingFollowingCursor,
    int& saplingFromInventorySlot,
    const sf::Texture& saplingTex,
    std::vector<PlantedSapling>& plantedSaplings,
    std::function<sf::Vector2f(sf::Vector2f)> snapToGrid,
    int& placementItemID,
    int& placementFromInventorySlot,
    sf::Vector2f& placementPos,
    float& placementRotation,
    bool& placementFollowingCursor,
    std::vector<PlacedCampfire>& campfires,
    std::vector<PlacedStructure>& structures,
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Texture& chestTex,
    std::function<void()> syncSaveCallback,
    const std::vector<CraftableItem>& craftableItems,
    bool& swordCrafting,
    float& swordCraftTimer,
    bool& shieldCrafting,
    float& shieldCraftTimer,
    CharacterSelection& characterSelection,
    int& option,
    bool inventoryOpen,
    const sf::Texture campfireFrames[8],
    float maxHunger,
    const int MAX_MULTIPLIER,
    float& hp,
    bool disasterActive,
    const std::string& currentDisasterType,
    bool* isLoggingOutPtr,
    bool* logoutCloseWindowPtr
) {
    bool clickHandled = false;
    playerGoldTyping = false;
    p2pGoldTyping = false;

    // Compute whether ANY blocking menu is open — used to swallow clicks below
    bool anyCampfireMenuOpen = std::any_of(campfires.begin(), campfires.end(),
        [](const PlacedCampfire& f) { return f.menuOpen; });
    bool anyFurnaceMenuOpen2 = std::any_of(structures.begin(), structures.end(),
        [](const PlacedStructure& s) { return s.id == ITEM_FURNACE && s.menuOpen; });
    bool isAnyMenuOpen = (tradeMenuOpen || tradeConfirmOpen || wrenchMenuOpen || craftMenuOpen
        || p2pTradeOpen || chestMenuOpen || anyCampfireMenuOpen || anyFurnaceMenuOpen2
        || quantityPromptOpen || iapPageOpen || iapVerifyingOpen || stockPageOpen);

    if (stockPageOpen) {
        sf::FloatRect sBoxRect({ (width - 900.f) / 2.f, (height - 600.f) / 2.f }, { 900.f, 600.f });
        sf::FloatRect closeBtnRect({ sBoxRect.position.x + 840.f, sBoxRect.position.y + 20.f }, { 40.f, 40.f });
        sf::FloatRect searchBarRect({ sBoxRect.position.x + 40.f, sBoxRect.position.y + 65.f }, { 680.f, 32.f });
        if (!sBoxRect.contains(uiPos) || closeBtnRect.contains(uiPos)) {
            stockPageOpen = false;
            marketSearchText = "";
            marketSearchActive = false;
            marketScrollOffset = 0;
            return;
        }
        else {
            marketSearchActive = searchBarRect.contains(uiPos);
        }
        return;
    }

    if (iapPageOpen && !iapVerifyingOpen) {
        clickHandled = handleIAPStoreClicks(
            uiPos,
            width,
            height,
            iapPageOpen,
            iapVerifyingOpen,
            iapVerifyingGold,
            iapVerifyingPrice,
            iapVerifyingTier,
            iapCodeInput,
            username
        );
        if (clickHandled) return;
    }

    if (iapVerifyingOpen) {
        clickHandled = handleIAPVerifyingClicks(
            uiPos,
            width,
            height,
            iapVerifyingOpen,
            iapVerifyingGold,
            iapVerifyingTier,
            iapCodeInput,
            username,
            currency,
            [&]() {
                authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, elapsedGameTime, hungerMultiplier, thirstMultiplier);
            }
        );
        if (clickHandled) return;
    }

    if (handleTopBarAndWrenchClicks(
        uiPos,
        worldPos,
        width,
        height,
        window,
        deltaClock,
        stockPageOpen,
        wrenchMenuOpen,
        craftMenuOpen,
        placementMode,
        tradeMenuOpen,
        tradeConfirmOpen,
        p2pTradeOpen,
        isAnyMenuOpen,   // block NPC/player wrench when a menu is already open
        isTargetNPC,
        wrenchTargetPlayer,
        wrenchMenuPos,
        otherPlayers,
        onlinePlayerDisplayPos,
        onlinePlayerWrenchBounds,
        playerWrenchBtn,
        npcFemale,
        npcMale,
        [&]() {
            if (isLoggingOutPtr) {
                *isLoggingOutPtr = true;
                if (logoutCloseWindowPtr) *logoutCloseWindowPtr = false;
            } else {
                saveAllGameDataAndClose(
                    window,
                    authManager,
                    username,
                    playerTradeSlots,
                    myP2PSlots,
                    p2pTradeID,
                    inventory,
                    hunger,
                    thirst,
                    currency,
                    level,
                    exp,
                    elapsedGameTime,
                    hungerMultiplier,
                    thirstMultiplier,
                    structures,
                    campfires
                );
            }
        }
    )) {
        return;
    }

    if (handleIntroAndTutorialClicks(
        uiPos,
        selectionDone,
        introActive,
        currentIntroFrame,
        introFrames,
        introSprite,
        tutorialActive,
        tutorialStep,
        tutorialSteps,
        tutorialBox,
        tutMoveDist,
        tutSprinted,
        tutInvOpened,
        tutWoodGathered,
        tutStoneGathered,
        tutAttacked,
        tutChatted,
        authManager,
        username,
        latestPlayerMsg,
        bubbleTimer
    )) {
        clickHandled = true;
    }
    else if (std::any_of(campfires.begin(), campfires.end(), [](const PlacedCampfire& f) { return f.menuOpen; })) {
        clickHandled = handleCampfireMenuClicks(
            uiPos,
            width,
            height,
            campfires,
            inventory,
            assets.woodTex,
            saplingTex,
            assets.cookedChickenTex,
            assets.cookedPigTex,
            assets.cookedSheepTex,
            quantityPromptOpen,
            quantityPromptMax,
            quantityPromptTargetSlot,
            quantityPromptSourceSlot,
            quantityPromptIsP2P,
            quantityPromptIsCampfire,
            quantityPromptCampfireSlot,
            quantityInput,
            saplingPlacementMode,
            syncSaveCallback,
            [&](int id) { return assets.getItemTexture(id); }
        );
    }
    else if (std::any_of(structures.begin(), structures.end(), [](const PlacedStructure& s) { return s.id == ITEM_FURNACE && s.menuOpen; })) {
        clickHandled = handleFurnaceMenuClicks(
            uiPos,
            width,
            height,
            structures,
            inventory,
            assets.woodTex,
            saplingTex,
            assets.cookedChickenTex,
            assets.cookedPigTex,
            assets.cookedSheepTex,
            quantityPromptOpen,
            quantityPromptMax,
            quantityPromptTargetSlot,
            quantityPromptSourceSlot,
            quantityPromptIsP2P,
            quantityPromptIsCampfire,
            quantityPromptCampfireSlot,
            quantityInput,
            saplingPlacementMode,
            syncSaveCallback,
            [&](int id) { return assets.getItemTexture(id); }
        );
    }
    else if (chestMenuOpen && activeChest != nullptr) {
        // ── Chest menu two-panel click handler — geometry MUST match drawChestMenu ──
        const float SLOT_S   = 66.f, GAP = 7.f;
        const float GRID_W   = 5 * SLOT_S + 4 * GAP;         // 358
        const float PAD      = 14.f;
        const float HEADER_H = 48.f, LABEL_H = 20.f, HINT_H = 26.f;
        const float DIV_W    = 2.f, COL_GAP = 20.f;
        const float LEFT_W   = PAD + GRID_W + PAD;            // 386
        const float RIGHT_W  = PAD + GRID_W + PAD + 40.f;    // 426
        const float PANEL_W  = LEFT_W + DIV_W + COL_GAP + RIGHT_W; // 834
        const float PANEL_H  = HEADER_H + LABEL_H + GAP + (4 * SLOT_S + 3 * GAP) + PAD + HINT_H;
        float ppx = (float)width  / 2.f - PANEL_W / 2.f;
        float ppy = (float)height / 2.f - PANEL_H / 2.f;

        // Close button or outside panel
        sf::FloatRect panelRect({ ppx, ppy }, { PANEL_W, PANEL_H });
        sf::FloatRect closeBtnRect({ ppx + PANEL_W - 42.f, ppy + 6.f }, { 36.f, 36.f });
        if (!panelRect.contains(uiPos) || closeBtnRect.contains(uiPos)) {
            chestMenuOpen    = false;
            activeChest      = nullptr;
            chestSelectedInvSlot = -1;
            clickHandled     = true;
        } else {
            const float LEFT_GX  = ppx + PAD;
            const float LEFT_GY  = ppy + HEADER_H + LABEL_H + GAP;
            const float RIGHT_GX = ppx + LEFT_W + DIV_W + COL_GAP + PAD;
            const float RIGHT_GY = LEFT_GY;

            bool handledSlot = false;

            // ── LEFT panel: inventory slot click (STORE into chest) ─────────────
            for (int i = 0; i < 20 && i < CHEST_SLOT_COUNT && !handledSlot; i++) {
                int col = i % 5, row = i / 5;
                float sx = LEFT_GX + col * (SLOT_S + GAP);
                float sy = LEFT_GY + row * (SLOT_S + GAP);
                sf::FloatRect slotRect({ sx, sy }, { SLOT_S, SLOT_S });
                if (slotRect.contains(uiPos)) {
                    if (inventory.hasItem(i)) {
                        chestSelectedInvSlot = i;
                        inventory.setSelectedSlot(i);
                        int totalQty = inventory.getItemCount(i);
                        // Open quantity prompt to store this item into the chest
                        quantityPromptOpen       = true;
                        quantityPromptIsChest    = true;
                        quantityPromptIsP2P      = false;
                        quantityPromptIsCampfire = false;
                        quantityPromptSourceSlot = i; // STORE mode
                        quantityPromptChestSlot  = -1; // Auto-target matching or first empty slot
                        quantityPromptMax        = totalQty;
                        quantityInput.clear();
                        quantityInput.setText(std::to_string(totalQty));
                        quantityInput.setFocus(true);
                    } else {
                        chestSelectedInvSlot = -1;
                    }
                    handledSlot  = true;
                    clickHandled = true;
                }
            }

            // ── RIGHT panel: chest slot click (RETRIEVE from chest) ────────────
            for (int i = 0; i < CHEST_SLOT_COUNT && !handledSlot; i++) {
                int col = i % 5, row = i / 5;
                float sx = RIGHT_GX + col * (SLOT_S + GAP);
                float sy = RIGHT_GY + row * (SLOT_S + GAP);
                sf::FloatRect slotRect({ sx, sy }, { SLOT_S, SLOT_S });
                if (slotRect.contains(uiPos)) {
                    ChestSlot& cs = activeChest->chestSlots[i];
                    bool slotOccupied = (cs.id != 0 && cs.count > 0);

                    if (slotOccupied) {
                        // Clicking an occupied chest slot ALWAYS RETRIEVES from chest,
                        // regardless of what item is currently held in inventory.
                        quantityPromptOpen       = true;
                        quantityPromptIsChest    = true;
                        quantityPromptIsP2P      = false;
                        quantityPromptIsCampfire = false;
                        quantityPromptSourceSlot = -1;      // RETRIEVE mode
                        quantityPromptChestSlot  = i;
                        quantityPromptMax        = cs.count;
                        quantityInput.clear();
                        quantityInput.setText(std::to_string(quantityPromptMax));
                        quantityInput.setFocus(true);
                        chestSelectedInvSlot     = -1;
                    } else {
                        // Clicking an EMPTY chest slot stores the selected inventory item into this slot
                        int srcSlot = (chestSelectedInvSlot >= 0) ? chestSelectedInvSlot : inventory.getSelectedSlot();
                        if (srcSlot >= 0 && inventory.hasItem(srcSlot)) {
                            quantityPromptOpen       = true;
                            quantityPromptIsChest    = true;
                            quantityPromptIsP2P      = false;
                            quantityPromptIsCampfire = false;
                            quantityPromptSourceSlot = srcSlot; // STORE mode
                            quantityPromptChestSlot  = i;      // Specific empty slot
                            quantityPromptMax        = inventory.getItemCount(srcSlot);
                            quantityInput.clear();
                            quantityInput.setText(std::to_string(quantityPromptMax));
                            quantityInput.setFocus(true);
                        }
                    }
                    handledSlot  = true;
                    clickHandled = true;
                }
            }
        }
        // Always consume clicks while chest menu is open
        clickHandled = true;
    }
    else if (p2pInvitePopup) {
        clickHandled = handleP2PInvitePopupClicks(
            uiPos,
            width,
            height,
            p2pInvitePopup,
            p2pTradeID,
            p2pTradeOpen,
            p2pAmInitiator,
            p2pPartner,
            p2pState,
            p2pMyReady,
            p2pPartnerReady,
            p2pStatusMsg,
            myP2PSlots,
            myP2PGold,
            myP2PGoldInput,
            partnerItems,
            partnerGold,
            authManager
        );
        if (clickHandled) p2pPollTimer = 0.f;
    }
    else if (tradeConfirmOpen) {
        clickHandled = handleNPCTradeConfirmClicks(
            uiPos,
            width,
            height,
            tradeConfirmOpen,
            tradeConfirmMessage,
            tradeConfirmIsP2P,
            tradeMenuOpen,
            tradeBoxesTicked,
            playerTradeSlots,
            npcTradeSlots,
            isSellingToNPC,
            playerTradeGold,
            playerGoldInput,
            npcTradeGold,
            npcGoldInput,
            hasBargained,
            bargainAccepted,
            bargainStatus,
            bargainColor,
            currency,
            username,
            elapsedGameTime,
            level,
            exp,
            hungerMultiplier,
            thirstMultiplier,
            hunger,
            thirst,
            inventory,
            authManager,
            stockMarket,
            getItemPrice,
            getItemName,
            adjustMarketItem,
            latestPlayerMsg,
            bubbleTimer,
            p2pTradeID,
            myP2PSlots,
            myP2PGold,
            &p2pOfferDirty,
            &p2pMyReady,
            p2pPartnerReady,
            &p2pStatusMsg,
            p2pPartner,
            disasterActive,
            currentDisasterType
        );
    }
    else if (p2pTradeOpen) {
        quantityPromptIsChest = false;
        quantityPromptIsCampfire = false;
        clickHandled = handleP2PTradeClick(
            uiPos,
            mousePixelPos,
            width,
            height,
            p2pTradeID,
            username,
            p2pPartner,
            myP2PSlots,
            myP2PGold,
            myP2PGoldInput,
            p2pGoldTyping,
            playerGoldTyping,
            p2pMyReady,
            p2pPartnerReady,
            p2pOfferDirty,
            p2pStatusMsg,
            p2pTradeOpen,
            partnerItems,
            partnerGold,
            inventory,
            authManager,
            quantityPromptOpen,
            quantityPromptMax,
            quantityPromptTargetSlot,
            quantityPromptSourceSlot,
            quantityPromptIsP2P,
            quantityInput,
            window,
            tradeConfirmOpen,
            tradeConfirmMessage,
            tradeConfirmIsP2P,
            currency,
            disasterActive,
            currentDisasterType,
            stockMarket
        );
        if (clickHandled) p2pPollTimer = 0.f;
    }
    else if (tradeMenuOpen) {
        quantityPromptIsChest = false;
        quantityPromptIsCampfire = false;
        clickHandled = handleNPCTradeClick(
            uiPos,
            mousePixelPos,
            width,
            height,
            playerTradeSlots,
            npcTradeSlots,
            isSellingToNPC,
            npcTradeGold,
            npcGoldInput,
            playerTradeGold,
            playerGoldInput,
            playerGoldTyping,
            stockPageOpen,
            hasBargained,
            bargainAccepted,
            bargainStatus,
            bargainColor,
            currency,
            tradeMenuOpen,
            latestPlayerMsg,
            bubbleTimer,
            username,
            elapsedGameTime,
            level,
            exp,
            hungerMultiplier,
            thirstMultiplier,
            hunger,
            thirst,
            inventory,
            authManager,
            stockMarket,
            getItemPrice,
            getItemName,
            adjustMarketItem,
            quantityPromptOpen,
            quantityPromptMax,
            quantityPromptTargetSlot,
            quantityPromptSourceSlot,
            quantityPromptIsP2P,
            quantityInput,
            window,
            tradeConfirmOpen,
            tradeConfirmMessage,
            tradeConfirmIsP2P,
            disasterActive,
            currentDisasterType
        );
    }
    else if (wrenchMenuOpen) {
        clickHandled = handleWrenchMenuClicks(
            uiPos,
            isTargetNPC,
            wrenchMenuPos,
            wrenchMenuOpen,
            wrenchTargetPlayer,
            tradeMenuOpen,
            craftMenuOpen,
            stockPageOpen,
            iapPageOpen,
            playerTradeSlots,
            npcTradeSlots,
            hasBargained,
            bargainAccepted,
            bargainStatus,
            bargainColor,
            playerTradeGold,
            npcTradeGold,
            npcGoldInput,
            playerGoldInput,
            tradeBoxesTicked,
            qInfo,
            availableQuests,
            latestPlayerMsg,
            bubbleTimer,
            p2pTradeID,
            username,
            p2pTradeOpen,
            p2pAmInitiator,
            p2pPartner,
            p2pMyReady,
            p2pPartnerReady,
            p2pStatusMsg,
            myP2PSlots,
            myP2PGold,
            myP2PGoldInput,
            partnerItems,
            partnerGold,
            p2pPollTimer,
            authManager,
            assets,
            getItemPrice
        );
    }
    else if (saplingPlacementMode) {
        clickHandled = handleSaplingPlacementClicks(
            uiPos,
            worldPos,
            width,
            height,
            saplingPlacementMode,
            saplingPlacementPos,
            saplingRotation,
            saplingFollowingCursor,
            saplingFromInventorySlot,
            saplingTex,
            plantedSaplings,
            inventory,
            snapToGrid
        );
    }
    else if (placementMode) {
        clickHandled = handleStructurePlacementClicks(
            uiPos,
            worldPos,
            width,
            height,
            placementMode,
            placementItemID,
            placementFromInventorySlot,
            placementPos,
            placementRotation,
            placementFollowingCursor,
            username,
            campfires,
            structures,
            inventory,
            qInfo,
            tutorialActive,
            authManager,
            furnaceTex,
            woodTileTex,
            woodWallTex,
            woodDoorTex,
            woodRoofTex,
            woodStairsTex,
            bedTex,
            chestTex,
            syncSaveCallback,
            snapToGrid
        );
    }
    else if (craftMenuOpen) {
        clickHandled = handleCraftMenuClicks(
            uiPos,
            width,
            height,
            craftMenuOpen,
            craftableItems,
            inventory,
            swordCrafting,
            swordCraftTimer,
            shieldCrafting,
            shieldCraftTimer
        );
    }
    else if (!selectionDone)
    {
        if (characterSelection.isMaleButtonHovered(uiPos, uiPos))
        {
            option = 1;
            selectionDone = true;
            authManager.saveGender(username.c_str(), option);
            clickHandled = true;
        }
        else if (characterSelection.isFemaleButtonHovered(uiPos, uiPos))
        {
            option = 2;
            selectionDone = true;
            authManager.saveGender(username.c_str(), option);
            clickHandled = true;
        }
    }

    // Only allow inventory slot interaction when no major menu is blocking the screen
    bool isMenuBlockingInventory = (tradeMenuOpen || tradeConfirmOpen || wrenchMenuOpen
        || p2pTradeOpen || chestMenuOpen || anyCampfireMenuOpen || anyFurnaceMenuOpen2
        || quantityPromptOpen);
    if (!clickHandled && !isMenuBlockingInventory)
    {
        bool anyFurnaceMenuOpen = anyFurnaceMenuOpen2;
        clickHandled = handleInventoryInteractionClick(
            uiPos,
            mousePixelPos,
            window,
            inventory,
            inventoryOpen,
            tradeMenuOpen,
            p2pTradeOpen,
            campfires,
            saplingTex,
            campfireFrames,
            saplingPlacementMode,
            saplingFollowingCursor,
            saplingFromInventorySlot,
            saplingRotation,
            placementMode,
            placementFollowingCursor,
            placementItemID,
            placementFromInventorySlot,
            hunger,
            hungerMultiplier,
            maxHunger,
            MAX_MULTIPLIER,
            hp,
            disasterActive,
            anyFurnaceMenuOpen,
            chestMenuOpen,
            quantityPromptOpen
        );
    }
}

bool handleGlobalKeyEvents(
    const sf::Event& event,
    sf::RenderWindow& window,
    const GameSettings& globalSettings,
    bool& inventoryOpen,
    bool& invKeyHeld,
    bool tutorialActive,
    int tutorialStep,
    bool& tutInvOpened,
    bool& tradeMenuOpen,
    bool& tradeConfirmOpen,
    bool& tradeBoxesTicked,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    Inventory& inventory,
    AuthManager& authManager,
    const std::string& username,
    TradeSlot myP2PSlots[4],
    int p2pTradeID,
    float hunger,
    float thirst,
    int currency,
    int level,
    int exp,
    float elapsedGameTime,
    int hungerMultiplier,
    int thirstMultiplier,
    std::vector<PlacedStructure>& structures,
    std::vector<PlacedCampfire>& campfires,
    bool& chestMenuOpen,
    PlacedStructure*& activeChest,
    bool& wrenchMenuOpen,
    bool& stockPageOpen,
    bool& iapPageOpen,
    bool& iapVerifyingOpen,
    int& chestSelectedInvSlot,
    bool* isLoggingOutPtr,
    bool* logoutCloseWindowPtr
) {
    if (event.is<sf::Event::Closed>())
    {
        if (isLoggingOutPtr) {
            *isLoggingOutPtr = true;
            if (logoutCloseWindowPtr) *logoutCloseWindowPtr = true;
            return true;
        }
        saveAllGameDataAndClose(
            window,
            authManager,
            username,
            playerTradeSlots,
            myP2PSlots,
            p2pTradeID,
            inventory,
            hunger,
            thirst,
            currency,
            level,
            exp,
            elapsedGameTime,
            hungerMultiplier,
            thirstMultiplier,
            structures,
            campfires
        );
        return true;
    }

    if (auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if (key->scancode == sf::Keyboard::Scancode::Escape)
        {
            if (tradeMenuOpen || tradeConfirmOpen) {
                tradeConfirmOpen = false;
                tradeMenuOpen = false;
                tradeBoxesTicked = false;
                for (int i = 0; i < 4; i++) {
                    if (playerTradeSlots[i].tex != nullptr) {
                        inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
                        playerTradeSlots[i].tex = nullptr;
                        playerTradeSlots[i].count = 0;
                        playerTradeSlots[i].id = 0;
                    }
                    npcTradeSlots[i].tex = nullptr;
                    npcTradeSlots[i].count = 0;
                    npcTradeSlots[i].id = 0;
                }
                return true;
            }
            if (wrenchMenuOpen) {
                wrenchMenuOpen = false;
                return true;
            }
            if (stockPageOpen) {
                stockPageOpen = false;
                return true;
            }
            if (iapPageOpen || iapVerifyingOpen) {
                iapPageOpen = false;
                iapVerifyingOpen = false;
                return true;
            }
            if (chestMenuOpen) {
                chestMenuOpen = false;
                activeChest = nullptr;
                chestSelectedInvSlot = -1;
                return true;
            }
            bool closedSubMenu = false;
            for (auto& f : campfires) {
                if (f.menuOpen) { f.menuOpen = false; closedSubMenu = true; }
            }
            for (auto& s : structures) {
                if (s.id == ITEM_FURNACE && s.menuOpen) { s.menuOpen = false; closedSubMenu = true; }
            }
            // ESC only closes open menus — use the Logout button to save and exit
            return closedSubMenu;
        }

        if (key->scancode == globalSettings.keyInventory && !invKeyHeld)
        {
            inventoryOpen = !inventoryOpen;

            if (inventoryOpen && tutorialActive && tutorialStep == 3)
                tutInvOpened = true;

            invKeyHeld = true;
            return true;
        }
    }

    if (auto* key = event.getIf<sf::Event::KeyReleased>())
    {
        if (key->scancode == globalSettings.keyInventory)
        {
            invKeyHeld = false;
            return true;
        }
    }

    return false;
}







// =========================================================
// ADMIN BAN / MUTE REASON PROMPT IMPLEMENTATION
// =========================================================

void drawAdminActionPrompt(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    bool isBan,
    const std::string& targetUser,
    const std::string& durationStr,
    int muteMinutes,
    TextInput& reasonInput
) {
    sf::View origView = window.getView();
    window.setView(GetUIView(&window));

    const float uiW = 1920.f;
    const float uiH = 1080.f;

    // Fullscreen dimmed backdrop
    sf::RectangleShape backdrop({ uiW, uiH });
    backdrop.setFillColor(sf::Color(0, 0, 0, 185));
    window.draw(backdrop);

    const float boxW = 540.f;
    const float boxH = 290.f;
    const float boxX = (uiW - boxW) / 2.f;
    const float boxY = (uiH - boxH) / 2.f;

    // Shadow
    sf::RectangleShape shadow({ boxW + 8.f, boxH + 8.f });
    shadow.setPosition({ boxX - 4.f, boxY + 4.f });
    shadow.setFillColor(sf::Color(0, 0, 0, 130));
    window.draw(shadow);

    // Box body
    sf::RectangleShape box({ boxW, boxH });
    box.setPosition({ boxX, boxY });
    box.setFillColor(sf::Color(24, 26, 32, 252));
    box.setOutlineThickness(3.f);
    box.setOutlineColor(isBan ? sf::Color(220, 50, 50) : sf::Color(240, 160, 40));
    window.draw(box);

    // Header bar
    sf::RectangleShape header({ boxW, 50.f });
    header.setPosition({ boxX, boxY });
    header.setFillColor(isBan ? sf::Color(75, 20, 20) : sf::Color(75, 48, 15));
    window.draw(header);

    sf::RectangleShape divH({ boxW, 2.f });
    divH.setPosition({ boxX, boxY + 50.f });
    divH.setFillColor(isBan ? sf::Color(220, 50, 50) : sf::Color(240, 160, 40));
    window.draw(divH);

    // Header title
    std::string titleStr = isBan ? "ADMIN BAN PROMPT" : "ADMIN MUTE PROMPT";
    sf::Text title(uiFont, titleStr, 22);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(isBan ? sf::Color(255, 90, 90) : sf::Color(255, 190, 60));
    auto tb = title.getLocalBounds();
    title.setPosition({ boxX + (boxW - tb.size.x) / 2.f, boxY + 12.f });
    window.draw(title);

    // Target info & Duration
    std::string durText;
    if (isBan) {
        durText = (durationStr == "perm" || durationStr == "permanent") ? "Permanent" : (durationStr + " Days");
    } else {
        durText = std::to_string(muteMinutes) + " Minutes";
    }

    std::string infoStr = "Target Player: " + targetUser + "    |    Valid Until: " + durText;
    sf::Text infoTxt(uiFont, infoStr, 14);
    infoTxt.setStyle(sf::Text::Bold);
    infoTxt.setFillColor(sf::Color(230, 230, 230));
    infoTxt.setPosition({ boxX + 30.f, boxY + 68.f });
    window.draw(infoTxt);

    // Instruction label
    sf::Text promptLbl(uiFont, "Write reason below (optional, left blank will be NULL):", 13);
    promptLbl.setFillColor(sf::Color(180, 185, 200));
    promptLbl.setPosition({ boxX + 30.f, boxY + 98.f });
    window.draw(promptLbl);

    // Reason Text Input Box
    reasonInput.setPosition({ boxX + 30.f, boxY + 130.f });
    reasonInput.setFocus(true);
    reasonInput.draw(window);

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    // Confirm button
    const float btnW = 160.f;
    const float btnH = 42.f;
    const float btnY = boxY + boxH - 62.f;
    sf::FloatRect confirmRect({ boxX + 60.f, btnY }, { btnW, btnH });
    bool confirmHov = confirmRect.contains(mousePos);
    sf::RectangleShape confirmBtn({ btnW, btnH });
    confirmBtn.setPosition({ boxX + 60.f, btnY });
    if (isBan) {
        confirmBtn.setFillColor(confirmHov ? sf::Color(210, 50, 50) : sf::Color(160, 35, 35));
        confirmBtn.setOutlineColor(confirmHov ? sf::Color(255, 120, 120) : sf::Color(200, 50, 50));
    } else {
        confirmBtn.setFillColor(confirmHov ? sf::Color(220, 140, 30) : sf::Color(170, 100, 20));
        confirmBtn.setOutlineColor(confirmHov ? sf::Color(255, 190, 70) : sf::Color(210, 130, 30));
    }
    confirmBtn.setOutlineThickness(1.5f);
    window.draw(confirmBtn);

    std::string confirmLabel = isBan ? "Confirm Ban" : "Confirm Mute";
    sf::Text confirmTxt(uiFont, confirmLabel, 15);
    confirmTxt.setStyle(sf::Text::Bold);
    confirmTxt.setFillColor(sf::Color::White);
    auto cb = confirmTxt.getLocalBounds();
    confirmTxt.setPosition({ boxX + 60.f + (btnW - cb.size.x) / 2.f, btnY + (btnH - cb.size.y) / 2.f - 4.f });
    window.draw(confirmTxt);

    // Cancel button
    sf::FloatRect cancelRect({ boxX + boxW - 60.f - btnW, btnY }, { btnW, btnH });
    bool cancelHov = cancelRect.contains(mousePos);
    sf::RectangleShape cancelBtn({ btnW, btnH });
    cancelBtn.setPosition({ boxX + boxW - 60.f - btnW, btnY });
    cancelBtn.setFillColor(cancelHov ? sf::Color(100, 100, 110) : sf::Color(70, 70, 80));
    cancelBtn.setOutlineThickness(1.5f);
    cancelBtn.setOutlineColor(cancelHov ? sf::Color(160, 160, 175) : sf::Color(110, 110, 120));
    window.draw(cancelBtn);

    sf::Text cancelTxt(uiFont, "Cancel", 15);
    cancelTxt.setStyle(sf::Text::Bold);
    cancelTxt.setFillColor(sf::Color::White);
    auto cxb = cancelTxt.getLocalBounds();
    cancelTxt.setPosition({ boxX + boxW - 60.f - btnW + (btnW - cxb.size.x) / 2.f, btnY + (btnH - cxb.size.y) / 2.f - 4.f });
    window.draw(cancelTxt);

    window.setView(origView);
}

bool handleAdminPromptEvent(
    const sf::Event& event,
    sf::RenderWindow& window,
    bool& adminPromptOpen,
    bool adminPromptIsBan,
    const std::string& adminPromptTarget,
    const std::string& adminPromptDuration,
    int adminPromptMuteMinutes,
    TextInput& adminReasonInput,
    AuthManager& authManager,
    const std::string& currentUsername,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    sf::Clock& adminPromptClock
) {
    if (!adminPromptOpen) return false;

    // Ignore events in the first 0.35s after opening so the Enter/click that opened it doesn't immediately close it!
    float elapsedSinceOpen = adminPromptClock.getElapsedTime().asSeconds();
    if (elapsedSinceOpen < 0.35f) {
        return true;
    }

    auto executeAction = [&]() {
        std::string reason = adminReasonInput.getText();
        while (!reason.empty() && (reason.front() == ' ' || reason.front() == '\t')) reason.erase(reason.begin());
        while (!reason.empty() && (reason.back() == ' ' || reason.back() == '\t')) reason.pop_back();

        std::string feedback;
        if (adminPromptIsBan) {
            authManager.banUser(adminPromptTarget, adminPromptDuration, feedback, reason, currentUsername);
        } else {
            authManager.muteUser(adminPromptTarget, adminPromptMuteMinutes, feedback, reason, currentUsername);
        }
        latestPlayerMsg = feedback;
        bubbleTimer = 6.f;
        adminPromptOpen = false;
        adminReasonInput.clear();
    };

    auto cancelAction = [&]() {
        adminPromptOpen = false;
        adminReasonInput.clear();
        latestPlayerMsg = "Admin action cancelled.";
        bubbleTimer = 3.f;
    };

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    if (auto* textEvent = event.getIf<sf::Event::TextEntered>()) {
        if (textEvent->unicode == 27) {
            cancelAction();
            return true;
        }
        if (textEvent->unicode == 13 || (textEvent->unicode < 32 && textEvent->unicode != 8)) {
            return true; // Ignore control characters and carriage returns in TextEntered
        }
        adminReasonInput.processEvent(event, mousePos);
        return true;
    }

    if (auto* keyEvent = event.getIf<sf::Event::KeyPressed>()) {
        if (keyEvent->code == sf::Keyboard::Key::Enter || keyEvent->scancode == sf::Keyboard::Scancode::Enter ||
            keyEvent->scancode == sf::Keyboard::Scancode::NumpadEnter) {
            executeAction();
            return true;
        }
        if (keyEvent->code == sf::Keyboard::Key::Escape || keyEvent->scancode == sf::Keyboard::Scancode::Escape) {
            cancelAction();
            return true;
        }
        adminReasonInput.processEvent(event, mousePos);
        return true;
    }

    if (auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            const float uiW = 1920.f;
            const float uiH = 1080.f;
            const float boxW = 540.f;
            const float boxH = 290.f;
            const float boxX = (uiW - boxW) / 2.f;
            const float boxY = (uiH - boxH) / 2.f;
            const float btnW = 160.f;
            const float btnH = 42.f;
            const float btnY = boxY + boxH - 62.f;

            sf::FloatRect confirmRect({ boxX + 60.f, btnY }, { btnW, btnH });
            sf::FloatRect cancelRect({ boxX + boxW - 60.f - btnW, btnY }, { btnW, btnH });

            if (confirmRect.contains(mousePos)) {
                executeAction();
                return true;
            }
            if (cancelRect.contains(mousePos)) {
                cancelAction();
                return true;
            }

            adminReasonInput.processEvent(event, mousePos);
            return true;
        }
    }

    return true;
}
