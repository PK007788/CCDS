#pragma once
#include <vector>
#include <string>
#include <fstream>
#include "product.h"
#include "nlohmann/json.hpp"

// Load products from a single JSON data file
inline std::vector<Product> loadProducts(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open product data: " + path);
    }
    nlohmann::json j;
    f >> j;

    std::vector<Product> products;
    for (auto& p : j["products"]) {
        Product prod;
        prod.id = p["id"];
        prod.name = p["name"];
        prod.attributes = p["attributes"];
        products.push_back(prod);
    }
    return products;
}

// Load products for a specific category
inline std::vector<Product> loadProductsForCategory(const std::string& dataDir,
                                                     const std::string& category) {
    std::string path = dataDir + "/products_" + category + ".json";
    return loadProducts(path);
}
