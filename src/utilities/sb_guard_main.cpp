#include "sb_guard_enhanced.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <filesystem>
#include <csignal>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// Global guardian instance for signal handling
static GuardEnhanced* g_guardian_instance = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    if (g_guardian_instance) {
        std::cout << "\nReceived signal " << signal << ", shutting down guardian..." << std::endl;
        g_guardian_instance->requestShutdown();
    }
}

// Command-line argument parser
class GuardCommandParser {
private:
    struct CommandOptions {
        // Operation mode
        bool start_mode = false;
        bool stop_mode = false;
        bool status_mode = false;
        bool check_mode = false;
        bool restart_mode = false;
        bool daemon_mode = false;
        bool help_mode = false;
        bool version_mode = false;
        
        // Configuration
        std::string config_file = "guardian.conf";
        std::string pid_file = "guardian.pid";
        std::string log_file = "guardian.log";
        
        // Guardian options
        SBEnhanced::GuardianMode guardian_mode = SBEnhanced::GuardianMode::AUTO_RESTART;
        SBEnhanced::MonitoringLevel monitoring_level = SBEnhanced::MonitoringLevel::STANDARD;
        uint32_t check_interval = 30;
        uint32_t max_failures = 3;
        bool enable_alerts = true;
        
        // Database operations
        std::string database_path;
        std::string database_alias;
        
        // Reporting
        bool generate_report = false;
        std::string report_path;
        std::string report_format = "TEXT";
        
        // General options
        bool verbose = false;
        bool quiet = false;
    };
    
