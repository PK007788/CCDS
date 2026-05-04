#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include "compat.h"
#include "category.h"
#include "filter.h"
#include "scoring.h"

// ─────────────── Pretty-Print Helpers ───────────────

inline void printBanner() {
    std::cout << "\n";
    std::cout << " ================================================================\n";
    std::cout << " |    CCDS - Config-driven Cognitive Decision Support System     |\n";
    std::cout << " |         AI-Powered Product Recommendation Engine              |\n";
    std::cout << " |                                                               |\n";
    std::cout << " |   Decisions: Structured C++ Logic  |  Explanations: Local LLM |\n";
    std::cout << " ================================================================\n";
    std::cout << "\n";
}

inline void printSectionHeader(const std::string& title) {
    std::cout << "\n +--- " << title << " ";
    int remaining = 58 - static_cast<int>(title.size());
    for (int i = 0; i < remaining; ++i) std::cout << "-";
    std::cout << "+\n";
}

// ─────────────── Category Selection ───────────────

inline int selectCategory(const std::vector<CategorySchema>& categories) {
    printSectionHeader("SELECT PRODUCT CATEGORY");
    for (size_t i = 0; i < categories.size(); ++i) {
        std::cout << "   [" << (i + 1) << "] " << categories[i].display_name
                  << " (" << categories[i].category << ")\n";
    }
    std::cout << "\n   Enter choice (1-" << categories.size() << "): ";

    int choice;
    std::cin >> choice;
    std::cin.ignore();

    if (choice < 1 || choice > static_cast<int>(categories.size())) {
        std::cout << "   [!] Invalid choice. Defaulting to 1.\n";
        choice = 1;
    }
    return choice - 1;
}

// ─────────────── Constraint Collection ───────────────

inline std::vector<UserConstraint> collectConstraints(const CategorySchema& schema) {
    printSectionHeader("YOUR REQUIREMENTS");
    std::vector<UserConstraint> constraints;

    for (auto& cs : schema.constraints) {
        std::cout << "   " << cs.prompt << ": ";

        UserConstraint uc;
        uc.attribute = cs.attribute;
        uc.op = cs.filter_op;

        if (cs.input_type == "numeric") {
            uc.is_numeric = true;
            std::cin >> uc.numeric_val;
            std::cin.ignore();
            // Skip constraint if value is 0 (means "any" for >= constraints)
            if (uc.op == ">=" && uc.numeric_val == 0) continue;
        } else {
            uc.is_numeric = false;
            std::getline(std::cin, uc.string_val);
        }

        constraints.push_back(uc);
    }

    return constraints;
}

// Build a human-readable summary of constraints
inline std::string constraintsSummary(const std::vector<UserConstraint>& constraints) {
    std::ostringstream ss;
    bool first = true;
    for (auto& c : constraints) {
        if (!first) ss << ", ";
        first = false;
        ss << c.attribute << " " << c.op << " ";
        if (c.is_numeric) ss << c.numeric_val;
        else ss << c.string_val;
    }
    return ss.str();
}

// ─────────────── Interactive Weight Tuning ───────────────

inline std::vector<double> tuneWeights(const CategorySchema& schema) {
    printSectionHeader("PRIORITY TUNING");
    std::cout << "   Rate how important each factor is to you (1-5):\n";
    std::cout << "   (1 = Not important, 3 = Moderate, 5 = Very important)\n\n";

    std::vector<double> raw_priorities;
    double total = 0;

    for (auto& attr : schema.attributes) {
        std::cout << "   " << std::setw(22) << std::left << attr.display << " [1-5]: ";
        int priority;
        std::cin >> priority;
        priority = clampVal(priority, 1, 5);
        raw_priorities.push_back(static_cast<double>(priority));
        total += priority;
    }
    std::cin.ignore();

    // Normalize priorities to weights that sum to 1.0
    std::vector<double> weights;
    std::cout << "\n   Computed weights:\n";
    for (size_t i = 0; i < schema.attributes.size(); ++i) {
        double w = raw_priorities[i] / total;
        weights.push_back(w);
        std::cout << "   " << std::setw(22) << std::left << schema.attributes[i].display
                  << " -> " << std::fixed << std::setprecision(1) << (w * 100) << "%\n";
    }

    return weights;
}

// Ask user if they want to tune weights or use defaults
inline std::vector<double> getWeights(const CategorySchema& schema) {
    std::cout << "\n   Would you like to customize attribute priorities? (y/n): ";
    char choice;
    std::cin >> choice;
    std::cin.ignore();

    if (choice == 'y' || choice == 'Y') {
        return tuneWeights(schema);
    }

    // Use default weights from config
    std::vector<double> defaults;
    std::cout << "\n   Using default weights:\n";
    for (auto& attr : schema.attributes) {
        defaults.push_back(attr.default_weight);
        std::cout << "   " << std::setw(22) << std::left << attr.display
                  << " -> " << std::fixed << std::setprecision(1)
                  << (attr.default_weight * 100) << "%\n";
    }
    return defaults;
}

// ─────────────── Results Display ───────────────

inline std::string makeBar(double value, int width = 20) {
    int filled = static_cast<int>(value * width);
    filled = clampVal(filled, 0, width);
    std::string bar(filled, '#');
    std::string empty(width - filled, '.');
    return "[" + bar + empty + "]";
}

inline void displayResults(const std::vector<ScoredProduct>& ranked,
                           const CategorySchema& schema,
                           int top_n = 3) {
    printSectionHeader("RECOMMENDATION RESULTS");

    int show = std::min(top_n, static_cast<int>(ranked.size()));

    // Display comparison table
    std::cout << "\n   " << std::setw(4) << "Rank"
              << "  " << std::setw(45) << std::left << "Product"
              << std::setw(10) << "Score" << "\n";
    std::cout << "   " << std::string(65, '-') << "\n";

    for (int i = 0; i < show; ++i) {
        std::string marker = (i == 0) ? " >> " : "    ";
        std::cout << marker << std::setw(45) << std::left << ranked[i].product.name
                  << std::fixed << std::setprecision(3) << ranked[i].total_score << "\n";
    }

    // Score breakdown for the top pick
    if (!ranked.empty()) {
        printSectionHeader("SCORE BREAKDOWN: " + ranked[0].product.name);
        for (auto& b : ranked[0].breakdown) {
            std::cout << "   " << std::setw(22) << std::left << b.display
                      << makeBar(b.normalized) << " "
                      << std::fixed << std::setprecision(2) << b.contribution
                      << "  (w:" << std::setprecision(0) << (b.weight * 100) << "%)\n";
        }

        // Confidence
        std::string conf = getConfidenceLevel(ranked);
        double gap = getConfidenceGap(ranked);
        std::cout << "\n   Confidence: " << conf;
        if (ranked.size() >= 2) {
            std::cout << " (gap to #2: " << std::fixed << std::setprecision(3) << gap << ")";
        }
        std::cout << "\n";
    }
}

inline void displayExplanation(const std::string& explanation, bool is_llm) {
    printSectionHeader("AI EXPLANATION");
    std::cout << "\n   " << explanation << "\n";
    std::cout << "\n   [Decision by structured scoring. "
              << (is_llm ? "LLM used for explanation only." : "Template-based explanation.")
              << "]\n";
}

inline void printFooter() {
    std::cout << "\n ================================================================\n";
    std::cout << " |              Thank you for using CCDS!                       |\n";
    std::cout << " ================================================================\n\n";
}
