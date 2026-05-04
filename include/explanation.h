#pragma once
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include "scoring.h"
#include "category.h"

// Context passed to the explanation engine — everything the LLM needs
struct DecisionContext {
    std::string category_name;
    ScoredProduct best;
    std::vector<ScoredProduct> top_results;
    std::string constraints_summary;
    std::string confidence;
    double confidence_gap;
};

// Build a highly constrained prompt so the LLM cannot hallucinate
inline std::string buildExplanationPrompt(const DecisionContext& ctx) {
    std::ostringstream ss;
    ss << "You are a product advisor. Based ONLY on the facts below, "
       << "write a brief 2-3 sentence explanation of why this product is the best choice. "
       << "Do NOT invent facts. Do NOT mention products not listed.\n\n";
    ss << "CATEGORY: " << ctx.category_name << "\n";
    ss << "USER NEEDS: " << ctx.constraints_summary << "\n";
    ss << "BEST PRODUCT: " << ctx.best.product.name << "\n";
    ss << "COMPOSITE SCORE: " << std::fixed << std::setprecision(3) << ctx.best.total_score << " / 1.000\n";
    ss << "CONFIDENCE: " << ctx.confidence << "\n";
    ss << "SCORE BREAKDOWN:\n";

    for (auto& b : ctx.best.breakdown) {
        ss << "  - " << b.display << ": normalized=" << std::fixed << std::setprecision(2)
           << b.normalized << ", weight=" << (int)(b.weight * 100) << "%, contribution="
           << std::setprecision(3) << b.contribution << "\n";
    }

    if (ctx.top_results.size() > 1) {
        ss << "\nRUNNER UP: " << ctx.top_results[1].product.name
           << " (score: " << std::fixed << std::setprecision(3) << ctx.top_results[1].total_score << ")\n";
    }

    ss << "\nRespond in 2-3 sentences only. Be specific about attribute values.\n";
    return ss.str();
}

// Template-based fallback explanation (no LLM needed)
inline std::string templateExplanation(const DecisionContext& ctx) {
    std::ostringstream ss;
    ss << "Based on your requirements (" << ctx.constraints_summary << "), "
       << ctx.best.product.name << " scored highest with a composite score of "
       << std::fixed << std::setprecision(3) << ctx.best.total_score << ". ";

    // Find the top contributing attribute
    std::string top_attr;
    double top_contrib = 0;
    for (auto& b : ctx.best.breakdown) {
        if (b.contribution > top_contrib) {
            top_contrib = b.contribution;
            top_attr = b.display;
        }
    }

    ss << "Its strongest advantage is in " << top_attr
       << " (contributing " << std::fixed << std::setprecision(1) << (top_contrib * 100)
       << "% of the total score). ";

    ss << "Confidence in this recommendation is " << ctx.confidence << ".";
    return ss.str();
}

// Call LLM via system() and capture output. Falls back to template if LLM fails.
inline std::string generateExplanation(const DecisionContext& ctx,
                                        const std::string& llama_cli_path,
                                        const std::string& model_path,
                                        bool use_llm = true) {
    if (!use_llm) {
        return templateExplanation(ctx);
    }

    std::string prompt = buildExplanationPrompt(ctx);

    // Escape quotes in the prompt for command line
    std::string escaped_prompt;
    for (char c : prompt) {
        if (c == '"') escaped_prompt += "\\\"";
        else if (c == '\n') escaped_prompt += " ";
        else escaped_prompt += c;
    }

    // Build command — redirect output to a temp file for capture
    std::string temp_file = "ccds_llm_output.tmp";
    std::string command = "\"" + llama_cli_path + "\" -m \"" + model_path
                        + "\" --no-display-prompt"
                        + " -n 150"
                        + " -p \"" + escaped_prompt + "\""
                        + " 2>nul > " + temp_file;

    int ret = system(command.c_str());

    if (ret != 0) {
        return templateExplanation(ctx);
    }

    // Read the LLM output
    std::ifstream f(temp_file);
    if (!f.is_open()) {
        return templateExplanation(ctx);
    }
    std::string llm_output((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    f.close();

    // Clean up temp file
    std::remove(temp_file.c_str());

    // Validate: non-empty, reasonable length
    if (llm_output.empty() || llm_output.size() > 1000) {
        return templateExplanation(ctx);
    }

    // Trim whitespace
    size_t start = llm_output.find_first_not_of(" \t\n\r");
    size_t end = llm_output.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return templateExplanation(ctx);
    llm_output = llm_output.substr(start, end - start + 1);

    if (llm_output.empty()) {
        return templateExplanation(ctx);
    }

    return llm_output;
}