    CommandOptions options;
    std::vector<std::string> errors;
    
public:
    bool parseArguments(int argc, char* argv[]) {
        if (argc < 2) {
            showUsage(argv[0]);
            return false;
        }
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                options.help_mode = true;
                return true;
            }
            else if (arg == "-v" || arg == "--version") {
                options.version_mode = true;
                return true;
            }
            else if (arg == "start" || arg == "-start") {
                options.start_mode = true;
            }
            else if (arg == "stop" || arg == "-stop") {
                options.stop_mode = true;
            }
            else if (arg == "status" || arg == "-status") {
                options.status_mode = true;
            }
            else if (arg == "check" || arg == "-check") {
                options.check_mode = true;
            }
            else if (arg == "restart" || arg == "-restart") {
                options.restart_mode = true;
            }
            else if (arg == "-daemon" || arg == "--daemon") {
                options.daemon_mode = true;
            }
            else if (arg == "-c" || arg == "--config") {
                if (i + 1 < argc) {
                    options.config_file = argv[++i];
                } else {
                    errors.push_back("Configuration file path required after " + arg);
                }
            }
            else if (arg == "-p" || arg == "--pid-file") {
                if (i + 1 < argc) {
                    options.pid_file = argv[++i];
                } else {
                    errors.push_back("PID file path required after " + arg);
                }
            }
            else if (arg == "-l" || arg == "--log-file") {
                if (i + 1 < argc) {
                    options.log_file = argv[++i];
                } else {
                    errors.push_back("Log file path required after " + arg);
                }
            }
            else if (arg == "-db" || arg == "--database") {
                if (i + 1 < argc) {
                    options.database_path = argv[++i];
                } else {
                    errors.push_back("Database path required after " + arg);
                }
            }
            else if (arg == "-alias" || arg == "--alias") {
                if (i + 1 < argc) {
                    options.database_alias = argv[++i];
                } else {
                    errors.push_back("Database alias required after " + arg);
                }
            }
            else if (arg == "-mode" || arg == "--guardian-mode") {
                if (i + 1 < argc) {
                    std::string mode_str = argv[++i];
                    options.guardian_mode = parseGuardianMode(mode_str);
                } else {
                    errors.push_back("Guardian mode required after " + arg);
                }
            }
            else if (arg == "-monitoring" || arg == "--monitoring-level") {
                if (i + 1 < argc) {
                    std::string level_str = argv[++i];
                    options.monitoring_level = parseMonitoringLevel(level_str);
                } else {
                    errors.push_back("Monitoring level required after " + arg);
                }
            }
            else if (arg == "-interval" || arg == "--check-interval") {
                if (i + 1 < argc) {
                    try {
                        options.check_interval = static_cast<uint32_t>(std::stoi(argv[++i]));
                    } catch (const std::exception&) {
                        errors.push_back("Invalid check interval: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("Check interval required after " + arg);
                }
            }
            else if (arg == "-max-failures" || arg == "--max-failures") {
                if (i + 1 < argc) {
                    try {
                        options.max_failures = static_cast<uint32_t>(std::stoi(argv[++i]));
                    } catch (const std::exception&) {
                        errors.push_back("Invalid max failures: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("Max failures required after " + arg);
                }
            }
            else if (arg == "-no-alerts" || arg == "--disable-alerts") {
                options.enable_alerts = false;
            }
            else if (arg == "-report" || arg == "--report") {
                options.generate_report = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    options.report_path = argv[++i];
                }
            }
            else if (arg == "-format" || arg == "--report-format") {
                if (i + 1 < argc) {
                    options.report_format = argv[++i];
                    std::transform(options.report_format.begin(), options.report_format.end(),
                                 options.report_format.begin(), ::toupper);
                } else {
                    errors.push_back("Report format required after " + arg);
                }
            }
            else if (arg == "-verbose" || arg == "--verbose") {
                options.verbose = true;
            }
            else if (arg == "-quiet" || arg == "--quiet") {
                options.quiet = true;
            }
            else if (arg[0] != '-') {
                // Positional argument - could be operation or database path
                if (!options.start_mode && !options.stop_mode && !options.status_mode && 
                    !options.check_mode && !options.restart_mode) {
                    if (arg == "start") {
                        options.start_mode = true;
                    } else if (arg == "stop") {
                        options.stop_mode = true;
                    } else if (arg == "status") {
                        options.status_mode = true;
                    } else if (arg == "check") {
                        options.check_mode = true;
                    } else if (arg == "restart") {
                        options.restart_mode = true;
                    } else if (options.database_path.empty()) {
                        options.database_path = arg;
                    }
                }
            }
            else {
                errors.push_back("Unknown option: " + arg);
            }
        }
        
        return validateOptions();
    }
    
    bool executeCommand() {
        if (options.help_mode) {
            showHelp();
            return true;
        }
        
        if (options.version_mode) {
            showVersion();
            return true;
        }
        
        if (options.start_mode) {
            return executeStart();
        }
        else if (options.stop_mode) {
            return executeStop();
        }
        else if (options.status_mode) {
            return executeStatus();
        }
        else if (options.check_mode) {
            return executeCheck();
        }
        else if (options.restart_mode) {
            return executeRestart();
        }
        
        std::cerr << "Error: No operation specified. Use -h for help." << std::endl;
        return false;
    }
    
private:
    SBEnhanced::GuardianMode parseGuardianMode(const std::string& mode_str) {
        std::string mode = mode_str;
        std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
        
        if (mode == "monitor" || mode == "monitor-only") {
            return SBEnhanced::GuardianMode::MONITOR_ONLY;
        } else if (mode == "restart" || mode == "auto-restart") {
            return SBEnhanced::GuardianMode::AUTO_RESTART;
        } else if (mode == "failover") {
            return SBEnhanced::GuardianMode::FAILOVER;
        } else if (mode == "full" || mode == "full-auto") {
            return SBEnhanced::GuardianMode::FULL_AUTO;
        }
        
        return SBEnhanced::GuardianMode::AUTO_RESTART;  // Default
    }
    
    SBEnhanced::MonitoringLevel parseMonitoringLevel(const std::string& level_str) {
        std::string level = level_str;
        std::transform(level.begin(), level.end(), level.begin(), ::tolower);
        
        if (level == "basic") {
            return SBEnhanced::MonitoringLevel::BASIC;
        } else if (level == "standard") {
            return SBEnhanced::MonitoringLevel::STANDARD;
        } else if (level == "comprehensive") {
            return SBEnhanced::MonitoringLevel::COMPREHENSIVE;
        } else if (level == "advanced") {
            return SBEnhanced::MonitoringLevel::ADVANCED;
        }
        
        return SBEnhanced::MonitoringLevel::STANDARD;  // Default
    }
    
    bool validateOptions() {
        // Check for conflicting modes
        int mode_count = 0;
        if (options.start_mode) mode_count++;
        if (options.stop_mode) mode_count++;
        if (options.status_mode) mode_count++;
        if (options.check_mode) mode_count++;
        if (options.restart_mode) mode_count++;
        
        if (mode_count == 0 && !options.help_mode && !options.version_mode) {
            errors.push_back("No operation specified");
        } else if (mode_count > 1) {
            errors.push_back("Only one operation can be specified");
        }
        
        // Validate check mode options
        if (options.check_mode) {
            if (options.database_path.empty() && options.database_alias.empty()) {
                errors.push_back("Database path or alias required for check operation");
            }
        }
        
        // Validate restart mode options
        if (options.restart_mode) {
            if (options.database_path.empty() && options.database_alias.empty()) {
                errors.push_back("Database path or alias required for restart operation");
            }
        }
        
        if (!errors.empty()) {
            for (const auto& error : errors) {
                std::cerr << "Error: " << error << std::endl;
            }
            return false;
        }
        
        return true;
    }
    
    bool executeStart() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced Guardian - Starting Database Guardian Service" << std::endl;
            std::cout << "==================================================================" << std::endl;
            std::cout << "Configuration file: " << options.config_file << std::endl;
            std::cout << "Guardian mode: " << getGuardianModeString(options.guardian_mode) << std::endl;
            std::cout << "Monitoring level: " << getMonitoringLevelString(options.monitoring_level) << std::endl;
            std::cout << "Check interval: " << options.check_interval << " seconds" << std::endl;
            if (options.daemon_mode) {
                std::cout << "Running as daemon: " << options.pid_file << std::endl;
            }
            std::cout << std::endl;
        }
        
