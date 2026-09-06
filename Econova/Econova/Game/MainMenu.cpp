#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <iostream>
#include <vector>
#include <algorithm>
#include <atomic>
#include <future>
#include <thread>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <SFML/Graphics.hpp>
#include "Game.h"
#include "Settings.h"
#include "ViewUtils.h"
#include "AuthManager.h"
#include "TextInput.h"

using namespace std;

enum class MenuState {
    MainMenu,
    AuthChoice,
    Register,
    Login,
    LoggingIn,      // async login in progress � shows progress bar
    Registering,    // async register in progress shows progress bar
    LoadingGame,    // loading screen before entering game

    ForgotPasswordStep1,
    ForgotPasswordStep2
};

bool isValidPassword(const std::string& pwd) {
    bool hasNum = false, hasAlpha = false, hasSymbol = false;
    for (char c : pwd) {
        if (isalpha(c)) hasAlpha = true;
        else if (isdigit(c)) hasNum = true;
        else hasSymbol = true;
    }
    return hasNum && hasAlpha && hasSymbol;
}

sf::RectangleShape createButton(float x, float y, float w, float h, sf::Color color) {
    sf::RectangleShape btn({ w, h });
    btn.setPosition({ x, y });
    btn.setFillColor(color);
    return btn;
}


int main(int argc, char** argv) {
#if defined(_WIN32)
    char path[MAX_PATH] = { 0 };
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        path[len] = '\0';
        std::string::size_type pos = std::string(path).find_last_of("\\/");
        if (pos != std::string::npos) {
            std::string parent_path = std::string(path).substr(0, pos);
            if (!parent_path.empty()) {
                _chdir(parent_path.c_str());
            }
        }
    }
#else
    std::string argvStr(argv[0]);
    std::string::size_type pos = argvStr.find_last_of("\\/");
    if (pos != std::string::npos) {
        std::string parent = argvStr.substr(0, pos);
        if (!parent.empty()) {
            chdir(parent.c_str());
        }
    }
