#pragma once
#include <SFML/Graphics.hpp>

// Update the view to exactly match the current window size (SFML 3)
inline void UpdateViewToWindow(sf::RenderWindow* window)
{
    if (!window) return;

    sf::Vector2u size = window->getSize();

    sf::View view;
    view.setSize(sf::Vector2f(static_cast<float>(size.x),
        static_cast<float>(size.y)));
    view.setCenter(sf::Vector2f(view.getSize().x / 2.f,
        view.getSize().y / 2.f));

    window->setView(view);
}