        // Setup signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
#ifndef _WIN32
        std::signal(SIGHUP, signalHandler);
#endif
        
        GuardEnhanced guardian;
        g_guardian_instance = &guardian;
        
        // Create guardian options
        SBEnhanced::GuardianOptions guardian_options;
        guardian_options.config_file_path = options.config_file;
        guardian_options.log_file_path = options.log_file;
        guardian_options.pid_file_path = options.pid_file;
        guardian_options.mode = options.guardian_mode;
        guardian_options.monitoring_level = options.monitoring_level;
        guardian_options.global_check_interval_seconds = options.check_interval;
        guardian_options.global_max_failures = options.max_failures;
        guardian_options.enable_alerts = options.enable_alerts;
        guardian_options.enable_logging = true;
        guardian_options.run_as_daemon = options.daemon_mode;
        
        // Set progress callback if verbose
        if (options.verbose) {
            guardian_options.progress_callback = [](const SBEnhanced::GuardianProgress& progress) {
                std::cout << "\r[" << progress.getUptime().count() << "s] "
                         << "Monitoring " << progress.databases_monitored << " databases, "
                         << "Health checks: " << progress.health_checks_performed << ", "
                         << "Alerts: " << progress.alerts_generated
                         << " " << std::flush;
            };
        }
        
        SBEnhanced::GuardianOperationResult result;
        bool success = guardian.startGuardianEnhanced(guardian_options, result);
        
        if (success) {
            if (!options.quiet) {
                std::cout << "Guardian started successfully!" << std::endl;
                
                if (options.verbose) {
                    std::cout << "Monitoring databases..." << std::endl;
                    std::cout << "Press Ctrl+C to stop the guardian." << std::endl;
                }
            }
            
            // If not daemon mode, wait for guardian to complete
            if (!options.daemon_mode) {
                while (guardian.isGuardianActive() && g_guardian_instance) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    
                    // Update progress display if verbose
                    if (options.verbose && guardian_options.progress_callback) {
                        guardian_options.progress_callback(guardian.getCurrentProgress());
                    }
                }
                
                if (options.verbose) {
                    std::cout << std::endl;  // New line after progress
                }
            }
            
