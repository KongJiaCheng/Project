#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <cmath>
#include <algorithm>

#include "Networking/AuthManager.h"
void gameLoop(sf::RenderWindow& window, unsigned int width, unsigned int height, const std::string& username, bool isFirstTime, BanInfo* outBanKick = nullptr);

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
    float dx = B.x - A.x;
    float dy = B.y - A.y;
    float l2 = dx * dx + dy * dy;
    if (l2 == 0.f) {
        float px = P.x - A.x;
        float py = P.y - A.y;
        return std::sqrt(px * px + py * py);
    }
    float t = ((P.x - A.x) * dx + (P.y - A.y) * dy) / l2;
    t = std::clamp(t, 0.f, 1.f);
    sf::Vector2f projection = A + t * sf::Vector2f(dx, dy);
    float rx = P.x - projection.x;
    float ry = P.y - projection.y;
    return std::sqrt(rx * rx + ry * ry);
}

inline bool segmentsIntersect(sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f q1, sf::Vector2f q2) {
    auto ccw = [](sf::Vector2f a, sf::Vector2f b, sf::Vector2f c) {
        return (c.y - a.y) * (b.x - a.x) > (b.y - a.y) * (c.x - a.x);
    };
    return (ccw(p1, q1, q2) != ccw(p2, q1, q2)) && (ccw(p1, p2, q1) != ccw(p1, p2, q2));
}

inline bool isPointInRhombus(sf::Vector2f pt, sf::Vector2f pos, float margin = 60.f) {
    if (pt.y < pos.y - 250.f - margin || pt.y > pos.y + margin) return false;
    float xShift = pos.y - pt.y;
    return (pt.x >= pos.x + xShift - margin && pt.x <= pos.x + 500.f + xShift + margin);
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
