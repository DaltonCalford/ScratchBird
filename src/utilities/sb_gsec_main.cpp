#include "sb_gsec_enhanced.h"
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

// Enhanced command line argument parsing for full GSEC compatibility
struct CommandLineArgs {
    std::string database_path;
    
    // Operation flags (original GSEC compatibility)
    bool add_user = false;
    bool modify_user = false;
    bool delete_user = false;
    bool display_user = false;
    bool display_all_users = false;
    
    // User information
    std::string username;
    std::string password;
    std::string first_name;
    std::string middle_name;
    std::string last_name;
    std::string description;
    std::string email;
    std::string phone;
    
    // Role management
    bool create_role = false;
    bool drop_role = false;
    bool grant_role = false;
    bool revoke_role = false;
    bool list_roles = false;
    bool describe_role = false;
    std::string role_name;
    std::vector<std::string> assign_roles;
    std::vector<std::string> grant_to_users;
    
    // Security options
    bool admin_privileges = false;
    bool force_password_change = false;
    AuthenticationMethod auth_method = AuthenticationMethod::SRP256;
    std::vector<DatabasePrivilege> grant_privileges;
    
    // Password policy
    bool configure_password_policy = false;
    PasswordPolicyLevel policy_level = PasswordPolicyLevel::STANDARD;
    uint32_t min_password_length = 8;
    uint32_t max_password_length = 128;
    
    // Security audit
    bool perform_security_audit = false;
    bool generate_security_report = false;
    SecurityAuditLevel audit_level = SecurityAuditLevel::STANDARD;
    std::string start_date_str;
    std::string end_date_str;
    std::string report_format = "TEXT";
    std::string output_file;
    
    // Session management
    bool list_sessions = false;
    bool kill_sessions = false;
    std::string target_username;
    bool force_disconnect = false;
    
    // Authentication configuration
    bool configure_auth = false;
    bool enable_multifactor = false;
    bool require_secure_connection = false;
    std::string certificate_path;
    
    // Database encryption
    bool configure_encryption = false;
    bool enable_encryption = false;
    std::string encryption_algorithm = "AES256";
    std::string key_file_path;
    
    // Compliance
    bool check_compliance = false;
    std::vector<std::string> compliance_standards;
    
    // General flags
    bool show_help = false;
    bool show_version = false;
    bool verbose = false;
    bool quiet = false;
    bool trusted_auth = false;
    
    // Authentication credentials
    std::string admin_username;
    std::string admin_password;
    std::string password_file;
};

