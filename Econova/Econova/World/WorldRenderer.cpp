#include "WorldRenderer.h"
#include "ItemID.h"
#include <cmath>

void drawFloorGrid(
    sf::RenderWindow& window,
    unsigned int mapWidth,
    unsigned int mapHeight
) {
    // Rhombus grid basis:
    //   e1 = (CELL, 0)   — horizontal axis
    //   e2 = (SX, -SY)   — 45° up-right axis  (SX == SY → exactly 45°)
    constexpr float CELL = 500.f;
    constexpr float SX   = 250.f;   // e2.x per row step
    constexpr float SY   = 250.f;   // e2.y per row step (negative = up on screen)

    sf::View currentView = window.getView();
    sf::Vector2f vCenter = currentView.getCenter();
    sf::Vector2f vSize   = currentView.getSize();
    float margin = CELL * 2.f;

    float left   = vCenter.x - vSize.x * 0.5f - margin;
    float right  = vCenter.x + vSize.x * 0.5f + margin;
    float top    = vCenter.y - vSize.y * 0.5f - margin;
    float bottom = vCenter.y + vSize.y * 0.5f + margin;

    // ── Hover cell ────────────────────────────────────────────────────────────
    sf::Vector2i pixelMouse = sf::Mouse::getPosition(window);
    sf::Vector2f worldM     = window.mapPixelToCoords(pixelMouse);
    int hoverGy = static_cast<int>(std::round(-worldM.y / SY));
    int hoverGx = static_cast<int>(std::round((worldM.x - hoverGy * SX) / CELL));

    // ── Row range (horizontal lines): y = -gy * SY ───────────────────────────
    int gyMin = static_cast<int>(std::floor(-bottom / SY)) - 1;
    int gyMax = static_cast<int>(std::ceil (-top    / SY)) + 1;

    // ── Column range (diagonal lines): visible when gx*CELL - y is in [left,right]
    // At top edge: x = gx*CELL - top  (since SX==SY, offset = -gy*SX = y)
    // At bottom edge: x = gx*CELL - bottom
    int gxMin = static_cast<int>(std::floor((left  + top)    / CELL)) - 2;
    int gxMax = static_cast<int>(std::ceil ((right + bottom) / CELL)) + 2;

    const sf::Color gridColor (255, 255, 255, 110);   // bright enough to see clearly
    const sf::Color hoverColor(255, 220,  40, 220);   // golden hover highlight

    sf::VertexArray lines(sf::PrimitiveType::Lines);

    // ── Horizontal row lines (parallel to e1) ────────────────────────────────
    for (int gy = gyMin; gy <= gyMax; ++gy) {
        float wy = -gy * SY;
        sf::Color col = gridColor;
        lines.append(sf::Vertex{{ left,  wy }, col});
        lines.append(sf::Vertex{{ right, wy }, col});
    }

    // ── Diagonal column lines (parallel to e2 = (SX, -SY)) ───────────────────
    // Each column gx: x = gx*CELL + gy*SX, y = -gy*SY
    // Since SX == SY: x + y = gx*CELL, so x = gx*CELL - y
    // Clip to [top, bottom]: y in [top, bottom] → x in [gx*CELL - bottom, gx*CELL - top]
    for (int gx = gxMin; gx <= gxMax; ++gx) {
        float x_at_top    = gx * CELL - top;      // x when y == top
        float x_at_bottom = gx * CELL - bottom;   // x when y == bottom
        sf::Color col = gridColor;
        lines.append(sf::Vertex{{ x_at_bottom, bottom }, col});
        lines.append(sf::Vertex{{ x_at_top,    top    }, col});
    }

    // ── Hover cell highlight (draw its 4 edges separately) ───────────────────
    {
        float bx = hoverGx * CELL + hoverGy * SX;
        float by = -hoverGy * SY;
        // 4 corners of the hovered rhombus cell
        sf::Vector2f BL{ bx,        by };
        sf::Vector2f BR{ bx + CELL, by };
        sf::Vector2f TR{ bx + CELL + SX, by - SY };
        sf::Vector2f TL{ bx + SX,        by - SY };

        lines.append(sf::Vertex{BL, hoverColor}); lines.append(sf::Vertex{BR, hoverColor});
        lines.append(sf::Vertex{BR, hoverColor}); lines.append(sf::Vertex{TR, hoverColor});
        lines.append(sf::Vertex{TR, hoverColor}); lines.append(sf::Vertex{TL, hoverColor});
        lines.append(sf::Vertex{TL, hoverColor}); lines.append(sf::Vertex{BL, hoverColor});
    }

    window.draw(lines);
}

