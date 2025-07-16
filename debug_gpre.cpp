#include <iostream>
#include <string>

// Minimal test to check command line parsing
int main(int argc, char* argv[]) {
    std::cout << "Arguments received: " << argc << std::endl;
    for (int i = 0; i < argc; i++) {
        std::cout << "  arg[" << i << "] = '" << argv[i] << "'" << std::endl;
    }
    
    bool manual_found = false;
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "-manual") {
            manual_found = true;
            break;
        }
    }
    
    std::cout << "Manual flag found: " << (manual_found ? "YES" : "NO") << std::endl;
    
    return 0;
}