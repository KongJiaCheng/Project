#include "GameInput.h"
#include "ViewUtils.h"
#include "ItemID.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "SheepMeat.h"
#include "Server.h"
#include "Game.h"

bool handleQuantityPromptEvent(
    const sf::Event& event,
    sf::RenderWindow& window,
    bool& quantityPromptOpen,
    TextInput& quantityInput,
    bool quantityPromptIsCampfire,
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
    std::function<void()> syncSaveCallback
) {
    if (!quantityPromptOpen) return false;

    quantityInput.processEvent(event, window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window)));

    if (auto* textEvent = event.getIf<sf::Event::TextEntered>()) {
        if (textEvent->unicode == 13) { // Enter pressed
            int amt = 1;
            try {
                std::string text = quantityInput.getText();
                if (!text.empty()) amt = std::stoi(text);
            } catch (...) {
                amt = 1;
            }
            if (amt < 1) amt = 1;

            if (quantityPromptIsCampfire) {
                PlacedCampfire* activeFire = nullptr;
                for (auto& f : campfires) {
                    if (f.menuOpen) {
                        activeFire = &f;
                        break;
                    }
                }
                if (activeFire) {
                    int itemID = inventory.getItemID(quantityPromptSourceSlot);
                    if (quantityPromptCampfireSlot == 1) { // Fuel
                        if (itemID == ITEM_WOOD) {
                            activeFire->fuel += amt * 4;
                        } else if (itemID == ITEM_SAPLING) {
                            activeFire->fuel += amt;
                            saplingPlacementMode = false;
                        }
                    } else if (quantityPromptCampfireSlot == 2) { // Raw Food
                        activeFire->inputItem = itemID;
                        activeFire->inputCount += amt;
                    }
                    for (int c = 0; c < amt; c++) {
                        inventory.consumeItem(quantityPromptSourceSlot);
                    }
                    if (syncSaveCallback) syncSaveCallback();
                }
            } else if (quantityPromptIsP2P) {
                myP2PSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                myP2PSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                myP2PSlots[quantityPromptTargetSlot].count = amt;
                myP2PSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                p2pOfferDirty = true; p2pMyReady = false;
            } else {
                playerTradeSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                playerTradeSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                playerTradeSlots[quantityPromptTargetSlot].count = amt;
                playerTradeSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                isSellingToNPC = true;

                npcTradeGold = 0;
                for (int j = 0; j < 4; j++) {
                    if (playerTradeSlots[j].tex != nullptr) {
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
            quantityPromptOpen = false;
            return true;
        }
    }

    if (auto* mouseButtonEvent = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouseButtonEvent->button == sf::Mouse::Button::Left) {
            sf::Vector2f uiPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
            auto sz = window.getSize();
            sf::Vector2f qBoxPos = { ((float)sz.x - 400.f) / 2.f, ((float)sz.y - 220.f) / 2.f };
            sf::FloatRect confirmBtnRect({ qBoxPos.x + 30.f, qBoxPos.y + 150.f }, { 160.f, 40.f });
            sf::FloatRect cancelBtnRect({ qBoxPos.x + 210.f, qBoxPos.y + 150.f }, { 160.f, 40.f });

            if (confirmBtnRect.contains(uiPos)) {
                int amt = 1;
                try {
                    std::string text = quantityInput.getText();
                    if (!text.empty()) amt = std::stoi(text);
                } catch (...) {
                    amt = 1;
                }
                if (amt < 1) amt = 1;

                if (quantityPromptIsCampfire) {
                    PlacedCampfire* activeFire = nullptr;
                    for (auto& f : campfires) {
                        if (f.menuOpen) {
                            activeFire = &f;
                            break;
                        }
                    }
                    if (activeFire) {
                        int itemID = inventory.getItemID(quantityPromptSourceSlot);
                        if (quantityPromptCampfireSlot == 1) { // Fuel
                            if (itemID == ITEM_WOOD) {
                                activeFire->fuel += amt * 4;
                            } else if (itemID == ITEM_SAPLING) {
                                activeFire->fuel += amt;
                                saplingPlacementMode = false;
                            }
                        } else if (quantityPromptCampfireSlot == 2) { // Raw Food
                            activeFire->inputItem = itemID;
                            activeFire->inputCount += amt;
                        }
                        for (int c = 0; c < amt; c++) {
                            inventory.consumeItem(quantityPromptSourceSlot);
                        }
                        if (syncSaveCallback) syncSaveCallback();
                    }
                } else if (quantityPromptIsP2P) {
                    myP2PSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                    myP2PSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                    myP2PSlots[quantityPromptTargetSlot].count = amt;
                    myP2PSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                    for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                    p2pOfferDirty = true; p2pMyReady = false;
                } else {
                    playerTradeSlots[quantityPromptTargetSlot].id = inventory.getItemID(quantityPromptSourceSlot);
                    playerTradeSlots[quantityPromptTargetSlot].tex = inventory.getItemTexture(quantityPromptSourceSlot);
                    playerTradeSlots[quantityPromptTargetSlot].count = amt;
                    playerTradeSlots[quantityPromptTargetSlot].originalSlot = quantityPromptSourceSlot;
                    for (int c = 0; c < amt; c++) inventory.consumeItem(quantityPromptSourceSlot);
                    isSellingToNPC = true;

                    npcTradeGold = 0;
                    for (int j = 0; j < 4; j++) {
                        if (playerTradeSlots[j].tex != nullptr) {
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
                quantityPromptOpen = false;
            } else if (cancelBtnRect.contains(uiPos)) {
                quantityPromptOpen = false;
            }
            return true;
        }
    }

    return true;
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
    std::function<void()> syncSaveCallback
) {
    for (auto& fire : campfires) {
        if (fire.menuOpen) {
            sf::Vector2f menuPos(
                (float)width / 2.f - 250.f,
                (float)height / 2.f - 150.f
            );
            
            sf::FloatRect closeBtn(
                { menuPos.x + 440.f, menuPos.y + 10.f },
                { 40.f, 40.f }
            );
            sf::FloatRect fuelBtn(
                { menuPos.x + 60.f, menuPos.y + 110.f },
                { 80.f, 80.f }
            );
            sf::FloatRect cookBtn(
                { menuPos.x + 210.f, menuPos.y + 110.f },
                { 80.f, 80.f }
            );
            sf::FloatRect takeBtn(
                { menuPos.x + 360.f, menuPos.y + 110.f },
                { 80.f, 80.f }
            );
            
            if (closeBtn.contains(uiPos)) {
                fire.menuOpen = false;
                return true;
            }
            else if (fuelBtn.contains(uiPos)) {
                if (fire.fuel > 0) {
                    int woodReturn = fire.fuel / 4;
                    int saplingReturn = fire.fuel % 4;
                    if (woodReturn > 0) {
                        inventory.addItem(ITEM_WOOD, woodTex, woodReturn);
                    }
                    if (saplingReturn > 0) {
                        inventory.addItem(ITEM_SAPLING, saplingTex, saplingReturn);
                    }
                    fire.fuel = 0;
                    if (syncSaveCallback) syncSaveCallback();
                } else {
                    int selSlot = inventory.getSelectedSlot();
                    if (selSlot != -1 && inventory.hasItem(selSlot)) {
                        int itemID = inventory.getItemID(selSlot);
                        if (itemID == ITEM_WOOD || itemID == ITEM_SAPLING) {
                            int totalQty = inventory.getItemCount(selSlot);
                            if (totalQty > 1) {
                                quantityPromptOpen = true;
                                quantityPromptMax = totalQty;
                                quantityPromptTargetSlot = 0;
                                quantityPromptSourceSlot = selSlot;
                                quantityPromptIsP2P = false;
                                quantityPromptIsCampfire = true;
                                quantityPromptCampfireSlot = 1;
                                quantityInput.clear();
                                quantityInput.setFocus(true);
                            } else {
                                if (itemID == ITEM_WOOD) {
                                    fire.fuel += 4;
                                } else {
                                    fire.fuel += 1;
                                    if (itemID == ITEM_SAPLING) {
                                        saplingPlacementMode = false;
                                    }
                                }
                                inventory.consumeItem(selSlot);
                                if (syncSaveCallback) syncSaveCallback();
                            }
                        }
                    }
                }
                return true;
            }
            else if (cookBtn.contains(uiPos)) {
                if (fire.inputCount > 0 && fire.inputItem != 0) {
                    const sf::Texture* rTex = (fire.inputItem == ITEM_CHICKEN_MEAT) ? ChickenMeat::getTexture() : 
                                              (fire.inputItem == ITEM_PIG_MEAT) ? PigMeat::getTexture() : 
                                              (fire.inputItem == ITEM_SHEEP_MEAT) ? SheepMeat::getTexture() : nullptr;
                    if (rTex) {
                        inventory.addItem(fire.inputItem, *rTex, fire.inputCount);
                    }
                    fire.inputCount = 0;
                    fire.inputItem = 0;
                    if (syncSaveCallback) syncSaveCallback();
                } else {
                    int selSlot = inventory.getSelectedSlot();
                    if (selSlot != -1 && inventory.hasItem(selSlot)) {
                        int itemID = inventory.getItemID(selSlot);
                        if (itemID == ITEM_CHICKEN_MEAT || itemID == ITEM_PIG_MEAT || itemID == ITEM_SHEEP_MEAT) {
                            int totalQty = inventory.getItemCount(selSlot);
                            if (totalQty > 1) {
                                quantityPromptOpen = true;
                                quantityPromptMax = totalQty;
                                quantityPromptTargetSlot = 0;
                                quantityPromptSourceSlot = selSlot;
                                quantityPromptIsP2P = false;
                                quantityPromptIsCampfire = true;
                                quantityPromptCampfireSlot = 2;
                                quantityInput.clear();
                                quantityInput.setFocus(true);
                            } else {
                                fire.inputItem = itemID;
                                fire.inputCount = 1;
                                inventory.consumeItem(selSlot);
                                if (syncSaveCallback) syncSaveCallback();
                            }
                        }
                    }
                }
                return true;
            }
            else if (takeBtn.contains(uiPos)) {
                if (fire.outputCount > 0) {
                    if (fire.outputItem == ITEM_COOKED_CHICKEN) {
                        inventory.addItem(ITEM_COOKED_CHICKEN, cookedChickenTex, fire.outputCount);
                    }
                    else if (fire.outputItem == ITEM_COOKED_PIG) {
                        inventory.addItem(ITEM_COOKED_PIG, cookedPigTex, fire.outputCount);
                    }
                    else if (fire.outputItem == ITEM_COOKED_SHEEP) {
                        inventory.addItem(ITEM_COOKED_SHEEP, cookedSheepTex, fire.outputCount);
                    }
                    fire.outputCount = 0;
                    fire.outputItem = 0;
                    if (syncSaveCallback) syncSaveCallback();
                }
                return true;
            }
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
    bool& tutChatted
) {
    if (msg.empty()) return false;

    if (msg.find("/ban ") == 0) {
        std::string rest = msg.substr(5);
        size_t spacePos = rest.find(' ');
        if (spacePos == std::string::npos) {
            latestPlayerMsg = "Usage: /ban <username> <3|7|14|30|perm>";
            bubbleTimer = 5.f;
        }
        else {
            std::string target = rest.substr(0, spacePos);
            std::string duration = rest.substr(spacePos + 1);
            std::string executorRole = authManager.getUserRole(username.c_str());
            if (executorRole != "moderator") {
                latestPlayerMsg = "Error: Only moderators can ban.";
                bubbleTimer = 5.f;
            }
            else {
                std::string feedback;
                authManager.banUser(target, duration, feedback);
                latestPlayerMsg = feedback;
                bubbleTimer = 5.f;
            }
        }
    }
    else if (msg.find("/unban ") == 0) {
        std::string target = msg.substr(7);
        std::string executorRole = authManager.getUserRole(username.c_str());
        if (executorRole != "moderator") {
            latestPlayerMsg = "Error: Only moderators can unban.";
            bubbleTimer = 5.f;
        }
        else {
            std::string feedback;
            authManager.unbanUser(target, feedback);
            latestPlayerMsg = feedback;
            bubbleTimer = 5.f;
        }
    }
    else if (msg.find("/mute ") == 0) {
        std::string rest = msg.substr(6);
        size_t spacePos = rest.find(' ');
        std::string target = rest;
        int minutes = 60;
        if (spacePos != std::string::npos) {
            target = rest.substr(0, spacePos);
            try {
                minutes = std::stoi(rest.substr(spacePos + 1));
            }
            catch (...) {
                minutes = 60;
            }
        }
        std::string executorRole = authManager.getUserRole(username.c_str());
        if (executorRole != "moderator") {
            latestPlayerMsg = "Error: Only moderators can mute.";
            bubbleTimer = 5.f;
        }
        else {
            std::string feedback;
            authManager.muteUser(target, minutes, feedback);
            latestPlayerMsg = feedback;
            bubbleTimer = 5.f;
        }
    }
    else if (msg.find("/unmute ") == 0) {
        std::string target = msg.substr(8);
        std::string executorRole = authManager.getUserRole(username.c_str());
        if (executorRole != "moderator") {
            latestPlayerMsg = "Error: Only moderators can unmute.";
            bubbleTimer = 5.f;
        }
        else {
            std::string feedback;
            authManager.unmuteUser(target, feedback);
            latestPlayerMsg = feedback;
            bubbleTimer = 5.f;
        }
    }
    else if (msg == "/makemoderator") {
        if (authManager.setUserRole(username.c_str(), "moderator")) {
            latestPlayerMsg = "You are now a moderator!";
        }
        else {
            latestPlayerMsg = "Failed to set role.";
        }
        bubbleTimer = 5.f;
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
        if (executorRole == "moderator") {
            latestPlayerMsg = "Commands:\n/pull (name) - Pull player\n/who - See online players\n/ban (name) (3|7|14|30|perm)\n/unban (name)\n/mute (name) [minutes]\n/unmute (name)\n/makemoderator\n/help - Show commands";
        }
        else {
            latestPlayerMsg = "Commands:\n/pull (name) - Pull player\n/who - See online players\n/help - Show commands";
        }
        bubbleTimer = 10.f;
    }
    else {
        std::string chatErr;
        if (authManager.sendChatMessage(username.c_str(), msg.c_str(), chatErr)) {
            if (tutorialActive && tutorialStep == 6) tutChatted = true;
            latestPlayerMsg = msg;
            bubbleTimer = 5.0f;
        }
        else {
            latestPlayerMsg = chatErr.empty() ? "Failed to send message." : chatErr;
            bubbleTimer = 5.0f;
        }
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
    bool& npcGoldTyping
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
            p2pOfferPushTimer = 0.2f;
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
                            tutChatted
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
        else if (key->scancode == sf::Keyboard::Scancode::Escape && (isChatting || chatActive)) {
            chatActive = false;
            isChatting = false;
            chatInput.setFocus(false);
            chatInput.clear();
            return true;
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

void handleMouseDragAndScroll(
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
    const std::string& username
) {
    if (auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (wheel->wheel == sf::Mouse::Wheel::Vertical) {
            sf::Vector2f uiPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
            if (sf::FloatRect({ chatBoxX, chatBoxY }, { chatBoxWidth, chatBoxHeight }).contains(uiPos)) {
                chatScroll -= wheel->delta * 30.f;
                if (chatScroll < 0.f) chatScroll = 0.f;
            }
            sf::FloatRect sBoxRect({ (width - 900.f) / 2.f, (height - 600.f) / 2.f }, { 900.f, 600.f });
            if (stockPageOpen && sBoxRect.contains(uiPos)) {
                marketScrollOffset -= (int)wheel->delta;
                if (marketScrollOffset < 0) marketScrollOffset = 0;
            }
        }
    }

    if (auto* mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouse->button == sf::Mouse::Button::Left) {
            for (auto& fire : campfires)
            {
                sf::FloatRect fireBounds({
                    fire.position.x - 50.f,
                    fire.position.y - 50.f },
                    { 100.f,
                    100.f }
                );

                if (fireBounds.contains(worldPos))
                {
                    for (int i = 0; i < 20; i++)
                    {
                        if (inventory.hasItem(i))
                        {
                            if (inventory.getItemID(i) == ITEM_WRENCH)
                            {
                                if (fire.owner.empty() || fire.owner == username)
                                    fire.menuOpen = true;
                            }
                        }
                    }
                }
            }

            sf::Vector2f uiPos = window.mapPixelToCoords(mouse->position, GetUIView(&window));
            if (sf::FloatRect({ chatBoxX, chatBoxY }, { chatBoxWidth, chatBoxHeight }).contains(uiPos)) {
                isDraggingChat = true;
                lastMousePosChat = uiPos;
            }
        }
    }

    if (auto* mouse = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (mouse->button == sf::Mouse::Button::Left) {
            isDraggingChat = false;
        }
    }

    if (auto* move = event.getIf<sf::Event::MouseMoved>()) {
        if (isDraggingChat) {
            sf::Vector2f uiPos = window.mapPixelToCoords(move->position, GetUIView(&window));
            chatScroll -= (uiPos.y - lastMousePosChat.y);
            if (chatScroll < 0.f) chatScroll = 0.f;
            lastMousePosChat = uiPos;
        }
    }
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
    bool& isTargetNPC,
    std::string& wrenchTargetPlayer,
    sf::Vector2f& wrenchMenuPos,
    const std::vector<OnlinePlayer>& otherPlayers,
    const std::map<std::string, sf::Vector2f>& onlinePlayerDisplayPos,
    const std::map<std::string, sf::FloatRect>& onlinePlayerWrenchBounds,
    const sf::Sprite& playerWrenchBtn,
    const NPC& npcFemale,
    const NPC& npcMale
) {
    sf::FloatRect settingsBtnRect({ (float)width - 140.f, 20.f }, { 120.f, 40.f });
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && settingsBtnRect.contains(uiPos)) {
        openSettingsWindow(window, width, height);
        deltaClock.restart();
        return true;
    }

    sf::FloatRect marketBtnRect({ (float)width - 300.f, 20.f }, { 150.f, 40.f });
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen && marketBtnRect.contains(uiPos)) {
        stockPageOpen = true;
        return true;
    }

    // Online player click -> open wrench with "Trade" option
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !p2pTradeOpen) {
        for (const auto& op : otherPlayers) {
            auto dispIt = onlinePlayerDisplayPos.find(op.username);
            sf::Vector2f opPos = (dispIt != onlinePlayerDisplayPos.end()) ? dispIt->second : sf::Vector2f(op.x, op.y);
            sf::FloatRect opBounds({ opPos.x - 30.f, opPos.y - 50.f }, { 60.f, 100.f });
            bool hitWrench = false;
            {
                auto wbIt = onlinePlayerWrenchBounds.find(op.username);
                if (wbIt != onlinePlayerWrenchBounds.end())
                    hitWrench = wbIt->second.contains(worldPos);
            }
            if (opBounds.contains(worldPos) || hitWrench) {
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
    if (!wrenchMenuOpen && !craftMenuOpen && !placementMode && !tradeMenuOpen && !tradeConfirmOpen) {
        // Player wrench button (generous clickable area)
        sf::FloatRect pb = playerWrenchBtn.getGlobalBounds();
        sf::FloatRect pbHit({ pb.position.x - 25.f, pb.position.y - 25.f }, { std::max(60.f, pb.size.x + 50.f), std::max(60.f, pb.size.y + 50.f) });
        if (pbHit.contains(worldPos)) {
            wrenchMenuOpen = true;
            isTargetNPC = false;
            wrenchTargetPlayer = "";
            wrenchMenuPos = uiPos;
            wrenchMenuPos.x = std::clamp(wrenchMenuPos.x, 10.f, 1920.f - 170.f);
            wrenchMenuPos.y = std::clamp(wrenchMenuPos.y, 10.f, 1080.f - 140.f);
            return true;
        }

        // Female NPC (wrench button or NPC body)
        bool hitFemale = false;
        if (npcFemale.wrenchBtn.has_value()) {
            sf::FloatRect fb = npcFemale.wrenchBtn->getGlobalBounds();
            sf::FloatRect fbHit({ fb.position.x - 25.f, fb.position.y - 25.f }, { std::max(60.f, fb.size.x + 50.f), std::max(60.f, fb.size.y + 50.f) });
            hitFemale = fbHit.contains(worldPos);
        }
        if (!hitFemale && npcFemale.sprite.has_value()) {
            hitFemale = npcFemale.sprite->getGlobalBounds().contains(worldPos);
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

        // Male NPC (wrench button or NPC body)
        bool hitMale = false;
        if (npcMale.wrenchBtn.has_value()) {
            sf::FloatRect mb = npcMale.wrenchBtn->getGlobalBounds();
            sf::FloatRect mbHit({ mb.position.x - 25.f, mb.position.y - 25.f }, { std::max(60.f, mb.size.x + 50.f), std::max(60.f, mb.size.y + 50.f) });
            hitMale = mbHit.contains(worldPos);
        }
        if (!hitMale && npcMale.sprite.has_value()) {
            hitMale = npcMale.sprite->getGlobalBounds().contains(worldPos);
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
    TextInput& quantityInput,
    QuestInfo& qInfo,
    const std::vector<DailyQuest>& availableQuests,
    float& p2pPollTimer,
    const GameAssets& assets,
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
    float& hp
) {
    bool clickHandled = false;
    playerGoldTyping = false;
    p2pGoldTyping = false;

    if (stockPageOpen) {
        sf::FloatRect sBoxRect({ (width - 900.f) / 2.f, (height - 600.f) / 2.f }, { 900.f, 600.f });
        sf::FloatRect closeBtnRect({ sBoxRect.position.x + 840.f, sBoxRect.position.y + 20.f }, { 40.f, 40.f });
        sf::FloatRect searchBarRect({ sBoxRect.position.x + 40.f, sBoxRect.position.y + 65.f }, { 680.f, 32.f });
        if (closeBtnRect.contains(uiPos)) {
            stockPageOpen = false;
            marketSearchText = "";
            marketSearchActive = false;
            marketScrollOffset = 0;
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
        isTargetNPC,
        wrenchTargetPlayer,
        wrenchMenuPos,
        otherPlayers,
        onlinePlayerDisplayPos,
        onlinePlayerWrenchBounds,
        playerWrenchBtn,
        npcFemale,
        npcMale
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
    }
    else if (p2pTradeOpen) {
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
            window
        );
    }
    else if (tradeConfirmOpen) {
        clickHandled = handleNPCTradeConfirmClicks(
            uiPos,
            width,
            height,
            tradeConfirmOpen,
            tradeMenuOpen,
            tradeBoxesTicked,
            playerTradeSlots,
            npcTradeSlots,
            playerTradeGold,
            npcTradeGold,
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
            stockMarket
        );
    }
    else if (tradeMenuOpen) {
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
            window
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

    if (!clickHandled)
    {
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
            hp
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
    std::vector<PlacedCampfire>& campfires
) {
    if (event.is<sf::Event::Closed>())
    {
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





