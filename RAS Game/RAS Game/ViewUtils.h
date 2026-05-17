#pragma once
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <string>

// Update the view to exactly match the current window size (SFML 3)
inline void UpdateViewToWindow(sf::RenderWindow* window)
{
    if (!window) return;

    sf::Vector2u size = window->getSize();

    sf::View view;
    view.setSize(sf::Vector2f(static_cast<float>(size.x),
        static_cast<float>(size.y)));
    view.setCenter(sf::Vector2f(view.getSize().x / 2.f,
        view.getSize().y / 2.f));

    window->setView(view);
}

// Automatically resolves asset paths, prioritizing local directory structure next to the executable
inline std::string resolvePath(const std::string& path) {
    if (std::filesystem::exists(path)) {
        return path;
    }
    if (path.rfind("../", 0) == 0) {
        std::string stripped = path.substr(3);
        if (std::filesystem::exists(stripped)) {
            return stripped;
        }
        std::string deep = "../" + stripped;
        if (std::filesystem::exists(deep)) {
            return deep;
        }
        std::string extraDeep = "../../" + stripped;
        if (std::filesystem::exists(extraDeep)) {
            return extraDeep;
        }
        std::string projectDeep = "../../RAS Game/" + stripped;
        if (std::filesystem::exists(projectDeep)) {
            return projectDeep;
        }
        std::string projectSemiDeep = "../RAS Game/" + stripped;
        if (std::filesystem::exists(projectSemiDeep)) {
            return projectSemiDeep;
        }
    } else {
        std::string addParent = "../" + path;
        if (std::filesystem::exists(addParent)) {
            return addParent;
        }
        std::string addTwoParents = "../../" + path;
        if (std::filesystem::exists(addTwoParents)) {
            return addTwoParents;
        }
        std::string addTwoParentsAndProject = "../../RAS Game/" + path;
        if (std::filesystem::exists(addTwoParentsAndProject)) {
            return addTwoParentsAndProject;
        }
        std::string addParentAndProject = "../RAS Game/" + path;
        if (std::filesystem::exists(addParentAndProject)) {
            return addParentAndProject;
        }
    }
    return path;
}
