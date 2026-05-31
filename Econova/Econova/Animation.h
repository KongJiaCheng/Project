#pragma once
#include <SFML/Graphics.hpp>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>

#include <filesystem>
#include "ViewUtils.h"


class Animation
{
public:
    std::vector<sf::Texture> frames;
    const std::vector<sf::Texture>* sharedFrames = nullptr;
    float frameTime = 0.12f;
    float timer = 0.f;
    int current = 0;
    bool playing = false;

    bool loadFromFolder(const std::string& folder, int frameCount, float fTime = 0.12f)
    {
        sharedFrames = nullptr;
        frames.clear();
        frameTime = fTime;
        frames.reserve(frameCount);

        for (int i = 0; i < frameCount; ++i)
        {
            std::ostringstream ss;
            ss << folder << "/frame_" << std::setfill('0') << std::setw(3) << i << ".png";
            std::string path = resolvePath(ss.str());

            sf::Texture tex;
            if (!tex.loadFromFile(path))
            {
                std::cerr << "Animation: failed to load " << path << '\n';
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
        size_t count = sharedFrames ? sharedFrames->size() : frames.size();
        if (!playing || count == 0) return;

        timer += dt;
        if (timer >= frameTime)
        {
            timer -= frameTime;
            current = (current + 1) % static_cast<int>(count);
        }
    }

    const sf::Texture& getCurrentTexture() const
    {
        const std::vector<sf::Texture>& targetFrames = sharedFrames ? *sharedFrames : frames;
        if (targetFrames.empty()) {
            static sf::Texture emptyTex;
            return emptyTex;
        }
        return targetFrames.at(std::max(0, std::min(current, (int)targetFrames.size() - 1)));
    }

    void stopAtFirst()
    {
        playing = false;
        timer = 0.f;
        current = 0;
    }
};