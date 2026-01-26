#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/parser/firebird/firebird_parser.h"
#include "scratchbird/parser/mysql/mysql_parser.h"
#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "unit/test_user_helpers.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace core = scratchbird::core;
namespace fb = scratchbird::parser::firebird;
namespace mysql = scratchbird::parser::mysql;
namespace pg = scratchbird::parser::postgresql;
namespace sblr = scratchbird::sblr;
namespace v2 = scratchbird::parser::v2;

namespace {

struct ParsedPath {
    core::PathType type = core::PathType::UNQUALIFIED;
    bool no_search_path = false;
    std::vector<std::string> components;
};

struct ParsedRename {
    uint8_t flags = 0;
    core::CatalogManager::ObjectType object_type = core::CatalogManager::ObjectType::UNKNOWN;
    ParsedPath path;
    std::string new_name;
};

struct ParsedMove {
    uint8_t flags = 0;
    core::CatalogManager::ObjectType object_type = core::CatalogManager::ObjectType::UNKNOWN;
    ParsedPath object_path;
    ParsedPath target_schema;
    std::string new_name;
};

std::string normalizeName(const std::string& name) {
    return core::IdentifierUtils::toUpper(name);
}

bool readU16(const std::vector<uint8_t>& bytecode, size_t* offset, uint16_t* out) {
    if (*offset + 2 > bytecode.size()) {
        return false;
    }
    *out = sblr::readInt16(&bytecode[*offset]);
    *offset += 2;
    return true;
}

bool readString16(const std::vector<uint8_t>& bytecode, size_t* offset, std::string* out) {
    uint16_t length = 0;
    if (!readU16(bytecode, offset, &length)) {
        return false;
    }
    if (*offset + length > bytecode.size()) {
        return false;
    }
    out->assign(reinterpret_cast<const char*>(&bytecode[*offset]), length);
    *offset += length;
    return true;
}

bool readUVarint(const std::vector<uint8_t>& bytecode, size_t* offset, uint64_t* out) {
    if (*offset >= bytecode.size()) {
        return false;
    }
    size_t bytes_read = 0;
    if (!sblr::readUVarint(&bytecode[*offset], bytecode.size() - *offset, *out, bytes_read)) {
        return false;
    }
    *offset += bytes_read;
    return true;
}

bool readStringVarint(const std::vector<uint8_t>& bytecode, size_t* offset, std::string* out) {
    uint64_t length = 0;
    if (!readUVarint(bytecode, offset, &length)) {
        return false;
    }
    if (*offset + length > bytecode.size()) {
        return false;
    }
    out->assign(reinterpret_cast<const char*>(&bytecode[*offset]), length);
    *offset += length;
    return true;
}

bool readPath(const std::vector<uint8_t>& bytecode, size_t* offset, ParsedPath* out) {
    if (*offset + 3 > bytecode.size()) {
        return false;
    }
    out->type = static_cast<core::PathType>(bytecode[*offset]);
    *offset += 1;
    out->no_search_path = bytecode[*offset] != 0;
    *offset += 1;
    uint8_t count = bytecode[*offset];
    *offset += 1;
    out->components.clear();
    out->components.reserve(count);
    for (uint8_t i = 0; i < count; ++i) {
        std::string component;
        if (!readString16(bytecode, offset, &component)) {
            return false;
        }
        out->components.push_back(std::move(component));
    }
    return true;
}

bool readExtendedHeader(const std::vector<uint8_t>& bytecode,
                        sblr::ExtendedOpcode expected,
                        size_t* offset) {
    if (bytecode.size() < 5) {
        return false;
    }
    if (bytecode[0] != static_cast<uint8_t>(sblr::Opcode::VERSION)) {
        return false;
    }
    if (bytecode[1] != static_cast<uint8_t>(sblr::SBLR_VERSION)) {
        return false;
    }
    size_t pos = 2;
    if (bytecode[pos] == static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE)) {
        if (pos + 2 >= bytecode.size()) {
            return false;
        }
        uint16_t ext = sblr::readInt16(&bytecode[pos + 1]);
        if (ext == static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DEBUG_SPAN)) {
            pos += 1 + 2 + 4 + 4;
        }
    }
    if (pos + 2 >= bytecode.size()) {
        return false;
    }
    if (bytecode[pos] != static_cast<uint8_t>(sblr::Opcode::EXTENDED_OPCODE)) {
        return false;
    }
    uint16_t opcode = sblr::readInt16(&bytecode[pos + 1]);
    if (opcode != static_cast<uint16_t>(expected)) {
        return false;
    }
    *offset = pos + 3;
    return true;
}

