#pragma once
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <ctime>
#include <vector>
#include <cerrno>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif
#include "scoring.h"
#include "category.h"

inline std::string scratchDirectory() {
    return "temp";
}

inline std::string scratchFile(const std::string& name) {
    return scratchDirectory() + "/" + name;
}

inline void ensureScratchDirectory() {
#ifdef _WIN32
    if (_mkdir(scratchDirectory().c_str()) != 0 && errno != EEXIST) {
        return;
    }
#else
    if (mkdir(scratchDirectory().c_str(), 0755) != 0 && errno != EEXIST) {
        return;
    }
#endif
}

// Context passed to the explanation engine — everything the LLM needs
struct DecisionContext {
    std::string category_name;
    ScoredProduct best;
    std::vector<ScoredProduct> top_results;
    std::string constraints_summary;
    std::string confidence;
    double confidence_gap;
};

// Build a simple prompt that tiny models can handle
inline std::string buildExplanationPrompt(const DecisionContext& ctx) {
    // Find the top contributing attribute
    std::string top_attr;
    double top_contrib = 0;
    for (auto& b : ctx.best.breakdown) {
        if (b.contribution > top_contrib) {
            top_contrib = b.contribution;
            top_attr = b.display;
        }
    }

    std::ostringstream ss;
    ss << "Why is " << ctx.best.product.name
       << " the best " << ctx.category_name << "? "
       << "It scored " << std::fixed << std::setprecision(3) << ctx.best.total_score
       << " out of 1.0. Its best feature is " << top_attr << ". "
       << "The user needs: " << ctx.constraints_summary << ". "
       << "Explain in 2 sentences why this is a good choice."
       << "\nRespond in 2-3 sentences only.\n";
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
    ensureScratchDirectory();

    // Use unique temp filenames to avoid conflicts with locked files from previous runs
    std::string suffix = std::to_string(std::clock());
    std::string prompt_file = scratchFile("ccds_prompt_" + suffix + ".tmp");
    std::string output_file = scratchFile("ccds_output_" + suffix + ".tmp");
    // Also try to clean up common stale files
    std::remove(scratchFile("ccds_llm_prompt.tmp").c_str());
    std::remove(scratchFile("ccds_llm_output.tmp").c_str());

    // Write prompt to a temp file to avoid command-line quoting issues
    {
        std::ofstream pf(prompt_file);
        if (!pf.is_open()) return templateExplanation(ctx);
        pf << prompt;
        pf.close();
    }

        std::string cmd = "\"" + llama_cli_path + "\""
                        + " -m \"" + model_path + "\""
                        + " --simple-io"
                        + " -no-cnv"
                        + " -st"
                        + " -n 150"
                        + " -f " + prompt_file;

        #ifdef _WIN32
        // Use CreateProcess with pipes to capture stdout/stderr on Windows
        // std::cout << "   [DEBUG] Running LLM command (CreateProcess): " << cmd << "\n";

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        HANDLE hRead = NULL, hWrite = NULL;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
            std::remove(prompt_file.c_str());
            return templateExplanation(ctx);
        }
        // Ensure the read handle is not inherited
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hNull == INVALID_HANDLE_VALUE) {
            CloseHandle(hRead);
            CloseHandle(hWrite);
            std::remove(prompt_file.c_str());
            return templateExplanation(ctx);
        }

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hWrite;
        si.hStdError = hNull;
        si.hStdInput = NULL;

        ZeroMemory(&pi, sizeof(pi));

        // Create mutable command line
        std::vector<char> cmdline(cmd.begin(), cmd.end());
        cmdline.push_back('\0');

        BOOL ok = CreateProcessA(NULL, cmdline.data(), NULL, NULL, TRUE,
                                 CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        // Close the write end in the parent so we can read EOF
        CloseHandle(hWrite);
        CloseHandle(hNull);

        if (!ok) {
            CloseHandle(hRead);
            std::remove(prompt_file.c_str());
            return templateExplanation(ctx);
        }

        // Read output from child
        std::string raw_output;
        const DWORD bufSize = 4096;
        char buffer[bufSize];
        DWORD readBytes = 0;
        while (ReadFile(hRead, buffer, bufSize, &readBytes, NULL) && readBytes > 0) {
            raw_output.append(buffer, buffer + readBytes);
        }

        // Wait for process to exit and get exit code
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hRead);

        // std::cout << "   [DEBUG] LLM exit code: " << exitCode << "\n";

        // Clean up prompt file
        std::remove(prompt_file.c_str());

        if (raw_output.empty()) {
            return templateExplanation(ctx);
        }

        #else
        // Non-Windows: fall back to system() with redirected output file
        std::string full_cmd = cmd + " > " + output_file + " 2>&1";
        std::cout << "   [DEBUG] Running LLM command: " << full_cmd << "\n";
        int ret = system(full_cmd.c_str());
        std::cout << "   [DEBUG] LLM command exit code: " << ret << "\n";

        // Clean up prompt file immediately
        std::remove(prompt_file.c_str());

        // Read redirected output file
        std::ifstream f(output_file);
        if (!f.is_open()) {
            std::remove(output_file.c_str());
            return templateExplanation(ctx);
        }

        std::string raw_output((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        f.close();
        std::remove(output_file.c_str());

        if (raw_output.empty()) {
            return templateExplanation(ctx);
        }
        #endif
    // With --simple-io, output is clean: just the LLM response text
    // followed by "> " interactive prompts and possible junk.
    std::string llm_response = raw_output;
    size_t cut = std::string::npos;

    // Debug output (uncomment for troubleshooting):
    // std::string debug_preview = raw_output.substr(0, 200);
    // for (auto& c : debug_preview) { if (c == '\n') c = ' '; }
    // std::cout << "   [DEBUG] Raw output (" << raw_output.size() << " bytes): " << debug_preview << "...\n";

    // The LLM may echo the prompt before the generated answer.
    // The prompt always ends with "Respond in 2-3 sentences only."
    // Find that marker and strip everything up to and including that line.
    size_t prompt_end = llm_response.find("Respond in 2-3 sentences only.");
    if (prompt_end != std::string::npos) {
        // Move past the marker line
        size_t line_end = llm_response.find('\n', prompt_end);
        if (line_end != std::string::npos) {
            llm_response = llm_response.substr(line_end + 1);
        } else {
            llm_response = ""; // The entire output was just the prompt
        }
    }
    // Fallback: try exact prefix match
    else if (llm_response.find(prompt) == 0) {
        llm_response = llm_response.substr(prompt.size());
    }

    size_t leading = llm_response.find_first_not_of(" \t\n\r");
    if (leading != std::string::npos) {
        llm_response = llm_response.substr(leading);
    }

    // Drop a trailing end-of-text marker if present.
    cut = llm_response.find("[end of text]");
    if (cut != std::string::npos) {
        llm_response = llm_response.substr(0, cut);
    }

    // Cut off at first interactive prompt marker "\n> "
    cut = llm_response.find("\n> ");
    if (cut != std::string::npos) {
        llm_response = llm_response.substr(0, cut);
    }

    // Cut off at any special token markers like <|user|>, <|end|>, etc.
    cut = llm_response.find("<|");
    if (cut != std::string::npos) {
        llm_response = llm_response.substr(0, cut);
    }

    // Cut off at stats line
    cut = llm_response.find("[ Prompt:");
    if (cut != std::string::npos) {
        llm_response = llm_response.substr(0, cut);
    }

    // Trim whitespace and trailing '>'
    size_t start = llm_response.find_first_not_of(" \t\n\r");
    size_t end = llm_response.find_last_not_of(" \t\n\r>");
    if (start == std::string::npos || end == std::string::npos || end < start) {
        return templateExplanation(ctx);
    }
    llm_response = llm_response.substr(start, end - start + 1);

    if (llm_response == prompt || llm_response.find("You are a product advisor.") == 0 ||
        llm_response.find("Respond in 2-3 sentences only.") == 0) {
        return templateExplanation(ctx);
    }

    if (llm_response.empty() || llm_response.size() < 10) {
        return templateExplanation(ctx);
    }

    return llm_response;
}
