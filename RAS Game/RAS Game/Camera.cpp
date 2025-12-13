#include <iostream>
#include <SFML/Graphics.hpp>

void UpdateCamera(sf::RenderWindow& window, const sf::Sprite& player, int mapWidth, int mapHeight)
{
    sf::View view = window.getView();

    sf::Vector2f pos = player.getPosition();
    view.setCenter(pos);

    // OPTIONAL: clamp camera to map boundaries
    float halfW = view.getSize().x / 2.f;
    float halfH = view.getSize().y / 2.f;

    if (pos.x < halfW) pos.x = halfW;
    if (pos.y < halfH) pos.y = halfH;
    if (pos.x > mapWidth - halfW)  pos.x = mapWidth - halfW;
    if (pos.y > mapHeight - halfH) pos.y = mapHeight - halfH;

    view.setCenter(pos);
    window.setView(view);
}
