#pragma once
#include <SFML/Graphics.hpp>
#include <string>

class TextInput {
public:
    TextInput(sf::Font& font, sf::Vector2f position, sf::Vector2f size, bool isPassword = false);
    
    void processEvent(const sf::Event& event, const sf::Vector2f& mousePos);
    void draw(sf::RenderWindow& window);
    std::string getText() const;
    void setFocus(bool focus);
    bool hasFocus() const;
    void clear();

private:
    sf::RectangleShape background;
    sf::Text textDisplay;
    std::string inputString;
    bool focused;
    bool isPasswordBox;
};
