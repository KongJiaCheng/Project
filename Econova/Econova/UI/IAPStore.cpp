#include "IAPStore.h"
#include "ViewUtils.h"
#include <algorithm>

std::vector<IAPCardData> getIAPCards() {
    return {
        { "Handful of Gold", "Great starter boost!", 100, 0.99f, sf::Color(205, 127, 50) },
        { "Sack of Gold", "Popular upgrades choice!", 1000, 9.99f, sf::Color(192, 192, 192) },
        { "Chest of Gold", "Ideal for core builders!", 5000, 39.99f, sf::Color(255, 215, 0) },
        { "Vault of Gold", "Tycoon class! Best value!", 10000, 79.99f, sf::Color(0, 242, 254) }
    };
}

void drawIAPStore(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    int currency
) {
    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));
    sf::RectangleShape sBox({ 900.f, 600.f });
    sf::Vector2f sBoxPos = { (width - 900.f) / 2.f, (height - 600.f) / 2.f };
    sBox.setPosition(sBoxPos);
    sBox.setFillColor(sf::Color(15, 20, 30, 248));
    sBox.setOutlineThickness(3.f);
    sBox.setOutlineColor(sf::Color(255, 215, 0, 180));
    window.draw(sBox);

    // Title
    sf::Text sTitle(uiFont, "IN-APP PURCHASE STORE", 28);
    sTitle.setStyle(sf::Text::Bold);
    sTitle.setFillColor(sf::Color(255, 215, 0));
    sTitle.setPosition({ sBoxPos.x + 40.f, sBoxPos.y + 20.f });
    window.draw(sTitle);

    // Subtitle
    sf::Text sSub(uiFont, "Get gold coins instantly to upgrade gear, craft items, and build structures!", 15);
    sSub.setFillColor(sf::Color(180, 180, 200));
    sSub.setPosition({ sBoxPos.x + 40.f, sBoxPos.y + 55.f });
    window.draw(sSub);

    // Gold Balance Display
    sf::RectangleShape balBox({ 300.f, 40.f });
    balBox.setPosition({ sBoxPos.x + 40.f, sBoxPos.y + 90.f });
    balBox.setFillColor(sf::Color(40, 45, 55, 200));
    balBox.setOutlineThickness(1.f);
    balBox.setOutlineColor(sf::Color(255, 215, 0, 80));
    window.draw(balBox);

    sf::Text balTxt(uiFont, "Your Gold Balance: " + std::to_string(currency) + " G", 16);
    balTxt.setStyle(sf::Text::Bold);
    balTxt.setFillColor(sf::Color(255, 225, 100));
    balTxt.setPosition({ balBox.getPosition().x + 15.f, balBox.getPosition().y + 8.f });
    window.draw(balTxt);

    // Close button (X)
    sf::RectangleShape closeBtn({ 40.f, 40.f });
    closeBtn.setPosition({ sBoxPos.x + 840.f, sBoxPos.y + 20.f });

    bool closeHover = closeBtn.getGlobalBounds().contains(mousePos);
    closeBtn.setFillColor(closeHover ? sf::Color(240, 70, 70) : sf::Color(200, 50, 50));
    closeBtn.setOutlineThickness(closeHover ? 2.f : 0.f);
    closeBtn.setOutlineColor(sf::Color::White);
    window.draw(closeBtn);

    sf::Text closeTxt(uiFont, "X", 22);
    closeTxt.setStyle(sf::Text::Bold);
    closeTxt.setPosition({ closeBtn.getPosition().x + 12.f, closeBtn.getPosition().y + 5.f });
    window.draw(closeTxt);

    // Cards
    float cardW = 190.f;
    float cardH = 360.f;
    float cardY = sBoxPos.y + 160.f;
    float cardGap = 20.f;
    float startX = sBoxPos.x + 45.f;

    auto cards = getIAPCards();

    for (int i = 0; i < 4; i++) {
        float cX = startX + i * (cardW + cardGap);
        sf::FloatRect cardRect({ cX, cardY }, { cardW, cardH });
        bool hovered = cardRect.contains(mousePos);

        sf::RectangleShape card({ cardW, cardH });
        card.setPosition({ cX, cardY });
        card.setFillColor(hovered ? sf::Color(30, 35, 50, 230) : sf::Color(20, 22, 30, 220));
        card.setOutlineThickness(hovered ? 3.f : 1.f);
        card.setOutlineColor(hovered ? cards[i].color : sf::Color(255, 255, 255, 30));
        window.draw(card);

        sf::Text cTitle(uiFont, cards[i].title, 16);
        cTitle.setStyle(sf::Text::Bold);
        cTitle.setFillColor(cards[i].color);
        cTitle.setPosition({ cX + 15.f, cardY + 20.f });
        window.draw(cTitle);

        sf::Text cCoins(uiFont, "G", 38);
        cCoins.setStyle(sf::Text::Bold);
        cCoins.setFillColor(cards[i].color);
        cCoins.setPosition({ cX + (cardW - cCoins.getGlobalBounds().size.x) / 2.f, cardY + 60.f });
        window.draw(cCoins);

        sf::Text cGold(uiFont, std::to_string(cards[i].gold) + " GOLD", 20);
        cGold.setStyle(sf::Text::Bold);
        cGold.setFillColor(sf::Color::White);
        cGold.setPosition({ cX + (cardW - cGold.getGlobalBounds().size.x) / 2.f, cardY + 130.f });
        window.draw(cGold);

        sf::Text cDesc(uiFont, cards[i].desc, 12);
        cDesc.setFillColor(sf::Color(150, 150, 160));
        cDesc.setPosition({ cX + (cardW - cDesc.getGlobalBounds().size.x) / 2.f, cardY + 175.f });
        window.draw(cDesc);

        sf::RectangleShape cDiv({ cardW - 40.f, 1.f });
        cDiv.setPosition({ cX + 20.f, cardY + 215.f });
        cDiv.setFillColor(sf::Color(255, 255, 255, 30));
        window.draw(cDiv);

        std::string priceStr = std::to_string(cards[i].price);
        priceStr = priceStr.substr(0, priceStr.find(".") + 3);
        sf::Text cPrice(uiFont, "$" + priceStr, 24);
        cPrice.setStyle(sf::Text::Bold);
        cPrice.setFillColor(sf::Color(100, 250, 150));
        cPrice.setPosition({ cX + (cardW - cPrice.getGlobalBounds().size.x) / 2.f, cardY + 230.f });
        window.draw(cPrice);

        sf::FloatRect btnRect({ cX + 15.f, cardY + cardH - 55.f }, { cardW - 30.f, 40.f });
        bool btnHovered = btnRect.contains(mousePos);

        sf::RectangleShape buyBtn({ cardW - 30.f, 40.f });
        buyBtn.setPosition({ cX + 15.f, cardY + cardH - 55.f });
        buyBtn.setFillColor(btnHovered ? sf::Color(40, 180, 120) : sf::Color(30, 130, 90));
        buyBtn.setOutlineThickness(btnHovered ? 1.f : 0.f);
        buyBtn.setOutlineColor(sf::Color::White);
        window.draw(buyBtn);

        sf::Text buyTxt(uiFont, "BUY NOW", 14);
        buyTxt.setStyle(sf::Text::Bold);
        buyTxt.setFillColor(sf::Color::White);
        buyTxt.setPosition({ buyBtn.getPosition().x + (buyBtn.getSize().x - buyTxt.getGlobalBounds().size.x) / 2.f, buyBtn.getPosition().y + 11.f });
        window.draw(buyTxt);
    }
}

