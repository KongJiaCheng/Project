#pragma once
#include <SFML/Graphics.hpp>

struct GameSettings {
    int volume = 50;
    int brightness = 50;
    int resolutionIndex = 1;
    sf::Keyboard::Scancode keyUp = sf::Keyboard::Scancode::W;
    sf::Keyboard::Scancode keyDown = sf::Keyboard::Scancode::S;
    sf::Keyboard::Scancode keyLeft = sf::Keyboard::Scancode::A;
    sf::Keyboard::Scancode keyRight = sf::Keyboard::Scancode::D;
    sf::Keyboard::Scancode keyInventory = sf::Keyboard::Scancode::I;
    sf::Keyboard::Scancode keyInteract = sf::Keyboard::Scancode::E;
    sf::Keyboard::Scancode keySprint = sf::Keyboard::Scancode::LShift;
    sf::Keyboard::Scancode keyAttack = sf::Keyboard::Scancode::Space;
    sf::Keyboard::Scancode keyChat = sf::Keyboard::Scancode::T;
    sf::Keyboard::Scancode keyRotate = sf::Keyboard::Scancode::R;
};

extern GameSettings globalSettings;

void openSettingsWindow(sf::RenderWindow& window, unsigned int& width, unsigned int& height);
