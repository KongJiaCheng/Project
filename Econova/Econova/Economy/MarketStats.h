#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <map>
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
    float quantity = 100.f;        // Simulated circulating supply (Qs)
    float baseQuantity = 100.f;    // Baseline equilibrium supply (Qd)
    int lifetimeVolume = 1000;     // Lifetime circulation/trade volume
    float demand = 100.f;          // Qd: Market demand
    float supply = 100.f;          // Qs: Market supply
    float pFactor = 1.0f;          // p: Price adjustment factor
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
    const sf::Texture* coalTex = nullptr;
    const sf::Texture* ironOreTex = nullptr;
    const sf::Texture* ironBarTex = nullptr;
    const sf::Texture* goldOreTex = nullptr;
    const sf::Texture* goldBarTex = nullptr;
    const sf::Texture* copperOreTex = nullptr;
    const sf::Texture* copperBarTex = nullptr;
    const sf::Texture* diamondTex = nullptr;
};

std::vector<StockItem> createInitialStockMarket();
void initStockMarket(std::vector<StockItem>& stockMarket, std::function<std::map<int, float>()> timedLoadAllPrices);
void initStockMarket(std::vector<StockItem>& stockMarket, std::function<float(int, float)> timedLoadPrice);

int getMarketItemPrice(int id, const std::vector<StockItem>& stockMarket);
std::string getMarketItemName(int id, const std::vector<StockItem>& stockMarket);

void adjustMarketItem(std::vector<StockItem>& stockMarket, int itemId, float percentChange, AuthManager& authManager);
void adjustItemQuantitySupply(std::vector<StockItem>& stockMarket, int itemId, float deltaQty, AuthManager& authManager);
void resetAllItemQuantities(std::vector<StockItem>& stockMarket, float targetQty, AuthManager& authManager);

void updateStockMarketSimulation(std::vector<StockItem>& stockMarket, AuthManager& authManager);
void runVirtualPlayerMarketGenerator(std::vector<StockItem>& stockMarket, AuthManager& authManager);
float calculateMarketPrice(float Pold, float Qd, float Qs, float p);
void applyDisasterFoodMarketImpact(std::vector<StockItem>& stockMarket, AuthManager& authManager);
void restoreDisasterFoodMarketImpact(std::vector<StockItem>& stockMarket, AuthManager& authManager);
void applyDroughtMarketImpact(std::vector<StockItem>& stockMarket, AuthManager& authManager);
void restoreDroughtMarketImpact(std::vector<StockItem>& stockMarket, AuthManager& authManager);

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
    const std::vector<TradeRecord>& recentTrades,
    bool isAdmin = false
);

void drawAdminQuantitySimulator(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::vector<StockItem>& stockMarket,
    int scrollOffset,
    const MarketTextures& textures
);

bool handleAdminQuantitySimulatorClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    std::vector<StockItem>& stockMarket,
    int& scrollOffset,
    bool& adminSimOpen,
    AuthManager& authManager
);