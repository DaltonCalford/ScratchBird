#include <iostream>
#include <cstdio>
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/transaction_manager.h"

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
    
    // First open - allocate some pages
    {
        Database db;
        ErrorContext ctx;
        Status status = db.open(db_path, &ctx);
        if (status != Status::Ok) {
            std::cerr << "Failed to open database (1): " << ctx.message << std::endl;
            return 1;
        }
        std::cout << "First open successful" << std::endl;
        
        // Get page manager and allocate pages
        PageManager* pm = db.page_manager();
        for (int i = 0; i < 3; i++) {
            uint32_t page_id;
            status = pm->allocate_page(page_id, &ctx);
            if (status != Status::Ok) {
                std::cerr << "Failed to allocate page " << i << ": " << ctx.message << std::endl;
                return 1;
            }
            std::cout << "Allocated page " << page_id << std::endl;
        }
        
        // Sync before close
        db.sync(&ctx);
        std::cout << "Database synced" << std::endl;
        
        // Database closes here
    }
    
    // Second open - this is where the error occurs
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