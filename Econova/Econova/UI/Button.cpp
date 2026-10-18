#include "Button.h"

Button::Button(
    const sf::Font& font,
    const std::string& label,
    sf::Vector2f size,
    unsigned int characterSize
)
    : text(font, label, characterSize)
{
    shape.setSize(size);

    shape.setFillColor(sf::Color::Green);

    text.setFillColor(sf::Color::White);

    // Center text inside button
    sf::FloatRect textBounds = text.getLocalBounds();

    text.setOrigin({
        textBounds.position.x + textBounds.size.x / 2.f,
        textBounds.position.y + textBounds.size.y / 2.f
        });
}

void Button::setPosition(sf::Vector2f position)
{
    shape.setPosition(position);

    sf::Vector2f size = shape.getSize();

    text.setPosition({
        position.x + size.x / 2.f,
        position.y + size.y / 2.f
        });
}

void Button::setFillColor(sf::Color color)
{
    shape.setFillColor(color);
}

void Button::setTextColor(sf::Color color)
{
    text.setFillColor(color);
}

void Button::setOutlineThickness(float thickness)
{
    shape.setOutlineThickness(thickness);
}

void Button::setOutlineColor(sf::Color color)
{
    shape.setOutlineColor(color);
}

void Button::setText(const std::string& label)
{
    text.setString(label);

    sf::FloatRect bounds = text.getLocalBounds();

    text.setOrigin({
        bounds.position.x + bounds.size.x / 2.f,
        bounds.position.y + bounds.size.y / 2.f
        });

    setPosition(shape.getPosition());
}

bool Button::isMouseOver(const sf::RenderWindow& window) const
{
    sf::Vector2f mousePosition =
        window.mapPixelToCoords(sf::Mouse::getPosition(window));

    return shape.getGlobalBounds().contains(mousePosition);
}

bool Button::isClicked(
    const sf::RenderWindow& window,
    const sf::Event& event
) const
{
    if (event.is<sf::Event::MouseButtonPressed>())
    {
        const auto* mouse =
            event.getIf<sf::Event::MouseButtonPressed>();

        if (mouse->button == sf::Mouse::Button::Left)
        {
            return isMouseOver(window);
        }
    }

    return false;
}

void Button::draw(sf::RenderWindow& window) const
{
    window.draw(shape);
    window.draw(text);
}

sf::Vector2f Button::getPosition() const
{
    return shape.getPosition();
}

sf::Vector2f Button::getSize() const
{
    return shape.getSize();
}

sf::FloatRect Button::getGlobalBounds() const
{
    return shape.getGlobalBounds();
}