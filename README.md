# CCDS — Config-driven Cognitive Decision Support System

An AI-powered product recommendation engine built in C++ that helps users choose the best product across multiple categories (RAM, GPU, Laptops).

## Core Principle

> **The LLM does NOT make decisions.**
> All decision-making is handled by structured C++ logic (filtering, scoring, ranking).
> The LLM (TinyLlama) is used ONLY to generate a natural language explanation of the final decision.

This separation is intentional — it ensures decisions are transparent, reproducible, and not subject to hallucination.

---

## How It Works (12-Step Pipeline)

```
User Input → Category Selection → Load Products → Collect Constraints
    → Filter → Weight Tuning → Score & Rank → Display Results
    → Build Context → Generate AI Explanation → Display → Done
```

### Step-by-Step:

1. **Banner** — Show the application header
2. **Load Categories** — Read category schemas from `config/categories/*.json`
3. **Category Selection** — User picks RAM, GPU, or Laptop
4. **Load Products** — Read product data from `data/products_*.json`
5. **Collect Constraints** — Ask the user for requirements (e.g., DDR4, budget ≤ 5000)
6. **Filter** — Remove products that don't match constraints
7. **Weight Tuning** — User rates how important each attribute is (1-5), system normalizes to weights
8. **Score & Rank** — Normalize each attribute to [0,1], multiply by weight, sum for composite score
9. **Display** — Show top 3 products with ASCII bar chart breakdown
10. **Build Context** — Package scoring results into a `DecisionContext` struct
11. **Generate Explanation** — Send context to TinyLlama via `CreateProcess`, with template fallback
12. **Display Explanation** — Show the AI-generated or template explanation

---

## Project Structure

```
CCDS/
├── main.cpp                          # Main orchestrator (12-step pipeline)
├── include/
│   ├── product.h                     # Product data structure
│   ├── category.h                    # Category schema loader
│   ├── data_loader.h                 # Product data file loader
│   ├── filter.h                      # Constraint-based filtering engine
│   ├── scoring.h                     # Scoring & ranking engine
│   ├── explanation.h                 # LLM integration + template fallback
│   ├── cli.h                         # CLI interface & visualizations
│   ├── compat.h                      # Compatibility polyfills (older compilers)
│   └── nlohmann/json.hpp             # JSON parsing library (single-header)
├── config/categories/
│   ├── ram.json                      # RAM category schema
│   ├── gpu.json                      # GPU category schema
│   └── laptop.json                   # Laptop category schema
├── data/
│   ├── products_ram.json             # 10 RAM products
│   ├── products_gpu.json             # 10 GPU products
│   └── products_laptop.json          # 10 Laptop products
├── models/
│   └── tinyllama.gguf                # TinyLlama 1.1B model (local LLM)
└── temp/                             # Temporary files for LLM communication
```

---

## File-by-File Explanation

### `main.cpp` — The Orchestrator
The entry point. It wires all components together in a 12-step pipeline. No business logic lives here — it just calls functions from the header files in order. This keeps it clean and easy to follow.

**Why this design?** Separating orchestration from logic makes it easy to add new features (like a new category) without touching the core pipeline.

---

### `include/product.h` — Product Data Structure
Defines a `Product` struct with a `name` and a dynamic `attributes` map (using `nlohmann::json`).

```cpp
struct Product {
    std::string name;
    nlohmann::json attributes;  // e.g., {"size_gb": 16, "speed_mhz": 3200, "price": 3500}
};
```

**Why dynamic attributes?** Instead of creating separate structs for RAM, GPU, and Laptop, we use a JSON map. This means the same code handles any product type without changes.

---

### `include/category.h` — Category Schema Loader
Reads a JSON config file (e.g., `ram.json`) and creates a `CategorySchema` that defines:
- What attributes exist (size, speed, price, etc.)
- Their display names
- Default weights
- Whether higher or lower is better
- What constraints to ask the user

**Why config-driven?** To add a new product category (e.g., Phones), you just create a new JSON file. Zero C++ code changes needed.

---

### `include/data_loader.h` — Product Data Loader
Reads `data/products_*.json` files and creates `Product` objects. Uses the category name to find the right file (e.g., category "ram" → loads `data/products_ram.json`).

**Why separate from category?** Categories define the schema (what attributes exist), while data files contain the actual products. This separation means you can update product prices without touching the category config.

---

### `include/filter.h` — Constraint-Based Filtering
Takes user inputs (e.g., type=DDR4, budget≤5000) and removes products that don't match. Supports three filter operations:
- `==` — exact match (e.g., type must be DDR4)
- `<=` — maximum (e.g., price must be ≤ 5000)
- `>=` — minimum (e.g., VRAM must be ≥ 8GB)

Also supports `"any"` — user can skip a constraint.

**Why structured filtering?** This is deterministic. If a product doesn't meet the constraint, it's out. No AI guessing.

---

### `include/scoring.h` — Scoring & Ranking Engine
The core decision engine. For each product that passed filtering:

1. **Normalize** each attribute to [0, 1] range using min-max normalization
   - For "higher is better" attributes (size, speed): `(value - min) / (max - min)`
   - For "lower is better" attributes (price, power draw): `1 - (value - min) / (max - min)`

2. **Multiply** each normalized value by its weight (set by user or defaults)

3. **Sum** all weighted values → composite score

4. **Rank** products by composite score (highest first)

