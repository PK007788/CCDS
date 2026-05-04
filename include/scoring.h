#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
#include "compat.h"
#include "product.h"
#include "category.h"

// Per-attribute score contribution for explainability
struct AttributeScore {
    std::string name;
    std::string display;
    double normalized;   // [0, 1]
    double weight;       // user-tuned or default
    double contribution; // normalized * weight
};

// A product with its computed score and breakdown
struct ScoredProduct {
    Product product;
    double total_score;
    std::vector<AttributeScore> breakdown;
};

// Normalize a numeric value to [0, 1] within a given range
inline double normalizeNumeric(double val, double min_val, double max_val) {
    if (max_val <= min_val) return 0.5;
    double norm = (val - min_val) / (max_val - min_val);
    return clampVal(norm, 0.0, 1.0);
}

// Normalize an ordinal value based on its position in the level list
inline double normalizeOrdinal(const std::string& val, const std::vector<std::string>& levels) {
    if (levels.empty()) return 0.5;
    for (size_t i = 0; i < levels.size(); ++i) {
        if (levels[i] == val) {
            return static_cast<double>(i) / static_cast<double>(levels.size() - 1);
        }
    }
    return 0.0; // unknown level
}

// Score a single product against a category schema with given weights
inline ScoredProduct scoreProduct(const Product& product,
                                   const CategorySchema& schema,
                                   const std::vector<double>& weights) {
    ScoredProduct sp;
    sp.product = product;
    sp.total_score = 0.0;

    for (size_t i = 0; i < schema.attributes.size(); ++i) {
        const auto& attr = schema.attributes[i];
        double weight = (i < weights.size()) ? weights[i] : attr.default_weight;

        double normalized = 0.0;

        if (attr.type == "numeric") {
            double val = product.getNumeric(attr.name);
            normalized = normalizeNumeric(val, attr.range_min, attr.range_max);
            // Invert for "lower_better" (e.g., price, weight, power draw)
            if (attr.direction == "lower_better") {
                normalized = 1.0 - normalized;
            }
        } else if (attr.type == "ordinal") {
            std::string val = product.getString(attr.name);
            normalized = normalizeOrdinal(val, attr.levels);
            if (attr.direction == "lower_better") {
                normalized = 1.0 - normalized;
            }
        }

        double contribution = normalized * weight;

        sp.breakdown.push_back({
            attr.name,
            attr.display,
            normalized,
            weight,
            contribution
        });

        sp.total_score += contribution;
    }

    return sp;
}

// Score and rank all products, return sorted (best first)
inline std::vector<ScoredProduct> scoreAndRank(const std::vector<Product>& products,
                                                const CategorySchema& schema,
                                                const std::vector<double>& weights) {
    std::vector<ScoredProduct> scored;
    for (auto& p : products) {
        scored.push_back(scoreProduct(p, schema, weights));
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredProduct& a, const ScoredProduct& b) {
        return a.total_score > b.total_score;
    });

    return scored;
}

// Compute confidence based on gap between #1 and #2
inline std::string getConfidenceLevel(const std::vector<ScoredProduct>& ranked) {
    if (ranked.size() < 2) return "N/A";
    double gap = ranked[0].total_score - ranked[1].total_score;
    if (gap > 0.15) return "HIGH";
    if (gap > 0.05) return "MEDIUM";
    return "LOW";
}

inline double getConfidenceGap(const std::vector<ScoredProduct>& ranked) {
    if (ranked.size() < 2) return 0.0;
    return ranked[0].total_score - ranked[1].total_score;
}
