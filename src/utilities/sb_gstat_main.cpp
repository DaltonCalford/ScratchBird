#include "sb_gstat_enhanced.h"
#include "sb_gstat_classic.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

// Version information
static const char* VERSION = "sb_gstat version SB-T0.6.0.1 ScratchBird 0.6 f90eae0";

// Mode detection
enum class GStatMode {
    AUTO_DETECT,
    CLASSIC,
    ENHANCED,
    HYBRID
};

// Show main usage
void showMainUsage() {
    std::cout << "sb_gstat - ScratchBird Database Statistics Utility" << std::endl;
    std::cout << VERSION << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: sb_gstat [mode] [options] database" << std::endl;
    std::cout << std::endl;
    std::cout << "Modes:" << std::endl;
    std::cout << "  --classic      Use classic gstat compatibility mode (direct file reading)" << std::endl;
    std::cout << "  --enhanced     Use enhanced mode with real-time monitoring and web interface" << std::endl;
    std::cout << "  --hybrid       Use both classic and enhanced analysis" << std::endl;
    std::cout << "  (default)      Auto-detect mode based on command-line options" << std::endl;
    std::cout << std::endl;
    std::cout << "Classic Mode Options:" << std::endl;
    std::cout << "  -a             analyze data and index pages" << std::endl;
    std::cout << "  -d             analyze data pages only" << std::endl;
    std::cout << "  -i             analyze index pages only" << std::endl;
    std::cout << "  -h             analyze header page ONLY" << std::endl;
    std::cout << "  -e             analyze database encryption status" << std::endl;
    std::cout << "  -s             analyze system relations" << std::endl;
    std::cout << "  -r             analyze record and version length" << std::endl;
    std::cout << "  -t <table>     analyze specific table" << std::endl;
    std::cout << "  -u <user>      database username" << std::endl;
    std::cout << "  -p <password>  database password" << std::endl;
    std::cout << "  -n             suppress creation date" << std::endl;
    std::cout << std::endl;
    std::cout << "Enhanced Mode Options:" << std::endl;
    std::cout << "  -web           start web interface" << std::endl;
    std::cout << "  -monitor       start continuous monitoring" << std::endl;
    std::cout << "  -alerts        enable alerts" << std::endl;
    std::cout << "  -format <fmt>  output format (table|csv|json|xml|html)" << std::endl;
    std::cout << "  -analyze <type> perform analysis (performance|health|trends)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  sb_gstat mydb.fdb                    # Auto-detect mode" << std::endl;
    std::cout << "  sb_gstat --classic -a mydb.fdb       # Classic analysis" << std::endl;
    std::cout << "  sb_gstat --enhanced -web mydb.fdb    # Enhanced with web interface" << std::endl;
    std::cout << "  sb_gstat --hybrid -a -web mydb.fdb   # Both classic and enhanced" << std::endl;
    std::cout << std::endl;
    std::cout << "For mode-specific help:" << std::endl;
    std::cout << "  sb_gstat --classic --help" << std::endl;
    std::cout << "  sb_gstat --enhanced --help" << std::endl;
}

// Detect mode from command line
GStatMode detectMode(int argc, char* argv[]) {
    // Check for explicit mode specification
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--classic") == 0) {
            return GStatMode::CLASSIC;
        } else if (strcmp(argv[i], "--enhanced") == 0) {
            return GStatMode::ENHANCED;
        } else if (strcmp(argv[i], "--hybrid") == 0) {
            return GStatMode::HYBRID;
        }
    }
    
    // Auto-detect based on options
    bool has_classic_options = false;
    bool has_enhanced_options = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        // Classic options
        if (arg == "-a" || arg == "-d" || arg == "-i" || arg == "-h" || 
            arg == "-e" || arg == "-s" || arg == "-r" || arg == "-n" ||
            arg.substr(0, 2) == "-t" || arg.substr(0, 2) == "-u" || 
            arg.substr(0, 2) == "-p") {
            has_classic_options = true;
        }
        
        // Enhanced options
        if (arg == "-web" || arg == "-monitor" || arg == "-alerts" ||
            arg.substr(0, 8) == "-analyze" || arg.substr(0, 7) == "-format" ||
            arg.substr(0, 9) == "-webport" || arg.substr(0, 8) == "-webaddr") {
            has_enhanced_options = true;
        }
    }
    
    // Decision logic
    if (has_classic_options && has_enhanced_options) {
        return GStatMode::HYBRID;
    } else if (has_classic_options) {
        return GStatMode::CLASSIC;
    } else if (has_enhanced_options) {
        return GStatMode::ENHANCED;
    } else {
        // Default to classic for compatibility
        return GStatMode::CLASSIC;
    }
}

