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

    std::cout << "[DEBUG] main() started. Entering AuthManager." << std::endl;
    AuthManager::getInstance().initDB();
    std::cout << "[DEBUG] Returned from initDB() successfully!" << std::endl;

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
        resolutions.push_back({windowWidth, windowHeight});
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

    while (window->isOpen()) {
        while (const std::optional event = window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window->close();
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                    window->close();
                }
            }
            
            sf::Vector2f mpos = window->mapPixelToCoords(sf::Mouse::getPosition(*window), GetUIView(window));

            // Distribute events
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
                            } else if (p != c) {
                                errorMessage = "Passwords do not match!";
                            } else if (!isValidPassword(p)) {
                                errorMessage = "Password needs 1 number, 1 letter, 1 symbol!";
                            } else {
                                if (AuthManager::getInstance().registerUser(sql::SQLString(u.c_str()), sql::SQLString(p.c_str()), sql::SQLString(e.c_str()))) {
                                    // Success
                                    PlayerStats stats = AuthManager::getInstance().loadPlayerStats(sql::SQLString(u.c_str()));
                                    gameLoop(*window, width, height, u, stats.isFirstTime);
                                    state = MenuState::MainMenu; // When game finishes, go back to main
                                } else {
                                    errorMessage = "Registration failed! Username may exist.";
                                }
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
                            } else {
                                std::string banErr;
                                if (AuthManager::getInstance().loginUser(sql::SQLString(u.c_str()), sql::SQLString(p.c_str()), banErr)) {
                                    PlayerStats stats = AuthManager::getInstance().loadPlayerStats(sql::SQLString(u.c_str()));
                                    gameLoop(*window, width, height, u, stats.isFirstTime);
                                    state = MenuState::MainMenu;
                                } else {
                                    errorMessage = banErr.empty() ? "Invalid Credentials!" : banErr;
                                }
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
                            } else {
                                sql::SQLString otpSQL = AuthManager::getInstance().generateForgotOTP(sql::SQLString(e.c_str()));
                                std::string otp(otpSQL.c_str());
                                if (!otp.empty()) {
                                    currentEmailForReset = e;
                                    currentOTP = otp;
                                    state = MenuState::ForgotPasswordStep2;
                                    errorMessage = "";
                                    fgOTP.clear();
                                    fgNewPass.clear();
                                    std::cout << "OTP has been generated for " << e << "!\n";
                                } else {
                                    errorMessage = "Email not found!";
                                }
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
                            } else if (!isValidPassword(np)) {
                                errorMessage = "Password needs 1 number, 1 letter, 1 symbol!";
                            } else {
                                if (AuthManager::getInstance().resetPassword(sql::SQLString(currentEmailForReset.c_str()), sql::SQLString(o.c_str()), sql::SQLString(np.c_str()))) {
                                    state = MenuState::Login;
                                    errorMessage = "Password Reset Success! Please login.";
                                    logUser.clear();
                                    logPass.clear();
                                } else {
                                    errorMessage = "Invalid OTP!";
                                }
                            }
                        }
                    }

                }
            }
        }

        sf::Vector2f currentMpos = window->mapPixelToCoords(sf::Mouse::getPosition(*window), GetUIView(window));

        // Handle button hover styling
        auto handleHover = [&](sf::RectangleShape& btn, sf::Color normalColor, sf::Color hoverColor) {
            if (btn.getGlobalBounds().contains(currentMpos)) {
                btn.setFillColor(hoverColor);
                btn.setOutlineThickness(4.f);
                btn.setOutlineColor(sf::Color::White);
            } else {
                btn.setFillColor(normalColor);
                btn.setOutlineThickness(0.f);
            }
        };

        window->setView(GetUIView(window));
        window->clear(sf::Color(30,30,30));
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

        window->display();
    }
    delete window;
    
    AuthManager::getInstance().shutdownDB();
    return 0;
}

