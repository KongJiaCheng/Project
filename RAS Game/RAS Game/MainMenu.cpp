#include <iostream>
#include <SFML/Graphics.hpp>
#include "Game.h"
#include "Settings.h"
#include "ViewUtils.h"

int main()
{
    unsigned int width = 3840;
	unsigned int height = 2160;
	sf::RenderWindow* window = new sf::RenderWindow(sf::VideoMode({ width, height }), "RAS Game");

	sf::Font font;
	if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
		std::cout << "Failed to load font!\n";
		return -1;
	}

	sf::RectangleShape button1;
	button1.setSize(sf::Vector2f({ 600.f, 200.f }));
	button1.setFillColor(sf::Color::Green);
	button1.setPosition({ (width - button1.getSize().x) / 2.f, (height - button1.getSize().y) / 2.f });

	sf::Text buttonText1(font, "Play Game", 120);
	buttonText1.setFillColor(sf::Color::White);
	buttonText1.setPosition({ (width - button1.getSize().x) / 2.f, (height - button1.getSize().y) / 2.f});

	sf::RectangleShape button2;
	button2.setSize(sf::Vector2f({ 600.f, 200.f }));
	button2.setFillColor(sf::Color::Green);
	button2.setPosition({ (width - button2.getSize().x) / 2.f, ((height - button2.getSize().y) / 2.f ) + 400.f});

	sf::Text buttonText2(font, "Settings", 120);
	buttonText2.setFillColor(sf::Color::White);
	buttonText2.setPosition({ (width - button2.getSize().x) / 2.f, ((height - button2.getSize().y) / 2.f ) + 400.f});

	sf::RectangleShape button3;
	button3.setSize(sf::Vector2f({ 600.f, 200.f }));
	button3.setFillColor(sf::Color::Green);
	button3.setPosition({ (width - button3.getSize().x) / 2.f, ((height - button3.getSize().y) / 2.f ) + 800.f});

	sf::Text buttonText3(font, "Exit", 120);
	buttonText3.setPosition({ (width - button3.getSize().x) / 2.f, ((height - button3.getSize().y) / 2.f ) + 800.f});

	UpdateViewToWindow(window);
	while (window->isOpen()) {

		while (const std::optional event = window->pollEvent()) {
			if (event->is<sf::Event::Closed>()) {
				window->close();
			}
			else if(const auto*keyPressed = event->getIf<sf::Event::KeyPressed>()){
				if(keyPressed->scancode == sf::Keyboard::Scancode::Escape){
					window->close();
				}
			}
			else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
				if (mouse->button == sf::Mouse::Button::Left) {
					sf::Vector2f pos(mouse->position.x, mouse->position.y);

					if (button1.getGlobalBounds().contains(pos)) {
						gameLoop(*window, width, height);
					}
					else if (button2.getGlobalBounds().contains(pos)) {
						openSettingsWindow(*window, width, height);
					}
					else if (button3.getGlobalBounds().contains(pos)) {
						window->close();
					}
				}
			}

		}

		window->clear();
		window->draw(button1);
		window->draw(button2);
		window->draw(button3);
		window->draw(buttonText1);
		window->draw(buttonText2);
		window->draw(buttonText3);
		window->display();
	}
	delete window;
    return 0;
}

