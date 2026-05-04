// ================================================================
// CCDS - Config-driven Cognitive Decision Support System
// ================================================================
// Architecture: All decisions made by structured C++ logic.
//               LLM used ONLY for natural language explanation.
//
// Components:
//   category.h     - Load category schemas from config JSON
//   data_loader.h  - Load product catalogs from data JSON
//   filter.h       - Apply user constraints to filter products
//   scoring.h      - Normalize, weight, and rank products
//   explanation.h  - Build LLM prompts + template fallback
//   cli.h          - User interface and result rendering
// ================================================================

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include "include/product.h"
#include "include/category.h"
#include "include/data_loader.h"
#include "include/filter.h"
#include "include/scoring.h"
#include "include/explanation.h"
#include "include/cli.h"

// Check if a file exists (no <filesystem> needed)
inline bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

int main() {
    // -- Paths (relative to executable working directory) --
    const std::string CONFIG_DIR = "config/categories";
    const std::string DATA_DIR   = "data";
    const std::string LLAMA_CLI  = "C:\\Users\\prajn\\llama.cpp\\build\\bin\\Release\\llama-completion.exe";
    const std::string MODEL_PATH = "models/tinyllama.gguf";

    // -- Step 1: Print banner --
    printBanner();

    // -- Step 2: Load category schemas from known config files --
    // Explicitly list category configs (avoids <filesystem> on old GCC)
    std::vector<std::string> categoryFiles = {
        CONFIG_DIR + "/ram.json",
        CONFIG_DIR + "/gpu.json",
        CONFIG_DIR + "/laptop.json"
    };

    std::vector<CategorySchema> categories = loadCategoriesFromList(categoryFiles);

    if (categories.empty()) {
        std::cerr << "   [ERROR] No category configs found!\n";
        return 1;
    }

    std::cout << "   Loaded " << categories.size() << " product categories.\n";

    // -- Step 3: Let user select a category --
    int catIndex = selectCategory(categories);
    CategorySchema& schema = categories[catIndex];
    std::cout << "\n   Selected: " << schema.display_name << "\n";

    // -- Step 4: Load product data for the selected category --
    std::vector<Product> products;
    try {
        products = loadProductsForCategory(DATA_DIR, schema.category);
    } catch (const std::exception& e) {
        std::cerr << "   [ERROR] Failed to load products: " << e.what() << "\n";
        return 1;
    }

    std::cout << "   Loaded " << products.size() << " products in '"
              << schema.category << "' catalog.\n";

    // -- Step 5: Collect user constraints --
    std::vector<UserConstraint> constraints = collectConstraints(schema);
    std::string constraintStr = constraintsSummary(constraints);

    // -- Step 6: Filter products by constraints --
    std::vector<Product> filtered = filterProducts(products, constraints);

    printSectionHeader("FILTERING RESULTS");
    std::cout << "   " << products.size() << " total products -> "
              << filtered.size() << " match your constraints\n";

    if (filtered.empty()) {
        std::cout << "\n   [!] No products match your constraints. Try relaxing your budget.\n";
        printFooter();
        return 0;
    }

    // -- Step 7: Interactive weight tuning --
    std::vector<double> weights = getWeights(schema);

    // -- Step 8: Score and rank --
    std::vector<ScoredProduct> ranked = scoreAndRank(filtered, schema, weights);

    // -- Step 9: Display results --
    displayResults(ranked, schema);

    // -- Step 10: Build decision context --
    DecisionContext ctx;
    ctx.category_name = schema.display_name;
    ctx.best = ranked[0];
    ctx.top_results = ranked;
    ctx.constraints_summary = constraintStr;
    ctx.confidence = getConfidenceLevel(ranked);
    ctx.confidence_gap = getConfidenceGap(ranked);

    // -- Step 11: Generate explanation --
    bool use_llm = fileExists(LLAMA_CLI) && fileExists(MODEL_PATH);
    if (!use_llm) {
        std::cout << "\n   [INFO] LLM not available. Using template-based explanation.\n";
    } else {
        std::cout << "\n   [INFO] Generating AI explanation using local LLM...\n";
    }

    std::string explanation = generateExplanation(ctx, LLAMA_CLI, MODEL_PATH, use_llm);

    // -- Step 12: Display explanation --
    displayExplanation(explanation, use_llm);

    // -- Done --
    printFooter();
    return 0;
}