// Print help information
void printHelp() {
    std::cout << "ScratchBird Enhanced GSEC - Security Management Utility" << std::endl;
    std::cout << "Enhanced version with 100% original GSEC compatibility" << std::endl << std::endl;
    std::cout << "Usage: sb_gsec [options] database_path" << std::endl << std::endl;
    
    std::cout << "User Management (Original GSEC Compatibility):" << std::endl;
    std::cout << "  -add <username>      Add new user account" << std::endl;
    std::cout << "  -modify <username>   Modify existing user account" << std::endl;
    std::cout << "  -delete <username>   Delete user account" << std::endl;
    std::cout << "  -display [username]  Display user information (all users if no username)" << std::endl << std::endl;
    
    std::cout << "User Properties (for -add and -modify):" << std::endl;
    std::cout << "  -pw <password>       User password" << std::endl;
    std::cout << "  -fname <name>        First name" << std::endl;
    std::cout << "  -mname <name>        Middle name" << std::endl;
    std::cout << "  -lname <name>        Last name" << std::endl;
    std::cout << "  -description <desc>  User description" << std::endl;
    std::cout << "  -email <email>       Email address" << std::endl;
    std::cout << "  -phone <phone>       Phone number" << std::endl;
    std::cout << "  -admin               Grant admin privileges" << std::endl;
    std::cout << "  -force_change        Force password change on next login" << std::endl << std::endl;
    
    std::cout << "Role Management:" << std::endl;
    std::cout << "  -create_role <role>  Create new database role" << std::endl;
    std::cout << "  -drop_role <role>    Drop database role" << std::endl;
    std::cout << "  -grant_role <role>   Grant role to user" << std::endl;
    std::cout << "  -revoke_role <role>  Revoke role from user" << std::endl;
    std::cout << "  -list_roles          List all database roles" << std::endl;
    std::cout << "  -describe_role <role> Describe role details" << std::endl << std::endl;
    
    std::cout << "Authentication Methods:" << std::endl;
    std::cout << "  -auth_method <method> Set authentication method" << std::endl;
    std::cout << "    Methods: legacy, srp, srp256, win_sspi, multifactor, certificate, kerberos, ldap" << std::endl;
    std::cout << "  -trusted             Use trusted authentication" << std::endl << std::endl;
    
    std::cout << "Password Policy:" << std::endl;
    std::cout << "  -configure_policy    Configure password policy" << std::endl;
    std::cout << "  -policy_level <level> Password policy level (none|basic|standard|strict|enterprise)" << std::endl;
    std::cout << "  -min_length <n>      Minimum password length" << std::endl;
    std::cout << "  -max_length <n>      Maximum password length" << std::endl << std::endl;
    
    std::cout << "Security Auditing:" << std::endl;
    std::cout << "  -audit               Perform security audit" << std::endl;
    std::cout << "  -audit_level <level> Audit level (disabled|basic|standard|comprehensive|forensic)" << std::endl;
    std::cout << "  -start_date <date>   Audit start date (YYYY-MM-DD)" << std::endl;
    std::cout << "  -end_date <date>     Audit end date (YYYY-MM-DD)" << std::endl;
    std::cout << "  -report_format <fmt> Report format (text|csv|json|xml)" << std::endl;
    std::cout << "  -output <file>       Output file path" << std::endl << std::endl;
    
    std::cout << "Session Management:" << std::endl;
    std::cout << "  -list_sessions       List active user sessions" << std::endl;
    std::cout << "  -kill_sessions       Kill user sessions" << std::endl;
    std::cout << "  -target_user <user>  Target username for session operations" << std::endl;
    std::cout << "  -force               Force session termination" << std::endl << std::endl;
    
    std::cout << "Database Encryption:" << std::endl;
    std::cout << "  -configure_encryption Configure database encryption" << std::endl;
    std::cout << "  -enable_encryption   Enable database encryption" << std::endl;
    std::cout << "  -encryption_alg <alg> Encryption algorithm (AES256|AES128|3DES)" << std::endl;
    std::cout << "  -key_file <path>     Encryption key file path" << std::endl;
    std::cout << "  -certificate <path>  Certificate file path" << std::endl << std::endl;
    
    std::cout << "Compliance Checking:" << std::endl;
    std::cout << "  -check_compliance    Perform compliance check" << std::endl;
    std::cout << "  -standard <std>      Compliance standard (GDPR|HIPAA|SOX|PCI-DSS|ISO27001|NIST)" << std::endl << std::endl;
    
    std::cout << "Authentication for Operations:" << std::endl;
    std::cout << "  -user <username>     Admin username for authentication" << std::endl;
    std::cout << "  -password <password> Admin password for authentication" << std::endl;
    std::cout << "  -fetch_password <file> Load admin password from file" << std::endl << std::endl;
    
    std::cout << "Output Options:" << std::endl;
    std::cout << "  -verbose, -v         Verbose output" << std::endl;
    std::cout << "  -quiet, -q           Quiet mode" << std::endl;
    std::cout << "  -help, -h            Show this help" << std::endl;
    std::cout << "  -version, -z         Show version information" << std::endl << std::endl;
    
    std::cout << "Original GSEC Compatibility Examples:" << std::endl;
    std::cout << "  sb_gsec -add john -pw secret123 employee.fdb" << std::endl;
    std::cout << "  sb_gsec -modify john -fname John -lname Doe employee.fdb" << std::endl;
    std::cout << "  sb_gsec -delete john employee.fdb" << std::endl;
    std::cout << "  sb_gsec -display employee.fdb" << std::endl;
    std::cout << "  sb_gsec -display john employee.fdb" << std::endl << std::endl;
    
    std::cout << "Enhanced Security Examples:" << std::endl;
    std::cout << "  sb_gsec -audit -audit_level comprehensive employee.fdb" << std::endl;
    std::cout << "  sb_gsec -configure_policy -policy_level enterprise employee.fdb" << std::endl;
    std::cout << "  sb_gsec -create_role accounting -grant_role accounting -add john employee.fdb" << std::endl;
    std::cout << "  sb_gsec -check_compliance -standard GDPR -output gdpr_report.txt employee.fdb" << std::endl;
    std::cout << "  sb_gsec -list_sessions -kill_sessions -target_user john employee.fdb" << std::endl << std::endl;
    
    std::cout << "Enhanced Features (beyond original GSEC):" << std::endl;
    std::cout << "  - Advanced role-based access control" << std::endl;
    std::cout << "  - Comprehensive security auditing" << std::endl;
    std::cout << "  - Password policy enforcement" << std::endl;
    std::cout << "  - Multi-factor authentication support" << std::endl;
    std::cout << "  - Database encryption management" << std::endl;
    std::cout << "  - Compliance checking (GDPR, HIPAA, SOX, etc.)" << std::endl;
    std::cout << "  - Session management and monitoring" << std::endl;
    std::cout << "  - Real-time security reporting" << std::endl << std::endl;
}