void drawWorldCampfires(
    sf::RenderWindow& window,
    const sf::Font& font,
    const std::vector<PlacedCampfire>& campfires,
    const sf::Texture campfireFrames[8],
    const sf::Texture& wrenchTex,
    const sf::Sprite& playerSprite,
    Inventory& inventory
) {
    for (const auto& pc : campfires) {
        sf::Sprite cs(campfireFrames[pc.currentFrame]);
        sf::Vector2u cfs = cs.getTexture().getSize();
        cs.setOrigin({ cfs.x / 2.f, (float)cfs.y });
        cs.setScale({ 0.9f, 0.9f });
        cs.setPosition(pc.position);
        if (!pc.isCrafted) {
            cs.setColor(sf::Color(255, 255, 255, 128));
        }
        window.draw(cs);

        if (!pc.isCrafted) {
            sf::Text t(font, "Crafting: " + std::to_string((int)pc.timeRemaining) + "s", 16);
            t.setFillColor(sf::Color::White);
            t.setPosition({ pc.position.x - 40.f, pc.position.y - 40.f });
            window.draw(t);
        }
        else {
            int selSlot = inventory.getSelectedSlot();
            bool hasWrench = false;
            if (selSlot != -1 && inventory.hasItem(selSlot) && inventory.getItemID(selSlot) == ITEM_WRENCH) {
                hasWrench = true;
            }
            for (int i = 0; i < 20; i++) {
                if (inventory.hasItem(i) && inventory.getItemID(i) == ITEM_WRENCH && inventory.isEquipped(i)) {
                    hasWrench = true;
                    break;
                }
            }
            if (hasWrench) {
                sf::Sprite ws(wrenchTex);
                sf::Vector2u wts = wrenchTex.getSize();
                ws.setOrigin({ wts.x / 2.f, wts.y / 2.f });
                ws.setScale({ 0.08f, 0.08f });
                ws.setPosition({ pc.position.x, pc.position.y - 480.f });
                window.draw(ws);
            }
        }
    }
}

// ── Rhombus tile renderer ─────────────────────────────────────────────────────
// Draws a textured quad filling one parallelogram cell of the slanted grid.
// pos = BL corner of the cell (direct result of snapToGrid).
// Grid basis: e1=(500,0), e2=(250,-250)  → exactly 45° slant.
static void drawRhombusTile(
    sf::RenderWindow& window,
    const sf::Texture& tex,
    sf::Vector2f pos,
    sf::Color color = sf::Color::White
) {
    constexpr float CELL = 500.f, SX = 250.f, SY = 250.f;

    sf::Vector2f BL{ pos.x,             pos.y      };
    sf::Vector2f BR{ pos.x + CELL,      pos.y      };
    sf::Vector2f TR{ pos.x + CELL + SX, pos.y - SY };
    sf::Vector2f TL{ pos.x + SX,        pos.y - SY };

    auto sz = tex.getSize();
    float tw = static_cast<float>(sz.x);
    float th = static_cast<float>(sz.y);

    sf::VertexArray va(sf::PrimitiveType::TriangleFan, 4);
    va[0] = sf::Vertex{ BL, color, sf::Vector2f{0.f, th} };
    va[1] = sf::Vertex{ BR, color, sf::Vector2f{tw,  th} };
    va[2] = sf::Vertex{ TR, color, sf::Vector2f{tw,  0.f} };
    va[3] = sf::Vertex{ TL, color, sf::Vector2f{0.f, 0.f} };

    sf::RenderStates rs;
    rs.texture = &tex;
    window.draw(va, rs);
}

void drawWall(
    sf::RenderWindow& window,
    const sf::Texture& tex,
    sf::Vector2f pos,
    float rotation,
    sf::Color color
) {
    constexpr float CELL = 500.f, SX = 250.f, SY = 250.f;
    constexpr float WALL_HEIGHT = 500.f;

    auto sz = tex.getSize();
    float tw = static_cast<float>(sz.x);
    float th = static_cast<float>(sz.y);

    sf::Vector2f BL, BR, TR, TL;
    if (rotation == 90.f) {
        // Sheared grid position: along e2 = (SX, -SY)
        BL = pos;
        BR = { pos.x + SX, pos.y - SY };
        TR = { pos.x + SX, pos.y - SY - WALL_HEIGHT };
        TL = { pos.x,      pos.y - WALL_HEIGHT };
    } else {
        // Horizontal grid position: along e1 = (CELL, 0)
        BL = pos;
        BR = { pos.x + CELL, pos.y };
        TR = { pos.x + CELL, pos.y - WALL_HEIGHT };
        TL = { pos.x,        pos.y - WALL_HEIGHT };
    }

    sf::VertexArray va(sf::PrimitiveType::TriangleFan, 4);
    va[0] = sf::Vertex{ BL, color, sf::Vector2f{0.f, th} };
    va[1] = sf::Vertex{ BR, color, sf::Vector2f{tw,  th} };
    va[2] = sf::Vertex{ TR, color, sf::Vector2f{tw,  0.f} };
    va[3] = sf::Vertex{ TL, color, sf::Vector2f{0.f, 0.f} };

    sf::RenderStates rs;
    rs.texture = &tex;
    window.draw(va, rs);
}

