#include "GameAssets.h"
#include "Animation.h"
#include "ItemID.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "SheepMeat.h"
#include "ZombieFlesh.h"
#include "Chicken.h"
#include "Pig.h"
#include "Sheep.h"
#include "Zombie.h"
#include <iostream>
#include <sstream>
#include <iomanip>

const sf::Texture* GameAssets::getItemTexture(int id) const {
    if (id == ITEM_WOOD) return &woodTex;
    if (id == ITEM_STONE) return &stoneTex;
    if (id == ITEM_SAPLING) return &saplingTex;
    if (id == ITEM_CHICKEN_MEAT) return ChickenMeat::getTexture();
    if (id == ITEM_PIG_MEAT) return PigMeat::getTexture();
    if (id == ITEM_SHEEP_MEAT) return SheepMeat::getTexture();
    if (id == ITEM_COOKED_SHEEP) return &cookedSheepTex;
    if (id == ITEM_COOKED_CHICKEN) return &cookedChickenTex;
    if (id == ITEM_COOKED_PIG) return &cookedPigTex;
    if (id == ITEM_WOOL) return &woolTex;
    if (id == ITEM_ZOMBIE_FLESH) return ZombieFlesh::getTexture();
    if (id == ITEM_WOODEN_SWORD) return &woodenSwordTex;
    if (id == ITEM_SHIELD) return &shieldTex;
    if (id == ITEM_FURNACE) return &furnaceTex;
    if (id == ITEM_BED) return &bedTex;
    if (id == ITEM_WOOD_TILE) return &woodTileTex;
    if (id == ITEM_WOOD_WALL) return &woodWallTex;
    if (id == ITEM_WOOD_DOOR) return &woodDoorTex;
    if (id == ITEM_WOOD_ROOF) return &woodRoofTex;
    if (id == ITEM_WOOD_STAIRS) return &woodStairsTex;
    if (id == ITEM_CHEST) return &chestTex;
    if (id == ITEM_COAL) return &coalTex;
    if (id == ITEM_COPPER_ORE) return &copperOreTex;
    if (id == ITEM_COPPER_BAR) return &copperBarTex;
    if (id == ITEM_IRON_ORE) return &ironOreTex;
    if (id == ITEM_IRON_BAR) return &ironBarTex;
    if (id == ITEM_GOLD_ORE) return &goldOreTex;
    if (id == ITEM_GOLD_BAR) return &goldBarTex;
    if (id == ITEM_DIAMOND) return &diamondTex;
    if (id == ITEM_WRENCH) return &wrenchTex;
    return nullptr;
}

