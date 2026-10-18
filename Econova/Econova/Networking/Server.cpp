#include "Server.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>

TimedGetMaxMessageID timedGetMaxMessageID = nullptr;
TimedGetQuestInfo timedGetQuestInfo = nullptr;
TimedLoadMarketPrice timedLoadMarketPrice = nullptr;
TimedLoadAllMarketPrices timedLoadAllMarketPrices = nullptr;

PlayerStats constructServer(AuthManager &authManager, bool dbStartupEnabled, const std::string& username, float STARTUP_DB_TIMEOUT_SECS) {
    auto timedLoadPlayerStats = [&](const std::string& uname) -> PlayerStats {
        if (!dbStartupEnabled) {
            return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };
        }
        struct SharedState {
            PlayerStats value{ 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };
            std::atomic<bool> done{ false };
            std::mutex mutex;
        };
        auto state = std::make_shared<SharedState>();
        std::thread([&authManager, state, uname]() {
            PlayerStats local = authManager.loadPlayerStats(uname.c_str());
            {
                std::lock_guard<std::mutex> lk(state->mutex);
                state->value = local;
            }
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < STARTUP_DB_TIMEOUT_SECS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] loadPlayerStats timed out. Using defaults.\n";
            return { 0.f, 0.f, 0, 1, 0, 0.f, -1, false, 1, 1 };
        }
        std::lock_guard<std::mutex> lk(state->mutex);
        return state->value;
    };

    timedGetMaxMessageID = [&authManager, STARTUP_DB_TIMEOUT_SECS]() -> int {
        struct SharedState {
            int value = 0;
            std::atomic<bool> done{ false };
        };

        auto state = std::make_shared<SharedState>();

        std::thread([&authManager, state]() {
            try {
                state->value = authManager.getMaxMessageID();
            }
            catch (...) {
                state->value = 0;
            }
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < STARTUP_DB_TIMEOUT_SECS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] getMaxMessageID timed out. Using fallback 0.\n";
            return 0;
        }
        return state->value;
    };

    timedGetQuestInfo = [&authManager, dbStartupEnabled, STARTUP_DB_TIMEOUT_SECS](const std::string& uname) -> QuestInfo {
        if (!dbStartupEnabled) {
            return { 0, 0, "2000-01-01", false };
        }
        struct SharedState {
            QuestInfo value{ 0, 0, "2000-01-01", false };
            std::atomic<bool> done{ false };
            std::mutex mutex;
        };
        auto state = std::make_shared<SharedState>();
        std::thread([&authManager, state, uname]() {
            QuestInfo local = authManager.getQuestInfo(uname.c_str());
            {
                std::lock_guard<std::mutex> lk(state->mutex);
                state->value = local;
            }
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < STARTUP_DB_TIMEOUT_SECS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] getQuestInfo timed out. Using fallback quest info.\n";
            return { 0, 0, "2000-01-01", false };
        }
        std::lock_guard<std::mutex> lk(state->mutex);
        return state->value;
    };

    timedLoadMarketPrice = [&authManager, dbStartupEnabled, STARTUP_DB_TIMEOUT_SECS](int itemId, float fallback) -> float {
        if (!dbStartupEnabled) {
            return fallback;
        }
        struct SharedState {
            float value = 0.f;
            std::atomic<bool> done{ false };
        };
        auto state = std::make_shared<SharedState>();
        std::thread([&authManager, state, itemId]() {
            state->value = authManager.loadMarketPrice(itemId);
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < STARTUP_DB_TIMEOUT_SECS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] loadMarketPrice timed out for item " << itemId << ".\n";
            return fallback;
        }
        return state->value;
    };

    timedLoadAllMarketPrices = [&authManager, dbStartupEnabled, STARTUP_DB_TIMEOUT_SECS]() -> std::map<int, float> {
        if (!dbStartupEnabled) {
            return {};
        }
        struct SharedState {
            std::map<int, float> prices;
            std::atomic<bool> done{ false };
            std::mutex mutex;
        };
        auto state = std::make_shared<SharedState>();
        std::thread([&authManager, state]() {
            auto p = authManager.loadAllMarketPrices();
            {
                std::lock_guard<std::mutex> lk(state->mutex);
                state->prices = std::move(p);
            }
            state->done = true;
            }).detach();

        sf::Clock waitClock;
        while (!state->done.load() && waitClock.getElapsedTime().asSeconds() < 2.0f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!state->done.load()) {
            std::cerr << "[WARN] loadAllMarketPrices timed out. Using default market prices.\n";
            return {};
        }
        std::lock_guard<std::mutex> lk(state->mutex);
        return state->prices;
    };

    return timedLoadPlayerStats(username);
}