void drawPlacementPreview(
    sf::RenderWindow& window,
    int placementItemID,
    float placementRotation,
    sf::Vector2f placementPos,
    const sf::Texture campfireFrames[8],
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    const sf::Texture& chestTex
) {
    sf::Sprite preview(campfireFrames[0]);
    if (placementItemID == ITEM_NONE) preview.setTexture(campfireFrames[0], true);
    else if (placementItemID == ITEM_FURNACE) preview.setTexture(furnaceTex, true);
    else if (placementItemID == ITEM_WOOD_TILE) preview.setTexture(woodTileTex, true);
    else if (placementItemID == ITEM_WOOD_WALL) preview.setTexture(woodWallTex, true);
    else if (placementItemID == ITEM_WOOD_DOOR) preview.setTexture(woodDoorTex, true);
    else if (placementItemID == ITEM_WOOD_ROOF) preview.setTexture(woodRoofTex, true);
    else if (placementItemID == ITEM_WOOD_STAIRS) preview.setTexture(woodStairsTex, true);
    else if (placementItemID == ITEM_BED) preview.setTexture(bedTex, true);
    else if (placementItemID == ITEM_CHEST) preview.setTexture(chestTex, true);

    sf::Vector2u pts = preview.getTexture().getSize();
    // Wall and door anchor to BL corner — use bottom-left origin to match placed sprite
    if (placementItemID == ITEM_WOOD_WALL || placementItemID == ITEM_WOOD_DOOR) {
        preview.setOrigin({ 0.f, (float)pts.y });
    } else {
        preview.setOrigin({ pts.x / 2.f, (float)pts.y });
    }

    float previewShear = 0.f;
    applyStructureScale(preview, placementItemID, placementRotation, false, previewShear);

    preview.setColor(sf::Color(255, 255, 255, 180));
    preview.setPosition(placementPos);

    if (placementItemID == ITEM_WOOD_TILE) {
        // Draw as rhombus to match the grid cell exactly
        drawRhombusTile(window, preview.getTexture(), placementPos,
                        sf::Color(255, 255, 255, 160));
    } else if (placementItemID == ITEM_WOOD_WALL) {
        drawWall(window, preview.getTexture(), placementPos, placementRotation,
                 sf::Color(255, 255, 255, 180));
    } else {
        window.draw(preview);
    }
}

void drawSpeechBubble(
    sf::RenderWindow& window,
    const sf::Font& font,
    const sf::Vector2f& playerPos,
    float bubbleTimer,
    const std::string& latestPlayerMsg
) {
    if (bubbleTimer > 0.f && !latestPlayerMsg.empty()) {
        sf::Text bubbleText(font, latestPlayerMsg, 24);
        bubbleText.setFillColor(sf::Color::Black);
        sf::FloatRect textBounds = bubbleText.getLocalBounds();

        float paddingX = 15.f;
        float paddingY = 10.f;
        sf::RectangleShape bubbleBg({ textBounds.size.x + paddingX * 2.f, textBounds.size.y + paddingY * 2.f });
        bubbleBg.setFillColor(sf::Color(255, 255, 255, 220));
        bubbleBg.setOutlineColor(sf::Color::Black);
        bubbleBg.setOutlineThickness(3.f);

        bubbleBg.setPosition({ playerPos.x - bubbleBg.getSize().x / 2.f, playerPos.y - 360.f });
        bubbleText.setPosition({ bubbleBg.getPosition().x + paddingX, bubbleBg.getPosition().y + paddingY - 6.f });

        window.draw(bubbleBg);
        window.draw(bubbleText);
    }
}

#include "Game.h"
#include <algorithm>

