/*
 * ScratchBird v0.5.0 - Interface Example 02: Transaction Management
 * 
 * This example demonstrates advanced transaction management using the
 * ScratchBird modern C++ interface.
 * 
 * Features demonstrated:
 * - Nested transactions with savepoints
 * - Transaction isolation levels
 * - Rollback and recovery scenarios
 * - Schema-aware transaction scoping
 * - Concurrent transaction handling
 * 
 * Compilation:
 * g++ -std=c++17 -I../include -o sb_interface_02_transactions sb_interface_02_transactions.cpp -lsbclient
 * 
 * The contents of this file are subject to the Initial
 * Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 * http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 * 
 * Copyright (c) 2025 ScratchBird Development Team
 * All Rights Reserved.
 */

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>

#include "sb_interface.h"

using namespace ScratchBird;

class TransactionManager {
private:
    std::unique_ptr<Database> db;
    std::mutex output_mutex;
    
public:
    TransactionManager() {
        std::cout << "ScratchBird Interface Example 02: Transaction Management" << std::endl;
        std::cout << "=======================================================" << std::endl;
        std::cout << "Version: SB-T0.5.0.1 ScratchBird 0.5 f90eae0" << std::endl;
        std::cout << std::endl;
    }
    
    void run() {
        try {
            connectToDatabase();
            setupTestSchema();
            demonstrateBasicTransactions();
            demonstrateSavepoints();
            demonstrateIsolationLevels();
            demonstrateConcurrentTransactions();
            
            std::cout << "Transaction management examples completed successfully!" << std::endl;
            
        } catch (const ScratchBirdException& e) {
            std::cerr << "ScratchBird Error: " << e.what() << std::endl;
            throw;
        }
    }
    
private:
    void connectToDatabase() {
        std::cout << "Connecting to database..." << std::endl;
        
        ConnectionParams params;
        params.database = "localhost:interface_example.fdb";
        params.username = "SYSDBA";
        params.password = "masterkey";
        
        db = DatabaseFactory::connect(params);
        std::cout << "Connected successfully!" << std::endl;
    }
    
    void setupTestSchema() {
        std::cout << "Setting up test schema..." << std::endl;
        
        auto tx = db->startTransaction();
        
        // Create schema for transaction tests
        tx->executeImmediate("CREATE SCHEMA IF NOT EXISTS transaction_tests");
        tx->setSchema("transaction_tests");
        
        // Create test table
        std::string createTable = R"(
            CREATE TABLE IF NOT EXISTS account_balances (
                account_id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
                account_number VARCHAR(20) UNIQUE NOT NULL,
                balance DECIMAL(15,2) NOT NULL DEFAULT 0.00,
                last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                version_number INTEGER DEFAULT 1
            )
        )";
        
        tx->executeImmediate(createTable);
        
