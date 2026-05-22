#pragma once
#include <SFML/Graphics.hpp>
#include <string>

void gameLoop(sf::RenderWindow& window, unsigned int width, unsigned int height, const std::string& username, bool isFirstTime);