void renderWorldDepthScene(
    sf::RenderWindow& window,
    sf::Sprite& sprite,
    std::vector<PlacedStructure>& structures,
    const std::vector<TileFloor>& elevatedTileFloors,
    int& playerFloorLevel,
    bool saplingPlacementMode,
    bool saplingFollowingCursor,
    sf::Vector2f& saplingPlacementPos,
    const sf::Texture& saplingTex,
    const std::vector<PlantedSapling>& plantedSaplings,
    const sf::Font& font,
    bool isPlayerHurt,
    float& playerHurtTimer,
    float dt,
    const std::vector<std::unique_ptr<Animal>>& animals,
    const std::vector<std::unique_ptr<Monster>>& monsters,
    bool animLoadedForSelection,
    const NPC& npcFemale,
    const NPC& npcMale,
    const std::string& username,
    const sf::Sprite& playerWrenchBtn,
    float bubbleTimer,
    const std::string& latestPlayerMsg,
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc
) {
    const float TILE_CONNECT_DIST = 260.f;
    bool isUnderRoof = false;
    float stairOffset = 0.f;
    sf::Vector2f pPos = sprite.getPosition();
    sf::Vector2f pFeetGlobal = { pPos.x, pPos.y + 250.f };

    playerFloorLevel = 0;
    bool playerOnStairs = false;

    for (auto& etf : elevatedTileFloors) {
        float dx = pFeetGlobal.x - etf.pos.x;
        float dy = pFeetGlobal.y - etf.pos.y;
        if (std::sqrt(dx * dx + dy * dy) < TILE_CONNECT_DIST) {
            if (etf.floorLevel > playerFloorLevel) {
                playerFloorLevel = etf.floorLevel;
            }
        }
    }

    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_STAIRS && s.sprite.has_value()) {
            sf::FloatRect sb = s.sprite->getGlobalBounds();
            sb.position.x -= 30.f; sb.size.x += 60.f;
            sb.position.y -= 30.f; sb.size.y += 60.f;
            if (sb.contains(pFeetGlobal)) {
                playerOnStairs = true;
                float progress = 0.f;
                if (s.rotation == 90.f) {
                    progress = (pFeetGlobal.x - sb.position.x) / sb.size.x;
                }
                else if (s.rotation == 180.f) {
                    progress = 1.0f - (pFeetGlobal.x - sb.position.x) / sb.size.x;
                }
                else {
                    progress = 1.0f - (pFeetGlobal.y - sb.position.y) / sb.size.y;
                }
                if (progress < 0.f) progress = 0.f;
                if (progress > 1.f) progress = 1.f;
                float baseOffset = -(float)playerFloorLevel * 180.f;
                stairOffset = baseOffset + progress * -180.f;
            }
        }
    }

    if (!playerOnStairs && playerFloorLevel > 0) {
        stairOffset = -(float)playerFloorLevel * 180.f;
    }

    bool isUnderElevatedFloor = false;
    for (auto& etf : elevatedTileFloors) {
        if (etf.floorLevel > playerFloorLevel) {
            float dx = pPos.x - etf.pos.x;
            float dy = pPos.y - etf.pos.y;
            if (std::abs(dx) < 200.f && std::abs(dy) < 150.f) {
                isUnderElevatedFloor = true;
                break;
            }
        }
    }

    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_ROOF || s.id == ITEM_WOOD_WALL) {
            sf::FloatRect rb = s.sprite->getGlobalBounds();
            rb.position.x -= 60.f; rb.size.x += 120.f;
            rb.position.y -= 60.f; rb.size.y += 120.f;
            if (rb.contains(pPos)) {
                isUnderRoof = true;
            }
        }
    }

    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_ROOF || s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR) {
            if (s.rotation == 0.f && (s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR)) {
                bool isBackWall = false;
                if (s.sprite.has_value()) {
                    sf::Vector2f sPos = s.sprite->getPosition();
                    for (auto& t : structures) {
                        if (t.id == ITEM_WOOD_TILE && t.sprite.has_value()) {
                            sf::Vector2f tPos = t.sprite->getPosition();
                            if (std::abs(tPos.x - sPos.x) < 50.f && std::abs(tPos.y - (sPos.y + 100.f)) < 50.f) {
                                isBackWall = true;
                                break;
                            }
                        }
                    }
                }

                if (isBackWall) {
                    if (isUnderRoof || isUnderElevatedFloor) s.sprite->setColor(sf::Color(255, 255, 255, 128));
                    else s.sprite->setColor(sf::Color(255, 255, 255, 0));
                }
                else {
                    if (isUnderRoof || isUnderElevatedFloor) s.sprite->setColor(sf::Color(255, 255, 255, 128));
                    else s.sprite->setColor(sf::Color::White);
                }
            }
            else {
                if (isUnderRoof || isUnderElevatedFloor) s.sprite->setColor(sf::Color(255, 255, 255, 128));
                else s.sprite->setColor(sf::Color::White);
            }
        }
    }

    auto tileIsElevated = [&](sf::Vector2f tp) -> bool {
        for (auto& etf : elevatedTileFloors) {
            float dx = tp.x - etf.pos.x;
            float dy = tp.y - etf.pos.y;
            if (dx * dx + dy * dy < 1.f) return true;
        }
        return false;
    };

    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
            if (isUnderElevatedFloor && tileIsElevated(s.sprite->getPosition())) {
                s.sprite->setColor(sf::Color(255, 255, 255, 128));
            }
            else {
                s.sprite->setColor(sf::Color::White);
            }
        }
    }

    // 1. Draw ground-floor tiles first (as rhombus quads)
    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
            if (tileIsElevated(s.sprite->getPosition())) continue;
            float dx = s.sprite->getPosition().x - sprite.getPosition().x;
            float dy = s.sprite->getPosition().y - sprite.getPosition().y;
            if (dx * dx + dy * dy < 3000000.f) {
                drawRhombusTile(window, s.sprite->getTexture(),
                                s.sprite->getPosition(), s.sprite->getColor());
            }
        }
    }

    // 2. Depth sort and draw
    struct Drawable { float groundY; float z; int priority; std::function<void()> draw; };
    std::vector<Drawable> depthDrawables;

    for (auto& s : structures) {
        if (!s.sprite.has_value()) continue;
        float dx = s.sprite->getPosition().x - sprite.getPosition().x;
        float dy = s.sprite->getPosition().y - sprite.getPosition().y;
        if (dx * dx + dy * dy >= 3000000.f) continue;

        float sPos_y = s.sprite->getPosition().y;
        int sFloor = getFloorLevelFromY(sPos_y);
        float sGroundY = sPos_y + sFloor * 180.f;
        float sZ = sFloor * 180.f;

        if (s.id == ITEM_WOOD_TILE) {
            if (tileIsElevated(s.sprite->getPosition())) {
                depthDrawables.push_back({ sGroundY, sZ, 0, [&s, &window]() {
                    if (s.sprite.has_value())
                        drawRhombusTile(window, s.sprite->getTexture(),
                                        s.sprite->getPosition(), s.sprite->getColor());
                } });
            }
        }
        else if (s.id == ITEM_WOOD_WALL) {
            depthDrawables.push_back({ sGroundY, sZ, 1, [&s, &window]() {
                if (s.sprite.has_value())
                    drawWall(window, s.sprite->getTexture(),
                             s.sprite->getPosition(), s.rotation, s.sprite->getColor());
            } });
        }
        else {
            int priority = (s.id == ITEM_WOOD_ROOF) ? 2 : 1;
            depthDrawables.push_back({ sGroundY, sZ, priority, [&s, &window]() {
                if (s.sprite.has_value()) window.draw(*s.sprite);
            } });
        }
    }

    if (saplingPlacementMode) {
        if (saplingFollowingCursor) {
            sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
            sf::Vector2f worldM = window.mapPixelToCoords(pixelPos);
            saplingPlacementPos = snapGridFunc(worldM);
        }
        sf::Sprite ghost(saplingTex);
        ghost.setScale({ 0.2f, 0.2f });
        ghost.setColor(sf::Color(255, 255, 255, 128));
        ghost.setPosition(saplingPlacementPos);
        window.draw(ghost);
    }

    for (auto& s : plantedSaplings) {
        if (!s.grown) {
            sf::Sprite spr(saplingTex);
            spr.setScale({ 0.2f, 0.2f });
            spr.setPosition(s.position);
            window.draw(spr);

            sf::Text t(font, "Growing: " + std::to_string((int)s.growTimer) + "s", 16);
            t.setFillColor(sf::Color::White);
            t.setPosition({ s.position.x - 30.f, s.position.y - 40.f });
            window.draw(t);
        }
    }

    if (isPlayerHurt) {
        playerHurtTimer -= dt;
        if (playerHurtTimer <= 0.f) isPlayerHurt = false;
        sprite.setColor(sf::Color(255, 100, 100));
    }
    else {
        sprite.setColor(sf::Color::White);
    }

    sf::Vector2f originalPlayerPos = sprite.getPosition();
    sprite.move({ 0.f, stairOffset });
    depthDrawables.push_back({ originalPlayerPos.y, -stairOffset, 1, [&sprite, &window]() { window.draw(sprite); } });

    for (auto& a : animals) {
        if (a->health > 0) {
            float dx = a->sprite.getPosition().x - sprite.getPosition().x;
            float dy = a->sprite.getPosition().y - sprite.getPosition().y;
            if (dx * dx + dy * dy < 3000000.f) {
                depthDrawables.push_back({ a->sprite.getPosition().y, 0.f, 1, [&a, &window]() {
                    window.draw(a->sprite);
                    if (a->health < a->maxHealth) {
                        sf::FloatRect bounds = a->sprite.getGlobalBounds();
                        sf::RectangleShape hpBg({ 40.f, 6.f });
                        hpBg.setFillColor(sf::Color::Red);
                        hpBg.setPosition({ bounds.position.x + bounds.size.x / 2.f - 20.f, bounds.position.y - 15.f });

                        sf::RectangleShape hpFg({ 40.f * (std::max(0.f, a->health) / a->maxHealth), 6.f });
                        hpFg.setFillColor(sf::Color::Green);
                        hpFg.setPosition(hpBg.getPosition());

                        window.draw(hpBg);
                        window.draw(hpFg);
                    }
                } });
            }
        }
    }
    for (auto& m : monsters) {
        if (m->health > 0) {
            float dx = m->sprite.getPosition().x - sprite.getPosition().x;
            float dy = m->sprite.getPosition().y - sprite.getPosition().y;
            if (dx * dx + dy * dy < 3000000.f) {
                depthDrawables.push_back({ m->sprite.getPosition().y, 0.f, 1, [&m, &window]() {
                    window.draw(m->sprite);
                    if (m->health < m->maxHealth) {
                        sf::FloatRect bounds = m->sprite.getGlobalBounds();
                        sf::RectangleShape hpBg({ 40.f, 6.f });
                        hpBg.setFillColor(sf::Color::Red);
                        hpBg.setPosition({ bounds.position.x + bounds.size.x / 2.f - 20.f, bounds.position.y - 15.f });

                        sf::RectangleShape hpFg({ 40.f * (std::max(0.f, m->health) / m->maxHealth), 6.f });
                        hpFg.setFillColor(sf::Color::Green);
                        hpFg.setPosition(hpBg.getPosition());

                        window.draw(hpBg);
                        window.draw(hpFg);
                    }
                } });
            }
        }
    }

    std::sort(depthDrawables.begin(), depthDrawables.end(), [](const Drawable& a, const Drawable& b) {
        if (std::abs(a.groundY - b.groundY) > 0.001f) {
            return a.groundY < b.groundY;
        }
        if (std::abs(a.z - b.z) > 0.001f) {
            return a.z < b.z;
        }
        return a.priority < b.priority;
    });

    for (auto& d : depthDrawables) d.draw();

    sprite.setPosition(originalPlayerPos);
    sprite.setColor(sf::Color::White);

    if (animLoadedForSelection) {
        if (npcFemale.sprite.has_value()) window.draw(*npcFemale.sprite);
        if (npcMale.sprite.has_value()) window.draw(*npcMale.sprite);
    }

    drawSpeechBubble(window, font, sprite.getPosition(), bubbleTimer, latestPlayerMsg);

    sf::Text nameText(font, username, 18);
    nameText.setFillColor(sf::Color::White);
    nameText.setOutlineColor(sf::Color::Black);
    nameText.setOutlineThickness(1.5f);
    nameText.setPosition({ sprite.getPosition().x - nameText.getLocalBounds().size.x / 2.f, sprite.getPosition().y - 280.f });
    auto wrenchBtnCopy = playerWrenchBtn;
    wrenchBtnCopy.setPosition({ nameText.getPosition().x + nameText.getLocalBounds().size.x + 5.f, nameText.getPosition().y });
    window.draw(nameText);
    window.draw(wrenchBtnCopy);

    if (animLoadedForSelection) {
        if (npcFemale.sprite.has_value() && npcFemale.wrenchBtn.has_value()) {
            sf::Text npcFemaleNameText(font, npcFemale.name, 18);
            npcFemaleNameText.setFillColor(sf::Color::White);
            npcFemaleNameText.setOutlineColor(sf::Color::Black);
            npcFemaleNameText.setOutlineThickness(1.5f);
            npcFemaleNameText.setPosition({ npcFemale.sprite->getPosition().x - npcFemaleNameText.getLocalBounds().size.x / 2.f, npcFemale.sprite->getPosition().y - 280.f });
            auto wBtn = *npcFemale.wrenchBtn;
            wBtn.setPosition({ npcFemaleNameText.getPosition().x + npcFemaleNameText.getLocalBounds().size.x + 5.f, npcFemaleNameText.getPosition().y });
            window.draw(npcFemaleNameText);
            window.draw(wBtn);
        }

        if (npcMale.sprite.has_value() && npcMale.wrenchBtn.has_value()) {
            sf::Text npcMaleNameText(font, npcMale.name, 18);
            npcMaleNameText.setFillColor(sf::Color::White);
            npcMaleNameText.setOutlineColor(sf::Color::Black);
            npcMaleNameText.setOutlineThickness(1.5f);
            npcMaleNameText.setPosition({ npcMale.sprite->getPosition().x - npcMaleNameText.getLocalBounds().size.x / 2.f, npcMale.sprite->getPosition().y - 280.f });
            auto wBtn = *npcMale.wrenchBtn;
            wBtn.setPosition({ npcMaleNameText.getPosition().x + npcMaleNameText.getLocalBounds().size.x + 5.f, npcMaleNameText.getPosition().y });
            window.draw(npcMaleNameText);
            window.draw(wBtn);
        }
    }
}

