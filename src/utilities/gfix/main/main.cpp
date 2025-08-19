#include "sb_gfix_enhanced.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <cstring>
#include <iomanip>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace SBEnhanced;

// Enhanced command line argument parsing for full GFIX compatibility
struct CommandLineArgs {
    std::string database_path;
    std::string operation = "validate";  // validate, repair, sweep, info
    ValidationOptions validation_opts;
    RepairOptions repair_opts;
    SweepOptions sweep_opts;
    ExtendedValidationOptions extended_validation_opts;
    DatabaseConfigOptions config_opts;
    ShutdownOptions shutdown_opts;
    TransactionOptions transaction_opts;
    ShadowOptions shadow_opts;
    ConnectionOptions connection_opts;
    
    // Operation flags (original GFIX compatibility)
    bool perform_validation = false;
    bool perform_repair = false;
    bool perform_sweep = false;
    bool perform_shutdown = false;
    bool bring_database_online = false;
    bool list_limbo_transactions = false;
    bool commit_limbo_transactions = false;
    bool rollback_limbo_transactions = false;
    bool two_phase_recovery = false;
    bool activate_shadow = false;
    bool kill_shadows = false;
    bool mend_database = false;
    bool configure_database = false;
    
    // Extended validation flags
    bool full_validation = false;
    bool read_only_validation = false;
    bool ignore_checksums = false;
    
    // Configuration flags
    bool set_page_buffers = false;
    bool set_sweep_interval = false;
    bool set_access_mode = false;
    bool set_write_mode = false;
    bool set_space_usage = false;
    bool set_sql_dialect = false;
    bool fix_icu = false;
    bool upgrade_ods = false;
    bool disable_linger = false;
    bool set_parallel_workers = false;
    bool set_replica_mode = false;
    
    // Authentication flags
    bool use_password_file = false;
    bool use_trusted_auth = false;
    
    // Specific transaction ID for commit/rollback
    uint64_t specific_transaction_id = 0;
    
    // General flags
    bool show_help = false;
    bool show_version = false;
    bool verbose = false;
    bool quiet = false;
};

void showVersion() {
    std::cout << "sb_gfix version SB-T0.6.0.1 ScratchBird 0.6 f90eae0" << std::endl;
    std::cout << "Enhanced database maintenance utility for ScratchBird" << std::endl;
    std::cout << "Copyright (c) 2025 ScratchBird Project" << std::endl;
}

