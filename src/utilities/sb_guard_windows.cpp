/**
 * ScratchBird Guard Utility - Windows Implementation
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
#include <windows.h>
#include <process.h>

static const char* VERSION = "sb_guard version SB-T0.5.0.1 ScratchBird 0.5 f90eae0";

class ScratchBirdGuard {
private:
    std::string serverPath;
    std::vector<std::string> serverArgs;
    bool signalReceived = false;
    HANDLE serverProcess = nullptr;
    
    void showUsage() {
        std::cout << "ScratchBird Guardian - Process Monitor\n\n";
        std::cout << "Usage: sb_guard [options] [server_path [server_args...]]\n\n";
        std::cout << "Options:\n";
        std::cout << "  -h, --help     Show this help message\n";
        std::cout << "  -z, --version  Show version information\n";
        std::cout << "  -f, --forever  Run forever (restart on exit)\n";
        std::cout << "  -o, --once     Run once (don't restart on exit)\n";
        std::cout << "  -s, --service  Run as Windows service\n\n";
        std::cout << "Default server path: C:\\Program Files\\ScratchBird\\bin\\scratchbird.exe\n";
        std::cout << "Default behavior: restart server if it exits unexpectedly\n";
    }
    
    void showVersion() {
        std::cout << VERSION << std::endl;
    }
    
    static BOOL WINAPI consoleHandler(DWORD dwType) {
        switch (dwType) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
                std::cout << "Guardian received shutdown signal, stopping..." << std::endl;
                return TRUE;
        }
        return FALSE;
    }
    
    void setupSignalHandlers() {
        SetConsoleCtrlHandler(consoleHandler, TRUE);
    }
    
    bool startServer() {
        std::cout << "Starting ScratchBird server: " << serverPath << std::endl;
        
        // Build command line
        std::string commandLine = serverPath;
        for (const auto& arg : serverArgs) {
            commandLine += " " + arg;
        }
        
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        
        // Start the child process
        if (CreateProcess(NULL, const_cast<char*>(commandLine.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            serverProcess = pi.hProcess;
            CloseHandle(pi.hThread);
            std::cout << "Server started with PID: " << pi.dwProcessId << std::endl;
            return true;
        } else {
            std::cerr << "Failed to start server, error: " << GetLastError() << std::endl;
            return false;
        }
    }
    
    int waitForServer() {
        if (serverProcess == nullptr) return -1;
        
        DWORD result = WaitForSingleObject(serverProcess, INFINITE);
        
        if (result == WAIT_OBJECT_0) {
            DWORD exitCode;
            if (GetExitCodeProcess(serverProcess, &exitCode)) {
                std::cout << "Server exited with code: " << exitCode << std::endl;
                CloseHandle(serverProcess);
                serverProcess = nullptr;
                return static_cast<int>(exitCode);
            }
        }
        
        return -1;
    }
    
public:
    ScratchBirdGuard() : serverPath("C:\\Program Files\\ScratchBird\\bin\\scratchbird.exe") {}
    
    int run(int argc, char* argv[]) {
        bool runForever = true;
        bool runAsService = false;
        
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
            } else if (arg == "-s" || arg == "--service") {
                runAsService = true;
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
        
        setupSignalHandlers();
        
        std::cout << "ScratchBird Guardian starting..." << std::endl;
        std::cout << "Server path: " << serverPath << std::endl;
        std::cout << "Run mode: " << (runForever ? "forever" : "once") << std::endl;
        
        if (runAsService) {
            std::cout << "Service mode: Enabled" << std::endl;
        }
        
        int restartCount = 0;
        
        do {
            if (restartCount > 0) {
                std::cout << "Restart attempt #" << restartCount << std::endl;
                Sleep(5000); // 5 second delay
            }
            
            if (!startServer()) {
                std::cerr << "Failed to start server" << std::endl;
                return 1;
            }
            
            int exitCode = waitForServer();
            
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