void drawWorldInteractions(
    sf::RenderWindow& window,
    const sf::Font& font,
    const sf::Sprite& sprite,
    int playerFloorLevel,
    bool isSleeping,
    bool chestMenuOpen,
    std::vector<PlacedStructure>& structures,
    bool nearPond,
    sf::Text& drinkText
) {
    if (nearPond) {
        drinkText.setPosition({
            sprite.getPosition().x - 160.f,
            sprite.getPosition().y - 100.f
        });
        window.draw(drinkText);
    }

    PlacedStructure* nearestDoor = nullptr;
    float minDist = 300.f;
    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_DOOR) {
            sf::Vector2f dPos = s.sprite->getPosition();
            sf::Vector2f pPos = sprite.getPosition();
            int doorFloor = getFloorLevelFromY(dPos.y);
            if (doorFloor == playerFloorLevel) {
                float dy = (dPos.y + doorFloor * 180.f) - pPos.y;
                float dx = dPos.x - pPos.x;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < minDist) {
                    minDist = d;
                    nearestDoor = &s;
                }
            }
        }
    }

    if (nearestDoor) {
        sf::RectangleShape doorBtn({ 200.f, 60.f });
        doorBtn.setPosition({ nearestDoor->sprite->getPosition().x - 100.f, nearestDoor->sprite->getPosition().y - 120.f });

        sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        bool isHovered = doorBtn.getGlobalBounds().contains(mpos);
        doorBtn.setFillColor(isHovered ? sf::Color(80, 160, 220, 230) : sf::Color(60, 140, 200, 200));
        doorBtn.setOutlineThickness(3.f);
        doorBtn.setOutlineColor(isHovered ? sf::Color::White : sf::Color(255, 255, 255, 120));
        window.draw(doorBtn);

        sf::Text doorTxt(font, nearestDoor->isOpen ? "Close Door" : "Open Door", 28);
        sf::FloatRect b = doorTxt.getLocalBounds();
        doorTxt.setPosition({ doorBtn.getPosition().x + (200.f - b.size.x) / 2.f, doorBtn.getPosition().y + (60.f - b.size.y) / 2.f - 8.f });
        window.draw(doorTxt);
    }

    PlacedStructure* nearestBed = nullptr;
    float minBedDist = 180.f;
    for (auto& s : structures) {
        if (s.id == ITEM_BED) {
            sf::Vector2f bPos = s.sprite->getPosition();
            sf::Vector2f pPos = sprite.getPosition();
            int bedFloor = getFloorLevelFromY(bPos.y);
            if (bedFloor == playerFloorLevel) {
                float dy = (bPos.y + bedFloor * 180.f) - pPos.y;
                float dx = bPos.x - pPos.x;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < minBedDist) {
                    minBedDist = d;
                    nearestBed = &s;
                }
            }
        }
    }

    if (nearestBed && !isSleeping) {
        sf::RectangleShape sleepBtn({ 200.f, 60.f });
        sleepBtn.setPosition({ nearestBed->sprite->getPosition().x - 100.f, nearestBed->sprite->getPosition().y - 120.f });

        sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        bool isHovered = sleepBtn.getGlobalBounds().contains(mpos);
        sleepBtn.setFillColor(isHovered ? sf::Color(80, 160, 220, 230) : sf::Color(60, 140, 200, 200));
        sleepBtn.setOutlineThickness(3.f);
        sleepBtn.setOutlineColor(isHovered ? sf::Color::White : sf::Color(255, 255, 255, 120));
        window.draw(sleepBtn);

        sf::Text sleepTxt(font, "Sleep", 28);
        sf::FloatRect b = sleepTxt.getLocalBounds();
        sleepTxt.setPosition({ sleepBtn.getPosition().x + (200.f - b.size.x) / 2.f, sleepBtn.getPosition().y + (60.f - b.size.y) / 2.f - 8.f });
        window.draw(sleepTxt);
    }

    PlacedStructure* nearestChest = nullptr;
    float minChestDist = 200.f;
    for (auto& s : structures) {
        if (s.id == ITEM_CHEST) {
            sf::Vector2f cPos = s.sprite->getPosition();
            sf::Vector2f pPos = sprite.getPosition();
            int chestFloor = getFloorLevelFromY(cPos.y);
            if (chestFloor == playerFloorLevel) {
                float dy = (cPos.y + chestFloor * 200.f) - pPos.y;
                float dx = cPos.x - pPos.x;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < minChestDist) {
                    minChestDist = d;
                    nearestChest = &s;
                }
            }
        }
    }

    if (nearestChest && !chestMenuOpen) {
        sf::RectangleShape chestBtn({ 200.f, 60.f });
        chestBtn.setPosition({ nearestChest->sprite->getPosition().x - 100.f, nearestChest->sprite->getPosition().y - 120.f });

        sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        bool isHovered = chestBtn.getGlobalBounds().contains(mpos);
        chestBtn.setFillColor(isHovered ? sf::Color(80, 160, 220, 230) : sf::Color(60, 140, 200, 200));
        chestBtn.setOutlineThickness(3.f);
        chestBtn.setOutlineColor(isHovered ? sf::Color::White : sf::Color(255, 255, 255, 120));
        window.draw(chestBtn);

        sf::Text chestTxt(font, "Open Chest", 28);
        sf::FloatRect b = chestTxt.getLocalBounds();
        chestTxt.setPosition({ chestBtn.getPosition().x + (200.f - b.size.x) / 2.f, chestBtn.getPosition().y + (60.f - b.size.y) / 2.f - 8.f });
        window.draw(chestTxt);
    }
}

