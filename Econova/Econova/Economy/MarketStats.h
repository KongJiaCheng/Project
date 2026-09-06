#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <functional>

class AuthManager;

struct StockItem {
    int id;
    std::string name;
    float currentPrice;
    float basePrice;
    std::vector<float> history;
    float predictedPrice;
    std::string trend;
    float volatility; // Volatility multiplier for price updates (e.g. 0.3 for low, 2.0 for high)
};

struct MarketTextures {
    const sf::Texture* woodTex = nullptr;
    const sf::Texture* stoneTex = nullptr;
    const sf::Texture* saplingTex = nullptr;
    const sf::Texture* cookedChickenTex = nullptr;
    const sf::Texture* cookedPigTex = nullptr;
    const sf::Texture* cookedSheepTex = nullptr;
    const sf::Texture* woolTex = nullptr;
    const sf::Texture* woodenSwordTex = nullptr;
    const sf::Texture* shieldTex = nullptr;
    const sf::Texture* furnaceTex = nullptr;
    const sf::Texture* woodTileTex = nullptr;
    const sf::Texture* woodWallTex = nullptr;
    const sf::Texture* woodDoorTex = nullptr;
    const sf::Texture* woodRoofTex = nullptr;
    const sf::Texture* woodStairsTex = nullptr;
    const sf::Texture* bedTex = nullptr;
};

std::vector<StockItem> createInitialStockMarket();
void initStockMarket(std::vector<StockItem>& stockMarket, std::function<float(int, float)> timedLoadPrice);

int getMarketItemPrice(int id, const std::vector<StockItem>& stockMarket);
std::string getMarketItemName(int id, const std::vector<StockItem>& stockMarket);

void adjustMarketItem(std::vector<StockItem>& stockMarket, int itemId, float percentChange, AuthManager& authManager);

void updateStockMarketSimulation(std::vector<StockItem>& stockMarket, AuthManager& authManager);

struct TradeRecord;

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
);