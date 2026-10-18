#include "CharacterSelection.h"
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

CharacterSelection::CharacterSelection(
    const sf::Font& font,
    float width,
    float height
)
    : button1(font, "Male", { 420.f, 120.f }, 55),
      button2(font, "Female", { 420.f, 120.f }, 55),
      titleText(font, "Choose Your Character", 70)
{
    float uiW = 1920.f;
    float uiH = 1080.f;

    titleText.setFillColor(sf::Color::White);
    titleText.setOutlineColor(sf::Color::Black);
    titleText.setOutlineThickness(3.f);
    titleText.setPosition({ (uiW - titleText.getLocalBounds().size.x) / 2.f, 220.f });

    button1.setFillColor(sf::Color(39, 174, 96));
    button1.setTextColor(sf::Color::White);
    button1.setPosition({
        uiW / 2.f - 420.f - 40.f,
        480.f
    });

    button2.setFillColor(sf::Color(39, 174, 96));
    button2.setTextColor(sf::Color::White);
    button2.setPosition({
        uiW / 2.f + 40.f,
        480.f
    });
}

void CharacterSelection::updateHover(const sf::Vector2f& mousePos)
{
    bool hover1 = button1.getGlobalBounds().contains(mousePos);
    button1.setFillColor(hover1 ? sf::Color(46, 204, 113) : sf::Color(39, 174, 96));
    button1.setOutlineThickness(hover1 ? 4.f : 0.f);
    button1.setOutlineColor(sf::Color::White);

    bool hover2 = button2.getGlobalBounds().contains(mousePos);
    button2.setFillColor(hover2 ? sf::Color(46, 204, 113) : sf::Color(39, 174, 96));
    button2.setOutlineThickness(hover2 ? 4.f : 0.f);
    button2.setOutlineColor(sf::Color::White);
}

void CharacterSelection::draw(sf::RenderWindow& window)
{
    window.draw(titleText);
    button1.draw(window);
    button2.draw(window);
}

bool CharacterSelection::handleEvent(
    const sf::RenderWindow& window,
    const sf::Event& event
)
{
    if (button1.isClicked(window, event))
    {
        // Male selected
        return true;
    }

    if (button2.isClicked(window, event))
    {
        // Female selected
        return true;
    }

    return false;
}

bool CharacterSelection::isMaleButtonHovered(const sf::Vector2f& uiPos, const sf::Vector2f &mousePos) const
{
    return button1.getGlobalBounds().contains(uiPos);
}

bool CharacterSelection::isFemaleButtonHovered(const sf::Vector2f& uiPos, const sf::Vector2f &mousePos) const
{
    return button2.getGlobalBounds().contains(uiPos);
}

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
) {
    if (!dbStartupEnabled) {
        if (option == 1 && texMaleOnline.getSize().x > 0 && texMaleOnline.getSize().y > 0) {
            sprite.setTexture(texMaleOnline);
            sf::Vector2u ts = texMaleOnline.getSize();
            sprite.setOrigin({ ts.x / 2.f, ts.y / 2.f });
            sprite.setScale({ 500.f / ts.x, 500.f / ts.y });
        }
        else if (texFemaleOnline.getSize().x > 0 && texFemaleOnline.getSize().y > 0) {
            sprite.setTexture(texFemaleOnline);
            sf::Vector2u ts = texFemaleOnline.getSize();
            sprite.setOrigin({ ts.x / 2.f, ts.y / 2.f });
            sprite.setScale({ 500.f / ts.x, 500.f / ts.y });
        }

        hunger = stats.hunger;
        thirst = stats.thirst;
        currency = stats.currency;
        level = stats.level;
        exp = stats.exp;
        timeOffset = stats.elapsedTime;
        elapsedGameTime = timeOffset;
        hungerMultiplier = stats.hungerMultiplier;
        thirstMultiplier = stats.thirstMultiplier;
        hungerMultiplierText.setString("X" + std::to_string(hungerMultiplier));
        thirstMultiplierText.setString("X" + std::to_string(thirstMultiplier));

        return true;
    }

    if (dbStartupEnabled) {
        bool hydrateSuccess = hydrateWorldAndPlayerData(
            window,
            width,
            height,
            option,
            username,
            dbStartupEnabled,
            authManager,
            font,
            sprite,
            animWalk,
            animWalkWithSword,
            animWalkWithShield,
            animWalkWithShieldAndSword,
            animAttackWithSword,
            animAttackWithShieldAndSword,
            framesPerDir,
            stats,
            hunger,
            thirst,
            currency,
            level,
            exp,
            timeOffset,
            elapsedGameTime,
            hungerMultiplier,
            thirstMultiplier,
            hungerMultiplierText,
            thirstMultiplierText,
            structures,
            campfires,
            inventory,
            assets,
            snapToGrid,
            npcFemale,
            npcMale,
            mapWidth,
            mapHeight
        );
        return hydrateSuccess;
    }

    return true;
}