void showHelp() {
    std::cout << "ScratchBird Enhanced GFIX - Database Maintenance Utility" << std::endl;
    std::cout << "Enhanced version with 100% original GFIX compatibility" << std::endl << std::endl;
    std::cout << "Usage: sb_gfix [options] database_path" << std::endl << std::endl;
    
    std::cout << "Validation Options:" << std::endl;
    std::cout << "  -validate, -v         Validate database structure" << std::endl;
    std::cout << "  -full                 Validate record fragments (full validation)" << std::endl;
    std::cout << "  -no_update           Read-only validation (don't update anything)" << std::endl;
    std::cout << "  -ignore              Ignore checksum errors during validation" << std::endl;
    std::cout << "  -mend                Prepare corrupt database for backup" << std::endl << std::endl;
    
    std::cout << "Database State Management:" << std::endl;
    std::cout << "  -shut [mode] [timeout]  Shutdown database" << std::endl;
    std::cout << "    Modes: normal, multi, single, full, force, attachment, transaction" << std::endl;
    std::cout << "  -online              Bring database online" << std::endl;
    std::cout << "  -force               Force shutdown immediately" << std::endl << std::endl;
    
    std::cout << "Access and Write Modes:" << std::endl;
    std::cout << "  -mode read_write     Set database to read-write mode" << std::endl;
    std::cout << "  -mode read_only      Set database to read-only mode" << std::endl;
    std::cout << "  -write sync          Set synchronous write mode" << std::endl;
    std::cout << "  -write async         Set asynchronous write mode" << std::endl << std::endl;
    
    std::cout << "Database Configuration:" << std::endl;
    std::cout << "  -buffers <count>     Set page buffer count" << std::endl;
    std::cout << "  -sweep [interval]    Perform sweep or set sweep interval" << std::endl;
    std::cout << "  -housekeeping <pages> Set sweep interval (classic compatibility)" << std::endl;
    std::cout << "  -sql_dialect <n>     Set SQL dialect (1, 2, or 3)" << std::endl;
    std::cout << "  -nolinger            Disable database linger" << std::endl << std::endl;
    
    std::cout << "Transaction Management:" << std::endl;
    std::cout << "  -list                List limbo transactions" << std::endl;
    std::cout << "  -commit [tid]        Commit limbo transaction(s)" << std::endl;
    std::cout << "  -rollback [tid]      Rollback limbo transaction(s)" << std::endl;
    std::cout << "  -two_phase           Automatic two-phase recovery" << std::endl << std::endl;
    
    std::cout << "Shadow File Management:" << std::endl;
    std::cout << "  -activate            Activate shadow file for database usage" << std::endl;
    std::cout << "  -kill                Kill all unavailable shadow files" << std::endl << std::endl;
    
    std::cout << "Space and Performance:" << std::endl;
    std::cout << "  -use reserve         Use reserve space for record versions" << std::endl;
    std::cout << "  -use full            Use full space for record versions" << std::endl;
    std::cout << "  -parallel <count>    Set parallel worker count" << std::endl << std::endl;
    
    std::cout << "ICU and Upgrade:" << std::endl;
    std::cout << "  -icu                 Fix ICU version for database compatibility" << std::endl;
    std::cout << "  -upgrade             Upgrade database On-Disk Structure (ODS)" << std::endl << std::endl;
    
    std::cout << "Authentication:" << std::endl;
    std::cout << "  -user <username>     Database username" << std::endl;
    std::cout << "  -password <password> Database password" << std::endl;
    std::cout << "  -role <role>         Database role" << std::endl;
    std::cout << "  -fetch_password <file> Load password from file" << std::endl;
    std::cout << "  -trusted             Use trusted authentication" << std::endl << std::endl;
    
    std::cout << "Replica Management:" << std::endl;
    std::cout << "  -replica none        Disable replica mode" << std::endl;
    std::cout << "  -replica read_only   Set read-only replica mode" << std::endl;
    std::cout << "  -replica read_write  Set read-write replica mode" << std::endl << std::endl;
    
    std::cout << "Output Options:" << std::endl;
    std::cout << "  -verbose, -v         Verbose output" << std::endl;
    std::cout << "  -quiet, -q           Quiet mode" << std::endl;
    std::cout << "  -help, -h            Show this help" << std::endl;
    std::cout << "  -version, -z         Show version information" << std::endl << std::endl;
    
    std::cout << "Enhanced Options (beyond original GFIX):" << std::endl;
    std::cout << "  --validate-severity <level>    Validation severity (basic|normal|full|deep|forensic)" << std::endl;
    std::cout << "  --repair-strategy <strategy>   Repair strategy (conservative|aggressive|salvage)" << std::endl;
    std::cout << "  --backup-before-repair         Create backup before repair operations" << std::endl;
    std::cout << "  --backup-path <path>           Backup file path" << std::endl;
    std::cout << "  --max-errors <count>           Maximum errors to report (default: 1000)" << std::endl;
    std::cout << "  --output-report <path>         Write detailed report to file" << std::endl;
    std::cout << "  --continue-on-errors           Continue validation on errors" << std::endl;
    std::cout << "  --check-fragments              Check record fragments (default: true)" << std::endl;
    std::cout << "  --check-blobs                  Check blob integrity (default: true)" << std::endl;
    std::cout << "  --check-indexes                Check index consistency (default: true)" << std::endl;
    std::cout << "  --check-referential            Check referential integrity (expensive)" << std::endl;
    std::cout << "  --rebuild-indexes              Rebuild corrupt indexes during repair" << std::endl;
    std::cout << "  --resolve-limbo                Resolve limbo transactions during repair" << std::endl;
    std::cout << "  --reclaim-space                Reclaim unused space during repair" << std::endl;
    std::cout << "  --force-sweep                  Force sweep operation" << std::endl;
    std::cout << "  --cooperative-sweep            Allow other connections during sweep" << std::endl;
    std::cout << "  --max-sweep-time <minutes>     Maximum sweep duration in minutes" << std::endl << std::endl;
    
    std::cout << "Original GFIX Compatibility Examples:" << std::endl;
    std::cout << "  sb_gfix -validate employee.fdb" << std::endl;
    std::cout << "  sb_gfix -full -mend corrupted.fdb" << std::endl;
    std::cout << "  sb_gfix -shut single employee.fdb" << std::endl;
    std::cout << "  sb_gfix -sweep 20000 employee.fdb" << std::endl;
    std::cout << "  sb_gfix -list employee.fdb" << std::endl;
    std::cout << "  sb_gfix -commit 12345 employee.fdb" << std::endl;
    std::cout << "  sb_gfix -buffers 1000 employee.fdb" << std::endl;
    std::cout << "  sb_gfix -mode read_only employee.fdb" << std::endl << std::endl;
    
    std::cout << "Enhanced Features (beyond original GFIX):" << std::endl;
    std::cout << "  - Real-time progress monitoring" << std::endl;
    std::cout << "  - Advanced repair strategies" << std::endl;
    std::cout << "  - Comprehensive validation reports" << std::endl;
    std::cout << "  - Pre-repair backup integration" << std::endl;
    std::cout << "  - Performance recommendations" << std::endl;
    std::cout << "  - Detailed operation statistics" << std::endl << std::endl;
}

