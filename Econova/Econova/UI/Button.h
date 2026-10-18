#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <string>

class Button
{
private:
    sf::RectangleShape shape;
    sf::Text text;
    sf::Vector2f size;

public:
    Button(
        const sf::Font& font,
        const std::string& label,
        sf::Vector2f size,
        unsigned int characterSize
    );

    void setPosition(sf::Vector2f position);

    void setFillColor(sf::Color color);
    void setTextColor(sf::Color color);
    void setOutlineThickness(float thickness);
    void setOutlineColor(sf::Color color);

    void setText(const std::string& label);

    bool isMouseOver(const sf::RenderWindow& window) const;
    bool isClicked(const sf::RenderWindow& window, const sf::Event& event) const;

    void draw(sf::RenderWindow& window) const;

    sf::Vector2f getPosition() const;
    sf::Vector2f getSize() const;
    sf::FloatRect getGlobalBounds() const;
};