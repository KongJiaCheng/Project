#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include "Button.h"

class CharacterSelection
{
private:
    Button button1;
    Button button2;
    sf::Text titleText;

public:
    CharacterSelection(const sf::Font& font, float width, float height);
    void draw(sf::RenderWindow& window);
    bool handleEvent(
        const sf::RenderWindow& window,
        const sf::Event& event
    );
    bool isMaleButtonHovered(const sf::Vector2f& uiPos, const sf::Vector2f& mousePos) const;
    bool isFemaleButtonHovered(const sf::Vector2f& uiPos, const sf::Vector2f& mousePos) const;
    void updateHover(const sf::Vector2f& mousePos);
    Button& getMaleButton() { return button1; }
    Button& getFemaleButton() { return button2; }
};

#include "Animation.h"
#include "AuthManager.h"
#include "StructureSystem.h"
#include "GameAssets.h"
#include "PlayerController.h"
#include <functional>

bool handleCharacterSelectionHydration(
    sf::RenderWindow& window,
    unsigned int width,
    unsigned int height,
    int option,
    const std::string& username,
    bool dbStartupEnabled,
    AuthManager& authManager,
    const sf::Font& font,
    sf::Sprite& sprite,
    AnimSet& animWalk,
    AnimSet& animWalkWithSword,
    AnimSet& animWalkWithShield,
    AnimSet& animWalkWithShieldAndSword,
    AnimSet& animAttackWithSword,
    AnimSet& animAttackWithShieldAndSword,
    int framesPerDir,
    const PlayerStats& stats,
    float& hunger,
    float& thirst,
    int& currency,
    int& level,
    int& exp,
    float& timeOffset,
    float& elapsedGameTime,
    int& hungerMultiplier,
    int& thirstMultiplier,
    sf::Text& hungerMultiplierText,
    sf::Text& thirstMultiplierText,
    std::vector<PlacedStructure>& structures,
    std::vector<PlacedCampfire>& campfires,
    Inventory& inventory,
    GameAssets& assets,
    std::function<sf::Vector2f(sf::Vector2f)> snapToGrid,
    NPC& npcFemale,
    NPC& npcMale,
    unsigned int mapWidth,
    unsigned int mapHeight,
    const sf::Texture& texMaleOnline,
    const sf::Texture& texFemaleOnline
);
