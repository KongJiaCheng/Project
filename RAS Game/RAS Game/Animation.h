#pragma once
#include <SFML/Graphics.hpp>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>

#include <filesystem>

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
            std::string path = ss.str();

            if (!std::filesystem::exists(path))
            {
                if (path.rfind("../", 0) == 0)
                {
                    std::string stripped = path.substr(3);
                    if (std::filesystem::exists(stripped)) {
                        path = stripped;
                    } else if (std::filesystem::exists("../" + stripped)) {
                        path = "../" + stripped;
                    } else if (std::filesystem::exists("../../" + stripped)) {
                        path = "../../" + stripped;
                    }
                }
                else
                {
                    std::string addParent = "../" + path;
                    if (std::filesystem::exists(addParent)) {
                        path = addParent;
                    } else if (std::filesystem::exists("../../" + path)) {
                        path = "../../" + path;
                    }
                }
            }

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
        if (frames.empty()) {
            static sf::Texture emptyTex;
            return emptyTex;
        }
        return frames.at(std::max(0, std::min(current, (int)frames.size() - 1)));
    }

    void stopAtFirst()
    {
        playing = false;
        timer = 0.f;
        current = 0;
    }
};