#include "sb_gbak_enhanced.h"
#include <iostream>
#include <signal.h>
#include <cstdlib>

// Signal handling for graceful shutdown
volatile sig_atomic_t interrupted = 0;
GBakEnhanced* global_gbak_instance = nullptr;

void signal_handler(int signal) {
    interrupted = 1;
    if (global_gbak_instance) {
        std::cout << "\nReceived signal " << signal << ", cancelling operation..." << std::endl;
        global_gbak_instance->cancelOperation();
    }
}

void setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#ifdef SIGQUIT
    signal(SIGQUIT, signal_handler);
#endif
}

int main(int argc, char* argv[]) {
    try {
        // Setup signal handling
        setup_signal_handlers();
        
        // Use the enhanced main class
        return GBakEnhancedMain::main(argc, argv);
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}