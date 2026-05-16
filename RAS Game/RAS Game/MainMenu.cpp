#include <iostream>
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

int main() {
    std::cout << "[DEBUG] main() started. Entering AuthManager." << std::endl;
    AuthManager::getInstance().initDB();
    std::cout << "[DEBUG] Returned from initDB() successfully!" << std::endl;

    unsigned int width = 3840;
    unsigned int height = 2160;
    sf::RenderWindow* window = new sf::RenderWindow(sf::VideoMode({ width, height }), "RAS Game");

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        std::cout << "Failed to load font!\n";
        return -1;
    }

    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile("C:/Users/DELL/source/repos/Project/RAS Game/Main Menu/Background.png")) {
        std::cout << "Failed to load background!\n";
    }
    sf::Sprite bgSprite(bgTexture);
    bgSprite.setScale({ width / bgSprite.getLocalBounds().size.x, height / bgSprite.getLocalBounds().size.y });

    sf::Texture logoTexture;
    if (!logoTexture.loadFromFile("C:/Users/DELL/source/repos/Project/RAS Game/Main Menu/Logo.jpg")) {
        std::cout << "Failed to load logo!\n";
    }
    sf::Sprite logoSprite(logoTexture);
    logoSprite.setScale({ 2.f, 2.f });
    logoSprite.setPosition({ width / 2.f - logoSprite.getGlobalBounds().size.x / 2.f, 100.f });

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

    TextInput regUser(font, { width / 2.f - 400.f, 400.f }, { 800.f, 100.f });
    sf::Text lblRegUser(font, "Username:", 70); lblRegUser.setPosition({ width / 2.f - 800.f, 400.f });

    TextInput regPass(font, { width / 2.f - 400.f, 600.f }, { 800.f, 100.f }, true);
    sf::Text lblRegPass(font, "Password:", 70); lblRegPass.setPosition({ width / 2.f - 800.f, 600.f });

    TextInput regConf(font, { width / 2.f - 400.f, 800.f }, { 800.f, 100.f }, true);
    sf::Text lblRegConf(font, "Confirm:", 70); lblRegConf.setPosition({ width / 2.f - 800.f, 800.f });

    TextInput regEmail(font, { width / 2.f - 400.f, 1000.f }, { 800.f, 100.f });
    sf::Text lblRegEmail(font, "Email:", 70); lblRegEmail.setPosition({ width / 2.f - 800.f, 1000.f });

    sf::RectangleShape btnSubmitReg = createButton((width - 400.f) / 2.f, 1200.f, 400.f, 120.f, sf::Color::Green);
    sf::Text txtSubmitReg(font, "Submit", 80); txtSubmitReg.setPosition(btnSubmitReg.getPosition());

    // State: Login
    sf::Text titleLogin(font, "Login", 120);
    titleLogin.setPosition({ width / 2.f - 200.f, 200.f });
    titleLogin.setFillColor(sf::Color::White);

    TextInput logUser(font, { width / 2.f - 400.f, 500.f }, { 800.f, 100.f });
    sf::Text lblLogUser(font, "Username:", 70); lblLogUser.setPosition({ width / 2.f - 800.f, 500.f });

    TextInput logPass(font, { width / 2.f - 400.f, 700.f }, { 800.f, 100.f }, true);
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

    TextInput fgEmail(font, { width / 2.f - 400.f, 600.f }, { 800.f, 100.f });
    sf::Text lblFgEmail(font, "Email:", 70); lblFgEmail.setPosition({ width / 2.f - 700.f, 600.f });

    sf::RectangleShape btnSendOTP = createButton((width - 500.f) / 2.f, 800.f, 500.f, 120.f, sf::Color::Green);
    sf::Text txtSendOTP(font, "Send OTP", 80); txtSendOTP.setPosition(btnSendOTP.getPosition());

    // State: Forgot Password Step 2
    sf::Text titleForgot2(font, "Enter OTP & New Password", 100);
    titleForgot2.setPosition({ width / 2.f - 600.f, 200.f });
    titleForgot2.setFillColor(sf::Color::White);

    TextInput fgOTP(font, { width / 2.f - 400.f, 500.f }, { 800.f, 100.f });
    sf::Text lblFgOTP(font, "OTP:", 70); lblFgOTP.setPosition({ width / 2.f - 700.f, 500.f });

    TextInput fgNewPass(font, { width / 2.f - 400.f, 700.f }, { 800.f, 100.f }, true);
    sf::Text lblFgNewPass(font, "New Pass:", 70); lblFgNewPass.setPosition({ width / 2.f - 750.f, 700.f });

    sf::RectangleShape btnSubmitReset = createButton((width - 500.f) / 2.f, 900.f, 500.f, 120.f, sf::Color::Green);
    sf::Text txtSubmitReset(font, "Reset", 80); txtSubmitReset.setPosition(btnSubmitReset.getPosition());

    UpdateViewToWindow(window);

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
            
            sf::Vector2f mpos = window->mapPixelToCoords(sf::Mouse::getPosition(*window));

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
                            openSettingsWindow(*window, width, height);
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
                                if (AuthManager::getInstance().loginUser(sql::SQLString(u.c_str()), sql::SQLString(p.c_str()))) {
                                    PlayerStats stats = AuthManager::getInstance().loadPlayerStats(sql::SQLString(u.c_str()));
                                    gameLoop(*window, width, height, u, stats.isFirstTime);
                                    state = MenuState::MainMenu;
                                } else {
                                    errorMessage = "Invalid Credentials!";
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

        window->clear(sf::Color(30,30,30));
        window->draw(bgSprite);
        errorText.setString(errorMessage);

        if (state == MenuState::MainMenu) {
            window->draw(logoSprite);
            window->draw(btnPlay); window->draw(txtPlay);
            window->draw(btnSettings); window->draw(txtSettings);
            window->draw(btnExit); window->draw(txtExit);
        }
        else if (state == MenuState::AuthChoice) {
            window->draw(titleAuthChoice);
            window->draw(btnLoginChoice); window->draw(txtLoginChoice);
            window->draw(btnRegisterChoice); window->draw(txtRegisterChoice);
            window->draw(btnBack); window->draw(txtBack);
        }
        else if (state == MenuState::Register) {
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
            window->draw(titleLogin);
            window->draw(lblLogUser); logUser.draw(*window);
            window->draw(lblLogPass); logPass.draw(*window);
            window->draw(btnSubmitLog); window->draw(txtSubmitLog);
            window->draw(btnForgot); window->draw(txtForgot);
            window->draw(btnBack); window->draw(txtBack);
            window->draw(errorText);
        }
        else if (state == MenuState::ForgotPasswordStep1) {
            window->draw(titleForgot1);
            window->draw(lblFgEmail); fgEmail.draw(*window);
            window->draw(btnSendOTP); window->draw(txtSendOTP);
            window->draw(btnBack); window->draw(txtBack);
            window->draw(errorText);
        }
        else if (state == MenuState::ForgotPasswordStep2) {
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

