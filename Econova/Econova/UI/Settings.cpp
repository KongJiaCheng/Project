#include "Settings.h"
#include "ViewUtils.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

GameSettings globalSettings;

bool isMouseInside(const sf::RectangleShape& box, sf::RenderWindow& window)
{
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    sf::Vector2f worldPos = window.mapPixelToCoords(mousePos);
    return box.getGlobalBounds().contains(worldPos);
}

void openSettingsWindow(sf::RenderWindow& window, unsigned int& width, unsigned int& height)
{
    sf::View oldView = window.getView();
    UpdateViewToWindow(&window);

    sf::Font font;
    font.openFromFile("C:/Windows/Fonts/arial.ttf");

    sf::Texture bgTexture;
    bgTexture.loadFromFile(resolvePath("MainMenu/Background.png"));
    sf::Sprite bgSprite(bgTexture);
    if (bgSprite.getLocalBounds().size.x > 0 && bgSprite.getLocalBounds().size.y > 0) {
        bgSprite.setScale({
            1920.f / bgSprite.getLocalBounds().size.x,
            1080.f / bgSprite.getLocalBounds().size.y
        });
    }

    int& volume = globalSettings.volume;
	int& brightness = globalSettings.brightness;
	int& resolutionIndex = globalSettings.resolutionIndex;

    std::vector<sf::Vector2u> resolutions = {
        {1280, 720},
        {1920, 1080},
        {2560, 1440},
        {3840, 2160}
    };

    sf::Vector2u desktopSize = sf::VideoMode::getDesktopMode().size;
    bool foundDesktop = false;
    for (const auto& res : resolutions) {
        if (res == desktopSize) {
            foundDesktop = true;
            break;
        }
    }
    if (!foundDesktop) {
        resolutions.push_back(desktopSize);
        std::sort(resolutions.begin(), resolutions.end(), [](const sf::Vector2u& a, const sf::Vector2u& b) {
            return (a.x * a.y) < (b.x * b.y);
        });
    }

    const int maxRes = static_cast<int>(resolutions.size());

    float logicalW = 1920.f;
    float logicalH = 1080.f;
    float centerX = logicalW / 2.f;
    float centerY = logicalH / 6.f;
    float sectionGap = 140.f;

    sf::Text title(font, "Settings", 120);
    title.setFillColor(sf::Color::Yellow);
    title.setPosition({
	    centerX - title.getGlobalBounds().size.x / 2.f, 20.f
    });

    float leftColX = centerX - 350.f;

    auto makeBar = [&](float x, float y) {
        sf::RectangleShape bar(sf::Vector2f(500.f, 30.f));
        bar.setFillColor(sf::Color(80, 80, 80));
        bar.setPosition({ x, y });
        return bar;
    };

    auto makeKnob = [&](float x, float y, float percent) {
        sf::RectangleShape knob(sf::Vector2f(30.f, 60.f));
        knob.setFillColor(sf::Color::White);
		float px = (percent / 100.f) * 500.f;
        knob.setPosition({ x + px - 15.f, y - 15.f });
        return knob;
    };

    // Top section: Volume, Brightness, Resolution
	sf::Text volumeLabel(font, "Volume: ", 60);
    volumeLabel.setPosition({ leftColX, centerY });

    sf::RectangleShape volumeBar = makeBar(leftColX + 350.f, centerY + 25.f);
	sf::RectangleShape volumeKnob = makeKnob(leftColX + 350.f, centerY + 25.f, volume);

	sf::Text brightnessLabel(font, "Brightness: ", 60);
    brightnessLabel.setPosition({ leftColX, centerY + sectionGap });
    
	sf::RectangleShape brightnessBar = makeBar(leftColX + 350.f, centerY + sectionGap + 25.f);
	sf::RectangleShape brightnessKnob = makeKnob(leftColX + 350.f, centerY + sectionGap + 25.f, brightness);

	sf::RectangleShape brightnessOverlay(sf::Vector2f(logicalW, logicalH));
	brightnessOverlay.setFillColor(sf::Color(0, 0, 0, 255 - (brightness * 2.55f)));

	sf::Text resolutionLabel(font, "Resolution: ", 60);
    resolutionLabel.setPosition({ leftColX, centerY + sectionGap * 2.f });

	sf::Text resolutionValue(font, std::to_string(resolutions[resolutionIndex].x) + " x " + std::to_string(resolutions[resolutionIndex].y), 60);
	resolutionValue.setPosition({ leftColX + 350.f, centerY + sectionGap * 2.f });
	resolutionValue.setFillColor(sf::Color::White);

    sf::Text back(font, "Press ESC to return", 60);
    back.setFillColor(sf::Color::White);
    back.setOutlineColor(sf::Color::Black);
    back.setOutlineThickness(2.f);
    back.setPosition({
		centerX - back.getGlobalBounds().size.x / 2.f, logicalH - 100.f
    });

    struct KeyBindUI {
        sf::Text label;
        sf::RectangleShape box;
        sf::Text valueText;
        sf::Keyboard::Scancode* boundKey;

        KeyBindUI(const sf::Font& f) : label(f), valueText(f), boundKey(nullptr) {}
    };
    std::vector<KeyBindUI> binds;

    auto addBind = [&](const std::string& name, sf::Keyboard::Scancode* keyPtr, float x, float y) {
        KeyBindUI b(font);
        b.label.setString(name);
        b.label.setCharacterSize(50);
        b.label.setPosition({ x, y });

        b.box.setSize({ 260.f, 60.f });
        b.box.setFillColor(sf::Color(100, 100, 100));
        b.box.setPosition({ x + 250.f, y });

        b.valueText.setString(sf::Keyboard::getDescription(*keyPtr).toAnsiString());
        b.valueText.setCharacterSize(50);
        b.valueText.setPosition({ x + 260.f, y + 2.f });
        
        b.boundKey = keyPtr;
        binds.push_back(b);
    };

    float bindStartY = centerY + sectionGap * 3.f - 20.f;
    float bindGapY = 80.f;
    float bindCol1 = centerX - 600.f;
    float bindCol2 = centerX + 100.f;

    // Col 1
    addBind("Up", &globalSettings.keyUp, bindCol1, bindStartY);
    addBind("Left", &globalSettings.keyLeft, bindCol1, bindStartY + bindGapY);
    addBind("Inventory", &globalSettings.keyInventory, bindCol1, bindStartY + bindGapY * 2);
    addBind("Sprint", &globalSettings.keySprint, bindCol1, bindStartY + bindGapY * 3);
    addBind("Chat", &globalSettings.keyChat, bindCol1, bindStartY + bindGapY * 4);

    // Col 2
    addBind("Down", &globalSettings.keyDown, bindCol2, bindStartY);
    addBind("Right", &globalSettings.keyRight, bindCol2, bindStartY + bindGapY);
    addBind("Interact", &globalSettings.keyInteract, bindCol2, bindStartY + bindGapY * 2);
    addBind("Attack", &globalSettings.keyAttack, bindCol2, bindStartY + bindGapY * 3);
    addBind("Rotate", &globalSettings.keyRotate, bindCol2, bindStartY + bindGapY * 4);

	bool draggingVol = false;
	bool draggingBright = false;
    int waitingForKeybind = -1;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (waitingForKeybind != -1) {
                    *binds[waitingForKeybind].boundKey = key->scancode;
                    binds[waitingForKeybind].valueText.setString(sf::Keyboard::getDescription(key->scancode).toAnsiString());
                    binds[waitingForKeybind].box.setFillColor(sf::Color(100, 100, 100));
                    waitingForKeybind = -1;
                    continue;
                }

                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    window.setView(oldView);
                    return; // <-- Return to main menu
                }

                if (key->scancode == sf::Keyboard::Scancode::A || key->scancode == sf::Keyboard::Scancode::Left) {
                    resolutionIndex = (resolutionIndex - 1 + maxRes) % maxRes;
                    resolutionValue.setString(std::to_string(resolutions[resolutionIndex].x) + " x " + std::to_string(resolutions[resolutionIndex].y));
                    window.setSize(resolutions[resolutionIndex]);
					UpdateViewToWindow(&window);
                    width = resolutions[resolutionIndex].x;
                    height = resolutions[resolutionIndex].y;
                }
                else if (key->scancode == sf::Keyboard::Scancode::D || key->scancode == sf::Keyboard::Scancode::Right) {
                    resolutionIndex = (resolutionIndex + 1) % maxRes;
                    resolutionValue.setString(std::to_string(resolutions[resolutionIndex].x) + " x " + std::to_string(resolutions[resolutionIndex].y));
                    window.setSize(resolutions[resolutionIndex]);
					UpdateViewToWindow(&window);
                    width = resolutions[resolutionIndex].x;
                    height = resolutions[resolutionIndex].y;
                }
            }
            if (auto* m = event->getIf<sf::Event::MouseButtonPressed>())
            {
				if (m->button == sf::Mouse::Button::Left)
                {
                    if (waitingForKeybind != -1) {
                        binds[waitingForKeybind].box.setFillColor(sf::Color(100, 100, 100));
                        waitingForKeybind = -1;
                    }

                    if (isMouseInside(volumeKnob, window) || isMouseInside(volumeBar, window))
                        draggingVol = true;

                    if (isMouseInside(brightnessKnob, window) || isMouseInside(brightnessBar, window))
                        draggingBright = true;

                    for (size_t i = 0; i < binds.size(); ++i) {
                        if (isMouseInside(binds[i].box, window)) {
                            waitingForKeybind = i;
                            binds[i].box.setFillColor(sf::Color::Red);
                            binds[i].valueText.setString("Press Key...");
                        }
                    }
                }
            }
            if (event->is<sf::Event::MouseButtonReleased>())
            {
                draggingVol = false;
                draggingBright = false;
            }
            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            if (draggingVol)
            {
                float newX = std::clamp(mouse.x - volumeBar.getPosition().x, 0.f, 500.f);

                volume = (newX / 500.f) * 100.f;

                volumeKnob.setPosition({ volumeBar.getPosition().x + newX - 15.f, volumeKnob.getPosition().y });

                sf::Listener::setGlobalVolume(volume);  // works
            }

            if (draggingBright)
            {
                float newX = std::clamp(mouse.x - brightnessBar.getPosition().x, 0.f, 500.f);

                brightness = (newX / 500.f) * 100.f;

                brightnessKnob.setPosition({ brightnessBar.getPosition().x + newX - 15.f, brightnessKnob.getPosition().y });

                brightnessOverlay.setFillColor(sf::Color(0, 0, 0, 255 - brightness * 2.55f));
            }
        }

        // Update keybind box and slider knob hover states
        sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        
        // Slider Knobs Hover
        if (volumeKnob.getGlobalBounds().contains(mpos)) {
            volumeKnob.setFillColor(sf::Color(220, 220, 220));
            volumeKnob.setOutlineThickness(2.f);
            volumeKnob.setOutlineColor(sf::Color::White);
        } else {
            volumeKnob.setFillColor(sf::Color::White);
            volumeKnob.setOutlineThickness(0.f);
        }

        if (brightnessKnob.getGlobalBounds().contains(mpos)) {
            brightnessKnob.setFillColor(sf::Color(220, 220, 220));
            brightnessKnob.setOutlineThickness(2.f);
            brightnessKnob.setOutlineColor(sf::Color::White);
        } else {
            brightnessKnob.setFillColor(sf::Color::White);
            brightnessKnob.setOutlineThickness(0.f);
        }

        // Binds boxes hover
        for (size_t i = 0; i < binds.size(); ++i) {
            if (waitingForKeybind == static_cast<int>(i)) {
                binds[i].box.setFillColor(sf::Color::Red);
                binds[i].box.setOutlineThickness(0.f);
            } else if (isMouseInside(binds[i].box, window)) {
                binds[i].box.setFillColor(sf::Color(140, 140, 140));
                binds[i].box.setOutlineThickness(3.f);
                binds[i].box.setOutlineColor(sf::Color::White);
            } else {
                binds[i].box.setFillColor(sf::Color(100, 100, 100));
                binds[i].box.setOutlineThickness(0.f);
            }
        }

        window.clear(sf::Color::Black);
        window.draw(bgSprite);
        window.draw(title);

		window.draw(volumeLabel);
		window.draw(volumeBar);
		window.draw(volumeKnob);

		window.draw(brightnessLabel);
		window.draw(brightnessBar);
		window.draw(brightnessKnob);

		window.draw(resolutionLabel);
		window.draw(resolutionValue);

        for (const auto& b : binds) {
            window.draw(b.label);
            window.draw(b.box);
            window.draw(b.valueText);
        }

        window.draw(back);

		window.draw(brightnessOverlay);

        window.display();
    }
}
