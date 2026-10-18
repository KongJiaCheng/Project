#include "HUD.h"
#include "ViewUtils.h"
#include <algorithm>
#include <thread>

HUDVitalsUI::HUDVitalsUI(const sf::Font& font, const sf::Texture& hpIconTex, const sf::Texture& hungerIconTex, const sf::Texture& thirstIconTex, float BAR_WIDTH, float BAR_HEIGHT)
    : hpIcon(hpIconTex), hungerIcon(hungerIconTex), thirstIcon(thirstIconTex),
      hpText(font, "", 16), hungerText(font, "", 16), thirstText(font, "", 16),
      hungerMultiplierText(font, "", 22), thirstMultiplierText(font, "", 22),
      defText(font, "", 16), levelText(font, "", 20),
      drinkText(font, "Press E to drink water", 28),
      hpBack({ BAR_WIDTH, BAR_HEIGHT }), hpBar({ BAR_WIDTH, BAR_HEIGHT }),
      hungerBack({ BAR_WIDTH, BAR_HEIGHT }), hungerBar({ BAR_WIDTH, BAR_HEIGHT }),
      thirstBack({ BAR_WIDTH, BAR_HEIGHT }), thirstBar({ BAR_WIDTH, BAR_HEIGHT }),
      defBack({ BAR_WIDTH, BAR_HEIGHT }), defBar({ BAR_WIDTH, BAR_HEIGHT }),
      expBack({ BAR_WIDTH, BAR_HEIGHT }), expBar({ BAR_WIDTH, BAR_HEIGHT })
{
    hpIcon.setScale({ 0.6f, 0.6f });
    hungerIcon.setScale({ 0.6f, 0.6f });
    thirstIcon.setScale({ 0.6f, 0.6f });

    hungerMultiplierText.setFillColor(sf::Color::Yellow);
    thirstMultiplierText.setFillColor(sf::Color::Cyan);
    hungerMultiplierText.setOutlineThickness(2.f);
    thirstMultiplierText.setOutlineThickness(2.f);
    hungerMultiplierText.setOutlineColor(sf::Color::Black);
    thirstMultiplierText.setOutlineColor(sf::Color::Black);

    drinkText.setFillColor(sf::Color::Yellow);

    hpText.setFillColor(sf::Color::Black);
    hungerText.setFillColor(sf::Color::Black);
    thirstText.setFillColor(sf::Color::Black);
    defText.setFillColor(sf::Color::Black);

    levelText.setFillColor(sf::Color::White);
    levelText.setOutlineColor(sf::Color::Black);
    levelText.setOutlineThickness(1.f);

    hpBack.setFillColor(sf::Color(60, 60, 60));
    hpBar.setFillColor(sf::Color::Red);

    hungerBack.setFillColor(sf::Color(60, 60, 60));
    hungerBar.setFillColor(sf::Color(255, 165, 0));

    thirstBack.setFillColor(sf::Color(60, 60, 60));
    thirstBar.setFillColor(sf::Color(0, 150, 255));

    defBack.setFillColor(sf::Color(60, 60, 60));
    defBar.setFillColor(sf::Color(100, 100, 255));

    expBack.setFillColor(sf::Color(60, 60, 60));
    expBar.setFillColor(sf::Color::Green);
}

void HUDVitalsUI::update(
    sf::RenderWindow& window,
    Inventory& inventory,
    bool shieldEquipped,
    int eqShieldSlot,
    float hp, float maxHP,
    float hunger, float maxHunger, int hungerMultiplier,
    float thirst, float maxThirst, int thirstMultiplier,
    int level, int exp
) {
    updateHUDVitalsLayoutAndTexts(
        window, inventory, shieldEquipped, eqShieldSlot,
        hp, maxHP, hunger, maxHunger, hungerMultiplier,
        thirst, maxThirst, thirstMultiplier, level, exp,
        hpBack, hpBar, hpIcon, hpText,
        hungerBack, hungerBar, hungerIcon, hungerText,
        thirstBack, thirstBar, thirstIcon, thirstText,
        hungerMultiplierText, thirstMultiplierText,
        expBack, expBar, levelText,
        defBack, defBar, defText
    );
}

void HUDVitalsUI::draw(
    sf::RenderWindow& window,
    bool inventoryOpen,
    bool shieldEquipped,
    Inventory& inventory
) const {
    drawHUDVitals(
        window, inventoryOpen,
        hpBack, hpBar, hpIcon, hpText,
        hungerBack, hungerBar, hungerIcon, hungerText,
        thirstBack, thirstBar, thirstIcon, thirstText,
        hungerMultiplierText, thirstMultiplierText,
        expBack, expBar, levelText,
        shieldEquipped, defBack, defBar, defText,
        inventory
    );
}


void drawHUDVitals(
    sf::RenderWindow& window,
    bool inventoryOpen,
    const sf::RectangleShape& hpBack, const sf::RectangleShape& hpBar, const sf::Sprite& hpIcon, const sf::Text& hpText,
    const sf::RectangleShape& hungerBack, const sf::RectangleShape& hungerBar, const sf::Sprite& hungerIcon, const sf::Text& hungerText,
    const sf::RectangleShape& thirstBack, const sf::RectangleShape& thirstBar, const sf::Sprite& thirstIcon, const sf::Text& thirstText,
    const sf::Text& hungerMultiplierText, const sf::Text& thirstMultiplierText,
    const sf::RectangleShape& expBack, const sf::RectangleShape& expBar, const sf::Text& levelText,
    bool shieldEquipped, const sf::RectangleShape& defBack, const sf::RectangleShape& defBar, const sf::Text& defText,
    Inventory& inventory
) {
    if (inventoryOpen) {
        window.setView(GetUIView(&window));

        window.draw(hpBack);
        window.draw(hpBar);
        window.draw(hpIcon);
        window.draw(hpText);

        window.draw(hungerBack);
        window.draw(hungerBar);
        window.draw(hungerIcon);
        window.draw(hungerText);

        window.draw(thirstBack);
        window.draw(thirstBar);
        window.draw(thirstIcon);
        window.draw(thirstText);

        window.draw(hungerMultiplierText);
        window.draw(thirstMultiplierText);

        window.draw(expBack);
        window.draw(expBar);

        window.draw(levelText);

        if (shieldEquipped) {
            window.draw(defBack);
            window.draw(defBar);
            window.draw(defText);
        }

        inventory.draw(window);
    }
}