#endif

    std::cout << "[DEBUG] main() started. Pre-constructing AuthManager singleton on main thread." << std::endl;
    // CRITICAL: touch the singleton here on the main thread BEFORE spawning any
    // worker threads.  C++11 guarantees the static-local inside getInstance() is
    // initialized exactly once, but if two threads race to do the first call the
    // program can deadlock on MSVC's static-init mutex.  Calling it here first
    // means the object is fully constructed by the time the worker thread calls it.
    (void)AuthManager::getInstance();

    // --- Async DB init with timeout ---
    // connect() can block forever without OPT_CONNECT_TIMEOUT.
    // We run it on a thread and give it 15 seconds before showing an error.
    std::atomic<bool> dbDone(false);
    std::atomic<bool> dbSuccess(false);
    auto launchDBInit = [&]() {
        dbDone = false;
        dbSuccess = false;
        std::thread([&]() {
            dbSuccess = AuthManager::getInstance().initDB();
            dbDone = true;
            }).detach();
        };
    launchDBInit();

    // Show loading window while connecting
    {
        sf::VideoMode loadDesktop = sf::VideoMode::getDesktopMode();
        sf::RenderWindow loadWin(sf::VideoMode({ loadDesktop.size.x, loadDesktop.size.y }), "Econova");
        loadWin.setFramerateLimit(60);

        sf::Font loadFont;
        bool fontOk = loadFont.openFromFile("C:/Windows/Fonts/arial.ttf");

        // --- Shared UI helpers ---
        auto makeText = [&](const std::string& str, unsigned int size, sf::Color col = sf::Color::White) {
            sf::Text t(loadFont, str, size);
            t.setFillColor(col);
            t.setOutlineColor(sf::Color::Black);
            t.setOutlineThickness(2.f);
            return t;
            };
        auto centerX = [&](sf::Text& t, float y, float winW) {
            t.setPosition({ winW / 2.f - t.getLocalBounds().size.x / 2.f, y });
            };

        sf::RectangleShape barBg({ 600.f, 24.f });
        barBg.setFillColor(sf::Color(60, 60, 60));
        sf::RectangleShape barFg({ 0.f, 24.f });
        barFg.setFillColor(sf::Color(39, 174, 96));

        // Retry / Offline buttons (shown only on failure)
        sf::RectangleShape btnRetry({ 260.f, 70.f });
        btnRetry.setFillColor(sf::Color(39, 174, 96));
        sf::RectangleShape btnOffline({ 260.f, 70.f });
        btnOffline.setFillColor(sf::Color(41, 128, 185));

        sf::Clock dotClock, barClock, timeoutClock;
        int dotCount = 0;
        bool timedOut = false;
        bool offlineChosen = false;
        const float TIMEOUT_SECS = 15.f;

        // Loading tips shown while connecting
        const std::vector<std::string> tips = {
            "Tip: Eat food regularly to keep your hunger up!",
            "Tip: Build structures to protect yourself at night.",
            "Tip: Trading with other players earns bonus gold.",
            "Tip: Campfires provide warmth and cook your food.",
            "Tip: Complete daily quests for extra rewards.",
        };
        int tipIndex = 0;
        sf::Clock tipClock;

        while (loadWin.isOpen()) {
            float lw = (float)loadWin.getSize().x;
            float lh = (float)loadWin.getSize().y;

            while (const std::optional ev = loadWin.pollEvent()) {
                if (ev->is<sf::Event::Closed>()) { loadWin.close(); return 0; }

                if (timedOut) {
                    if (const auto* mb = ev->getIf<sf::Event::MouseButtonPressed>()) {
                        sf::Vector2f mp = loadWin.mapPixelToCoords(mb->position);
                        if (btnRetry.getGlobalBounds().contains(mp)) {
                            // Retry: relaunch the init thread
                            timedOut = false;
                            timeoutClock.restart();
                            barClock.restart();
                            launchDBInit();
                        }
                        else if (btnOffline.getGlobalBounds().contains(mp)) {
                            offlineChosen = true;
                            loadWin.close();
                        }
                    }
                }
            }

            if (!timedOut && dbDone.load()) {
                // Connected (or failed fast) � close and continue
                loadWin.close();
                break;
            }

            if (!timedOut && timeoutClock.getElapsedTime().asSeconds() > TIMEOUT_SECS) {
                timedOut = true;
                std::cerr << "[WARN] DB connect timed out after " << TIMEOUT_SECS << "s\n";
            }

            loadWin.clear(sf::Color(20, 20, 20));

            if (!timedOut) {
                // Animate dots
                if (dotClock.getElapsedTime().asSeconds() > 0.4f) {
                    dotCount = (dotCount + 1) % 4;
                    dotClock.restart();
                }
                // Cycle tips every 4 seconds
                if (tipClock.getElapsedTime().asSeconds() > 4.f) {
                    tipIndex = (tipIndex + 1) % (int)tips.size();
                    tipClock.restart();
                }
                float prog = std::min(0.85f, timeoutClock.getElapsedTime().asSeconds() / TIMEOUT_SECS);
                barFg.setSize({ 600.f * prog, 24.f });

                if (fontOk) {
                    // Title
                    auto titleTxt = makeText("ECONOVA", 80, sf::Color(39, 174, 96));
                    centerX(titleTxt, lh / 2.f - 180.f, lw);
                    loadWin.draw(titleTxt);

                    // Connecting text with animated dots
                    auto connTxt = makeText("Connecting to server" + std::string(dotCount, '.'), 46);
                    centerX(connTxt, lh / 2.f - 80.f, lw);
                    barBg.setPosition({ lw / 2.f - 300.f, lh / 2.f });
                    barFg.setPosition(barBg.getPosition());
                    loadWin.draw(connTxt);
                    loadWin.draw(barBg);
                    loadWin.draw(barFg);

                    auto subTxt = makeText("This may take a few seconds...", 28, sf::Color(160, 160, 160));
                    centerX(subTxt, lh / 2.f + 40.f, lw);
                    loadWin.draw(subTxt);

                    // Cycling tip
                    auto tipTxt = makeText(tips[tipIndex], 26, sf::Color(200, 200, 120));
                    centerX(tipTxt, lh / 2.f + 100.f, lw);
                    loadWin.draw(tipTxt);
                }
            }
            else {
                // Timed-out / error screen
                if (fontOk) {
                    auto errTxt = makeText("Could not connect to server", 52, sf::Color(231, 76, 60));
                    auto sub1 = makeText("Check your internet connection or try again.", 30, sf::Color(200, 200, 200));
                    auto sub2 = makeText("You can also play offline (no saving/loading).", 30, sf::Color(200, 200, 200));
                    centerX(errTxt, lh / 2.f - 120.f, lw);
                    centerX(sub1, lh / 2.f - 50.f, lw);
                    centerX(sub2, lh / 2.f - 10.f, lw);
                    loadWin.draw(errTxt);
                    loadWin.draw(sub1);
                    loadWin.draw(sub2);

                    // Retry button
                    btnRetry.setPosition({ lw / 2.f - 280.f, lh / 2.f + 60.f });
                    sf::Vector2f mp = loadWin.mapPixelToCoords(sf::Mouse::getPosition(loadWin));
                    btnRetry.setFillColor(btnRetry.getGlobalBounds().contains(mp)
                        ? sf::Color(46, 204, 113) : sf::Color(39, 174, 96));
                    auto retryTxt = makeText("Retry", 34);
                    retryTxt.setPosition({
                        btnRetry.getPosition().x + (btnRetry.getSize().x - retryTxt.getLocalBounds().size.x) / 2.f,
                        btnRetry.getPosition().y + (btnRetry.getSize().y - retryTxt.getLocalBounds().size.y) / 2.f - 8.f
                        });
                    loadWin.draw(btnRetry);
                    loadWin.draw(retryTxt);

                    // Play offline button
                    btnOffline.setPosition({ lw / 2.f + 20.f, lh / 2.f + 60.f });
                    btnOffline.setFillColor(btnOffline.getGlobalBounds().contains(mp)
                        ? sf::Color(52, 152, 219) : sf::Color(41, 128, 185));
                    auto offTxt = makeText("Play Offline", 34);
                    offTxt.setPosition({
                        btnOffline.getPosition().x + (btnOffline.getSize().x - offTxt.getLocalBounds().size.x) / 2.f,
                        btnOffline.getPosition().y + (btnOffline.getSize().y - offTxt.getLocalBounds().size.y) / 2.f - 8.f
                        });
                    loadWin.draw(btnOffline);
                    loadWin.draw(offTxt);
                }
            }

            loadWin.display();
        }

        if (offlineChosen) {
            std::cout << "[INFO] User chose offline mode.\n";
            // dbSuccess stays false � all AuthManager calls will no-op safely
        }
    }

    std::cout << "[DEBUG] DB init result: " << (dbSuccess ? "connected" : "offline/failed") << "\n";

    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    unsigned int windowWidth = desktop.size.x;
    unsigned int windowHeight = desktop.size.y;

    // Use a fixed virtual resolution of 1920x1080 for UI positioning and gameplay coordinates
    unsigned int width = 1920;
    unsigned int height = 1080;

    // Standard resolution list
    std::vector<sf::Vector2u> resolutions = {
        {1280, 720},
        {1920, 1080},
        {2560, 1440},
        {3840, 2160}
    };
    bool foundDesktop = false;
    for (size_t i = 0; i < resolutions.size(); ++i) {
        if (resolutions[i].x == windowWidth && resolutions[i].y == windowHeight) {
            globalSettings.resolutionIndex = static_cast<int>(i);
            foundDesktop = true;
            break;
        }
    }
    if (!foundDesktop) {
        resolutions.push_back({ windowWidth, windowHeight });
        std::sort(resolutions.begin(), resolutions.end(), [](const sf::Vector2u& a, const sf::Vector2u& b) {
            return (a.x * a.y) < (b.x * b.y);
            });
        for (size_t i = 0; i < resolutions.size(); ++i) {
            if (resolutions[i].x == windowWidth && resolutions[i].y == windowHeight) {
                globalSettings.resolutionIndex = static_cast<int>(i);
                break;
            }
        }
    }

    sf::RenderWindow* window = new sf::RenderWindow(sf::VideoMode({ windowWidth, windowHeight }), "Econova");
    window->setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        std::cout << "Failed to load font!\n";
        return -1;
    }

    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile(resolvePath("MainMenu/Background.png"))) {
        std::cout << "Failed to load background!\n";
    }
    sf::Sprite bgSprite(bgTexture);
    bgSprite.setScale({
        1920.f / bgSprite.getLocalBounds().size.x,
        1080.f / bgSprite.getLocalBounds().size.y
        });

    sf::Texture logoTexture;
    if (!logoTexture.loadFromFile(resolvePath("MainMenu/Logo.jpg"))) {
        std::cout << "Failed to load logo!\n";
    }
    sf::Sprite logoSprite(logoTexture);
    if (logoSprite.getLocalBounds().size.x > 0 && logoSprite.getLocalBounds().size.y > 0) {
        float maxLogoW = 550.f;
        float maxLogoH = 260.f;
        float logoScaleX = maxLogoW / logoSprite.getLocalBounds().size.x;
        float logoScaleY = maxLogoH / logoSprite.getLocalBounds().size.y;
        float logoScale = std::min(logoScaleX, logoScaleY);
        logoSprite.setScale({ logoScale, logoScale });
    }
    logoSprite.setPosition({ width / 2.f - logoSprite.getGlobalBounds().size.x / 2.f, 60.f });

    MenuState state = MenuState::MainMenu;
    std::string errorMessage = "";
    std::string currentEmailForReset = "";
    std::string currentOTP = "";

    // Async OTP state – generateForgotOTP is a DB round-trip; don't call it on the render thread.
    std::atomic<bool> otpPending(false);
    std::atomic<bool> otpDone(false);
    std::string       otpResult;        // written by worker, read after otpDone
    std::mutex        otpMutex;

    // Common Text settings
    sf::Text errorText(font, "", 60);
    errorText.setFillColor(sf::Color::Red);
    errorText.setPosition({ width / 2.f - 400.f, height - 300.f });

    // State: MainMenu
    sf::RectangleShape btnPlay = createButton((width - 600.f) / 2.f, height / 2.f - 200.f, 600.f, 150.f, sf::Color::Green);
    sf::Text txtPlay(font, "Play Game", 100);
    txtPlay.setPosition(btnPlay.getPosition());

    sf::RectangleShape btnSettings = createButton((width - 600.f) / 2.f, height / 2.f + 50.f, 600.f, 150.f, sf::Color::Green);
    sf::Text txtSettings(font, "Settings", 100);
    txtSettings.setPosition(btnSettings.getPosition());

    sf::RectangleShape btnExit = createButton((width - 600.f) / 2.f, height / 2.f + 300.f, 600.f, 150.f, sf::Color::Green);
    sf::Text txtExit(font, "Exit", 100);
    txtExit.setPosition(btnExit.getPosition());

    // State: AuthChoice
    sf::Text titleAuthChoice(font, "You must login to play", 120);
    titleAuthChoice.setPosition({ width / 2.f - 600.f, 200.f });
    titleAuthChoice.setFillColor(sf::Color::White);

    sf::RectangleShape btnLoginChoice = createButton((width - 600.f) / 2.f, height / 2.f - 200.f, 600.f, 150.f, sf::Color::Blue);
    sf::Text txtLoginChoice(font, "Login", 100);
    txtLoginChoice.setPosition(btnLoginChoice.getPosition());

    sf::RectangleShape btnRegisterChoice = createButton((width - 600.f) / 2.f, height / 2.f + 50.f, 600.f, 150.f, sf::Color::Blue);
    sf::Text txtRegisterChoice(font, "Register", 100);
    txtRegisterChoice.setPosition(btnRegisterChoice.getPosition());

    sf::RectangleShape btnBack = createButton(100.f, 100.f, 300.f, 100.f, sf::Color::Red);
    sf::Text txtBack(font, "Back", 70);
    txtBack.setPosition(btnBack.getPosition());

    // State: Register
    sf::Text titleRegister(font, "Register", 120);
    titleRegister.setPosition({ width / 2.f - 300.f, 150.f });
    titleRegister.setFillColor(sf::Color::White);

    TextInput regUser(font, { width / 2.f - 200.f, 260.f }, { 500.f, 70.f });
    sf::Text lblRegUser(font, "Username:", 70); lblRegUser.setPosition({ width / 2.f - 800.f, 400.f });

    TextInput regPass(font, { width / 2.f - 200.f, 380.f }, { 500.f, 70.f }, true);
    sf::Text lblRegPass(font, "Password:", 70); lblRegPass.setPosition({ width / 2.f - 800.f, 600.f });

    TextInput regConf(font, { width / 2.f - 200.f, 500.f }, { 500.f, 70.f }, true);
    sf::Text lblRegConf(font, "Confirm:", 70); lblRegConf.setPosition({ width / 2.f - 800.f, 800.f });

    TextInput regEmail(font, { width / 2.f - 200.f, 620.f }, { 500.f, 70.f });
    sf::Text lblRegEmail(font, "Email:", 70); lblRegEmail.setPosition({ width / 2.f - 800.f, 1000.f });

    sf::RectangleShape btnSubmitReg = createButton((width - 400.f) / 2.f, 1200.f, 400.f, 120.f, sf::Color::Green);
    sf::Text txtSubmitReg(font, "Submit", 80); txtSubmitReg.setPosition(btnSubmitReg.getPosition());

    // State: Login
    sf::Text titleLogin(font, "Login", 120);
    titleLogin.setPosition({ width / 2.f - 200.f, 200.f });
    titleLogin.setFillColor(sf::Color::White);

    TextInput logUser(font, { width / 2.f - 200.f, 340.f }, { 500.f, 70.f });
    sf::Text lblLogUser(font, "Username:", 70); lblLogUser.setPosition({ width / 2.f - 800.f, 500.f });

    TextInput logPass(font, { width / 2.f - 200.f, 480.f }, { 500.f, 70.f }, true);
    sf::Text lblLogPass(font, "Password:", 70); lblLogPass.setPosition({ width / 2.f - 800.f, 700.f });

    sf::RectangleShape btnSubmitLog = createButton((width - 400.f) / 2.f, 900.f, 400.f, 120.f, sf::Color::Green);
    sf::Text txtSubmitLog(font, "Login", 80); txtSubmitLog.setPosition(btnSubmitLog.getPosition());

    sf::RectangleShape btnForgot = createButton((width - 600.f) / 2.f, 1100.f, 600.f, 100.f, sf::Color::Yellow);
    sf::Text txtForgot(font, "Forgot Password?", 70); txtForgot.setPosition(btnForgot.getPosition());
    txtForgot.setFillColor(sf::Color::Black);

    // State: Forgot Password Step 1
    sf::Text titleForgot1(font, "Reset Password", 120);
    titleForgot1.setPosition({ width / 2.f - 400.f, 200.f });
    titleForgot1.setFillColor(sf::Color::White);

    TextInput fgEmail(font, { width / 2.f - 250.f, 400.f }, { 500.f, 70.f });
    sf::Text lblFgEmail(font, "Email:", 70); lblFgEmail.setPosition({ width / 2.f - 700.f, 600.f });

    sf::RectangleShape btnSendOTP = createButton((width - 500.f) / 2.f, 800.f, 500.f, 120.f, sf::Color::Green);
    sf::Text txtSendOTP(font, "Send OTP", 80); txtSendOTP.setPosition(btnSendOTP.getPosition());

    // State: Forgot Password Step 2
    sf::Text titleForgot2(font, "Enter OTP & New Password", 100);
    titleForgot2.setPosition({ width / 2.f - 600.f, 200.f });
    titleForgot2.setFillColor(sf::Color::White);

    TextInput fgOTP(font, { width / 2.f - 250.f, 320.f }, { 500.f, 70.f });
    sf::Text lblFgOTP(font, "OTP:", 70); lblFgOTP.setPosition({ width / 2.f - 700.f, 500.f });

    TextInput fgNewPass(font, { width / 2.f - 250.f, 460.f }, { 500.f, 70.f }, true);
    sf::Text lblFgNewPass(font, "New Pass:", 70); lblFgNewPass.setPosition({ width / 2.f - 750.f, 700.f });

    sf::RectangleShape btnSubmitReset = createButton((width - 500.f) / 2.f, 900.f, 500.f, 120.f, sf::Color::Green);
    sf::Text txtSubmitReset(font, "Reset", 80); txtSubmitReset.setPosition(btnSubmitReset.getPosition());

    auto centerTextInButton = [](sf::Text& text, const sf::RectangleShape& btn) {
        sf::FloatRect textBounds = text.getLocalBounds();
        text.setPosition({
            btn.getPosition().x + (btn.getSize().x - textBounds.size.x) / 2.f - textBounds.position.x,
            btn.getPosition().y + (btn.getSize().y - textBounds.size.y) / 2.f - textBounds.position.y
            });
        };

    auto updateUIPositions = [&](unsigned int w, unsigned int h) {
        logoSprite.setPosition({ w / 2.f - logoSprite.getGlobalBounds().size.x / 2.f, 60.f });
        errorText.setPosition({ w / 2.f - 400.f, h - 180.f });
        errorText.setCharacterSize(45);

        // State: MainMenu
        btnPlay.setSize({ 500.f, 110.f });
        btnPlay.setPosition({ (w - 500.f) / 2.f, 520.f });
        txtPlay.setCharacterSize(50);
        centerTextInButton(txtPlay, btnPlay);

        btnSettings.setSize({ 500.f, 110.f });
        btnSettings.setPosition({ (w - 500.f) / 2.f, 670.f });
        txtSettings.setCharacterSize(50);
        centerTextInButton(txtSettings, btnSettings);

        btnExit.setSize({ 500.f, 110.f });
        btnExit.setPosition({ (w - 500.f) / 2.f, 820.f });
        txtExit.setCharacterSize(50);
        centerTextInButton(txtExit, btnExit);

        // State: AuthChoice
        titleAuthChoice.setPosition({ w / 2.f - 400.f, 260.f });
        titleAuthChoice.setCharacterSize(70);

        btnLoginChoice.setSize({ 500.f, 110.f });
        btnLoginChoice.setPosition({ (w - 500.f) / 2.f, 520.f });
        txtLoginChoice.setCharacterSize(50);
        centerTextInButton(txtLoginChoice, btnLoginChoice);

        btnRegisterChoice.setSize({ 500.f, 110.f });
        btnRegisterChoice.setPosition({ (w - 500.f) / 2.f, 670.f });
        txtRegisterChoice.setCharacterSize(50);
        centerTextInButton(txtRegisterChoice, btnRegisterChoice);

        btnBack.setSize({ 220.f, 80.f });
        btnBack.setPosition({ 100.f, 100.f });
        txtBack.setCharacterSize(40);
        centerTextInButton(txtBack, btnBack);

        // State: Register
        titleRegister.setPosition({ w / 2.f - 200.f, 100.f });
        titleRegister.setCharacterSize(80);

        regUser.setPosition({ w / 2.f - 200.f, 260.f });
        lblRegUser.setPosition({ w / 2.f - 550.f, 260.f });
        lblRegUser.setCharacterSize(45);

        regPass.setPosition({ w / 2.f - 200.f, 380.f });
        lblRegPass.setPosition({ w / 2.f - 550.f, 380.f });
        lblRegPass.setCharacterSize(45);

        regConf.setPosition({ w / 2.f - 200.f, 500.f });
        lblRegConf.setPosition({ w / 2.f - 550.f, 500.f });
        lblRegConf.setCharacterSize(45);

        regEmail.setPosition({ w / 2.f - 200.f, 620.f });
        lblRegEmail.setPosition({ w / 2.f - 550.f, 620.f });
        lblRegEmail.setCharacterSize(45);

        btnSubmitReg.setSize({ 360.f, 90.f });
        btnSubmitReg.setPosition({ (w - 360.f) / 2.f, 760.f });
        txtSubmitReg.setCharacterSize(45);
        centerTextInButton(txtSubmitReg, btnSubmitReg);

        // State: Login
        titleLogin.setPosition({ w / 2.f - 120.f, 150.f });
        titleLogin.setCharacterSize(80);

        logUser.setPosition({ w / 2.f - 200.f, 340.f });
        lblLogUser.setPosition({ w / 2.f - 550.f, 340.f });
        lblLogUser.setCharacterSize(45);

        logPass.setPosition({ w / 2.f - 200.f, 480.f });
        lblLogPass.setPosition({ w / 2.f - 550.f, 480.f });
        lblLogPass.setCharacterSize(45);

        btnSubmitLog.setSize({ 360.f, 90.f });
        btnSubmitLog.setPosition({ (w - 360.f) / 2.f, 620.f });
        txtSubmitLog.setCharacterSize(45);
        centerTextInButton(txtSubmitLog, btnSubmitLog);

        btnForgot.setSize({ 420.f, 80.f });
        btnForgot.setPosition({ (w - 420.f) / 2.f, 750.f });
        txtForgot.setCharacterSize(35);
        centerTextInButton(txtForgot, btnForgot);

        // State: Forgot Password Step 1
        titleForgot1.setPosition({ w / 2.f - 300.f, 200.f });
        titleForgot1.setCharacterSize(80);

        fgEmail.setPosition({ w / 2.f - 200.f, 400.f });
        lblFgEmail.setPosition({ w / 2.f - 550.f, 400.f });
        lblFgEmail.setCharacterSize(45);

        btnSendOTP.setSize({ 360.f, 90.f });
        btnSendOTP.setPosition({ (w - 360.f) / 2.f, 540.f });
        txtSendOTP.setCharacterSize(45);
        centerTextInButton(txtSendOTP, btnSendOTP);

        // State: Forgot Password Step 2
        titleForgot2.setPosition({ w / 2.f - 400.f, 150.f });
        titleForgot2.setCharacterSize(80);

        fgOTP.setPosition({ w / 2.f - 200.f, 320.f });
        lblFgOTP.setPosition({ w / 2.f - 550.f, 320.f });
        lblFgOTP.setCharacterSize(45);

        fgNewPass.setPosition({ w / 2.f - 200.f, 460.f });
        lblFgNewPass.setPosition({ w / 2.f - 550.f, 460.f });
        lblFgNewPass.setCharacterSize(45);

        btnSubmitReset.setSize({ 360.f, 90.f });
        btnSubmitReset.setPosition({ (w - 360.f) / 2.f, 600.f });
        txtSubmitReset.setCharacterSize(45);
        centerTextInButton(txtSubmitReset, btnSubmitReset);
        };

    UpdateViewToWindow(window);
    updateUIPositions(1920, 1080);

    // ---- Async login / register state ----
    struct AsyncAuthResult {
        std::uint64_t requestId = 0;
        bool success = false;
        std::string errorMsg;
        std::string username;
        PlayerStats stats{};
    };
    std::atomic<bool>   authDone(false);
    AsyncAuthResult     authResult;
    std::mutex          authResultMutex;   // guards authResult reads/writes
    sf::Clock           authClock;
    sf::Clock           authDotClock;
    int                 authDotCount = 0;
    std::atomic<std::uint64_t> authRequestCounter(0);
    std::uint64_t       activeAuthRequestId = 0;
    constexpr float     AUTH_TIMEOUT_SECS = 12.0f;

    // ---- Loading game state ----
    std::string         loadingUsername;
    bool                loadingIsFirstTime;
    sf::Clock           loadingClock;
    sf::Clock           loadingDotClock;
    sf::Clock           loadingCompleteClock;
    int                 loadingDotCount = 0;
    bool                loadingCompleteShown = false;
    bool                launchGameRequested = false;
    std::string         launchGameUsername;
    bool                launchGameIsFirstTime = false;

    auto startLoginAsync = [&](const std::string& u, const std::string& p) {
        authDone = false;
        activeAuthRequestId = ++authRequestCounter;
        {
            std::lock_guard<std::mutex> lk(authResultMutex);
            authResult = {};
        }
        authClock.restart();
        authDotClock.restart();
        authDotCount = 0;
        std::thread([&, u, p, reqId = activeAuthRequestId]() {
            AsyncAuthResult r;
            r.requestId = reqId;
            r.username = u;
            if (AuthManager::getInstance().loginUser(
                sql::SQLString(u.c_str()), sql::SQLString(p.c_str()), r.errorMsg)) {
                r.stats = AuthManager::getInstance().loadPlayerStats(sql::SQLString(u.c_str()));
                r.success = true;
            }
            {
                std::lock_guard<std::mutex> lk(authResultMutex);
                authResult = r;
            }
            authDone = true;  // written last – main thread reads authResult only after this
            }).detach();
        };

    auto startRegisterAsync = [&](const std::string& u, const std::string& p, const std::string& e) {
        authDone = false;
        activeAuthRequestId = ++authRequestCounter;
        {
            std::lock_guard<std::mutex> lk(authResultMutex);
            authResult = {};
        }
        authClock.restart();
        authDotClock.restart();
        authDotCount = 0;
        std::thread([&, u, p, e, reqId = activeAuthRequestId]() {
            AsyncAuthResult r;
            r.requestId = reqId;
            r.username = u;
            if (AuthManager::getInstance().registerUser(
                sql::SQLString(u.c_str()), sql::SQLString(p.c_str()), sql::SQLString(e.c_str()))) {
                r.stats = AuthManager::getInstance().loadPlayerStats(sql::SQLString(u.c_str()));
                r.success = true;
            }
            else {
                r.errorMsg = "Registration failed! Username may already exist.";
            }
            {
                std::lock_guard<std::mutex> lk(authResultMutex);
                authResult = r;
            }
            authDone = true;
            }).detach();
        };
    // ---- end async auth state ----

    while (window->isOpen()) {
        while (const std::optional event = window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window->close();
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                    // In form states go back instead of closing window
                    if (state == MenuState::Register || state == MenuState::Login) {
                        state = MenuState::AuthChoice;
                        errorMessage = "";
                    } else if (state == MenuState::ForgotPasswordStep1 || state == MenuState::ForgotPasswordStep2) {
                        state = MenuState::Login;
                        errorMessage = "";
                    } else if (state == MenuState::AuthChoice) {
                        state = MenuState::MainMenu;
                    } else {
                        window->close();
                    }
                }
                // Tab: cycle focus through Register fields
                else if (keyPressed->scancode == sf::Keyboard::Scancode::Tab
                      && state == MenuState::Register) {
                    if      (regUser.hasFocus())  { regUser.setFocus(false); regPass.setFocus(true); }
                    else if (regPass.hasFocus())  { regPass.setFocus(false); regConf.setFocus(true); }
                    else if (regConf.hasFocus())  { regConf.setFocus(false); regEmail.setFocus(true); }
                    else                          { regEmail.setFocus(false); regUser.setFocus(true); }
                }
                // Tab: cycle focus through Login fields
                else if (keyPressed->scancode == sf::Keyboard::Scancode::Tab
                      && state == MenuState::Login) {
                    if (logUser.hasFocus()) { logUser.setFocus(false); logPass.setFocus(true); }
                    else                   { logPass.setFocus(false); logUser.setFocus(true); }
                }
                // Enter: submit Register form
                else if (keyPressed->scancode == sf::Keyboard::Scancode::Enter
                      && state == MenuState::Register) {
                    std::string u = regUser.getText(), p = regPass.getText();
                    std::string c = regConf.getText(), e = regEmail.getText();
                    if (u.empty() || p.empty() || c.empty() || e.empty()) {
                        errorMessage = "Fields cannot be blank!";
                    } else if (p != c) {
                        errorMessage = "Passwords do not match!";
                    } else if (!isValidPassword(p)) {
                        errorMessage = "Password needs 1 number, 1 letter, 1 symbol!";
                    } else {
                        startRegisterAsync(u, p, e);
                        state = MenuState::Registering;
                        errorMessage = "";
                    }
                }
                // Enter: submit Login form
                else if (keyPressed->scancode == sf::Keyboard::Scancode::Enter
                      && state == MenuState::Login) {
                    std::string u = logUser.getText(), p = logPass.getText();
                    if (u.empty() || p.empty()) {
                        errorMessage = "Fields cannot be blank!";
                    } else {
                        startLoginAsync(u, p);
                        state = MenuState::LoggingIn;
                        errorMessage = "";
                    }
                }
            }

            sf::Vector2f mpos = window->mapPixelToCoords(sf::Mouse::getPosition(*window), GetUIView(window));

            // Distribute events to text inputs
            if (state == MenuState::Register) {
                regUser.processEvent(*event, mpos);
                regPass.processEvent(*event, mpos);
                regConf.processEvent(*event, mpos);
                regEmail.processEvent(*event, mpos);
            }
            else if (state == MenuState::Login) {
                logUser.processEvent(*event, mpos);
                logPass.processEvent(*event, mpos);
            }
            else if (state == MenuState::ForgotPasswordStep1) {
                fgEmail.processEvent(*event, mpos);
            }
            else if (state == MenuState::ForgotPasswordStep2) {
                fgOTP.processEvent(*event, mpos);
                fgNewPass.processEvent(*event, mpos);
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {

                    if (state == MenuState::MainMenu) {
                        if (btnPlay.getGlobalBounds().contains(mpos)) {
                            state = MenuState::AuthChoice;
                            errorMessage = "";
                        }
                        else if (btnSettings.getGlobalBounds().contains(mpos)) {
                            unsigned int tempW = window->getSize().x;
                            unsigned int tempH = window->getSize().y;
                            openSettingsWindow(*window, tempW, tempH);
                            if (bgSprite.getLocalBounds().size.x > 0 && bgSprite.getLocalBounds().size.y > 0) {
                                bgSprite.setScale({
                                    1920.f / bgSprite.getLocalBounds().size.x,
                                    1080.f / bgSprite.getLocalBounds().size.y
                                    });
                            }
                            UpdateViewToWindow(window);
                            updateUIPositions(1920, 1080);
                        }
                        else if (btnExit.getGlobalBounds().contains(mpos)) {
                            window->close();
                        }
                    }
                    else if (state == MenuState::AuthChoice) {
                        if (btnBack.getGlobalBounds().contains(mpos)) {
                            state = MenuState::MainMenu;
                        }
                        else if (btnLoginChoice.getGlobalBounds().contains(mpos)) {
                            state = MenuState::Login;
                            errorMessage = "";
                            logUser.clear();
                            logPass.clear();
                        }
                        else if (btnRegisterChoice.getGlobalBounds().contains(mpos)) {
                            state = MenuState::Register;
                            errorMessage = "";
                            regUser.clear();
                            regPass.clear();
                            regConf.clear();
                            regEmail.clear();
                        }
                    }
                    else if (state == MenuState::Register) {
                        if (btnBack.getGlobalBounds().contains(mpos)) {
                            state = MenuState::AuthChoice;
                        }
                        else if (btnSubmitReg.getGlobalBounds().contains(mpos)) {
                            std::string u = regUser.getText();
                            std::string p = regPass.getText();
                            std::string c = regConf.getText();
                            std::string e = regEmail.getText();

                            if (u.empty() || p.empty() || c.empty() || e.empty()) {
                                errorMessage = "Fields cannot be blank!";
                            }
                            else if (p != c) {
                                errorMessage = "Passwords do not match!";
                            }
                            else if (!isValidPassword(p)) {
                                errorMessage = "Password needs 1 number, 1 letter, 1 symbol!";
                            }
                            else {
                                // Kick off async � don't block the render thread
                                startRegisterAsync(u, p, e);
                                state = MenuState::Registering;
                                errorMessage = "";
                            }
                        }
                    }
                    else if (state == MenuState::Login) {
                        if (btnBack.getGlobalBounds().contains(mpos)) {
                            state = MenuState::AuthChoice;
                        }
                        else if (btnSubmitLog.getGlobalBounds().contains(mpos)) {
                            std::string u = logUser.getText();
                            std::string p = logPass.getText();
                            if (u.empty() || p.empty()) {
                                errorMessage = "Fields cannot be blank!";
                            }
                            else {
                                // Kick off async � don't block the render thread
                                startLoginAsync(u, p);
                                state = MenuState::LoggingIn;
                                errorMessage = "";
                            }
                        }
                        else if (btnForgot.getGlobalBounds().contains(mpos)) {
                            state = MenuState::ForgotPasswordStep1;
                            errorMessage = "";
                            fgEmail.clear();
                        }
                    }
                    else if (state == MenuState::ForgotPasswordStep1) {
                        if (btnBack.getGlobalBounds().contains(mpos)) {
                            state = MenuState::Login;
                            errorMessage = "";
                        }
                        else if (btnSendOTP.getGlobalBounds().contains(mpos)) {
                            std::string e = fgEmail.getText();
                            if (e.empty()) {
                                errorMessage = "Email cannot be blank!";
                            }
                            else if (!otpPending.load()) {
                                // Launch async – do NOT call generateForgotOTP on the render thread
                                currentEmailForReset = e;
                                otpDone = false;
                                otpPending = true;
                                errorMessage = "Sending OTP...";
                                std::thread([&, e]() {
                                    sql::SQLString otpSQL = AuthManager::getInstance().generateForgotOTP(sql::SQLString(e.c_str()));
                                    std::string otp(otpSQL.c_str());
                                    {
                                        std::lock_guard<std::mutex> lk(otpMutex);
                                        otpResult = otp;
                                    }
                                    otpDone = true;
                                    otpPending = false;
                                    }).detach();
                            }
                        }
                    }
                    else if (state == MenuState::ForgotPasswordStep2) {
                        if (btnBack.getGlobalBounds().contains(mpos)) {
                            state = MenuState::ForgotPasswordStep1;
                            errorMessage = "";
                        }
                        else if (btnSubmitReset.getGlobalBounds().contains(mpos)) {
                            std::string o = fgOTP.getText();
                            std::string np = fgNewPass.getText();

                            if (o.empty() || np.empty()) {
                                errorMessage = "Fields cannot be blank!";
                            }
                            else if (!isValidPassword(np)) {
                                errorMessage = "Password needs 1 number, 1 letter, 1 symbol!";
                            }
                            else {
                                if (AuthManager::getInstance().resetPassword(sql::SQLString(currentEmailForReset.c_str()), sql::SQLString(o.c_str()), sql::SQLString(np.c_str()))) {
                                    state = MenuState::Login;
                                    errorMessage = "Password Reset Success! Please login.";
                                    logUser.clear();
                                    logPass.clear();
                                }
                                else {
                                    errorMessage = "Invalid OTP!";
                                }
                            }
                        }
                    }

                }
            }
        }

        // Execute game launch outside menu rendering branch to avoid re-entrant render transition stalls.
        if (launchGameRequested) {
            {
                std::ofstream f("mainmenu_transition_trace.log", std::ios::app);
                if (f.is_open()) f << "before_gameLoop\n";
            }
            gameLoop(*window, width, height, launchGameUsername, launchGameIsFirstTime);
            {
                std::ofstream f("mainmenu_transition_trace.log", std::ios::app);
                if (f.is_open()) f << "after_gameLoop\n";
            }
            launchGameRequested = false;
            state = MenuState::MainMenu;
            continue;
        }

        sf::Vector2f currentMpos = window->mapPixelToCoords(sf::Mouse::getPosition(*window), GetUIView(window));

        // ---- Poll async OTP result ----
        if (otpDone.load()) {
            otpDone = false;
            std::string otp;
            {
                std::lock_guard<std::mutex> lk(otpMutex);
                otp = otpResult;
            }
            if (!otp.empty()) {
                currentOTP = otp;
                state = MenuState::ForgotPasswordStep2;
                errorMessage = "";
                fgOTP.clear();
                fgNewPass.clear();
            }
            else {
                errorMessage = "Email not found!";
            }
        }

        // Handle button hover styling
        auto handleHover = [&](sf::RectangleShape& btn, sf::Color normalColor, sf::Color hoverColor) {
            if (btn.getGlobalBounds().contains(currentMpos)) {
                btn.setFillColor(hoverColor);
                btn.setOutlineThickness(4.f);
                btn.setOutlineColor(sf::Color::White);
            }
            else {
                btn.setFillColor(normalColor);
                btn.setOutlineThickness(0.f);
            }
            };

        window->setView(GetUIView(window));
        window->clear(sf::Color(30, 30, 30));
        window->draw(bgSprite);
        errorText.setString(errorMessage);

        if (state == MenuState::MainMenu) {
            handleHover(btnPlay, sf::Color(39, 174, 96), sf::Color(46, 204, 113));
            handleHover(btnSettings, sf::Color(39, 174, 96), sf::Color(46, 204, 113));
            handleHover(btnExit, sf::Color(192, 57, 43), sf::Color(231, 76, 60));

            window->draw(logoSprite);
            window->draw(btnPlay); window->draw(txtPlay);
            window->draw(btnSettings); window->draw(txtSettings);
            window->draw(btnExit); window->draw(txtExit);
        }
        else if (state == MenuState::AuthChoice) {
            handleHover(btnLoginChoice, sf::Color(41, 128, 185), sf::Color(52, 152, 219));
            handleHover(btnRegisterChoice, sf::Color(41, 128, 185), sf::Color(52, 152, 219));
            handleHover(btnBack, sf::Color(127, 140, 141), sf::Color(149, 165, 166));

            window->draw(titleAuthChoice);
            window->draw(btnLoginChoice); window->draw(txtLoginChoice);
            window->draw(btnRegisterChoice); window->draw(txtRegisterChoice);
            window->draw(btnBack); window->draw(txtBack);
        }
        else if (state == MenuState::Register) {
            handleHover(btnSubmitReg, sf::Color(39, 174, 96), sf::Color(46, 204, 113));
            handleHover(btnBack, sf::Color(127, 140, 141), sf::Color(149, 165, 166));

            window->draw(titleRegister);
            window->draw(lblRegUser); regUser.draw(*window);
            window->draw(lblRegPass); regPass.draw(*window);
            window->draw(lblRegConf); regConf.draw(*window);
            window->draw(lblRegEmail); regEmail.draw(*window);
            window->draw(btnSubmitReg); window->draw(txtSubmitReg);
            window->draw(btnBack); window->draw(txtBack);
            window->draw(errorText);
        }
        else if (state == MenuState::Login) {
            handleHover(btnSubmitLog, sf::Color(39, 174, 96), sf::Color(46, 204, 113));
            handleHover(btnForgot, sf::Color(241, 196, 15), sf::Color(243, 156, 18));
            handleHover(btnBack, sf::Color(127, 140, 141), sf::Color(149, 165, 166));

            window->draw(titleLogin);
            window->draw(lblLogUser); logUser.draw(*window);
            window->draw(lblLogPass); logPass.draw(*window);
            window->draw(btnSubmitLog); window->draw(txtSubmitLog);
            window->draw(btnForgot); window->draw(txtForgot);
            window->draw(btnBack); window->draw(txtBack);
            window->draw(errorText);
        }
        else if (state == MenuState::ForgotPasswordStep1) {
            handleHover(btnSendOTP, sf::Color(39, 174, 96), sf::Color(46, 204, 113));
            handleHover(btnBack, sf::Color(127, 140, 141), sf::Color(149, 165, 166));

            window->draw(titleForgot1);
            window->draw(lblFgEmail); fgEmail.draw(*window);
            window->draw(btnSendOTP); window->draw(txtSendOTP);
            window->draw(btnBack); window->draw(txtBack);
            window->draw(errorText);
        }
        else if (state == MenuState::ForgotPasswordStep2) {
            handleHover(btnSubmitReset, sf::Color(39, 174, 96), sf::Color(46, 204, 113));
            handleHover(btnBack, sf::Color(127, 140, 141), sf::Color(149, 165, 166));

            window->draw(titleForgot2);
            window->draw(lblFgOTP); fgOTP.draw(*window);
            window->draw(lblFgNewPass); fgNewPass.draw(*window);
            window->draw(btnSubmitReset); window->draw(txtSubmitReset);
            window->draw(btnBack); window->draw(txtBack);
            window->draw(errorText);
        }
        else if (state == MenuState::LoggingIn || state == MenuState::Registering) {
            bool isReg = (state == MenuState::Registering);

            // ---- Poll async result ----
            if (authDone.load()) {
                AsyncAuthResult localResult;
                {
                    std::lock_guard<std::mutex> lk(authResultMutex);
                    localResult = authResult;
                }
                authDone = false;

                // Ignore stale completion from an older timed-out request.
                if (localResult.requestId != activeAuthRequestId) {
                    // Keep waiting for the active request only.
                }
                else if (localResult.success) {
                    state = MenuState::LoadingGame;
                    loadingClock.restart();
                    loadingDotClock.restart();
                    loadingDotCount = 0;
                    loadingCompleteShown = false;
                    loadingUsername = localResult.username;
                    loadingIsFirstTime = localResult.stats.isFirstTime;
                }
                else {
                    errorMessage = localResult.errorMsg.empty()
                        ? (isReg ? "Registration failed! Username may exist." : "Invalid Credentials!")
                        : localResult.errorMsg;
                    state = isReg ? MenuState::Register : MenuState::Login;
                }
            }
            else if (authClock.getElapsedTime().asSeconds() > AUTH_TIMEOUT_SECS) {
                errorMessage = (isReg ? "Registration request timed out. Please try again."
                    : "Login request timed out. Please try again.");
                state = isReg ? MenuState::Register : MenuState::Login;
            }

            // ---- Draw progress bar screen ----
            float elapsed = authClock.getElapsedTime().asSeconds();

            // Bar fills quickly to 85% then holds – never goes backward
            constexpr float FILL_DURATION = 1.2f;
            float fillProg = (elapsed < FILL_DURATION)
                ? (elapsed / FILL_DURATION) * 0.85f
                : 0.85f;

            float winW = static_cast<float>(width);
            float winH = static_cast<float>(height);
            float barW = winW * 0.45f;
            float barH = 22.f;
            float barX = winW / 2.f - barW / 2.f;
            float barY = winH / 2.f + 30.f;

            // Background bar
            sf::RectangleShape authBarBg({ barW, barH });
            authBarBg.setFillColor(sf::Color(60, 60, 60));
            authBarBg.setPosition({ barX, barY });
            window->draw(authBarBg);

            // Filled portion – only ever grows
            sf::RectangleShape authBarFg({ barW * fillProg, barH });
            authBarFg.setFillColor(sf::Color(52, 152, 219));
            authBarFg.setPosition({ barX, barY });
            window->draw(authBarFg);

            // Shimmer pulse sweeps left-to-right across the filled region
            if (barW * fillProg > 10.f) {
                float shimmerPhase = std::fmod(elapsed * 1.2f, 1.f); // 0→1, loops
                float shimmerX = barX + shimmerPhase * (barW * fillProg);
                float shimmerW = barW * 0.18f; // ~18% width shimmer band
                // clamp so it doesn't overflow the filled area
                shimmerW = std::min(shimmerW, (barX + barW * fillProg) - shimmerX);
                if (shimmerW > 0.f) {
                    sf::RectangleShape shimmer({ shimmerW, barH });
                    shimmer.setFillColor(sf::Color(255, 255, 255, 55));
                    shimmer.setPosition({ shimmerX, barY });
                    window->draw(shimmer);
                }
            }

            // Animated dots
            if (authDotClock.getElapsedTime().asSeconds() > 0.4f) {
                authDotCount = (authDotCount + 1) % 4;
                authDotClock.restart();
            }
            std::string labelStr = (isReg ? "Creating account" : "Logging in")
                + std::string(authDotCount, '.');
            sf::Text authLabel(font, labelStr, 48);
            authLabel.setFillColor(sf::Color::White);
            authLabel.setOutlineColor(sf::Color::Black);
            authLabel.setOutlineThickness(2.f);
            authLabel.setPosition({
                winW / 2.f - authLabel.getLocalBounds().size.x / 2.f,
                winH / 2.f - 50.f
                });
            window->draw(authLabel);

            sf::Text subLabel(font, "Please wait...", 28);
            subLabel.setFillColor(sf::Color(180, 180, 180));
            subLabel.setPosition({
                winW / 2.f - subLabel.getLocalBounds().size.x / 2.f,
                barY + barH + 14.f
                });
            window->draw(subLabel);
        }
        else if (state == MenuState::LoadingGame) {
            // ---- Draw loading game screen ----
            float elapsed = loadingClock.getElapsedTime().asSeconds();
            const float LOAD_DURATION = 2.4f;
            const float COMPLETE_HOLD_SECS = 0.8f;
            const std::string stageMessages[] = {
                "Preparing player profile...",
                "Preparing inventory...",
                "Preparing world objects...",
                "Finalizing..."
            };
            int stageIdx = std::min(3, std::max(0, static_cast<int>((elapsed / LOAD_DURATION) * 4.0f)));

            if (!loadingCompleteShown && elapsed >= LOAD_DURATION) {
                loadingCompleteShown = true;
                loadingCompleteClock.restart();
            }

            if (loadingCompleteShown && loadingCompleteClock.getElapsedTime().asSeconds() > COMPLETE_HOLD_SECS) {
                // Queue transition and execute at top of next tick.
                launchGameRequested = true;
                launchGameUsername = loadingUsername;
                launchGameIsFirstTime = loadingIsFirstTime;
                state = MenuState::MainMenu;
            }

            float winW = static_cast<float>(width);
            float winH = static_cast<float>(height);

            // Animated dots
            if (loadingDotClock.getElapsedTime().asSeconds() > 0.4f) {
                loadingDotCount = (loadingDotCount + 1) % 4;
                loadingDotClock.restart();
            }
            std::string labelStr = loadingCompleteShown
                ? "Loading complete!"
                : ("Loading game" + std::string(loadingDotCount, '.'));
            sf::Text loadLabel(font, labelStr, 48);
            loadLabel.setFillColor(sf::Color::White);
            loadLabel.setOutlineColor(sf::Color::Black);
            loadLabel.setOutlineThickness(2.f);
            loadLabel.setPosition({
                winW / 2.f - loadLabel.getLocalBounds().size.x / 2.f,
                winH / 2.f - 50.f
                });
            window->draw(loadLabel);

            sf::Text subLabel(
                font,
                loadingCompleteShown
                ? "Starting your world now..."
                : stageMessages[stageIdx],
                28
            );
            subLabel.setFillColor(sf::Color(180, 180, 180));
            subLabel.setPosition({
                winW / 2.f - subLabel.getLocalBounds().size.x / 2.f,
                winH / 2.f + 20.f
                });
            window->draw(subLabel);

            float progress = loadingCompleteShown
                ? 1.0f
                : std::min(1.0f, elapsed / LOAD_DURATION);
            float barW = winW * 0.4f;
            float barH = 18.f;
            float barX = winW / 2.f - barW / 2.f;
            float barY = winH / 2.f + 70.f;

            sf::RectangleShape barBg({ barW, barH });
            barBg.setFillColor(sf::Color(60, 60, 60));
            barBg.setPosition({ barX, barY });
            window->draw(barBg);

            sf::RectangleShape barFg({ barW * progress, barH });
            barFg.setFillColor(loadingCompleteShown ? sf::Color(46, 204, 113) : sf::Color(52, 152, 219));
            barFg.setPosition({ barX, barY });
            window->draw(barFg);
        }

        window->display();
    }
    delete window;

    AuthManager::getInstance().shutdownDB();
    return 0;
}




