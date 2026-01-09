#include "Game.h"
#include "ViewUtils.h"
#include "Camera.h"
#include "Inventory.h"
#include "ItemManager.h"
#include "Animation.h"
#include "Animal.h"
#include "Monster.h"
#include "Chicken.h"
#include "Pig.h"
#include "Zombie.h"
#include "ChickenMeat.h"
#include "PigMeat.h"
#include "ZombieFlesh.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <cmath>
#include <random>

const int MAP_WIDTH = 5000;
const int MAP_HEIGHT = 5000;

bool isInWater(const sf::Vector2f& pos, const sf::FloatRect& water)
{
    return water.contains(pos);
}

void gameLoop(sf::RenderWindow& window, unsigned int width, unsigned int height)
{
    int option = 0;
    bool selectionDone = false;
    bool inventoryOpen = false;
    bool invKeyHeld = false;
    bool itemSpawned = false;
    bool musicLoaded = false;
    bool musicStarted = false;
    bool nearPond = false;
    bool isDead = false;
    bool wasNight = false;

    sf::Music nightMusic;
    bool nightMusicLoaded = false;
    bool nightMusicPlaying = false;

    std::vector<std::unique_ptr<Animal>> animals;
    std::vector<std::unique_ptr<Monster>> monsters;

    sf::RectangleShape darkness;
    darkness.setSize(sf::Vector2f((float)MAP_WIDTH, (float)MAP_HEIGHT));
    darkness.setFillColor(sf::Color(0, 0, 0, 0));

    Inventory inventory;
    ItemManager itemManager;
    Animation animation;

    const int FRAMES_PER_DIR = 6;
    Animation animSouth, animNorth, animEast, animWest;
    bool animLoadedForSelection = false;

    sf::Font font;
    font.openFromFile("C:/Windows/Fonts/arial.ttf");

    sf::Text Option(font, "Choose Your Character", 80);
    Option.setFillColor(sf::Color::Black);
    Option.setPosition({ (width - 600.f) / 2.f, 50.f });

    sf::RectangleShape button1({ 600.f, 200.f });
    button1.setFillColor(sf::Color::Green);
    button1.setPosition({ (width - 600.f) / 2.f, (height - 200.f) / 2.f });

    sf::Text buttonText1(font, "Male", 120);
    buttonText1.setFillColor(sf::Color::White);
    buttonText1.setPosition(button1.getPosition());

    sf::RectangleShape button2({ 600.f, 200.f });
    button2.setFillColor(sf::Color::Green);
    button2.setPosition({ (width - 600.f) / 2.f, ((height - 200.f) / 2.f) + 400.f });

    sf::Text buttonText2(font, "Female", 120);
    buttonText2.setFillColor(sf::Color::White);
    buttonText2.setPosition(button2.getPosition());

    sf::Texture bgTexture;
    bgTexture.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/Background.jpg");

    sf::Sprite background(bgTexture);
    float scaleX = float(MAP_WIDTH) / bgTexture.getSize().x;
    float scaleY = float(MAP_HEIGHT) / bgTexture.getSize().y;
    background.setScale({ scaleX, scaleY });
    background.setPosition({ 0.f, 0.f });

    const float BAR_WIDTH = 400.f;
    const float BAR_HEIGHT = 22.f;

    float maxHP = 100.f;
    float hp = 100.f;

    float maxHunger = 100.f;
    float hunger = 0.f;

    float maxThirst = 100.f;
    float thirst = 0.f;

    // rates (per second)
    const float HUNGER_RATE = 1.f;
    const float THIRST_RATE = 1.f;
    const float DAMAGE_RATE = 10.f;

    float zombieAttackTimer = 0.f;
    const float ZOMBIE_ATTACK_COOLDOWN = 0.8f;
    const int ZOMBIE_DAMAGE = 10;

    sf::Texture texSouth;
    sf::Sprite sprite(texSouth);
    sprite.setOrigin({ 32.f, 32.f });
    sprite.setPosition({ width / 2.f, height / 2.f });

    sf::Texture woodTex;
    sf::Texture treeTex;
    sf::Texture stoneTex;
    sf::Texture pondTex;

    sf::Texture hpIconTex, hungerIconTex, thirstIconTex;
    hpIconTex.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\UI\\hp.png");
    hungerIconTex.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\UI\\hunger.png");
    thirstIconTex.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\UI\\thirst.png");

    sf::Sprite hpIcon(hpIconTex);
    sf::Sprite hungerIcon(hungerIconTex);
    sf::Sprite thirstIcon(thirstIconTex);

    hpIcon.setScale({ 0.6f, 0.6f });
    hungerIcon.setScale({ 0.6f, 0.6f });
    thirstIcon.setScale({ 0.6f, 0.6f });

    sf::Font uiFont;
    uiFont.openFromFile("C:/Windows/Fonts/arial.ttf");

    sf::Text hpText(uiFont, "", 16);
    sf::Text hungerText(uiFont, "", 16);
    sf::Text thirstText(uiFont, "", 16);

    sf::Text drinkText(font, "Press E to drink water", 28);
    drinkText.setFillColor(sf::Color::Yellow);

    hpText.setFillColor(sf::Color::Black);
    hungerText.setFillColor(sf::Color::Black);
    thirstText.setFillColor(sf::Color::Black);

    sf::RectangleShape hpBack({ BAR_WIDTH, BAR_HEIGHT });
    hpBack.setFillColor(sf::Color(60, 60, 60));

    sf::RectangleShape hpBar({ BAR_WIDTH, BAR_HEIGHT });
    hpBar.setFillColor(sf::Color::Red);

    sf::RectangleShape hungerBack = hpBack;
    sf::RectangleShape hungerBar({ BAR_WIDTH, BAR_HEIGHT });
    hungerBar.setFillColor(sf::Color(255, 165, 0));

    sf::RectangleShape thirstBack = hpBack;
    sf::RectangleShape thirstBar({ BAR_WIDTH, BAR_HEIGHT });
    thirstBar.setFillColor(sf::Color(0, 150, 255));

    hpBar.setSize({ BAR_WIDTH * (hp / maxHP), BAR_HEIGHT });
    hpBar.setPosition(hpBack.getPosition());
    hungerBar.setSize({ BAR_WIDTH * (1.f - hunger / maxHunger), BAR_HEIGHT });
    hungerBar.setPosition(hungerBack.getPosition());
    thirstBar.setSize({ BAR_WIDTH * (1.f - thirst / maxThirst), BAR_HEIGHT });
    thirstBar.setPosition(thirstBack.getPosition());

    ChickenMeat::loadTexture();
    PigMeat::loadTexture();
    ZombieFlesh::loadTexture();
    Chicken::texture.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Animals\\Chicken\\South.png");
    Pig::texture.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Animals\\Pig\\South.png");
    Zombie::texture.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Monsters\\Zombie\\South.png");

    woodTex.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/wood.png");
    treeTex.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/Tree.png");
    stoneTex.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/Stone.png");
    pondTex.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/Pond.png");

    sf::Sprite pond(pondTex);
    pond.setScale({ 10.0f, 10.0f });   // ⬅️ make it 10× bigger

    itemManager.setResourceTextures(&woodTex, &stoneTex);

    sf::Clock animalRespawnClock;
    const int MAX_ANIMALS = 20;

    sf::Clock deathClock;

    const float totalCycleTime = 900.f;

    sf::Clock deltaClock;
    sf::Clock dayNightClock;

    UpdateViewToWindow(&window);

    enum class Dir { South, North, East, West };
    Dir currentDir = Dir::South;

    const float MOVE_SPEED = 250.f;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> randX(0.f, (float)MAP_WIDTH);
    std::uniform_real_distribution<float> randY(0.f, (float)MAP_HEIGHT);

    if (animalRespawnClock.getElapsedTime().asSeconds() > 10.f &&
        animals.size() < MAX_ANIMALS)
    {
        animals.push_back(std::make_unique<Chicken>(
            sf::Vector2f(randX(gen), randY(gen))
        ));

        animalRespawnClock.restart();
    }

    sf::Vector2u texSize = pondTex.getSize();
    sf::Vector2f pondWorldSize(
        texSize.x * pond.getScale().x,
        texSize.y * pond.getScale().y
    );

    std::uniform_real_distribution<float> pondX(
        0.f, MAP_WIDTH - pondWorldSize.x
    );
    std::uniform_real_distribution<float> pondY(
        0.f, MAP_HEIGHT - pondWorldSize.y
    );

    pond.setPosition({ pondX(gen), pondY(gen) });

    sf::FloatRect pondBounds = pond.getGlobalBounds();


    sf::Music bgMusic;
    if (bgMusic.openFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Music\\Locally Sourced - Jason Farnham.mp3"))
    {
        bgMusic.setLooping(true);
        bgMusic.setVolume(50.f); // 0–100
        musicLoaded = true;
    }
    else
    {
        std::cerr << "Failed to load background music\n";
    }

    while (window.isOpen())
    {
        float dt = deltaClock.restart().asSeconds();

        // survival update
        hunger = std::min(maxHunger, hunger + HUNGER_RATE * dt);
        thirst = std::min(maxThirst, thirst + THIRST_RATE * dt);

        if (hunger >= maxHunger || thirst >= maxThirst)
        {
            hp = std::max(0.f, hp - DAMAGE_RATE * dt);
        }

        if (hp <= 0.f && !isDead)
        {
            isDead = true;
            deathClock.restart();
        }

        if (inventoryOpen)
        {
            // === Inventory layout (must match Inventory.cpp) ===
            const float SLOT_W = 64.f;   // same as boxW
            const float SLOT_GAP = 4.f;
            const int SLOT_COUNT = 8;

            float invX = inventory.computeX(window);
            float invY = inventory.computeY(window);
            float invWidth = SLOT_COUNT * (SLOT_W + SLOT_GAP);

            float barWidth = invWidth;
            float barHeight = 22.f;
            float spacing = 80.f;

            float startX = invX;
            float startY = invY - (barHeight * 3.f) - (spacing * 2.f);

            float iconPadding = 6.f;
            float textPadding = 10.f;

            auto hpBounds = hpIcon.getGlobalBounds();
            auto hungerBounds = hungerIcon.getGlobalBounds();
            auto thirstBounds = thirstIcon.getGlobalBounds();

            hpBack.setSize({ barWidth, barHeight });
            hungerBack.setSize({ barWidth, barHeight });
            thirstBack.setSize({ barWidth, barHeight });

            hpBack.setPosition({ startX, startY });
            hungerBack.setPosition({ startX, startY + barHeight + spacing });
            thirstBack.setPosition({ startX, startY + (barHeight + spacing) * 2.f });

            hpBar.setSize({ barWidth * (hp / maxHP), barHeight });
            hungerBar.setSize({ barWidth * (1.f - hunger / maxHunger), barHeight });
            thirstBar.setSize({ barWidth * (1.f - thirst / maxThirst), barHeight });

            hpBar.setPosition(hpBack.getPosition());
            hungerBar.setPosition(hungerBack.getPosition());
            thirstBar.setPosition(thirstBack.getPosition());

            hpIcon.setPosition({
                hpBack.getPosition().x - hpBounds.size.x - iconPadding,
                hpBack.getPosition().y + (barHeight - hpBounds.size.y) / 2.f
                });

            hungerIcon.setPosition({
                hungerBack.getPosition().x - hungerBounds.size.x - iconPadding,
                hungerBack.getPosition().y + (barHeight - hungerBounds.size.y) / 2.f
                });

            thirstIcon.setPosition({
                thirstBack.getPosition().x - thirstBounds.size.x - iconPadding,
                thirstBack.getPosition().y + (barHeight - thirstBounds.size.y) / 2.f
                });

            // TEXT CONTENT
            hpText.setString("HP " + std::to_string((int)hp) + " / " + std::to_string((int)maxHP));
            hungerText.setString("Hunger " + std::to_string((int)(maxHunger - hunger)));
            thirstText.setString("Thirst " + std::to_string((int)(maxThirst - thirst)));

            // TEXT POSITION (inside bar)
            hpText.setPosition({
                hpBack.getPosition().x + textPadding,
                hpBack.getPosition().y + 2.f
                });

            hungerText.setPosition({
                hungerBack.getPosition().x + textPadding,
                hungerBack.getPosition().y + 2.f
                });

            thirstText.setPosition({
                thirstBack.getPosition().x + textPadding,
                thirstBack.getPosition().y + 2.f
                });
        }

        while (std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>()) window.close();

            if (auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape)
                    return;

                if (key->scancode == sf::Keyboard::Scancode::I && !invKeyHeld)
                {
                    inventoryOpen = !inventoryOpen;
                    invKeyHeld = true;
                }

                if (key->scancode == sf::Keyboard::Scancode::Space)
                {
                    for (auto& a : animals)
                    {
                        float dx = a->sprite.getPosition().x - sprite.getPosition().x;
                        float dy = a->sprite.getPosition().y - sprite.getPosition().y;
                        float dist = std::sqrt(dx * dx + dy * dy);

                        if (dist < 60.f && a->health > 0)
                        {
                            a->health -= 25.f;
                            a->isHurt = true;
                            a->hurtTimer = 0.15f;

                            // 🔥 KNOCKBACK GOES HERE
                            sf::Vector2f dir = a->sprite.getPosition() - sprite.getPosition();
                            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                            if (len != 0.f) dir /= len;
                            a->sprite.move(dir * 40.f);
                        }
                    }

                    for (auto& a : monsters)
                    {
                        float dx = a->sprite.getPosition().x - sprite.getPosition().x;
                        float dy = a->sprite.getPosition().y - sprite.getPosition().y;
                        float dist = std::sqrt(dx * dx + dy * dy);

                        if (dist < 60.f && a->health > 0)
                        {
                            a->health -= 25.f;
                            a->isHurt = true;
                            a->hurtTimer = 0.15f;

                            // 🔥 KNOCKBACK GOES HERE
                            sf::Vector2f dir = a->sprite.getPosition() - sprite.getPosition();
                            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                            if (len != 0.f) dir /= len;
                            a->sprite.move(dir * 40.f);
                        }
                    }
                }
            }

            if (auto* key = event->getIf<sf::Event::KeyReleased>())
            {
                if (key->scancode == sf::Keyboard::Scancode::I)
                    invKeyHeld = false;
            }

            if (auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouse->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f mpos((float)mouse->position.x, (float)mouse->position.y);

                    if (!selectionDone)
                    {
                        if (button1.getGlobalBounds().contains(mpos))
                        {
                            option = 1;
                            selectionDone = true;
                        }
                        else if (button2.getGlobalBounds().contains(mpos))
                        {
                            option = 2;
                            selectionDone = true;
                        }
                    }
                    else if (inventoryOpen)
                    {
                        inventory.handleClick(mouse->position, window);
                        int slot = inventory.getSelectedSlot();
                        if (slot != -1 && inventory.hasItem(slot))
                        {
                            const sf::Texture* tex = inventory.getItemTexture(slot);

                            if (tex == ChickenMeat::getTexture())
                            {
                                hunger = std::max(0.f, hunger - 30.f);
                                inventory.consumeItem(slot);
                            }
                            else if (tex == PigMeat::getTexture())
                            {
                                hunger = std::max(0.f, hunger - 45.f);
                                inventory.consumeItem(slot);
                            }
                            else if (tex == ZombieFlesh::getTexture())
                            {
                                hunger = std::max(0.f, hunger - 20.f);
                                hp = std::max(0.f, hp - 10.f); // eating zombie flesh hurts you
                                inventory.consumeItem(slot);
                            }
                        }
                    }
                }
            }
        }

        if (selectionDone && !animLoadedForSelection)
        {
            if (option == 1)
            {
                animSouth.loadFromFolder("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Male\\Walk\\South", FRAMES_PER_DIR);
                animNorth.loadFromFolder("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Male\\Walk\\North", FRAMES_PER_DIR);
                animEast.loadFromFolder("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Male\\Walk\\East", FRAMES_PER_DIR);
                animWest.loadFromFolder("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Male\\Walk\\West", FRAMES_PER_DIR);
            }
            else
            {
                animSouth.loadFromFolder("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Female\\Walk\\South", FRAMES_PER_DIR);
                animNorth.loadFromFolder("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Female\\Walk\\North", FRAMES_PER_DIR);
                animEast.loadFromFolder("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Female\\Walk\\East", FRAMES_PER_DIR);
                animWest.loadFromFolder("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Sprites\\Female\\Walk\\West", FRAMES_PER_DIR);
            }

            sprite.setTexture(animSouth.getCurrentTexture());
            inventory.setItem(0, animSouth.getCurrentTexture(), 5);
            animLoadedForSelection = true;
        }

        if (selectionDone && musicLoaded && !musicStarted)
        {
            bgMusic.play();
            musicStarted = true;
        }

        if (!selectionDone)
        {
            window.clear(sf::Color::White);
            window.setView(window.getDefaultView());
            window.draw(Option);
            window.draw(button1);
            window.draw(buttonText1);
            window.draw(button2);
            window.draw(buttonText2);

            window.display();
            continue;
        }

        if (!isDead)
        {
            for (auto& m : monsters)
            {
                if (m->health > 0)
                    m->update(dt, sprite.getPosition());

                sf::FloatRect zombieBounds = m->sprite.getGlobalBounds();
                sf::FloatRect playerBounds = sprite.getGlobalBounds();

                sf::FloatRect z = m->sprite.getGlobalBounds();
                sf::FloatRect p = sprite.getGlobalBounds();

                bool hit =
                    z.position.x < p.position.x + p.size.x &&
                    z.position.x + z.size.x > p.position.x &&
                    z.position.y < p.position.y + p.size.y &&
                    z.position.y + z.size.y > p.position.y;
                if (hit)
                {
                    zombieAttackTimer += dt;

                    if (zombieAttackTimer >= ZOMBIE_ATTACK_COOLDOWN)
                    {
                        m->health -= ZOMBIE_DAMAGE;
                        zombieAttackTimer = 0.f;
                    }
                }

            }

            animSouth.playing = animNorth.playing = animEast.playing = animWest.playing = false;

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W))
            {
                sprite.move({ 0.f, -MOVE_SPEED * dt });
                currentDir = Dir::North;
                animNorth.playing = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S))
            {
                sprite.move({ 0.f, MOVE_SPEED * dt });
                currentDir = Dir::South;
                animSouth.playing = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A))
            {
                sprite.move({ -MOVE_SPEED * dt, 0.f });
                currentDir = Dir::West;
                animWest.playing = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D))
            {
                sprite.move({ MOVE_SPEED * dt, 0.f });
                currentDir = Dir::East;
                animEast.playing = true;
            }
            else
            {
                animSouth.stopAtFirst();
                animNorth.stopAtFirst();
                animEast.stopAtFirst();
                animWest.stopAtFirst();
            }

            animSouth.update(dt);
            animNorth.update(dt);
            animEast.update(dt);
            animWest.update(dt);

            if (currentDir == Dir::South) sprite.setTexture(animSouth.getCurrentTexture(), true);
            if (currentDir == Dir::North) sprite.setTexture(animNorth.getCurrentTexture(), true);
            if (currentDir == Dir::East)  sprite.setTexture(animEast.getCurrentTexture(), true);
            if (currentDir == Dir::West)  sprite.setTexture(animWest.getCurrentTexture(), true);

            UpdateCamera(window, sprite, MAP_WIDTH, MAP_HEIGHT);

            sf::Vector2f pondCenter(
                pondBounds.position.x + pondBounds.size.x * 0.5f,
                pondBounds.position.y + pondBounds.size.y * 0.5f
            );

            float dx = sprite.getPosition().x - pondCenter.x;
            float dy = sprite.getPosition().y - pondCenter.y;

            float distance = std::sqrt(dx * dx + dy * dy);

            nearPond = distance < 80.f; // ← smaller radius


            window.clear(sf::Color::White);
            window.draw(background);
            window.draw(pond);
            itemManager.draw(window);
            window.draw(sprite);

            if (nearPond)
            {
                drinkText.setPosition({
                    sprite.getPosition().x - 160.f,
                    sprite.getPosition().y - 100.f
                    });

                window.draw(drinkText);
            }

            for (auto& a : animals)
            {
                if (a->health > 0)
                    window.draw(a->sprite);
            }

            for (auto& a : animals)
            {
                if (a->health > 0)
                    a->update(dt, sprite.getPosition());

                // ---- CLAMP ANIMAL TO MAP (FULL SPRITE) ----
                sf::FloatRect bounds = a->sprite.getGlobalBounds();
                sf::Vector2f pos = a->sprite.getPosition();

                if (bounds.position.x < 0.f)
                    pos.x -= bounds.position.x;

                if (bounds.position.y < 0.f)
                    pos.y -= bounds.position.y;

                if (bounds.position.x + bounds.size.x > MAP_WIDTH)
                    pos.x -= (bounds.position.x + bounds.size.x - MAP_WIDTH);

                if (bounds.position.y + bounds.size.y > MAP_HEIGHT)
                    pos.y -= (bounds.position.y + bounds.size.y - MAP_HEIGHT);

                a->sprite.setPosition(pos);
            }

            for (auto& a : animals)
            {
                if (a->health <= 0 && !a->droppedLoot)
                {
                    itemManager.addItem(
                        a->getFoodItemID(),
                        *a->getFoodTexture(),
                        a->sprite.getPosition(),
                        a->getFoodDropCount()
                    );

                    a->droppedLoot = true;
                }
            }

            for (auto& m : monsters)
            {
                if (m->health > 0)
                    m->update(dt, sprite.getPosition());

                sf::FloatRect zombieBounds = m->sprite.getGlobalBounds();
                sf::FloatRect playerBounds = sprite.getGlobalBounds();

                sf::FloatRect z = m->sprite.getGlobalBounds();
                sf::FloatRect p = sprite.getGlobalBounds();

                bool hit =
                    z.position.x < p.position.x + p.size.x &&
                    z.position.x + z.size.x > p.position.x &&
                    z.position.y < p.position.y + p.size.y &&
                    z.position.y + z.size.y > p.position.y;

                if (hit)
                {
                    zombieAttackTimer += dt;

                    if (zombieAttackTimer >= ZOMBIE_ATTACK_COOLDOWN)
                    {
                        m->health -= ZOMBIE_DAMAGE;
                        zombieAttackTimer = 0.f;
                    }
                }

                sf::FloatRect bounds = m->sprite.getGlobalBounds();
                sf::Vector2f pos = m->sprite.getPosition();

                if (bounds.position.x < 0.f)
                    pos.x -= bounds.position.x;

                if (bounds.position.y < 0.f)
                    pos.y -= bounds.position.y;

                if (bounds.position.x + bounds.size.x > MAP_WIDTH)
                    pos.x -= (bounds.position.x + bounds.size.x - MAP_WIDTH);

                if (bounds.position.y + bounds.size.y > MAP_HEIGHT)
                    pos.y -= (bounds.position.y + bounds.size.y - MAP_HEIGHT);

                m->sprite.setPosition(pos);
            }

            for (auto& m : monsters)
            {
                if (m->health <= 0 && !m->droppedLoot)
                {
                    itemManager.addItem(
                        m->getFoodItemID(),
                        *m->getFoodTexture(),
                        m->sprite.getPosition(),
                        m->getFoodDropCount()
                    );

                    m->droppedLoot = true;
                }
            }

            for (auto& m : monsters)
            {
                if (m->health > 0)
                    window.draw(m->sprite);
            }



            float nearestDist = itemManager.getNearestDistance(sprite);
            int nearestID = itemManager.getNearestItemID(sprite);
            const float RADIUS = 250.f;

            if (nearestDist < RADIUS)
            {
                sf::Text prompt(font, "", 32);
                prompt.setFillColor(sf::Color::Yellow);

                if (nearestID == ITEM_TREE)
                    prompt.setString("Press E to chop tree");
                else prompt.setString("Press E to pick up");

                prompt.setPosition({ sprite.getPosition().x - 120, sprite.getPosition().y - 80 });

                window.draw(prompt);
            }

            static sf::Clock drinkClock;

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::E))
            {
                itemManager.tryPickup(sprite, inventory);

                if (nearPond && drinkClock.getElapsedTime().asSeconds() > 1.f)
                {
                    thirst = std::max(0.f, thirst - 40.f);
                    drinkClock.restart();
                }
            }
        }
        const float totalCycleTime = 900.f;
        float cycleValue = std::fmod(dayNightClock.getElapsedTime().asSeconds(), totalCycleTime);
        float t = cycleValue / totalCycleTime;
        float timeOfDay = (t < 0.5f) ? (t * 2.f) : (1.f - (t - 0.5f) * 2.f);
        int darknessLevel = static_cast<int>(timeOfDay * 200.f);
        darkness.setFillColor(sf::Color(0, 0, 0, darknessLevel));
        window.draw(darkness);

        bool isNight = (darknessLevel > 80);

        if (inventoryOpen)
        {
            window.setView(window.getDefaultView());

            window.draw(hpBack);
            window.draw(hpBar);
            window.draw(hpIcon);
            window.draw(hpText);

            window.draw(hungerBack);
            window.draw(hungerBar);
            window.draw(hungerIcon);
            window.draw(hungerText);

            window.draw(thirstBack);
            window.draw(thirstBar);
            window.draw(thirstIcon);
            window.draw(thirstText);

            inventory.draw(window);
        }


        // NIGHT MUSIC LOGIC (FIXED)
        if (isNight && !wasNight)
        {
            wasNight = true;

            monsters.clear();

            const int ZOMBIE_COUNT = 6;

            for (int i = 0; i < ZOMBIE_COUNT; ++i)
            {
                monsters.push_back(std::make_unique<Zombie>(
                    sf::Vector2f(randX(gen), randY(gen))
                ));
            }

            bgMusic.stop();                     // ✅ STOP DAY MUSIC

            if (!nightMusicLoaded)
            {
                if (nightMusic.openFromFile(
                    "C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Music/Shirfine_-_Illusionary_Daytime_(mp3.pm).mp3"
                ))
                {
                    nightMusic.setLooping(true);
                    nightMusic.setVolume(40.f);
                    nightMusic.play();          // ✅ START PLAYING
                    nightMusicLoaded = true;
                }
            }

            nightMusic.play();              // ✅ START PLAYING 
        }
        else if (!isNight && wasNight)
        {
            wasNight = false;

            nightMusic.stop();               // ✅ STOP NIGHT MUSIC
            bgMusic.play();                     // ✅ START DAY MUSIC
            monsters.clear();
        }


        if (isDead)
        {
            window.setView(window.getDefaultView());

            float survivedSeconds = dayNightClock.getElapsedTime().asSeconds();
            int daysSurvived = static_cast<int>(survivedSeconds / totalCycleTime);

            sf::RectangleShape overlay({ (float)width, (float)height });
            overlay.setFillColor(sf::Color(0, 0, 0, 200));
            window.draw(overlay);

            sf::Text deathText(font, "YOU DIED", 120);
            deathText.setFillColor(sf::Color::Red);
            deathText.setPosition({
                (width - deathText.getGlobalBounds().size.x) / 2.f,
                height / 2.f - 150.f
                });

            sf::Text daysText(
                font,
                "You survived " + std::to_string(daysSurvived) + " days",
                48
            );
            daysText.setFillColor(sf::Color::White);
            daysText.setPosition({
                (width - daysText.getGlobalBounds().size.x) / 2.f,
                height / 2.f
                });

            sf::Text exitText(font, "Press ESC to quit", 32);
            exitText.setFillColor(sf::Color::Yellow);
            exitText.setPosition({
                (width - exitText.getGlobalBounds().size.x) / 2.f,
                height / 2.f + 100.f
                });

            window.draw(deathText);
            window.draw(daysText);
            window.draw(exitText);
        }

        window.display();

        if (selectionDone && !itemSpawned)
        {
            const float MIN_DISTANCE = 120.f;

            for (int i = 0; i < 15; ++i)
            {
                for (int a = 0; a < 50; ++a)
                {
                    sf::Vector2f p(randX(gen), randY(gen));
                    if (!isInWater(p, pondBounds) &&
                        itemManager.isAreaFreeExplicit(p, MIN_DISTANCE, &treeTex))
                    {
                        itemManager.addItem(ITEM_TREE, treeTex, p, 1);
                        break;
                    }
                }
            }

            for (int i = 0; i < 15; ++i)
            {
                for (int a = 0; a < 50; ++a)
                {
                    sf::Vector2f p(randX(gen), randY(gen));
                    if (itemManager.isAreaFreeExplicit(p, MIN_DISTANCE, &stoneTex))
                    {
                        itemManager.addItem(ITEM_STONE, stoneTex, p, 1);
                        break;
                    }
                }
            }

            itemSpawned = true;

            const int CHICKEN_COUNT = 10;
            const int PIG_COUNT = 6;

            for (int i = 0; i < CHICKEN_COUNT; ++i)
            {
                animals.push_back(std::make_unique<Chicken>(
                    sf::Vector2f(randX(gen), randY(gen))
                ));
            }

            for (int i = 0; i < PIG_COUNT; ++i)
            {
                animals.push_back(std::make_unique<Pig>(
                    sf::Vector2f(randX(gen), randY(gen))
                ));
            }

        }
    }
}
