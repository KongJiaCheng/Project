#include "ChatSystem.h"
#include "ViewUtils.h"
#include <algorithm>

void drawChatBox(
    sf::RenderWindow& window,
    const sf::Font& font,
    float chatBoxX,
    float chatBoxY,
    float chatBoxWidth,
    float chatBoxHeight,
    float chatScroll,
    const std::vector<std::pair<std::string, std::string>>& chatMessages,
    bool isChatting,
    TextInput& chatInput
) {
    // Draw chat background
    sf::RectangleShape chatBg({ chatBoxWidth, chatBoxHeight });
    chatBg.setPosition({ chatBoxX, chatBoxY });
    chatBg.setFillColor(sf::Color(0, 0, 0, 150));
    chatBg.setOutlineThickness(2.f);
    chatBg.setOutlineColor(sf::Color::White);
    window.draw(chatBg);

    // Draw chat messages with scroll and clipping
    float msgMinY = chatBoxY + 10.f;
    float msgMaxY = chatBoxY + chatBoxHeight - 60.f;
    float chatY = msgMinY - chatScroll;

    for (auto it = chatMessages.rbegin(); it != chatMessages.rend(); ++it) {
        sf::Text msgText(font, it->first + ": " + it->second, 24);
        float msgHeight = msgText.getLocalBounds().size.y + 10.f;

        // Only draw if within bounds (simple clipping)
        if (chatY + msgHeight > msgMinY && chatY < msgMaxY) {
            msgText.setFillColor(sf::Color::White);
            msgText.setOutlineColor(sf::Color::Black);
            msgText.setOutlineThickness(1.5f);
            msgText.setPosition({ chatBoxX + 15.f, chatY });
            window.draw(msgText);
        }
        chatY += msgHeight;
    }

    if (isChatting) {
        chatInput.draw(window);
    }
}

void drawWrenchMenu(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    const sf::Vector2f& wrenchMenuPos,
    bool isTargetNPC,
    const std::string& wrenchTargetPlayer
) {
    sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    if (isTargetNPC) {
        // NPC -> TRADE, QUEST, MARKET STATS, IN-APP PURCHASE
        sf::RectangleShape menuBox({ 160.f, 120.f });
        menuBox.setPosition(wrenchMenuPos);
        menuBox.setFillColor(sf::Color(30, 30, 35, 245));
        menuBox.setOutlineColor(sf::Color(220, 220, 220));
        menuBox.setOutlineThickness(1.5f);
        window.draw(menuBox);

        const char* labels[4] = { "1. Trade", "2. Daily Quest", "3. Market Stats", "4. Buy Gold" };
        for (int i = 0; i < 4; i++) {
            sf::FloatRect rowRect({ wrenchMenuPos.x, wrenchMenuPos.y + i * 30.f }, { 160.f, 30.f });
            if (rowRect.contains(mpos)) {
                sf::RectangleShape hoverBg({ 160.f, 30.f });
                hoverBg.setPosition(rowRect.position);
                hoverBg.setFillColor(sf::Color(52, 152, 219, 180));
                window.draw(hoverBg);
            }
            sf::Text txt(uiFont, labels[i], 18);
            txt.setFillColor(sf::Color::White);
            txt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + i * 30.f + 4.f });
            window.draw(txt);
        }
    }
    else if (!wrenchTargetPlayer.empty()) {
        // Online Player -> TRADE + CRAFT
        sf::RectangleShape menuBox({ 160.f, 60.f });
        menuBox.setPosition(wrenchMenuPos);
        menuBox.setFillColor(sf::Color(30, 30, 35, 245));
        menuBox.setOutlineColor(sf::Color(220, 220, 220));
        menuBox.setOutlineThickness(1.5f);
        window.draw(menuBox);

        sf::FloatRect r1({ wrenchMenuPos.x, wrenchMenuPos.y }, { 160.f, 30.f });
        if (r1.contains(mpos)) {
            sf::RectangleShape hoverBg({ 160.f, 30.f });
            hoverBg.setPosition(r1.position);
            hoverBg.setFillColor(sf::Color(52, 152, 219, 180));
            window.draw(hoverBg);
        }
        sf::Text trdTxt(uiFont, "Trade: " + wrenchTargetPlayer, 15);
        trdTxt.setFillColor(sf::Color(100, 220, 255));
        trdTxt.setPosition({ wrenchMenuPos.x + 8.f, wrenchMenuPos.y + 6.f });
        window.draw(trdTxt);

        sf::FloatRect r2({ wrenchMenuPos.x, wrenchMenuPos.y + 30.f }, { 160.f, 30.f });
        if (r2.contains(mpos)) {
            sf::RectangleShape hoverBg({ 160.f, 30.f });
            hoverBg.setPosition(r2.position);
            hoverBg.setFillColor(sf::Color(52, 152, 219, 180));
            window.draw(hoverBg);
        }
        sf::Text craftTxt2(uiFont, "Craft Items", 18);
        craftTxt2.setFillColor(sf::Color::White);
        craftTxt2.setPosition({ wrenchMenuPos.x + 8.f, wrenchMenuPos.y + 34.f });
        window.draw(craftTxt2);
    }
    else {
        // Own wrench -> Craft only
        sf::RectangleShape menuBox({ 160.f, 34.f });
        menuBox.setPosition(wrenchMenuPos);
        menuBox.setFillColor(sf::Color(30, 30, 35, 245));
        menuBox.setOutlineColor(sf::Color(220, 220, 220));
        menuBox.setOutlineThickness(1.5f);
        window.draw(menuBox);

        sf::FloatRect r1({ wrenchMenuPos.x, wrenchMenuPos.y }, { 160.f, 34.f });
        if (r1.contains(mpos)) {
            sf::RectangleShape hoverBg({ 160.f, 34.f });
            hoverBg.setPosition(r1.position);
            hoverBg.setFillColor(sf::Color(52, 152, 219, 180));
            window.draw(hoverBg);
        }
        sf::Text craftTxt(uiFont, "Craft Items", 18);
        craftTxt.setFillColor(sf::Color::White);
        craftTxt.setPosition({ wrenchMenuPos.x + 10.f, wrenchMenuPos.y + 6.f });
        window.draw(craftTxt);
    }
}

