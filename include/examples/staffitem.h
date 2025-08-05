//
// Created by ozhang on 03/08/2025.
//
#include "../csimpy/itembase.h"
#ifndef STAFFITEM_H
#define STAFFITEM_H

// Derived struct for staff items
struct StaffItem : ItemBase {
    std::string role;
    int skill_level;

    StaffItem(std::string n, int i, std::string r, int skill)
        : ItemBase(std::move(n), i), role(std::move(r)), skill_level(skill) {}

    std::string to_string() const override {
        return "StaffItem(" + name + ", id=" + std::to_string(id) +
               ", role=" + role + ", skill=" + std::to_string(skill_level) + ")";
    }

    ItemBase* clone() const override {
        return new StaffItem(*this);
    }
};


#endif //STAFFITEM_H
