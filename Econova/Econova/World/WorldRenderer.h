#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <memory>
#include "Inventory.h"
#include "StructureSystem.h"

void drawFloorGrid(
    sf::RenderWindow& window,
    unsigned int mapWidth,
    unsigned int mapHeight
);

void drawWorldCampfires(
    sf::RenderWindow& window,
    const sf::Font& font,
    const std::vector<PlacedCampfire>& campfires,
    const sf::Texture campfireFrames[8],
    const sf::Texture& wrenchTex,
    const sf::Sprite& playerSprite,
    Inventory& inventory
);

void drawWall(
    sf::RenderWindow& window,
    const sf::Texture& tex,
    sf::Vector2f pos,
    float rotation,
    sf::Color color = sf::Color::White
);

void drawDoor(
    sf::RenderWindow& window,
    const sf::Texture& tex,
    sf::Vector2f pos,
    float rotation,
    bool isOpen,
    sf::Color color = sf::Color::White
);

void drawPlacementPreview(
    sf::RenderWindow& window,
    int placementItemID,
    float placementRotation,
    sf::Vector2f placementPos,
    const sf::Texture campfireFrames[8],
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Texture& chestTex
);

void drawSpeechBubble(
    sf::RenderWindow& window,
    const sf::Font& font,
    const sf::Vector2f& playerPos,
    float bubbleTimer,
    const std::string& latestPlayerMsg
);

#include "Animal.h"
#include "Monster.h"
#include "PlayerController.h"

void renderWorldDepthScene(
    sf::RenderWindow& window,
    sf::Sprite& sprite,
    std::vector<PlacedStructure>& structures,
    const std::vector<TileFloor>& elevatedTileFloors,
    int& playerFloorLevel,
    bool saplingPlacementMode,
    bool saplingFollowingCursor,
    sf::Vector2f& saplingPlacementPos,
    const sf::Texture& saplingTex,
    const std::vector<PlantedSapling>& plantedSaplings,
    const sf::Font& font,
    bool isPlayerHurt,
    float& playerHurtTimer,
    float dt,
    const std::vector<std::unique_ptr<Animal>>& animals,
    const std::vector<std::unique_ptr<Monster>>& monsters,
    bool animLoadedForSelection,
    const NPC& npcFemale,
    const NPC& npcMale,
    const std::string& username,
    const sf::Sprite& playerWrenchBtn,
    float bubbleTimer,
    const std::string& latestPlayerMsg,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc
);

void drawWorldInteractions(
    sf::RenderWindow& window,
    const sf::Font& font,
    const sf::Sprite& sprite,
    int playerFloorLevel,
    bool isSleeping,
    bool chestMenuOpen,
    std::vector<PlacedStructure>& structures,
    bool nearPond,
    sf::Text& drinkText,
    const sf::Texture& wrenchTex,
    Inventory& inventory
);

#include <SFML/Audio.hpp>
#include <random>

void updateDayNightCycleAndMusic(
    sf::RenderWindow& window,
    long long serverEpochCache,
    float elapsedGameTime,
    float totalCycleTime,
    sf::RectangleShape& darkness,
    bool& isNight,
    bool& wasNight,
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Music& bgMusic,
    sf::Music& nightMusic,
    bool& nightMusicLoaded,
    float volume,
    bool tutorialActive,
    std::mt19937& gen,
    std::uniform_real_distribution<float>& randX,
    std::uniform_real_distribution<float>& randY
);

