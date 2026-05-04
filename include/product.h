#pragma once
#include <string>
#include <map>
#include "nlohmann/json.hpp"

// A generic product with dynamic attributes — no hardcoded fields.
// Every attribute is stored in a JSON value so it can be numeric, string, or ordinal.

struct Product {
    std::string id;
    std::string name;
    nlohmann::json attributes; // key → value (flexible schema)

    // Convenience: get a numeric attribute (returns 0 if missing)
    double getNumeric(const std::string& key) const {
        if (attributes.contains(key) && attributes[key].is_number())
            return attributes[key].get<double>();
        return 0.0;
    }

    // Convenience: get a string attribute (returns "" if missing)
    std::string getString(const std::string& key) const {
        if (attributes.contains(key) && attributes[key].is_string())
            return attributes[key].get<std::string>();
        return "";
    }
};
