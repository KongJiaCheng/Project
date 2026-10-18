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
#include <thread>

static void saveAllMarketPricesAsync(AuthManager& authManager, const std::vector<StockItem>& stockMarket) {
    std::vector<std::pair<int, float>> priceList;
    priceList.reserve(stockMarket.size());
    for (const auto& item : stockMarket) {
        priceList.push_back({ item.id, item.currentPrice });
    }
    std::thread([&authManager, priceList = std::move(priceList)]() {
        try {
            authManager.saveAllMarketPrices(priceList);
        }
        catch (...) {}
    }).detach();
}

static void saveSingleMarketPriceAsync(AuthManager& authManager, int itemId, float price) {
    std::thread([&authManager, itemId, price]() {
        try {
            authManager.saveMarketPrice(itemId, price);
        }
        catch (...) {}
    }).detach();
}

float calculateMarketPrice(float Pold, float Qd, float Qs, float p) {
    float safeQs = std::max(1.f, Qs);
    float excessDemandRatio = (Qd - safeQs) / safeQs;
    float Pnew = Pold * (1.f + p * excessDemandRatio);
    return std::max(1.f, std::round(Pnew));
}

std::vector<StockItem> createInitialStockMarket() {
    std::vector<StockItem> stockMarket = {
        { ITEM_WOOD, "Wood", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.4f, 100.f, 100.f, 3250 },
        { ITEM_STONE, "Stone", 8.f, 8.f, {8.f}, 8.f, "STABLE", 0.3f, 100.f, 100.f, 2840 },
        { ITEM_SAPLING, "Sapling", 12.f, 12.f, {12.f}, 12.f, "STABLE", 0.6f, 100.f, 100.f, 1120 },
        { ITEM_CHICKEN_MEAT, "Chicken Meat", 15.f, 15.f, {15.f}, 15.f, "STABLE", 1.8f, 100.f, 100.f, 1540 },
        { ITEM_PIG_MEAT, "Pig Meat", 25.f, 25.f, {25.f}, 25.f, "STABLE", 0.5f, 100.f, 100.f, 1210 },
        { ITEM_SHEEP_MEAT, "Sheep Meat", 20.f, 20.f, {20.f}, 20.f, "STABLE", 0.5f, 100.f, 100.f, 980 },
        { ITEM_WOOL, "Wool", 15.f, 15.f, {15.f}, 15.f, "STABLE", 0.5f, 100.f, 100.f, 1430 },
        { ITEM_ZOMBIE_FLESH, "Zombie Flesh", 10.f, 10.f, {10.f}, 10.f, "STABLE", 2.5f, 100.f, 100.f, 870 },
        { ITEM_COOKED_CHICKEN, "Cooked Chicken", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f, 100.f, 100.f, 1350 },
        { ITEM_COOKED_PIG, "Cooked Pig", 50.f, 50.f, {50.f}, 50.f, "STABLE", 0.5f, 100.f, 100.f, 1100 },
        { ITEM_COOKED_SHEEP, "Cooked Sheep", 40.f, 40.f, {40.f}, 40.f, "STABLE", 0.5f, 100.f, 100.f, 920 },
        { ITEM_WOODEN_SWORD, "Wooden Sword", 55.f, 55.f, {55.f}, 55.f, "STABLE", 0.5f, 100.f, 100.f, 430 },
        { ITEM_SHIELD, "Sturdy Shield", 115.f, 115.f, {115.f}, 115.f, "STABLE", 0.4f, 100.f, 100.f, 310 },
        { ITEM_FURNACE, "Furnace", 84.f, 84.f, {84.f}, 84.f, "STABLE", 0.3f, 100.f, 100.f, 520 },
        { ITEM_WOOD_TILE, "Wood Tile", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.5f, 100.f, 100.f, 2100 },
        { ITEM_WOOD_WALL, "Wood Wall", 5.f, 5.f, {5.f}, 5.f, "STABLE", 0.5f, 100.f, 100.f, 1850 },
        { ITEM_WOOD_DOOR, "Wood Door", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f, 100.f, 100.f, 740 },
        { ITEM_WOOD_ROOF, "Wood Roof", 30.f, 30.f, {30.f}, 30.f, "STABLE", 0.5f, 100.f, 100.f, 680 },
        { ITEM_WOOD_STAIRS, "Wood Stairs", 48.f, 48.f, {48.f}, 48.f, "STABLE", 0.5f, 100.f, 100.f, 610 },
        { ITEM_BED, "Bed", 80.f, 80.f, {80.f}, 80.f, "STABLE", 0.6f, 100.f, 100.f, 490 },
        { ITEM_COAL, "Coal", 15.f, 15.f, {15.f}, 15.f, "STABLE", 0.4f, 100.f, 100.f, 1950 },
        { ITEM_COPPER_ORE, "Copper Ore", 20.f, 20.f, {20.f}, 20.f, "STABLE", 0.5f, 100.f, 100.f, 1620 },
        { ITEM_COPPER_BAR, "Copper Bar", 45.f, 45.f, {45.f}, 45.f, "STABLE", 0.5f, 100.f, 100.f, 1080 },
        { ITEM_IRON_ORE, "Iron Ore", 35.f, 35.f, {35.f}, 35.f, "STABLE", 0.5f, 100.f, 100.f, 1400 },
        { ITEM_IRON_BAR, "Iron Bar", 75.f, 75.f, {75.f}, 75.f, "STABLE", 0.5f, 100.f, 100.f, 890 },
        { ITEM_GOLD_ORE, "Gold Ore", 90.f, 90.f, {90.f}, 90.f, "STABLE", 0.6f, 100.f, 100.f, 540 },
        { ITEM_GOLD_BAR, "Gold Bar", 180.f, 180.f, {180.f}, 180.f, "STABLE", 0.7f, 100.f, 100.f, 360 },
        { ITEM_DIAMOND, "Diamond", 350.f, 350.f, {350.f}, 350.f, "STABLE", 0.8f, 100.f, 100.f, 210 }
    };

    for (auto& item : stockMarket) {
        item.demand = item.baseQuantity;
        item.supply = item.quantity;
        item.pFactor = item.volatility;
    }

    for (int step = 0; step < 8; ++step) {
        for (auto& item : stockMarket) {
            item.history.push_back(item.currentPrice);
        }
    }
    return stockMarket;
}

