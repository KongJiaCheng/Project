#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "Inventory.h"

struct CraftableItem {
    int id;
    std::string name;
    int woodReq;
    int stoneReq;
    int woolReq;
    const sf::Texture* tex;
};

void drawCraftMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::vector<CraftableItem>& craftableItems,
    const Inventory& inventory,
    const sf::Texture& woodTex,
    const sf::Texture& stoneTex,
    const sf::Texture& woolTex,
    bool swordCrafting,
    bool shieldCrafting
);

void drawPlacementControls(
    sf::RenderWindow& window,
    const sf::Font& font,
    unsigned int width,
    unsigned int height,
    const Inventory& inventory,
    const sf::Texture& woodTex,
    const sf::Texture& stoneTex
);

void drawSaplingPlacementControls(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    float saplingRotation
);

bool handleCraftMenuClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& craftMenuOpen,
    const std::vector<CraftableItem>& craftableItems,
    Inventory& inventory,
    bool& swordCrafting,
    float& swordCraftTimer,
    bool& shieldCrafting,
    float& shieldCraftTimer
);
