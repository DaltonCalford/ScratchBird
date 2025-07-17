/**
 * ScratchBird Guard Utility - Modern C++17 Implementation
 * Based on Firebird's guard utility
 * 
 * The Guardian is a simple process monitor that restarts the server
 * process if it terminates unexpectedly.
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

static const char* VERSION = "sb_guard version SB-T0.5.0.1 ScratchBird 0.5 f90eae0";

class ScratchBirdGuard {
private:
    std::string serverPath;
    std::vector<std::string> serverArgs;
    bool signalReceived = false;
    pid_t serverPid = 0;
    
    void showUsage() {
        std::cout << "ScratchBird Guardian - Process Monitor\n\n";
        std::cout << "Usage: sb_guard [options] [server_path [server_args...]]\n\n";
        std::cout << "Options:\n";
        std::cout << "  -h, --help     Show this help message\n";
        std::cout << "  -z, --version  Show version information\n";
        std::cout << "  -f, --forever  Run forever (restart on exit)\n";
        std::cout << "  -o, --once     Run once (don't restart on exit)\n";
        std::cout << "  -s, --signore  Ignore signals (let server handle them)\n\n";
        std::cout << "Default server path: /usr/local/scratchbird/bin/scratchbird\n";
        std::cout << "Default behavior: restart server if it exits unexpectedly\n";
    }
    
    void showVersion() {
        std::cout << VERSION << std::endl;
    }
    
    static void signalHandler(int sig) {
        std::cout << "Guardian received signal " << sig << ", shutting down..." << std::endl;
        // Set a flag to indicate we should stop
        // Note: This is a simplified signal handler
        exit(0);
    }
    
    void setupSignalHandlers() {
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
    }
    
    bool startServer() {
        std::cout << "Starting ScratchBird server: " << serverPath << std::endl;
        
        serverPid = fork();
        if (serverPid == 0) {
            // Child process - exec the server
            std::vector<char*> args;
            args.push_back(const_cast<char*>(serverPath.c_str()));
            
            for (const auto& arg : serverArgs) {
                args.push_back(const_cast<char*>(arg.c_str()));
            }
            args.push_back(nullptr);
            
            execv(serverPath.c_str(), args.data());
            perror("Failed to start server");
            exit(1);
        } else if (serverPid > 0) {
            // Parent process
            std::cout << "Server started with PID: " << serverPid << std::endl;
            return true;
        } else {
            perror("Failed to fork server process");
            return false;
        }
    }
    
    int waitForServer() {
        int status;
        pid_t result = waitpid(serverPid, &status, 0);
        
        if (result == serverPid) {
            if (WIFEXITED(status)) {
                int exitCode = WEXITSTATUS(status);
                std::cout << "Server exited with code: " << exitCode << std::endl;
                return exitCode;
            } else if (WIFSIGNALED(status)) {
                int signal = WTERMSIG(status);
                std::cout << "Server terminated by signal: " << signal << std::endl;
                return -signal;
            }
        }
        
        return -1;
    }
    
public:
    ScratchBirdGuard() : serverPath("/usr/local/scratchbird/bin/scratchbird") {}
    
    int run(int argc, char* argv[]) {
        bool runForever = true;
        bool ignoreSignals = false;
        
        // Parse command line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                showUsage();
                return 0;
            } else if (arg == "-z" || arg == "--version") {
                showVersion();
                return 0;
            } else if (arg == "-f" || arg == "--forever") {
                runForever = true;
            } else if (arg == "-o" || arg == "--once") {
                runForever = false;
            } else if (arg == "-s" || arg == "--signore") {
                ignoreSignals = true;
            } else {
                // First non-option argument is server path
                serverPath = arg;
                // Remaining arguments are server arguments
                for (int j = i + 1; j < argc; ++j) {
                    serverArgs.push_back(argv[j]);
                }
                break;
            }
        }
        
        if (!ignoreSignals) {
            setupSignalHandlers();
        }
        
        std::cout << "ScratchBird Guardian starting..." << std::endl;
        std::cout << "Server path: " << serverPath << std::endl;
        std::cout << "Run mode: " << (runForever ? "forever" : "once") << std::endl;
        
        int restartCount = 0;
        
        do {
            if (restartCount > 0) {
                std::cout << "Restart attempt #" << restartCount << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
            
            if (!startServer()) {
                std::cerr << "Failed to start server" << std::endl;
                return 1;
            }
            
            int exitCode = waitForServer();
            serverPid = 0;
            
            if (exitCode == 0) {
                std::cout << "Server shut down normally" << std::endl;
                if (!runForever) {
                    break;
                }
            } else {
                std::cout << "Server exited unexpectedly with code: " << exitCode << std::endl;
                if (runForever) {
                    std::cout << "Restarting server..." << std::endl;
                    restartCount++;
                    
                    // Limit restart attempts to prevent infinite loops
                    if (restartCount > 10) {
                        std::cerr << "Too many restart attempts, giving up" << std::endl;
                        return 1;
                    }
                }
            }
            
        } while (runForever && !signalReceived);
        
        std::cout << "Guardian shutting down" << std::endl;
        return 0;
    }
};

int main(int argc, char* argv[]) {
    try {
        ScratchBirdGuard guard;
        return guard.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}