void updateHUDVitalsLayoutAndTexts(
    sf::RenderWindow& window,
    Inventory& inventory,
    bool shieldEquipped,
    int eqShieldSlot,
    float hp, float maxHP,
    float hunger, float maxHunger, int hungerMultiplier,
    float thirst, float maxThirst, int thirstMultiplier,
    int level, int exp,
    sf::RectangleShape& hpBack, sf::RectangleShape& hpBar, sf::Sprite& hpIcon, sf::Text& hpText,
    sf::RectangleShape& hungerBack, sf::RectangleShape& hungerBar, sf::Sprite& hungerIcon, sf::Text& hungerText,
    sf::RectangleShape& thirstBack, sf::RectangleShape& thirstBar, sf::Sprite& thirstIcon, sf::Text& thirstText,
    sf::Text& hungerMultiplierText, sf::Text& thirstMultiplierText,
    sf::RectangleShape& expBack, sf::RectangleShape& expBar, sf::Text& levelText,
    sf::RectangleShape& defBack, sf::RectangleShape& defBar, sf::Text& defText
) {
    const float SLOT_W = 64.f;
    const float SLOT_GAP = 4.f;
    const int SLOT_COUNT = 8;

    float invX = inventory.computeX(window);
    float invY = inventory.computeY(window);
    float invWidth = SLOT_COUNT * (SLOT_W + SLOT_GAP);

    float barWidth = invWidth;
    float barHeight = 22.f;
    float spacing = 80.f;

    float startX = invX;
    hpBack.setSize({ barWidth, barHeight });
    hungerBack.setSize({ barWidth, barHeight });
    thirstBack.setSize({ barWidth, barHeight });
    expBack.setSize({ barWidth, barHeight });

    float numBars = shieldEquipped ? 5.f : 4.f;
    float startY = invY - (barHeight * numBars) - (spacing * (numBars - 1.f));

    hpBack.setPosition({ startX, startY });
    hungerBack.setPosition({ startX, startY + barHeight + spacing });
    thirstBack.setPosition({ startX, startY + (barHeight + spacing) * 2.f });
    expBack.setPosition({ startX, startY + (barHeight + spacing) * 3.f });

    float iconPadding = 6.f;
    float textPadding = 4.f;

    if (shieldEquipped) {
        defBack.setSize({ barWidth, barHeight });
        defBack.setPosition({ startX, startY + (barHeight + spacing) * 4.f });

        float defVal = (eqShieldSlot >= 0) ? (float)inventory.getDurability(eqShieldSlot) : 0.f;
        defBar.setSize({ barWidth * (defVal / 500.f), barHeight });
        defBar.setPosition(defBack.getPosition());

        defText.setString("Defense " + std::to_string((int)defVal) + " / 500");
        defText.setCharacterSize(16);
        defText.setPosition({
            defBack.getPosition().x + textPadding,
            defBack.getPosition().y + 2.f
        });
    }

    hpBar.setSize({ barWidth * (hp / maxHP), barHeight });
    hungerBar.setSize({ barWidth * (1.f - hunger / maxHunger), barHeight });
    thirstBar.setSize({ barWidth * (1.f - thirst / maxThirst), barHeight });

    hpBar.setPosition(hpBack.getPosition());
    hungerBar.setPosition(hungerBack.getPosition());
    thirstBar.setPosition(thirstBack.getPosition());

    hungerMultiplierText.setString("X" + std::to_string(hungerMultiplier));
    hungerMultiplierText.setPosition({
        hungerBack.getPosition().x + barWidth + 20.f,
        hungerBack.getPosition().y - 2.f
    });

    thirstMultiplierText.setString("X" + std::to_string(thirstMultiplier));
    thirstMultiplierText.setPosition({
        thirstBack.getPosition().x + barWidth + 20.f,
        thirstBack.getPosition().y - 2.f
    });

    int nextLevelExp = level * 100;
    expBar.setSize({ barWidth * (static_cast<float>(exp) / nextLevelExp), barHeight });
    expBar.setPosition(expBack.getPosition());

    auto hpBounds = hpIcon.getGlobalBounds();
    auto hungerBounds = hungerIcon.getGlobalBounds();
    auto thirstBounds = thirstIcon.getGlobalBounds();

    hpIcon.setPosition({
        hpBack.getPosition().x - hpBounds.size.x - iconPadding,
        hpBack.getPosition().y + (barHeight - hpBounds.size.y) / 2.f
    });

    hungerIcon.setPosition({
        hungerBack.getPosition().x - hungerBounds.size.x - iconPadding,
        hungerBack.getPosition().y + (barHeight - hungerBounds.size.y) / 2.f
    });

    thirstIcon.setPosition({
        thirstBack.getPosition().x - thirstBounds.size.x - iconPadding,
        thirstBack.getPosition().y + (barHeight - thirstBounds.size.y) / 2.f
    });

    hpText.setString("HP " + std::to_string((int)hp) + " / " + std::to_string((int)maxHP));
    hungerText.setString("Hunger " + std::to_string((int)(maxHunger - hunger)));
    thirstText.setString("Thirst " + std::to_string((int)(maxThirst - thirst)));

    hpText.setCharacterSize(16);
    hungerText.setCharacterSize(16);
    thirstText.setCharacterSize(16);

    hpText.setPosition({
        hpBack.getPosition().x + textPadding,
        hpBack.getPosition().y + 2.f
    });

    hungerText.setPosition({
        hungerBack.getPosition().x + textPadding,
        hungerBack.getPosition().y + 2.f
    });

    thirstText.setPosition({
        thirstBack.getPosition().x + textPadding,
        thirstBack.getPosition().y + 2.f
    });

    levelText.setString("Lv." + std::to_string(level) + " EXP: " + std::to_string(exp) + "/" + std::to_string(level * 100));
    levelText.setPosition({ startX, startY - 40.f });
}