#include "ItemID.h"
#include <algorithm>

void syncAndRenderOnlinePlayers(
    sf::RenderWindow& window,
    AuthManager& authManager,
    bool dbStartupEnabled,
    const std::string& username,
    sf::Sprite& sprite,
    float dt,
    std::future<SyncResult>& syncFuture,
    bool& syncInFlight,
    std::vector<OnlinePlayer>& otherPlayers,
    std::map<std::string, sf::Vector2f>& onlinePlayerTargets,
    std::map<std::string, sf::Vector2f>& onlinePlayerDisplayPos,
    std::map<std::string, sf::FloatRect>& onlinePlayerWrenchBounds,
    std::vector<PlacedStructure>& structures,
    std::vector<PlacedCampfire>& campfires,
    long long& serverEpochCache,
    const sf::Texture& texFemaleOnline,
    const sf::Texture& texMaleOnline,
    const sf::Texture& wrenchTex,
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Texture& chestTex,
    const sf::Font& uiFont,
    std::string& latestPlayerMsg,
    float& bubbleTimer,
    int& p2pTradeID,
    bool p2pTradeOpen,
    bool& p2pInvitePopup,
    bool& p2pAmInitiator,
    P2PTradeState& p2pState,
    std::string& p2pPartner,
    std::string& p2pStatusMsg,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc
) {
    if (!dbStartupEnabled) return;

    static sf::Clock syncClock;
    if (!syncInFlight && syncClock.getElapsedTime().asSeconds() > 0.4f) {
        syncInFlight = true;

        float px = sprite.getPosition().x;
        float py = sprite.getPosition().y;
        std::string uname = username;

        syncFuture = std::async(std::launch::async, [&authManager, uname, px, py]() -> SyncResult {
            SyncResult result;
            authManager.updateOnlineStatus(uname.c_str(), px, py);

            // Rate-limit chat cleanup: only every 75 ticks (~30 seconds at 0.4s/tick)
            // Previously ran every 0.4s — a slow correlated DELETE on the cloud DB.
            static int chatCleanupTick = 0;
            ++chatCleanupTick;
            if (chatCleanupTick >= 75) {
                authManager.cleanupInactiveChatMessages();
                chatCleanupTick = 0;
            }

            result.players = authManager.getOnlinePlayers(uname.c_str());
            OnlinePlayer self = authManager.getSelfOnlineInfo(uname.c_str());

            if (!self.pullBy.empty()) {
                result.hasPull = true;
                result.pullByUsername = self.pullBy;
                for (const auto& op : result.players) {
                    if (op.username == self.pullBy) {
                        result.pullPosition = { op.x, op.y };
                        break;
                    }
                }
                authManager.clearPull(uname.c_str());
            }

            result.serverEpoch = authManager.getServerEpochSeconds();

            static int structSyncTick = 0;
            ++structSyncTick;
            // Fetch immediately on first tick (structSyncTick == 1), and then every 10 ticks (~4s)
            // BUG FIX: was resetting to 1 (not 0), causing structures to reload every ~0.8s instead of every ~4s
            if (structSyncTick == 1 || structSyncTick >= 10) {
                result.otherStructures = authManager.loadOtherStructures(uname.c_str());
                result.otherCampfires = authManager.loadOtherCampfires(uname.c_str());
                result.structuresUpdated = true;
                structSyncTick = 0;  // FIX: reset to 0 so next fire is at tick 10 (~4s), not tick 1 (~0.4s)
            }

            result.pendingTradeID = authManager.getPendingP2PTradeFor(uname);
            if (result.pendingTradeID > 0) {
                result.pendingTradeState = authManager.getP2PTradeState(result.pendingTradeID);
            }

            return result;
        });
    }

    if (syncInFlight && syncFuture.valid()) {
        try {
            if (syncFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                SyncResult res = syncFuture.get();
                otherPlayers = res.players;

                if (res.serverEpoch > 0) serverEpochCache = res.serverEpoch;

                if (res.structuresUpdated) {
                    structures.erase(
                        std::remove_if(structures.begin(), structures.end(),
                            [&](const PlacedStructure& s) {
                                return s.owner != username;
                            }),
                        structures.end());
                    for (auto& s : res.otherStructures) {
                        const sf::Texture* tex = nullptr;
                        if      (s.id == ITEM_FURNACE)     tex = &furnaceTex;
                        else if (s.id == ITEM_WOOD_TILE)   tex = &woodTileTex;
                        else if (s.id == ITEM_WOOD_WALL)   tex = &woodWallTex;
                        else if (s.id == ITEM_WOOD_DOOR)   tex = &woodDoorTex;
                        else if (s.id == ITEM_WOOD_ROOF)   tex = &woodRoofTex;
                        else if (s.id == ITEM_WOOD_STAIRS) tex = &woodStairsTex;
                        else if (s.id == ITEM_BED)         tex = &bedTex;
                        else if (s.id == ITEM_CHEST)       tex = &chestTex;
                        if (tex) {
                            structures.emplace_back(s.id, *tex, s.rotation, s.isOpen, s.shearY, "other");
                            auto& ns = structures.back();
                            ns.sprite->setPosition({ s.x, s.y });
                            ns.health = s.health;
                            sf::Vector2u ts = ns.sprite->getTexture().getSize();
                            if (s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR || s.id == ITEM_WOOD_STAIRS) {
                                ns.sprite->setOrigin({ 0.f, (float)ts.y });
                            } else {
                                ns.sprite->setOrigin({ ts.x / 2.f, (float)ts.y });
                            }
                            applyStructureScale(*ns.sprite, s.id, s.rotation, s.isOpen, ns.shearY);
                        }
                    }
                    campfires.erase(
                        std::remove_if(campfires.begin(), campfires.end(),
                            [&](const PlacedCampfire& c) {
                                return c.owner != username;
                            }),
                        campfires.end());
                    for (auto& c : res.otherCampfires) {
                        PlacedCampfire pc;
                        pc.position      = { c.x, c.y };
                        pc.timeRemaining = c.timeRemaining;
                        pc.isCrafted     = c.isCrafted;
                        pc.currentFrame  = 0;
                        pc.frameTimer    = 0.f;
                        pc.fuel          = c.fuel;
                        pc.inputItem     = c.inputItem;
                        pc.inputCount    = c.inputCount;
                        pc.outputItem    = c.outputItem;
                        pc.outputCount   = c.outputCount;
                        pc.owner         = c.owner;
                        campfires.push_back(pc);
                    }
                }

                for (const auto& op : otherPlayers) {
                    sf::Vector2f target(op.x, op.y);
                    onlinePlayerTargets[op.username] = target;
                    if (onlinePlayerDisplayPos.find(op.username) == onlinePlayerDisplayPos.end()) {
                        onlinePlayerDisplayPos[op.username] = target;
                    }
                }
                for (auto it = onlinePlayerDisplayPos.begin(); it != onlinePlayerDisplayPos.end(); ) {
                    bool found = false;
                    for (const auto& op : otherPlayers) if (op.username == it->first) { found = true; break; }
                    it = found ? std::next(it) : onlinePlayerDisplayPos.erase(it);
                }

                if (res.hasPull && !res.pullByUsername.empty()) {
                    sprite.setPosition(res.pullPosition);
                    latestPlayerMsg = "I've been pulled by " + res.pullByUsername + "!";
                    bubbleTimer = 4.f;
                }

                if (p2pTradeID <= 0 && !p2pTradeOpen && !p2pInvitePopup && res.pendingTradeID > 0) {
                    p2pTradeID = res.pendingTradeID;
                    p2pAmInitiator = false;
                    p2pState = res.pendingTradeState;
                    p2pPartner = p2pState.initiator;
                    p2pInvitePopup = true;
                    p2pStatusMsg = "Pending invite from " + p2pPartner;
                }

                syncInFlight = false;
                syncClock.restart();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[ERROR] Asynchronous sync failure: " << e.what() << "\n";
            syncInFlight = false;
            syncClock.restart();
        }
        catch (...) {
            std::cerr << "[ERROR] Asynchronous sync unknown failure\n";
            syncInFlight = false;
            syncClock.restart();
        }
    }

    {
        for (auto& kv : onlinePlayerDisplayPos) {
            auto it = onlinePlayerTargets.find(kv.first);
            if (it != onlinePlayerTargets.end()) {
                sf::Vector2f& cur = kv.second;
                const sf::Vector2f& tgt = it->second;
                float lerpSpeed = 10.0f;
                float factor = (std::min)(1.0f, static_cast<float>(lerpSpeed * dt));
                cur.x += (tgt.x - cur.x) * factor;
                cur.y += (tgt.y - cur.y) * factor;
            }
        }
    }

    for (const auto& op : otherPlayers) {
        auto dispIt = onlinePlayerDisplayPos.find(op.username);
        sf::Vector2f renderPos = (dispIt != onlinePlayerDisplayPos.end())
            ? dispIt->second
            : sf::Vector2f(op.x, op.y);

        float dx = renderPos.x - sprite.getPosition().x;
        float dy = renderPos.y - sprite.getPosition().y;
        if (dx * dx + dy * dy >= 3000000.f) continue;

        sf::Sprite opSprite(op.gender == 2 ? texFemaleOnline : texMaleOnline);
        sf::Vector2u ts_init = opSprite.getTexture().getSize();
        if (ts_init.x > 0 && ts_init.y > 0) {
            opSprite.setOrigin({ ts_init.x / 2.f, ts_init.y / 2.f });
            opSprite.setScale({ 500.f / ts_init.x, 500.f / ts_init.y });
        }

        opSprite.setPosition(renderPos);
        window.draw(opSprite);

        sf::Text opName(uiFont, op.username, 16);
        opName.setFillColor(sf::Color::White);
        opName.setOutlineColor(sf::Color::Black);
        opName.setOutlineThickness(1.f);
        sf::FloatRect nb = opName.getLocalBounds();
        opName.setPosition({ renderPos.x - nb.size.x / 2.f, renderPos.y - 280.f });
        window.draw(opName);

        sf::Sprite opWrench(wrenchTex);
        opWrench.setScale({ 0.05f, 0.05f });
        float wIconX = opName.getPosition().x + opName.getLocalBounds().size.x + 5.f;
        float wIconY = opName.getPosition().y;
        opWrench.setPosition({ wIconX, wIconY });
        window.draw(opWrench);
        onlinePlayerWrenchBounds[op.username] = sf::FloatRect(
            { wIconX, wIconY },
            { opWrench.getGlobalBounds().size.x, opWrench.getGlobalBounds().size.y });
    }
}

void saveAllGameData(
    AuthManager& authManager,
    const std::string& username,
    TradeSlot playerTradeSlots[4],
    TradeSlot myP2PSlots[4],
    int p2pTradeID,
    Inventory& inventory,
    float hunger,
    float thirst,
    int currency,
    int level,
    int exp,
    float elapsedGameTime,
    int hungerMultiplier,
    int thirstMultiplier,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
) {
    for (int i = 0; i < 4; i++) {
        if (playerTradeSlots[i].tex != nullptr) {
            inventory.addItem(playerTradeSlots[i].id, *playerTradeSlots[i].tex, playerTradeSlots[i].count);
            playerTradeSlots[i] = {};
        }
    }
    for (int i = 0; i < 4; i++) {
        if (myP2PSlots[i].tex != nullptr) {
            inventory.addItem(myP2PSlots[i].id, *myP2PSlots[i].tex, myP2PSlots[i].count);
            myP2PSlots[i] = {};
        }
    }
    if (p2pTradeID > 0) {
        authManager.cancelP2PTrade(p2pTradeID);
    }

    std::vector<std::pair<int, int>> items(20, { 0, 0 });
    for (int i = 0; i < 20; i++) {
        if (inventory.hasItem(i)) {
            items[i] = { inventory.getItemID(i), inventory.getItemCount(i) };
        }
    }
    std::cout << "Saving inventory...\n";
    authManager.saveInventory(username.c_str(), items);

    float currentElapsed = elapsedGameTime;
    authManager.savePlayerStats(username.c_str(), hunger, thirst, currency, level, exp, currentElapsed, hungerMultiplier, thirstMultiplier);

    std::vector<StoredStructure> structData;
    for (auto& s : structures) {
        if (s.sprite.has_value() && s.owner == username) {
            structData.push_back({ s.id, s.sprite->getPosition().x, s.sprite->getPosition().y, s.health, s.rotation, s.isOpen, s.shearY });
        }
    }
    authManager.saveStructures(username.c_str(), structData);

    std::vector<StoredCampfire> campData;
    for (auto& c : campfires) {
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
    authManager.saveCampfires(username.c_str(), campData);

    // When the player leaves, do not save their chat messages/history in the server/online database
    authManager.clearPlayerChatMessages(username.c_str());

    // Mark player as offline in server database
    authManager.setPlayerOffline(username.c_str());

    std::cout << "Saved!\n";
}

void saveAllGameDataAndClose(
    sf::RenderWindow& window,
    AuthManager& authManager,
    const std::string& username,
    TradeSlot playerTradeSlots[4],
    TradeSlot myP2PSlots[4],
    int p2pTradeID,
    Inventory& inventory,
    float hunger,
    float thirst,
    int currency,
    int level,
    int exp,
    float elapsedGameTime,
    int hungerMultiplier,
    int thirstMultiplier,
    const std::vector<PlacedStructure>& structures,
    const std::vector<PlacedCampfire>& campfires
) {
    saveAllGameData(
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
    window.close();
}