            if (!options.quiet) {
                std::cout << "Guardian stopped." << std::endl;
            }
        } else {
            std::cerr << "Failed to start guardian!" << std::endl;
            for (const auto& error : result.errors) {
                std::cerr << "Error: " << error << std::endl;
            }
        }
        
        // Display warnings
        if (!result.warnings.empty() && !options.quiet) {
            std::cout << "\nWarnings:" << std::endl;
            for (const auto& warning : result.warnings) {
                std::cout << "Warning: " << warning << std::endl;
            }
        }
        
        g_guardian_instance = nullptr;
        return success;
    }
    
    bool executeStop() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced Guardian - Stopping Database Guardian Service" << std::endl;
            std::cout << "==================================================================" << std::endl;
        }
        
        GuardEnhanced guardian;
        SBEnhanced::GuardianOperationResult result;
        bool success = guardian.stopGuardianEnhanced(result);
        
        if (success) {
            if (!options.quiet) {
                std::cout << "Guardian stopped successfully!" << std::endl;
            }
        } else {
            std::cerr << "Failed to stop guardian!" << std::endl;
            for (const auto& error : result.errors) {
                std::cerr << "Error: " << error << std::endl;
            }
        }
        
        return success;
    }
    
    bool executeStatus() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced Guardian - Guardian Status Report" << std::endl;
            std::cout << "=====================================================" << std::endl;
        }
        
        GuardEnhanced guardian;
        
        // Check if guardian is running
        bool is_active = guardian.isGuardianActive();
        if (!options.quiet) {
            std::cout << "Guardian Status: " << (is_active ? "RUNNING" : "STOPPED") << std::endl;
        }
        
        if (is_active) {
            // Get guardian progress
            auto progress = guardian.getCurrentProgress();
            
            if (!options.quiet) {
                std::cout << "Uptime: " << progress.getUptime().count() << " seconds" << std::endl;
                std::cout << "Databases monitored: " << progress.databases_monitored << std::endl;
                std::cout << "Health checks performed: " << progress.health_checks_performed << std::endl;
                std::cout << "Restarts performed: " << progress.restarts_performed << std::endl;
                std::cout << "Failovers performed: " << progress.failovers_performed << std::endl;
                std::cout << "Alerts generated: " << progress.alerts_generated << std::endl;
                
                if (!progress.current_database.empty()) {
                    std::cout << "Currently monitoring: " << progress.current_database << std::endl;
                }
            }
            
            // Get database status
            std::vector<SBEnhanced::DatabaseStatus> status_list;
            if (guardian.getAllDatabaseStatus(status_list)) {
                if (!options.quiet && !status_list.empty()) {
                    std::cout << "\nDatabase Status:" << std::endl;
                    std::cout << std::string(80, '-') << std::endl;
                    
                    for (const auto& status : status_list) {
                        std::cout << "Database: " << status.alias_name << std::endl;
                        std::cout << "  Path: " << status.database_path << std::endl;
                        std::cout << "  State: " << getDatabaseStateString(status.state) << std::endl;
                        std::cout << "  Responding: " << (status.is_responding ? "YES" : "NO") << std::endl;
                        std::cout << "  Consecutive Failures: " << status.consecutive_failures << std::endl;
                        std::cout << "  Total Failures: " << status.total_failures << std::endl;
                        std::cout << "  Total Restarts: " << status.total_restarts << std::endl;
                        
                        if (status.response_time_ms > 0) {
                            std::cout << "  Response Time: " << std::fixed << std::setprecision(2) 
                                     << status.response_time_ms << " ms" << std::endl;
                        }
                        
                        if (!status.last_error.empty()) {
                            std::cout << "  Last Error: " << status.last_error << std::endl;
                        }
                        
                        std::cout << std::endl;
                    }
                }
            }
            
            // Generate report if requested
            if (options.generate_report) {
                bool report_success = guardian.generateStatusReport(options.report_format, options.report_path);
                if (report_success && !options.quiet) {
                    std::cout << "Status report generated: " << options.report_path << std::endl;
                }
            }
        }
        
        return true;
    }
    
    bool executeCheck() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced Guardian - Database Health Check" << std::endl;
            std::cout << "====================================================" << std::endl;
            
            if (!options.database_alias.empty()) {
                std::cout << "Checking database: " << options.database_alias << std::endl;
            } else {
                std::cout << "Checking database: " << options.database_path << std::endl;
            }
            std::cout << std::endl;
        }
        
        GuardEnhanced guardian;
        
        // Perform health check
        SBEnhanced::HealthCheckOptions check_options;
        if (!options.database_alias.empty()) {
            // Would need to look up database path from alias
            check_options.database_path = options.database_path;  // Simplified
        } else {
            check_options.database_path = options.database_path;
        }
        
        check_options.check_connectivity = true;
        check_options.check_responsiveness = true;
        check_options.check_disk_space = true;
        check_options.check_memory_usage = true;
        check_options.check_active_connections = true;
        check_options.check_transaction_health = true;
        check_options.perform_query_test = options.verbose;
        
        SBEnhanced::HealthCheckResult result;
        bool success = guardian.performHealthCheck(check_options, result);
        
        if (!options.quiet) {
            std::cout << "Health Check Results:" << std::endl;
            std::cout << "====================" << std::endl;
            std::cout << "Overall Health: " << (result.health_check_successful ? "HEALTHY" : "UNHEALTHY") << std::endl;
            std::cout << "Database State: " << getDatabaseStateString(result.detected_state) << std::endl;
            std::cout << "Response Time: " << std::fixed << std::setprecision(2) 
                     << result.response_time_ms << " ms" << std::endl;
            
            std::cout << "\nDetailed Results:" << std::endl;
            std::cout << "- Connectivity: " << (result.connectivity_ok ? "OK" : "FAILED") << std::endl;
            std::cout << "- Responsiveness: " << (result.responsiveness_ok ? "OK" : "FAILED") << std::endl;
            std::cout << "- Disk Space: " << (result.disk_space_ok ? "OK" : "LOW") << std::endl;
            std::cout << "- Memory Usage: " << (result.memory_usage_ok ? "OK" : "HIGH") << std::endl;
            std::cout << "- Connections: " << (result.connections_ok ? "OK" : "HIGH") << std::endl;
            std::cout << "- Transactions: " << (result.transactions_ok ? "OK" : "ISSUES") << std::endl;
            
            if (options.verbose) {
                std::cout << "\nMetrics:" << std::endl;
                std::cout << "- Available Disk Space: " << (result.available_disk_space / (1024 * 1024)) << " MB" << std::endl;
                std::cout << "- Memory Usage: " << result.memory_usage_mb << " MB" << std::endl;
                std::cout << "- Active Connections: " << result.active_connections << std::endl;
                std::cout << "- Active Transactions: " << result.active_transactions << std::endl;
            }
            
            if (!result.health_issues.empty()) {
                std::cout << "\nHealth Issues:" << std::endl;
                for (const auto& issue : result.health_issues) {
                    std::cout << "- " << issue << std::endl;
                }
            }
            
            if (!result.recommendations.empty()) {
                std::cout << "\nRecommendations:" << std::endl;
                for (const auto& recommendation : result.recommendations) {
                    std::cout << "- " << recommendation << std::endl;
                }
            }
        }
        
        return success;
    }
    
    bool executeRestart() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced Guardian - Database Restart" << std::endl;
            std::cout << "================================================" << std::endl;
            
            if (!options.database_alias.empty()) {
                std::cout << "Restarting database: " << options.database_alias << std::endl;
            } else {
                std::cout << "Restarting database: " << options.database_path << std::endl;
            }
            std::cout << std::endl;
        }
        
        GuardEnhanced guardian;
        SBEnhanced::GuardianOperationResult result;
        
        bool success;
        if (!options.database_alias.empty()) {
            success = guardian.restartDatabaseEnhanced(options.database_alias, result);
        } else {
            success = guardian.restartDatabase(options.database_path);
        }
        
        if (success) {
            if (!options.quiet) {
                std::cout << "Database restart completed successfully!" << std::endl;
            }
        } else {
            std::cerr << "Database restart failed!" << std::endl;
            for (const auto& error : result.errors) {
                std::cerr << "Error: " << error << std::endl;
            }
        }
        
        return success;
    }
    
    std::string getGuardianModeString(SBEnhanced::GuardianMode mode) {
        switch (mode) {
            case SBEnhanced::GuardianMode::MONITOR_ONLY: return "Monitor Only";
            case SBEnhanced::GuardianMode::AUTO_RESTART: return "Auto Restart";
            case SBEnhanced::GuardianMode::FAILOVER: return "Failover";
            case SBEnhanced::GuardianMode::FULL_AUTO: return "Full Automatic";
            default: return "Unknown";
        }
    }
    
    std::string getMonitoringLevelString(SBEnhanced::MonitoringLevel level) {
        switch (level) {
            case SBEnhanced::MonitoringLevel::BASIC: return "Basic";
            case SBEnhanced::MonitoringLevel::STANDARD: return "Standard";
            case SBEnhanced::MonitoringLevel::COMPREHENSIVE: return "Comprehensive";
            case SBEnhanced::MonitoringLevel::ADVANCED: return "Advanced";
            default: return "Unknown";
        }
    }
    
    std::string getDatabaseStateString(SBEnhanced::DatabaseState state) {
        switch (state) {
            case SBEnhanced::DatabaseState::ONLINE: return "ONLINE";
            case SBEnhanced::DatabaseState::OFFLINE: return "OFFLINE";
            case SBEnhanced::DatabaseState::SHUTTING_DOWN: return "SHUTTING DOWN";
            case SBEnhanced::DatabaseState::STARTING_UP: return "STARTING UP";
            case SBEnhanced::DatabaseState::ERROR: return "ERROR";
            case SBEnhanced::DatabaseState::MAINTENANCE: return "MAINTENANCE";
            case SBEnhanced::DatabaseState::BACKUP_IN_PROGRESS: return "BACKUP IN PROGRESS";
            default: return "UNKNOWN";
        }
    }
    
    void showUsage(const char* program_name) {
        std::cout << "Usage: " << program_name << " [OPTIONS] OPERATION" << std::endl;
        std::cout << "Try '" << program_name << " --help' for more information." << std::endl;
    }
    
    void showVersion() {
        std::cout << "sb_guard version SB-T0.5.0.1 ScratchBird 0.5 f90eae0" << std::endl;
        std::cout << "ScratchBird Enhanced Database Guardian Service" << std::endl;
        std::cout << "Copyright (C) 2025 ScratchBird Project" << std::endl;
    }
    
    void showHelp() {
        std::cout << "ScratchBird Enhanced Guardian - Database Guardian Service" << std::endl;
        std::cout << "=========================================================" << std::endl;
        std::cout << std::endl;
        std::cout << "OPERATIONS:" << std::endl;
        std::cout << "  start               Start the guardian service" << std::endl;
        std::cout << "  stop                Stop the guardian service" << std::endl;
        std::cout << "  status              Show guardian and database status" << std::endl;
        std::cout << "  check               Perform database health check" << std::endl;
        std::cout << "  restart             Restart a specific database" << std::endl;
        std::cout << std::endl;
        std::cout << "CONFIGURATION OPTIONS:" << std::endl;
        std::cout << "  -c, --config FILE   Configuration file path (default: guardian.conf)" << std::endl;
        std::cout << "  -p, --pid-file FILE PID file path (default: guardian.pid)" << std::endl;
        std::cout << "  -l, --log-file FILE Log file path (default: guardian.log)" << std::endl;
        std::cout << "  -daemon, --daemon   Run as daemon process" << std::endl;
        std::cout << std::endl;
        std::cout << "GUARDIAN OPTIONS:" << std::endl;
        std::cout << "  -mode MODE          Guardian mode: monitor, restart, failover, full" << std::endl;
        std::cout << "  -monitoring LEVEL   Monitoring level: basic, standard, comprehensive, advanced" << std::endl;
        std::cout << "  -interval SECONDS   Check interval in seconds (default: 30)" << std::endl;
        std::cout << "  -max-failures N     Maximum failures before action (default: 3)" << std::endl;
        std::cout << "  -no-alerts          Disable alert notifications" << std::endl;
        std::cout << std::endl;
        std::cout << "DATABASE OPTIONS:" << std::endl;
        std::cout << "  -db, --database PATH Database file path (for check/restart operations)" << std::endl;
        std::cout << "  -alias NAME         Database alias name (for check/restart operations)" << std::endl;
        std::cout << std::endl;
        std::cout << "REPORTING OPTIONS:" << std::endl;
        std::cout << "  -report [PATH]      Generate status report" << std::endl;
        std::cout << "  -format FORMAT      Report format: TEXT, JSON, XML, HTML (default: TEXT)" << std::endl;
        std::cout << std::endl;
        std::cout << "GENERAL OPTIONS:" << std::endl;
        std::cout << "  -verbose            Verbose output with detailed information" << std::endl;
        std::cout << "  -quiet              Suppress non-error output" << std::endl;
        std::cout << "  -h, --help          Show this help message" << std::endl;
        std::cout << "  -v, --version       Show version information" << std::endl;
        std::cout << std::endl;
        std::cout << "EXAMPLES:" << std::endl;
        std::cout << "  # Start guardian with custom configuration" << std::endl;
        std::cout << "  sb_guard start -c /etc/guardian.conf -daemon" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Check guardian status with detailed information" << std::endl;
        std::cout << "  sb_guard status -verbose -report status.txt" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Perform health check on specific database" << std::endl;
        std::cout << "  sb_guard check -db /data/mydb.fdb -verbose" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Restart database by alias" << std::endl;
        std::cout << "  sb_guard restart -alias production_db" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Start with comprehensive monitoring and failover" << std::endl;
        std::cout << "  sb_guard start -mode failover -monitoring comprehensive -interval 15" << std::endl;
        std::cout << std::endl;
        std::cout << "ORIGINAL GUARDIAN COMPATIBILITY:" << std::endl;
        std::cout << "  sb_guard guardian.conf" << std::endl;
        std::cout << "  sb_guard -start guardian.conf" << std::endl;
        std::cout << "  sb_guard -stop" << std::endl;
        std::cout << std::endl;
        std::cout << "CONFIGURATION FILE FORMAT:" << std::endl;
        std::cout << "  The configuration file uses INI format with database sections:" << std::endl;
        std::cout << "  [database:mydb]" << std::endl;
        std::cout << "  database_path=/data/mydb.fdb" << std::endl;
        std::cout << "  server_host=localhost" << std::endl;
        std::cout << "  username=SYSDBA" << std::endl;
        std::cout << "  password=masterkey" << std::endl;
        std::cout << "  check_interval=30" << std::endl;
        std::cout << "  auto_restart=true" << std::endl;
        std::cout << std::endl;
        std::cout << "For more information, see the ScratchBird documentation." << std::endl;
    }
};

