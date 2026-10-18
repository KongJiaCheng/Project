#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <string>
#include <optional>
#include "Animation.h"
#include "Crafting.h"
#include "HUD.h"

struct DailyQuest {
    int id;
    std::string title;
    std::string description;
    int goal;
};

struct GameAssets {
    sf::Font font;
    sf::Texture selBgTexture;
    std::optional<sf::Sprite> selBgSprite;
    sf::Texture bgTexture;
    std::optional<sf::Sprite> background;
    
    // UI Icons
    sf::Texture hpIconTex, hungerIconTex, thirstIconTex, wrenchTex;
    sf::Texture goldTex;
    std::optional<sf::Sprite> goldSprite;
    bool goldTexLoaded = false;
    
    // World Textures
    sf::Texture woodTex, treeTex, stoneTex, pondTex, saplingTex;
    sf::Texture woodenSwordTex, shieldTex, wrenchItemTex;
    sf::Texture furnaceTex, woodTileTex, woodWallTex, woodDoorTex, woodRoofTex, woodStairsTex, bedTex, chestTex;
    sf::Texture swordIconTex, shieldIconTex, tileIconTex, wallIconTex, doorIconTex, roofIconTex, stairsIconTex, bedIconTex;
    sf::Texture cookedChickenTex, cookedPigTex, cookedZombieTex, cookedSheepTex, woolTex;
    sf::Texture coalTex, copperOreTex, copperBarTex, ironOreTex, ironBarTex, goldOreTex, goldBarTex, diamondTex;
    sf::Texture texMaleOnline, texFemaleOnline;
    sf::Texture campfireFrames[8];

    // Animations
    AnimSet animWalk;
    AnimSet animWalkWithSword;
    AnimSet animWalkWithShield;
    AnimSet animWalkWithShieldAndSword;
    AnimSet animAttackWithSword;
    AnimSet animAttackWithShieldAndSword;

    std::vector<sf::Texture> introFrames;
    std::vector<std::string> introTexts;
    std::vector<TutorialStep> tutorialSteps;
    std::vector<DailyQuest> availableQuests;
    std::vector<CraftableItem> craftableItems;

    const sf::Texture* getItemTexture(int id) const;
};

bool loadGameAssets(GameAssets& assets, unsigned int mapWidth, unsigned int mapHeight);
