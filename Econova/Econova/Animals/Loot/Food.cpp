#include "Food.h"
#include <algorithm>
#include <iostream>

bool consumeFoodItem(int itemID, float& hunger, int& hungerMultiplier, float maxHunger, int maxMultiplier, float& hp, bool disasterActive) {
    if (itemID == ITEM_CHICKEN_MEAT) {
        float foodRestore = 20.f;
        hunger -= foodRestore;
        while (hunger < 0.f && hungerMultiplier < maxMultiplier) {
            hunger += maxHunger;
            hungerMultiplier++;
        }
        hunger = (std::max)(0.f, hunger);
        if (disasterActive) {
            // Raw meat is poisoned during disaster epidemic! Decreases HP
            hp = (std::max)(0.f, hp - 25.f);
        }
        return true;
    }
    else if (itemID == ITEM_PIG_MEAT) {
        float foodRestore = 30.f;
        hunger -= foodRestore;
        while (hunger < 0.f && hungerMultiplier < maxMultiplier) {
            hunger += maxHunger;
            hungerMultiplier++;
        }
        hunger = (std::max)(0.f, hunger);
        if (disasterActive) {
            // Raw meat is poisoned during disaster epidemic! Decreases HP
            hp = (std::max)(0.f, hp - 30.f);
        }
        return true;
    }
    else if (itemID == ITEM_SHEEP_MEAT) {
        float foodRestore = 30.f;
        hunger -= foodRestore;
        while (hunger < 0.f && hungerMultiplier < maxMultiplier) {
            hunger += maxHunger;
            hungerMultiplier++;
        }
        hunger = (std::max)(0.f, hunger);
        if (disasterActive) {
            // Raw meat is poisoned during disaster epidemic! Decreases HP
            hp = (std::max)(0.f, hp - 30.f);
        }
        return true;
    }
    else if (itemID == ITEM_ZOMBIE_FLESH) {
        float foodRestore = 30.f;
        hunger -= foodRestore;
        while (hunger < 0.f && hungerMultiplier < maxMultiplier) {
            hunger += maxHunger;
            hungerMultiplier++;
        }
        hunger = (std::max)(0.f, hunger);
        float damage = disasterActive ? 35.f : 10.f;
        hp = (std::max)(0.f, hp - damage);
        return true;
    }
    else if (itemID == ITEM_COOKED_CHICKEN) {
        float foodRestore = 40.f;
        if (hungerMultiplier < maxMultiplier) {
            hungerMultiplier++;
        }
        hunger -= foodRestore;
        while (hunger < 0.f) {
            hunger += maxHunger;
        }
        hunger = (std::max)(0.f, hunger);
        return true;
    }
    else if (itemID == ITEM_COOKED_PIG) {
        float foodRestore = 60.f;
        if (hungerMultiplier < maxMultiplier) {
            hungerMultiplier++;
        }
        hunger -= foodRestore;
        while (hunger < 0.f) {
            hunger += maxHunger;
        }
        hunger = (std::max)(0.f, hunger);
        return true;
    }
    else if (itemID == ITEM_COOKED_SHEEP) {
        float foodRestore = 60.f;
        if (hungerMultiplier < maxMultiplier) {
            hungerMultiplier++;
        }
        hunger -= foodRestore;
        while (hunger < 0.f) {
            hunger += maxHunger;
        }
        hunger = (std::max)(0.f, hunger);
        return true;
    }
    return false;
}
