#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include "nlohmann/json.hpp"

// Describes a single scoreable attribute in a category
struct AttributeSchema {
    std::string name;
    std::string display;
    std::string type;       // "numeric" or "ordinal"
    std::string direction;  // "higher_better" or "lower_better"
    double default_weight;
    double range_min = 0, range_max = 1;          // for numeric
    std::vector<std::string> levels;               // for ordinal
};

// Describes a user constraint/filter for the category
struct ConstraintSchema {
    std::string attribute;
    std::string prompt;
    std::string filter_op;   // "==", "<=", ">="
    std::string input_type;  // "string" or "numeric"
};

// Full category definition loaded from JSON config
struct CategorySchema {
    std::string category;
    std::string display_name;
    std::vector<AttributeSchema> attributes;
    std::vector<ConstraintSchema> constraints;
};

// Parse a single category JSON file into a CategorySchema
inline CategorySchema loadCategorySchema(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open category config: " + path);
    }
    nlohmann::json j;
    f >> j;

    CategorySchema cs;
    cs.category = j["category"];
    cs.display_name = j["display_name"];

    for (auto& a : j["attributes"]) {
        AttributeSchema as;
        as.name = a["name"];
        as.display = a["display"];
        as.type = a["type"];
        as.direction = a["direction"];
        as.default_weight = a["default_weight"];
        if (as.type == "numeric") {
            as.range_min = a["range"][0];
            as.range_max = a["range"][1];
        }
        if (as.type == "ordinal" && a.contains("levels")) {
            as.levels = a["levels"].get<std::vector<std::string>>();
        }
        cs.attributes.push_back(as);
    }

    for (auto& c : j["constraints"]) {
        ConstraintSchema con;
        con.attribute = c["attribute"];
        con.prompt = c["prompt"];
        con.filter_op = c["filter_op"];
        con.input_type = c["input_type"];
        cs.constraints.push_back(con);
    }

    return cs;
}

// Load categories from a list of known file paths
// (avoids <filesystem> dependency for older compilers)
inline std::vector<CategorySchema> loadCategoriesFromList(
    const std::vector<std::string>& paths) {
    std::vector<CategorySchema> categories;
    for (auto& path : paths) {
        try {
            categories.push_back(loadCategorySchema(path));
        } catch (const std::exception& e) {
            std::cerr << "   [WARN] Failed to load " << path << ": " << e.what() << "\n";
        }
    }
    return categories;
}