void initStockMarket(std::vector<StockItem>& stockMarket, std::function<std::map<int, float>()> timedLoadAllPrices) {
    stockMarket = createInitialStockMarket();

    for (int step = 0; step < 8; ++step) {
        for (auto& item : stockMarket) {
            item.history.push_back(item.currentPrice);

            float baseRange = 15.f * item.volatility;
            int rangeInt = (int)(baseRange * 2.f) + 1;
            float shift = (-(int)baseRange + (rand() % (rangeInt > 0 ? rangeInt : 1)));
            float simQd = std::max(10.f, item.demand + shift);
            float simQs = item.supply;

            float p = item.volatility;
            float Pold = item.currentPrice;
            item.currentPrice = calculateMarketPrice(Pold, simQd, simQs, p);
        }
    }

    if (timedLoadAllPrices) {
        std::map<int, float> savedPrices = timedLoadAllPrices();
        for (auto& item : stockMarket) {
            auto it = savedPrices.find(item.id);
            if (it != savedPrices.end() && it->second > 0.f) {
                item.currentPrice = it->second;
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
            item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f + item.currentPrice * noise));

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

void initStockMarket(std::vector<StockItem>& stockMarket, std::function<float(int, float)> timedLoadPrice) {
    stockMarket = createInitialStockMarket();

    for (int step = 0; step < 8; ++step) {
        for (auto& item : stockMarket) {
            item.history.push_back(item.currentPrice);

            // Simulate realistic Qd and Qs fluctuations
            float baseRange = 15.f * item.volatility;
            int rangeInt = (int)(baseRange * 2.f) + 1;
            float shift = (-(int)baseRange + (rand() % (rangeInt > 0 ? rangeInt : 1)));
            float simQd = std::max(10.f, item.demand + shift);
            float simQs = item.supply;

            // Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
            float p = item.volatility;
            float Pold = item.currentPrice;
            item.currentPrice = calculateMarketPrice(Pold, simQd, simQs, p);
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
            item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f + item.currentPrice * noise));

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

            // Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
            // Here percentChange is the effective impact: p * (Qd - Qs) / Qs = percentChange
            float Pold = item.currentPrice;
            float p = item.pFactor > 0.01f ? item.pFactor : 1.0f;
            float excessDemandRatio = percentChange / p;
            item.demand = std::max(5.f, item.supply * (1.f + excessDemandRatio));

            item.currentPrice = calculateMarketPrice(Pold, item.demand, item.supply, p);

            // Guarantee a noticeable price move if percentChange is non-trivial (e.g. unreasonable price trade)
            if (percentChange > 0.05f && item.currentPrice <= Pold) {
                item.currentPrice = Pold + 1.f;
            } else if (percentChange < -0.05f && item.currentPrice >= Pold && Pold > 1.f) {
                item.currentPrice = Pold - 1.f;
            }

            // Keep within realistic economic bounds (0.2x to 5.0x base price)
            float minPrice = std::max(1.f, std::round(item.basePrice * 0.2f));
            float maxPrice = std::max(minPrice + 1.f, std::round(item.basePrice * 5.0f));
            item.currentPrice = std::clamp(item.currentPrice, minPrice, maxPrice);

            saveSingleMarketPriceAsync(authManager, item.id, item.currentPrice);

            if (!item.history.empty()) {
                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));

                float trendThreshold = item.volatility > 1.2f ? 0.04f : 0.02f;
                if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold) || diff > 0.f) item.trend = "UP";
                else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold) || diff < 0.f) item.trend = "DOWN";
                else item.trend = "STABLE";
            }
            break;
        }
    }
}

void adjustItemQuantitySupply(std::vector<StockItem>& stockMarket, int itemId, float deltaQty, AuthManager& authManager) {
    for (auto& item : stockMarket) {
        if (item.id == itemId) {
            float oldQty = item.quantity;
            item.quantity = std::clamp(item.quantity + deltaQty, 5.f, 500.f);
            item.supply = item.quantity;
            if (item.quantity > oldQty) {
                item.lifetimeVolume += static_cast<int>(item.quantity - oldQty);
            }

            // Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
            // p = price adjustment factor (Admin Simulator Adjustment Impact)
            float p = 1.0f;              // Admin simulator adjustment impact factor
            float Pold = item.basePrice; // Evaluated against base equilibrium price
            float Qd = item.demand;      // Demand
            float Qs = item.supply;      // Adjusted supply

            float newPrice = calculateMarketPrice(Pold, Qd, Qs, p);

            if (item.history.size() >= 10) {
                item.history.erase(item.history.begin());
            }
            item.history.push_back(item.currentPrice);
            item.currentPrice = newPrice;

            saveSingleMarketPriceAsync(authManager, item.id, item.currentPrice);

            if (!item.history.empty()) {
                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));

                float trendThreshold = item.volatility > 1.2f ? 0.04f : 0.02f;
                if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) item.trend = "UP";
                else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) item.trend = "DOWN";
                else item.trend = "STABLE";
            }
            break;
        }
    }
}

void resetAllItemQuantities(std::vector<StockItem>& stockMarket, float targetQty, AuthManager& authManager) {
    for (auto& item : stockMarket) {
        float oldQty = item.quantity;
        item.quantity = std::clamp(targetQty, 5.f, 500.f);
        item.supply = item.quantity;
        if (item.quantity > oldQty) {
            item.lifetimeVolume += static_cast<int>(item.quantity - oldQty);
        }

        // Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
        // p = price adjustment factor (Admin Simulator Adjustment Impact)
        float p = 1.0f;              // Admin simulator adjustment impact factor
        float Pold = item.basePrice; // Evaluated against base equilibrium price
        float Qd = item.demand;      // Baseline demand
        float Qs = item.supply;      // Preset supply

        float newPrice = calculateMarketPrice(Pold, Qd, Qs, p);

        if (item.history.size() >= 10) {
            item.history.erase(item.history.begin());
        }
        item.history.push_back(item.currentPrice);
        item.currentPrice = newPrice;

        if (!item.history.empty()) {
            float lastPrice = item.history.back();
            float diff = item.currentPrice - lastPrice;
            item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));

            float trendThreshold = item.volatility > 1.2f ? 0.04f : 0.02f;
            if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) item.trend = "UP";
            else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) item.trend = "DOWN";
            else item.trend = "STABLE";
        }
    }
    saveAllMarketPricesAsync(authManager, stockMarket);
}