bool readRenamePayload(const std::vector<uint8_t>& bytecode, size_t offset, ParsedRename* out) {
    if (offset + 2 > bytecode.size()) {
        return false;
    }
    out->flags = bytecode[offset++];
    out->object_type = static_cast<core::CatalogManager::ObjectType>(bytecode[offset++]);

    // If has_uuid flag is set, skip the UUID (16 bytes)
    if (out->flags & 0x01) {
        if (offset + 16 > bytecode.size()) {
            return false;
        }
        offset += 16;  // Skip UUID
    }

    if (!readPath(bytecode, &offset, &out->path)) {
        return false;
    }
    return readString16(bytecode, &offset, &out->new_name);
}

bool readMovePayload(const std::vector<uint8_t>& bytecode, size_t offset, ParsedMove* out) {
    if (offset + 2 > bytecode.size()) {
        return false;
    }
    out->flags = bytecode[offset++];
    out->object_type = static_cast<core::CatalogManager::ObjectType>(bytecode[offset++]);

    // If has_uuid flag is set, skip the UUID (16 bytes)
    if (out->flags & 0x01) {
        if (offset + 16 > bytecode.size()) {
            return false;
        }
        offset += 16;  // Skip UUID
    }

    if (!readPath(bytecode, &offset, &out->object_path)) {
        return false;
    }
    if (!readPath(bytecode, &offset, &out->target_schema)) {
        return false;
    }
    return readString16(bytecode, &offset, &out->new_name);
}

bool containsUpperToken(const std::string& value, const std::string& token_upper) {
    auto normalized = normalizeName(value);
    return normalized.find(token_upper) != std::string::npos;
}

}  // namespace

class RenameMoveOpcodeDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = "/tmp/test_rename_move_opcodes.db";
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");

        core::ErrorContext ctx;
        ASSERT_EQ(core::Database::create(test_db_path_, 16384, &ctx), core::Status::OK)
            << ctx.message;
        ASSERT_EQ(db_.open(test_db_path_, &ctx), core::Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        EnsureUser(catalog_, "test_user");
        ASSERT_EQ(catalog_->createSchema("test", "test_user", test_schema_id_, &ctx),
                  core::Status::OK)
            << ctx.message;

        ASSERT_EQ(db_.connect(conn_, &ctx), core::Status::OK) << ctx.message;
        core::ConnectionContext::setCurrent(conn_.get());

        // Set current user to test_user so schema resolution works
        core::CatalogManager::UserInfo user_info;
        ASSERT_EQ(catalog_->getUserByName("test_user", user_info, &ctx), core::Status::OK)
            << ctx.message;
        conn_->setCurrentUser(user_info.user_id, false);

        conn_->setCurrentSchemaId(test_schema_id_);
    }

    void TearDown() override {
        core::ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    std::vector<uint8_t> compileV2(const std::string& sql) {
        sblr::QueryCompilerV2 compiler(&db_);
        compiler.setCurrentSchema(test_schema_id_);
        auto result = compiler.compile(sql);
        EXPECT_TRUE(result.success()) << "Compilation failed for: " << sql;
        return result.bytecode();
    }

    std::vector<uint8_t> compileFirebird(const std::string& sql) {
        fb::Parser parser(sql);
        auto parse_result = parser.parseStatement();
        EXPECT_TRUE(parse_result.success) << "Firebird parse failed for: " << sql;
        if (!parse_result.success || !parse_result.statement) {
            return {};
        }

        v2::SemanticAnalyzerV2 analyzer(*catalog_, parser.stringPool());
        analyzer.setCurrentSchema(test_schema_id_);
        auto sem_result = analyzer.analyze(parse_result.statement.get());
        if (!sem_result.success()) {
            std::ostringstream oss;
            oss << "Semantic analysis failed for: " << sql << "\nErrors: ";
            for (const auto& err : sem_result.errors()) {
                oss << err.message << "; ";
            }
            EXPECT_TRUE(false) << oss.str();
            return {};
        }

        v2::BytecodeGeneratorV2 generator(parser.stringPool());
        auto bc_result = generator.generate(sem_result.statement());
        EXPECT_TRUE(bc_result.success()) << "Bytecode generation failed for: " << sql;
        return bc_result.bytecode();
    }

    core::ID createTable(const std::string& name, const std::vector<std::string>& columns) {
        core::ErrorContext ctx;
        std::vector<core::CatalogManager::ColumnInfo> cols;
        cols.reserve(columns.size());
        for (const auto& col_name : columns) {
            core::CatalogManager::ColumnInfo col;
            col.column_name = col_name;
            col.data_type = static_cast<uint16_t>(core::DataType::INT32);
            col.max_length = 4;
            col.nullable = false;
            cols.push_back(col);
        }

        core::ID table_id;
        auto status = catalog_->createTable(test_schema_id_, name, cols, table_id, 0, &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        return table_id;
    }

    core::ID createDomain(const std::string& name) {
        core::DomainManager* dm = db_.domain_manager();
        EXPECT_NE(dm, nullptr);
        if (!dm) {
            return {};
        }

        core::ErrorContext ctx;
        core::ID domain_id;
        std::vector<core::DomainConstraint> constraints;
        auto status = dm->createBasicDomain(
            test_schema_id_,
            name,
            core::DataType::INT32,
            0,
            0,
            true,
            "",
            constraints,
            domain_id,
            &ctx);
        EXPECT_EQ(status, core::Status::OK) << ctx.message;
        return domain_id;
    }

    std::string test_db_path_;
    core::Database db_;
    core::CatalogManager* catalog_ = nullptr;
    core::ID test_schema_id_;
    std::unique_ptr<core::ConnectionContext> conn_;
};

TEST(PostgreSQLOpcodeTest, RenameTableEmitsExtendedOpcode) {
    pg::Parser parser("ALTER TABLE IF EXISTS foo RENAME TO bar");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());

    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(result.bytecode(), sblr::ExtendedOpcode::EXT_RENAME_OBJECT, &offset));

    ParsedRename rename;
    ASSERT_TRUE(readRenamePayload(result.bytecode(), offset, &rename));
    EXPECT_TRUE(rename.flags & 0x02);
    EXPECT_FALSE(rename.flags & 0x01);
    EXPECT_EQ(rename.object_type, core::CatalogManager::ObjectType::TABLE);
    ASSERT_FALSE(rename.path.components.empty());
    EXPECT_EQ(normalizeName(rename.path.components.back()), "FOO");
    EXPECT_EQ(normalizeName(rename.new_name), "BAR");
}

TEST(PostgreSQLOpcodeTest, MoveTableEmitsExtendedOpcode) {
    pg::Parser parser("ALTER TABLE IF EXISTS foo SET SCHEMA app");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());

    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(result.bytecode(), sblr::ExtendedOpcode::EXT_MOVE_OBJECT, &offset));

    ParsedMove move;
    ASSERT_TRUE(readMovePayload(result.bytecode(), offset, &move));
    EXPECT_TRUE(move.flags & 0x02);
    EXPECT_FALSE(move.flags & 0x01);
    EXPECT_EQ(move.object_type, core::CatalogManager::ObjectType::TABLE);
    ASSERT_FALSE(move.object_path.components.empty());
    EXPECT_EQ(normalizeName(move.object_path.components.back()), "FOO");
    ASSERT_FALSE(move.target_schema.components.empty());
    EXPECT_TRUE(containsUpperToken(move.target_schema.components.back(), "APP"));
    EXPECT_TRUE(move.new_name.empty());
}

