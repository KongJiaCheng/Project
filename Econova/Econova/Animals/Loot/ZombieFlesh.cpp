#include "ZombieFlesh.h"

sf::Texture ZombieFlesh::texture;

const sf::Texture* ZombieFlesh::getTexture()
{
    if (texture.getSize().x == 0)
        texture.loadFromFile(resolvePath("Items/ZombieFlesh.png"));

    return &texture;
}