void applyDisasterFoodMarketImpact(std::vector<StockItem>& stockMarket, AuthManager& authManager) {
    // Raw foods drop drastically (Demand Qd plummets due to contamination)
    // Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
    // p = price adjustment factor (Disaster Impact)
    std::vector<int> rawFoods = { ITEM_CHICKEN_MEAT, ITEM_PIG_MEAT, ITEM_SHEEP_MEAT, ITEM_ZOMBIE_FLESH };
    for (int id : rawFoods) {
        for (auto& item : stockMarket) {
            if (item.id == id) {
                float Pold = item.currentPrice;
                float Qd = 45.f - (rand() % 15); // Demand drops to 30..45 (excess supply)
                float Qs = item.supply;
                float p = 1.0f; // Disaster price adjustment factor
                item.demand = Qd;

                float newPrice = calculateMarketPrice(Pold, Qd, Qs, p);

                if (item.history.size() >= 10) item.history.erase(item.history.begin());
                item.history.push_back(Pold);
                item.currentPrice = newPrice;

                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));
                item.trend = "DOWN";
                break;
            }
        }
    }

    // Cooked safe foods rise drastically (Demand Qd surges)
    // Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
    // p = price adjustment factor (Disaster Impact)
    std::vector<int> cookedFoods = { ITEM_COOKED_CHICKEN, ITEM_COOKED_PIG, ITEM_COOKED_SHEEP };
    for (int id : cookedFoods) {
        for (auto& item : stockMarket) {
            if (item.id == id) {
                float Pold = item.currentPrice;
                float Qd = 165.f + (rand() % 25); // Demand surges to 165..190 (excess demand)
                float Qs = item.supply;
                float p = 1.0f; // Disaster price adjustment factor
                item.demand = Qd;

                float newPrice = calculateMarketPrice(Pold, Qd, Qs, p);

                if (item.history.size() >= 10) item.history.erase(item.history.begin());
                item.history.push_back(Pold);
                item.currentPrice = newPrice;

                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));
                item.trend = "UP";
                break;
            }
        }
    }
    saveAllMarketPricesAsync(authManager, stockMarket);
}

void restoreDisasterFoodMarketImpact(std::vector<StockItem>& stockMarket, AuthManager& authManager) {
    // Reset raw meats and cooked foods back to equilibrium (Qd = Qs = 100, Pnew = basePrice)
    std::vector<int> foodItems = { ITEM_CHICKEN_MEAT, ITEM_PIG_MEAT, ITEM_SHEEP_MEAT, ITEM_ZOMBIE_FLESH,
                                   ITEM_COOKED_CHICKEN, ITEM_COOKED_PIG, ITEM_COOKED_SHEEP };
    for (int id : foodItems) {
        for (auto& item : stockMarket) {
            if (item.id == id) {
                item.demand = item.baseQuantity;
                item.supply = item.baseQuantity;
                item.quantity = item.supply;

                if (item.history.size() >= 10) item.history.erase(item.history.begin());
                item.history.push_back(item.currentPrice);

                item.currentPrice = item.basePrice;

                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));
                item.trend = "STABLE";
                break;
            }
        }
    }
    saveAllMarketPricesAsync(authManager, stockMarket);

    std::cout << "[MARKET] Natural disaster ended: raw meat and food prices restored to normal base prices.\n";
}

void applyDroughtMarketImpact(std::vector<StockItem>& stockMarket, AuthManager& authManager) {
    // Severe drought makes wood scarce (Qs drops severely) -> Price surges
    // Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
    // p = price adjustment factor (Disaster Impact)
    for (auto& item : stockMarket) {
        if (item.id == ITEM_WOOD) {
            float Pold = item.currentPrice;
            item.supply = 50.f; // Supply halved
            item.quantity = item.supply;
            item.demand = 100.f;
            float p = 0.95f + ((rand() % 25) / 100.f); // Disaster price adjustment factor

            float newPrice = calculateMarketPrice(Pold, item.demand, item.supply, p);

            if (item.history.size() >= 10) item.history.erase(item.history.begin());
            item.history.push_back(Pold);
            item.currentPrice = newPrice;

            float lastPrice = item.history.back();
            float diff = item.currentPrice - lastPrice;
            item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));
            item.trend = "UP";
            break;
        }
    }

    // Wildlife perished and livestock dehydrated -> Meats supply Qs drops
    std::vector<int> meats = {
        ITEM_CHICKEN_MEAT, ITEM_PIG_MEAT, ITEM_SHEEP_MEAT,
        ITEM_COOKED_CHICKEN, ITEM_COOKED_PIG, ITEM_COOKED_SHEEP
    };
    for (int id : meats) {
        for (auto& item : stockMarket) {
            if (item.id == id) {
                float Pold = item.currentPrice;
                item.supply = 52.f; // Supply drops
                item.quantity = item.supply;
                item.demand = 100.f;
                float p = 0.90f + ((rand() % 30) / 100.f); // Disaster price adjustment factor

                float newPrice = calculateMarketPrice(Pold, item.demand, item.supply, p);

                if (item.history.size() >= 10) item.history.erase(item.history.begin());
                item.history.push_back(Pold);
                item.currentPrice = newPrice;

                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));
                item.trend = "UP";
                break;
            }
        }
    }
    saveAllMarketPricesAsync(authManager, stockMarket);

    std::cout << "[MARKET] Severe drought applied: Wood and all meat prices (raw and cooked) have surged drastically!\n";
}