ValidationSeverity parseValidationSeverity(const std::string& severity) {
    if (severity == "basic") return ValidationSeverity::BASIC;
    if (severity == "normal") return ValidationSeverity::NORMAL;
    if (severity == "full") return ValidationSeverity::FULL;
    if (severity == "deep") return ValidationSeverity::DEEP;
    if (severity == "forensic") return ValidationSeverity::FORENSIC;
    return ValidationSeverity::NORMAL;
}

RepairStrategy parseRepairStrategy(const std::string& strategy) {
    if (strategy == "conservative") return RepairStrategy::CONSERVATIVE;
    if (strategy == "aggressive") return RepairStrategy::AGGRESSIVE;
    if (strategy == "salvage") return RepairStrategy::SALVAGE;
    if (strategy == "validate_only") return RepairStrategy::VALIDATE_ONLY;
    return RepairStrategy::CONSERVATIVE;
}

bool parseCommandLine(int argc, char* argv[], CommandLineArgs& args) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Help and version
        if (arg == "-?" || arg == "--help" || arg == "-help" || arg == "-h") {
            args.show_help = true;
            return true;
        } else if (arg == "-z" || arg == "--version" || arg == "-version") {
            args.show_version = true;
            return true;
        }
        
        // Validation operations
        else if (arg == "-v" || arg == "-validate") {
            args.perform_validation = true;
        } else if (arg == "-full") {
            args.perform_validation = true;
            args.full_validation = true;
            args.extended_validation_opts.full_validation = true;
        } else if (arg == "-no_update") {
            args.perform_validation = true;
            args.read_only_validation = true;
            args.extended_validation_opts.read_only_validation = true;
        } else if (arg == "-ignore") {
            args.perform_validation = true;
            args.ignore_checksums = true;
            args.extended_validation_opts.ignore_checksums = true;
        } else if (arg == "-mend") {
            args.mend_database = true;
            args.extended_validation_opts.mend_database = true;
        }
        
        // Database state management
        else if (arg == "-shut") {
            args.perform_shutdown = true;
            args.shutdown_opts.mode = ShutdownMode::NORMAL;
            // Check for shutdown mode
            if (i + 1 < argc) {
                std::string mode = argv[i + 1];
                if (mode == "normal") {
                    ++i;
                    args.shutdown_opts.mode = ShutdownMode::NORMAL;
                } else if (mode == "multi") {
                    ++i;
                    args.shutdown_opts.mode = ShutdownMode::MULTI;
                } else if (mode == "single") {
                    ++i;
                    args.shutdown_opts.mode = ShutdownMode::SINGLE;
                } else if (mode == "full") {
                    ++i;
                    args.shutdown_opts.mode = ShutdownMode::FULL;
                } else if (mode == "force") {
                    ++i;
                    args.shutdown_opts.mode = ShutdownMode::FORCE;
                } else if (mode == "attachment") {
                    ++i;
                    args.shutdown_opts.mode = ShutdownMode::ATTACHMENT;
                } else if (mode == "transaction") {
                    ++i;
                    args.shutdown_opts.mode = ShutdownMode::TRANSACTION;
                }
                // Check for timeout
                if (i + 1 < argc) {
                    std::string timeout_str = argv[i + 1];
                    if (timeout_str.find_first_not_of("0123456789") == std::string::npos) {
                        ++i;
                        args.shutdown_opts.timeout_seconds = std::stoul(timeout_str);
                    }
                }
            }
        } else if (arg == "-online") {
            args.bring_database_online = true;
        } else if (arg == "-force") {
            args.shutdown_opts.force_shutdown = true;
        }
        
        // Access and write modes
        else if (arg == "-mode" && i + 1 < argc) {
            std::string mode = argv[++i];
            args.set_access_mode = true;
            args.configure_database = true;
            if (mode == "read_write") {
                args.config_opts.set_access_mode = true;
                args.config_opts.access_mode = DatabaseAccessMode::READ_WRITE;
            } else if (mode == "read_only") {
                args.config_opts.set_access_mode = true;
                args.config_opts.access_mode = DatabaseAccessMode::READ_ONLY;
            } else {
                std::cerr << "Invalid access mode: " << mode << std::endl;
                return false;
            }
        } else if (arg == "-write" && i + 1 < argc) {
            std::string mode = argv[++i];
            args.set_write_mode = true;
            args.configure_database = true;
            args.config_opts.set_write_mode = true;
            if (mode == "sync") {
                args.config_opts.write_mode = DatabaseWriteMode::SYNC;
            } else if (mode == "async") {
                args.config_opts.write_mode = DatabaseWriteMode::ASYNC;
            } else {
                std::cerr << "Invalid write mode: " << mode << std::endl;
                return false;
            }
        }
        
        // Database configuration
        else if (arg == "-buffers" && i + 1 < argc) {
            std::string buffers_str = argv[++i];
            args.set_page_buffers = true;
            args.configure_database = true;
            args.config_opts.set_page_buffers = true;
            args.config_opts.page_buffers = std::stoul(buffers_str);
        } else if (arg == "-sweep") {
            if (i + 1 < argc) {
                std::string next_arg = argv[i + 1];
                if (next_arg.find_first_not_of("0123456789") == std::string::npos) {
                    ++i;
                    args.set_sweep_interval = true;
                    args.configure_database = true;
                    args.config_opts.set_sweep_interval = true;
                    args.config_opts.sweep_interval = std::stoul(next_arg);
                } else {
                    args.perform_sweep = true;
                }
            } else {
                args.perform_sweep = true;
            }
        } else if (arg == "-housekeeping" && i + 1 < argc) {
            std::string interval_str = argv[++i];
            args.set_sweep_interval = true;
            args.configure_database = true;
            args.config_opts.set_sweep_interval = true;
            args.config_opts.sweep_interval = std::stoul(interval_str);
        } else if (arg == "-sql_dialect" && i + 1 < argc) {
            std::string dialect_str = argv[++i];
            args.set_sql_dialect = true;
            args.configure_database = true;
            args.config_opts.set_sql_dialect = true;
            args.config_opts.sql_dialect = std::stoul(dialect_str);
        } else if (arg == "-nolinger") {
            args.disable_linger = true;
            args.configure_database = true;
            args.config_opts.disable_linger = true;
        }
        
        // Transaction management
        else if (arg == "-list") {
            args.list_limbo_transactions = true;
            args.transaction_opts.list_only = true;
        } else if (arg == "-commit") {
            args.commit_limbo_transactions = true;
            args.transaction_opts.resolution = TransactionResolution::COMMIT;
            if (i + 1 < argc) {
                std::string tid_str = argv[i + 1];
                if (tid_str.find_first_not_of("0123456789") == std::string::npos) {
                    ++i;
                    args.specific_transaction_id = std::stoull(tid_str);
                    args.transaction_opts.specific_transaction_id = args.specific_transaction_id;
                }
            }
        } else if (arg == "-rollback") {
            args.rollback_limbo_transactions = true;
            args.transaction_opts.resolution = TransactionResolution::ROLLBACK;
            if (i + 1 < argc) {
                std::string tid_str = argv[i + 1];
                if (tid_str.find_first_not_of("0123456789") == std::string::npos) {
                    ++i;
                    args.specific_transaction_id = std::stoull(tid_str);
                    args.transaction_opts.specific_transaction_id = args.specific_transaction_id;
                }
            }
        } else if (arg == "-two_phase") {
            args.two_phase_recovery = true;
            args.transaction_opts.resolution = TransactionResolution::AUTO_TWO_PHASE;
        }
        
        // Shadow file management
        else if (arg == "-activate") {
            args.activate_shadow = true;
            args.shadow_opts.operation = ShadowOperation::ACTIVATE;
        } else if (arg == "-kill") {
            args.kill_shadows = true;
            args.shadow_opts.operation = ShadowOperation::KILL;
        }
        
        // Space usage
        else if (arg == "-use" && i + 1 < argc) {
            std::string usage = argv[++i];
            args.set_space_usage = true;
            args.configure_database = true;
            args.config_opts.set_space_usage = true;
            if (usage == "reserve") {
                args.config_opts.space_usage = SpaceUsageMode::RESERVE;
            } else if (usage == "full") {
                args.config_opts.space_usage = SpaceUsageMode::FULL;
            } else {
                std::cerr << "Invalid space usage mode: " << usage << std::endl;
                return false;
            }
        }
        
        // ICU and upgrade
        else if (arg == "-icu") {
            args.fix_icu = true;
            args.configure_database = true;
            args.config_opts.fix_icu = true;
        } else if (arg == "-upgrade") {
            args.upgrade_ods = true;
            args.configure_database = true;
            args.config_opts.upgrade_ods = true;
        }
        
        // Authentication
        else if (arg == "-user" && i + 1 < argc) {
            args.connection_opts.username = argv[++i];
        } else if (arg == "-password" && i + 1 < argc) {
            args.connection_opts.password = argv[++i];
        } else if (arg == "-role" && i + 1 < argc) {
            args.connection_opts.role = argv[++i];
        } else if (arg == "-fetch_password" && i + 1 < argc) {
            args.connection_opts.password_file = argv[++i];
            args.use_password_file = true;
        } else if (arg == "-trusted") {
            args.use_trusted_auth = true;
            args.connection_opts.use_trusted_auth = true;
        }
        
        // Parallel processing
        else if (arg == "-parallel" && i + 1 < argc) {
            std::string workers_str = argv[++i];
            args.set_parallel_workers = true;
            args.configure_database = true;
            args.config_opts.set_parallel_workers = true;
            args.config_opts.parallel_workers = std::stoul(workers_str);
        }
        
        // Replica mode
        else if (arg == "-replica" && i + 1 < argc) {
            std::string mode = argv[++i];
            args.set_replica_mode = true;
            args.configure_database = true;
            args.config_opts.set_replica_mode = true;
            if (mode == "none") {
                args.config_opts.replica_mode = ReplicaMode::NONE;
            } else if (mode == "read_only") {
                args.config_opts.replica_mode = ReplicaMode::READ_ONLY;
            } else if (mode == "read_write") {
                args.config_opts.replica_mode = ReplicaMode::READ_WRITE;
            } else {
                std::cerr << "Invalid replica mode: " << mode << std::endl;
                return false;
            }
        }
        
        // Enhanced options (beyond original GFIX)
        else if (arg == "--validate-severity" && i + 1 < argc) {
            args.validation_opts.severity = parseValidationSeverity(argv[++i]);
        } else if (arg == "--repair-strategy" && i + 1 < argc) {
            args.repair_opts.strategy = parseRepairStrategy(argv[++i]);
        } else if (arg == "--backup-before-repair") {
            args.repair_opts.create_backup_before_repair = true;
        } else if (arg == "--backup-path" && i + 1 < argc) {
            args.repair_opts.backup_path = argv[++i];
        } else if (arg == "--max-errors" && i + 1 < argc) {
            args.validation_opts.max_errors_to_report = std::atoi(argv[++i]);
        } else if (arg == "--output-report" && i + 1 < argc) {
            args.validation_opts.output_file_path = argv[++i];
        } else if (arg == "--continue-on-errors") {
            args.validation_opts.continue_on_errors = true;
        } else if (arg == "--check-fragments") {
            args.validation_opts.check_record_fragments = true;
        } else if (arg == "--check-blobs") {
            args.validation_opts.check_blob_integrity = true;
        } else if (arg == "--check-indexes") {
            args.validation_opts.check_index_consistency = true;
        } else if (arg == "--check-referential") {
            args.validation_opts.check_referential_integrity = true;
        } else if (arg == "--rebuild-indexes") {
            args.repair_opts.rebuild_corrupt_indexes = true;
        } else if (arg == "--resolve-limbo") {
            args.repair_opts.resolve_limbo_transactions = true;
        } else if (arg == "--reclaim-space") {
            args.repair_opts.reclaim_unused_space = true;
        } else if (arg == "--force-sweep") {
            args.sweep_opts.force_sweep = true;
        } else if (arg == "--cooperative-sweep") {
            args.sweep_opts.cooperative_sweep = true;
        } else if (arg == "--max-sweep-time" && i + 1 < argc) {
            args.sweep_opts.max_sweep_duration_minutes = std::atoi(argv[++i]);
        }
        
        // Output options
        else if (arg == "-verbose" || arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "-quiet" || arg == "--quiet" || arg == "-q") {
            args.quiet = true;
        }
        
        // Database path (no leading dash)
        else if (!arg.empty() && arg[0] != '-') {
            if (args.database_path.empty()) {
                args.database_path = arg;
            } else {
                std::cerr << "Multiple database paths specified" << std::endl;
                return false;
            }
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            return false;
        }
    }
    
    // If no explicit operation specified but database path provided, default to validation
    if (!args.perform_validation && !args.perform_repair && !args.perform_sweep &&
        !args.perform_shutdown && !args.bring_database_online && 
        !args.list_limbo_transactions && !args.commit_limbo_transactions &&
        !args.rollback_limbo_transactions && !args.two_phase_recovery &&
        !args.activate_shadow && !args.kill_shadows && !args.mend_database &&
        !args.configure_database && !args.database_path.empty()) {
        args.perform_validation = true;
    }
    
    return !args.database_path.empty() || args.show_help || args.show_version;
}


