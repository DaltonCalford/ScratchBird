#ifndef SCRATCHBIRD_ENGINE_AST_H
#define SCRATCHBIRD_ENGINE_AST_H

#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird
{
    namespace engine
    {

        enum class NodeKind {
            Unknown,
            SelectLiteral,
            SessionStmt,
            DdlTable,
            DdlIndex,
            DdlSchema,
            DdlTablespace,
            DdlDbLink,
            DdlForeignServer,
            DdlUserMapping,
            DdlForeignTable,
            DdlImportForeignSchema,
            DdlPublication,
            DdlSubscription,
            DdlTraceProfile,
            DdlAuditPolicy,
            DdlCluster,
            DdlClusterNode,
            DdlClusterService,
            DdlAuthProvider,
            DdlRlsPolicy,
            DdlMaterializedView,
            DdlSequence,
            DdlDomain,
            DdlView,
            DdlCollation,
            DdlCharset,
            DdlException,
            DdlComment,
            DdlRename,
            DdlMove,
            DdlRole,
            DdlUser,
            DdlGrant,
            DdlRevoke,
            PsqlBlock,
            PsqlExecStmt,
            PsqlRoutine,
            PsqlTrigger,
            PsqlPackage,
            PsqlCall,
            DdlUdf,
            DdlUdr,
            DdlBlobFilter,
            DdlMapping,
            DdlGtt,
            DdlExplain,
            DdlAnalyzeVacuum
        };

        struct SourceSpan {
            int start{0};
            int end{0};
        };

        enum class SessionKind {
            CreateDb,
            AlterDb,
            DropDb,
            Connect,
            Disconnect,
            SetNames,
            SetRole,
            SetDialect,
            SetTxn,
            SetOption,
            Commit,
            Rollback,
            Savepoint,
            Release
        };

        struct SessionStmtAst {
            SessionKind kind{SessionKind::Connect};
            std::string a; // db name / role / names / dialect / savepoint / conn string
            std::string b; // optional extra (e.g., user) if we later need
            struct DbOptionsRaw {
                std::string page_size;
                std::string default_charset;
                std::string page_cache;
                std::string sweep_interval;
                std::string reserve_space;
                std::vector<std::string> files;   // secondary file specs
                std::vector<std::string> shadows; // shadow specs
                std::string dialect;              // numeric
            } dbopts;
            struct SetOptionsRaw {
                std::string lock_timeout;     // WAIT n | NO WAIT | LOCK TIMEOUT n
                std::string stats;            // e.g., STATISTICS/PLAN/TIMING ON|OFF
                std::string isolation;        // READ COMMITTED | SNAPSHOT
                std::string access;           // READ ONLY | READ WRITE
                std::string wait;             // WAIT | NO WAIT
                std::string read_consistency; // RECORD/LOCK resolution hints raw
                bool snapshot_table_stability{false};
                std::vector<std::pair<std::string, std::string>>
                    table_reservations; // (table, mode raw)
                // New options
                std::string time_zone;      // raw TZ value
                std::string bind;           // raw bind config
                std::string optimize;       // raw optimize config
                std::string search_path;    // raw search path
                std::string debug_option;   // raw debug option
                std::string decfloat_round; // raw
                std::string decfloat_traps; // raw
                bool session_reset{false};
            } setopts;
            SourceSpan span{};
        };

        struct Ast {
            NodeKind kind{NodeKind::Unknown};
            std::int64_t literal_value{0};
            SessionStmtAst session; // valid if kind == SessionStmt
            std::vector<std::string> warnings;
            std::vector<SourceSpan> warning_spans;
            std::vector<std::string> errors;
            // DDL payloads (minimal fields; valid per matching kind)
            struct {
                std::string name;
                std::string columns_raw;     // raw column list
                std::string constraints_raw; // raw table constraints/options
                std::string column_defs_raw; // split-out column defs
                std::string table_attrs_raw; // trailing attrs after )
                struct AlterOp {
                    std::string kind;   // ADD/DROP/ALTER [COLUMN|CONSTRAINT]
                    std::string target; // column/constraint name when available
                    std::string raw;    // raw operation text
                };
                std::vector<AlterOp> alter_ops; // for ALTER TABLE
                std::vector<std::pair<std::string, std::string>>
                    computed_by; // per-column normalized COMPUTED BY expressions
                std::vector<std::pair<std::string, std::string>>
                    column_charsets; // column -> CHARACTER SET
                std::vector<std::pair<std::string, std::string>>
                    column_collates;                       // column -> COLLATE
                std::vector<std::string> not_null_columns; // columns declared NOT NULL
                std::vector<std::pair<std::string, std::string>>
                    identity_columns; // column -> ALWAYS|BY DEFAULT
                std::vector<std::pair<std::string, std::string>>
                    identity_options; // column -> options inside AS IDENTITY(...)
                struct IdentityOptionsParsed {
                    long long start_with{0};
                    long long increment_by{0};
                    bool cycle{false};
                    std::string raw; // original options text
                };
                std::vector<std::pair<std::string, IdentityOptionsParsed>>
                    identity_options_parsed; // column -> parsed options
                struct TableConstraint {
                    std::string kind;                 // PRIMARY KEY | UNIQUE | CHECK | FOREIGN KEY
                    std::string name;                 // optional name after CONSTRAINT
                    std::vector<std::string> columns; // column list
                    std::string ref_table;            // for FOREIGN KEY
                    std::vector<std::string> ref_columns; // for FOREIGN KEY
                    std::string check_expr;               // for CHECK
                    std::string
                        on_delete; // CASCADE | SET NULL | SET DEFAULT | NO ACTION | RESTRICT
                    std::string on_update; // same variants
                    bool deferrable{false};
                    bool initially_deferred{false};
                };
                std::vector<TableConstraint> table_constraints;
                std::string external_file; // EXTERNAL FILE path if present
                SourceSpan span{};
            } ddlTable;
            struct {
                std::string name;
                std::string on_table;
                bool unique{false};
                std::string expr_raw;             // expression index or column list raw
                std::string options;              // ASC/DESC, ACTIVE/INACTIVE
                std::vector<std::string> columns; // parsed columns inside (...)
                bool rebuild{false};              // ALTER INDEX ... REBUILD
                std::string statistics;           // SET STATISTICS value raw
                std::string method;               // USING method or PARTIAL HASH
                std::string where_raw;            // partial index WHERE condition raw
                std::vector<std::pair<std::string, std::string>>
                    column_directions; // (column/expression, ASC|DESC|"")
                std::vector<std::pair<std::string, std::string>>
                    column_collates; // (column/expression, COLLATE name)
                std::string action;  // CREATE | ALTER | DROP | REINDEX | VALIDATE
                SourceSpan span{};
            } ddlIndex;
            struct {
                std::string name;
                std::string attrs; // raw attributes
                SourceSpan span{};
            } ddlSchema;
            struct {
                std::string action; // CREATE|ALTER|DROP|DETACH|ATTACH
                std::string name;
                std::string attrs; // LOCATION/ADD FILE/SET/KEEP FILES/OPTIONS tail
                SourceSpan span{};
            } ddlTablespace;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options; // raw options
                SourceSpan span{};
            } ddlForeignServer;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string user_name;
                std::string server_name;
                std::string options; // raw options
                SourceSpan span{};
            } ddlUserMapping;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string server_name;
                std::string columns_raw;
                std::string options; // raw options
                SourceSpan span{};
            } ddlForeignTable;
            struct {
                std::string remote_schema;
                std::string server_name;
                std::string into_schema;
                std::string options; // raw options
                SourceSpan span{};
            } ddlImportForeignSchema;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options; // raw
                SourceSpan span{};
            } ddlPublication;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options; // raw
                SourceSpan span{};
            } ddlSubscription;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options; // raw
                SourceSpan span{};
            } ddlTraceProfile;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options; // raw
                SourceSpan span{};
            } ddlAuditPolicy;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options;
                SourceSpan span{};
            } ddlCluster;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options;
                SourceSpan span{};
            } ddlClusterNode;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options;
                SourceSpan span{};
            } ddlClusterService;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options;
                SourceSpan span{};
            } ddlAuthProvider;
            struct {
                std::string action; // CREATE|ALTER|DROP
                std::string name;
                std::string options;
                SourceSpan span{};
            } ddlRlsPolicy;
            struct {
                std::string action; // CREATE|ALTER|DROP|REFRESH
                std::string name;
                std::string options; // includes AS SELECT body or REFRESH options
                SourceSpan span{};
            } ddlMaterializedView;
            struct {
                std::string action; // CREATE/ALTER/DROP
                std::string name;
                std::string attrs; // connection string, options raw
                SourceSpan span{};
            } ddlDbLink;
            struct {
                std::string name;
                std::string action; // RESTART WITH n, SET GENERATOR, INCREMENT BY
                long long start_with{0};
                long long increment_by{0};
                bool cycle{false};
                SourceSpan span{};
            } ddlSequence;
            struct {
                std::string name;
                std::string type_raw; // base type
                std::string default_raw;
                std::string check_raw;
                std::string collate;
                SourceSpan span{};
                bool not_null{false};
            } ddlDomain;
            struct {
                std::string name;
                std::string body_raw; // AS (select ...)
                bool with_check_option{false};
                std::string check_option_variant; // LOCAL/CASCADED/empty
                std::string columns_raw;          // optional column list after name
                SourceSpan span{};
            } ddlView;
            struct {
                std::string name;
                std::string based_on;
                std::string from_external;
                SourceSpan span{};
            } ddlCollation;
            struct {
                std::string name;
                std::string attributes;
                SourceSpan span{};
            } ddlCharset;
            struct {
                std::string name;
                std::string message;
                SourceSpan span{};
            } ddlException;
            struct {
                std::string object_type;
                std::string object_name;
                std::string text;
                SourceSpan span{};
            } ddlComment;
            struct {
                std::string object_type;
                std::string old_name;
                std::string new_name;
                SourceSpan span{};
            } ddlRename;
            struct {
                std::string object_type; // procedure | function | package
                std::string name;        // existing name
                std::string new_schema;  // target schema
                SourceSpan span{};
            } ddlMove;
            struct {
                std::string name;
                std::string attrs;
                bool active{true};
                SourceSpan span{};
            } ddlRole;
            struct {
                std::string name;
                std::string attrs;
                std::string password;
                std::string first_name;
                std::string last_name;
                std::string middle_name;
                bool active{true};
                SourceSpan span{};
            } ddlUser;
            struct {
                std::string privileges;                  // raw list or ALL PRIVILEGES
                std::vector<std::string> privilege_list; // split privileges
                std::string object_type;
                std::string object_name;
                std::string grantees; // raw list
                bool with_grant_option{false};
                bool admin_option{false};
                bool revoke_grant_option{false};
                SourceSpan span{};
            } grantStmt;
            // PSQL
            enum class PsqlStmtKind {
                Unknown,
                Assign,
                If,
                ForSelect,
                Suspend,
                Return,
                ExecStmt,
                Case,
                ExecProc,
                Call,
                While,
                Leave,
                Continue,
                Exception,
                Raise,
                PostEvent,
                Declare,
                OpenCursor,
                FetchCursor,
                CloseCursor
            };
            struct PsqlStmt {
                PsqlStmtKind kind{PsqlStmtKind::Unknown};
                std::string raw;
                // Lightweight parsed fields for enrichment
                std::string name;                   // variable or cursor name, label
                std::string type_raw;               // DECLARE variable type
                bool declare_is_cursor{false};      // DECLARE cursor vs var
                std::string cursor_name;            // OPEN/FETCH/CLOSE
                std::vector<std::string> into_vars; // INTO var list
                std::vector<std::string> args;      // for CALL/EXECUTE PROCEDURE args
                struct ExecStmtOptions {
                    bool caller_privileges{false};
                    std::string as_user;
                    std::string password;
                    std::string role;
                    std::string on_external; // connection string
                    std::string bind_option; // raw bind option
                    std::string timeout;     // raw timeout value
                } exec_opts;
                std::string for_query_raw;      // FOR SELECT ... query body
                std::string when_condition_raw; // WHEN condition
                bool when_has_do{false};
                std::string label;            // label for LEAVE/EXIT
                std::vector<PsqlStmt> nested; // nested statements for BEGIN ... END blocks
                struct TypeDescLite {
                    std::string name;
                    int length{-1};
                    int precision{-1};
                    int scale{-1};
                    std::string charset;
                    std::string collate;
                    int array_rank{0};
                } decl_type;
                SourceSpan span{};
            };
            struct {
                std::string params_raw;
                std::string returns_raw;
                std::vector<PsqlStmt> body;
                SourceSpan span{};
            } psqlBlock;
            struct {
                std::string raw;
                SourceSpan span{};
            } psqlExec;
            struct {
                std::string kind; // PROCEDURE or FUNCTION
                std::string name;
                std::string params_in;       // raw input params
                std::string returns;         // raw returns (for functions/procedures)
                std::string param_modes_raw; // IN/OUT/INOUT markers captured raw
                std::string attributes_raw;  // DETERMINISTIC, SQL SECURITY, etc.
                std::string body_raw;        // PSQL body raw
                std::vector<std::pair<std::string, std::string>> params; // (mode+name, type_raw)
                struct ParamTypeLite {
                    std::string name;
                    int length{-1};
                    int precision{-1};
                    int scale{-1};
                    std::string charset;
                    std::string collate;
                    int array_rank{0};
                };
                std::vector<ParamTypeLite> param_types; // aligned to params
                SourceSpan span{};
            } psqlRoutine;
            struct {
                std::string name;
                std::string schema_name;                     // optional schema
                std::string header_body;                     // package header declarations
                std::string implementation_body;             // package body implementation
                bool is_header{true};                        // true for header, false for body
                std::vector<std::string> public_procedures;  // public procedure names
                std::vector<std::string> public_functions;   // public function names
                std::vector<std::string> private_procedures; // private procedure names
                std::vector<std::string> private_functions;  // private function names
                SourceSpan span{};
            } psqlPackage;
            struct {
                std::string name;
                std::string table;
                std::string timing;                         // BEFORE/AFTER
                std::string events;                         // INSERT/UPDATE/DELETE or combinations
                std::vector<std::string> events_list;       // split event tokens
                std::vector<std::string> update_of_columns; // column list for UPDATE OF
                std::string for_each;                       // ROW/STATEMENT
                bool active{true};
                int position{0};
                std::string body_raw;
                SourceSpan span{};
            } psqlTrigger;
            struct {
                std::string routine_name;
                std::vector<std::string> arguments; // argument expressions
                std::string args_raw;               // raw argument list for later parsing
                SourceSpan span{};
            } psqlCall;
            struct {
                std::string name;
                std::string external_name;
                std::string engine;
                std::string attrs; // raw tail
                SourceSpan span{};
            } ddlUdf;
            struct {
                std::string name;
                std::string external_name;
                std::string engine;
                std::string attrs; // raw tail
                SourceSpan span{};
            } ddlUdr;
            struct {
                std::string name;
                std::string input_type;  // raw subtype
                std::string output_type; // raw subtype
                std::string entry_point; // 'ep'
                std::string module_name; // 'lib'
                std::string attrs;       // raw tail / drop marker
                SourceSpan span{};
            } ddlBlobFilter;
            struct {
                std::string name;
                std::string attrs; // raw mapping attributes
                SourceSpan span{};
            } ddlMapping;
            struct {
                std::string name;
                std::string on_commit; // PRESERVE ROWS / DELETE ROWS
                std::string columns_raw;
                SourceSpan span{};
            } ddlGtt;
            struct {
                std::string statement_raw;
                bool analyze{false};
                SourceSpan span{};
            } ddlExplain;
            struct {
                std::string table_name;
                std::string column_list; // optional "(a,b)"
                bool full{false};
                bool verbose{false};
                std::string kind;       // ANALYZE | VACUUM | CREATE/DROP STATISTICS
                std::string stats_name; // for CREATE/DROP STATISTICS
                SourceSpan span{};
            } ddlAnalyzeVacuum;
        };

    } // namespace engine
} // namespace scratchbird

#endif // SCRATCHBIRD_ENGINE_AST_H