void drawTopRightButtons(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    bool isAdmin
) {
    if (isAdmin) {
        // Admin Sim Button
        sf::RectangleShape adminSimBtn({ 130.f, 40.f });
        adminSimBtn.setPosition({ (float)width - 440.f, 20.f });
        adminSimBtn.setFillColor(sf::Color(85, 30, 115, 220)); // Regal purple/indigo
        adminSimBtn.setOutlineThickness(2.f);
        adminSimBtn.setOutlineColor(sf::Color(255, 215, 0)); // Gold outline
        window.draw(adminSimBtn);

        sf::Text adminSimTxt(uiFont, "Admin Sim", 18);
        adminSimTxt.setStyle(sf::Text::Bold);
        adminSimTxt.setFillColor(sf::Color(255, 225, 120));
        adminSimTxt.setPosition({ (float)width - 425.f, 28.f });
        window.draw(adminSimTxt);
    }

    // HUD Market Stats Button
    sf::RectangleShape marketBtnTopRight({ 150.f, 40.f });
    marketBtnTopRight.setPosition({ (float)width - 300.f, 20.f });
    marketBtnTopRight.setFillColor(sf::Color(40, 110, 90, 200)); // Premium emerald green
    marketBtnTopRight.setOutlineThickness(2.f);
    marketBtnTopRight.setOutlineColor(sf::Color(255, 215, 0)); // Gold outline
    window.draw(marketBtnTopRight);

    sf::Text marketBtnTxtTopRight(uiFont, "Market Stats", 18);
    marketBtnTxtTopRight.setPosition({ (float)width - 280.f, 28.f });
    window.draw(marketBtnTxtTopRight);

    // Settings Button
    sf::RectangleShape settingsBtnTopRight({ 120.f, 40.f });
    settingsBtnTopRight.setPosition({ (float)width - 140.f, 20.f });
    settingsBtnTopRight.setFillColor(sf::Color(80, 80, 80, 200));
    settingsBtnTopRight.setOutlineThickness(2.f);
    settingsBtnTopRight.setOutlineColor(sf::Color::White);
    window.draw(settingsBtnTopRight);

    sf::Text settingsBtnTxtTopRight(uiFont, "Settings", 20);
    settingsBtnTxtTopRight.setPosition({ (float)width - 115.f, 28.f });
    window.draw(settingsBtnTxtTopRight);

    // Logout Button — saves & returns to main menu
    sf::RectangleShape logoutBtn({ 110.f, 40.f });
    logoutBtn.setPosition({ (float)width - 140.f, 70.f });
    logoutBtn.setFillColor(sf::Color(160, 30, 30, 220));
    logoutBtn.setOutlineThickness(2.f);
    logoutBtn.setOutlineColor(sf::Color(255, 100, 100));
    window.draw(logoutBtn);

    sf::Text logoutTxt(uiFont, "Logout", 20);
    logoutTxt.setStyle(sf::Text::Bold);
    logoutTxt.setFillColor(sf::Color::White);
    logoutTxt.setPosition({ (float)width - 120.f, 78.f });
    window.draw(logoutTxt);
}

void drawGoldCurrency(
    sf::RenderWindow& window,
    const sf::Font& font,
    int currency,
    bool goldTexLoaded,
    sf::Sprite& goldSprite
) {
    if (goldTexLoaded) {
        goldSprite.setPosition({ 10.f, 10.f });
        window.draw(goldSprite);
    }

    sf::Text curText(font, std::to_string(currency), 24);
    curText.setFillColor(sf::Color::Yellow);
    curText.setOutlineColor(sf::Color::Black);
    curText.setOutlineThickness(2.f);
    curText.setPosition({ 10.f + (goldTexLoaded ? goldSprite.getGlobalBounds().size.x + 5.f : 0.f), 10.f });
    window.draw(curText);
}

void drawDeathScreen(
    sf::RenderWindow& window,
    const sf::Font& font,
    unsigned int width,
    unsigned int height,
    float elapsedGameTime,
    float timeOffset,
    float totalCycleTime
) {
    window.setView(GetUIView(&window));

    float survivedSeconds = elapsedGameTime - timeOffset;
    int daysSurvived = static_cast<int>(survivedSeconds / totalCycleTime);

    sf::RectangleShape overlay({ (float)width, (float)height });
    overlay.setFillColor(sf::Color(0, 0, 0, 200));
    window.draw(overlay);

    sf::Text deathText(font, "YOU DIED", 120);
    deathText.setFillColor(sf::Color::Red);
    deathText.setPosition({
        (width - deathText.getGlobalBounds().size.x) / 2.f,
        height / 2.f - 150.f
        });

    sf::Text daysText(
        font,
        "You survived " + std::to_string(daysSurvived) + " days",
        48
    );
    daysText.setFillColor(sf::Color::White);
    daysText.setPosition({
        (width - daysText.getGlobalBounds().size.x) / 2.f,
        height / 2.f
        });

    sf::Text exitText(font, "Press ESC to quit", 32);
    exitText.setFillColor(sf::Color::Yellow);
    exitText.setPosition({
        (width - exitText.getGlobalBounds().size.x) / 2.f,
        height / 2.f + 100.f
        });

    window.draw(deathText);
    window.draw(daysText);
    window.draw(exitText);
}

