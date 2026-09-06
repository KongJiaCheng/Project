#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <future>
#include "TextInput.h"

void drawChatBox(
    sf::RenderWindow& window,
    const sf::Font& font,
    float chatBoxX,
    float chatBoxY,
    float chatBoxWidth,
    float chatBoxHeight,
    float chatScroll,
    const std::vector<std::pair<std::string, std::string>>& chatMessages,
    bool isChatting,
    TextInput& chatInput
);

void drawWrenchMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    const sf::Vector2f& wrenchMenuPos,
    bool isTargetNPC,
    const std::string& wrenchTargetPlayer
);

void drawP2PInvitePopup(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::string& initiator
);