void drawP2PInvitePopup(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::string& initiator
) {
    float pw = 400.f, ph = 160.f;
    float px2 = (width - pw) / 2.f, py2 = (height - ph) / 2.f;
    sf::RectangleShape popupBox({ pw, ph });
    popupBox.setPosition({ px2, py2 });
    popupBox.setFillColor(sf::Color(22, 22, 30, 250));
    popupBox.setOutlineThickness(2.f);
    popupBox.setOutlineColor(sf::Color(100, 200, 255));
    window.draw(popupBox);
    sf::Text pt1(uiFont, "Trade Request!", 22);
    pt1.setStyle(sf::Text::Bold);
    pt1.setFillColor(sf::Color(100, 220, 255));
    pt1.setPosition({ px2 + 20.f, py2 + 14.f });
    window.draw(pt1);
    sf::Text pt2(uiFont, initiator + " wants to trade with you.", 16);
    pt2.setFillColor(sf::Color::White);
    pt2.setPosition({ px2 + 20.f, py2 + 50.f });
    window.draw(pt2);
    sf::RectangleShape acceptBtn({ 120.f, 40.f });
    acceptBtn.setPosition({ px2 + 40.f, py2 + 100.f });
    acceptBtn.setFillColor(sf::Color(40, 160, 70));
    window.draw(acceptBtn);
    sf::Text at(uiFont, "Accept", 16);
    at.setPosition({ px2 + 58.f, py2 + 110.f });
    window.draw(at);
    sf::RectangleShape declineBtn({ 120.f, 40.f });
    declineBtn.setPosition({ px2 + 240.f, py2 + 100.f });
    declineBtn.setFillColor(sf::Color(180, 50, 50));
    window.draw(declineBtn);
    sf::Text dt2(uiFont, "Decline", 16);
    dt2.setPosition({ px2 + 256.f, py2 + 110.f });
    window.draw(dt2);
}
