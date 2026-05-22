#include <iostream>
#include <SFML/Graphics.hpp>
#include "ViewUtils.h"

void UpdateCamera(sf::RenderWindow& window, const sf::Sprite& player, int mapWidth, int mapHeight)
{
    sf::View view = window.getView();

    // Make gameplay camera size larger (180% of logical UI size) to zoom out more
    sf::View uiView = GetUIView(&window);
    view.setSize({ uiView.getSize().x * 1.8f, uiView.getSize().y * 1.8f });

    sf::Vector2f pos = player.getPosition();
    view.setCenter(pos);

    // Clamping camera to map boundaries based on the smaller view size
    float halfW = view.getSize().x / 2.f;
    float halfH = view.getSize().y / 2.f;

    if (pos.x < halfW) pos.x = halfW;
    if (pos.y < halfH) pos.y = halfH;
    if (pos.x > mapWidth - halfW)  pos.x = mapWidth - halfW;
    if (pos.y > mapHeight - halfH) pos.y = mapHeight - halfH;

    view.setCenter(pos);
    window.setView(view);
}