void drawBrightnessOverlay(
    sf::RenderWindow& window,
    unsigned int width,
    unsigned int height,
    float brightness
) {
    float alpha = std::clamp(255.f - (brightness * 2.55f), 0.f, 255.f);
    if (alpha <= 0.5f) return;
    sf::RectangleShape brightnessOverlay({ (float)width, (float)height });
    brightnessOverlay.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(alpha)));
    sf::View prevView = window.getView();
    window.setView(GetUIView(&window));
    window.draw(brightnessOverlay);
    window.setView(prevView);
}

void drawIntroStory(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    unsigned int width,
    unsigned int height,
    const std::optional<sf::Sprite>& introSprite,
    size_t currentIntroFrame,
    const std::vector<std::string>& introTexts
) {
    window.setView(GetUIView(&window));
    window.clear(sf::Color::Black);

    if (introSprite.has_value()) {
        sf::Sprite sprite = *introSprite;
        sf::Vector2u ts = sprite.getTexture().getSize();
        if (ts.x > 0 && ts.y > 0) {
            sprite.setScale({ (float)width / ts.x, (float)height / ts.y });
        }
        window.draw(sprite);

        // Draw Story Text at the bottom
        if (currentIntroFrame < introTexts.size()) {
            sf::RectangleShape textBg({ (float)width - 200.f, 150.f });
            textBg.setFillColor(sf::Color(0, 0, 0, 180));
            textBg.setOutlineColor(sf::Color::White);
            textBg.setOutlineThickness(2.f);
            textBg.setPosition({ 100.f, (float)height - 300.f });
            window.draw(textBg);

            sf::Text storyLine(uiFont, introTexts[currentIntroFrame], 40);
            storyLine.setFillColor(sf::Color::White);
            sf::FloatRect textBounds = storyLine.getLocalBounds();
            storyLine.setPosition({
                textBg.getPosition().x + (textBg.getSize().x - textBounds.size.x) / 2.f,
                textBg.getPosition().y + (textBg.getSize().y - textBounds.size.y) / 2.f - 10.f
                });
            window.draw(storyLine);
        }
    }
    else {
        // Fallback text if no images
        sf::Text story(uiFont, "Long ago, the world was vast and full of life...\nBut a darkness arrived, and survivors scattered.\nYou are one of the few left who can rebuild.\n\nClick to start your journey.", 30);
        story.setPosition({ 100.f, height / 2.f - 100.f });
        window.draw(story);
    }

    sf::Text skipText(uiFont, "Click to Continue...", 30);
    skipText.setPosition({ width / 2.f - 100.f, height - 100.f });
    window.draw(skipText);
}

void drawTutorial(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    const sf::RectangleShape& tutorialOverlay,
    const sf::RectangleShape& tutorialBox,
    size_t tutorialStep,
    const std::vector<TutorialStep>& tutorialSteps,
    float tutMoveDist,
    bool tutSprinted,
    bool tutInvOpened,
    int tutWoodGathered,
    int tutStoneGathered,
    bool tutAttacked,
    bool tutChatted
) {
    if (tutorialStep >= tutorialSteps.size()) return;

    window.setView(GetUIView(&window));
    // Tutorial black overlay removed per user request: only draw the tutorial dialog box
    window.draw(tutorialBox);

    sf::Text title(uiFont, tutorialSteps[tutorialStep].title, 34);
    title.setFillColor(sf::Color::Cyan);
    title.setPosition({ tutorialBox.getPosition().x + 40.f, tutorialBox.getPosition().y + 30.f });
    window.draw(title);

    sf::Text desc(uiFont, tutorialSteps[tutorialStep].description, 24);
    desc.setPosition({ tutorialBox.getPosition().x + 40.f, tutorialBox.getPosition().y + 100.f });
    window.draw(desc);

    bool goalMet = true;
    std::string goalProgress = tutorialSteps[tutorialStep].goal;
    if (tutorialStep == 1) {
        goalMet = (tutMoveDist >= 500.f);
        goalProgress += " (" + std::to_string((int)std::min(500.f, tutMoveDist)) + "/500)";
    }
    else if (tutorialStep == 2) goalMet = tutSprinted;
    else if (tutorialStep == 3) goalMet = tutInvOpened;
    else if (tutorialStep == 4) {
        goalMet = (tutWoodGathered >= 1 && tutStoneGathered >= 1);
        goalProgress += " (Wood: " + std::to_string(std::min(1, tutWoodGathered)) + "/1, Stone: " + std::to_string(std::min(1, tutStoneGathered)) + "/1)";
    }
    else if (tutorialStep == 5) goalMet = tutAttacked;
    else if (tutorialStep == 6) goalMet = tutChatted;

    sf::Text goal(uiFont, "Goal: " + goalProgress, 20);
    goal.setFillColor(goalMet ? sf::Color::Green : sf::Color::Yellow);
    goal.setPosition({ tutorialBox.getPosition().x + 40.f, tutorialBox.getPosition().y + 280.f });
    window.draw(goal);

    sf::RectangleShape nextBtn({ 120.f, 40.f });
    nextBtn.setPosition({ tutorialBox.getPosition().x + 650.f, tutorialBox.getPosition().y + 330.f });
    nextBtn.setFillColor(goalMet ? sf::Color(0, 150, 0) : sf::Color(100, 100, 100));
    window.draw(nextBtn);

    sf::Text nextTxt(uiFont, (tutorialStep == tutorialSteps.size() - 1) ? "Finish" : "Next", 20);
    nextTxt.setPosition({ nextBtn.getPosition().x + 35.f, nextBtn.getPosition().y + 8.f });
    window.draw(nextTxt);
}

