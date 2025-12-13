#include "Settings.h"
#include "ViewUtils.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <string>

bool isMouseInside(const sf::RectangleShape& box, sf::RenderWindow& window)
{
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    return box.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos));
}

void openSettingsWindow(sf::RenderWindow& window, unsigned int width, unsigned int height)
{
    sf::Font font;
    font.openFromFile("C:/Windows/Fonts/arial.ttf");

    int volume = 50;
	int brightness = 50;
	int resolutionIndex = 1;

    sf::Vector2u resolutions[] = {
        {1280, 720},
        {1920, 1080},
        {2560, 1440},
        {3840, 2160}
	};

    const int maxRes = 4;

	float centerX = width / 2.f;
	float centerY = height / 4.f;
    float sectionGap = 180.f;

    sf::Text title(font, "Settings", 160);
    title.setFillColor(sf::Color::Yellow);
    title.setPosition({
	centerX - width / 2.f, 80.f
        });

    auto makeBar = [&](float y) {
        sf::RectangleShape bar(sf::Vector2f(600, 30));
        bar.setFillColor(sf::Color(80, 80, 80));
        bar.setPosition({ centerX - 300.f, y });
        return bar;
        };

    auto makeKnob = [&](float y, float percent) {
        sf::RectangleShape knob(sf::Vector2f(30, 50));
        knob.setFillColor(sf::Color::White);

		float px = (percent / 100.f) * 600.f;
        knob.setPosition({ centerX - 300.f + px - 15.f, y - 10.f });
        return knob;
        };

	sf::Text volumeLabel(font, "Volume: ", 80);
    volumeLabel.setPosition({
        centerX - 600.f, centerY});

    sf::RectangleShape volumeBar = makeBar(centerY);
	sf::RectangleShape volumeKnob = makeKnob(centerY, volume);

	sf::Text brightnessLabel(font, "Brightness: ", 80);
    brightnessLabel.setPosition({
		centerX - 600.f, centerY + sectionGap
    });
    
	sf::RectangleShape brightnessBar = makeBar(centerY + sectionGap);
	sf::RectangleShape brightnessKnob = makeKnob(centerY + sectionGap, brightness);

	sf::RectangleShape brightnessOverlay(sf::Vector2f(width, height));
	brightnessOverlay.setFillColor(sf::Color(255, 255, 255, 255 - (brightness * 2.55f)));

	sf::Text resolutionLabel(font, "Resolution: ", 80);
    resolutionLabel.setPosition({
		centerX - 600.f, centerY + sectionGap * 2.f
		});

	sf::Text resolutionValue(font, std::to_string(resolutions[resolutionIndex].x) + " x " + std::to_string(resolutions[resolutionIndex].y), 80);
	resolutionValue.setPosition({
		centerX - 200.f, centerY + sectionGap * 2.f
		});
	resolutionValue.setFillColor(sf::Color::White);

    sf::Text back(font, "Press ESC to return", 80);
    back.setFillColor(sf::Color::Black);
    back.setPosition({
		centerX - width / 2.f, height - 200.f
        });

	bool draggingVol = false;
	bool draggingBright = false;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape)
                    return; // <-- Return to main menu

                if (key->scancode == sf::Keyboard::Scancode::A) {
                    resolutionIndex = (resolutionIndex + 1) % maxRes;
                    resolutionValue.setString(std::to_string(resolutions[resolutionIndex].x) + " x " + std::to_string(resolutions[resolutionIndex].y));
                }
                if (key->scancode == sf::Keyboard::Scancode::D) {
                    resolutionIndex = (resolutionIndex - 1 + maxRes) % maxRes;
                    resolutionValue.setString(std::to_string(resolutions[resolutionIndex].x) + " x " + std::to_string(resolutions[resolutionIndex].y));
                }

                resolutionValue.setString(std::to_string(resolutions[resolutionIndex].x) + " x " + std::to_string(resolutions[resolutionIndex].y));

                if (key->scancode == sf::Keyboard::Scancode::A ||
                    key->scancode == sf::Keyboard::Scancode::D)
                {
                    window.setSize(resolutions[resolutionIndex]);
					UpdateViewToWindow(&window);

                    // Reset the view to match the new window size
                    sf::View view = window.getDefaultView();
                    window.setView(view);
                }
            }
            if (auto* m = event->getIf<sf::Event::MouseButtonPressed>())
            {
				if (m->button == sf::Mouse::Button::Left)
                {
                    if (isMouseInside(volumeKnob, window) || isMouseInside(volumeBar, window))
                        draggingVol = true;

                    if (isMouseInside(brightnessKnob, window) || isMouseInside(brightnessBar, window))
                        draggingBright = true;
                }
            }
            if (event->is<sf::Event::MouseButtonReleased>())
            {
                draggingVol = false;
                draggingBright = false;
            }
            sf::Vector2i mouse = sf::Mouse::getPosition(window);

            if (draggingVol)
            {
                float newX = std::clamp(mouse.x - volumeBar.getPosition().x, 0.f, 600.f);

                volume = (newX / 600.f) * 100.f;

                volumeKnob.setPosition({ volumeBar.getPosition().x + newX - 15.f, volumeKnob.getPosition().y });

                sf::Listener::setGlobalVolume(volume);  // works
            }

            if (draggingBright)
            {
                float newX = std::clamp(mouse.x - brightnessBar.getPosition().x, 0.f, 600.f);

                brightness = (newX / 600.f) * 100.f;

                brightnessKnob.setPosition({ brightnessBar.getPosition().x + newX - 15.f, brightnessKnob.getPosition().y });

                brightnessOverlay.setFillColor(sf::Color(255, 255, 255, 255 - brightness * 2.55f));
            }
        }

        window.clear(sf::Color::Blue);
        window.draw(title);

		window.draw(volumeLabel);
		window.draw(volumeBar);
		window.draw(volumeKnob);

		window.draw(brightnessLabel);
		window.draw(brightnessBar);
		window.draw(brightnessKnob);

		window.draw(resolutionLabel);
		window.draw(resolutionValue);

        window.draw(back);

		window.draw(brightnessOverlay);

        window.display();
    }
}
