/*
 * ScratchBird v0.5.0 - API Example 01: Database Creation
 * 
 * This example demonstrates creating a new database using the ScratchBird
 * C++ API with hierarchical schema support.
 * 
 * Features demonstrated:
 * - Database creation with SQL Dialect 4
 * - Hierarchical schema creation
 * - Modern C++ exception handling
 * - Connection management
 * - Basic table operations
 * 
 * Compilation:
 * g++ -std=c++17 -I../include -o sb_api_01_create sb_api_01_create.cpp -lsbclient
 * 
 * Usage:
 * ./sb_api_01_create
 * 
 * The contents of this file are subject to the Initial
 * Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 * http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 * 
 * Software distributed under the License is distributed AS IS,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the License for the specific language governing rights
 * and limitations under the License.
 * 
 * The Original Code was created by ScratchBird Development Team
 * for the ScratchBird Open Source RDBMS project.
 * 
 * Copyright (c) 2025 ScratchBird Development Team
 * and all contributors signed below.
 * 
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstdlib>

// ScratchBird API includes
#include "sb_api.h"

// Environment variable helpers
class Environment {
public:
    static std::string get(const std::string& name, const std::string& defaultValue = "") {
        const char* value = std::getenv(name.c_str());
        return value ? std::string(value) : defaultValue;
    }
};

// ScratchBird connection parameters
struct ConnectionParams {
    std::string host = "localhost";
    std::string port = "4050";  // ScratchBird default port
    std::string user = "SYSDBA";
    std::string password = "masterkey";
    std::string database = "example_db.fdb";
    
    ConnectionParams() {
        // Override with environment variables if available
        host = Environment::get("SB_HOST", host);
        port = Environment::get("SB_PORT", port);
        user = Environment::get("SB_USER", user);
        password = Environment::get("SB_PASSWORD", password);
        database = Environment::get("SB_DATABASE", database);
    }
    
    std::string getConnectionString() const {
        return host + "/" + port + ":" + database;
    }
};

// Database creation and schema management class
class ScratchBirdExample {
private:
    ConnectionParams params;
    sb_connection_handle connection = nullptr;
    sb_transaction_handle transaction = nullptr;
    
public:
    ScratchBirdExample() {
        std::cout << "ScratchBird API Example 01: Database Creation" << std::endl;
        std::cout << "=============================================" << std::endl;
        std::cout << "Version: SB-T0.5.0.1 ScratchBird 0.5 f90eae0" << std::endl;
        std::cout << std::endl;
    }
    
    ~ScratchBirdExample() {
        cleanup();
    }
    
    void run() {
        try {
            createDatabase();
            connectToDatabase();
            createHierarchicalSchemas();
            createTables();
            insertSampleData();
            demonstrateQueries();
            
            std::cout << "Example completed successfully!" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            throw;
        }
    }
    
private:
    void createDatabase() {
        std::cout << "Creating database: " << params.getConnectionString() << std::endl;
        
        // Create database with SQL Dialect 4 for hierarchical schema support
        std::string createDbSQL = 
            "CREATE DATABASE '" + params.getConnectionString() + "' "
            "USER '" + params.user + "' "
            "PASSWORD '" + params.password + "' "
            "DEFAULT CHARACTER SET UTF8 "
            "SQL DIALECT 4";
        
        sb_status_array status;
        sb_connection_handle tempConnection = nullptr;
        
        // Execute create database statement
        if (sb_dsql_execute_immediate(status, &tempConnection, nullptr, 0, 
                                     createDbSQL.c_str(), 1, nullptr) != 0) {
            
            // Check if database already exists
            long sqlcode = sb_sqlcode(status);
            if (sqlcode == -902) {  // Database already exists
                std::cout << "Database already exists, continuing..." << std::endl;
            } else {
                throw std::runtime_error("Failed to create database: " + std::string(sb_sqlcode_text(sqlcode)));
            }
        } else {
            std::cout << "Database created successfully!" << std::endl;
        }
        
        // Clean up temporary connection
        if (tempConnection) {
            sb_detach_database(status, &tempConnection);
        }
    }
    
    void connectToDatabase() {
        std::cout << "Connecting to database..." << std::endl;
        
        sb_status_array status;
        
        // Build database parameter buffer
        std::string dbParams = params.user + ":" + params.password;
        
        // Connect to database
        if (sb_attach_database(status, 0, params.getConnectionString().c_str(), 
                              &connection, dbParams.length(), dbParams.c_str()) != 0) {
            throw std::runtime_error("Failed to connect to database");
        }
        
        std::cout << "Connected successfully!" << std::endl;
    }
    
    void createHierarchicalSchemas() {
        std::cout << "Creating hierarchical schemas..." << std::endl;
        
        // Start transaction
        startTransaction();
        
        // Create schema hierarchy
        std::vector<std::string> schemas = {
            "CREATE SCHEMA company",
            "CREATE SCHEMA company.finance",
            "CREATE SCHEMA company.finance.accounting",
            "CREATE SCHEMA company.finance.accounting.reports",
            "CREATE SCHEMA company.hr",
            "CREATE SCHEMA company.hr.employees",
            "CREATE SCHEMA company.operations",
            "CREATE SCHEMA company.operations.manufacturing"
        };
        
        for (const auto& schemaSQL : schemas) {
            executeSQL(schemaSQL);
            std::cout << "  " << schemaSQL << std::endl;
        }
        
        // Commit transaction
        commitTransaction();
        
        std::cout << "Hierarchical schemas created successfully!" << std::endl;
    }
    
    void createTables() {
        std::cout << "Creating tables in hierarchical schemas..." << std::endl;
        
        startTransaction();
        
        // Set schema context
        executeSQL("SET SCHEMA 'company.finance.accounting.reports'");
        
        // Create monthly reports table
        std::string createReportsTable = R"(
            CREATE TABLE monthly_reports (
                report_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
                month_year DATE NOT NULL,
                total_revenue DECIMAL(15,2),
                total_expenses DECIMAL(15,2),
                net_profit DECIMAL(15,2),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                created_by VARCHAR(50) DEFAULT USER
            )
        )";
        
        executeSQL(createReportsTable);
        std::cout << "  Created monthly_reports table" << std::endl;
        
        // Switch to HR schema
        executeSQL("SET SCHEMA 'company.hr.employees'");
        
        // Create employees table with advanced data types
        std::string createEmployeesTable = R"(
            CREATE TABLE employee_records (
                employee_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
                employee_number UINTEGER NOT NULL,
                first_name VARCHAR(50) NOT NULL,
                last_name VARCHAR(50) NOT NULL,
                email VARCHAR(100) UNIQUE,
                hire_date DATE NOT NULL,
                salary DECIMAL(10,2),
                department_id UUID,
                is_active BOOLEAN DEFAULT TRUE,
                work_location INET,
                mac_address MACADDR,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        executeSQL(createEmployeesTable);
        std::cout << "  Created employee_records table" << std::endl;
        
        commitTransaction();
        
        std::cout << "Tables created successfully!" << std::endl;
    }
    
    void insertSampleData() {
        std::cout << "Inserting sample data..." << std::endl;
        
        startTransaction();
        
        // Insert into monthly reports
        executeSQL("SET SCHEMA 'company.finance.accounting.reports'");
        
        std::string insertReports = R"(
            INSERT INTO monthly_reports (month_year, total_revenue, total_expenses, net_profit)
            VALUES 
                ('2025-01-01', 150000.00, 120000.00, 30000.00),
                ('2025-02-01', 165000.00, 125000.00, 40000.00),
                ('2025-03-01', 180000.00, 130000.00, 50000.00)
        )";
        
        executeSQL(insertReports);
        std::cout << "  Inserted monthly reports data" << std::endl;
        
        // Insert into employees
        executeSQL("SET SCHEMA 'company.hr.employees'");
        
        std::string insertEmployees = R"(
            INSERT INTO employee_records (employee_number, first_name, last_name, email, hire_date, salary, work_location, mac_address)
            VALUES 
                (1001, 'John', 'Smith', 'john.smith@company.com', '2023-01-15', 75000.00, '192.168.1.100', '00:11:22:33:44:55'),
                (1002, 'Jane', 'Doe', 'jane.doe@company.com', '2023-02-20', 82000.00, '192.168.1.101', '00:11:22:33:44:56'),
                (1003, 'Bob', 'Johnson', 'bob.johnson@company.com', '2023-03-10', 78000.00, '192.168.1.102', '00:11:22:33:44:57')
        )";
        
        executeSQL(insertEmployees);
        std::cout << "  Inserted employee records data" << std::endl;
        
        commitTransaction();
        
        std::cout << "Sample data inserted successfully!" << std::endl;
    }
    
    void demonstrateQueries() {
        std::cout << "Demonstrating queries..." << std::endl;
        
        startTransaction();
        
        // Query 1: Monthly reports with schema context
        std::cout << "Query 1: Monthly reports from company.finance.accounting.reports schema" << std::endl;
        executeSQL("SET SCHEMA 'company.finance.accounting.reports'");
        
        std::string query1 = R"(
            SELECT 
                EXTRACT(MONTH FROM month_year) as month,
                EXTRACT(YEAR FROM month_year) as year,
                total_revenue,
                total_expenses,
                net_profit,
                created_at
            FROM monthly_reports
            ORDER BY month_year
        )";
        
        queryAndDisplay(query1);
        
        // Query 2: Employee records with network types
        std::cout << "\nQuery 2: Employee records with network information" << std::endl;
        executeSQL("SET SCHEMA 'company.hr.employees'");
        
        std::string query2 = R"(
            SELECT 
                employee_number,
                first_name || ' ' || last_name as full_name,
                email,
                hire_date,
                salary,
                work_location,
                mac_address
            FROM employee_records
            WHERE is_active = TRUE
            ORDER BY hire_date
        )";
        
        queryAndDisplay(query2);
        
        // Query 3: Cross-schema query
        std::cout << "\nQuery 3: Cross-schema query (reports + employees)" << std::endl;
        
        std::string query3 = R"(
            SELECT 
                'Financial Report' as data_type,
                EXTRACT(MONTH FROM r.month_year) as month,
                r.total_revenue,
                r.created_by
            FROM company.finance.accounting.reports.monthly_reports r
            WHERE r.month_year >= '2025-01-01'
            UNION ALL
            SELECT 
                'Employee Record' as data_type,
                EXTRACT(MONTH FROM e.hire_date) as month,
                e.salary,
                e.first_name || ' ' || e.last_name
            FROM company.hr.employees.employee_records e
            WHERE e.hire_date >= '2023-01-01'
            ORDER BY month
        )";
        
        queryAndDisplay(query3);
        
        commitTransaction();
        
        std::cout << "Queries completed successfully!" << std::endl;
    }
    
    void startTransaction() {
        if (transaction) {
            return;  // Transaction already active
        }
        
        sb_status_array status;
        if (sb_start_transaction(status, &transaction, 1, &connection, 0, nullptr) != 0) {
            throw std::runtime_error("Failed to start transaction");
        }
    }
    
    void commitTransaction() {
        if (!transaction) {
            return;  // No active transaction
        }
        
        sb_status_array status;
        if (sb_commit_transaction(status, &transaction) != 0) {
            throw std::runtime_error("Failed to commit transaction");
        }
        transaction = nullptr;
    }
    
    void executeSQL(const std::string& sql) {
        sb_status_array status;
        
        if (sb_dsql_execute_immediate(status, &connection, &transaction, 0, 
                                     sql.c_str(), 1, nullptr) != 0) {
            throw std::runtime_error("Failed to execute SQL: " + sql);
        }
    }
    
    void queryAndDisplay(const std::string& sql) {
        sb_status_array status;
        sb_statement_handle stmt = nullptr;
        
        try {
            // Prepare statement
            if (sb_dsql_allocate_statement(status, &connection, &stmt) != 0) {
                throw std::runtime_error("Failed to allocate statement");
            }
            
            if (sb_dsql_prepare(status, &transaction, &stmt, 0, sql.c_str(), 1, nullptr) != 0) {
                throw std::runtime_error("Failed to prepare statement");
            }
            
            // Execute query
            if (sb_dsql_execute(status, &transaction, &stmt, 1, nullptr) != 0) {
                throw std::runtime_error("Failed to execute query");
            }
            
            // Note: This is a simplified example
            // In a real implementation, you would:
            // 1. Use sb_dsql_describe to get column information
            // 2. Allocate SQLDA for output
            // 3. Use sb_dsql_fetch to retrieve rows
            // 4. Format and display the results
            
            std::cout << "  Query executed successfully (results would be displayed here)" << std::endl;
            
        } catch (...) {
            if (stmt) {
                sb_dsql_free_statement(status, &stmt, DSQL_close);
            }
            throw;
        }
        
        if (stmt) {
            sb_dsql_free_statement(status, &stmt, DSQL_close);
        }
    }
    
    void cleanup() {
        sb_status_array status;
        
        if (transaction) {
            sb_rollback_transaction(status, &transaction);
            transaction = nullptr;
        }
        
        if (connection) {
            sb_detach_database(status, &connection);
            connection = nullptr;
        }
    }
};

// Error handling helper
class ScratchBirdError : public std::runtime_error {
public:
    ScratchBirdError(const std::string& message) : std::runtime_error(message) {}
};

int main() {
    try {
        ScratchBirdExample example;
        example.run();
        
        std::cout << "\n=============================================" << std::endl;
        std::cout << "ScratchBird API Example 01 completed successfully!" << std::endl;
        std::cout << "Features demonstrated:" << std::endl;
        std::cout << "- Database creation with SQL Dialect 4" << std::endl;
        std::cout << "- Hierarchical schema creation and navigation" << std::endl;
        std::cout << "- Advanced data types (UUID, INET, MACADDR)" << std::endl;
        std::cout << "- Modern C++ exception handling" << std::endl;
        std::cout << "- Cross-schema queries" << std::endl;
        std::cout << "=============================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

/*
 * Build Instructions:
 * 
 * Linux:
 * g++ -std=c++17 -I../include -o sb_api_01_create sb_api_01_create.cpp -lsbclient
 * 
 * Windows (MinGW):
 * x86_64-w64-mingw32-g++ -std=c++17 -I../include -o sb_api_01_create.exe sb_api_01_create.cpp -lsbclient
 * 
 * Usage:
 * ./sb_api_01_create
 * 
 * Environment Variables:
 * SB_HOST=localhost
 * SB_PORT=4050
 * SB_USER=SYSDBA
 * SB_PASSWORD=masterkey
 * SB_DATABASE=example_db.fdb
 * 
 * This example demonstrates the power of ScratchBird's hierarchical schema system
 * combined with PostgreSQL-compatible data types, providing enterprise-grade
 * database organization capabilities that exceed PostgreSQL's own schema system.
 */