void drawSpawnOverlay(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    int spawnOverlayDone,
    int spawnOverlayTotal
) {
    window.setView(GetUIView(&window));
    float pct = 100.f * (float)spawnOverlayDone / (float)spawnOverlayTotal;
    pct = std::clamp(pct, 0.f, 100.f);

    sf::RectangleShape panel({ 520.f, 90.f });
    panel.setFillColor(sf::Color(0, 0, 0, 160));
    panel.setPosition({ 30.f, 30.f });
    window.draw(panel);

    sf::Text t(
        uiFont,
        "Generating world... " + std::to_string((int)pct) + "% (" + std::to_string(spawnOverlayDone) + "/" + std::to_string(spawnOverlayTotal) + ")",
        22
    );
    t.setFillColor(sf::Color::White);
    t.setPosition({ 45.f, 45.f });
    window.draw(t);
}

void drawSleepOverlay(
    sf::RenderWindow& window,
    unsigned int width,
    unsigned int height,
    float sleepFadeTimer,
    float sleepFadeDuration
) {
    sf::View currentView = window.getView();
    window.setView(GetUIView(&window));
    float alpha = (sleepFadeTimer / sleepFadeDuration) * 255.f;
    alpha = std::clamp(alpha, 0.f, 255.f);
    sf::RectangleShape fadeRect(sf::Vector2f((float)width, (float)height));
    fadeRect.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(alpha)));
    fadeRect.setPosition({0.f, 0.f});
    window.draw(fadeRect);
    window.setView(currentView);
}

void drawDisasterNotification(
    sf::RenderWindow& window,
    const sf::Font& font,
    unsigned int width,
    bool disasterActive,
    float disasterRemaining,
    float bannerTimer,
    const std::string& bannerMessage,
    const std::string& currentDisasterType
) {
    sf::View old = window.getView();
    window.setView(GetUIView(&window));

    // 1. If a disaster prompt/banner message is active, show prominent banner
    if (bannerTimer > 0.f && !bannerMessage.empty()) {
        float alpha = 255.f;
        if (bannerTimer < 0.6f) {
            alpha = (bannerTimer / 0.6f) * 255.f;
        }
        std::uint8_t a = static_cast<std::uint8_t>(std::clamp(alpha, 0.f, 255.f));

        sf::Text bText(font, bannerMessage, 22);
        bText.setStyle(sf::Text::Bold);
        sf::FloatRect tb = bText.getLocalBounds();

        float padX = 28.f;
        float bw = tb.size.x + padX * 2.f;
        float bh = 48.f;
        float bx = ((float)width - bw) / 2.f;
        float by = 75.f;

        sf::RectangleShape bg({ bw, bh });
        bg.setPosition({ bx, by });
        if (currentDisasterType == "drought") {
            bg.setFillColor(sf::Color(55, 25, 10, a));
            bg.setOutlineThickness(3.f);
            bg.setOutlineColor(sf::Color(255, 140, 30, a));
        } else {
            bg.setFillColor(sf::Color(45, 12, 12, a));
            bg.setOutlineThickness(3.f);
            bg.setOutlineColor(sf::Color(255, 60, 60, a));
        }
        window.draw(bg);

        sf::RectangleShape sideBar({ 6.f, bh });
        sideBar.setPosition({ bx, by });
        sideBar.setFillColor(sf::Color(255, 180, 0, a));
        window.draw(sideBar);

        bText.setFillColor(sf::Color(255, 235, 120, a));
        bText.setPosition({
            bx + padX,
            by + (bh - tb.size.y) / 2.f - tb.position.y
        });
        window.draw(bText);
    }

    // 2. While disaster is ongoing, show a warning badge at top of screen
    if (disasterActive && disasterRemaining > 0.f) {
        int totalSec = (int)std::ceil(disasterRemaining);
        std::string timeStr;
        if (totalSec >= 60) {
            int m = totalSec / 60;
            int s = totalSec % 60;
            timeStr = std::to_string(m) + "m " + (s < 10 ? "0" : "") + std::to_string(s) + "s";
        } else {
            timeStr = std::to_string(totalSec) + "s";
        }

        std::string badgeStr;
        if (currentDisasterType == "drought") {
            badgeStr = "! SEVERE DROUGHT: Ponds dried up! Flora & Fauna withering! (" + timeStr + ") !";
        } else {
            badgeStr = "! DIARRHEA OUTBREAK: Dehydrating rapidly! (" + timeStr + ") !";
        }

        sf::Text badgeText(font, badgeStr, 17);
        badgeText.setStyle(sf::Text::Bold);
        sf::FloatRect bounds = badgeText.getLocalBounds();

        float w = bounds.size.x + 30.f;
        float h = 32.f;
        float x = ((float)width - w) / 2.f;
        float y = 20.f;

        sf::RectangleShape bg({ w, h });
        bg.setPosition({ x, y });
        if (currentDisasterType == "drought") {
            bg.setFillColor(sf::Color(115, 45, 10, 230));
            bg.setOutlineThickness(2.f);
            bg.setOutlineColor(sf::Color(255, 140, 30, 240));
            badgeText.setFillColor(sf::Color(255, 230, 130));
        } else {
            bg.setFillColor(sf::Color(80, 20, 20, 220));
            bg.setOutlineThickness(2.f);
            bg.setOutlineColor(sf::Color(255, 80, 80, 240));
            badgeText.setFillColor(sf::Color(255, 220, 100));
        }
        window.draw(bg);

        badgeText.setPosition({
            x + 15.f,
            y + (h - bounds.size.y) / 2.f - bounds.position.y
        });
        window.draw(badgeText);
    }

    window.setView(old);
}