void restoreDroughtMarketImpact(std::vector<StockItem>& stockMarket, AuthManager& authManager) {
    std::vector<int> impactedItems = {
        ITEM_WOOD,
        ITEM_CHICKEN_MEAT, ITEM_PIG_MEAT, ITEM_SHEEP_MEAT,
        ITEM_COOKED_CHICKEN, ITEM_COOKED_PIG, ITEM_COOKED_SHEEP
    };

    for (int id : impactedItems) {
        for (auto& item : stockMarket) {
            if (item.id == id) {
                item.demand = item.baseQuantity;
                item.supply = item.baseQuantity;
                item.quantity = item.supply;

                if (item.history.size() >= 10) item.history.erase(item.history.begin());
                item.history.push_back(item.currentPrice);

                item.currentPrice = item.basePrice;

                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f));
                item.trend = "STABLE";
                break;
            }
        }
    }
    saveAllMarketPricesAsync(authManager, stockMarket);

    std::cout << "[MARKET] Severe drought ended: Wood and meat prices (raw & cooked) restored to normal base prices.\n";
}

void runVirtualPlayerMarketGenerator(std::vector<StockItem>& stockMarket, AuthManager& authManager) {
    std::vector<std::string> virtualPlayers = { "MinerBob", "LumberjackJack", "HunterMax", "SwordSmith", "GoldDigger", "FarmerFred" };

    // In each 30-minute cycle, generate 2-4 virtual player transactions that impact the market stats
    int numTrades = 2 + (rand() % 3); // 2 to 4 trades per cycle
    for (int t = 0; t < numTrades; ++t) {
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
            { ITEM_WOOD_STAIRS, "Wood Stairs" },
            { ITEM_COAL, "Coal" },
            { ITEM_COPPER_ORE, "Copper Ore" },
            { ITEM_COPPER_BAR, "Copper Bar" },
            { ITEM_IRON_ORE, "Iron Ore" },
            { ITEM_IRON_BAR, "Iron Bar" },
            { ITEM_GOLD_ORE, "Gold Ore" },
            { ITEM_GOLD_BAR, "Gold Bar" },
            { ITEM_DIAMOND, "Diamond" }
        };

        auto chosenItem = vItems[rand() % vItems.size()];
        int itemId = chosenItem.first;
        std::string itemName = chosenItem.second;
        int qty = 1 + rand() % 5;
        int unitPrice = getMarketItemPrice(itemId, stockMarket);
        int totalG = qty * unitPrice;

        // Log trade record in AuthManager database for the recent trade ticker
        authManager.addTradeRecord(vPlayer.c_str(), vAction, itemName, qty, unitPrice, totalG);

        // Update market stats for the chosen item using:
        // Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
        for (auto& item : stockMarket) {
            if (item.id == itemId) {
                float Pold = item.currentPrice;

                // A buy increases demand (Qd); a sell increases supply (Qs)
                if (vAction == "BUY") {
                    item.demand = std::clamp(item.demand + qty * 2.5f, 20.f, 300.f);
                } else {
                    item.supply = std::clamp(item.supply + qty * 2.5f, 20.f, 300.f);
                    item.quantity = item.supply;
                }

                // p = price adjustment factor (Virtual Player Market Stats Generator)
                float p = 0.5f * item.volatility;
                float Pnew = calculateMarketPrice(Pold, item.demand, item.supply, p);

                if (item.history.size() >= 10) item.history.erase(item.history.begin());
                item.history.push_back(Pold);
                item.currentPrice = Pnew;

                float lastPrice = item.history.back();
                float diff = item.currentPrice - lastPrice;
                float baseNoise = 8.f * item.volatility;
                int noiseRange = (int)(baseNoise * 2.f) + 1;
                float noise = (-(int)baseNoise + (rand() % (noiseRange > 0 ? noiseRange : 1))) / 100.f;
                item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f + item.currentPrice * noise));

                float trendThreshold = item.volatility > 1.2f ? 0.08f : 0.04f;
                if (item.predictedPrice > item.currentPrice * (1.f + trendThreshold)) item.trend = "UP";
                else if (item.predictedPrice < item.currentPrice * (1.f - trendThreshold)) item.trend = "DOWN";
                else item.trend = "STABLE";

                break;
            }
        }
    }
    saveAllMarketPricesAsync(authManager, stockMarket);
}

