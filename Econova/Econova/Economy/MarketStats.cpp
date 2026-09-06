#include "MarketStats.h"
#include "AuthManager.h"
#include "ItemID.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "SheepMeat.h"
#include "ZombieFlesh.h"
#include "ViewUtils.h"
#include <cmath>
#include <algorithm>

std::vector<StockItem> createInitialStockMarket() {
    std::vector<StockItem> stockMarket = {
        { ITEM_WOOD, "Wood", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.4f },
        { ITEM_STONE, "Stone", 8.f, 8.f, {8.f}, 8.f, "STABLE", 0.3f },
        { ITEM_SAPLING, "Sapling", 12.f, 12.f, {12.f}, 12.f, "STABLE", 0.6f },
        { ITEM_CHICKEN_MEAT, "Chicken Meat", 15.f, 15.f, {15.f}, 15.f, "STABLE", 1.8f },
        { ITEM_PIG_MEAT, "Pig Meat", 25.f, 25.f, {25.f}, 25.f, "STABLE", 0.5f },
        { ITEM_SHEEP_MEAT, "Sheep Meat", 20.f, 20.f, {20.f}, 20.f, "STABLE", 0.5f },
        { ITEM_WOOL, "Wool", 15.f, 15.f, {15.f}, 15.f, "STABLE", 0.5f },
        { ITEM_ZOMBIE_FLESH, "Zombie Flesh", 10.f, 10.f, {10.f}, 10.f, "STABLE", 2.5f },
        { ITEM_COOKED_CHICKEN, "Cooked Chicken", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f },
        { ITEM_COOKED_PIG, "Cooked Pig", 50.f, 50.f, {50.f}, 50.f, "STABLE", 0.5f },
        { ITEM_COOKED_SHEEP, "Cooked Sheep", 40.f, 40.f, {40.f}, 40.f, "STABLE", 0.5f },
        { ITEM_WOODEN_SWORD, "Wooden Sword", 55.f, 55.f, {55.f}, 55.f, "STABLE", 0.5f },
        { ITEM_SHIELD, "Sturdy Shield", 115.f, 115.f, {115.f}, 115.f, "STABLE", 0.4f },
        { ITEM_FURNACE, "Furnace", 84.f, 84.f, {84.f}, 84.f, "STABLE", 0.3f },
        { ITEM_WOOD_TILE, "Wood Tile", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.5f },
        { ITEM_WOOD_WALL, "Wood Wall", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.5f },
        { ITEM_WOOD_DOOR, "Wood Door", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f },
        { ITEM_WOOD_ROOF, "Wood Roof", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f },
        { ITEM_WOOD_STAIRS, "Wood Stairs", 48.f, 48.f, {48.f}, 48.f, "STABLE", 0.5f },
        { ITEM_BED, "Bed", 80.f, 80.f, {80.f}, 80.f, "STABLE", 0.6f }
    };

    for (int step = 0; step < 8; ++step) {
        for (auto& item : stockMarket) {
            item.history.push_back(item.currentPrice);
        }
    }
    return stockMarket;
}

