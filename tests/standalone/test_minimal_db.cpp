#include "scratchbird/core/database.h"
#include <iostream>

using namespace scratchbird::core;

int main()
{
    std::cout << "Testing minimal database creation..." << std::endl;

    const char *db_path = "/tmp/test_minimal";
    std::remove(db_path);

    ErrorContext ctx;
    Database db;

    std::cout << "Calling Database::create..." << std::endl;
    Status status = Database::create(db_path, 8192, &ctx);

    std::cout << "Status: " << static_cast<int>(status) << std::endl;
    if (ctx.message[0] != '\0')
    {
        std::cout << "Error message: " << ctx.message << std::endl;
    }

    if (status != Status::OK)
    {
        std::cerr << "Failed to create database!" << std::endl;
        return 1;
    }

    std::cout << "Success!" << std::endl;
    return 0;
}