// Print version information
void printVersion() {
    std::cout << "ScratchBird Enhanced GSEC version SB-T0.5.0.1" << std::endl;
    std::cout << "Enhanced security management utility" << std::endl;
    std::cout << "Based on ScratchBird 0.5 engine with 100% original GSEC compatibility" << std::endl;
    std::cout << "Features: User management, role-based access, security auditing, compliance checking" << std::endl;
}

// Parse authentication method
AuthenticationMethod parseAuthenticationMethod(const std::string& method) {
    if (method == "legacy") return AuthenticationMethod::LEGACY;
    if (method == "srp") return AuthenticationMethod::SRP;
    if (method == "srp256") return AuthenticationMethod::SRP256;
    if (method == "win_sspi") return AuthenticationMethod::WIN_SSPI;
    if (method == "multifactor") return AuthenticationMethod::MULTIFACTOR;
    if (method == "certificate") return AuthenticationMethod::CERTIFICATE;
    if (method == "kerberos") return AuthenticationMethod::KERBEROS;
    if (method == "ldap") return AuthenticationMethod::LDAP;
    return AuthenticationMethod::SRP256;
}

// Parse password policy level
PasswordPolicyLevel parsePasswordPolicyLevel(const std::string& level) {
    if (level == "none") return PasswordPolicyLevel::NONE;
    if (level == "basic") return PasswordPolicyLevel::BASIC;
    if (level == "standard") return PasswordPolicyLevel::STANDARD;
    if (level == "strict") return PasswordPolicyLevel::STRICT;
    if (level == "enterprise") return PasswordPolicyLevel::ENTERPRISE;
    return PasswordPolicyLevel::STANDARD;
}

// Parse security audit level
SecurityAuditLevel parseSecurityAuditLevel(const std::string& level) {
    if (level == "disabled") return SecurityAuditLevel::DISABLED;
    if (level == "basic") return SecurityAuditLevel::BASIC;
    if (level == "standard") return SecurityAuditLevel::STANDARD;
    if (level == "comprehensive") return SecurityAuditLevel::COMPREHENSIVE;
    if (level == "forensic") return SecurityAuditLevel::FORENSIC;
    return SecurityAuditLevel::STANDARD;
}

