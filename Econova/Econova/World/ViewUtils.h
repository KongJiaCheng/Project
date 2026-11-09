#pragma once
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <SFML/Graphics.hpp>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>

// Get a logical aspect-ratio-correct UI View of size 1920x1080 for any window size
inline sf::View GetUIView(sf::RenderWindow* window)
{
    sf::View view;
    if (!window) return view;

    sf::Vector2u windowSize = window->getSize();
    float windowWidth = static_cast<float>(windowSize.x);
    float windowHeight = static_cast<float>(windowSize.y);

    float targetWidth = 1920.f;
    float targetHeight = 1080.f;
    float targetAspectRatio = targetWidth / targetHeight;
    float windowAspectRatio = windowWidth / windowHeight;

    float viewportWidth = 1.f;
    float viewportHeight = 1.f;
    float viewportX = 0.f;
    float viewportY = 0.f;

    if (windowAspectRatio > targetAspectRatio) {
        // Pillarbox (bars on the sides)
        viewportWidth = targetAspectRatio / windowAspectRatio;
        viewportX = (1.f - viewportWidth) / 2.f;
    } else {
        // Letterbox (bars on top/bottom)
        viewportHeight = windowAspectRatio / targetAspectRatio;
        viewportY = (1.f - viewportHeight) / 2.f;
    }

    view.setSize({ targetWidth, targetHeight });
    view.setCenter({ targetWidth / 2.f, targetHeight / 2.f });
    view.setViewport(sf::FloatRect({ viewportX, viewportY }, { viewportWidth, viewportHeight }));
    return view;
}

// Update the view to exactly match a virtual resolution of 1920x1080 with aspect ratio preservation (SFML 3)
inline void UpdateViewToWindow(sf::RenderWindow* window)
{
    if (!window) return;
    window->setView(GetUIView(window));
}

#include <set>
#include <iostream>
#include <fstream>

inline bool fileExists(const std::string& path) {
    if (path.empty()) return false;
    std::ifstream f(path);
    return f.good();
}

// Automatically resolves asset paths, prioritizing direct local folders (crucial for standalone execution)
inline std::string resolvePathInternal(const std::string& path) {
    auto checkExist = [](const std::string& p) -> std::string {
        if (p.empty()) return "";
        if (fileExists(p)) return p;
        return "";
    };

    // 1. Direct local lookup (assets next to the executable, e.g., in a packaged folder)
    if (std::string res = checkExist(path); !res.empty()) return res;

    // Remove any relative prefix for checking alternate build environments
    std::string cleanPath = path;
    if (cleanPath.rfind("../", 0) == 0) {
        cleanPath = cleanPath.substr(3);
    } else if (cleanPath.rfind("./", 0) == 0) {
        cleanPath = cleanPath.substr(2);
    }

    // 2. Visual Studio developer environment path lookups (relative to project / output structures)
    if (std::string res = checkExist(cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("Release/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("Econova/Release/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("Econova/Econova/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../../" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../Release/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../../Release/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../x64/Release/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../../x64/Release/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../x64/Debug/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../../x64/Debug/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../Econova/" + cleanPath); !res.empty()) return res;
    if (std::string res = checkExist("../../Econova/" + cleanPath); !res.empty()) return res;

    return path;
}

inline std::string resolvePath(const std::string& path) {
    std::string resolved = resolvePathInternal(path);

    static std::set<std::string> loggedPaths;
    if (loggedPaths.find(path) == loggedPaths.end()) {
        loggedPaths.insert(path);

        std::string lowerPath = path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if (lowerPath.find(".png") != std::string::npos || lowerPath.find(".jpg") != std::string::npos || lowerPath.find(".jpeg") != std::string::npos) {
            std::string displayPath = resolved;
            std::replace(displayPath.begin(), displayPath.end(), '\\', '/');
            std::cout << "[Asset Load] Image: \"" << path << "\" -> asset load directory: \"" << displayPath << "\"" << std::endl;
        }
    }

    return resolved;
}

// Safely loads a texture, validating dimensions against the OpenGL GL_MAX_TEXTURE_SIZE (sf::Texture::getMaximumSize()).
// If the image exceeds the maximum texture size supported by the current driver (common in VirtualBox without 3D acceleration),
// it automatically downscales the image in memory before texture creation, completely preventing OpenGL texture failure.
inline bool loadTextureSafe(sf::Texture& tex, const std::string& path, bool sRgb = false) {
    std::string resolved = resolvePath(path);
    if (!fileExists(resolved)) {
        return false;
    }

    unsigned int maxTexDim = sf::Texture::getMaximumSize();
    if (maxTexDim == 0) {
        maxTexDim = 1024;
    }

    sf::Image img;
    if (!img.loadFromFile(resolved)) {
        return false;
    }

    sf::Vector2u sz = img.getSize();
    if (sz.x == 0 || sz.y == 0) {
        return false;
    }

    // Direct load if within GPU limits
    if (sz.x <= maxTexDim && sz.y <= maxTexDim) {
        return tex.loadFromImage(img, sRgb);
    }

    // Downscale proportionally to fit within maxTexDim
    float scaleX = (sz.x > maxTexDim) ? static_cast<float>(maxTexDim) / static_cast<float>(sz.x) : 1.f;
    float scaleY = (sz.y > maxTexDim) ? static_cast<float>(maxTexDim) / static_cast<float>(sz.y) : 1.f;
    float scale = (std::min)(scaleX, scaleY);

    unsigned int targetW = (std::max)(1u, static_cast<unsigned int>(sz.x * scale));
    unsigned int targetH = (std::max)(1u, static_cast<unsigned int>(sz.y * scale));

    std::vector<std::uint8_t> newPixels(static_cast<size_t>(targetW) * targetH * 4);
    const std::uint8_t* srcPixels = img.getPixelsPtr();

    for (unsigned int y = 0; y < targetH; ++y) {
        float srcY = static_cast<float>(y) / scale;
        unsigned int y0 = (std::min)(static_cast<unsigned int>(srcY), sz.y - 1);
        for (unsigned int x = 0; x < targetW; ++x) {
            float srcX = static_cast<float>(x) / scale;
            unsigned int x0 = (std::min)(static_cast<unsigned int>(srcX), sz.x - 1);

            size_t srcIdx = (static_cast<size_t>(y0) * sz.x + x0) * 4;
            size_t dstIdx = (static_cast<size_t>(y) * targetW + x) * 4;

            newPixels[dstIdx + 0] = srcPixels[srcIdx + 0];
            newPixels[dstIdx + 1] = srcPixels[srcIdx + 1];
            newPixels[dstIdx + 2] = srcPixels[srcIdx + 2];
            newPixels[dstIdx + 3] = srcPixels[srcIdx + 3];
        }
    }

    sf::Image resizedImg;
    resizedImg.resize(sf::Vector2u(targetW, targetH), newPixels.data());
    std::cout << "[loadTextureSafe] Auto-downscaled \"" << path << "\" from "
              << sz.x << "x" << sz.y << " to " << targetW << "x" << targetH
              << " (system max: " << maxTexDim << "x" << maxTexDim << ")" << std::endl;

    return tex.loadFromImage(resizedImg, sRgb);
}