TEST(MySQLOpcodeTest, RenameTableEmitsExtendedOpcode) {
    mysql::Parser parser("RENAME TABLE foo TO bar");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());

    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(result.bytecode(), sblr::ExtendedOpcode::EXT_RENAME_OBJECT, &offset));

    ParsedRename rename;
    ASSERT_TRUE(readRenamePayload(result.bytecode(), offset, &rename));
    EXPECT_EQ(rename.flags, 0);
    EXPECT_EQ(rename.object_type, core::CatalogManager::ObjectType::TABLE);
    ASSERT_FALSE(rename.path.components.empty());
    EXPECT_EQ(normalizeName(rename.path.components.back()), "FOO");
    EXPECT_EQ(normalizeName(rename.new_name), "BAR");
}

TEST(MySQLOpcodeTest, MoveTableEmitsExtendedOpcode) {
    mysql::Parser parser("RENAME TABLE foo TO app.bar");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());

    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(result.bytecode(), sblr::ExtendedOpcode::EXT_MOVE_OBJECT, &offset));

    ParsedMove move;
    ASSERT_TRUE(readMovePayload(result.bytecode(), offset, &move));
    EXPECT_EQ(move.flags, 0);
    EXPECT_EQ(move.object_type, core::CatalogManager::ObjectType::TABLE);
    ASSERT_FALSE(move.object_path.components.empty());
    EXPECT_EQ(normalizeName(move.object_path.components.back()), "FOO");
    ASSERT_FALSE(move.target_schema.components.empty());
    EXPECT_TRUE(containsUpperToken(move.target_schema.components.back(), "APP"));
    EXPECT_EQ(normalizeName(move.new_name), "BAR");
}

TEST_F(RenameMoveOpcodeDbTest, V2RenameTableEmitsExtendedOpcode) {
    auto bytecode = compileV2("ALTER TABLE IF EXISTS foo RENAME TO bar");
    ASSERT_FALSE(bytecode.empty());

    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(bytecode, sblr::ExtendedOpcode::EXT_RENAME_OBJECT, &offset));

    ParsedRename rename;
    ASSERT_TRUE(readRenamePayload(bytecode, offset, &rename));
    EXPECT_TRUE(rename.flags & 0x02);
    EXPECT_FALSE(rename.flags & 0x01);
    EXPECT_EQ(rename.object_type, core::CatalogManager::ObjectType::TABLE);
    ASSERT_FALSE(rename.path.components.empty());
    EXPECT_EQ(normalizeName(rename.path.components.back()), "FOO");
    EXPECT_EQ(normalizeName(rename.new_name), "BAR");
}

TEST_F(RenameMoveOpcodeDbTest, V2MoveTableEmitsExtendedOpcode) {
    auto bytecode = compileV2("ALTER TABLE IF EXISTS foo SET SCHEMA app");
    ASSERT_FALSE(bytecode.empty());

    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(bytecode, sblr::ExtendedOpcode::EXT_MOVE_OBJECT, &offset));

    ParsedMove move;
    ASSERT_TRUE(readMovePayload(bytecode, offset, &move));
    EXPECT_TRUE(move.flags & 0x02);
    EXPECT_FALSE(move.flags & 0x01);
    EXPECT_EQ(move.object_type, core::CatalogManager::ObjectType::TABLE);
    ASSERT_FALSE(move.object_path.components.empty());
    EXPECT_EQ(normalizeName(move.object_path.components.back()), "FOO");
    ASSERT_FALSE(move.target_schema.components.empty());
    EXPECT_EQ(normalizeName(move.target_schema.components.back()), "APP");
    EXPECT_TRUE(move.new_name.empty());
}