// Progress display helper for daemon mode
class GuardianStatusDisplay {
private:
    bool enabled;
    std::chrono::steady_clock::time_point last_update;
    
public:
    GuardianStatusDisplay(bool enable) : enabled(enable) {
        last_update = std::chrono::steady_clock::now();
    }
    
    void update(const SBEnhanced::GuardianProgress& progress) {
        if (!enabled) return;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update);
        
        // Update every 10 seconds to avoid too frequent updates
        if (elapsed.count() < 10) return;
        
        last_update = now;
        
        std::cout << "[" << progress.getUptime().count() << "s] "
                 << "DBs: " << progress.databases_monitored << ", "
                 << "Checks: " << progress.health_checks_performed << ", "
                 << "Restarts: " << progress.restarts_performed << ", "
                 << "Alerts: " << progress.alerts_generated;
        
        if (!progress.current_database.empty()) {
            std::cout << " (Current: " << progress.current_database << ")";
        }
        
        std::cout << std::endl;
    }
};

// Main function
int main(int argc, char* argv[]) {
    try {
        GuardCommandParser parser;
        
        if (!parser.parseArguments(argc, argv)) {
            return 1;
        }
        
        bool success = parser.executeCommand();
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: Unknown exception" << std::endl;
        return 1;
    }
}