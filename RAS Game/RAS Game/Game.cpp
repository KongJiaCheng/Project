#include "Game.h"
#include "ViewUtils.h"
#include "Camera.h"
#include "Inventory.h"
#include "ItemManager.h"
#include "Animation.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <cmath>
#include <random>

const int MAP_WIDTH = 5000;
const int MAP_HEIGHT = 5000;

void gameLoop(sf::RenderWindow& window, unsigned int width, unsigned int height)
{
    int option = 0;
    bool selectionDone = false;
    bool inventoryOpen = false;
    bool invKeyHeld = false;
    bool itemSpawned = false;

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

    sf::Texture texSouth;
    sf::Sprite sprite(texSouth);
    sprite.setOrigin({ 32.f, 32.f });
    sprite.setPosition({ width / 2.f, height / 2.f });

    sf::Texture woodTex;
    sf::Texture treeTex;
    sf::Texture stoneTex;

    woodTex.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/wood.png");
    treeTex.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/Tree.png");
    stoneTex.loadFromFile("C:/Users/DELL/Desktop/RobotAliceSkull/RAS Project/RAS Game/x64/Debug/Items/Stone.png");

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

    while (window.isOpen())
    {
        float dt = deltaClock.restart().asSeconds();

        while (std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>()) window.close();

            if (auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape) return;

                if (key->scancode == sf::Keyboard::Scancode::I && !invKeyHeld)
                {
                    inventoryOpen = !inventoryOpen;
                    invKeyHeld = true;
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

        window.clear(sf::Color::White);
        window.draw(background);
        itemManager.draw(window);
        window.draw(sprite);

        float nearestDist = itemManager.getNearestDistance(sprite);
        int nearestID = itemManager.getNearestItemID(sprite);
        const float RADIUS = 250.f;

        if (nearestDist < RADIUS)
        {
            sf::Text prompt(font, "", 32);
            prompt.setFillColor(sf::Color::Yellow);

            if (nearestID == 0)
                prompt.setString("Press E to chop tree");
            else prompt.setString("Press E to pick up");

            prompt.setPosition({ sprite.getPosition().x - 120, sprite.getPosition().y - 80 });

            window.draw(prompt);
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::E))
        {
            // itemManager.tryPickup takes player's sprite, inventory, and woodTex in your code
            itemManager.tryPickup(sprite, inventory, woodTex);
        }

        if (inventoryOpen)
        {
            window.setView(window.getDefaultView());
            inventory.draw(window);
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
                    if (itemManager.isAreaFreeExplicit(p, MIN_DISTANCE, &treeTex))
                    {
                        itemManager.addItem(0, treeTex, p, 1);
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
                        itemManager.addItem(1, stoneTex, p, 1);
                        break;
                    }
                }
            }

            itemSpawned = true;
        }
    }
}