void initStockMarket(std::vector<StockItem>& stockMarket, std::function<float(int, float)> timedLoadPrice) {
    stockMarket = createInitialStockMarket();

    for (int step = 0; step < 8; ++step) {
        for (auto& item : stockMarket) {
            item.history.push_back(item.currentPrice);

            float baseRange = 15.f * item.volatility;
            int rangeInt = (int)(baseRange * 2.f) + 1;
            float percentChange = (-(int)baseRange + (rand() % (rangeInt > 0 ? rangeInt : 1))) / 100.f;

            float dev = (item.currentPrice - item.basePrice) / item.basePrice;
            percentChange -= dev * 0.05f;
            item.currentPrice = std::round(item.currentPrice * (1.f + percentChange));

            float minP = item.basePrice * (item.volatility > 1.2f ? 0.2f : 0.5f);
            float maxP = item.basePrice * (item.volatility > 1.2f ? 4.0f : 1.8f);
            if (item.currentPrice < minP) item.currentPrice = minP;
            if (item.currentPrice > maxP) item.currentPrice = maxP;

            item.currentPrice = std::round(item.currentPrice);
            if (item.currentPrice < 1.f) item.currentPrice = 1.f;
        }
    }

    if (timedLoadPrice) {
        for (auto& item : stockMarket) {
            float savedPrice = timedLoadPrice(item.id, item.currentPrice);
            if (savedPrice > 0.f) {
                item.currentPrice = savedPrice;
            }
        }
    }

    for (auto& item : stockMarket) {
        if (item.history.size() >= 2) {
            float lastPrice = item.history.back();
            float diff = item.currentPrice - lastPrice;
            float baseNoise = 8.f * item.volatility;
            int noiseRange = (int)(baseNoise * 2.f) + 1;
            float noise = (-(int)baseNoise + (rand() % (noiseRange > 0 ? noiseRange : 1))) / 100.f;
            item.predictedPrice = std::round(item.currentPrice + diff * 0.5f + item.currentPrice * noise);
            if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;

            float trendThreshold = item.volatility > 1.2f ? 0.08f : 0.04f;
            if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) item.trend = "UP";
            else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) item.trend = "DOWN";
            else item.trend = "STABLE";
        }
        else {
            item.predictedPrice = item.currentPrice;
            item.trend = "STABLE";
        }
    }
}

int getMarketItemPrice(int id, const std::vector<StockItem>& stockMarket) {
    for (const auto& sItem : stockMarket) {
        if (sItem.id == id) {
            return (int)sItem.currentPrice;
        }
    }

    int woodPrice = 5;
    int stonePrice = 8;
    for (const auto& sItem : stockMarket) {
        if (sItem.id == ITEM_WOOD) woodPrice = (int)sItem.currentPrice;
        if (sItem.id == ITEM_STONE) stonePrice = (int)sItem.currentPrice;
    }

    switch (id) {
    case ITEM_WOODEN_SWORD:
        return woodPrice * 8 + 15;
    case ITEM_SHIELD:
        return woodPrice * 10 + stonePrice * 5 + 25;
    case ITEM_FURNACE:
        return stonePrice * 8 + 20;
    case ITEM_WOOD_TILE:
        return woodPrice * 1;
    case ITEM_WOOD_WALL:
        return woodPrice * 1;
    case ITEM_WOOD_DOOR:
        return woodPrice * 5 + 5;
    case ITEM_WOOD_ROOF:
        return woodPrice * 5 + 5;
    case ITEM_WOOD_STAIRS:
        return woodPrice * 8 + 8;
    case ITEM_BED:
    {
        int woolPrice = 15;
        for (const auto& sItem : stockMarket) {
            if (sItem.id == ITEM_WOOL) woolPrice = (int)sItem.currentPrice;
        }
        return woodPrice * 10 + woolPrice * 5 + 30;
    }
    default:
        return 10;
    }
}

std::string getMarketItemName(int id, const std::vector<StockItem>& stockMarket) {
    for (const auto& sItem : stockMarket) {
        if (sItem.id == id) return sItem.name;
    }
    switch (id) {
    case ITEM_WOODEN_SWORD: return "Wooden Sword";
    case ITEM_SHIELD: return "Sturdy Shield";
    case ITEM_FURNACE: return "Furnace";
    case ITEM_WOOD_TILE: return "Wood Tile";
    case ITEM_WOOD_WALL: return "Wood Wall";
    case ITEM_WOOD_DOOR: return "Wood Door";
    case ITEM_WOOD_ROOF: return "Wood Roof";
    case ITEM_WOOD_STAIRS: return "Wood Stairs";
    case ITEM_BED: return "Bed";
    case ITEM_CHICKEN_MEAT: return "Chicken Meat";
    case ITEM_PIG_MEAT: return "Pig Meat";
    case ITEM_SHEEP_MEAT: return "Sheep Meat";
    case ITEM_WOOL: return "Wool";
    case ITEM_COOKED_CHICKEN: return "Cooked Chicken";
    case ITEM_COOKED_PIG: return "Cooked Pig";
    case ITEM_COOKED_SHEEP: return "Cooked Sheep";
    default: return "Unknown Item";
    }
}