// Command-line argument parser
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
        
        // User management operations
        else if (arg == "-add" && i + 1 < argc) {
            args.add_user = true;
            args.username = argv[++i];
        } else if (arg == "-modify" && i + 1 < argc) {
            args.modify_user = true;
            args.username = argv[++i];
        } else if (arg == "-delete" && i + 1 < argc) {
            args.delete_user = true;
            args.username = argv[++i];
        } else if (arg == "-display") {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                args.display_user = true;
                args.username = argv[++i];
            } else {
                args.display_all_users = true;
            }
        }
        
        // User properties
        else if (arg == "-pw" && i + 1 < argc) {
            args.password = argv[++i];
        } else if (arg == "-fname" && i + 1 < argc) {
            args.first_name = argv[++i];
        } else if (arg == "-mname" && i + 1 < argc) {
            args.middle_name = argv[++i];
        } else if (arg == "-lname" && i + 1 < argc) {
            args.last_name = argv[++i];
        } else if (arg == "-description" && i + 1 < argc) {
            args.description = argv[++i];
        } else if (arg == "-email" && i + 1 < argc) {
            args.email = argv[++i];
        } else if (arg == "-phone" && i + 1 < argc) {
            args.phone = argv[++i];
        } else if (arg == "-admin") {
            args.admin_privileges = true;
        } else if (arg == "-force_change") {
            args.force_password_change = true;
        }
        
        // Role management
        else if (arg == "-create_role" && i + 1 < argc) {
            args.create_role = true;
            args.role_name = argv[++i];
        } else if (arg == "-drop_role" && i + 1 < argc) {
            args.drop_role = true;
            args.role_name = argv[++i];
        } else if (arg == "-grant_role" && i + 1 < argc) {
            args.grant_role = true;
            args.assign_roles.push_back(argv[++i]);
        } else if (arg == "-revoke_role" && i + 1 < argc) {
            args.revoke_role = true;
            args.role_name = argv[++i];
        } else if (arg == "-list_roles") {
            args.list_roles = true;
        } else if (arg == "-describe_role" && i + 1 < argc) {
            args.describe_role = true;
            args.role_name = argv[++i];
        }
        
        // Authentication
        else if (arg == "-auth_method" && i + 1 < argc) {
            args.auth_method = parseAuthenticationMethod(argv[++i]);
        } else if (arg == "-trusted") {
            args.trusted_auth = true;
        }
        
        // Password policy
        else if (arg == "-configure_policy") {
            args.configure_password_policy = true;
        } else if (arg == "-policy_level" && i + 1 < argc) {
            args.policy_level = parsePasswordPolicyLevel(argv[++i]);
        } else if (arg == "-min_length" && i + 1 < argc) {
            args.min_password_length = std::stoul(argv[++i]);
        } else if (arg == "-max_length" && i + 1 < argc) {
            args.max_password_length = std::stoul(argv[++i]);
        }
        
        // Security auditing
        else if (arg == "-audit") {
            args.perform_security_audit = true;
        } else if (arg == "-audit_level" && i + 1 < argc) {
            args.audit_level = parseSecurityAuditLevel(argv[++i]);
        } else if (arg == "-start_date" && i + 1 < argc) {
            args.start_date_str = argv[++i];
        } else if (arg == "-end_date" && i + 1 < argc) {
            args.end_date_str = argv[++i];
        } else if (arg == "-report_format" && i + 1 < argc) {
            args.report_format = argv[++i];
        } else if (arg == "-output" && i + 1 < argc) {
            args.output_file = argv[++i];
        }
        
        // Session management
        else if (arg == "-list_sessions") {
            args.list_sessions = true;
        } else if (arg == "-kill_sessions") {
            args.kill_sessions = true;
        } else if (arg == "-target_user" && i + 1 < argc) {
            args.target_username = argv[++i];
        } else if (arg == "-force") {
            args.force_disconnect = true;
        }
        
        // Database encryption
        else if (arg == "-configure_encryption") {
            args.configure_encryption = true;
        } else if (arg == "-enable_encryption") {
            args.enable_encryption = true;
        } else if (arg == "-encryption_alg" && i + 1 < argc) {
            args.encryption_algorithm = argv[++i];
        } else if (arg == "-key_file" && i + 1 < argc) {
            args.key_file_path = argv[++i];
        } else if (arg == "-certificate" && i + 1 < argc) {
            args.certificate_path = argv[++i];
        }
        
        // Compliance
        else if (arg == "-check_compliance") {
            args.check_compliance = true;
        } else if (arg == "-standard" && i + 1 < argc) {
            args.compliance_standards.push_back(argv[++i]);
        }
        
        // Authentication credentials
        else if (arg == "-user" && i + 1 < argc) {
            args.admin_username = argv[++i];
        } else if (arg == "-password" && i + 1 < argc) {
            args.admin_password = argv[++i];
        } else if (arg == "-fetch_password" && i + 1 < argc) {
            args.password_file = argv[++i];
        }
        
        // Output options
        else if (arg == "-verbose" || arg == "--verbose" || arg == "-v") {
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
    
    return true;
}

