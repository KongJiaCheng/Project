#include "Chicken.h"

sf::Texture Chicken::texture;
std::vector<sf::Texture> Chicken::sharedSouth;
std::vector<sf::Texture> Chicken::sharedNorth;
std::vector<sf::Texture> Chicken::sharedEast;
std::vector<sf::Texture> Chicken::sharedWest;
bool Chicken::sharedLoaded = false;
