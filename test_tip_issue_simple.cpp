#include <iostream>
#include <cstdio>
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

int main() {
    const char* db_path = "test_tip_debug.db";
    
    // Remove existing file
    std::remove(db_path);
    
    // Create database
    {
        ErrorContext ctx;
        Status status = Database::create(db_path, 16384, &ctx);
        if (status != Status::Ok) {
            std::cerr << "Failed to create database: " << ctx.message << std::endl;
            return 1;
        }
        std::cout << "Database created successfully" << std::endl;
    }
    
    // First open - just open and close
    {
        Database db;
        ErrorContext ctx;
        Status status = db.open(db_path, &ctx);
        if (status != Status::Ok) {
            std::cerr << "Failed to open database (1): " << ctx.message << std::endl;
            return 1;
        }
        std::cout << "First open successful" << std::endl;
        
        // Sync before close
        db.sync(&ctx);
        std::cout << "Database synced" << std::endl;
        
        // Database closes here
    }
    
    // Second open - this might show the error
    {
        Database db;
        ErrorContext ctx;
        Status status = db.open(db_path, &ctx);
        if (status != Status::Ok) {
            std::cerr << "Failed to open database (2): " << ctx.message << std::endl;
            std::cerr << "Error code: " << static_cast<int>(status) << std::endl;
            return 1;
        }
        std::cout << "Second open successful" << std::endl;
    }
    
    std::cout << "Test completed successfully" << std::endl;
    std::remove(db_path);
    return 0;
}