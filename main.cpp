#include <iostream>
#include <cstdlib>

using namespace std;

int main() {
    string prompt = "Explain why 16GB DDR4 RAM is a good choice for a laptop under budget.";

    string command = "C:\\Users\\prajn\\llama.cpp\\build\\bin\\Release\\llama-cli.exe -m C:\\Users\\prajn\\OneDrive\\Desktop\\CCDS\\models\\tinyllama.gguf -p \"" + prompt + "\"";

    system(command.c_str());

    return 0;
}