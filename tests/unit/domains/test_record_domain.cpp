#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include <iostream>
#include <cassert>
#include <cstdio>

using namespace scratchbird::core;

int main() {
    std::cout << "Testing RECORD Domain Implementation (Phase 2)...\n\n";

    // Create a test database
    const char* test_db = "test_record_domain.sbdb";
    std::remove(test_db);

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    if (status != Status::OK) {
        std::cerr << "Failed to create database: " << static_cast<int>(status) << "\n";
        return 1;
    }

    Database db;
    status = db.open(test_db, &ctx);
    if (status != Status::OK) {
        std::cerr << "Failed to open database: " << static_cast<int>(status) << "\n";
        return 1;
    }

    DomainManager* dm = db.domain_manager();
    CatalogManager* catalog = db.catalog_manager();
    assert(dm != nullptr);
    assert(catalog != nullptr);

    // Create schema
    ID schema_id;
    status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
    assert(status == Status::OK);

    // Test 1: Create RECORD domain
    std::cout << "Test 1: Create RECORD domain\n";
    {
        std::vector<RecordField> fields;
        fields.push_back(RecordField("id", DataType::INT32, false));
        fields.push_back(RecordField("name", DataType::VARCHAR, false));
        fields.push_back(RecordField("email", DataType::VARCHAR, true));

        ID person_domain_id;
        status = dm->createRecordDomain(schema_id, "Person", fields, person_domain_id, &ctx);
        assert(status == Status::OK);
        std::cout << "  Created Person RECORD domain with 3 fields ✓\n";
    }
    std::cout << "  ✓ Create RECORD domain passed\n\n";

    // Test 2: Get RECORD domain info
    std::cout << "Test 2: Get RECORD domain info\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "Person", info, &ctx);
        assert(status == Status::OK);
        assert(info.domain_type == DomainType::RECORD);
        assert(info.base_type == DataType::COMPOSITE);
        assert(info.fields.size() == 3);
        std::cout << "  Retrieved Person domain: " << info.fields.size() << " fields ✓\n";

        // Verify field details
        assert(info.fields[0].name == "id");
        assert(info.fields[0].type == DataType::INT32);
        assert(info.fields[0].nullable == false);

        assert(info.fields[1].name == "name");
        assert(info.fields[1].type == DataType::VARCHAR);
        assert(info.fields[1].nullable == false);

        assert(info.fields[2].name == "email");
        assert(info.fields[2].type == DataType::VARCHAR);
        assert(info.fields[2].nullable == true);

        std::cout << "  All fields verified ✓\n";
    }
    std::cout << "  ✓ Get RECORD domain info passed\n\n";

    // Test 3: Get specific field from RECORD domain
    std::cout << "Test 3: Get specific field\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "Person", info, &ctx);
        assert(status == Status::OK);

        RecordField field;
        status = dm->getRecordField(info.domain_id, "email", field, &ctx);
        assert(status == Status::OK);
        assert(field.name == "email");
        assert(field.type == DataType::VARCHAR);
        assert(field.nullable == true);
        std::cout << "  Retrieved 'email' field: VARCHAR, nullable=true ✓\n";

        // Try non-existent field
        status = dm->getRecordField(info.domain_id, "nonexistent", field, &ctx);
        assert(status == Status::NOT_FOUND);
        std::cout << "  Non-existent field rejected as expected ✓\n";
    }
    std::cout << "  ✓ Get specific field passed\n\n";

    // Test 4: Reject duplicate field names
    std::cout << "Test 4: Reject duplicate field names\n";
    {
        std::vector<RecordField> fields;
        fields.push_back(RecordField("id", DataType::INT32, false));
        fields.push_back(RecordField("id", DataType::INT64, false));  // Duplicate!

        ID dup_domain_id;
        status = dm->createRecordDomain(schema_id, "Invalid", fields, dup_domain_id, &ctx);
        assert(status == Status::INVALID_ARGUMENT);
        std::cout << "  Duplicate field name rejected ✓\n";
    }
    std::cout << "  ✓ Reject duplicate field names passed\n\n";

    // Test 5: Reject empty field list
    std::cout << "Test 5: Reject empty field list\n";
    {
        std::vector<RecordField> fields;  // Empty!

        ID empty_domain_id;
        status = dm->createRecordDomain(schema_id, "Empty", fields, empty_domain_id, &ctx);
        assert(status == Status::INVALID_ARGUMENT);
        std::cout << "  Empty field list rejected ✓\n";
    }
    std::cout << "  ✓ Reject empty field list passed\n\n";

    // Test 6: Complex RECORD domain
    std::cout << "Test 6: Complex RECORD domain with many fields\n";
    {
        std::vector<RecordField> fields;
        fields.push_back(RecordField("employee_id", DataType::INT32, false));
        fields.push_back(RecordField("first_name", DataType::VARCHAR, false));
        fields.push_back(RecordField("last_name", DataType::VARCHAR, false));
        fields.push_back(RecordField("email", DataType::VARCHAR, true));
        fields.push_back(RecordField("phone", DataType::VARCHAR, true));
        fields.push_back(RecordField("hire_date", DataType::DATE, false));
        fields.push_back(RecordField("salary", DataType::DECIMAL, false));
        fields.push_back(RecordField("is_active", DataType::BOOLEAN, false));

        ID employee_domain_id;
        status = dm->createRecordDomain(schema_id, "Employee", fields, employee_domain_id, &ctx);
        assert(status == Status::OK);

        // Verify all fields
        DomainInfo info;
        status = dm->getDomain(schema_id, "Employee", info, &ctx);
        assert(status == Status::OK);
        assert(info.fields.size() == 8);
        std::cout << "  Created Employee RECORD with 8 fields ✓\n";

        // Test field access for each field
        for (const auto& expected_field : fields) {
            RecordField field;
            status = dm->getRecordField(info.domain_id, expected_field.name, field, &ctx);
            assert(status == Status::OK);
            assert(field.name == expected_field.name);
            assert(field.type == expected_field.type);
            assert(field.nullable == expected_field.nullable);
        }
        std::cout << "  All 8 fields accessible ✓\n";
    }
    std::cout << "  ✓ Complex RECORD domain passed\n\n";

    // Test 7: List RECORD and BASIC domains together
    std::cout << "Test 7: List mixed domain types\n";
    {
        // Create a basic domain
        ID basic_domain_id;
        std::vector<DomainConstraint> constraints;
        status = dm->createBasicDomain(
            schema_id, "PositiveInt", DataType::INT32,
            0, 0, false, "0", constraints,
            basic_domain_id, &ctx
        );
        assert(status == Status::OK);

        // List all domains
        std::vector<DomainInfo> domains;
        status = dm->listDomains(schema_id, domains, &ctx);
        assert(status == Status::OK);
        assert(domains.size() >= 3);  // At least Person, Employee, PositiveInt

        int basic_count = 0;
        int record_count = 0;
        for (const auto& domain : domains) {
            if (domain.domain_type == DomainType::BASIC) {
                basic_count++;
            } else if (domain.domain_type == DomainType::RECORD) {
                record_count++;
            }
            std::cout << "    - " << domain.domain_name
                     << " (" << (domain.domain_type == DomainType::BASIC ? "BASIC" : "RECORD") << ")\n";
        }

        assert(basic_count >= 1);
        assert(record_count >= 2);
        std::cout << "  Found " << basic_count << " BASIC and " << record_count << " RECORD domains ✓\n";
    }
    std::cout << "  ✓ List mixed domain types passed\n\n";

    db.close();
    std::remove(test_db);

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "RECORD Domain Phase 2 is functional.\n";
    std::cout << "========================================\n";
    std::cout << "\nNote: Field extraction from RECORD values (extractField)\n";
    std::cout << "requires TypedValue extension and is planned for future enhancement.\n";

    return 0;
}
