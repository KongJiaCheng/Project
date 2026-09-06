#include "Zombie.h"

sf::Texture Zombie::texture;
std::vector<sf::Texture> Zombie::sharedSouth;
std::vector<sf::Texture> Zombie::sharedNorth;
std::vector<sf::Texture> Zombie::sharedEast;
std::vector<sf::Texture> Zombie::sharedWest;
bool Zombie::sharedLoaded = false;