void adjustMarketItem(std::vector<StockItem>& stockMarket, int itemId, float percentChange, AuthManager& authManager) {
    for (auto& item : stockMarket) {
        if (item.id == itemId) {
            if (item.history.size() >= 10) {
                item.history.erase(item.history.begin());
            }
            item.history.push_back(item.currentPrice);

            item.currentPrice *= (1.f + percentChange);

            float minP = item.basePrice * (item.volatility > 1.2f ? 0.2f : 0.5f);
            float maxP = item.basePrice * (item.volatility > 1.2f ? 4.0f : 1.8f);
            if (item.currentPrice < minP) item.currentPrice = minP;
            if (item.currentPrice > maxP) item.currentPrice = maxP;
            item.currentPrice = std::round(item.currentPrice);
            if (item.currentPrice < 1.f) item.currentPrice = 1.f;

            authManager.saveMarketPrice(item.id, item.currentPrice);

            if (!item.history.empty()) {
                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                item.predictedPrice = std::round(item.currentPrice + diff * 0.5f);
                if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;

                float trendThreshold = item.volatility > 1.2f ? 0.04f : 0.02f;
                if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) item.trend = "UP";
                else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) item.trend = "DOWN";
                else item.trend = "STABLE";
            }
            break;
        }
    }
}

void updateStockMarketSimulation(std::vector<StockItem>& stockMarket, AuthManager& authManager) {
    for (auto& item : stockMarket) {
        if (item.history.size() >= 10) {
            item.history.erase(item.history.begin());
        }
        item.history.push_back(item.currentPrice);

        float baseRange = 15.f * item.volatility;
        int rangeInt = (int)(baseRange * 2.f) + 1;
        float percentChange = (-(int)baseRange + (rand() % (rangeInt > 0 ? rangeInt : 1))) / 100.f;

        float dev = (item.currentPrice - item.basePrice) / item.basePrice;
        percentChange -= dev * 0.05f;

        item.currentPrice = item.currentPrice * (1.f + percentChange);

        float minP = item.basePrice * (item.volatility > 1.2f ? 0.2f : 0.5f);
        float maxP = item.basePrice * (item.volatility > 1.2f ? 4.0f : 1.8f);
        if (item.currentPrice < minP) item.currentPrice = minP;
        if (item.currentPrice > maxP) item.currentPrice = maxP;

        item.currentPrice = std::round(item.currentPrice);
        authManager.saveMarketPrice(item.id, item.currentPrice);
        if (item.currentPrice < 1.f) item.currentPrice = 1.f;

        if (item.history.size() >= 2) {
            float lastPrice = item.history.back();
            float diff = item.currentPrice - lastPrice;
            float baseNoise = 8.f * item.volatility;
            int noiseRange = (int)(baseNoise * 2.f) + 1;
            float noise = (-(int)baseNoise + (rand() % (noiseRange > 0 ? noiseRange : 1))) / 100.f;
            item.predictedPrice = std::round(item.currentPrice + diff * 0.5f + item.currentPrice * noise);
            if (item.predictedPrice < 1.f) item.predictedPrice = 1.f;

            float trendThreshold = item.volatility > 1.2f ? 0.08f : 0.04f;
            if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) {
                item.trend = "UP";
            }
            else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) {
                item.trend = "DOWN";
            }
            else {
                item.trend = "STABLE";
            }
        }
        else {
            item.predictedPrice = item.currentPrice;
            item.trend = "STABLE";
        }
    }

    if (rand() % 100 < 80) {
        std::vector<std::string> virtualPlayers = { "MinerBob", "LumberjackJack", "HunterMax", "SwordSmith", "GoldDigger", "FarmerFred" };
        std::string vPlayer = virtualPlayers[rand() % virtualPlayers.size()];
        std::string vAction = (rand() % 2 == 0) ? "SELL" : "BUY";

        std::vector<std::pair<int, std::string>> vItems = {
            { ITEM_WOOD, "Wood" },
            { ITEM_STONE, "Stone" },
            { ITEM_SAPLING, "Sapling" },
            { ITEM_CHICKEN_MEAT, "Chicken Meat" },
            { ITEM_PIG_MEAT, "Pig Meat" },
            { ITEM_SHEEP_MEAT, "Sheep Meat" },
            { ITEM_WOOL, "Wool" },
            { ITEM_ZOMBIE_FLESH, "Zombie Flesh" },
            { ITEM_COOKED_CHICKEN, "Cooked Chicken" },
            { ITEM_COOKED_PIG, "Cooked Pig" },
            { ITEM_COOKED_SHEEP, "Cooked Sheep" },
            { ITEM_WOODEN_SWORD, "Wooden Sword" },
            { ITEM_SHIELD, "Sturdy Shield" },
            { ITEM_FURNACE, "Furnace" },
            { ITEM_BED, "Bed" },
            { ITEM_WOOD_TILE, "Wood Tile" },
            { ITEM_WOOD_WALL, "Wood Wall" },
            { ITEM_WOOD_DOOR, "Wood Door" },
            { ITEM_WOOD_ROOF, "Wood Roof" },
            { ITEM_WOOD_STAIRS, "Wood Stairs" }
        };
        auto chosenItem = vItems[rand() % vItems.size()];
        int itemId = chosenItem.first;
        std::string itemName = chosenItem.second;
        int qty = 1 + rand() % 5;
        int unitPrice = getMarketItemPrice(itemId, stockMarket);
        int totalG = qty * unitPrice;

        authManager.addTradeRecord(vPlayer.c_str(), vAction, itemName, qty, unitPrice, totalG);

        float impact = (vAction == "BUY" ? 0.003f : -0.003f) * qty;
        adjustMarketItem(stockMarket, itemId, impact, authManager);
    }
}