void renderAllGameUIs(
    sf::RenderWindow& window,
    const sf::Font& uiFont,
    const sf::Font& font,
    unsigned int width,
    unsigned int height,
    const GameSettings& globalSettings,
    bool wrenchMenuOpen,
    sf::Vector2f wrenchMenuPos,
    bool isTargetNPC,
    const std::string& wrenchTargetPlayer,
    bool p2pInvitePopup,
    int p2pTradeID,
    const P2PTradeState& p2pState,
    bool p2pTradeOpen,
    const std::string& p2pPartner,
    TradeSlot myP2PSlots[4],
    std::string& myP2PGoldInput,
    bool p2pGoldTyping,
    bool p2pMyReady,
    const std::vector<P2PItemDisplay>& partnerItems,
    int partnerGold,
    bool p2pPartnerReady,
    const std::string& p2pStatusMsg,
    bool craftMenuOpen,
    const std::vector<CraftableItem>& craftableItems,
    Inventory& inventory,
    const sf::Texture& woodTex,
    const sf::Texture& stoneTex,
    const sf::Texture& woolTex,
    const sf::Texture& saplingTex,
    const sf::Texture& cookedChickenTex,
    const sf::Texture& cookedPigTex,
    const sf::Texture& cookedSheepTex,
    const sf::Texture& woodenSwordTex,
    const sf::Texture& shieldTex,
    const sf::Texture& furnaceTex,
    const sf::Texture& woodTileTex,
    const sf::Texture& woodWallTex,
    const sf::Texture& woodDoorTex,
    const sf::Texture& woodRoofTex,
    const sf::Texture& woodStairsTex,
    const sf::Texture& bedTex,
    bool swordCrafting,
    bool shieldCrafting,
    bool placementMode,
    bool saplingPlacementMode,
    float saplingRotation,
    bool tradeMenuOpen,
    TradeSlot playerTradeSlots[4],
    TradeSlot npcTradeSlots[4],
    bool isSellingToNPC,
    int npcTradeGold,
    std::string& npcGoldInput,
    std::string& playerGoldInput,
    bool playerGoldTyping,
    int currency,
    bool hasBargained,
    bool bargainAccepted,
    const std::string& bargainStatus,
    sf::Color bargainColor,
    bool tradeConfirmOpen,
    const std::string& tradeConfirmMessage,
    bool stockPageOpen,
    bool dbStartupEnabled,
    bool& recentTradesFetchInFlight,
    sf::Clock& recentTradesClock,
    std::future<std::vector<TradeRecord>>& recentTradesFuture,
    std::vector<TradeRecord>& cachedRecentTrades,
    const std::vector<StockItem>& stockMarket,
    bool marketSearchActive,
    const std::string& marketSearchText,
    sf::Clock& marketCursorBlink,
    int marketScrollOffset,
    bool iapPageOpen,
    bool iapVerifyingOpen,
    const std::string& iapVerifyingTier,
    int iapVerifyingGold,
    float iapVerifyingPrice,
    TextInput& iapCodeInput,
    bool isDead,
    float elapsedGameTime,
    float timeOffset,
    float totalCycleTime,
    bool selectionDone,
    bool chatActive,
    int loginMessageIdOffset,
    bool& chatFetchInFlight,
    sf::Clock& chatFetchClock,
    std::future<std::vector<std::pair<std::string, std::string>>>& chatFuture,
    std::vector<std::pair<std::string, std::string>>& chatMessages,
    float chatBoxX, float chatBoxY, float chatBoxWidth, float chatBoxHeight,
    float chatScroll, bool isChatting, TextInput& chatInput,
    bool goldTexLoaded, sf::Sprite& goldSprite,
    bool introActive, const std::optional<sf::Sprite>& introSprite, int currentIntroFrame, const std::vector<std::string>& introTexts,
    bool tutorialActive, const sf::RectangleShape& tutorialOverlay, const sf::RectangleShape& tutorialBox,
    int tutorialStep, const std::vector<TutorialStep>& tutorialSteps,
    float tutMoveDist, bool tutSprinted, bool tutInvOpened, int tutWoodGathered, int tutStoneGathered, bool tutAttacked, bool tutChatted,
    bool spawnOverlayActive, int spawnOverlayDone, int spawnOverlayTotal,
    bool chestMenuOpen,
    PlacedStructure* activeChest,
    int chestSelectedInvSlot,
    std::vector<PlacedCampfire>& campfires,
    std::vector<PlacedStructure>& structures,
    bool quantityPromptOpen, bool quantityPromptIsCampfire, int quantityPromptCampfireSlot, int quantityPromptMax, TextInput& quantityInput,
    bool quantityPromptIsChest, int quantityPromptChestSlot, int quantityPromptSourceSlot,
    bool isSleeping, float sleepFadeTimer, float sleepFadeDuration,
    std::function<const sf::Texture* (int id)> getItemTexture,
    std::function<std::string(int id)> getItemName,
    std::function<int(int id)> getItemPrice,
    bool disasterActive,
    float disasterRemaining,
    float disasterBannerTimer,
    const std::string& disasterBannerMessage,
    bool isAdmin,
    const std::string& currentDisasterType
) {
    window.setView(GetUIView(&window));

    if (wrenchMenuOpen) {
        drawWrenchMenu(window, uiFont, wrenchMenuPos, isTargetNPC, wrenchTargetPlayer);
    }

    if (p2pInvitePopup && p2pTradeID > 0) {
        drawP2PInvitePopup(window, uiFont, width, height, p2pState.initiator);
    }

    if (p2pTradeOpen) {
        drawP2PTrade(
            window,
            uiFont,
            width,
            height,
            p2pPartner,
            myP2PSlots,
            myP2PGoldInput,
            p2pGoldTyping,
            p2pMyReady,
            partnerItems,
            partnerGold,
            p2pPartnerReady,
            p2pStatusMsg,
            sf::Color(200, 200, 100),
            getItemTexture,
            getItemName
        );
    }

    if (craftMenuOpen) {
        drawCraftMenu(window, uiFont, width, height, craftableItems, inventory, woodTex, stoneTex, woolTex, swordCrafting, shieldCrafting);
    }

    if (placementMode) {
        drawPlacementControls(window, font, width, height, inventory, woodTex, stoneTex);
    }

    if (saplingPlacementMode) {
        drawSaplingPlacementControls(window, uiFont, width, height, saplingRotation);
    }

    if (tradeMenuOpen) {
        drawNPCTrade(
            window,
            uiFont,
            width,
            height,
            playerTradeSlots,
            npcTradeSlots,
            isSellingToNPC,
            npcTradeGold,
            npcGoldInput,
            playerGoldInput,
            playerGoldTyping,
            currency,
            hasBargained,
            bargainAccepted,
            bargainStatus,
            bargainColor,
            getItemPrice
        );
    }

    if (tradeConfirmOpen) {
        drawTradeConfirmPopup(window, uiFont, width, height, tradeConfirmMessage);
    }

    if (stockPageOpen) {
        if (dbStartupEnabled) {
            if (!recentTradesFetchInFlight && (cachedRecentTrades.empty() || recentTradesClock.getElapsedTime().asSeconds() > 2.0f)) {
                recentTradesFuture = std::async(std::launch::async, []() {
                    return AuthManager::getInstance().getRecentTrades(3);
                });
                recentTradesFetchInFlight = true;
                recentTradesClock.restart();
            }
            if (recentTradesFetchInFlight && recentTradesFuture.valid()) {
                try {
                    if (recentTradesFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                        cachedRecentTrades = recentTradesFuture.get();
                        recentTradesFetchInFlight = false;
                    }
                }
                catch (...) {
                    recentTradesFetchInFlight = false;
                }
            }
        }

        MarketTextures mTex;
        mTex.woodTex = &woodTex;
        mTex.stoneTex = &stoneTex;
        mTex.saplingTex = &saplingTex;
        mTex.cookedChickenTex = &cookedChickenTex;
        mTex.cookedPigTex = &cookedPigTex;
        mTex.cookedSheepTex = &cookedSheepTex;
        mTex.woolTex = &woolTex;
        mTex.woodenSwordTex = &woodenSwordTex;
        mTex.shieldTex = &shieldTex;
        mTex.furnaceTex = &furnaceTex;
        mTex.woodTileTex = &woodTileTex;
        mTex.woodWallTex = &woodWallTex;
        mTex.woodDoorTex = &woodDoorTex;
        mTex.woodRoofTex = &woodRoofTex;
        mTex.woodStairsTex = &woodStairsTex;
        mTex.bedTex = &bedTex;
        mTex.coalTex = getItemTexture ? getItemTexture(ITEM_COAL) : nullptr;
        mTex.copperOreTex = getItemTexture ? getItemTexture(ITEM_COPPER_ORE) : nullptr;
        mTex.copperBarTex = getItemTexture ? getItemTexture(ITEM_COPPER_BAR) : nullptr;
        mTex.ironOreTex = getItemTexture ? getItemTexture(ITEM_IRON_ORE) : nullptr;
        mTex.ironBarTex = getItemTexture ? getItemTexture(ITEM_IRON_BAR) : nullptr;
        mTex.goldOreTex = getItemTexture ? getItemTexture(ITEM_GOLD_ORE) : nullptr;
        mTex.goldBarTex = getItemTexture ? getItemTexture(ITEM_GOLD_BAR) : nullptr;
        mTex.diamondTex = getItemTexture ? getItemTexture(ITEM_DIAMOND) : nullptr;

        drawStockMarketPage(
            window,
            uiFont,
            width,
            height,
            stockMarket,
            marketSearchActive,
            marketSearchText,
            marketCursorBlink,
            marketScrollOffset,
            mTex,
            cachedRecentTrades,
            isAdmin
        );
    }

    if (iapPageOpen) {
        drawIAPStore(window, uiFont, width, height, currency);
    }

    if (iapVerifyingOpen) {
        drawIAPVerifying(window, uiFont, width, height, iapVerifyingTier, iapVerifyingGold, iapVerifyingPrice, iapCodeInput);
    }

    drawTopRightButtons(window, uiFont, width, isAdmin);

    if (isDead) {
        drawDeathScreen(window, font, width, height, elapsedGameTime, timeOffset, totalCycleTime);
    }

    if (selectionDone) {
        if (chatActive) {
            if (dbStartupEnabled) {
                if (!chatFetchInFlight && chatFetchClock.getElapsedTime().asSeconds() > 2.0f) {
                    int offset = loginMessageIdOffset;
                    chatFuture = std::async(std::launch::async, [offset]() {
                        return AuthManager::getInstance().fetchChatMessages(50, offset);
                    });
                    chatFetchInFlight = true;
                    chatFetchClock.restart();
                }
                if (chatFetchInFlight && chatFuture.valid()) {
                    try {
                        if (chatFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                            chatMessages = chatFuture.get();
                            chatFetchInFlight = false;
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "[ERROR] Async chat failure: " << e.what() << "\n";
                        chatFetchInFlight = false;
                    }
                    catch (...) {
                        std::cerr << "[ERROR] Async chat unknown failure\n";
                        chatFetchInFlight = false;
                    }
                }
            }

            drawChatBox(window, font, chatBoxX, chatBoxY, chatBoxWidth, chatBoxHeight, chatScroll, chatMessages, isChatting, chatInput);
        }

        drawGoldCurrency(window, font, currency, goldTexLoaded, goldSprite);
    }

    if (introActive) {
        drawIntroStory(window, uiFont, width, height, introSprite, currentIntroFrame, introTexts);
    }

    if (tutorialActive) {
        drawTutorial(window, uiFont, tutorialOverlay, tutorialBox, tutorialStep, tutorialSteps, tutMoveDist, tutSprinted, tutInvOpened, tutWoodGathered, tutStoneGathered, tutAttacked, tutChatted);
    }

    if (spawnOverlayActive && spawnOverlayTotal > 0) {
        drawSpawnOverlay(window, uiFont, spawnOverlayDone, spawnOverlayTotal);
    }

    if (chestMenuOpen && activeChest != nullptr)
    {
        drawChestMenu(window, uiFont, width, height, *activeChest, inventory, chestSelectedInvSlot, getItemTexture);
    }

    for (auto& fire : campfires) {
        if (fire.menuOpen) {
            drawCampfireMenu(window, uiFont, width, height, fire, woodTex, cookedChickenTex, cookedPigTex, cookedSheepTex);
        }
    }

    for (auto& s : structures) {
        if (s.id == ITEM_FURNACE && s.menuOpen) {
            drawFurnaceMenu(window, uiFont, width, height, s, woodTex, cookedChickenTex, cookedPigTex, cookedSheepTex, getItemTexture);
        }
    }

    if (quantityPromptOpen) {
        // Determine if this is a retrieve (slot occupied) or store (slot empty) operation
        bool isChestRetrieve = quantityPromptIsChest && (quantityPromptSourceSlot == -1);
        drawQuantityPrompt(window, uiFont, width, height,
            quantityPromptIsCampfire, quantityPromptCampfireSlot,
            quantityPromptMax, quantityInput,
            quantityPromptIsChest, quantityPromptChestSlot, isChestRetrieve);
    }

    if (isSleeping) {
        drawSleepOverlay(window, width, height, sleepFadeTimer, sleepFadeDuration);
    }

    drawDisasterNotification(window, uiFont, width, disasterActive, disasterRemaining, disasterBannerTimer, disasterBannerMessage, currentDisasterType);
}

bool handleIntroAndTutorialClicks(
    sf::Vector2f uiPos,
    bool selectionDone,
    bool& introActive,
    int& currentIntroFrame,
    const std::vector<sf::Texture>& introFrames,
    std::optional<sf::Sprite>& introSprite,
    bool& tutorialActive,
    int& tutorialStep,
    const std::vector<TutorialStep>& tutorialSteps,
    const sf::RectangleShape& tutorialBox,
    float tutMoveDist,
    bool tutSprinted,
    bool tutInvOpened,
    int tutWoodGathered,
    int tutStoneGathered,
    bool tutAttacked,
    bool tutChatted,
    AuthManager& authManager,
    const std::string& username,
    std::string& latestPlayerMsg,
    float& bubbleTimer
) {
    if (selectionDone && introActive) {
        currentIntroFrame++;
        if (currentIntroFrame >= introFrames.size() || (introFrames.empty() && currentIntroFrame >= 1)) {
            introActive = false;
            tutorialActive = true;
            tutorialStep = 0;
        }
        else if (!introFrames.empty() && introSprite.has_value()) {
            introSprite->setTexture(introFrames[currentIntroFrame]);
        }
        return true;
    }
    else if (selectionDone && tutorialActive) {
        bool goalMet = true;
        if (tutorialStep == 1) goalMet = (tutMoveDist >= 500.f);
        else if (tutorialStep == 2) goalMet = tutSprinted;
        else if (tutorialStep == 3) goalMet = tutInvOpened;
        else if (tutorialStep == 4) goalMet = (tutWoodGathered >= 1 && tutStoneGathered >= 1);
        else if (tutorialStep == 5) goalMet = tutAttacked;
        else if (tutorialStep == 6) goalMet = tutChatted;

        sf::FloatRect nextBtn({ tutorialBox.getPosition().x + 650.f, tutorialBox.getPosition().y + 330.f }, { 120.f, 40.f });
        if (goalMet && nextBtn.contains(uiPos)) {
            tutorialStep++;
            if (tutorialStep >= (int)tutorialSteps.size()) {
                tutorialActive = false;
                std::string uName = username;
                std::thread([uName]() {
                    try {
                        AuthManager::getInstance().setFirstTimeFinished(uName.c_str());
                    } catch (...) {}
                }).detach();
                latestPlayerMsg = "You can now get daily quests from the NPC!";
                bubbleTimer = 5.0f;
            }
        }
        return true;
    }
    return false;
}
