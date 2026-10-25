#include "TextInput.h"
#include <cstdint>

TextInput::TextInput(sf::Font& font, sf::Vector2f position, sf::Vector2f size, bool isPassword) 
    : textDisplay(font), focused(false), isPasswordBox(isPassword), isMutedBox(false) {
    background.setPosition(position);
    background.setSize(size);
    background.setFillColor(sf::Color::White);
    background.setOutlineThickness(2.f);
    background.setOutlineColor(sf::Color::Black);

    textDisplay.setCharacterSize(static_cast<unsigned int>(size.y * 0.6f));
    textDisplay.setFillColor(sf::Color::Black);
    textDisplay.setPosition({ position.x + 5.f, position.y + (size.y - textDisplay.getCharacterSize()) / 2.f - 5.f });
}

void TextInput::processEvent(const sf::Event& event, const sf::Vector2f& mousePos) {
    if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouse->button == sf::Mouse::Button::Left) {
            if (background.getGlobalBounds().contains(mousePos)) {
                setFocus(true);
            } else {
                setFocus(false);
            }
        }
    }

    if (focused) {
        if (const auto* text = event.getIf<sf::Event::TextEntered>()) {
            std::uint32_t unicode = text->unicode;
            if (unicode == 8 && !inputString.empty()) { // Backspace
                inputString.pop_back();
            } else if (unicode >= 32 && unicode < 128) { // Printable characters
                if (isMutedBox) {
                    static const char GIBBERISH_POOL[] = "mfmfmfmfmmfmmmf";
                    char gib = GIBBERISH_POOL[inputString.length() % (sizeof(GIBBERISH_POOL) - 1)];
                    inputString += gib;
                } else {
                    inputString += static_cast<char>(unicode);
                }
            }
            
            std::string displayStr = isPasswordBox ? std::string(inputString.length(), '*') : inputString;
            textDisplay.setString(displayStr);
        }
    }
}

void TextInput::draw(sf::RenderWindow& window) {
    window.draw(background);
    window.draw(textDisplay);
}

std::string TextInput::getText() const {
    return inputString;
}

void TextInput::setFocus(bool focus) {
    focused = focus;
    background.setOutlineColor(focused ? sf::Color::Blue : sf::Color::Black);
}

bool TextInput::hasFocus() const {
    return focused;
}

void TextInput::clear() {
    inputString.clear();
    textDisplay.setString("");
}

void TextInput::setText(const std::string& text) {
    inputString = text;
    std::string displayStr = isPasswordBox ? std::string(inputString.length(), '*') : inputString;
    textDisplay.setString(displayStr);
}

void TextInput::setPosition(sf::Vector2f position) {
    background.setPosition(position);
    textDisplay.setPosition({ position.x + 5.f, position.y + (background.getSize().y - textDisplay.getCharacterSize()) / 2.f - 5.f });
}

void TextInput::setMuted(bool muted) {
    isMutedBox = muted;
}

bool TextInput::isMuted() const {
    return isMutedBox;
}