TEST_F(RenameMoveOpcodeDbTest, FirebirdRenameColumnEmitsExtendedOpcode) {
    createTable("foo", {"bar"});
    // Use unqualified name since current schema is already set to "test"
    auto bytecode = compileFirebird("ALTER TABLE foo ALTER COLUMN bar TO baz");
    ASSERT_FALSE(bytecode.empty()) << "Bytecode is empty - compilation failed";

    // Debug: print bytecode size and first few bytes
    std::cout << "Bytecode size: " << bytecode.size() << "\n";
    std::cout << "First 20 bytes: ";
    for (size_t i = 0; i < std::min(size_t(20), bytecode.size()); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytecode[i]) << " ";
    }
    std::cout << std::dec << "\n";

    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(bytecode, sblr::ExtendedOpcode::EXT_RENAME_OBJECT, &offset));

    std::cout << "Offset after readExtendedHeader: " << offset << "\n";
    std::cout << "Payload bytes (offset " << offset << " to " << offset + 20 << "): ";
    for (size_t i = offset; i < std::min(offset + 20, bytecode.size()); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytecode[i]) << " ";
    }
    std::cout << std::dec << "\n";

    ParsedRename rename;
    bool read_success = readRenamePayload(bytecode, offset, &rename);
    std::cout << "readRenamePayload returned: " << (read_success ? "true" : "false") << "\n";
    if (read_success) {
        std::cout << "flags=" << static_cast<int>(rename.flags) << " object_type=" << static_cast<int>(rename.object_type) << "\n";
    }
    ASSERT_TRUE(read_success);
    EXPECT_TRUE(rename.flags & 0x01);
    EXPECT_FALSE(rename.flags & 0x02);
    EXPECT_EQ(rename.object_type, core::CatalogManager::ObjectType::COLUMN);
    ASSERT_FALSE(rename.path.components.empty());
    EXPECT_EQ(normalizeName(rename.path.components.back()), "BAR");
    EXPECT_EQ(normalizeName(rename.new_name), "BAZ");
}

TEST_F(RenameMoveOpcodeDbTest, FirebirdRenameDomainEmitsExtendedOpcode) {
    // Firebird uppercases unquoted identifiers, so create domain with uppercase name
    createDomain("MY_DOMAIN");

    // Verify domain can be found
    core::DomainInfo dinfo;
    core::ErrorContext verify_ctx;
    auto verify_status = catalog_->getDomainByName(test_schema_id_, "MY_DOMAIN", dinfo, &verify_ctx);
    std::cout << "getDomainByName verification: " << (verify_status == core::Status::OK ? "OK" : "FAILED") << "\n";
    if (verify_status != core::Status::OK) {
        std::cout << "Verification error: " << verify_ctx.message << "\n";
    }

    auto bytecode = compileFirebird("ALTER DOMAIN my_domain TO new_domain");
    ASSERT_FALSE(bytecode.empty());

    size_t offset = 0;
    ASSERT_TRUE(readExtendedHeader(bytecode, sblr::ExtendedOpcode::EXT_ALTER_DOMAIN, &offset));

    if (offset >= bytecode.size()) {
        FAIL() << "Unexpected end of bytecode while reading ALTER DOMAIN payload";
    }
    uint8_t action = bytecode[offset++];
    EXPECT_EQ(action, static_cast<uint8_t>(sblr::AlterDomainAction::RENAME));

    std::string domain_path;
    ASSERT_TRUE(readStringVarint(bytecode, &offset, &domain_path));
    std::string new_name;
    ASSERT_TRUE(readStringVarint(bytecode, &offset, &new_name));

    std::string domain_last = domain_path;
    auto dot_pos = domain_last.find_last_of('.');
    if (dot_pos != std::string::npos) {
        domain_last = domain_last.substr(dot_pos + 1);
    }
    EXPECT_EQ(normalizeName(domain_last), "MY_DOMAIN");
    EXPECT_EQ(normalizeName(new_name), "NEW_DOMAIN");
}