5. **Confidence** — calculated from the gap between #1 and #2:
   - Gap > 0.10 → HIGH confidence
   - Gap > 0.03 → MEDIUM confidence
   - Gap ≤ 0.03 → LOW confidence

**Why normalization?** Without it, a ₹50,000 GPU price would dominate a 12GB VRAM value. Normalization puts everything on the same 0-1 scale.

**Why user-tunable weights?** Different users care about different things. A gamer might prioritize speed, while a student prioritizes price.

---

### `include/explanation.h` — LLM Integration + Fallback
This is the AI component. It:

1. **Builds a completion-style prompt** from the scoring results:
   ```
   "NVIDIA RTX 4070 12GB is the best Graphics Card for a user who needs
   brand == NVIDIA, price <= 60000. It scored 0.52 out of 1.0 and its
   strongest feature is Benchmark Score. This product is recommended because"
   ```

2. **Sends it to TinyLlama** via `CreateProcess` (Windows API) — captures stdout through a pipe, suppresses stderr

3. **Parses the output** — strips junk, interactive prompts, special tokens

4. **Falls back to template** if:
   - LLM binary not found
   - LLM produces empty output
   - LLM just echoes the prompt without adding anything
   - Output is too short or contains garbage

**Why completion-style prompt?** TinyLlama (1.1B parameters) is a very small model. It works best when given an incomplete sentence to finish, rather than complex instructions.

**Why template fallback?** This is a hallucination guard. If the LLM produces unreliable output, the system automatically uses a deterministic template that cites actual facts from the scoring results. This ensures the user always gets a useful explanation.

**Why CreateProcess instead of system()?** `system()` can't easily capture stdout on Windows. `CreateProcess` with pipes gives us full control over stdin/stdout/stderr, which is needed to capture the LLM's output cleanly.

---

### `include/cli.h` — CLI Interface & Visualization
Handles all user interaction:
- Category selection menu
- Constraint input prompts
- Interactive weight tuning (rate 1-5, normalized to percentages)
- Score breakdown with ASCII bar charts:
  ```
  VRAM (GB)  [########............] 0.12  (w:30%)
  Price      [###############.....] 0.14  (w:25%)
  ```
- Confidence display (HIGH/MEDIUM/LOW with gap value)

**Why ASCII bar charts?** They give an instant visual understanding of why a product scored high — which attributes contributed most.

---

### `include/compat.h` — Compatibility Polyfills
Contains a `clampVal()` function that replaces `std::clamp` (not available in C++14 on older GCC).

**Why needed?** Our compiler (GCC 6.3 / MinGW) only supports C++14. `std::clamp` requires C++17.

---

### `include/nlohmann/json.hpp` — JSON Library
A single-header C++ JSON library (v3.9.1). Used to parse all config and data files.

**Why this library?** It's header-only (no build steps), widely used, and compatible with C++14.

---

### Config Files (`config/categories/*.json`)
Each file defines a product category's schema. Example (`ram.json`):
```json
{
  "category": "ram",
  "display_name": "Laptop / Desktop RAM",
  "attributes": [
    { "key": "size_gb", "display": "Size (GB)", "weight": 0.40, "higher_is_better": true, "type": "numeric" },
    { "key": "speed_mhz", "display": "Speed (MHz)", "weight": 0.25, "higher_is_better": true, "type": "numeric" }
  ],
  "constraints": [
    { "attribute": "type", "prompt": "Enter RAM type (DDR3/DDR4/DDR5)", "filter_op": "==", "input_type": "string" }
  ]
}
```

**Why JSON configs?** Adding a new category (e.g., Phones) requires zero C++ changes — just add a JSON config and data file.

---

### Data Files (`data/products_*.json`)
Each file contains an array of products with their attributes. Example:
```json
{
  "products": [
    { "name": "Kingston Fury Beast 16GB DDR4 3200MHz", "attributes": { "type": "DDR4", "size_gb": 16, "speed_mhz": 3200, "price": 3500, "brand_tier": 5 } }
  ]
}
```

---

## How to Compile & Run

```bash
g++ -std=c++14 main.cpp -o ccds.exe
.\ccds.exe
```

**Requirements:**
- GCC 6.3+ (MinGW on Windows)
- TinyLlama GGUF model in `models/` folder
- llama.cpp's `llama-completion.exe` binary

---

## How to Add a New Category (e.g., Phones)

1. Create `config/categories/phone.json` — define attributes, weights, constraints
2. Create `data/products_phone.json` — add 10+ products
3. Add `"config/categories/phone.json"` to the `categoryFiles` vector in `main.cpp`
4. Recompile — done!

No other code changes needed. That's the power of config-driven design.

---

## Key Design Decisions

| Decision | Why |
|---|---|
| **LLM as explainer only** | Prevents hallucination in critical decision logic |
| **Config-driven categories** | Zero-code extensibility for new product types |
| **Weighted scoring** | Transparent, reproducible, user-tunable decisions |
| **Min-max normalization** | Puts different units (GB, MHz, ₹) on same scale |
| **Template fallback** | Guarantees useful output even when LLM fails |
| **Header-only architecture** | Simple compilation, no complex build systems |
| **CreateProcess for LLM** | Clean stdout capture on Windows |
| **Dynamic JSON attributes** | One `Product` struct works for all categories |

---

## Technologies Used

- **C++14** — Core language
- **nlohmann/json** — JSON parsing (single-header library)
- **llama.cpp** — Local LLM inference engine
- **TinyLlama 1.1B** — Small language model for explanation generation
- **Windows API** (CreateProcess) — Process management for LLM invocation
