/*
 * ScratchBird v0.5.0 - Interface Example 01: Database Creation
 * 
 * This example demonstrates creating a database using the ScratchBird
 * modern C++ interface with hierarchical schema support.
 * 
 * Features demonstrated:
 * - Modern C++ interfaces (no GPRE required)
 * - RAII-based resource management
 * - Hierarchical schema creation
 * - Exception-safe database operations
 * - PostgreSQL-compatible data types
 * 
 * Compilation:
 * g++ -std=c++17 -I../include -o sb_interface_01_create sb_interface_01_create.cpp -lsbclient
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
#include <vector>

// ScratchBird modern C++ interface
#include "sb_interface.h"

using namespace ScratchBird;

class DatabaseManager {
private:
    std::unique_ptr<Database> db;
    std::unique_ptr<Transaction> tx;
    
public:
    DatabaseManager() {
        std::cout << "ScratchBird Interface Example 01: Database Creation" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << "Version: SB-T0.5.0.1 ScratchBird 0.5 f90eae0" << std::endl;
        std::cout << std::endl;
    }
    
    ~DatabaseManager() {
        cleanup();
    }
    
    void run() {
        try {
            createDatabase();
            connectToDatabase();
            createHierarchicalSchemas();
            createTables();
            insertData();
            runQueries();
            
            std::cout << "Example completed successfully!" << std::endl;
            
        } catch (const ScratchBirdException& e) {
            std::cerr << "ScratchBird Error: " << e.what() << std::endl;
            std::cerr << "Error Code: " << e.getErrorCode() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            throw;
        }
    }
    
private:
    void createDatabase() {
        std::cout << "Creating database..." << std::endl;
        
        std::string dbPath = "localhost:interface_example.fdb";
        
        try {
            // Create database with SQL Dialect 4
            DatabaseFactory::createDatabase(dbPath, "SYSDBA", "masterkey", 
                                          DatabaseOptions()
                                              .setDialect(4)
                                              .setCharacterSet("UTF8")
                                              .setPageSize(8192));
            
            std::cout << "Database created successfully!" << std::endl;
        } catch (const DatabaseExistsException& e) {
            std::cout << "Database already exists, continuing..." << std::endl;
        }
    }
    
    void connectToDatabase() {
        std::cout << "Connecting to database..." << std::endl;
        
        ConnectionParams params;
        params.database = "localhost:interface_example.fdb";
        params.username = "SYSDBA";
        params.password = "masterkey";
        params.role = "";
        params.characterSet = "UTF8";
        
        db = DatabaseFactory::connect(params);
        std::cout << "Connected successfully!" << std::endl;
    }
    
    void createHierarchicalSchemas() {
        std::cout << "Creating hierarchical schemas..." << std::endl;
        
        tx = db->startTransaction();
        
        // Create schema hierarchy
        std::vector<std::string> schemas = {
            "CREATE SCHEMA interface_demo",
            "CREATE SCHEMA interface_demo.users",
            "CREATE SCHEMA interface_demo.users.profiles",
            "CREATE SCHEMA interface_demo.products",
            "CREATE SCHEMA interface_demo.products.inventory",
            "CREATE SCHEMA interface_demo.orders",
            "CREATE SCHEMA interface_demo.orders.processing"
        };
        
        for (const auto& schema : schemas) {
            auto stmt = tx->prepareStatement(schema);
            stmt->execute();
            std::cout << "  " << schema << std::endl;
        }
        
        tx->commit();
        std::cout << "Hierarchical schemas created successfully!" << std::endl;
    }
    
    void createTables() {
        std::cout << "Creating tables..." << std::endl;
        
        tx = db->startTransaction();
        
        // Set schema context
        tx->setSchema("interface_demo.users.profiles");
        
        // Create user profiles table
        std::string createUsersTable = R"(
            CREATE TABLE user_profiles (
                profile_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
                user_number UINTEGER NOT NULL,
                username VARCHAR(50) UNIQUE NOT NULL,
                email VARCHAR(100) UNIQUE NOT NULL,
                full_name VARCHAR(100),
                birth_date DATE,
                last_login TIMESTAMP,
                is_active BOOLEAN DEFAULT TRUE,
                profile_data JSONB,
                avatar_url VARCHAR(500),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        auto stmt = tx->prepareStatement(createUsersTable);
        stmt->execute();
        std::cout << "  Created user_profiles table" << std::endl;
        
        // Switch to products schema
        tx->setSchema("interface_demo.products.inventory");
        
        // Create products table with advanced data types
        std::string createProductsTable = R"(
            CREATE TABLE product_inventory (
                product_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
                product_code VARCHAR(50) UNIQUE NOT NULL,
                product_name VARCHAR(200) NOT NULL,
                description TEXT,
                price DECIMAL(10,2) NOT NULL,
                quantity_in_stock UINTEGER DEFAULT 0,
                weight_kg DECIMAL(8,3),
                dimensions VARCHAR(50),
                category_id UUID,
                supplier_network INET,
                warehouse_location VARCHAR(100),
                is_active BOOLEAN DEFAULT TRUE,
                metadata JSONB,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        stmt = tx->prepareStatement(createProductsTable);
        stmt->execute();
        std::cout << "  Created product_inventory table" << std::endl;
        
        // Switch to orders schema
        tx->setSchema("interface_demo.orders.processing");
        
        // Create orders table
        std::string createOrdersTable = R"(
            CREATE TABLE order_processing (
                order_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
                order_number UINTEGER NOT NULL,
                user_id UUID NOT NULL,
                order_status VARCHAR(20) DEFAULT 'pending',
                total_amount DECIMAL(15,2) NOT NULL,
                shipping_address TEXT,
                billing_address TEXT,
                payment_method VARCHAR(50),
                order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                shipped_date TIMESTAMP,
                delivered_date TIMESTAMP,
                tracking_number VARCHAR(100),
                notes TEXT
            )
        )";
        
        stmt = tx->prepareStatement(createOrdersTable);
        stmt->execute();
        std::cout << "  Created order_processing table" << std::endl;
        
        tx->commit();
        std::cout << "Tables created successfully!" << std::endl;
    }
    
    void insertData() {
        std::cout << "Inserting sample data..." << std::endl;
        
        tx = db->startTransaction();
        
        // Insert users
        tx->setSchema("interface_demo.users.profiles");
        
        std::string insertUsers = R"(
            INSERT INTO user_profiles (user_number, username, email, full_name, birth_date, profile_data)
            VALUES (?, ?, ?, ?, ?, ?)
        )";
        
        auto stmt = tx->prepareStatement(insertUsers);
        
        // User 1
        stmt->setUInteger(1, 1001);
        stmt->setString(2, "john_doe");
        stmt->setString(3, "john.doe@example.com");
        stmt->setString(4, "John Doe");
        stmt->setDate(5, Date(1990, 5, 15));
        stmt->setString(6, R"({"preferences": {"theme": "dark", "notifications": true}})");
        stmt->execute();
        
        // User 2
        stmt->setUInteger(1, 1002);
        stmt->setString(2, "jane_smith");
        stmt->setString(3, "jane.smith@example.com");
        stmt->setString(4, "Jane Smith");
        stmt->setDate(5, Date(1985, 3, 22));
        stmt->setString(6, R"({"preferences": {"theme": "light", "notifications": false}})");
        stmt->execute();
        
        std::cout << "  Inserted user profiles" << std::endl;
        
        // Insert products
        tx->setSchema("interface_demo.products.inventory");
        
        std::string insertProducts = R"(
            INSERT INTO product_inventory (product_code, product_name, description, price, quantity_in_stock, weight_kg, supplier_network, metadata)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        stmt = tx->prepareStatement(insertProducts);
        
        // Product 1
        stmt->setString(1, "WIDGET-001");
        stmt->setString(2, "Premium Widget");
        stmt->setString(3, "High-quality widget for professional use");
        stmt->setDecimal(4, Decimal("29.99"));
        stmt->setUInteger(5, 150);
        stmt->setDecimal(6, Decimal("0.5"));
        stmt->setString(7, "192.168.1.100");
        stmt->setString(8, R"({"color": "blue", "material": "plastic", "warranty": "2 years"})");
        stmt->execute();
        
        // Product 2
        stmt->setString(1, "GADGET-002");
        stmt->setString(2, "Smart Gadget");
        stmt->setString(3, "Internet-connected smart device");
        stmt->setDecimal(4, Decimal("149.99"));
        stmt->setUInteger(5, 75);
        stmt->setDecimal(6, Decimal("1.2"));
        stmt->setString(7, "10.0.0.50");
        stmt->setString(8, R"({"connectivity": "WiFi", "battery": "rechargeable", "warranty": "1 year"})");
        stmt->execute();
        
        std::cout << "  Inserted product inventory" << std::endl;
        
        tx->commit();
        std::cout << "Sample data inserted successfully!" << std::endl;
    }
    
    void runQueries() {
        std::cout << "Running queries..." << std::endl;
        
        tx = db->startTransaction();
        
        // Query 1: User profiles
        std::cout << "\nQuery 1: User profiles" << std::endl;
        tx->setSchema("interface_demo.users.profiles");
        
        std::string query1 = R"(
            SELECT username, email, full_name, birth_date, created_at
            FROM user_profiles
            WHERE is_active = TRUE
            ORDER BY created_at
        )";
        
        auto stmt = tx->prepareStatement(query1);
        auto result = stmt->executeQuery();
        
        std::cout << "Username\t\tEmail\t\t\tFull Name\t\tBirth Date" << std::endl;
        std::cout << "--------\t\t-----\t\t\t---------\t\t----------" << std::endl;
        
        while (result->next()) {
            std::cout << result->getString("username") << "\t\t"
                      << result->getString("email") << "\t"
                      << result->getString("full_name") << "\t\t"
                      << result->getDate("birth_date").toString() << std::endl;
        }
        
        // Query 2: Product inventory
        std::cout << "\nQuery 2: Product inventory" << std::endl;
        tx->setSchema("interface_demo.products.inventory");
        
        std::string query2 = R"(
            SELECT product_code, product_name, price, quantity_in_stock, weight_kg
            FROM product_inventory
            WHERE is_active = TRUE
            ORDER BY product_name
        )";
        
        stmt = tx->prepareStatement(query2);
        result = stmt->executeQuery();
        
        std::cout << "Product Code\t\tProduct Name\t\tPrice\t\tQuantity\tWeight" << std::endl;
        std::cout << "------------\t\t------------\t\t-----\t\t--------\t------" << std::endl;
        
        while (result->next()) {
            std::cout << result->getString("product_code") << "\t\t"
                      << result->getString("product_name") << "\t\t"
                      << result->getDecimal("price").toString() << "\t\t"
                      << result->getUInteger("quantity_in_stock") << "\t\t"
                      << result->getDecimal("weight_kg").toString() << std::endl;
        }
        
        // Query 3: Cross-schema query
        std::cout << "\nQuery 3: Cross-schema summary" << std::endl;
        
        std::string query3 = R"(
            SELECT 
                'Users' as data_type,
                COUNT(*) as record_count,
                'interface_demo.users.profiles' as schema_location
            FROM interface_demo.users.profiles.user_profiles
            WHERE is_active = TRUE
            UNION ALL
            SELECT 
                'Products' as data_type,
                COUNT(*) as record_count,
                'interface_demo.products.inventory' as schema_location
            FROM interface_demo.products.inventory.product_inventory
            WHERE is_active = TRUE
            ORDER BY data_type
        )";
        
        stmt = tx->prepareStatement(query3);
        result = stmt->executeQuery();
        
        std::cout << "Data Type\t\tRecord Count\t\tSchema Location" << std::endl;
        std::cout << "---------\t\t------------\t\t---------------" << std::endl;
        
        while (result->next()) {
            std::cout << result->getString("data_type") << "\t\t\t"
                      << result->getInteger("record_count") << "\t\t\t"
                      << result->getString("schema_location") << std::endl;
        }
        
        tx->commit();
        std::cout << "\nQueries completed successfully!" << std::endl;
    }
    
    void cleanup() {
        try {
            if (tx && tx->isActive()) {
                tx->rollback();
            }
            if (db && db->isConnected()) {
                db->disconnect();
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }
};

int main() {
    try {
        DatabaseManager manager;
        manager.run();
        
        std::cout << "\n==================================================" << std::endl;
        std::cout << "ScratchBird Interface Example 01 completed successfully!" << std::endl;
        std::cout << "Features demonstrated:" << std::endl;
        std::cout << "- Modern C++ interface (no GPRE required)" << std::endl;
        std::cout << "- RAII-based resource management" << std::endl;
        std::cout << "- Hierarchical schema creation and navigation" << std::endl;
        std::cout << "- Advanced data types (UUID, UINTEGER, JSONB)" << std::endl;
        std::cout << "- Exception-safe database operations" << std::endl;
        std::cout << "- Cross-schema queries" << std::endl;
        std::cout << "==================================================" << std::endl;
        
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
 * g++ -std=c++17 -I../include -o sb_interface_01_create sb_interface_01_create.cpp -lsbclient
 * 
 * Windows (MinGW):
 * x86_64-w64-mingw32-g++ -std=c++17 -I../include -o sb_interface_01_create.exe sb_interface_01_create.cpp -lsbclient
 * 
 * Usage:
 * ./sb_interface_01_create
 * 
 * This example demonstrates ScratchBird's modern C++ interface which provides:
 * - Type-safe parameter binding
 * - Automatic memory management
 * - Exception-safe operations
 * - RAII-based resource management
 * - Full hierarchical schema support
 * - PostgreSQL-compatible data types
 */