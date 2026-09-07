#pragma once
#include <SFML/Graphics.hpp>
#include <string>

void gameLoop(sf::RenderWindow& window, unsigned int width, unsigned int height, const std::string& username, bool isFirstTime);

// Slanted rhombus grid: e1=(500,0) horizontal, e2=(250,-250) at 45° up-right
// World position for cell (gx,gy): x = gx*500 + gy*250, y = -gy*250
// Compatible with std::function<sf::Vector2f(sf::Vector2f)>
inline sf::Vector2f snapToGrid(sf::Vector2f pos) {
    constexpr float CELL = 500.f;
    constexpr float SX   = 250.f;
    constexpr float SY   = 250.f;
    float gy = std::round(-pos.y / SY);
    float gx = std::round((pos.x - gy * SX) / CELL);
    pos.x = gx * CELL + gy * SX;
    pos.y = -gy * SY;
    return pos;
}

// Same grid — all items use 500-unit cell corners
inline sf::Vector2f snapToGrid2x2(sf::Vector2f pos) {
    return snapToGrid(pos);
}

// Door: same 250-unit cell snap
inline sf::Vector2f snapToGridDoor(sf::Vector2f pos) {
    return snapToGrid(pos);
}

inline float distanceToSegment(sf::Vector2f P, sf::Vector2f A, sf::Vector2f B) {
    float l2 = std::pow(A.x - B.x, 2) + std::pow(A.y - B.y, 2);
    if (l2 == 0.f) return std::sqrt(std::pow(P.x - A.x, 2) + std::pow(P.y - A.y, 2));
    float t = ((P.x - A.x) * (B.x - A.x) + (P.y - A.y) * (B.y - A.y)) / l2;
    t = std::max(0.f, std::min(1.f, t));
    sf::Vector2f projection = A + t * (B - A);
    return std::sqrt(std::pow(P.x - projection.x, 2) + std::pow(P.y - projection.y, 2));
}

inline int getFloorLevelFromY(float y) {
    int iy = (int)std::round(y);
    for (int L = 0; L <= 10; ++L) {
        if ((iy + L * 450) % 250 == 0) {
            return L;
        }
    }
    return 0;
}
