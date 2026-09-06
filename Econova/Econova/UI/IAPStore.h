#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "TextInput.h"

struct IAPCardData {
    std::string title;
    std::string desc;
    int gold;
    float price;
    sf::Color color;
};

std::vector<IAPCardData> getIAPCards();

void drawIAPStore(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    int currency
);

void drawIAPVerifying(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::string& iapVerifyingTier,
    int iapVerifyingGold,
    float iapVerifyingPrice,
    TextInput& iapCodeInput
);

#include <functional>

bool handleIAPStoreClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& iapPageOpen,
    bool& iapVerifyingOpen,
    int& iapVerifyingGold,
    float& iapVerifyingPrice,
    std::string& iapVerifyingTier,
    TextInput& iapCodeInput,
    const std::string& username
);

bool handleIAPVerifyingClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& iapVerifyingOpen,
    int iapVerifyingGold,
    const std::string& iapVerifyingTier,
    TextInput& iapCodeInput,
    const std::string& username,
    int& currency,
    std::function<void()> savePlayerStatsCallback
);