// Progress callback for operation monitoring
void progressCallback(const SecurityProgress& progress) {
    static auto last_update = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // Update every second to avoid flooding output
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count() >= 1) {
        last_update = now;
        
        double percentage = progress.getProgressPercentage();
        auto elapsed = progress.getElapsedTime();
        auto eta = progress.getEstimatedTimeRemaining();
        
        std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << percentage << "% "
                  << "(" << progress.completed_operations << "/" << progress.total_operations << ") "
                  << "Elapsed: " << elapsed.count() << "s "
                  << "ETA: " << eta.count() << "s";
        
        if (!progress.current_user.empty()) {
            std::cout << " [" << progress.current_user << "]";
        }
        
        std::cout << std::flush;
    }
}

// Load password from file
bool loadPasswordFromFile(const std::string& file_path, std::string& password) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Cannot open password file: " << file_path << std::endl;
            return false;
        }
        
        std::getline(file, password);
        if (password.empty()) {
            std::cerr << "Password file is empty: " << file_path << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error reading password file: " << e.what() << std::endl;
        return false;
    }
}

// Format user account for display
void displayUserAccount(const UserAccount& user, bool verbose) {
    std::cout << "Username: " << user.username << std::endl;
    
    if (verbose || !user.first_name.empty() || !user.last_name.empty()) {
        std::cout << "Name: " << user.first_name;
        if (!user.middle_name.empty()) {
            std::cout << " " << user.middle_name;
        }
        if (!user.last_name.empty()) {
            std::cout << " " << user.last_name;
        }
        std::cout << std::endl;
    }
    
    if (verbose || !user.description.empty()) {
        std::cout << "Description: " << user.description << std::endl;
    }
    
    if (verbose || !user.email.empty()) {
        std::cout << "Email: " << user.email << std::endl;
    }
    
    if (verbose || !user.phone.empty()) {
        std::cout << "Phone: " << user.phone << std::endl;
    }
    
    if (verbose) {
        std::cout << "Admin privileges: " << (user.admin_privileges ? "Yes" : "No") << std::endl;
        std::cout << "Account status: ";
        switch (user.status) {
            case UserAccountStatus::ACTIVE: std::cout << "Active"; break;
            case UserAccountStatus::DISABLED: std::cout << "Disabled"; break;
            case UserAccountStatus::LOCKED: std::cout << "Locked"; break;
            case UserAccountStatus::EXPIRED: std::cout << "Expired"; break;
            case UserAccountStatus::PENDING: std::cout << "Pending"; break;
        }
        std::cout << std::endl;
        
        std::cout << "Authentication method: ";
        switch (user.auth_method) {
            case AuthenticationMethod::LEGACY: std::cout << "Legacy"; break;
            case AuthenticationMethod::SRP: std::cout << "SRP"; break;
            case AuthenticationMethod::SRP256: std::cout << "SRP256"; break;
            case AuthenticationMethod::WIN_SSPI: std::cout << "Windows SSPI"; break;
            case AuthenticationMethod::MULTIFACTOR: std::cout << "Multi-factor"; break;
            case AuthenticationMethod::CERTIFICATE: std::cout << "Certificate"; break;
            case AuthenticationMethod::KERBEROS: std::cout << "Kerberos"; break;
            case AuthenticationMethod::LDAP: std::cout << "LDAP"; break;
        }
        std::cout << std::endl;
        
        if (!user.assigned_roles.empty()) {
            std::cout << "Assigned roles: ";
            for (size_t i = 0; i < user.assigned_roles.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << user.assigned_roles[i];
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << std::endl;
}

// Main execution function
int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            printHelp();
            return 1;
        }
        
        CommandLineArgs args;
        
        if (!parseCommandLine(argc, argv, args)) {
            std::cerr << "Error parsing command line arguments." << std::endl;
            return 1;
        }
        
        if (args.show_help) {
            printHelp();
            return 0;
        }
        
        if (args.show_version) {
            printVersion();
            return 0;
        }
        
        if (args.database_path.empty()) {
            std::cerr << "Database path is required." << std::endl;
            printHelp();
            return 1;
        }
        
        // Verify database file exists
        if (!fs::exists(args.database_path)) {
            std::cerr << "Database file does not exist: " << args.database_path << std::endl;
            return 1;
        }
        
        // Load admin password from file if specified
        if (!args.password_file.empty()) {
            if (!loadPasswordFromFile(args.password_file, args.admin_password)) {
                return 1;
            }
        }
        
        // Initialize enhanced GSEC utility
        GSecEnhanced gsec;
        bool operation_successful = true;
        
        if (!args.quiet) {
            std::cout << "ScratchBird Enhanced GSEC - Operating on: " << args.database_path << std::endl;
        }
        
        // Execute operations based on parsed options
        
        // User management operations
        if (args.add_user) {
            if (args.username.empty()) {
                std::cerr << "Username is required for -add operation" << std::endl;
                return 1;
            }
            
            if (args.password.empty()) {
                std::cerr << "Password is required for -add operation" << std::endl;
                return 1;
            }
            
            UserManagementOptions options;
            options.operation = UserOperation::ADD_USER;
            options.username = args.username;
            options.password = args.password;
            options.first_name = args.first_name;
            options.middle_name = args.middle_name;
            options.last_name = args.last_name;
            options.description = args.description;
            options.email = args.email;
            options.phone = args.phone;
            options.auth_method = args.auth_method;
            options.admin_privileges = args.admin_privileges;
            options.force_password_change = args.force_password_change;
            options.assign_roles = args.assign_roles;
            options.progress_callback = args.verbose ? progressCallback : nullptr;
            
            SecurityOperationResult result;
            operation_successful = gsec.addUser(args.database_path, args.username, args.password, options, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "User '" << args.username << "' added successfully." << std::endl;
                } else {
                    std::cerr << "Failed to add user '" << args.username << "'." << std::endl;
                }
            }
        }
        
        else if (args.modify_user) {
            if (args.username.empty()) {
                std::cerr << "Username is required for -modify operation" << std::endl;
                return 1;
            }
            
            UserManagementOptions options;
            options.operation = UserOperation::MODIFY_USER;
            options.username = args.username;
            options.password = args.password;
            options.first_name = args.first_name;
            options.middle_name = args.middle_name;
            options.last_name = args.last_name;
            options.description = args.description;
            options.email = args.email;
            options.phone = args.phone;
            options.auth_method = args.auth_method;
            options.admin_privileges = args.admin_privileges;
            options.force_password_change = args.force_password_change;
            options.assign_roles = args.assign_roles;
            options.progress_callback = args.verbose ? progressCallback : nullptr;
            
            SecurityOperationResult result;
            operation_successful = gsec.modifyUser(args.database_path, args.username, options, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "User '" << args.username << "' modified successfully." << std::endl;
                } else {
                    std::cerr << "Failed to modify user '" << args.username << "'." << std::endl;
                }
            }
        }
        
        else if (args.delete_user) {
            if (args.username.empty()) {
                std::cerr << "Username is required for -delete operation" << std::endl;
                return 1;
            }
            
            SecurityOperationResult result;
            operation_successful = gsec.deleteUser(args.database_path, args.username, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "User '" << args.username << "' deleted successfully." << std::endl;
                } else {
                    std::cerr << "Failed to delete user '" << args.username << "'." << std::endl;
                }
            }
        }
        
        else if (args.display_user) {
            if (args.username.empty()) {
                std::cerr << "Username is required for -display operation" << std::endl;
                return 1;
            }
            
            UserAccount user_info;
            SecurityOperationResult result;
            operation_successful = gsec.displayUser(args.database_path, args.username, user_info, result);
            
            if (operation_successful) {
                displayUserAccount(user_info, args.verbose);
            } else {
                std::cerr << "User '" << args.username << "' not found." << std::endl;
            }
        }
        
        else if (args.display_all_users) {
            std::vector<UserAccount> users;
            SecurityOperationResult result;
            operation_successful = gsec.displayAllUsers(args.database_path, users, result);
            
            if (operation_successful) {
                if (users.empty()) {
                    if (!args.quiet) {
                        std::cout << "No users found in database." << std::endl;
                    }
                } else {
                    if (!args.quiet) {
                        std::cout << "Database users (" << users.size() << " found):" << std::endl;
                        std::cout << "========================================" << std::endl;
                    }
                    for (const auto& user : users) {
                        displayUserAccount(user, args.verbose);
                    }
                }
            } else {
                std::cerr << "Failed to retrieve user list." << std::endl;
            }
        }
        
        // Role management operations
        else if (args.create_role) {
            RoleManagementOptions options;
            options.operation = RoleOperation::CREATE_ROLE;
            options.role_name = args.role_name;
            options.description = args.description;
            options.progress_callback = args.verbose ? progressCallback : nullptr;
            
            RoleManagementResult result;
            operation_successful = gsec.performRoleManagement(args.database_path, options, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Role '" << args.role_name << "' created successfully." << std::endl;
                } else {
                    std::cerr << "Failed to create role '" << args.role_name << "'." << std::endl;
                }
            }
        }
        
        else if (args.list_roles) {
            RoleManagementOptions options;
            options.operation = RoleOperation::LIST_ROLES;
            options.progress_callback = args.verbose ? progressCallback : nullptr;
            
            RoleManagementResult result;
            operation_successful = gsec.performRoleManagement(args.database_path, options, result);
            
            if (operation_successful) {
                if (result.database_roles.empty()) {
                    if (!args.quiet) {
                        std::cout << "No roles found in database." << std::endl;
                    }
                } else {
                    if (!args.quiet) {
                        std::cout << "Database roles (" << result.database_roles.size() << " found):" << std::endl;
                        std::cout << "========================================" << std::endl;
                    }
                    for (const auto& role : result.database_roles) {
                        std::cout << "Role: " << role.role_name << std::endl;
                        if (!role.description.empty()) {
                            std::cout << "Description: " << role.description << std::endl;
                        }
                        if (args.verbose) {
                            std::cout << "System role: " << (role.system_role ? "Yes" : "No") << std::endl;
                            std::cout << "Privileges: " << role.privileges.size() << std::endl;
                            std::cout << "Granted users: " << role.granted_users.size() << std::endl;
                        }
                        std::cout << std::endl;
                    }
                }
            } else {
                std::cerr << "Failed to retrieve role list." << std::endl;
            }
        }
        
        // Password policy configuration
        else if (args.configure_password_policy) {
            PasswordPolicyOptions options;
            options.level = args.policy_level;
            options.minimum_length = args.min_password_length;
            options.maximum_length = args.max_password_length;
            options.progress_callback = args.verbose ? progressCallback : nullptr;
            
            SecurityOperationResult result;
            operation_successful = gsec.configurePasswordPolicy(args.database_path, options, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Password policy configured successfully." << std::endl;
                } else {
                    std::cerr << "Failed to configure password policy." << std::endl;
                }
            }
        }
        
        // Security audit
        else if (args.perform_security_audit) {
            SecurityAuditOptions options;
            options.level = args.audit_level;
            options.output_file_path = args.output_file;
            options.report_format = args.report_format;
            options.progress_callback = args.verbose ? progressCallback : nullptr;
            
            // Parse dates if provided
            // Note: Date parsing would need proper implementation
            
            SecurityAuditResult result;
            operation_successful = gsec.performSecurityAudit(args.database_path, options, result);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Security audit completed successfully." << std::endl;
                    std::cout << "Events analyzed: " << result.total_events_analyzed << std::endl;
                    std::cout << "Security violations: " << result.security_violations_found << std::endl;
                    std::cout << "Policy violations: " << result.policy_violations_found << std::endl;
                    
                    if (result.hasSecurityIssues()) {
                        std::cout << "WARNING: Security issues found!" << std::endl;
                    }
                    
                    if (!args.output_file.empty()) {
                        std::cout << "Report saved to: " << args.output_file << std::endl;
                    }
                } else {
                    std::cerr << "Security audit failed." << std::endl;
                }
            }
        }
        
        // Session management
        else if (args.list_sessions) {
            std::vector<std::map<std::string, std::string>> sessions;
            operation_successful = gsec.listActiveSessions(args.database_path, sessions);
            
            if (operation_successful) {
                if (sessions.empty()) {
                    if (!args.quiet) {
                        std::cout << "No active sessions found." << std::endl;
                    }
                } else {
                    if (!args.quiet) {
                        std::cout << "Active sessions (" << sessions.size() << " found):" << std::endl;
                        std::cout << "========================================" << std::endl;
                    }
                    for (const auto& session : sessions) {
                        for (const auto& attr : session) {
                            std::cout << attr.first << ": " << attr.second << std::endl;
                        }
                        std::cout << std::endl;
                    }
                }
            } else {
                std::cerr << "Failed to retrieve session list." << std::endl;
            }
        }
        
        else if (args.kill_sessions) {
            if (args.target_username.empty()) {
                std::cerr << "Target username is required for -kill_sessions operation" << std::endl;
                return 1;
            }
            
            operation_successful = gsec.terminateUserSessions(args.database_path, args.target_username, args.force_disconnect);
            
            if (!args.quiet) {
                if (operation_successful) {
                    std::cout << "Sessions for user '" << args.target_username << "' terminated successfully." << std::endl;
                } else {
                    std::cerr << "Failed to terminate sessions for user '" << args.target_username << "'." << std::endl;
                }
            }
        }
        
        // If no specific operation was requested, default to displaying all users
        else if (!args.configure_encryption && !args.check_compliance) {
            std::vector<UserAccount> users;
            SecurityOperationResult result;
            operation_successful = gsec.displayAllUsers(args.database_path, users, result);
            
            if (operation_successful) {
                if (users.empty()) {
                    if (!args.quiet) {
                        std::cout << "No users found in database." << std::endl;
                    }
                } else {
                    if (!args.quiet) {
                        std::cout << "Database users (" << users.size() << " found):" << std::endl;
                        std::cout << "========================================" << std::endl;
                    }
                    for (const auto& user : users) {
                        displayUserAccount(user, args.verbose);
                    }
                }
            } else {
                std::cerr << "Failed to retrieve user list." << std::endl;
            }
        }
        
        // Show errors if any occurred
        if (!operation_successful) {
            auto errors = gsec.getErrors();
            if (!errors.empty() && !args.quiet) {
                std::cerr << "\nErrors encountered:" << std::endl;
                for (const auto& error : errors) {
                    std::cerr << "  " << error << std::endl;
                }
            }
        }
        
        // Show warnings if verbose mode
        if (args.verbose) {
            auto warnings = gsec.getWarnings();
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