void drawIAPVerifying(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::string& iapVerifyingTier,
    int iapVerifyingGold,
    float iapVerifyingPrice,
    TextInput& iapCodeInput
) {
    sf::RectangleShape vBox({ 500.f, 320.f });
    sf::Vector2f vBoxPos = { (width - 500.f) / 2.f, (height - 320.f) / 2.f };
    vBox.setPosition(vBoxPos);
    vBox.setFillColor(sf::Color(20, 25, 35, 250));
    vBox.setOutlineThickness(3.f);
    vBox.setOutlineColor(sf::Color(0, 242, 254, 180));
    window.draw(vBox);

    sf::Text vTitle(uiFont, "SECURE CHECKOUT", 22);
    vTitle.setStyle(sf::Text::Bold);
    vTitle.setFillColor(sf::Color(0, 242, 254));
    vTitle.setPosition({ vBoxPos.x + 30.f, vBoxPos.y + 25.f });
    window.draw(vTitle);

    sf::Text vLabel(uiFont, "A secure checkout page has been opened in your browser.\nPlease complete your payment of:", 13);
    vLabel.setFillColor(sf::Color(200, 200, 210));
    vLabel.setPosition({ vBoxPos.x + 30.f, vBoxPos.y + 65.f });
    window.draw(vLabel);

    std::string priceStr = std::to_string(iapVerifyingPrice);
    priceStr = priceStr.substr(0, priceStr.find(".") + 3);
    sf::Text vDetails(uiFont, iapVerifyingTier + " (" + std::to_string(iapVerifyingGold) + " Gold) for $" + priceStr + " USD", 15);
    vDetails.setStyle(sf::Text::Bold);
    vDetails.setFillColor(sf::Color(255, 215, 0));
    vDetails.setPosition({ vBoxPos.x + 30.f, vBoxPos.y + 110.f });
    window.draw(vDetails);

    sf::Text vCodeLbl(uiFont, "Paste your verification code below to sync your Gold balance:", 12);
    vCodeLbl.setFillColor(sf::Color(170, 170, 180));
    vCodeLbl.setPosition({ vBoxPos.x + 30.f, vBoxPos.y + 145.f });
    window.draw(vCodeLbl);

    iapCodeInput.draw(window);

    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), GetUIView(&window));

    sf::FloatRect verifyBtnRect({ vBoxPos.x + 50.f, vBoxPos.y + 240.f }, { 190.f, 45.f });
    bool verifyHover = verifyBtnRect.contains(mousePos);
    sf::RectangleShape verifyBtn({ 190.f, 45.f });
    verifyBtn.setPosition({ vBoxPos.x + 50.f, vBoxPos.y + 240.f });
    verifyBtn.setFillColor(verifyHover ? sf::Color(0, 200, 254) : sf::Color(0, 150, 200));
    window.draw(verifyBtn);

    sf::Text verifyTxt(uiFont, "Verify & Sync", 15);
    verifyTxt.setStyle(sf::Text::Bold);
    verifyTxt.setFillColor(sf::Color::White);
    verifyTxt.setPosition({ verifyBtn.getPosition().x + (verifyBtn.getSize().x - verifyTxt.getGlobalBounds().size.x) / 2.f, verifyBtn.getPosition().y + 12.f });
    window.draw(verifyTxt);

    sf::FloatRect cancelBtnRect({ vBoxPos.x + 260.f, vBoxPos.y + 240.f }, { 190.f, 45.f });
    bool cancelHover = cancelBtnRect.contains(mousePos);
    sf::RectangleShape cancelBtn({ 190.f, 45.f });
    cancelBtn.setPosition({ vBoxPos.x + 260.f, vBoxPos.y + 240.f });
    cancelBtn.setFillColor(cancelHover ? sf::Color(200, 60, 60) : sf::Color(150, 40, 40));
    window.draw(cancelBtn);

    sf::Text cancelTxt(uiFont, "Cancel", 15);
    cancelTxt.setStyle(sf::Text::Bold);
    cancelTxt.setFillColor(sf::Color::White);
    cancelTxt.setPosition({ cancelBtn.getPosition().x + (cancelBtn.getSize().x - cancelTxt.getGlobalBounds().size.x) / 2.f, cancelBtn.getPosition().y + 12.f });
    window.draw(cancelTxt);
}