// Filter arguments for specific mode
std::vector<char*> filterArgumentsForMode(int argc, char* argv[], GStatMode mode) {
    std::vector<char*> filtered_args;
    
    // Always include program name
    filtered_args.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        // Skip mode specifiers
        if (arg == "--classic" || arg == "--enhanced" || arg == "--hybrid") {
            continue;
        }
        
        // For classic mode, skip enhanced options
        if (mode == GStatMode::CLASSIC) {
            if (arg == "-web" || arg == "-monitor" || arg == "-alerts" ||
                arg.substr(0, 8) == "-analyze" || arg.substr(0, 7) == "-format" ||
                arg.substr(0, 9) == "-webport" || arg.substr(0, 8) == "-webaddr" ||
                arg.substr(0, 7) == "-webssl" || arg.substr(0, 8) == "-webcors") {
                // Skip enhanced options and their arguments
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    i++; // Skip argument too
                }
                continue;
            }
        }
        
        // For enhanced mode, convert some classic options
        if (mode == GStatMode::ENHANCED) {
            if (arg == "-a") {
                filtered_args.push_back(const_cast<char*>("-all"));
                continue;
            } else if (arg == "-d") {
                filtered_args.push_back(const_cast<char*>("-tables"));
                continue;
            } else if (arg == "-i") {
                filtered_args.push_back(const_cast<char*>("-indexes"));
                continue;
            } else if (arg == "-h") {
                filtered_args.push_back(const_cast<char*>("-database"));
                continue;
            } else if (arg == "-s") {
                // System tables - pass through
                continue;
            } else if (arg == "-r") {
                // Record analysis - pass through
                continue;
            } else if (arg == "-n") {
                // Suppress creation date - pass through
                continue;
            } else if (arg.substr(0, 2) == "-t") {
                // Table filter - convert to --table
                filtered_args.push_back(const_cast<char*>("-table"));
                if (arg.length() > 2) {
                    // Handle -ttablename format
                    filtered_args.push_back(const_cast<char*>(arg.c_str() + 2));
                } else if (i + 1 < argc) {
                    // Handle -t tablename format
                    i++;
                    filtered_args.push_back(argv[i]);
                }
                continue;
            }
        }
        
        // Pass through all other arguments
        filtered_args.push_back(argv[i]);
    }
    
    return filtered_args;
}

// Main function
int main(int argc, char* argv[]) {
    try {
        // Check for help or version
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-?") == 0) {
                showMainUsage();
                return 0;
            } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-z") == 0) {
                std::cout << VERSION << std::endl;
                return 0;
            }
        }
        
        if (argc < 2) {
            showMainUsage();
            return 1;
        }
        
        // Detect mode
        GStatMode mode = detectMode(argc, argv);
        
        std::cout << "ScratchBird GSTAT - ";
        switch (mode) {
            case GStatMode::CLASSIC:
                std::cout << "Classic Mode (Direct File Analysis)" << std::endl;
                break;
            case GStatMode::ENHANCED:
                std::cout << "Enhanced Mode (Real-time Monitoring)" << std::endl;
                break;
            case GStatMode::HYBRID:
                std::cout << "Hybrid Mode (Classic + Enhanced)" << std::endl;
                break;
            default:
                std::cout << "Auto-Detect Mode" << std::endl;
                break;
        }
        std::cout << std::endl;
        
        int result = 0;
        
        // Execute based on mode
        switch (mode) {
            case GStatMode::CLASSIC: {
                auto filtered_args = filterArgumentsForMode(argc, argv, GStatMode::CLASSIC);
                result = GSTATClassicMain::main(static_cast<int>(filtered_args.size()), filtered_args.data());
                break;
            }
            
            case GStatMode::ENHANCED: {
                auto filtered_args = filterArgumentsForMode(argc, argv, GStatMode::ENHANCED);
                
                // Use enhanced main function (would need to be implemented)
                std::cout << "Enhanced mode execution..." << std::endl;
                std::cout << "Note: Enhanced mode would use the existing sb_gstat_enhanced_main.cpp" << std::endl;
                std::cout << "For now, showing that enhanced mode would be called with:" << std::endl;
                for (size_t i = 0; i < filtered_args.size(); i++) {
                    std::cout << "  arg[" << i << "]: " << filtered_args[i] << std::endl;
                }
                result = 0; // Placeholder
                break;
            }
            
            case GStatMode::HYBRID: {
                std::cout << "Hybrid mode: Running both classic and enhanced analysis..." << std::endl;
                std::cout << std::endl;
                
                // First run classic analysis
                std::cout << "=== Classic Analysis ===" << std::endl;
                auto classic_args = filterArgumentsForMode(argc, argv, GStatMode::CLASSIC);
                int classic_result = GSTATClassicMain::main(static_cast<int>(classic_args.size()), classic_args.data());
                
                std::cout << std::endl;
                std::cout << "=== Enhanced Analysis ===" << std::endl;
                
                // Then run enhanced analysis (would be implemented)
                std::cout << "Enhanced analysis would run here..." << std::endl;
                
                result = classic_result; // Use classic result for now
                break;
            }
            
            default:
                showMainUsage();
                result = 1;
                break;
        }
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}