void drawStockMarketPage(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::vector<StockItem>& stockMarket,
    bool marketSearchActive,
    const std::string& marketSearchText,
    sf::Clock& marketCursorBlink,
    int& marketScrollOffset,
    const MarketTextures& textures,
    const std::vector<TradeRecord>& recentTrades
) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    // Panel background
    sf::RectangleShape sBox({ 920.f, 620.f });
    sBox.setPosition({ (width - 920.f) / 2.f, (height - 620.f) / 2.f });
    sBox.setFillColor(sf::Color(18, 18, 24, 250));
    sBox.setOutlineThickness(2.f);
    sBox.setOutlineColor(sf::Color(80, 80, 110));
    window.draw(sBox);

    float bx = sBox.getPosition().x;
    float by = sBox.getPosition().y;

    // Title
    sf::Text sTitle(uiFont, "STOCK MARKET PREDICTIONS", 26);
    sTitle.setStyle(sf::Text::Bold);
    sTitle.setFillColor(sf::Color(255, 215, 0));
    sTitle.setPosition({ bx + 30.f, by + 18.f });
    window.draw(sTitle);

    // Close button
    sf::RectangleShape closeBtn({ 36.f, 36.f });
    closeBtn.setPosition({ bx + 864.f, by + 18.f });
    bool closeHov = closeBtn.getGlobalBounds().contains(mousePos);
    closeBtn.setFillColor(closeHov ? sf::Color(240, 70, 70) : sf::Color(190, 45, 45));
    closeBtn.setOutlineThickness(closeHov ? 2.f : 0.f);
    closeBtn.setOutlineColor(sf::Color::White);
    window.draw(closeBtn);
    sf::Text closeTxt(uiFont, "X", 20);
    closeTxt.setStyle(sf::Text::Bold);
    closeTxt.setPosition({ bx + 875.f, by + 21.f });
    window.draw(closeTxt);

    // Search bar
    sf::FloatRect searchBarRect({ bx + 30.f, by + 62.f }, { 700.f, 32.f });
    sf::RectangleShape searchBg({ 700.f, 32.f });
    searchBg.setPosition({ bx + 30.f, by + 62.f });
    searchBg.setFillColor(marketSearchActive ? sf::Color(35, 38, 52) : sf::Color(28, 30, 42));
    searchBg.setOutlineThickness(marketSearchActive ? 2.f : 1.f);
    searchBg.setOutlineColor(marketSearchActive ? sf::Color(100, 160, 255) : sf::Color(60, 62, 80));
    window.draw(searchBg);

    sf::Text searchIcon(uiFont, "  ", 16);
    searchIcon.setFillColor(sf::Color(120, 120, 150));
    searchIcon.setPosition({ bx + 36.f, by + 69.f });
    window.draw(searchIcon);

    std::string displaySearch = marketSearchText;
    bool cursorVisible = (marketCursorBlink.getElapsedTime().asMilliseconds() % 1000) < 500;
    if (marketSearchActive && cursorVisible) displaySearch += "|";
    else if (displaySearch.empty() && !marketSearchActive) displaySearch = "Search items...";

    sf::Text searchTxt(uiFont, displaySearch, 15);
    searchTxt.setFillColor(marketSearchText.empty() && !marketSearchActive
        ? sf::Color(80, 82, 100)
        : sf::Color(220, 220, 240));
    searchTxt.setPosition({ bx + 52.f, by + 69.f });
    window.draw(searchTxt);

    struct DisplayItem {
        std::string name;
        int currentPrice;
        std::string trend;
        int predictedPrice;
        std::vector<float> history;
        const sf::Texture* tex = nullptr;
        bool isCrafted = false;
        float volatility = 1.0f;
    };

    std::vector<DisplayItem> allItems;

    for (const auto& sm : stockMarket) {
        DisplayItem di;
        di.name = sm.name;
        di.currentPrice = (int)sm.currentPrice;
        di.predictedPrice = (int)sm.predictedPrice;
        di.trend = sm.trend;
        di.history = sm.history;
        di.isCrafted = (sm.id == ITEM_WOODEN_SWORD || sm.id == ITEM_SHIELD || sm.id == ITEM_FURNACE ||
                        sm.id == ITEM_WOOD_TILE || sm.id == ITEM_WOOD_WALL || sm.id == ITEM_WOOD_DOOR ||
                        sm.id == ITEM_WOOD_ROOF || sm.id == ITEM_WOOD_STAIRS || sm.id == ITEM_BED);
        di.volatility = sm.volatility;
        if (sm.id == ITEM_WOOD)               di.tex = textures.woodTex;
        else if (sm.id == ITEM_STONE)          di.tex = textures.stoneTex;
        else if (sm.id == ITEM_SAPLING)        di.tex = textures.saplingTex;
        else if (sm.id == ITEM_CHICKEN_MEAT)   di.tex = ChickenMeat::getTexture();
        else if (sm.id == ITEM_PIG_MEAT)       di.tex = PigMeat::getTexture();
        else if (sm.id == ITEM_SHEEP_MEAT)     di.tex = SheepMeat::getTexture();
        else if (sm.id == ITEM_COOKED_CHICKEN) di.tex = textures.cookedChickenTex;
        else if (sm.id == ITEM_COOKED_PIG)     di.tex = textures.cookedPigTex;
        else if (sm.id == ITEM_COOKED_SHEEP)   di.tex = textures.cookedSheepTex;
        else if (sm.id == ITEM_WOOL)           di.tex = textures.woolTex;
        else if (sm.id == ITEM_ZOMBIE_FLESH)   di.tex = ZombieFlesh::getTexture();
        else if (sm.id == ITEM_WOODEN_SWORD)   di.tex = textures.woodenSwordTex;
        else if (sm.id == ITEM_SHIELD)         di.tex = textures.shieldTex;
        else if (sm.id == ITEM_FURNACE)        di.tex = textures.furnaceTex;
        else if (sm.id == ITEM_WOOD_TILE)      di.tex = textures.woodTileTex;
        else if (sm.id == ITEM_WOOD_WALL)      di.tex = textures.woodWallTex;
        else if (sm.id == ITEM_WOOD_DOOR)      di.tex = textures.woodDoorTex;
        else if (sm.id == ITEM_WOOD_ROOF)      di.tex = textures.woodRoofTex;
        else if (sm.id == ITEM_WOOD_STAIRS)    di.tex = textures.woodStairsTex;
        else if (sm.id == ITEM_BED)            di.tex = textures.bedTex;
        allItems.push_back(di);
    }

    std::vector<DisplayItem*> filtered;
    for (auto& di : allItems) {
        if (marketSearchText.empty()) {
            filtered.push_back(&di);
        }
        else {
            std::string nameLower = di.name;
            std::string queryLower = marketSearchText;
            for (char& c : nameLower)  c = (char)std::tolower((unsigned char)c);
            for (char& c : queryLower) c = (char)std::tolower((unsigned char)c);
            if (nameLower.find(queryLower) != std::string::npos)
                filtered.push_back(&di);
        }
    }

    const int maxVisible = 6;
    const float rowHeight = 72.f;
    int totalRows = (int)filtered.size();
    int maxScroll = std::max(0, totalRows - maxVisible);
    if (marketScrollOffset > maxScroll) marketScrollOffset = maxScroll;
    if (marketScrollOffset < 0) marketScrollOffset = 0;

    float headerY = by + 104.f;
    auto makeHeader = [&](const std::string& label, float x) {
        sf::Text h(uiFont, label, 14);
        h.setFillColor(sf::Color(140, 140, 170));
        h.setPosition({ x, headerY });
        window.draw(h);
    };
    makeHeader("Item", bx + 30.f);
    makeHeader("Current Price", bx + 210.f);
    makeHeader("Trend", bx + 330.f);
    makeHeader("History Chart", bx + 460.f);
    makeHeader("Forecast / Signal", bx + 650.f);

    sf::RectangleShape hDivider({ 840.f, 1.f });
    hDivider.setPosition({ bx + 30.f, headerY + 22.f });
    hDivider.setFillColor(sf::Color(55, 55, 75));
    window.draw(hDivider);

    float listTop = headerY + 28.f;
    float listH = maxVisible * rowHeight;

    for (int vi = 0; vi < maxVisible; vi++) {
        int idx = vi + marketScrollOffset;
        if (idx >= totalRows) break;
        DisplayItem* item = filtered[idx];
        float y = listTop + vi * rowHeight;

        sf::RectangleShape rBg({ 838.f, rowHeight - 4.f });
        rBg.setPosition({ bx + 30.f, y });
        rBg.setFillColor(sf::Color(vi % 2 == 0 ? 38 : 28, vi % 2 == 0 ? 38 : 28, vi % 2 == 0 ? 52 : 40, 180));
        window.draw(rBg);

        if (item->tex) {
            sf::Sprite icon(*item->tex);
            icon.setPosition({ bx + 36.f, y + 10.f });
            sf::Vector2u sz = item->tex->getSize();
            icon.setScale({ 34.f / sz.x, 34.f / sz.y });
            window.draw(icon);
        }
        else {
            sf::RectangleShape placeholder({ 34.f, 34.f });
            placeholder.setPosition({ bx + 36.f, y + 10.f });
            placeholder.setFillColor(sf::Color(60, 60, 80));
            placeholder.setOutlineThickness(1.f);
            placeholder.setOutlineColor(sf::Color(100, 100, 130));
            window.draw(placeholder);
            sf::Text qMark(uiFont, "?", 18);
            qMark.setPosition({ bx + 47.f, y + 13.f });
            qMark.setFillColor(sf::Color(160, 160, 200));
            window.draw(qMark);
        }

        sf::Text nameTxt(uiFont, item->name, 16);
        nameTxt.setPosition({ bx + 80.f, y + 14.f });
        nameTxt.setFillColor(item->isCrafted ? sf::Color(200, 200, 220) : sf::Color::White);
        window.draw(nameTxt);

        if (item->isCrafted) {
            sf::RectangleShape badge({ 50.f, 14.f });
            badge.setPosition({ bx + 80.f, y + 36.f });
            badge.setFillColor(sf::Color(70, 50, 100));
            window.draw(badge);
            sf::Text badgeTxt(uiFont, "CRAFTED", 9);
            badgeTxt.setFillColor(sf::Color(200, 160, 255));
            badgeTxt.setPosition({ bx + 82.f, y + 38.f });
            window.draw(badgeTxt);
        }
        else {
            sf::RectangleShape badge({ 55.f, 14.f });
            badge.setPosition({ bx + 80.f, y + 36.f });
            if (item->volatility > 1.2f) {
                badge.setFillColor(sf::Color(140, 40, 40));
                window.draw(badge);
                sf::Text badgeTxt(uiFont, "UNSTABLE", 8);
                badgeTxt.setFillColor(sf::Color(255, 140, 140));
                badgeTxt.setPosition({ bx + 82.f, y + 38.f });
                window.draw(badgeTxt);
            }
            else {
                badge.setFillColor(sf::Color(30, 85, 80));
                window.draw(badge);
                sf::Text badgeTxt(uiFont, "STABLE", 8);
                badgeTxt.setFillColor(sf::Color(140, 255, 230));
                badgeTxt.setPosition({ bx + 86.f, y + 38.f });
                window.draw(badgeTxt);
            }
        }

        sf::Text priceTxt(uiFont, std::to_string(item->currentPrice) + " G", 17);
        priceTxt.setFillColor(sf::Color(255, 230, 80));
        priceTxt.setPosition({ bx + 210.f, y + 18.f });
        window.draw(priceTxt);

        sf::Text trendTxt(uiFont, "", 15);
        if (item->trend == "UP") {
            trendTxt.setString("^ UP");
            trendTxt.setFillColor(sf::Color(80, 220, 80));
        }
        else if (item->trend == "DOWN") {
            trendTxt.setString("v DOWN");
            trendTxt.setFillColor(sf::Color(220, 80, 80));
        }
        else {
            trendTxt.setString("~ STABLE");
            trendTxt.setFillColor(sf::Color(150, 150, 170));
        }
        trendTxt.setPosition({ bx + 330.f, y + 18.f });
        window.draw(trendTxt);

        if (!item->history.empty()) {
            float minH = item->history[0], maxH = item->history[0];
            for (float hp : item->history) {
                if (hp < minH) minH = hp;
                if (hp > maxH) maxH = hp;
            }
            float range = maxH - minH;
            if (range < 1.f) range = 1.f;

            float graphW = 160.f, graphH = 38.f;
            float graphX = bx + 460.f, graphY = y + 10.f;

            sf::RectangleShape graphBg({ graphW, graphH });
            graphBg.setPosition({ graphX, graphY });
            graphBg.setFillColor(sf::Color(16, 16, 22, 210));
            graphBg.setOutlineThickness(1.f);
            graphBg.setOutlineColor(sf::Color(50, 50, 68));
            window.draw(graphBg);

            std::vector<float> pts = item->history;
            pts.push_back((float)item->currentPrice);

            sf::Color lineCol = (pts.back() >= pts.front())
                ? sf::Color(80, 220, 80) : sf::Color(220, 80, 80);

            if (pts.size() >= 2) {
                float stepX = graphW / (float)(pts.size() - 1);
                for (size_t p = 0; p + 1 < pts.size(); p++) {
                    float x1 = graphX + p * stepX;
                    float y1 = graphY + graphH - 4.f - ((pts[p] - minH) / range) * (graphH - 8.f);
                    float x2 = graphX + (p + 1) * stepX;
                    float y2 = graphY + graphH - 4.f - ((pts[p + 1] - minH) / range) * (graphH - 8.f);
                    sf::RectangleShape seg;
                    float dist = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
                    seg.setSize({ dist, 2.f });
                    seg.setOrigin({ 0.f, 1.f });
                    seg.setPosition({ x1, y1 });
                    float angle = std::atan2(y2 - y1, x2 - x1) * 180.f / 3.14159f;
                    seg.setRotation(sf::degrees(angle));
                    seg.setFillColor(lineCol);
                    window.draw(seg);
                }
                sf::RectangleShape dot({ 5.f, 5.f });
                float dotX = graphX + (pts.size() - 1) * stepX - 2.f;
                float dotY = graphY + graphH - 4.f - ((pts.back() - minH) / range) * (graphH - 8.f) - 2.f;
                dot.setPosition({ dotX, dotY });
                dot.setFillColor(sf::Color::White);
                window.draw(dot);
            }
        }

        sf::Text predTxt(uiFont, "Est: " + std::to_string(item->predictedPrice) + " G", 14);
        predTxt.setFillColor(sf::Color(200, 200, 220));
        predTxt.setPosition({ bx + 650.f, y + 8.f });
        window.draw(predTxt);

        sf::Text sigTxt(uiFont, "", 13);
        sigTxt.setStyle(sf::Text::Bold);
        if (item->trend == "UP") {
            sigTxt.setString("SIGNAL: BUY");
            sigTxt.setFillColor(sf::Color(80, 220, 80));
        }
        else if (item->trend == "DOWN") {
            sigTxt.setString("SIGNAL: SELL");
            sigTxt.setFillColor(sf::Color(220, 80, 80));
        }
        else {
            sigTxt.setString("SIGNAL: HOLD");
            sigTxt.setFillColor(sf::Color(150, 150, 170));
        }
        sigTxt.setPosition({ bx + 650.f, y + 28.f });
        window.draw(sigTxt);

        sf::RectangleShape rowDiv({ 838.f, 1.f });
        rowDiv.setPosition({ bx + 30.f, y + rowHeight - 4.f });
        rowDiv.setFillColor(sf::Color(45, 45, 62));
        window.draw(rowDiv);
    }

    if (totalRows > maxVisible) {
        float trackX = bx + 882.f;
        float trackY = listTop;
        float trackH = listH;

        sf::RectangleShape track({ 8.f, trackH });
        track.setPosition({ trackX, trackY });
        track.setFillColor(sf::Color(40, 40, 55));
        track.setOutlineThickness(1.f);
        track.setOutlineColor(sf::Color(60, 60, 80));
        window.draw(track);

        float thumbRatio = (float)maxVisible / totalRows;
        float thumbH = std::max(20.f, trackH * thumbRatio);
        float thumbY = trackY + (trackH - thumbH) * ((float)marketScrollOffset / maxScroll);

        sf::RectangleShape thumb({ 8.f, thumbH });
        thumb.setPosition({ trackX, thumbY });
        thumb.setFillColor(sf::Color(100, 120, 200));
        window.draw(thumb);
    }

    if (filtered.empty()) {
        sf::Text noResult(uiFont, "No items match \"" + marketSearchText + "\"", 18);
        noResult.setFillColor(sf::Color(130, 130, 160));
        noResult.setPosition({ bx + 280.f, listTop + listH / 2.f - 10.f });
        window.draw(noResult);
    }

    sf::Text countTxt(uiFont,
        std::to_string(filtered.size()) + " / " + std::to_string(allItems.size()) + " items", 13);
    countTxt.setFillColor(sf::Color(100, 100, 130));
    countTxt.setPosition({ bx + 740.f, by + 68.f });
    window.draw(countTxt);

    if (totalRows > maxVisible) {
        sf::Text hint(uiFont, "Scroll with mouse wheel to see more items", 13);
        hint.setFillColor(sf::Color(80, 80, 110));
        hint.setPosition({ bx + 30.f, by + 595.f });
        window.draw(hint);
    }

    // Recent trades ticker
    sf::RectangleShape tickerBg({ 880.f, 28.f });
    tickerBg.setPosition({ bx + 20.f, by + 578.f });
    tickerBg.setFillColor(sf::Color(28, 28, 38, 220));
    tickerBg.setOutlineThickness(1.f);
    tickerBg.setOutlineColor(sf::Color(60, 60, 90));
    window.draw(tickerBg);

    std::string tickerText = "  LIVE:  ";
    if (recentTrades.empty()) {
        tickerText += "No transactions recorded yet.";
    }
    else {
        bool firstEntry = true;
        for (size_t ri = 0; ri < recentTrades.size(); ri++) {
            auto& r = recentTrades[ri];
            if (r.itemName.empty() || r.itemName == "Unknown Item" || r.itemName == "Unknown") continue;
            std::string uName = r.username.length() > 10 ? r.username.substr(0, 8) + ".." : r.username;
            if (!firstEntry) tickerText += "   |   ";
            tickerText += "[" + uName + "] " + (r.actionType == "SELL" ? "Sold" : "Bought")
                + " " + std::to_string(r.quantity) + "x " + r.itemName
                + " (" + std::to_string(r.totalGold) + "G)";
            firstEntry = false;
        }
        if (firstEntry) tickerText += "No transactions recorded yet.";
    }
    sf::Text tickerTxt(uiFont, tickerText, 13);
    tickerTxt.setFillColor(sf::Color(255, 215, 0));
    tickerTxt.setPosition({ tickerBg.getPosition().x + 10.f, tickerBg.getPosition().y + 5.f });
    window.draw(tickerTxt);
}