void updateStockMarketSimulation(std::vector<StockItem>& stockMarket, AuthManager& authManager) {
    // 1. General periodic market fluctuations using the formula:
    // Pnew = Pold * (1 + p * (Qd - Qs) / Qs)
    for (auto& item : stockMarket) {
        float Pold = item.currentPrice;

        // Natural shifts in demand and supply
        float baseRange = 10.f * item.volatility;
        int rangeInt = (int)(baseRange * 2.f) + 1;
        float shift = (-(int)baseRange + (rand() % (rangeInt > 0 ? rangeInt : 1)));

        // Gentle mean reversion towards equilibrium 100.f
        item.demand += (100.f - item.demand) * 0.1f;
        item.supply += (100.f - item.supply) * 0.1f;

        item.demand = std::clamp(item.demand + shift, 20.f, 250.f);
        item.supply = std::clamp(item.supply, 20.f, 250.f);
        item.quantity = item.supply;

        // p = price adjustment factor
        float p = item.volatility * 0.5f;
        float Pnew = calculateMarketPrice(Pold, item.demand, item.supply, p);

        if (item.history.size() >= 10) item.history.erase(item.history.begin());
        item.history.push_back(Pold);
        item.currentPrice = Pnew;

        if (item.history.size() >= 2) {
            float lastPrice = item.history.back();
            float diff = item.currentPrice - lastPrice;
            float baseNoise = 8.f * item.volatility;
            int noiseRange = (int)(baseNoise * 2.f) + 1;
            float noise = (-(int)baseNoise + (rand() % (noiseRange > 0 ? noiseRange : 1))) / 100.f;
            item.predictedPrice = std::max(1.f, std::round(item.currentPrice + diff * 0.5f + item.currentPrice * noise));

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
    saveAllMarketPricesAsync(authManager, stockMarket);

    // 2. Run Virtual Player Market Stats Generator (Cycle every 30 minutes)
    runVirtualPlayerMarketGenerator(stockMarket, authManager);
    std::cout << "[MARKET] 30-minute virtual player market stats generator cycle complete.\n";
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
    const std::vector<TradeRecord>& recentTrades,
    bool isAdmin
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

    // Admin Simulator Shortcut Button (Admin only)
    if (isAdmin) {
        sf::RectangleShape simBtn({ 150.f, 36.f });
        simBtn.setPosition({ bx + 700.f, by + 18.f });
        bool simHov = simBtn.getGlobalBounds().contains(mousePos);
        simBtn.setFillColor(simHov ? sf::Color(70, 30, 110, 240) : sf::Color(50, 20, 85, 220));
        simBtn.setOutlineThickness(1.5f);
        simBtn.setOutlineColor(sf::Color(255, 215, 0));
        window.draw(simBtn);

        sf::Text simTxt(uiFont, "ADMIN SIM", 15);
        simTxt.setStyle(sf::Text::Bold);
        simTxt.setFillColor(sf::Color(255, 225, 120));
        simTxt.setPosition({ bx + 730.f, by + 26.f });
        window.draw(simTxt);
    }

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
                        sm.id == ITEM_WOOD_ROOF || sm.id == ITEM_WOOD_STAIRS || sm.id == ITEM_BED ||
                        sm.id == ITEM_COPPER_BAR || sm.id == ITEM_IRON_BAR || sm.id == ITEM_GOLD_BAR);
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
        else if (sm.id == ITEM_COAL)           di.tex = textures.coalTex;
        else if (sm.id == ITEM_COPPER_ORE)     di.tex = textures.copperOreTex;
        else if (sm.id == ITEM_COPPER_BAR)     di.tex = textures.copperBarTex;
        else if (sm.id == ITEM_IRON_ORE)       di.tex = textures.ironOreTex;
        else if (sm.id == ITEM_IRON_BAR)       di.tex = textures.ironBarTex;
        else if (sm.id == ITEM_GOLD_ORE)       di.tex = textures.goldOreTex;
        else if (sm.id == ITEM_GOLD_BAR)       di.tex = textures.goldBarTex;
        else if (sm.id == ITEM_DIAMOND)        di.tex = textures.diamondTex;
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

void drawAdminQuantitySimulator(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::vector<StockItem>& stockMarket,
    int scrollOffset,
    const MarketTextures& textures
) {
    sf::View old = window.getView();
    window.setView(GetUIView(&window));
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    // Dim background overlay
    sf::RectangleShape dimOverlay({ (float)width, (float)height });
    dimOverlay.setFillColor(sf::Color(0, 0, 0, 195));
    window.draw(dimOverlay);

    // Modal frame
    float boxW = 1000.f;
    float boxH = 680.f;
    float bx = ((float)width - boxW) / 2.f;
    float by = ((float)height - boxH) / 2.f;

    sf::RectangleShape box({ boxW, boxH });
    box.setPosition({ bx, by });
    box.setFillColor(sf::Color(16, 20, 30, 252));
    box.setOutlineThickness(2.f);
    box.setOutlineColor(sf::Color(255, 205, 60));
    window.draw(box);

    // Header Title
    sf::Text title(uiFont, "ADMIN SUPPLY & DEMAND SIMULATOR", 24);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(sf::Color(255, 215, 0));
    title.setPosition({ bx + 24.f, by + 16.f });
    window.draw(title);

    // Subtitle
    sf::Text subtitle(uiFont, "Lifetime Price & Circulation Supply Controls | Scarcity (Low Qty) -> Price Surges | Surplus (High Qty) -> Price Drops", 13);
    subtitle.setFillColor(sf::Color(155, 185, 225));
    subtitle.setPosition({ bx + 24.f, by + 46.f });
    window.draw(subtitle);

    // Close button [X]
    sf::RectangleShape closeBtn({ 34.f, 34.f });
    closeBtn.setPosition({ bx + boxW - 48.f, by + 16.f });
    bool closeHov = closeBtn.getGlobalBounds().contains(mousePos);
    closeBtn.setFillColor(closeHov ? sf::Color(240, 70, 70) : sf::Color(180, 40, 40));
    closeBtn.setOutlineThickness(closeHov ? 2.f : 0.f);
    closeBtn.setOutlineColor(sf::Color::White);
    window.draw(closeBtn);

    sf::Text closeTxt(uiFont, "X", 18);
    closeTxt.setStyle(sf::Text::Bold);
    closeTxt.setPosition({ bx + boxW - 38.f, by + 21.f });
    window.draw(closeTxt);

    // Column Headers Bar
    sf::RectangleShape headerBar({ boxW - 48.f, 30.f });
    headerBar.setPosition({ bx + 24.f, by + 72.f });
    headerBar.setFillColor(sf::Color(26, 33, 50, 230));
    headerBar.setOutlineThickness(1.f);
    headerBar.setOutlineColor(sf::Color(55, 65, 95));
    window.draw(headerBar);

    auto drawColHeader = [&](const std::string& txt, float xOffset) {
        sf::Text ht(uiFont, txt, 13);
        ht.setStyle(sf::Text::Bold);
        ht.setFillColor(sf::Color(200, 215, 240));
        ht.setPosition({ bx + xOffset, by + 78.f });
        window.draw(ht);
    };

    drawColHeader("Item", 35.f);
    drawColHeader("Lifetime Vol.", 230.f);
    drawColHeader("Circulation Supply (Adjust)", 355.f);
    drawColHeader("Base Price", 620.f);
    drawColHeader("Live Price", 715.f);
    drawColHeader("Market Status", 830.f);

    // Rows
    float listTop = by + 106.f;
    float rowH = 48.f;
    int maxVisible = 9;
    int totalRows = (int)stockMarket.size();
    int maxScroll = std::max(0, totalRows - maxVisible);
    int curScroll = std::clamp(scrollOffset, 0, maxScroll);

    float colQuantity = bx + 355.f;

    for (int i = 0; i < maxVisible; ++i) {
        int idx = curScroll + i;
        if (idx >= totalRows) break;

        const auto& item = stockMarket[idx];
        float ry = listTop + (float)i * rowH;

        // Row background
        sf::RectangleShape rowBg({ boxW - 48.f, rowH - 2.f });
        rowBg.setPosition({ bx + 24.f, ry });
        rowBg.setFillColor((i % 2 == 0) ? sf::Color(20, 26, 38, 200) : sf::Color(26, 34, 50, 200));
        window.draw(rowBg);

        // Item Texture / Icon
        const sf::Texture* itex = nullptr;
        if (item.id == ITEM_WOOD)               itex = textures.woodTex;
        else if (item.id == ITEM_STONE)          itex = textures.stoneTex;
        else if (item.id == ITEM_SAPLING)        itex = textures.saplingTex;
        else if (item.id == ITEM_CHICKEN_MEAT)   itex = ChickenMeat::getTexture();
        else if (item.id == ITEM_PIG_MEAT)       itex = PigMeat::getTexture();
        else if (item.id == ITEM_SHEEP_MEAT)     itex = SheepMeat::getTexture();
        else if (item.id == ITEM_WOOL)           itex = textures.woolTex;
        else if (item.id == ITEM_ZOMBIE_FLESH)   itex = ZombieFlesh::getTexture();
        else if (item.id == ITEM_COOKED_CHICKEN) itex = textures.cookedChickenTex;
        else if (item.id == ITEM_COOKED_PIG)     itex = textures.cookedPigTex;
        else if (item.id == ITEM_COOKED_SHEEP)   itex = textures.cookedSheepTex;
        else if (item.id == ITEM_WOODEN_SWORD)   itex = textures.woodenSwordTex;
        else if (item.id == ITEM_SHIELD)         itex = textures.shieldTex;
        else if (item.id == ITEM_FURNACE)        itex = textures.furnaceTex;
        else if (item.id == ITEM_WOOD_TILE)      itex = textures.woodTileTex;
        else if (item.id == ITEM_WOOD_WALL)      itex = textures.woodWallTex;
        else if (item.id == ITEM_WOOD_DOOR)      itex = textures.woodDoorTex;
        else if (item.id == ITEM_WOOD_ROOF)      itex = textures.woodRoofTex;
        else if (item.id == ITEM_WOOD_STAIRS)    itex = textures.woodStairsTex;
        else if (item.id == ITEM_BED)            itex = textures.bedTex;
        else if (item.id == ITEM_COAL)           itex = textures.coalTex;
        else if (item.id == ITEM_COPPER_ORE)     itex = textures.copperOreTex;
        else if (item.id == ITEM_COPPER_BAR)     itex = textures.copperBarTex;
        else if (item.id == ITEM_IRON_ORE)       itex = textures.ironOreTex;
        else if (item.id == ITEM_IRON_BAR)       itex = textures.ironBarTex;
        else if (item.id == ITEM_GOLD_ORE)       itex = textures.goldOreTex;
        else if (item.id == ITEM_GOLD_BAR)       itex = textures.goldBarTex;
        else if (item.id == ITEM_DIAMOND)        itex = textures.diamondTex;

        if (itex) {
            sf::Sprite ispr(*itex);
            float scale = 30.f / std::max(1u, std::max(itex->getSize().x, itex->getSize().y));
            ispr.setScale({ scale, scale });
            ispr.setPosition({ bx + 32.f, ry + 8.f });
            window.draw(ispr);
        }

        // Item Name
        sf::Text nameTxt(uiFont, item.name, 15);
        nameTxt.setStyle(sf::Text::Bold);
        nameTxt.setFillColor(sf::Color::White);
        nameTxt.setPosition({ bx + 70.f, ry + 14.f });
        window.draw(nameTxt);

        // Lifetime Volume
        sf::Text volTxt(uiFont, std::to_string(item.lifetimeVolume) + " units", 14);
        volTxt.setFillColor(sf::Color(170, 195, 230));
        volTxt.setPosition({ bx + 230.f, ry + 15.f });
        window.draw(volTxt);

        // --- Controls: [-10], [-1], [ Quantity Box ], [+1], [+10] ---
        // [-10] Button
        sf::RectangleShape btnMinus10({ 38.f, 28.f });
        btnMinus10.setPosition({ colQuantity, ry + 10.f });
        bool m10Hov = btnMinus10.getGlobalBounds().contains(mousePos);
        btnMinus10.setFillColor(m10Hov ? sf::Color(175, 45, 45) : sf::Color(125, 30, 30));
        btnMinus10.setOutlineThickness(1.f);
        btnMinus10.setOutlineColor(sf::Color(200, 60, 60));
        window.draw(btnMinus10);
        sf::Text txtMinus10(uiFont, "-10", 12);
        txtMinus10.setStyle(sf::Text::Bold);
        txtMinus10.setFillColor(sf::Color::White);
        txtMinus10.setPosition({ colQuantity + 6.f, ry + 15.f });
        window.draw(txtMinus10);

        // [-1] Button
        sf::RectangleShape btnMinus1({ 32.f, 28.f });
        btnMinus1.setPosition({ colQuantity + 43.f, ry + 10.f });
        bool m1Hov = btnMinus1.getGlobalBounds().contains(mousePos);
        btnMinus1.setFillColor(m1Hov ? sf::Color(180, 75, 45) : sf::Color(135, 45, 30));
        btnMinus1.setOutlineThickness(1.f);
        btnMinus1.setOutlineColor(sf::Color(210, 95, 60));
        window.draw(btnMinus1);
        sf::Text txtMinus1(uiFont, "-1", 13);
        txtMinus1.setStyle(sf::Text::Bold);
        txtMinus1.setFillColor(sf::Color::White);
        txtMinus1.setPosition({ colQuantity + 50.f, ry + 14.f });
        window.draw(txtMinus1);

        // Quantity Box
        sf::RectangleShape qtyBox({ 62.f, 28.f });
        qtyBox.setPosition({ colQuantity + 80.f, ry + 10.f });
        qtyBox.setFillColor(sf::Color(10, 14, 22));
        qtyBox.setOutlineThickness(1.f);
        qtyBox.setOutlineColor(sf::Color(90, 120, 170));
        window.draw(qtyBox);
        sf::Text qtyTxt(uiFont, std::to_string((int)item.quantity), 15);
        qtyTxt.setStyle(sf::Text::Bold);
        qtyTxt.setFillColor(sf::Color(255, 230, 140));
        sf::FloatRect qtyBounds = qtyTxt.getLocalBounds();
        qtyTxt.setPosition({ colQuantity + 80.f + (62.f - qtyBounds.size.x) / 2.f - qtyBounds.position.x, ry + 13.f });
        window.draw(qtyTxt);

        // [+1] Button
        sf::RectangleShape btnPlus1({ 32.f, 28.f });
        btnPlus1.setPosition({ colQuantity + 147.f, ry + 10.f });
        bool p1Hov = btnPlus1.getGlobalBounds().contains(mousePos);
        btnPlus1.setFillColor(p1Hov ? sf::Color(45, 150, 75) : sf::Color(30, 110, 55));
        btnPlus1.setOutlineThickness(1.f);
        btnPlus1.setOutlineColor(sf::Color(65, 185, 95));
        window.draw(btnPlus1);
        sf::Text txtPlus1(uiFont, "+1", 13);
        txtPlus1.setStyle(sf::Text::Bold);
        txtPlus1.setFillColor(sf::Color::White);
        txtPlus1.setPosition({ colQuantity + 154.f, ry + 14.f });
        window.draw(txtPlus1);

        // [+10] Button
        sf::RectangleShape btnPlus10({ 38.f, 28.f });
        btnPlus10.setPosition({ colQuantity + 184.f, ry + 10.f });
        bool p10Hov = btnPlus10.getGlobalBounds().contains(mousePos);
        btnPlus10.setFillColor(p10Hov ? sf::Color(40, 165, 85) : sf::Color(25, 120, 60));
        btnPlus10.setOutlineThickness(1.f);
        btnPlus10.setOutlineColor(sf::Color(55, 200, 100));
        window.draw(btnPlus10);
        sf::Text txtPlus10(uiFont, "+10", 12);
        txtPlus10.setStyle(sf::Text::Bold);
        txtPlus10.setFillColor(sf::Color::White);
        txtPlus10.setPosition({ colQuantity + 189.f, ry + 15.f });
        window.draw(txtPlus10);

        // Base Price
        sf::Text baseTxt(uiFont, std::to_string((int)item.basePrice) + " G", 15);
        baseTxt.setFillColor(sf::Color(140, 150, 175));
        baseTxt.setPosition({ bx + 620.f, ry + 15.f });
        window.draw(baseTxt);

        // Live Simulated Price
        sf::Text liveTxt(uiFont, std::to_string((int)item.currentPrice) + " G", 16);
        liveTxt.setStyle(sf::Text::Bold);
        if (item.currentPrice > item.basePrice) {
            liveTxt.setFillColor(sf::Color(255, 215, 0)); // Gold surge
        } else if (item.currentPrice < item.basePrice) {
            liveTxt.setFillColor(sf::Color(100, 205, 255)); // Cyan discount
        } else {
            liveTxt.setFillColor(sf::Color(230, 230, 230)); // Equilibrium
        }
        liveTxt.setPosition({ bx + 715.f, ry + 14.f });
        window.draw(liveTxt);

        // Market Status Badge
        sf::RectangleShape badgeBg({ 125.f, 26.f });
        badgeBg.setPosition({ bx + 830.f, ry + 11.f });
        std::string badgeLabel;
        sf::Color badgeColor;

        if (item.quantity < 80.f) {
            badgeBg.setFillColor(sf::Color(75, 20, 20, 210));
            badgeBg.setOutlineThickness(1.5f);
            badgeBg.setOutlineColor(sf::Color(240, 80, 80));
            badgeLabel = "▲ HIGH DEMAND";
            badgeColor = sf::Color(255, 120, 120);
        } else if (item.quantity > 120.f) {
            badgeBg.setFillColor(sf::Color(20, 45, 75, 210));
            badgeBg.setOutlineThickness(1.5f);
            badgeBg.setOutlineColor(sf::Color(70, 160, 240));
            badgeLabel = "▼ SURPLUS";
            badgeColor = sf::Color(120, 200, 255);
        } else {
            badgeBg.setFillColor(sf::Color(20, 60, 40, 210));
            badgeBg.setOutlineThickness(1.5f);
            badgeBg.setOutlineColor(sf::Color(60, 180, 100));
            badgeLabel = "● BALANCED";
            badgeColor = sf::Color(120, 240, 150);
        }
        window.draw(badgeBg);

        sf::Text badgeTxt(uiFont, badgeLabel, 11);
        badgeTxt.setStyle(sf::Text::Bold);
        badgeTxt.setFillColor(badgeColor);
        sf::FloatRect bb = badgeTxt.getLocalBounds();
        badgeTxt.setPosition({ bx + 830.f + (125.f - bb.size.x) / 2.f - bb.position.x, ry + 16.f });
        window.draw(badgeTxt);
    }

    // Scrollbar Track & Thumb
    if (totalRows > maxVisible) {
        float trackX = bx + boxW - 14.f;
        float trackY = listTop;
        float trackH = (float)maxVisible * rowH;

        sf::RectangleShape track({ 6.f, trackH });
        track.setPosition({ trackX, trackY });
        track.setFillColor(sf::Color(35, 42, 60));
        window.draw(track);

        float thumbRatio = (float)maxVisible / (float)totalRows;
        float thumbH = std::max(24.f, trackH * thumbRatio);
        float thumbY = trackY + (trackH - thumbH) * ((float)curScroll / (float)maxScroll);

        sf::RectangleShape thumb({ 6.f, thumbH });
        thumb.setPosition({ trackX, thumbY });
        thumb.setFillColor(sf::Color(120, 150, 220));
        window.draw(thumb);
    }

    // Bottom Presets Bar
    float botY = by + boxH - 52.f;

    // Preset: Reset All (100)
    sf::RectangleShape btnReset({ 165.f, 36.f });
    btnReset.setPosition({ bx + 24.f, botY });
    bool resHov = btnReset.getGlobalBounds().contains(mousePos);
    btnReset.setFillColor(resHov ? sf::Color(55, 75, 110) : sf::Color(35, 50, 75));
    btnReset.setOutlineThickness(1.5f);
    btnReset.setOutlineColor(sf::Color(100, 140, 210));
    window.draw(btnReset);
    sf::Text txtReset(uiFont, "Reset Normal (100)", 13);
    txtReset.setStyle(sf::Text::Bold);
    txtReset.setFillColor(sf::Color::White);
    txtReset.setPosition({ bx + 36.f, botY + 9.f });
    window.draw(txtReset);

    // Preset: Scarcity Crisis (25)
    sf::RectangleShape btnScarcity({ 175.f, 36.f });
    btnScarcity.setPosition({ bx + 200.f, botY });
    bool scaHov = btnScarcity.getGlobalBounds().contains(mousePos);
    btnScarcity.setFillColor(scaHov ? sf::Color(140, 40, 40) : sf::Color(95, 25, 25));
    btnScarcity.setOutlineThickness(1.5f);
    btnScarcity.setOutlineColor(sf::Color(220, 70, 70));
    window.draw(btnScarcity);
    sf::Text txtScarcity(uiFont, "Scarcity Crisis (25)", 13);
    txtScarcity.setStyle(sf::Text::Bold);
    txtScarcity.setFillColor(sf::Color(255, 200, 200));
    txtScarcity.setPosition({ bx + 215.f, botY + 9.f });
    window.draw(txtScarcity);

    // Preset: Market Surplus (250)
    sf::RectangleShape btnSurplus({ 175.f, 36.f });
    btnSurplus.setPosition({ bx + 386.f, botY });
    bool surHov = btnSurplus.getGlobalBounds().contains(mousePos);
    btnSurplus.setFillColor(surHov ? sf::Color(30, 95, 135) : sf::Color(20, 65, 95));
    btnSurplus.setOutlineThickness(1.5f);
    btnSurplus.setOutlineColor(sf::Color(60, 170, 230));
    window.draw(btnSurplus);
    sf::Text txtSurplus(uiFont, "Market Surplus (250)", 13);
    txtSurplus.setStyle(sf::Text::Bold);
    txtSurplus.setFillColor(sf::Color(190, 235, 255));
    txtSurplus.setPosition({ bx + 400.f, botY + 9.f });
    window.draw(txtSurplus);

    // Supply & Demand Formula Note
    sf::Text formulaTxt(uiFont, "Formula: Pnew = Pold * (1 + p * (Qd - Qs) / Qs)", 13);
    formulaTxt.setFillColor(sf::Color(160, 175, 205));
    formulaTxt.setPosition({ bx + 575.f, botY + 10.f });
    window.draw(formulaTxt);

    window.setView(old);
}

bool handleAdminQuantitySimulatorClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    std::vector<StockItem>& stockMarket,
    int& scrollOffset,
    bool& adminSimOpen,
    AuthManager& authManager
) {
    if (!adminSimOpen) return false;

    float boxW = 1000.f;
    float boxH = 680.f;
    float bx = ((float)width - boxW) / 2.f;
    float by = ((float)height - boxH) / 2.f;
    sf::FloatRect boxRect({ bx, by }, { boxW, boxH });

    if (!boxRect.contains(uiPos)) {
        adminSimOpen = false;
        return true;
    }

    // Close button [X]
    sf::FloatRect closeBtnRect({ bx + boxW - 48.f, by + 16.f }, { 34.f, 34.f });
    if (closeBtnRect.contains(uiPos)) {
        adminSimOpen = false;
        return true;
    }

    float botY = by + boxH - 52.f;

    // Reset Normal (100)
    sf::FloatRect resetRect({ bx + 24.f, botY }, { 165.f, 36.f });
    if (resetRect.contains(uiPos)) {
        resetAllItemQuantities(stockMarket, 100.f, authManager);
        return true;
    }

    // Scarcity Crisis (25)
    sf::FloatRect scarcityRect({ bx + 200.f, botY }, { 175.f, 36.f });
    if (scarcityRect.contains(uiPos)) {
        resetAllItemQuantities(stockMarket, 25.f, authManager);
        return true;
    }

    // Market Surplus (250)
    sf::FloatRect surplusRect({ bx + 386.f, botY }, { 175.f, 36.f });
    if (surplusRect.contains(uiPos)) {
        resetAllItemQuantities(stockMarket, 250.f, authManager);
        return true;
    }

    // Row Buttons
    float listTop = by + 106.f;
    float rowH = 48.f;
    int maxVisible = 9;
    int totalRows = (int)stockMarket.size();
    int maxScroll = std::max(0, totalRows - maxVisible);
    int curScroll = std::clamp(scrollOffset, 0, maxScroll);
    float colQuantity = bx + 355.f;

    for (int i = 0; i < maxVisible; ++i) {
        int idx = curScroll + i;
        if (idx >= totalRows) break;

        auto& item = stockMarket[idx];
        float ry = listTop + (float)i * rowH;

        // [-10] Button
        sf::FloatRect m10Rect({ colQuantity, ry + 10.f }, { 38.f, 28.f });
        if (m10Rect.contains(uiPos)) {
            adjustItemQuantitySupply(stockMarket, item.id, -10.f, authManager);
            return true;
        }

        // [-1] Button
        sf::FloatRect m1Rect({ colQuantity + 43.f, ry + 10.f }, { 32.f, 28.f });
        if (m1Rect.contains(uiPos)) {
            adjustItemQuantitySupply(stockMarket, item.id, -1.f, authManager);
            return true;
        }

        // [+1] Button
        sf::FloatRect p1Rect({ colQuantity + 147.f, ry + 10.f }, { 32.f, 28.f });
        if (p1Rect.contains(uiPos)) {
            adjustItemQuantitySupply(stockMarket, item.id, +1.f, authManager);
            return true;
        }

        // [+10] Button
        sf::FloatRect p10Rect({ colQuantity + 184.f, ry + 10.f }, { 38.f, 28.f });
        if (p10Rect.contains(uiPos)) {
            adjustItemQuantitySupply(stockMarket, item.id, +10.f, authManager);
            return true;
        }
    }

    return true;
}