#include "AuthManager.h"
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

bool handleIAPStoreClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& iapPageOpen,
    bool& iapVerifyingOpen,
    int& iapVerifyingGold,
    float& iapVerifyingPrice,
    std::string& iapVerifyingTier,
    TextInput& iapCodeInput,
    const std::string& username
) {
    sf::FloatRect sBoxRect({ (width - 900.f) / 2.f, (height - 600.f) / 2.f }, { 900.f, 600.f });
    sf::FloatRect closeBtnRect({ sBoxRect.position.x + 840.f, sBoxRect.position.y + 20.f }, { 40.f, 40.f });
    if (closeBtnRect.contains(uiPos)) {
        iapPageOpen = false;
        return true;
    }

    float cardW = 190.f;
    float cardH = 360.f;
    float cardY = sBoxRect.position.y + 160.f;
    float cardGap = 20.f;
    float startX = sBoxRect.position.x + 45.f;

    for (int i = 0; i < 4; i++) {
        sf::FloatRect buyBtnRect({ startX + i * (cardW + cardGap) + 15.f, cardY + cardH - 55.f }, { cardW - 30.f, 40.f });
        if (buyBtnRect.contains(uiPos)) {
            int gold = 100;
            float price = 0.99f;
            std::string tierName = "Handful of Gold";
            if (i == 1) { gold = 1000; price = 9.99f; tierName = "Sack of Gold"; }
            else if (i == 2) { gold = 5000; price = 39.99f; tierName = "Chest of Gold"; }
            else if (i == 3) { gold = 10000; price = 79.99f; tierName = "Vault of Gold"; }

            iapVerifyingOpen = true;
            iapVerifyingGold = gold;
            iapVerifyingPrice = price;
            iapVerifyingTier = tierName;
            iapCodeInput.clear();
            iapCodeInput.setFocus(true);

#ifdef _WIN32
            bool useRealStripeGateway = true;

            std::string priceStr = std::to_string(price);
            priceStr = priceStr.substr(0, priceStr.find(".") + 3);

            std::string urlTier = tierName;
            size_t spacePos = urlTier.find(" ");
            while (spacePos != std::string::npos) {
                urlTier.replace(spacePos, 1, "%20");
                spacePos = urlTier.find(" ");
            }

            if (useRealStripeGateway) {
                std::string url = "https://econova-qfhu.onrender.com/checkout?user=" + username + "&gold=" + std::to_string(gold) + "&price=" + priceStr + "&tier=" + urlTier;
                ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            else {
                ShellExecuteA(NULL, "open", "powershell.exe", "-WindowStyle Hidden -ExecutionPolicy Bypass -File ..\\payment_portal\\local_server.ps1", NULL, SW_HIDE);
                char fullPath[MAX_PATH] = { 0 };
                std::string url = "";
                if (_fullpath(fullPath, "payment_portal/index.html", MAX_PATH) != NULL) {
                    std::string absPath = fullPath;
                    std::replace(absPath.begin(), absPath.end(), '\\', '/');
                    url = absPath + "?user=" + username + "&gold=" + std::to_string(gold) + "&price=" + priceStr + "&tier=" + urlTier;
                }
                ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
#endif
            return true;
        }
    }
    return true;
}

bool handleIAPVerifyingClicks(
    sf::Vector2f uiPos,
    unsigned int width,
    unsigned int height,
    bool& iapVerifyingOpen,
    int iapVerifyingGold,
    const std::string& iapVerifyingTier,
    TextInput& iapCodeInput,
    const std::string& username,
    int& currency,
    std::function<void()> savePlayerStatsCallback
) {
    sf::FloatRect vBoxRect({ (width - 500.f) / 2.f, (height - 320.f) / 2.f }, { 500.f, 320.f });
    sf::FloatRect verifyBtnRect({ vBoxRect.position.x + 50.f, vBoxRect.position.y + 240.f }, { 190.f, 45.f });
    sf::FloatRect cancelBtnRect({ vBoxRect.position.x + 260.f, vBoxRect.position.y + 240.f }, { 190.f, 45.f });

    if (verifyBtnRect.contains(uiPos)) {
        std::string enteredCode = iapCodeInput.getText();
        bool success = false;

        std::string prefix = "GOLD-" + std::to_string(iapVerifyingGold);
        if (enteredCode.find(prefix) == 0) {
            success = true;
        }

        if (!success && fileExists("payment_success.json")) {
            std::ifstream file("payment_success.json");
            if (file.is_open()) {
                std::string line;
                std::string verifiedUser = "";
                int verifiedGold = 0;
                while (std::getline(file, line)) {
                    if (line.find("username") != std::string::npos) {
                        size_t start = line.find("\"", line.find(":"));
                        size_t end = line.find("\"", start + 1);
                        if (start != std::string::npos && end != std::string::npos) {
                            verifiedUser = line.substr(start + 1, end - start - 1);
                        }
                    }
                    if (line.find("gold") != std::string::npos) {
                        std::string goldStr = "";
                        for (char c : line) {
                            if (std::isdigit(c)) goldStr += c;
                        }
                        if (!goldStr.empty()) verifiedGold = std::stoi(goldStr);
                    }
                }
                file.close();
                if (verifiedUser == username && verifiedGold == iapVerifyingGold) {
                    success = true;
                    std::remove("payment_success.json");
                }
            }
        }

        if (success) {
            currency += iapVerifyingGold;
            savePlayerStatsCallback();
            std::cout << "[IAP SUCCESS] Added " << iapVerifyingGold << " gold to " << username << " for tier " << iapVerifyingTier << "!\n";
            iapVerifyingOpen = false;
        }
        else {
            std::cout << "[IAP FAILED] Invalid verification code provided.\n";
        }
        return true;
    }

    if (cancelBtnRect.contains(uiPos)) {
        iapVerifyingOpen = false;
        return true;
    }

    return false;
}
