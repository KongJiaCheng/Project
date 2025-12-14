#include "ItemManager.h"

void ItemManager::spawnRandomItems(const std::vector<SpawnRule>& rules, int mapWidth, int mapHeight)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> randX(0.f, (float)mapWidth);
    std::uniform_real_distribution<float> randY(0.f, (float)mapHeight);

    for (const auto& rule : rules)
    {
        if (!rule.texture) continue;

        std::uniform_int_distribution<int> amountDist(rule.minCount, rule.maxCount);
        int totalItems = amountDist(gen);

        const float MIN_DISTANCE = 150.f;   // separation between items

        for (int i = 0; i < totalItems; i++)
        {
            sf::Vector2f pos;
            int attempts = 0;
            const int MAX_ATTEMPTS = 40;

            do {
                pos = { randX(gen), randY(gen) };
                attempts++;
            } while (!isAreaFreeExplicit(pos, MIN_DISTANCE, rule.texture) && attempts < MAX_ATTEMPTS);

            if (attempts < MAX_ATTEMPTS)
                items.emplace_back(rule.itemID, 1, *rule.texture, pos);
        }
    }
}
