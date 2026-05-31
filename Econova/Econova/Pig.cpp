#include "Pig.h"

sf::Texture Pig::texture;
std::vector<sf::Texture> Pig::sharedSouth;
std::vector<sf::Texture> Pig::sharedNorth;
std::vector<sf::Texture> Pig::sharedEast;
std::vector<sf::Texture> Pig::sharedWest;
bool Pig::sharedLoaded = false;
