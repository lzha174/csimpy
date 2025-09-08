// Required for map, shared_ptr, and any
#include <map>
#include <memory>
#include <any>
//
// Created by ozhang on 03/08/2025.
//

#ifndef ITEMBASE_H
#define ITEMBASE_H
// Base struct for items in Store
struct ItemBase {
    std::string name;
    int id;

    explicit ItemBase(std::string n, int i)
        : name(std::move(n)), id(i) {}

    virtual ~ItemBase() = default;

    virtual std::string to_string() const {
        return "Item(" + name + ", id=" + std::to_string(id) + ")";
    }

    virtual ItemBase* clone() const = 0;
};

// MapItem: an item with a map_value member
struct MapItem : public ItemBase {
    std::map<int, std::any> map_value;

    explicit MapItem(std::string n, int i) : ItemBase(std::move(n), i) {}

    std::string to_string() const override {
        return "MapItem(" + name + ", id=" + std::to_string(id) + ")";
    }

    ItemBase* clone() const override {
        return new MapItem(*this);
    }
};


// A simple item used to signal task completion
struct FinishItem : ItemBase {
    FinishItem()
        : ItemBase("finish", 0) {}

    FinishItem(int identifier)
        : ItemBase("finish", identifier) {}

    // Clone must return a copy of this object
    FinishItem* clone() const override {
        return new FinishItem(*this);
    }

    // Human-readable representation
    std::string to_string() const override {
        return "FinishItem(name=" + name + ", id=" + std::to_string(id) + ")";
    }
};


struct SimpleItem : public ItemBase {
    SimpleItem(std::string n, int i) : ItemBase(std::move(n), i) {}
    ItemBase* clone() const override { return new SimpleItem(*this); }
};

#endif //ITEMBASE_H