bool loadGameAssets(GameAssets& assets, unsigned int mapWidth, unsigned int mapHeight) {
    bool fontLoaded = assets.font.openFromFile("C:/Windows/Fonts/arial.ttf");
    if (!fontLoaded) fontLoaded = assets.font.openFromFile("C:/Windows/Fonts/calibri.ttf");
    if (!fontLoaded) fontLoaded = assets.font.openFromFile("C:/Windows/Fonts/segoeui.ttf");
    if (!fontLoaded) fontLoaded = assets.font.openFromFile("C:/Windows/Fonts/tahoma.ttf");

    loadTextureSafe(assets.selBgTexture, "MainMenu/Background.png");
    assets.selBgSprite.emplace(assets.selBgTexture);
    if (assets.selBgTexture.getSize().x > 0 && assets.selBgTexture.getSize().y > 0) {
        assets.selBgSprite->setScale({
            1920.f / assets.selBgTexture.getSize().x,
            1080.f / assets.selBgTexture.getSize().y
        });
    }

    loadTextureSafe(assets.bgTexture, "Items/Background.jpg");
    assets.background.emplace(assets.bgTexture);
    if (assets.bgTexture.getSize().x > 0 && assets.bgTexture.getSize().y > 0) {
        float scaleX = float(mapWidth) / assets.bgTexture.getSize().x;
        float scaleY = float(mapHeight) / assets.bgTexture.getSize().y;
        assets.background->setScale({ scaleX, scaleY });
    }
    assets.background->setPosition({ 0.f, 0.f });

    loadTextureSafe(assets.texMaleOnline, "Sprites/Male/Walk/South/frame_000.png");
    loadTextureSafe(assets.texFemaleOnline, "Sprites/Female/Walk/South/frame_000.png");

    loadTextureSafe(assets.hpIconTex, "Sprites/UI/hp.png");
    loadTextureSafe(assets.hungerIconTex, "Sprites/UI/hunger.png");
    loadTextureSafe(assets.thirstIconTex, "Sprites/UI/thirst.png");
    loadTextureSafe(assets.wrenchTex, "Sprites/UI/wrench.png");

    for (int i = 0; i < 8; i++) {
        loadTextureSafe(assets.campfireFrames[i], "Items/campfire/frame_" + std::to_string(i) + "_campfire.png");
    }

    ChickenMeat::loadTexture();
    PigMeat::loadTexture();
    ZombieFlesh::loadTexture();
    SheepMeat::loadTexture();
    CookedSheepMeat::loadTexture();
    loadTextureSafe(Chicken::texture, "Sprites/Animals/Chicken/South.png");
    loadTextureSafe(Pig::texture, "Sprites/Animals/Pig/South.png");
    loadTextureSafe(Sheep::texture, "Sprites/Animals/Sheep/Walk/South/frame_000.png");
    loadTextureSafe(Zombie::texture, "Sprites/Monsters/Zombie/South.png");

    loadTextureSafe(assets.woodTex, "Items/wood.png");
    loadTextureSafe(assets.treeTex, "Items/Tree.png");
    loadTextureSafe(assets.stoneTex, "Items/Stone.png");
    loadTextureSafe(assets.pondTex, "Items/pond.png");
    loadTextureSafe(assets.saplingTex, "Items/sapling.png");
    loadTextureSafe(assets.goldTex, "Items/gold.png");
    loadTextureSafe(assets.woodenSwordTex, "Items/wooden sword.png");
    loadTextureSafe(assets.shieldTex, "Items/shield.png");
    loadTextureSafe(assets.furnaceTex, "Items/furnance.png");
    loadTextureSafe(assets.woodTileTex, "Items/wood tile.png");
    loadTextureSafe(assets.woodWallTex, "Items/wood wall.png");
    loadTextureSafe(assets.woodDoorTex, "Items/wooden door.png");
    loadTextureSafe(assets.woodRoofTex, "Items/wooden roof.png");
    loadTextureSafe(assets.woodStairsTex, "Items/wooden stairs.png");
    loadTextureSafe(assets.bedTex, "Items/bed.png");
    loadTextureSafe(assets.chestTex, "Items/chest.png");
    loadTextureSafe(assets.cookedChickenTex, "Items/cooked chicken meat.png");
    loadTextureSafe(assets.cookedPigTex, "Items/cooked pig meat.png");
    loadTextureSafe(assets.cookedSheepTex, "Items/cooked sheep meat.png");
    loadTextureSafe(assets.woolTex, "Items/wool.png");
    loadTextureSafe(assets.wrenchItemTex, "Sprites/UI/wrench.png");

    loadTextureSafe(assets.swordIconTex, "Items/wooden_sword_shield_icons.png");
    loadTextureSafe(assets.shieldIconTex, "Items/wooden_sword_shield_icons.png");
    loadTextureSafe(assets.tileIconTex, "Items/building_parts_icons.png");
    loadTextureSafe(assets.wallIconTex, "Items/building_parts_icons.png");
    loadTextureSafe(assets.doorIconTex, "Items/building_parts_icons.png");
    loadTextureSafe(assets.roofIconTex, "Items/building_parts_icons.png");
    loadTextureSafe(assets.stairsIconTex, "Items/building_parts_icons.png");
    loadTextureSafe(assets.bedIconTex, "Items/bed.png");

    loadTextureSafe(assets.coalTex, "Items/coal.png");
    loadTextureSafe(assets.copperOreTex, "Items/copper ore.png");
    loadTextureSafe(assets.copperBarTex, "Items/copper bar.png");
    loadTextureSafe(assets.ironOreTex, "Items/iron ore.png");
    loadTextureSafe(assets.ironBarTex, "Items/iron bar.png");
    loadTextureSafe(assets.goldOreTex, "Items/gold ore.png");
    loadTextureSafe(assets.goldBarTex, "Items/gold bar.png");
    loadTextureSafe(assets.diamondTex, "Items/diamond.png");

    assets.goldTexLoaded = (assets.goldTex.getSize().x > 0);
    assets.goldSprite.emplace(assets.goldTex);
    assets.goldSprite->setScale({ 0.5f, 0.5f });

    // Story frames
    for (int i = 0; i < 10; i++) {
        std::ostringstream ss;
        ss << "Story/frame_" << std::setfill('0') << std::setw(3) << i << ".jpg";
        std::string path = resolvePath(ss.str());

        if (fileExists(path)) {
            sf::Texture tex;
            if (loadTextureSafe(tex, ss.str())) {
                assets.introFrames.push_back(tex);
            }
            else {
                break;
            }
        }
        else {
            break;
        }
    }

    assets.introTexts = {
        "The world as we knew it ended in a flash of terror.",
        "A mysterious plague swept across the continents, leaving only chaos.",
        "Cities that once buzzed with life became silent graveyards.",
        "The survivors were forced to flee into the wild, seeking refuge.",
        "But the wilderness is no longer safe; the infected are everywhere.",
        "Hunger and thirst will be your constant companions in this new world.",
        "You must scavenge for food and clean water to stay alive.",
        "Trees provide wood for tools and fire; stones are essential for building.",
        "Crafting is the key to your survival. Without tools, you are defenseless.",
        "Nightfall brings the most danger. Zombies emerge from the shadows.",
        "Build a campfire to keep the darkness at bay and stay warm.",
        "But remember, you are not alone. Others may be out there.",
        "Some might help you, while others will fight for the same resources.",
        "Every choice you make could be your last. Stay alert.",
        "This is a world of RAS - Ruins, Ashes, and Survival.",
        "Your story begins here. Can you endure the coming storms?"
    };

    assets.tutorialSteps = {
        {"WELCOME SURVIVOR", "You have arrived in a dangerous world. Let's learn the basics.", "Click 'Next' to continue"},
        {"MOVEMENT", "Use [W, A, S, D] to move around the map.", "Walk a short distance"},
        {"SPRINTING", "Hold [Left Shift] while moving to sprint.", "Try sprinting now"},
        {"INVENTORY", "Press [I] to open your backpack. You can equip tools here.", "Open your inventory"},
        {"GATHERING", "Go near a tree and press [Space] to chop wood.\nGo near a stone and press [Space] to mine stone.", "Gather 1 Wood and 1 Stone"},
        {"COMBAT", "Animals and Monsters are everywhere. Use [Space] to attack.", "Defend yourself!"},
        {"COMMUNICATION", "Press [T] to open the global chat and talk to others.", "Say 'Hello' in chat"},
        {"ALL SET!", "You are ready to survive. Good luck!", "Click 'Finish' to start playing"}
    };

    assets.availableQuests = {
        {1, "Lumberjack", "Chop down 10 trees.", 10},
        {2, "Stone Collector", "Collect 8 stones.", 8},
        {3, "Keep Warm", "Craft 1 campfire.", 1},
        {4, "Hunter", "Collect 5 meat items.", 5}
    };

    assets.craftableItems = {
        {ITEM_NONE, "Campfire", 10, 0, 0, &assets.campfireFrames[0]},
        {ITEM_WOODEN_SWORD, "Wooden Sword", 8, 0, 0, &assets.woodenSwordTex},
        {ITEM_SHIELD, "Sturdy Shield", 10, 5, 0, &assets.shieldTex},
        {ITEM_FURNACE, "Stone Furnace", 0, 10, 0, &assets.furnaceTex},
        {ITEM_WOOD_TILE, "Wood Tile", 1, 0, 0, &assets.woodTileTex},
        {ITEM_WOOD_WALL, "Wood Wall", 1, 0, 0, &assets.woodWallTex},
        {ITEM_WOOD_DOOR, "Wood Door", 5, 0, 0, &assets.woodDoorTex},
        {ITEM_WOOD_ROOF, "Wood Roof", 5, 0, 0, &assets.woodRoofTex},
        {ITEM_WOOD_STAIRS, "Wood Stairs", 8, 0, 0, &assets.woodStairsTex},
        {ITEM_BED, "Comfortable Bed", 10, 0, 5, &assets.bedTex},
        {ITEM_CHEST, "Chest", 15, 0, 0, &assets.chestTex}
    };

    return true;
}