// Progress callback for operation monitoring
void progressCallback(const MaintenanceProgress& progress) {
    static auto last_update = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // Update every second to avoid flooding output
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count() >= 1) {
        last_update = now;
        
        double percentage = progress.getProgressPercentage();
        auto elapsed = progress.getElapsedTime();
        auto eta = progress.getEstimatedTimeRemaining();
        
        std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << percentage << "% "
                  << "(" << progress.processed_pages << "/" << progress.total_pages << " pages) "
                  << "Elapsed: " << elapsed.count() << "s "
                  << "ETA: " << eta.count() << "s";
        
        if (!progress.current_object.empty()) {
            std::cout << " [" << progress.current_object << "]";
        }
        
        std::cout << std::flush;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            showHelp();
            return 1;
        }
        
        CommandLineArgs args;
        
        if (!parseCommandLine(argc, argv, args)) {
            std::cerr << "Error parsing command line arguments." << std::endl;
            return 1;
        }
        
        if (args.show_help) {
            showHelp();
            return 0;
        }
        
        if (args.show_version) {
            showVersion();
            return 0;
        }
        
        if (args.database_path.empty()) {
            std::cerr << "Database path is required." << std::endl;
            showHelp();
            return 1;
        }
        
        // Verify database file exists (unless creating new operations)
        if (!fs::exists(args.database_path) && !args.upgrade_ods) {
            std::cerr << "Database file does not exist: " << args.database_path << std::endl;
            return 1;
        }
        
        // Initialize enhanced GFIX utility
        GFixEnhanced gfix;
        bool operation_successful = true;
        
        // Load password from file if specified
        if (args.use_password_file && !args.connection_opts.password_file.empty()) {
            std::string password;
            if (!gfix.loadPasswordFromFile(args.connection_opts.password_file, password)) {
                std::cerr << "Failed to load password from file: " << args.connection_opts.password_file << std::endl;
                return 1;
            }
            args.connection_opts.password = password;
        }
        
        // Authenticate connection if credentials provided
        if (!args.connection_opts.username.empty() || args.use_trusted_auth) {
            if (!gfix.authenticateConnection(args.database_path, args.connection_opts)) {
                std::cerr << "Authentication failed." << std::endl;
                return 1;
            }
        }
        
        if (!args.quiet) {
            std::cout << "ScratchBird Enhanced GFIX - Operating on: " << args.database_path << std::endl;
        }
        
        // Execute operations based on parsed options
        
        // Database state management operations
        if (args.perform_shutdown) {
            args.shutdown_opts.progress_callback = args.verbose ? progressCallback : nullptr;
            
            GFixOperationResult result;
            operation_successful = gfix.shutdownDatabase(args.database_path, args.shutdown_opts, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Database shutdown completed successfully." << std::endl;
                } else {
                    std::cerr << "Database shutdown failed." << std::endl;
                }
            }
        }
        
        if (args.bring_database_online) {
            GFixOperationResult result;
            operation_successful = gfix.bringDatabaseOnline(args.database_path, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Database brought online successfully." << std::endl;
                } else {
                    std::cerr << "Failed to bring database online." << std::endl;
                }
            }
        }
        
        // Database configuration operations
        if (args.configure_database) {
            args.config_opts.progress_callback = args.verbose ? progressCallback : nullptr;
            
            GFixOperationResult result;
            operation_successful = gfix.configureDatabaseSettings(args.database_path, args.config_opts, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Database configuration updated successfully." << std::endl;
                } else {
                    std::cerr << "Database configuration update failed." << std::endl;
                }
            }
        }
        
        // Transaction management operations
        if (args.list_limbo_transactions || args.commit_limbo_transactions || 
            args.rollback_limbo_transactions || args.two_phase_recovery) {
            
            if (args.list_limbo_transactions) {
                std::vector<uint64_t> transaction_ids;
                GFixOperationResult result;
                operation_successful = gfix.listLimboTransactions(args.database_path, transaction_ids, result);
                
                if (operation_successful) {
                    if (transaction_ids.empty()) {
                        if (!args.quiet) {
                            std::cout << "No limbo transactions found." << std::endl;
                        }
                    } else {
                        std::cout << "Limbo transactions found:" << std::endl;
                        for (uint64_t tid : transaction_ids) {
                            std::cout << "  Transaction ID: " << tid << std::endl;
                        }
                    }
                } else {
                    std::cerr << "Failed to list limbo transactions." << std::endl;
                }
            } else {
                args.transaction_opts.progress_callback = args.verbose ? progressCallback : nullptr;
                
                GFixOperationResult result;
                if (args.commit_limbo_transactions && args.specific_transaction_id > 0) {
                    operation_successful = gfix.commitLimboTransaction(args.database_path, args.specific_transaction_id, result);
                } else if (args.rollback_limbo_transactions && args.specific_transaction_id > 0) {
                    operation_successful = gfix.rollbackLimboTransaction(args.database_path, args.specific_transaction_id, result);
                } else {
                    operation_successful = gfix.performTwoPhaseRecovery(args.database_path, args.transaction_opts, result);
                }
                
                if (!args.quiet) {
                    if (operation_successful) {
                        std::cout << "Transaction management completed successfully." << std::endl;
                    } else {
                        std::cerr << "Transaction management failed." << std::endl;
                    }
                }
            }
        }
        
        // Shadow file operations
        if (args.activate_shadow || args.kill_shadows) {
            args.shadow_opts.progress_callback = args.verbose ? progressCallback : nullptr;
            
            GFixOperationResult result;
            operation_successful = gfix.manageShadowFiles(args.database_path, args.shadow_opts, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Shadow file operation completed successfully." << std::endl;
                } else {
                    std::cerr << "Shadow file operation failed." << std::endl;
                }
            }
        }
        
        // Validation operations
        if (args.perform_validation || args.full_validation || 
            args.read_only_validation || args.ignore_checksums) {
            
            args.extended_validation_opts.progress_callback = args.verbose ? progressCallback : nullptr;
            
            ValidationResult result;
            operation_successful = gfix.performExtendedValidation(args.database_path, args.extended_validation_opts, result);
            
            if (args.verbose || !args.quiet) {
                std::cout << std::endl; // New line after progress
                std::cout << gfix.generateValidationReport(result) << std::endl;
            }
            
            if (!args.quiet) {
                if (result.isDatabaseHealthy()) {
                    std::cout << "Database validation completed - no issues found." << std::endl;
                } else if (result.requiresImmediateAttention()) {
                    std::cout << "Database validation completed - critical issues found!" << std::endl;
                } else {
                    std::cout << "Database validation completed - minor issues found." << std::endl;
                }
            }
        }
        
        // Mend operation
        if (args.mend_database) {
            GFixOperationResult result;
            operation_successful = gfix.mendCorruptDatabase(args.database_path, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Database mend operation completed successfully." << std::endl;
                    std::cout << "Database is now prepared for backup." << std::endl;
                } else {
                    std::cerr << "Database mend operation failed." << std::endl;
                }
            }
        }
        
        // Sweep operation
        if (args.perform_sweep) {
            args.sweep_opts.progress_callback = args.verbose ? progressCallback : nullptr;
            
            MaintenanceStatistics stats;
            operation_successful = gfix.performDatabaseSweep(args.database_path, args.sweep_opts, stats);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << std::endl; // New line after progress
                    std::cout << "Database sweep completed successfully." << std::endl;
                    if (args.verbose) {
                        std::cout << gfix.generateMaintenanceReport(stats) << std::endl;
                    }
                } else {
                    std::cerr << "Database sweep failed." << std::endl;
                }
            }
        }
        
        // Show errors if any occurred
        if (!operation_successful) {
            auto errors = gfix.getErrors();
            if (!errors.empty() && !args.quiet) {
                std::cerr << "\nErrors encountered:" << std::endl;
                for (const auto& error : errors) {
                    std::cerr << "  " << error << std::endl;
                }
            }
        }
        
        // Show warnings if verbose mode
        if (args.verbose) {
            auto warnings = gfix.getWarnings();
            if (!warnings.empty()) {
                std::cout << "\nWarnings:" << std::endl;
                for (const auto& warning : warnings) {
                    std::cout << "  " << warning << std::endl;
                }
            }
        }
        
        return operation_successful ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred." << std::endl;
        return 2;
    }
}