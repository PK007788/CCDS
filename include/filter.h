#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "product.h"
#include "category.h"

// A user constraint collected at runtime
struct UserConstraint {
    std::string attribute;
    std::string op;           // "==", "<=", ">="
    std::string string_val;   // for string comparisons
    double numeric_val = 0;   // for numeric comparisons
    bool is_numeric = false;
};

// Apply all constraints to a product list and return filtered results
inline std::vector<Product> filterProducts(const std::vector<Product>& products,
                                           const std::vector<UserConstraint>& constraints) {
    std::vector<Product> result;

    for (auto& p : products) {
        bool passes = true;

        for (auto& c : constraints) {
            if (!p.attributes.contains(c.attribute)) {
                // If product doesn't have this attribute, skip this constraint
                continue;
            }

            if (c.is_numeric) {
                double val = p.getNumeric(c.attribute);
                if (c.op == "<=" && val > c.numeric_val) { passes = false; break; }
                if (c.op == ">=" && val < c.numeric_val) { passes = false; break; }
                if (c.op == "==" && val != c.numeric_val) { passes = false; break; }
            } else {
                std::string val = p.getString(c.attribute);
                // "any" means no filtering on this attribute
                if (c.string_val == "any" || c.string_val == "ANY" || c.string_val == "Any") {
                    continue;
                }
                if (c.op == "==" && val != c.string_val) { passes = false; break; }
            }
        }

        if (passes) {
            result.push_back(p);
        }
    }

    return result;
}
