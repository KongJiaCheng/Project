#pragma once
#include <SFML/Graphics.hpp>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>

class Animation
{
public:
    std::vector<sf::Texture> frames;
    float frameTime = 0.12f;
    float timer = 0.f;
    int current = 0;
    bool playing = false;

    bool loadFromFolder(const std::string& folder, int frameCount, float fTime = 0.12f)
    {
        frames.clear();
        frameTime = fTime;
        frames.reserve(frameCount);

        for (int i = 0; i < frameCount; ++i)
        {
            std::ostringstream ss;
            ss << folder << "/frame_" << std::setfill('0') << std::setw(3) << i << ".png";

            sf::Texture tex;
            if (!tex.loadFromFile(ss.str()))
            {
                std::cerr << "Animation: failed to load " << ss.str() << '\n';
                return false;
            }
            frames.push_back(std::move(tex));
        }

        current = 0;
        timer = 0.f;
        playing = false;
        return true;
    }

    void update(float dt)
    {
        if (!playing || frames.empty()) return;

        timer += dt;
        if (timer >= frameTime)
        {
            timer -= frameTime;
            current = (current + 1) % static_cast<int>(frames.size());
        }
    }

    const sf::Texture& getCurrentTexture() const
    {
        return frames.at(std::max(0, std::min(current, (int)frames.size() - 1)));
    }

    void stopAtFirst()
    {
        playing = false;
        timer = 0.f;
        current = 0;
    }
};