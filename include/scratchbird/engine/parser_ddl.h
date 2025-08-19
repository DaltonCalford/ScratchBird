// SPDX-License-Identifier: IDPL
#ifndef SCRATCHBIRD_ENGINE_PARSER_DDL_H
#define SCRATCHBIRD_ENGINE_PARSER_DDL_H

#include "scratchbird/engine/ast.h"

#include <string>

namespace scratchbird::engine
{
    Ast parse_ddl_table(const std::string& sql);
    Ast parse_ddl_index(const std::string& sql);
    Ast parse_ddl_sequence(const std::string& sql);
    Ast parse_ddl_domain(const std::string& sql);
    Ast parse_ddl_view(const std::string& sql);
    Ast parse_ddl_collation(const std::string& sql);
    Ast parse_ddl_charset(const std::string& sql);
    Ast parse_ddl_exception(const std::string& sql);
    Ast parse_ddl_comment(const std::string& sql);
    Ast parse_ddl_rename(const std::string& sql);
    Ast parse_ddl_move(const std::string& sql);
    Ast parse_ddl_role(const std::string& sql);
    Ast parse_ddl_user(const std::string& sql);
    Ast parse_grant_stmt(const std::string& sql);
    Ast parse_revoke_stmt(const std::string& sql);
    Ast parse_ddl_udf(const std::string& sql);
    Ast parse_ddl_udr(const std::string& sql);
    Ast parse_ddl_blob_filter(const std::string& sql);
    Ast parse_ddl_mapping(const std::string& sql);
    Ast parse_ddl_gtt(const std::string& sql);
    Ast parse_ddl_schema(const std::string& sql);
    Ast parse_ddl_dblink(const std::string& sql);
    Ast parse_ddl_tablespace(const std::string& sql);
    Ast parse_ddl_foreign_server(const std::string& sql);
    Ast parse_ddl_user_mapping(const std::string& sql);
    Ast parse_ddl_foreign_table(const std::string& sql);
    Ast parse_ddl_import_foreign_schema(const std::string& sql);
    Ast parse_ddl_publication(const std::string& sql);
    Ast parse_ddl_subscription(const std::string& sql);
    Ast parse_ddl_trace_profile(const std::string& sql);
    Ast parse_ddl_audit_policy(const std::string& sql);
    Ast parse_ddl_cluster(const std::string& sql);
    Ast parse_ddl_cluster_node(const std::string& sql);
    Ast parse_ddl_cluster_service(const std::string& sql);
    Ast parse_ddl_auth_provider(const std::string& sql);
    Ast parse_ddl_rls_policy(const std::string& sql);
    Ast parse_ddl_materialized_view(const std::string& sql);
} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_PARSER_DDL_H