        // Insert test accounts
        std::string insertAccounts = R"(
            INSERT INTO account_balances (account_number, balance) VALUES
            ('ACC-001', 1000.00),
            ('ACC-002', 2000.00),
            ('ACC-003', 1500.00),
            ('ACC-004', 500.00)
        )";
        
        try {
            tx->executeImmediate(insertAccounts);
        } catch (const DuplicateKeyException& e) {
            // Accounts already exist, continue
        }
        
        tx->commit();
        std::cout << "Test schema setup completed!" << std::endl;
    }
    
    void demonstrateBasicTransactions() {
        std::cout << "\n=== Basic Transaction Operations ===" << std::endl;
        
        // Demonstrate successful transaction
        std::cout << "1. Successful transaction (transfer $100 from ACC-001 to ACC-002)" << std::endl;
        
        auto tx = db->startTransaction();
        tx->setSchema("transaction_tests");
        
        try {
            // Debit from source account
            auto stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance - 100.00, "
                "last_updated = CURRENT_TIMESTAMP WHERE account_number = 'ACC-001'"
            );
            stmt->execute();
            
            // Credit to destination account
            stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance + 100.00, "
                "last_updated = CURRENT_TIMESTAMP WHERE account_number = 'ACC-002'"
            );
            stmt->execute();
            
            tx->commit();
            std::cout << "   Transfer completed successfully!" << std::endl;
            
        } catch (const std::exception& e) {
            tx->rollback();
            std::cout << "   Transfer failed: " << e.what() << std::endl;
        }
        
        // Demonstrate failed transaction with rollback
        std::cout << "2. Failed transaction (insufficient funds)" << std::endl;
        
        tx = db->startTransaction();
        tx->setSchema("transaction_tests");
        
        try {
            // Try to debit more than available
            auto stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance - 2000.00 "
                "WHERE account_number = 'ACC-004'"
            );
            stmt->execute();
            
            // Check if balance would be negative
            stmt = tx->prepareStatement(
                "SELECT balance FROM account_balances WHERE account_number = 'ACC-004'"
            );
            auto result = stmt->executeQuery();
            
            if (result->next()) {
                auto balance = result->getDecimal("balance");
                if (balance < Decimal("0.00")) {
                    throw std::runtime_error("Insufficient funds");
                }
            }
            
            tx->commit();
            
        } catch (const std::exception& e) {
            tx->rollback();
            std::cout << "   Transaction rolled back: " << e.what() << std::endl;
        }
        
        displayBalances();
    }
    
    void demonstrateSavepoints() {
        std::cout << "\n=== Savepoint Operations ===" << std::endl;
        
        auto tx = db->startTransaction();
        tx->setSchema("transaction_tests");
        
        try {
            // Create savepoint before first operation
            tx->createSavepoint("before_first_transfer");
            
            // First transfer: ACC-001 -> ACC-002 ($50)
            std::cout << "1. First transfer: $50 from ACC-001 to ACC-002" << std::endl;
            
            auto stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance - 50.00 "
                "WHERE account_number = 'ACC-001'"
            );
            stmt->execute();
            
            stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance + 50.00 "
                "WHERE account_number = 'ACC-002'"
            );
            stmt->execute();
            
            // Create savepoint after first transfer
            tx->createSavepoint("after_first_transfer");
            
            // Second transfer: ACC-002 -> ACC-003 ($75)
            std::cout << "2. Second transfer: $75 from ACC-002 to ACC-003" << std::endl;
            
            stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance - 75.00 "
                "WHERE account_number = 'ACC-002'"
            );
            stmt->execute();
            
            stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance + 75.00 "
                "WHERE account_number = 'ACC-003'"
            );
            stmt->execute();
            
            // Simulate error and rollback to savepoint
            std::cout << "3. Simulating error - rolling back to after_first_transfer" << std::endl;
            tx->rollbackToSavepoint("after_first_transfer");
            
            // Third transfer: ACC-002 -> ACC-004 ($25)
            std::cout << "4. Third transfer: $25 from ACC-002 to ACC-004" << std::endl;
            
            stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance - 25.00 "
                "WHERE account_number = 'ACC-002'"
            );
            stmt->execute();
            
            stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance + 25.00 "
                "WHERE account_number = 'ACC-004'"
            );
            stmt->execute();
            
            tx->commit();
            std::cout << "   Savepoint operations completed!" << std::endl;
            
        } catch (const std::exception& e) {
            tx->rollback();
            std::cout << "   Savepoint operations failed: " << e.what() << std::endl;
        }
        
        displayBalances();
    }
    
    void demonstrateIsolationLevels() {
        std::cout << "\n=== Transaction Isolation Levels ===" << std::endl;
        
        // Read Committed (default)
        std::cout << "1. Read Committed isolation level" << std::endl;
        
        auto tx1 = db->startTransaction(IsolationLevel::ReadCommitted);
        auto tx2 = db->startTransaction(IsolationLevel::ReadCommitted);
        
        tx1->setSchema("transaction_tests");
        tx2->setSchema("transaction_tests");
        
        try {
            // TX1: Read initial balance
            auto stmt1 = tx1->prepareStatement(
                "SELECT balance FROM account_balances WHERE account_number = 'ACC-001'"
            );
            auto result1 = stmt1->executeQuery();
            result1->next();
            auto balance1 = result1->getDecimal("balance");
            std::cout << "   TX1: Initial balance for ACC-001: " << balance1.toString() << std::endl;
            
            // TX2: Update balance
            auto stmt2 = tx2->prepareStatement(
                "UPDATE account_balances SET balance = balance + 100.00 "
                "WHERE account_number = 'ACC-001'"
            );
            stmt2->execute();
            tx2->commit();
            std::cout << "   TX2: Added $100 to ACC-001 and committed" << std::endl;
            
            // TX1: Read balance again (should see the change)
            result1 = stmt1->executeQuery();
            result1->next();
            auto balance2 = result1->getDecimal("balance");
            std::cout << "   TX1: Balance after TX2 commit: " << balance2.toString() << std::endl;
            
            tx1->commit();
            
        } catch (const std::exception& e) {
            tx1->rollback();
            tx2->rollback();
            std::cout << "   Isolation level test failed: " << e.what() << std::endl;
        }
        
        // Serializable isolation
        std::cout << "2. Serializable isolation level" << std::endl;
        
        auto tx3 = db->startTransaction(IsolationLevel::Serializable);
        auto tx4 = db->startTransaction(IsolationLevel::Serializable);
        
        tx3->setSchema("transaction_tests");
        tx4->setSchema("transaction_tests");
        
        try {
            // TX3: Read balance
            auto stmt3 = tx3->prepareStatement(
                "SELECT balance FROM account_balances WHERE account_number = 'ACC-002'"
            );
            auto result3 = stmt3->executeQuery();
            result3->next();
            auto balance3 = result3->getDecimal("balance");
            std::cout << "   TX3: Balance for ACC-002: " << balance3.toString() << std::endl;
            
            // TX4: Try to update the same record
            auto stmt4 = tx4->prepareStatement(
                "UPDATE account_balances SET balance = balance + 50.00 "
                "WHERE account_number = 'ACC-002'"
            );
            stmt4->execute();
            
            // TX3: Try to update as well (should cause conflict)
            stmt3 = tx3->prepareStatement(
                "UPDATE account_balances SET balance = balance - 30.00 "
                "WHERE account_number = 'ACC-002'"
            );
            stmt3->execute();
            
            tx4->commit();
            tx3->commit();
            
        } catch (const SerializationException& e) {
            tx3->rollback();
            tx4->rollback();
            std::cout << "   Serialization conflict detected: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            tx3->rollback();
            tx4->rollback();
            std::cout << "   Serializable test failed: " << e.what() << std::endl;
        }
    }
    
    void demonstrateConcurrentTransactions() {
        std::cout << "\n=== Concurrent Transaction Handling ===" << std::endl;
        
        std::vector<std::thread> threads;
        
        // Start multiple concurrent transactions
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back([this, i]() {
                this->workerThread(i);
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Concurrent transactions completed!" << std::endl;
        displayBalances();
    }
    
    void workerThread(int threadId) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * threadId));
        
        auto tx = db->startTransaction();
        tx->setSchema("transaction_tests");
        
        try {
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "   Thread " << threadId << ": Starting transaction" << std::endl;
            }
            
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            // Perform account operation
            std::string sourceAccount = "ACC-00" + std::to_string(threadId + 1);
            std::string destAccount = "ACC-00" + std::to_string(((threadId + 1) % 4) + 1);
            
            auto stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance - 10.00 "
                "WHERE account_number = ?"
            );
            stmt->setString(1, sourceAccount);
            stmt->execute();
            
            stmt = tx->prepareStatement(
                "UPDATE account_balances SET balance = balance + 10.00 "
                "WHERE account_number = ?"
            );
            stmt->setString(1, destAccount);
            stmt->execute();
            
            // Simulate processing time
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            
            tx->commit();
            
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "   Thread " << threadId << ": Transaction committed ($10 from " 
                          << sourceAccount << " to " << destAccount << ")" << std::endl;
            }
            
        } catch (const std::exception& e) {
            tx->rollback();
            
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "   Thread " << threadId << ": Transaction failed: " << e.what() << std::endl;
            }
        }
    }
    
    void displayBalances() {
        std::cout << "\nCurrent Account Balances:" << std::endl;
        std::cout << "Account\t\tBalance\t\tLast Updated" << std::endl;
        std::cout << "-------\t\t-------\t\t------------" << std::endl;
        
        auto tx = db->startTransaction();
        tx->setSchema("transaction_tests");
        
        auto stmt = tx->prepareStatement(
            "SELECT account_number, balance, last_updated FROM account_balances ORDER BY account_number"
        );
        auto result = stmt->executeQuery();
        
        while (result->next()) {
            std::cout << result->getString("account_number") << "\t\t"
                      << result->getDecimal("balance").toString() << "\t\t"
                      << result->getTimestamp("last_updated").toString() << std::endl;
        }
        
        tx->commit();
        std::cout << std::endl;
    }
};

int main() {
    try {
        TransactionManager manager;
        manager.run();
        
        std::cout << "=======================================================" << std::endl;
        std::cout << "ScratchBird Interface Example 02 completed successfully!" << std::endl;
        std::cout << "Features demonstrated:" << std::endl;
        std::cout << "- Basic transaction operations (commit/rollback)" << std::endl;
        std::cout << "- Savepoint management" << std::endl;
        std::cout << "- Transaction isolation levels" << std::endl;
        std::cout << "- Concurrent transaction handling" << std::endl;
        std::cout << "- Schema-aware transaction scoping" << std::endl;
        std::cout << "=======================================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}