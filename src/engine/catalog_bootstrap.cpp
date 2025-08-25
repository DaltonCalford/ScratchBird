#include "scratchbird/engine/catalog_bootstrap.h"

#include <sstream>

namespace scratchbird::engine
{

    static void emit(std::ostringstream& ss, const char* s)
    {
        ss << s << "\n";
    }

    std::string generate_catalog_bootstrap_sql(const BootstrapOptions& opts)
    {
        std::ostringstream ss;
        // Domains (subset)
        emit(ss, "CREATE DOMAIN SDB$OID AS BINARY(16) NOT NULL -- UUID bytes");
        emit(ss, "CREATE DOMAIN SDB$NAME AS VARCHAR(128) NOT NULL");
        emit(ss, "CREATE DOMAIN SDB$TEXT AS BLOB SUB_TYPE TEXT");
        emit(ss, "CREATE DOMAIN SDB$BOOL AS BOOLEAN");
        emit(ss, "CREATE DOMAIN SDB$JSON AS BLOB SUB_TYPE TEXT");
        emit(ss, "CREATE DOMAIN SDB$TIMESTAMPTZ AS TIMESTAMP WITH TIME ZONE");
        emit(ss, "CREATE DOMAIN SDB$INT AS INTEGER");
        emit(ss, "CREATE DOMAIN SDB$BIGINT AS BIGINT");
        emit(ss, "");
        // Core tables
        emit(ss, "CREATE TABLE SDB$OBJECT (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  type VARCHAR(24) NOT NULL,\n"
                 "  schema_oid SDB$OID,\n"
                 "  name SDB$NAME NOT NULL,\n"
                 "  owner_oid SDB$OID,\n"
                 "  created_at SDB$TIMESTAMPTZ,\n"
                 "  altered_at SDB$TIMESTAMPTZ,\n"
                 "  flags BIGINT DEFAULT 0,\n"
                 "  comment SDB$TEXT,\n"
                 "  source_hash CHAR(40)\n"
                 ")");
        emit(ss, "CREATE UNIQUE INDEX UX_SDB$OBJECT_NAME ON SDB$OBJECT(schema_oid, type, name)");
        emit(ss, "COMMENT ON TABLE SDB$OBJECT IS 'All catalog objects (UUID, type, name, owner, "
                 "timestamps, doc)'");
        emit(ss, "COMMENT ON COLUMN SDB$OBJECT.oid IS 'UUID object identifier' ");
        emit(ss, "COMMENT ON COLUMN SDB$OBJECT.type IS 'Object type' ");
        emit(ss, "COMMENT ON COLUMN SDB$OBJECT.schema_oid IS 'Owning schema UUID' ");
        emit(ss, "COMMENT ON COLUMN SDB$OBJECT.name IS 'Local object name' ");
        emit(ss, "COMMENT ON COLUMN SDB$OBJECT.owner_oid IS 'Owner UUID' ");
        emit(ss, "COMMENT ON COLUMN SDB$OBJECT.comment IS 'Documentation comment' ");
        emit(ss, "");
        emit(ss, "CREATE TABLE SDB$SCHEMA (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  parent_oid SDB$OID,\n"
                 "  name SDB$NAME NOT NULL,\n"
                 "  kind VARCHAR(16) NOT NULL,\n"
                 "  path_cache SDB$TEXT\n"
                 ")");
        emit(ss, "CREATE UNIQUE INDEX UX_SDB$SCHEMA_PARENT_NAME ON SDB$SCHEMA(parent_oid, name)");
        emit(ss, "COMMENT ON TABLE SDB$SCHEMA IS 'Schemas (recursive)'");
        emit(ss, "COMMENT ON COLUMN SDB$SCHEMA.parent_oid IS 'Parent schema UUID (nullable)'");
        emit(ss, "COMMENT ON COLUMN SDB$SCHEMA.kind IS 'SYSTEM | USER | REMOTE'");
        emit(ss, "");
        // Relations and columns (subset)
        emit(ss, "CREATE TABLE SDB$RELATION (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  kind VARCHAR(16) NOT NULL,\n"
                 "  persistence VARCHAR(16) DEFAULT 'PERMANENT',\n"
                 "  external_file SDB$TEXT,\n"
                 "  tablespace SDB$NAME,\n"
                 "  with_oids SDB$BOOL DEFAULT FALSE\n"
                 ")");
        emit(ss, "COMMENT ON TABLE SDB$RELATION IS 'Tables, views, foreign tables'");
        emit(ss, "");
        emit(ss, "CREATE TABLE SDB$COLUMN (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  relation_oid SDB$OID NOT NULL,\n"
                 "  position INTEGER NOT NULL,\n"
                 "  name SDB$NAME NOT NULL,\n"
                 "  domain_oid SDB$OID,\n"
                 "  inline_type SDB$JSON,\n"
                 "  default_expr SDB$TEXT,\n"
                 "  identity_kind VARCHAR(16) DEFAULT 'NONE',\n"
                 "  identity_options SDB$JSON,\n"
                 "  computed_expr SDB$TEXT,\n"
                 "  col_charset VARCHAR(64),\n"
                 "  col_collate VARCHAR(64),\n"
                 "  not_null SDB$BOOL DEFAULT FALSE\n"
                 ")");
        emit(ss, "CREATE UNIQUE INDEX UX_SDB$COLUMN_NAME ON SDB$COLUMN(relation_oid, name)");
        emit(ss, "");
        // Domains table
        emit(ss, "CREATE TABLE SDB$DOMAIN (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  base_type VARCHAR(64) NOT NULL,\n"
                 "  length INTEGER,\n"
                 "  precision INTEGER,\n"
                 "  scale INTEGER,\n"
                 "  charset VARCHAR(64),\n"
                 "  collate VARCHAR(64),\n"
                 "  not_null SDB$BOOL DEFAULT FALSE,\n"
                 "  default_expr SDB$TEXT,\n"
                 "  check_expr SDB$TEXT\n"
                 ")");
        emit(ss, "");
        // Index and keys
        emit(ss, "CREATE TABLE SDB$INDEX (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  relation_oid SDB$OID,\n"
                 "  unique SDB$BOOL DEFAULT FALSE,\n"
                 "  method VARCHAR(16) NOT NULL,\n"
                 "  where_expr SDB$TEXT,\n"
                 "  include_cols SDB$JSON,\n"
                 "  tablespace SDB$NAME\n"
                 ")");
        emit(ss, "CREATE TABLE SDB$INDEX_KEY (\n"
                 "  index_oid SDB$OID NOT NULL,\n"
                 "  position INTEGER NOT NULL,\n"
                 "  column_oid SDB$OID,\n"
                 "  expr SDB$TEXT,\n"
                 "  direction VARCHAR(8) DEFAULT 'ASC',\n"
                 "  collation VARCHAR(64),\n"
                 "  PRIMARY KEY(index_oid, position)\n"
                 ")");
        emit(ss, "");
        // Constraints
        emit(ss, "CREATE TABLE SDB$CONSTRAINT (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  relation_oid SDB$OID NOT NULL,\n"
                 "  type VARCHAR(8) NOT NULL,\n"
                 "  deferrable SDB$BOOL DEFAULT FALSE,\n"
                 "  initially_deferred SDB$BOOL DEFAULT FALSE,\n"
                 "  check_expr SDB$TEXT,\n"
                 "  index_oid SDB$OID,\n"
                 "  ref_relation_oid SDB$OID,\n"
                 "  on_delete VARCHAR(12) DEFAULT 'NO_ACTION',\n"
                 "  on_update VARCHAR(12) DEFAULT 'NO_ACTION'\n"
                 ")");
        emit(ss, "CREATE TABLE SDB$CONSTRAINT_KEY (\n"
                 "  constraint_oid SDB$OID NOT NULL,\n"
                 "  position INTEGER NOT NULL,\n"
                 "  column_oid SDB$OID NOT NULL,\n"
                 "  ref_column_oid SDB$OID,\n"
                 "  PRIMARY KEY(constraint_oid, position)\n"
                 ")");
        emit(ss, "");
        // Routines and params
        emit(ss, "CREATE TABLE SDB$ROUTINE (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  kind VARCHAR(16) NOT NULL,\n"
                 "  language VARCHAR(16) NOT NULL,\n"
                 "  security VARCHAR(16) DEFAULT 'INVOKER',\n"
                 "  volatility VARCHAR(16) DEFAULT 'VOLATILE',\n"
                 "  leakproof SDB$BOOL DEFAULT FALSE,\n"
                 "  returns_set SDB$BOOL DEFAULT FALSE\n"
                 ")");
        emit(ss, "CREATE TABLE SDB$ROUTINE_PARAM (\n"
                 "  routine_oid SDB$OID NOT NULL,\n"
                 "  position INTEGER NOT NULL,\n"
                 "  name SDB$NAME NOT NULL,\n"
                 "  mode VARCHAR(8) DEFAULT 'IN',\n"
                 "  domain_oid SDB$OID,\n"
                 "  inline_type SDB$JSON,\n"
                 "  PRIMARY KEY(routine_oid, position)\n"
                 ")");
        emit(ss, "");
        // Packages and members
        emit(ss, "CREATE TABLE SDB$PACKAGE (\n"
                 "  oid SDB$OID PRIMARY KEY\n"
                 ")");
        emit(ss, "CREATE TABLE SDB$PACKAGE_MEMBER (\n"
                 "  package_oid SDB$OID NOT NULL,\n"
                 "  member_name SDB$NAME NOT NULL,\n"
                 "  routine_oid SDB$OID NOT NULL,\n"
                 "  PRIMARY KEY(package_oid, member_name)\n"
                 ")");
        emit(ss, "");
        // Sequences
        emit(ss, "CREATE TABLE SDB$SEQUENCE (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  start_value NUMERIC(38),\n"
                 "  increment_by NUMERIC(38),\n"
                 "  min_value NUMERIC(38),\n"
                 "  max_value NUMERIC(38),\n"
                 "  cycle SDB$BOOL DEFAULT FALSE,\n"
                 "  cache INTEGER DEFAULT 1\n"
                 ")");
        emit(ss, "");
        // Triggers
        emit(ss, "CREATE TABLE SDB$TRIGGER (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  relation_oid SDB$OID,\n"
                 "  timing VARCHAR(8) NOT NULL,\n"
                 "  events SDB$JSON NOT NULL,\n"
                 "  position INTEGER DEFAULT 0,\n"
                 "  for_each VARCHAR(9) DEFAULT 'ROW',\n"
                 "  active SDB$BOOL DEFAULT TRUE,\n"
                 "  update_of SDB$JSON\n"
                 ")");
        emit(ss, "");
        // Exceptions
        emit(ss, "CREATE TABLE SDB$EXCEPTION (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  message SDB$TEXT NOT NULL\n"
                 ")");
        emit(ss, "");
        // Charsets & Collations
        emit(ss, "CREATE TABLE SDB$CHARSET (\n"
                 "  name VARCHAR(64) PRIMARY KEY,\n"
                 "  description SDB$TEXT\n"
                 ")");
        emit(ss, "CREATE TABLE SDB$COLLATION (\n"
                 "  name VARCHAR(64) PRIMARY KEY,\n"
                 "  base_charset VARCHAR(64) NOT NULL,\n"
                 "  deterministic SDB$BOOL DEFAULT TRUE\n"
                 ")");
        emit(ss, "");
        // Grants
        emit(ss, "CREATE TABLE SDB$GRANT (\n"
                 "  grantee_oid SDB$OID NOT NULL,\n"
                 "  object_oid SDB$OID NOT NULL,\n"
                 "  privilege VARCHAR(32) NOT NULL,\n"
                 "  grantor_oid SDB$OID NOT NULL,\n"
                 "  grant_option SDB$BOOL DEFAULT FALSE,\n"
                 "  columns SDB$JSON,\n"
                 "  PRIMARY KEY(grantee_oid, object_oid, privilege, columns)\n"
                 ")");
        emit(ss, "");
        // Dependencies
        emit(ss, "CREATE TABLE SDB$DEPENDENCY (\n"
                 "  from_oid SDB$OID NOT NULL,\n"
                 "  to_oid SDB$OID NOT NULL,\n"
                 "  kind VARCHAR(16) NOT NULL,\n"
                 "  detail_from SDB$JSON,\n"
                 "  detail_to SDB$JSON,\n"
                 "  PRIMARY KEY(from_oid, to_oid, kind)\n"
                 ")");
        emit(ss, "");
        // Source & Stats
        emit(ss, "CREATE TABLE SDB$SOURCE (\n"
                 "  object_oid SDB$OID PRIMARY KEY,\n"
                 "  text SDB$TEXT NOT NULL,\n"
                 "  doc SDB$TEXT\n"
                 ")");
        emit(ss, "CREATE TABLE SDB$STATS (\n"
                 "  object_oid SDB$OID PRIMARY KEY,\n"
                 "  stats SDB$JSON NOT NULL\n"
                 ")");
        emit(ss, "");
        // Foreign Data Wrapper (FDW) tables
        emit(ss, "CREATE TABLE SDB$FOREIGN_SERVER (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  server_name SDB$NAME NOT NULL,\n"
                 "  fdw_name VARCHAR(64) NOT NULL,\n"
                 "  host VARCHAR(255),\n"
                 "  port INTEGER,\n"
                 "  database_name VARCHAR(128),\n"
                 "  options SDB$JSON,\n"
                 "  use_ssl SDB$BOOL DEFAULT FALSE,\n"
                 "  ssl_cert_path VARCHAR(512),\n"
                 "  ssl_key_path VARCHAR(512),\n"
                 "  ssl_ca_path VARCHAR(512),\n"
                 "  created_at SDB$TIMESTAMPTZ,\n"
                 "  modified_at SDB$TIMESTAMPTZ\n"
                 ")");
        emit(ss,
             "CREATE UNIQUE INDEX UX_SDB$FOREIGN_SERVER_NAME ON SDB$FOREIGN_SERVER(server_name)");
        emit(ss, "COMMENT ON TABLE SDB$FOREIGN_SERVER IS 'Foreign servers for FDW connections'");
        emit(ss, "");
        emit(ss, "CREATE TABLE SDB$USER_MAPPING (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  server_name SDB$NAME NOT NULL,\n"
                 "  local_username SDB$NAME NOT NULL,\n"
                 "  remote_username VARCHAR(128),\n"
                 "  remote_password_hash VARCHAR(256),\n"
                 "  options SDB$JSON,\n"
                 "  created_at SDB$TIMESTAMPTZ,\n"
                 "  FOREIGN KEY (server_name) REFERENCES SDB$FOREIGN_SERVER(server_name)\n"
                 ")");
        emit(ss, "CREATE UNIQUE INDEX UX_SDB$USER_MAPPING ON SDB$USER_MAPPING(server_name, "
                 "local_username)");
        emit(ss,
             "COMMENT ON TABLE SDB$USER_MAPPING IS 'User credentials for foreign server access'");
        emit(ss, "");
        emit(ss, "CREATE TABLE SDB$FOREIGN_TABLE (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  table_name SDB$NAME NOT NULL,\n"
                 "  server_name SDB$NAME NOT NULL,\n"
                 "  remote_schema VARCHAR(128),\n"
                 "  remote_table SDB$NAME,\n"
                 "  options SDB$JSON,\n"
                 "  created_at SDB$TIMESTAMPTZ,\n"
                 "  FOREIGN KEY (server_name) REFERENCES SDB$FOREIGN_SERVER(server_name)\n"
                 ")");
        emit(ss, "CREATE UNIQUE INDEX UX_SDB$FOREIGN_TABLE_NAME ON SDB$FOREIGN_TABLE(table_name)");
        emit(ss, "COMMENT ON TABLE SDB$FOREIGN_TABLE IS 'Foreign table definitions'");
        emit(ss, "");
        emit(ss, "CREATE TABLE SDB$DATABASE_LINK (\n"
                 "  oid SDB$OID PRIMARY KEY,\n"
                 "  link_name SDB$NAME NOT NULL,\n"
                 "  host VARCHAR(255) NOT NULL,\n"
                 "  port INTEGER,\n"
                 "  database_name VARCHAR(128) NOT NULL,\n"
                 "  username VARCHAR(128),\n"
                 "  password_hash VARCHAR(256),\n"
                 "  use_ssl SDB$BOOL DEFAULT FALSE,\n"
                 "  connection_timeout INTEGER DEFAULT 30,\n"
                 "  query_timeout INTEGER DEFAULT 300,\n"
                 "  options SDB$JSON,\n"
                 "  created_at SDB$TIMESTAMPTZ,\n"
                 "  last_used_at SDB$TIMESTAMPTZ,\n"
                 "  status VARCHAR(16) DEFAULT 'ACTIVE'\n"
                 ")");
        emit(ss, "CREATE UNIQUE INDEX UX_SDB$DATABASE_LINK_NAME ON SDB$DATABASE_LINK(link_name)");
        emit(
            ss,
            "COMMENT ON TABLE SDB$DATABASE_LINK IS 'Database links for cross-database operations'");
        emit(ss, "");
        // Version tables
        emit(ss, "CREATE TABLE SDB$CATALOG_VERSION(major INTEGER, minor INTEGER, stamp "
                 "SDB$TIMESTAMPTZ)");
        emit(ss, "CREATE TABLE SDB$MIGRATIONS(id VARCHAR(64) PRIMARY KEY, applied_at "
                 "SDB$TIMESTAMPTZ, text_hash CHAR(40))");
        emit(ss, "");
        // Seeds (placeholders for UUIDs)
        emit(ss, "-- Seed schemas (UUIDs to be parameterized by engine)\n"
                 "INSERT INTO SDB$SCHEMA(oid,parent_oid,name,kind) VALUES "
                 "('<SYS_CATALOG_UUID>',NULL,'sys.catalog','SYSTEM');\n"
                 "INSERT INTO SDB$SCHEMA(oid,parent_oid,name,kind) VALUES "
                 "('<SYS_SECURITY_UUID>',NULL,'sys.security','SYSTEM');\n"
                 "INSERT INTO SDB$SCHEMA(oid,parent_oid,name,kind) VALUES "
                 "('<SYS_MONITORING_UUID>',NULL,'sys.monitoring','SYSTEM');\n"
                 "INSERT INTO SDB$SCHEMA(oid,parent_oid,name,kind) VALUES "
                 "('<PUBLIC_SCHEMA_UUID>',NULL,'public','USER');");
        emit(ss, "-- Seed users and roles\n"
                 "INSERT INTO SDB$USER(oid,name) VALUES ('<SYSDBA_UUID>','SYSDBA');\n"
                 "INSERT INTO SDB$ROLE(oid,name) VALUES ('<PUBLIC_ROLE_UUID>','PUBLIC');");
        emit(ss, "");
        // Catalog version
        ss << "INSERT INTO SDB$CATALOG_VERSION(major,minor,stamp) VALUES (" << opts.catalog_major
           << "," << opts.catalog_minor << ", CURRENT_TIMESTAMP);\n";
        emit(ss, "");
        // Compatibility views (minimal set)
        emit(ss, "-- Compatibility views (RDB$*)");
        emit(ss, "CREATE VIEW RDB$RELATIONS AS\n"
                 "SELECT\n"
                 "  o.name AS RDB$RELATION_NAME,\n"
                 "  CAST(NULL AS SMALLINT) AS RDB$SYSTEM_FLAG,\n"
                 "  r.external_file AS RDB$EXTERNAL_FILE,\n"
                 "  o.comment AS RDB$DESCRIPTION\n"
                 "FROM SDB$OBJECT o JOIN SDB$RELATION r ON r.oid=o.oid;");
        emit(ss, "CREATE VIEW RDB$FIELDS AS\n"
                 "SELECT\n"
                 "  d.oid AS RDB$FIELD_NAME,\n"
                 "  d.base_type AS RDB$FIELD_TYPE,\n"
                 "  d.length AS RDB$FIELD_LENGTH,\n"
                 "  d.precision AS RDB$FIELD_PRECISION,\n"
                 "  d.scale AS RDB$FIELD_SCALE\n"
                 "FROM SDB$DOMAIN d;");
        emit(ss, "");
        emit(ss, "-- Monitoring views (MON$*) stubs");
        emit(ss, "CREATE VIEW MON$ATTACHMENTS AS\n"
                 "SELECT\n"
                 "  CAST(NULL AS BIGINT) AS MON$ATTACHMENT_ID,\n"
                 "  CAST(NULL AS VARCHAR(31)) AS MON$USER\n"
                 "WHERE 1=0;");
        emit(ss, "");
        return ss.str();
    }

} // namespace scratchbird::engine
