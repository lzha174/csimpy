//
// Created by ozhang on 05/08/2025.
//

#ifndef STAFFMANAGER_H
#define STAFFMANAGER_H

#include <vector>
#include <memory>
#include "EDStaff.h"

class StaffManager {
private:
    std::vector<std::shared_ptr<EDStaff>> staff_list;

public:
    // Add a staff member
    void add_staff(std::shared_ptr<EDStaff> staff) {
        staff_list.push_back(staff);
    }

    // Retrieve all staff members
    const std::vector<std::shared_ptr<EDStaff>>& get_all_staff() const {
        return staff_list;
    }

    // Find a staff member by ID
    std::shared_ptr<EDStaff> get_staff_by_id(int id) const {
        for (const auto& staff : staff_list) {
            if (staff->get_id() == id) {
                return staff;
            }
        }
        return nullptr;
    }
};

#endif // STAFFMANAGER_H
