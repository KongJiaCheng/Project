#include "ZombieFlesh.h"

sf::Texture ZombieFlesh::texture;

const sf::Texture* ZombieFlesh::getTexture()
{
    if (texture.getSize().x == 0)
        texture.loadFromFile("C:\\Users\\DELL\\Desktop\\RobotAliceSkull\\RAS Project\\RAS Game\\x64\\Debug\\Items\\ZombieFlesh.png");

    return &texture;
}