#include "Zombie.h"

void updateDayNightCycleAndMusic(
    sf::RenderWindow& window,
    long long serverEpochCache,
    float elapsedGameTime,
    float totalCycleTime,
    sf::RectangleShape& darkness,
    bool& isNight,
    bool& wasNight,
    std::vector<std::unique_ptr<Monster>>& monsters,
    sf::Music& bgMusic,
    sf::Music& nightMusic,
    bool& nightMusicLoaded,
    float volume,
    bool tutorialActive,
    std::mt19937& gen,
    std::uniform_real_distribution<float>& randX,
    std::uniform_real_distribution<float>& randY
) {
    float cycleValue = (serverEpochCache > 0)
        ? static_cast<float>(serverEpochCache % static_cast<long long>(totalCycleTime))
        : std::fmod(elapsedGameTime, totalCycleTime);
    float t = cycleValue / totalCycleTime;
    float timeOfDay = (t < 0.5f) ? (t * 2.f) : (1.f - (t - 0.5f) * 2.f);
    int darknessLevel = static_cast<int>(timeOfDay * 200.f);
    darkness.setFillColor(sf::Color(0, 0, 0, darknessLevel));
    window.draw(darkness);

    isNight = (darknessLevel > 150);

    if (isNight && !wasNight)
    {
        wasNight = true;
        monsters.clear();

        const int ZOMBIE_COUNT = 60;
        for (int i = 0; i < ZOMBIE_COUNT; ++i)
        {
            monsters.push_back(std::make_unique<Zombie>(
                sf::Vector2f(randX(gen), randY(gen))
            ));
        }

        bgMusic.stop();

        if (!nightMusicLoaded)
        {
            if (nightMusic.openFromFile("Music/Shirfine_-_Illusionary_Daytime_(mp3.pm).mp3"))
            {
                nightMusic.setLooping(true);
                nightMusic.setVolume(volume);
                nightMusicLoaded = true;
            }
        }

        nightMusic.play();
    }
    else if (!isNight && (wasNight || (!tutorialActive && !monsters.empty())))
    {
        wasNight = false;
        nightMusic.stop();
        bgMusic.play();
        monsters.clear();
    }
}


