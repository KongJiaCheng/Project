#include "WorldRenderer.h"
#include "ItemID.h"
#include "Settings.h"
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


// ── Upper floor grid overlays ─────────────────────────────────────────────────
// Draws a colored cell-outline grid for each floor level above ground.
// Floor 2 = green, Floor 3 = blue, Floor 4+ = purple.
// Each grid sits 180 units above the floor below it.
// playerFloorLevel: the floor the player is currently standing on (0=ground).
void drawSecondFloorGrid(
    sf::RenderWindow& window,
    const std::vector<PlacedStructure>& structures,
    int playerFloorLevel
) {
    // Only active when at least one staircase has been built
    bool hasStairs = false;
    for (const auto& s : structures)
        if (s.id == ITEM_WOOD_STAIRS) { hasStairs = true; break; }
    if (!hasStairs) return;

    constexpr float CELL = 500.f, SX = 250.f, SY = 250.f;
    constexpr float FLOOR_STEP = 180.f;   // visual Y lift per floor

    // Per-floor color palette.  Index 0 = floor 2 (targetFloor=1), 1 = floor 3, 2 = floor 4+
    struct FloorColors { sf::Color outline, hover, filled; };
    const FloorColors palette[] = {
        // Floor 2 — green
        { sf::Color( 40, 220,  80, 170),   // outline
          sf::Color( 60, 255, 100, 235),   // hover
          sf::Color( 40, 200,  80,  70) }, // already-placed tint
        // Floor 3 — blue
        { sf::Color( 40, 130, 255, 170),
          sf::Color(100, 180, 255, 235),
          sf::Color( 40, 100, 200,  70) },
        // Floor 4+ — purple
        { sf::Color(160,  50, 255, 170),
          sf::Color(200, 110, 255, 235),
          sf::Color(130,  40, 200,  70) },
    };

    // Draw a grid for every floor that has source tiles (i.e. the floor below it exists)
    // targetFloor 1 = 2nd floor grid, 2 = 3rd floor grid, etc.
    for (int targetFloor = 1; targetFloor <= 3; ++targetFloor) {
        float sourceShearY  = static_cast<float>(targetFloor - 1);   // shearY of the floor below
        float targetShearY  = static_cast<float>(targetFloor);
        float elevOffset    = targetFloor * FLOOR_STEP;

        // Collect source tile positions (the floor directly below the one we're drawing)
        std::vector<sf::Vector2f> sourceTiles;
        for (const auto& s : structures) {
            if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
                float sy = s.shearY;
                bool isSource = (targetFloor == 1)
                    ? (sy < 1.f)                          // ground tiles feed floor 2
                    : (sy >= targetShearY - 1.f && sy < targetShearY); // upper floors
                if (isSource)
                    sourceTiles.push_back(s.sprite->getPosition());
            }
        }
        if (sourceTiles.empty()) break;   // no source tiles → no higher floors possible

        // For floor 3+ we also need the floor below to have tiles, checked above.
        // For floor 4+ we cap at palette[2].
        const FloorColors& col = palette[std::min(targetFloor - 1, 2)];

        // Pre-collect placed tile positions on this target floor to avoid O(N^2) loop
        std::vector<sf::Vector2f> placedOnTargetFloor;
        for (const auto& s : structures) {
            if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
                if (s.shearY >= targetShearY - 0.5f && s.shearY <= targetShearY + 0.5f) {
                    placedOnTargetFloor.push_back(s.sprite->getPosition());
                }
            }
        }

        auto hasFloorTileAt = [&](sf::Vector2f p) -> bool {
            for (const auto& sp : placedOnTargetFloor) {
                if (std::abs(sp.x - p.x) < 1.f && std::abs(sp.y - p.y) < 1.f) return true;
            }
            return false;
        };

        // Mouse hover cell — adjust for this floor's visual elevation
        sf::Vector2i pixelMouse = sf::Mouse::getPosition(window);
        sf::Vector2f worldM = window.mapPixelToCoords(pixelMouse);
        worldM.y += elevOffset;   // compensate for upward shift
        int hGy = static_cast<int>(std::round(-worldM.y / SY));
        int hGx = static_cast<int>(std::round((worldM.x - hGy * SX) / CELL));
        sf::Vector2f hoverBase(hGx * CELL + hGy * SX, -hGy * SY);

        sf::VertexArray lines(sf::PrimitiveType::Lines);

        for (const sf::Vector2f& tilePos : sourceTiles) {
            sf::Vector2f pos(tilePos.x, tilePos.y - elevOffset);

            bool isHover  = (std::abs(tilePos.x - hoverBase.x) < 1.f &&
                             std::abs(tilePos.y - hoverBase.y) < 1.f);
            bool isFilled = hasFloorTileAt(tilePos);

            sf::Color lineCol = isFilled ? col.filled : (isHover ? col.hover : col.outline);

            sf::Vector2f BL{ pos.x,            pos.y      };
            sf::Vector2f BR{ pos.x + CELL,      pos.y      };
            sf::Vector2f TR{ pos.x + CELL + SX, pos.y - SY };
            sf::Vector2f TL{ pos.x + SX,        pos.y - SY };

            lines.append(sf::Vertex{BL, lineCol}); lines.append(sf::Vertex{BR, lineCol});
            lines.append(sf::Vertex{BR, lineCol}); lines.append(sf::Vertex{TR, lineCol});
            lines.append(sf::Vertex{TR, lineCol}); lines.append(sf::Vertex{TL, lineCol});
            lines.append(sf::Vertex{TL, lineCol}); lines.append(sf::Vertex{BL, lineCol});
        }

        window.draw(lines);
    }
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
    sf::Vector2f pPos = playerSprite.getPosition();
    for (const auto& pc : campfires) {
        float dx = pc.position.x - pPos.x;
        float dy = pc.position.y - pPos.y;
        float distSq = dx * dx + dy * dy;
        if (distSq >= 3000000.f) continue;

        sf::Sprite cs(campfireFrames[pc.currentFrame]);
        sf::Vector2u cfs = cs.getTexture().getSize();
        cs.setOrigin({ cfs.x / 2.f, (float)cfs.y });
        cs.setScale({ 0.9f, 0.9f });
        cs.setPosition(pc.position);
        if (!pc.isCrafted) {
            cs.setColor(sf::Color(255, 255, 255, 128));
        } else if (pc.isHurt) {
            cs.setColor(sf::Color(255, 100, 100));
        }
        window.draw(cs);

        if (!pc.isCrafted) {
            sf::Text t(font, "Crafting: " + std::to_string((int)pc.timeRemaining) + "s", 16);
            t.setFillColor(sf::Color::White);
            t.setPosition({ pc.position.x - 40.f, pc.position.y - 40.f });
            window.draw(t);
        }

        // Show wrench icon above campfire only when player is nearby
        if (distSq < 280.f * 280.f) {
            sf::Sprite ws(wrenchTex);
            sf::Vector2u wts = wrenchTex.getSize();
            ws.setOrigin({ wts.x / 2.f, wts.y / 2.f });
            ws.setScale({ 0.05f, 0.05f });
            ws.setPosition({ pc.position.x, pc.position.y - 75.f });
            window.draw(ws);
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

void drawDoor(
    sf::RenderWindow& window,
    const sf::Texture& tex,
    sf::Vector2f pos,
    float rotation,
    bool isOpen,
    sf::Color color
) {
    constexpr float CELL = 500.f, SX = 250.f, SY = 250.f;
    float doorHeight = isOpen ? 50.f : 500.f;

    auto sz = tex.getSize();
    float tw = static_cast<float>(sz.x);
    float th = static_cast<float>(sz.y);

    sf::Vector2f BL, BR, TR, TL;
    if (rotation == 90.f) {
        // Sheared grid position: along e2 = (SX, -SY)
        BL = pos;
        BR = { pos.x + SX, pos.y - SY };
        TR = { pos.x + SX, pos.y - SY - doorHeight };
        TL = { pos.x,      pos.y - doorHeight };
    } else {
        // Horizontal grid position: along e1 = (CELL, 0)
        BL = pos;
        BR = { pos.x + CELL, pos.y };
        TR = { pos.x + CELL, pos.y - doorHeight };
        TL = { pos.x,        pos.y - doorHeight };
    }

    float uLeft = 0.f, uRight = tw;
    float vTop = 0.f, vBottom = th;
    if (tw == 1024.f && th == 1024.f) {
        uLeft = 224.f;
        uRight = 801.f;
        vTop = 55.f;
        vBottom = 968.f;
    }

    sf::VertexArray va(sf::PrimitiveType::TriangleFan, 4);
    va[0] = sf::Vertex{ BL, color, sf::Vector2f{uLeft,  vBottom} };
    va[1] = sf::Vertex{ BR, color, sf::Vector2f{uRight, vBottom} };
    va[2] = sf::Vertex{ TR, color, sf::Vector2f{uRight, vTop} };
    va[3] = sf::Vertex{ TL, color, sf::Vector2f{uLeft,  vTop} };

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
    const sf::Texture& chestTex,
    const std::vector<PlacedStructure>& structures
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
    // Wall, door, and stairs anchor to BL corner — use bottom-left origin to match placed sprite
    if (placementItemID == ITEM_WOOD_WALL || placementItemID == ITEM_WOOD_DOOR || placementItemID == ITEM_WOOD_STAIRS) {
        preview.setOrigin({ 0.f, (float)pts.y });
    } else {
        preview.setOrigin({ pts.x / 2.f, (float)pts.y });
    }

    float previewShear = 0.f;
    applyStructureScale(preview, placementItemID, placementRotation, false, previewShear);

    bool hasTileUnder = false;
    bool hasStairsBuilt = false;
    bool hasGroundTileAtPos = false;
    bool hasSecondFloorTileAtPos = false;

    for (const auto& s : structures) {
        if (s.id == ITEM_WOOD_STAIRS) {
            hasStairsBuilt = true;
        }
        if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
            if (std::abs(s.sprite->getPosition().x - placementPos.x) < 1.f &&
                std::abs(s.sprite->getPosition().y - placementPos.y) < 1.f) {
                if (s.shearY >= 1.f) {
                    hasSecondFloorTileAtPos = true;
                } else {
                    hasGroundTileAtPos = true;
                    hasTileUnder = true;
                }
            }
        }
    }

    if (placementItemID == ITEM_WOOD_STAIRS) {
        if (!hasTileUnder) {
            preview.setColor(sf::Color(255, 100, 100, 180)); // Red tint: requires wood tile under!
        } else {
            preview.setColor(sf::Color(255, 255, 255, 180));
        }
    } else {
        preview.setColor(sf::Color(255, 255, 255, 180));
    }
    preview.setPosition(placementPos);

    if (placementItemID == ITEM_WOOD_TILE) {
        if (hasStairsBuilt && hasGroundTileAtPos) {
            if (hasSecondFloorTileAtPos) {
                drawRhombusTile(window, preview.getTexture(), { placementPos.x, placementPos.y - 180.f },
                                sf::Color(255, 100, 100, 160));
            } else {
                drawRhombusTile(window, preview.getTexture(), { placementPos.x, placementPos.y - 180.f },
                                sf::Color(180, 220, 255, 180));
            }
        } else {
            drawRhombusTile(window, preview.getTexture(), placementPos,
                            sf::Color(255, 255, 255, 160));
        }
    } else if (placementItemID == ITEM_WOOD_WALL) {
        drawWall(window, preview.getTexture(), placementPos, placementRotation,
                 sf::Color(255, 255, 255, 180));
    } else if (placementItemID == ITEM_WOOD_DOOR) {
        drawDoor(window, preview.getTexture(), placementPos, placementRotation, false,
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
    std::function<sf::Vector2f(sf::Vector2f)> snapGridFunc,
    float& outStairOffset
) {
    bool isUnderRoof = false;
    float stairOffset = 0.f;
    sf::Vector2f pPos = sprite.getPosition();
    sf::Vector2f pFeetGlobal = { pPos.x, pPos.y + 250.f };

    auto isPointInRhombus = [](sf::Vector2f pt, sf::Vector2f pos, float margin = 40.f) -> bool {
        if (pt.y < pos.y - 250.f - margin || pt.y > pos.y + margin) return false;
        float xShift = pos.y - pt.y;
        return (pt.x >= pos.x + xShift - margin && pt.x <= pos.x + 500.f + xShift + margin);
    };

    playerFloorLevel = 0;
    bool playerOnStairs = false;

    for (auto& etf : elevatedTileFloors) {
        if (isPointInRhombus(pFeetGlobal, etf.pos)) {
            if (etf.floorLevel > playerFloorLevel) {
                playerFloorLevel = etf.floorLevel;
            }
        }
    }

    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_STAIRS && s.sprite.has_value()) {
            // Stair grid position (BL corner of the 500×500 rhombus cell)
            sf::Vector2f sPos = s.sprite->getPosition();

            // Stair zone: a wide rhombus area the player walks through.
            // The stair occupies the rhombus cell at sPos.
            // We detect the player being in this cell via Y-range + X-range check.
            // sPos.y is the bottom of the rhombus; the cell goes from sPos.y-250 (top) to sPos.y (bottom).
            // We extend generously: 100 unit margin so the player can enter from ground level.
            const float margin = 150.f;
            float stairZoneTop    = sPos.y - 250.f - margin;
            float stairZoneBottom = sPos.y + margin;
            float stairZoneLeft   = sPos.x - margin;
            float stairZoneRight  = sPos.x + 500.f + margin;

            if (pPos.y >= stairZoneTop && pPos.y <= stairZoneBottom &&
                pPos.x >= stairZoneLeft && pPos.x <= stairZoneRight)
            {
                playerOnStairs = true;
                float progress = 0.f;
                if (s.rotation == 90.f) {
                    // Diagonal stair: progress increases left→right
                    progress = (pPos.x - sPos.x) / 500.f;
                } else if (s.rotation == 180.f) {
                    // Reversed diagonal
                    progress = 1.0f - (pPos.x - sPos.x) / 500.f;
                } else {
                    // Default: player walks upward (decreasing Y) through the stair zone.
                    // At bottom of zone (sPos.y): progress=0 (ground). At top (sPos.y-250): progress=1 (top floor).
                    progress = 1.0f - (pPos.y - (sPos.y - 250.f)) / 250.f;
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
            if (isPointInRhombus(pPos, etf.pos, 0.f)) {
                isUnderElevatedFloor = true;
                break;
            }
        }
    }

    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_ROOF || s.id == ITEM_WOOD_WALL) {
            if (!s.sprite.has_value()) continue;
            float dx = s.sprite->getPosition().x - pPos.x;
            float dy = s.sprite->getPosition().y - pPos.y;
            if (dx * dx + dy * dy > 800.f * 800.f) continue;

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
            if (!s.sprite.has_value()) continue;
            float dx = s.sprite->getPosition().x - pPos.x;
            float dy = s.sprite->getPosition().y - pPos.y;
            if (dx * dx + dy * dy > 2000.f * 2000.f) continue;

            if (s.rotation == 0.f && (s.id == ITEM_WOOD_WALL || s.id == ITEM_WOOD_DOOR)) {
                bool isBackWall = false;
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

    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
            if (isUnderElevatedFloor && s.shearY >= 1.f) {
                s.sprite->setColor(sf::Color(255, 255, 255, 128));
            }
            else {
                s.sprite->setColor(sf::Color::White);
            }
        }
    }

    // Export stairOffset so Game.cpp can offset the movement collision probe
    outStairOffset = stairOffset;

    // 1. Draw ground-floor tiles first (as rhombus quads)
    for (auto& s : structures) {
        if (s.id == ITEM_WOOD_TILE && s.sprite.has_value()) {
            if (s.shearY >= 1.f) continue;
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
        int sFloor = (s.shearY >= 1.f) ? (int)s.shearY : getFloorLevelFromY(sPos_y);
        float sGroundY = sPos_y + sFloor * 180.f;
        float sZ = sFloor * 180.f;

        if (s.id == ITEM_WOOD_TILE) {
            if (s.shearY >= 1.f) {
                sf::Vector2f elevatedPos = { s.sprite->getPosition().x, s.sprite->getPosition().y - 180.f * s.shearY };
                depthDrawables.push_back({ sGroundY, sZ, 0, [structPtr = &s, elevatedPos, &window]() {
                    if (structPtr->sprite.has_value())
                        drawRhombusTile(window, structPtr->sprite->getTexture(),
                                        elevatedPos, structPtr->sprite->getColor());
                } });
            }
        }
        else if (s.id == ITEM_WOOD_WALL) {
            depthDrawables.push_back({ sGroundY, sZ, 1, [structPtr = &s, &window]() {
                if (structPtr->sprite.has_value())
                    drawWall(window, structPtr->sprite->getTexture(),
                             structPtr->sprite->getPosition(), structPtr->rotation, structPtr->sprite->getColor());
            } });
        }
        else if (s.id == ITEM_WOOD_DOOR) {
            depthDrawables.push_back({ sGroundY, sZ, 1, [structPtr = &s, &window]() {
                if (structPtr->sprite.has_value())
                    drawDoor(window, structPtr->sprite->getTexture(),
                             structPtr->sprite->getPosition(), structPtr->rotation, structPtr->isOpen, structPtr->sprite->getColor());
            } });
        }
        else {
            int priority = (s.id == ITEM_WOOD_ROOF) ? 2 : 1;
            depthDrawables.push_back({ sGroundY, sZ, priority, [structPtr = &s, &window]() {
                if (structPtr->sprite.has_value()) window.draw(*structPtr->sprite);
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
                depthDrawables.push_back({ a->sprite.getPosition().y, 0.f, 1, [animalPtr = a.get(), &window]() {
                    window.draw(animalPtr->sprite);
                    if (animalPtr->health < animalPtr->maxHealth) {
                        sf::FloatRect bounds = animalPtr->sprite.getGlobalBounds();
                        sf::RectangleShape hpBg({ 40.f, 6.f });
                        hpBg.setFillColor(sf::Color::Red);
                        hpBg.setPosition({ bounds.position.x + bounds.size.x / 2.f - 20.f, bounds.position.y - 15.f });

                        sf::RectangleShape hpFg({ 40.f * (std::max(0.f, animalPtr->health) / animalPtr->maxHealth), 6.f });
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
                depthDrawables.push_back({ m->sprite.getPosition().y, 0.f, 1, [monsterPtr = m.get(), &window]() {
                    window.draw(monsterPtr->sprite);
                    if (monsterPtr->health < monsterPtr->maxHealth) {
                        sf::FloatRect bounds = monsterPtr->sprite.getGlobalBounds();
                        sf::RectangleShape hpBg({ 40.f, 6.f });
                        hpBg.setFillColor(sf::Color::Red);
                        hpBg.setPosition({ bounds.position.x + bounds.size.x / 2.f - 20.f, bounds.position.y - 15.f });

                        sf::RectangleShape hpFg({ 40.f * (std::max(0.f, monsterPtr->health) / monsterPtr->maxHealth), 6.f });
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
    sf::Text& drinkText,
    const sf::Texture& wrenchTex,
    Inventory& inventory
) {
    int selSlot = inventory.getSelectedSlot();
    bool hasWrench = (selSlot != -1 && inventory.hasItem(selSlot) && inventory.getItemID(selSlot) == ITEM_WRENCH);

    if (nearPond) {
        drinkText.setPosition({
            sprite.getPosition().x - 160.f,
            sprite.getPosition().y - 100.f
        });
        window.draw(drinkText);
    }

    for (const auto& s : structures) {
        if ((s.id == ITEM_BED || s.id == ITEM_FURNACE) && s.sprite.has_value()) {
            sf::Vector2f pos = s.sprite->getPosition();
            float dx = pos.x - sprite.getPosition().x;
            float dy = pos.y - sprite.getPosition().y;
            float distSq = dx * dx + dy * dy;
            if (distSq < 380.f * 380.f) {
                sf::Sprite ws(wrenchTex);
                sf::Vector2u wts = wrenchTex.getSize();
                ws.setOrigin({ wts.x / 2.f, wts.y / 2.f });
                ws.setScale({ 0.05f, 0.05f });
                if (s.id == ITEM_FURNACE) {
                    ws.setPosition({ pos.x, pos.y - 150.f });
                } else {
                    ws.setPosition({ pos.x, pos.y - 80.f });
                }
                window.draw(ws);
            }
        }
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
        float doorCenterX = nearestDoor->sprite->getPosition().x + (nearestDoor->rotation == 90.f ? 125.f : 250.f);
        float doorCenterY = nearestDoor->sprite->getPosition().y - (nearestDoor->rotation == 90.f ? 125.f : 0.f);
        doorBtn.setPosition({ doorCenterX - 100.f, doorCenterY - 120.f });

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
    long long sleepTimeOffset,
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
        ? static_cast<float>((serverEpochCache + sleepTimeOffset) % static_cast<long long>(totalCycleTime))
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

        // Night monsters are dynamically spawned within camera bounds (max 6, spaced apart)
        // by spawnNightZombies() to prevent clustering and lag.

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

        nightMusic.setVolume(volume);
        currentActiveMusic = &nightMusic;
        nightMusic.play();
    }
    else if (!isNight && (wasNight || (!tutorialActive && !monsters.empty()) || nightMusic.getStatus() == sf::Music::Status::Playing))
    {
        wasNight = false;
        if (nightMusic.getStatus() == sf::Music::Status::Playing)
        {
            nightMusic.stop();
        }
        bgMusic.setVolume(volume);
        currentActiveMusic = &bgMusic;
        if (bgMusic.getStatus() != sf::Music::Status::Playing)
        {
            bgMusic.play();
        }
        monsters.clear();
    }
}


