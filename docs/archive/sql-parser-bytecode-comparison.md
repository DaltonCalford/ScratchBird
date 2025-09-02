# SQL Parser and Bytecode Compilation: Comparative Analysis

> **Note**: The BLR implementation details from this document have been incorporated into the [Complete SBLR/BLR Specification](./scratchbird-bytecode-complete-specification.md), which provides the authoritative bytecode specification for the ScratchBird project.

## Table of Contents
1. [FirebirdSQL BLR Implementation](#firebirdsql-blr-implementation)
2. [PostgreSQL Parse Tree and Plan Trees](#postgresql-parse-tree-and-plan-trees)
3. [MySQL/MariaDB Prepared Statements](#mysqlmariadb-prepared-statements)
4. [Microsoft SQL Server Query Plans](#microsoft-sql-server-query-plans)
5. [Python Bytecode Compilation](#python-bytecode-compilation)
6. [Comparative Analysis](#comparative-analysis)
7. [Optimization Strategies](#optimization-strategies)

---

# FirebirdSQL BLR Implementation

## BLR (Binary Language Representation)

### BLR Structure and Opcodes
```c
// BLR opcodes
enum blr_operators {
    blr_version4 = 4,
    blr_version5 = 5,
    blr_begin = 2,
    blr_end = 255,
    blr_message = 4,
    blr_eoc = 76,
    blr_assignment = 1,
    
    // Data types
    blr_text = 14,
    blr_text2 = 15,
    blr_short = 7,
    blr_long = 8,
    blr_int64 = 16,
    blr_float = 10,
    blr_double = 27,
    blr_timestamp = 35,
    blr_varying = 37,
    blr_varying2 = 38,
    blr_blob = 261,
    
    // Expressions
    blr_add = 34,
    blr_subtract = 35,
    blr_multiply = 36,
    blr_divide = 37,
    blr_negate = 38,
    blr_concatenate = 39,
    blr_substring = 40,
    
    // Comparisons
    blr_eql = 47,
    blr_neq = 48,
    blr_gtr = 49,
    blr_geq = 50,
    blr_lss = 51,
    blr_leq = 52,
    blr_between = 53,
    blr_like = 54,
    
    // Control flow
    blr_if = 60,
    blr_loop = 61,
    blr_for = 62,
    blr_while = 63,
    blr_leave = 64,
    blr_continue = 65,
    
    // Database operations
    blr_store = 70,
    blr_modify = 71,
    blr_erase = 72,
    blr_fetch = 73,
    blr_for_select = 74,
    blr_send = 75,
    blr_receive = 77,
    
    // Functions
    blr_function = 100,
    blr_gen_id = 101,
    blr_aggregate = 102,
    blr_run_count = 103,
    blr_user_name = 104,
    
    // Field access
    blr_field = 120,
    blr_parameter = 121,
    blr_variable = 122,
    blr_literal = 123,
    blr_cast = 124,
    
    // Procedure/trigger
    blr_procedure = 130,
    blr_trigger = 131,
    blr_exec_proc = 132,
    blr_exec_stmt = 133,
    blr_block = 134
};

// BLR generation from SQL
typedef struct blr_generator {
    UCHAR*      blr_buffer;         // BLR buffer
    ULONG       blr_size;           // Buffer size
    ULONG       blr_offset;         // Current offset
    USHORT      blr_msg_number;    // Message number
    USHORT      blr_parameter;     // Parameter number
    JrdMemoryPool* blr_pool;       // Memory pool
} BLRGenerator;

// Generate BLR from parsed SQL
void GEN_statement(
    CompilerScratch*    csb,
    ParseNode*          node)
{
    BLRGenerator* gen = csb->csb_blr_gen;
    
    switch (node->nod_type) {
        case nod_select:
            GEN_select(gen, node);
            break;
            
        case nod_insert:
            GEN_insert(gen, node);
            break;
            
        case nod_update:
            GEN_update(gen, node);
            break;
            
        case nod_delete:
            GEN_delete(gen, node);
            break;
            
        case nod_execute_procedure:
            GEN_execute_procedure(gen, node);
            break;
            
        case nod_if:
            GEN_if_statement(gen, node);
            break;
            
        case nod_while:
            GEN_while_loop(gen, node);
            break;
            
        case nod_for_select:
            GEN_for_select(gen, node);
            break;
    }
}

// Generate SELECT in BLR
void GEN_select(
    BLRGenerator*       gen,
    ParseNode*          node)
{
    // Start RSE (Record Selection Expression)
    stuff_byte(gen, blr_rse);
    
    // Number of relations
    stuff_byte(gen, node->nod_count);
    
    // Generate relations
    for (int i = 0; i < node->nod_count; i++) {
        ParseNode* relation = node->nod_relations[i];
        stuff_byte(gen, blr_relation);
        stuff_string(gen, relation->nod_name);
        stuff_byte(gen, i);  // Context number
    }
    
    // Generate WHERE clause if present
    if (node->nod_where) {
        stuff_byte(gen, blr_boolean);
        GEN_expression(gen, node->nod_where);
    }
    
    // Generate projection (SELECT list)
    stuff_byte(gen, blr_project);
    stuff_byte(gen, node->nod_select_count);
    
    for (int i = 0; i < node->nod_select_count; i++) {
        GEN_expression(gen, node->nod_select_list[i]);
    }
    
    stuff_byte(gen, blr_end);
}

// Generate expression in BLR
void GEN_expression(
    BLRGenerator*       gen,
    ParseNode*          node)
{
    switch (node->nod_type) {
        case nod_field:
            stuff_byte(gen, blr_field);
            stuff_byte(gen, node->nod_context);
            stuff_word(gen, node->nod_field_id);
            break;
            
        case nod_literal:
            stuff_byte(gen, blr_literal);
            stuff_byte(gen, node->nod_dtype);
            stuff_literal(gen, node->nod_value);
            break;
            
        case nod_add:
            stuff_byte(gen, blr_add);
            GEN_expression(gen, node->nod_arg[0]);
            GEN_expression(gen, node->nod_arg[1]);
            break;
            
        case nod_multiply:
            stuff_byte(gen, blr_multiply);
            GEN_expression(gen, node->nod_arg[0]);
            GEN_expression(gen, node->nod_arg[1]);
            break;
            
        case nod_equal:
            stuff_byte(gen, blr_eql);
            GEN_expression(gen, node->nod_arg[0]);
            GEN_expression(gen, node->nod_arg[1]);
            break;
            
        case nod_function:
            stuff_byte(gen, blr_function);
            stuff_string(gen, node->nod_function_name);
            stuff_byte(gen, node->nod_arg_count);
            for (int i = 0; i < node->nod_arg_count; i++) {
                GEN_expression(gen, node->nod_arg[i]);
            }
            break;
    }
}

// BLR interpreter/executor
typedef struct blr_reader {
    const UCHAR*    blr_base;       // BLR base pointer
    const UCHAR*    blr_ptr;        // Current position
    const UCHAR*    blr_end;        // End of BLR
} BLRReader;

// Execute BLR
void EXE_execute_blr(
    Request*            request,
    const UCHAR*        blr,
    ULONG               blr_length)
{
    BLRReader reader;
    reader.blr_base = blr;
    reader.blr_ptr = blr;
    reader.blr_end = blr + blr_length;
    
    // Check version
    UCHAR version = BLR_get_byte(&reader);
    if (version != blr_version5) {
        ERR_post("Unsupported BLR version");
    }
    
    // Execute statements
    while (reader.blr_ptr < reader.blr_end) {
        UCHAR op = BLR_get_byte(&reader);
        
        switch (op) {
            case blr_begin:
                EXE_begin_block(request, &reader);
                break;
                
            case blr_for_select:
                EXE_for_select(request, &reader);
                break;
                
            case blr_store:
                EXE_store(request, &reader);
                break;
                
            case blr_modify:
                EXE_modify(request, &reader);
                break;
                
            case blr_erase:
                EXE_erase(request, &reader);
                break;
                
            case blr_if:
                EXE_if_statement(request, &reader);
                break;
                
            case blr_assignment:
                EXE_assignment(request, &reader);
                break;
                
            case blr_end:
                return;
                
            default:
                ERR_post("Unknown BLR opcode");
        }
    }
}

// BLR optimization pass
void OPT_optimize_blr(
    UCHAR**             blr,
    ULONG*              blr_length)
{
    BLROptimizer optimizer;
    
    // Parse BLR into intermediate representation
    optimizer.ir = parse_blr_to_ir(*blr, *blr_length);
    
    // Optimization passes
    OPT_constant_folding(&optimizer);
    OPT_dead_code_elimination(&optimizer);
    OPT_common_subexpression_elimination(&optimizer);
    OPT_loop_invariant_motion(&optimizer);
    OPT_index_selection(&optimizer);
    OPT_join_order_optimization(&optimizer);
    
    // Generate optimized BLR
    *blr = generate_blr_from_ir(optimizer.ir, blr_length);
}
```

---

# PostgreSQL Parse Tree and Plan Trees

## PostgreSQL Multi-Stage Compilation

### Parse Tree Structure
```c
// Raw parse tree node
typedef struct RawStmt {
    NodeTag     type;
    Node       *stmt;           // Raw parse tree
    int         stmt_location;  // Statement location in query string
    int         stmt_len;       // Statement length
} RawStmt;

// Query tree after analysis
typedef struct Query {
    NodeTag     type;
    CmdType     commandType;    // SELECT/INSERT/UPDATE/DELETE
    QuerySource querySource;    // Where did I come from?
    uint64      queryId;        // Query identifier
    bool        canSetTag;      // Can set command tag?
    
    Node       *utilityStmt;    // Non-optimizable statement
    int         resultRelation; // Target relation for INSERT/UPDATE/DELETE
    bool        hasAggs;        // Has aggregates?
    bool        hasWindowFuncs; // Has window functions?
    bool        hasTargetSRFs;  // Has set-returning functions?
    bool        hasSubLinks;    // Has subqueries?
    bool        hasDistinctOn;  // Has DISTINCT ON?
    bool        hasRecursive;   // Has WITH RECURSIVE?
    bool        hasModifyingCTE; // Has INSERT/UPDATE/DELETE in WITH?
    bool        hasForUpdate;   // Has FOR UPDATE?
    bool        hasRowSecurity; // Row-level security?
    
    List       *cteList;        // WITH list
    List       *rtable;         // Range table
    FromExpr   *jointree;       // Table join tree
    List       *targetList;     // Target list (SELECT expressions)
    List       *withCheckOptions; // WITH CHECK OPTION list
    List       *returningList;  // RETURNING list
    List       *groupClause;    // GROUP BY clauses
    List       *groupingSets;   // GROUPING SETS clauses
    Node       *havingQual;     // HAVING expression
    List       *windowClause;   // WINDOW clauses
    List       *distinctClause; // DISTINCT clauses
    List       *sortClause;     // ORDER BY clauses
    Node       *limitOffset;    // OFFSET expression
    Node       *limitCount;     // LIMIT expression
    List       *rowMarks;       // FOR UPDATE/SHARE info
    Node       *setOperations;  // UNION/INTERSECT/EXCEPT tree
} Query;

// Plan tree node (execution plan)
typedef struct Plan {
    NodeTag     type;
    
    /* Estimated execution costs */
    Cost        startup_cost;   // Cost before first result
    Cost        total_cost;     // Total cost
    double      plan_rows;      // Number of rows
    int         plan_width;     // Average row width
    
    /* Parallel execution */
    bool        parallel_aware;  // Parallel-aware plan?
    bool        parallel_safe;   // Safe for parallel execution?
    int         plan_node_id;    // Node ID for EXPLAIN
    
    /* Common structural data */
    List       *targetlist;      // Target list
    List       *qual;            // Qualification clauses
    struct Plan *lefttree;       // Left subtree
    struct Plan *righttree;      // Right subtree
    List       *initPlan;        // Init subplans
    
    /* Information for executor */
    Bitmapset  *extParam;
    Bitmapset  *allParam;
} Plan;

// Parse SQL to raw parse tree
List *
raw_parser(const char *str)
{
    core_yyscan_t yyscanner;
    base_yy_extra_type yyextra;
    int         yyresult;
    
    /* Initialize scanner */
    yyscanner = scanner_init(str, &yyextra.core_yy_extra,
                            &ScanKeywords, ScanKeywordTokens);
    
    /* Parse */
    yyresult = base_yyparse(yyscanner);
    
    /* Clean up */
    scanner_finish(yyscanner);
    
    if (yyresult) {
        return NIL;
    }
    
    return yyextra.parsetree;
}

// Analyze and rewrite parse tree
Query *
parse_analyze(RawStmt *parseTree, const char *sourceText,
             Oid *paramTypes, int numParams,
             QueryEnvironment *queryEnv)
{
    ParseState *pstate = make_parsestate(NULL);
    Query      *query;
    
    pstate->p_sourcetext = sourceText;
    pstate->p_paramtypes = paramTypes;
    pstate->p_numparams = numParams;
    pstate->p_queryEnv = queryEnv;
    
    /* Transform raw parse tree to Query */
    query = transformTopLevelStmt(pstate, parseTree);
    
    /* Run through rewriter */
    query = QueryRewrite(query);
    
    free_parsestate(pstate);
    
    return query;
}

// Generate execution plan
PlannedStmt *
planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
    PlannedStmt *result;
    PlannerGlobal *glob;
    PlannerInfo *root;
    RelOptInfo *final_rel;
    Path       *best_path;
    Plan       *top_plan;
    
    /* Create global planner state */
    glob = makeNode(PlannerGlobal);
    glob->boundParams = boundParams;
    glob->subplans = NIL;
    glob->subroots = NIL;
    glob->rewindPlanIDs = NULL;
    glob->finalrtable = NIL;
    glob->finalrowmarks = NIL;
    glob->resultRelations = NIL;
    glob->rootResultRelations = NIL;
    glob->parallelizeWorkers = 0;
    
    /* Create root planner state */
    root = makeNode(PlannerInfo);
    root->parse = parse;
    root->glob = glob;
    root->query_level = 1;
    root->parent_root = NULL;
    root->plan_params = NIL;
    root->outer_params = NULL;
    
    /* Process targetlist and joins */
    root->processed_tlist = preprocess_targetlist(root);
    root->processed_groupClause = preprocess_groupclause(root);
    
    /* Build RelOptInfo for each base relation */
    setup_simple_rel_arrays(root);
    add_base_rels_to_query(root, (Node *) parse->jointree);
    
    /* Build access paths for each base relation */
    set_base_rel_sizes(root);
    set_base_rel_pathlists(root);
    
    /* Generate join paths */
    final_rel = make_one_rel(root, root->join_rel_list);
    
    /* Select best path */
    best_path = get_cheapest_fractional_path(final_rel, 
                                            root->tuple_fraction);
    
    /* Create plan from path */
    top_plan = create_plan(root, best_path);
    
    /* Build PlannedStmt */
    result = makeNode(PlannedStmt);
    result->commandType = parse->commandType;
    result->queryId = parse->queryId;
    result->hasReturning = (parse->returningList != NIL);
    result->hasModifyingCTE = parse->hasModifyingCTE;
    result->canSetTag = parse->canSetTag;
    result->transientPlan = false;
    result->dependsOnRole = false;
    result->parallelModeNeeded = false;
    result->planTree = top_plan;
    result->rtable = glob->finalrtable;
    result->resultRelations = glob->resultRelations;
    result->rootResultRelations = glob->rootResultRelations;
    result->subplans = glob->subplans;
    result->rewindPlanIDs = glob->rewindPlanIDs;
    result->rowMarks = glob->finalrowmarks;
    result->relationOids = glob->relationOids;
    result->invalItems = glob->invalItems;
    result->paramExecTypes = glob->paramExecTypes;
    
    return result;
}

// JIT compilation for expressions
typedef struct ExprState {
    NodeTag     tag;
    uint8       flags;          // Bitmask of EEO_FLAG_* bits
    bool        resnull;        // Result is NULL
    Datum       resvalue;       // Result value
    TupleTableSlot *resultslot; // Result slot
    struct ExprEvalStep *steps; // Evaluation steps
    ExprStateEvalFunc evalfunc; // Evaluation function
    Expr       *expr;           // Expression tree
    int         steps_len;      // Number of steps
    int         steps_alloc;    // Allocated size
    
    /* JIT compilation state */
    struct JitContext *jit_context;
} ExprState;

// Expression evaluation steps (bytecode-like)
typedef enum ExprEvalOp {
    /* Constant/variable operations */
    EEOP_DONE,
    EEOP_CONST,
    EEOP_PARAM,
    EEOP_VAR,
    
    /* Comparison operations */
    EEOP_QUAL,
    EEOP_JUMP,
    EEOP_JUMP_IF_NULL,
    EEOP_JUMP_IF_NOT_NULL,
    EEOP_JUMP_IF_NOT_TRUE,
    
    /* Function calls */
    EEOP_FUNCEXPR,
    EEOP_FUNCEXPR_STRICT,
    EEOP_FUNCEXPR_FUSAGE,
    EEOP_FUNCEXPR_STRICT_FUSAGE,
    
    /* Aggregates */
    EEOP_AGG_STRICT_DESERIALIZE,
    EEOP_AGG_DESERIALIZE,
    EEOP_AGG_STRICT_INPUT_CHECK,
    EEOP_AGG_PLAIN_TRANS,
    EEOP_AGG_ORDERED_TRANS,
    
    /* Row operations */
    EEOP_ROW,
    EEOP_ROWCOMPARE_STEP,
    EEOP_ROWCOMPARE_FINAL,
    
    /* Array operations */
    EEOP_ARRAYEXPR,
    EEOP_ARRAYCOERCE,
    EEOP_ARRAY_REF,
    
    /* Case/coalesce */
    EEOP_CASE_TESTVAL,
    EEOP_MAKE_READONLY,
    EEOP_COALESCE
} ExprEvalOp;

typedef struct ExprEvalStep {
    ExprEvalOp opcode;
    Datum      *resvalue;
    bool       *resnull;
    
    union {
        struct {
            Datum value;
            bool isnull;
        } constval;
        
        struct {
            int paramid;
            Oid paramtype;
        } param;
        
        struct {
            FmgrInfo *finfo;
            FunctionCallInfo fcinfo_data;
            PGFunction fn_addr;
            int nargs;
        } func;
        
        struct {
            int jumpdone;
        } jump;
    } d;
} ExprEvalStep;

// Compile expression to steps
ExprState *
ExecInitExpr(Expr *node, PlanState *parent)
{
    ExprState  *state;
    ExprEvalStep scratch = {0};
    
    /* Create expression state */
    state = makeNode(ExprState);
    state->expr = node;
    state->parent = parent;
    state->steps = NULL;
    state->steps_len = 0;
    state->steps_alloc = 16;
    state->steps = palloc(sizeof(ExprEvalStep) * state->steps_alloc);
    
    /* Compile expression to steps */
    ExecInitExprRec(node, state, &state->resvalue, &state->resnull);
    
    /* Add DONE step */
    scratch.opcode = EEOP_DONE;
    ExprEvalPushStep(state, &scratch);
    
    /* Try to JIT compile if enabled */
    if (jit_enabled && jit_above_cost >= 0 &&
        state->parent && state->parent->plan &&
        state->parent->plan->total_cost > jit_above_cost) {
        
        jit_compile_expr(state);
    }
    
    /* Select evaluation function */
    if (state->jit_context && state->jit_context->compiled) {
        state->evalfunc = state->jit_context->evalfunc;
    } else {
        state->evalfunc = ExecInterpExpr;
    }
    
    return state;
}
```

---

# MySQL/MariaDB Prepared Statements

## MySQL Internal Representation

### Prepared Statement Structure
```c
// Prepared statement
typedef struct st_mysql_stmt {
    MEM_ROOT       mem_root;        // Memory root
    LIST           list;            // List node
    mysql         *mysql;           // Connection handle
    MYSQL_BIND    *params;          // Input parameters
    MYSQL_BIND    *bind;            // Output bindings
    MYSQL_FIELD   *fields;          // Result metadata
    MYSQL_DATA     result;          // Cached result
    MYSQL_ROWS    *data_cursor;     // Current row
    
    /* Statement state */
    enum enum_stmt_state state;
    
    /* Prepared statement on server */
    ulong          stmt_id;         // Statement ID
    ulong          flags;           // Flags
    ulong          prefetch_rows;   // Prefetch row count
    uint           server_status;   // Server status
    uint           last_errno;      // Last error number
    uint           param_count;     // Parameter count
    uint           field_count;     // Field count
    
    my_bool        bind_param_done; // Parameters bound?
    my_bool        bind_result_done; // Results bound?
    my_bool        unbuffered_fetch_cancelled;
    my_bool        update_max_length;
} MYSQL_STMT;

// Server-side prepared statement
class Prepared_statement {
public:
    THD           *thd;             // Thread context
    LEX           *lex;             // Parsed statement
    Item_param   **param_array;     // Parameter array
    uint           param_count;      // Number of parameters
    
    Protocol_binary protocol;       // Binary protocol
    
    Query_arena    main_mem_root;   // Memory root
    
    /* Execution state */
    bool           is_sql_prepare;  // SQL PREPARE?
    bool           with_log;        // Log this statement?
    
    /* The query string */
    LEX_CSTRING    query_string;
    
    /* Parsed tree after preparation */
    bool (*set_params)(Prepared_statement *ps, uchar *packet,
                      uchar *packet_end);
    bool (*execute)(Prepared_statement *ps, 
                   bool open_cursor,
                   uchar *packet, uchar *packet_end);
    
    /* Performance statistics */
    ulonglong      m_prepared_stmt_count;
    ulonglong      m_executed_stmt_count;
};

// Parse and prepare statement
bool
Prepared_statement::prepare(const char *query, uint query_length)
{
    DBUG_ENTER("Prepared_statement::prepare");
    
    /* Save query */
    query_string.str = query;
    query_string.length = query_length;
    
    /* Parse query */
    Parser_state parser_state;
    if (parser_state.init(thd, query, query_length)) {
        DBUG_RETURN(true);
    }
    
    /* Parse SQL */
    lex_start(thd);
    thd->lex->safe_to_cache_query = FALSE;
    
    bool error = parse_sql(thd, &parser_state, NULL);
    
    if (error) {
        DBUG_RETURN(true);
    }
    
    lex = thd->lex;
    
    /* Prepare the statement */
    error = mysql_test_select(thd, lex);
    
    if (!error) {
        /* Count parameters */
        param_count = lex->param_list.elements;
        
        /* Allocate parameter array */
        if (param_count > 0) {
            param_array = (Item_param **)
                alloc_root(&main_mem_root,
                          sizeof(Item_param *) * param_count);
            
            /* Initialize parameters */
            List_iterator<Item_param> param_iterator(lex->param_list);
            Item_param *param;
            uint i = 0;
            
            while ((param = param_iterator++)) {
                param_array[i++] = param;
            }
        }
        
        /* Setup execution functions */
        set_params = insert_params;
        
        if (lex->sql_command == SQLCOM_SELECT) {
            execute = mysql_execute_select;
        } else if (lex->sql_command == SQLCOM_INSERT) {
            execute = mysql_execute_insert;
        } else if (lex->sql_command == SQLCOM_UPDATE) {
            execute = mysql_execute_update;
        } else if (lex->sql_command == SQLCOM_DELETE) {
            execute = mysql_execute_delete;
        }
    }
    
    DBUG_RETURN(error);
}

// Execute prepared statement
bool
Prepared_statement::execute_loop(bool open_cursor,
                                uchar *packet,
                                uchar *packet_end)
{
    DBUG_ENTER("Prepared_statement::execute_loop");
    
    /* Set parameters from packet */
    if (param_count && set_params(this, packet, packet_end)) {
        DBUG_RETURN(true);
    }
    
    /* Rewrite statement for current parameters */
    if (unlikely(thd->optimizer_switch_flag(OPTIMIZER_SWITCH_PS_REWRITE))) {
        rewrite_query();
    }
    
    /* Execute statement */
    bool error = execute(this, open_cursor, packet, packet_end);
    
    /* Update statistics */
    m_executed_stmt_count++;
    
    DBUG_RETURN(error);
}

// Item representation (expression tree)
class Item {
public:
    enum Type {
        FIELD_ITEM,
        FUNC_ITEM,
        SUM_FUNC_ITEM,
        STRING_ITEM,
        INT_ITEM,
        REAL_ITEM,
        NULL_ITEM,
        VARBIN_ITEM,
        COPY_STR_ITEM,
        FIELD_AVG_ITEM,
        COND_ITEM,
        REF_ITEM,
        FIELD_STD_ITEM,
        FIELD_VARIANCE_ITEM,
        INSERT_VALUE_ITEM,
        SUBSELECT_ITEM,
        ROW_ITEM,
        CACHE_ITEM,
        TYPE_HOLDER,
        PARAM_ITEM
    };
    
    /* Item properties */
    Type        type() const { return m_type; }
    
    /* Evaluation */
    virtual longlong val_int() = 0;
    virtual double val_real() = 0;
    virtual String *val_str(String *str) = 0;
    virtual my_decimal *val_decimal(my_decimal *decimal_buffer) = 0;
    virtual bool val_bool() = 0;
    
    /* Code generation for optimizer */
    virtual void compile(Bytecode_writer *writer) {
        switch (type()) {
            case INT_ITEM:
                writer->write_opcode(OP_PUSH_INT);
                writer->write_int(val_int());
                break;
                
            case FIELD_ITEM:
                writer->write_opcode(OP_LOAD_FIELD);
                writer->write_int(((Item_field *)this)->field->field_index);
                break;
                
            case FUNC_ITEM:
                Item_func *func = (Item_func *)this;
                for (uint i = 0; i < func->arg_count; i++) {
                    func->args[i]->compile(writer);
                }
                writer->write_opcode(func->functype());
                break;
        }
    }
    
protected:
    Type        m_type;
};

// Bytecode for faster execution (MariaDB 10.3+)
class sp_instr {
public:
    enum enum_opcode {
        OP_NOP,
        OP_PUSH_INT,
        OP_PUSH_REAL,
        OP_PUSH_STRING,
        OP_PUSH_NULL,
        OP_LOAD_FIELD,
        OP_LOAD_VAR,
        OP_STORE_VAR,
        OP_ADD,
        OP_SUB,
        OP_MUL,
        OP_DIV,
        OP_EQ,
        OP_NE,
        OP_LT,
        OP_LE,
        OP_GT,
        OP_GE,
        OP_AND,
        OP_OR,
        OP_NOT,
        OP_JUMP,
        OP_JUMP_IF_TRUE,
        OP_JUMP_IF_FALSE,
        OP_CALL,
        OP_RETURN
    };
    
    uint            m_ip;           // Instruction pointer
    enum_opcode     m_opcode;       // Operation code
    
    virtual int execute(THD *thd, uint *nextp) = 0;
};

// Stored procedure compilation
class sp_head {
public:
    LEX_STRING      m_name;         // Procedure name
    LEX_STRING      m_body;         // Procedure body
    
    /* Compiled instructions */
    Dynamic_array<sp_instr *> m_instr;
    
    /* Compile procedure */
    bool compile(THD *thd) {
        /* Parse procedure body */
        Parser_state parser_state;
        parser_state.init(thd, m_body.str, m_body.length);
        
        if (parse_sql(thd, &parser_state, this)) {
            return true;
        }
        
        /* Generate bytecode */
        Bytecode_writer writer(&m_instr);
        
        List_iterator<Item> it(thd->lex->item_list);
        Item *item;
        
        while ((item = it++)) {
            item->compile(&writer);
        }
        
        return false;
    }
    
    /* Execute compiled procedure */
    bool execute(THD *thd) {
        uint ip = 0;
        
        while (ip < m_instr.elements) {
            sp_instr *i = m_instr.at(ip);
            
            if (i->execute(thd, &ip)) {
                return true;
            }
        }
        
        return false;
    }
};
```

---

# Microsoft SQL Server Query Plans

## SQL Server Plan Cache and Compilation

### Query Plan Structure
```c
// Compiled plan
typedef struct COMPILED_PLAN {
    GUID            plan_guid;       // Plan identifier
    ULONG           plan_id;         // Plan ID
    ULONG           size;            // Plan size
    ULONG           cost;            // Plan cost
    ULONG           compile_time;    // Compilation time
    ULONG           compile_cpu;     // CPU used for compilation
    ULONG           compile_memory;  // Memory used
    
    /* Plan tree */
    QUERY_PLAN     *plan_tree;       // Root of plan tree
    
    /* Parameters */
    PARAM_INFO     *parameters;      // Parameter information
    ULONG           param_count;     // Number of parameters
    
    /* Metadata */
    SCHEMA_INFO    *schema_binding;  // Schema binding info
    SET_OPTIONS     set_options;     // SET options
    
    /* Statistics */
    ULONG           use_count;        // Usage count
    ULONG           recompile_count; // Recompile count
    SYSTEMTIME      creation_time;   // Creation time
    SYSTEMTIME      last_execution;  // Last execution
} COMPILED_PLAN;

// Query plan operators
typedef enum OPERATOR_TYPE {
    OP_SELECT,
    OP_PROJECT,
    OP_FILTER,
    OP_NESTED_LOOP,
    OP_HASH_JOIN,
    OP_MERGE_JOIN,
    OP_HASH_AGG,
    OP_STREAM_AGG,
    OP_SORT,
    OP_TOP,
    OP_COMPUTE_SCALAR,
    OP_TABLE_SCAN,
    OP_INDEX_SCAN,
    OP_INDEX_SEEK,
    OP_RID_LOOKUP,
    OP_KEY_LOOKUP,
    OP_INSERT,
    OP_UPDATE,
    OP_DELETE,
    OP_SPOOL,
    OP_ASSERT,
    OP_SPLIT,
    OP_COLLAPSE,
    OP_SEGMENT,
    OP_SEQUENCE,
    OP_SWITCH,
    OP_UDAGG,
    OP_UDX
} OPERATOR_TYPE;

// Query plan node
typedef struct QUERY_PLAN {
    OPERATOR_TYPE   operator_type;   // Operator type
    ULONG           node_id;         // Node ID
    
    /* Cost estimates */
    double          estimated_rows;  // Estimated rows
    double          estimated_cpu;   // CPU cost
    double          estimated_io;    // I/O cost
    double          estimated_cost;  // Total cost
    
    /* Runtime statistics */
    ULONG           actual_rows;     // Actual rows
    ULONG           actual_rebinds;  // Actual rebinds
    ULONG           actual_rewinds;  // Actual rewinds
    
    /* Children */
    struct QUERY_PLAN *left_child;   // Left child
    struct QUERY_PLAN *right_child;  // Right child
    
    /* Operator-specific data */
    union {
        struct {
            TABLE_ID    table_id;
            INDEX_ID    index_id;
            BOOL        ordered;
            DIRECTION   direction;
        } scan;
        
        struct {
            EXPRESSION *predicate;
        } filter;
        
        struct {
            EXPRESSION *join_predicate;
            JOIN_TYPE   join_type;
        } join;
        
        struct {
            EXPRESSION *group_by;
            EXPRESSION *aggregates;
        } aggregate;
        
        struct {
            SORT_KEYS  *sort_keys;
            BOOL        distinct;
        } sort;
    } op_data;
} QUERY_PLAN;

// Parse and compile query
COMPILED_PLAN *
CompileQuery(const WCHAR *query_text, ULONG query_length)
{
    PARSER_CONTEXT  parser_ctx;
    OPTIMIZER_CONTEXT optimizer_ctx;
    COMPILED_PLAN  *plan;
    
    /* Initialize parser context */
    InitParserContext(&parser_ctx, query_text, query_length);
    
    /* Parse query */
    PARSE_TREE *parse_tree = ParseSQL(&parser_ctx);
    
    if (!parse_tree) {
        return NULL;
    }
    
    /* Bind and validate */
    if (!BindAndValidate(parse_tree)) {
        FreeParseTree(parse_tree);
        return NULL;
    }
    
    /* Initialize optimizer */
    InitOptimizerContext(&optimizer_ctx, parse_tree);
    
    /* Generate logical plan */
    LOGICAL_PLAN *logical_plan = GenerateLogicalPlan(&optimizer_ctx);
    
    /* Optimize */
    OptimizeLogicalPlan(&optimizer_ctx, logical_plan);
    
    /* Generate physical plan */
    QUERY_PLAN *physical_plan = GeneratePhysicalPlan(&optimizer_ctx, 
                                                     logical_plan);
    
    /* Create compiled plan */
    plan = AllocateCompiledPlan();
    plan->plan_tree = physical_plan;
    plan->compile_time = GetTickCount() - start_time;
    plan->cost = optimizer_ctx.best_cost;
    
    /* Store in plan cache */
    AddToPlanCache(plan);
    
    return plan;
}

// T-SQL stored procedure compilation
typedef struct SP_COMPILED {
    WCHAR          *sp_name;         // Procedure name
    ULONG           sp_id;           // Procedure ID
    
    /* Compiled body */
    SP_STATEMENT   *statements;      // Statement list
    ULONG           statement_count; // Number of statements
    
    /* Variables */
    SP_VARIABLE    *variables;       // Variable declarations
    ULONG           variable_count;  // Number of variables
    
    /* Parameters */
    SP_PARAMETER   *parameters;      // Parameters
    ULONG           parameter_count; // Number of parameters
} SP_COMPILED;

// Statement types in stored procedure
typedef enum SP_STMT_TYPE {
    SP_STMT_ASSIGN,
    SP_STMT_IF,
    SP_STMT_WHILE,
    SP_STMT_GOTO,
    SP_STMT_LABEL,
    SP_STMT_RETURN,
    SP_STMT_EXEC,
    SP_STMT_SELECT,
    SP_STMT_INSERT,
    SP_STMT_UPDATE,
    SP_STMT_DELETE,
    SP_STMT_DECLARE,
    SP_STMT_SET,
    SP_STMT_PRINT,
    SP_STMT_RAISERROR,
    SP_STMT_THROW,
    SP_STMT_TRY,
    SP_STMT_CATCH,
    SP_STMT_BEGIN_TRAN,
    SP_STMT_COMMIT,
    SP_STMT_ROLLBACK
} SP_STMT_TYPE;

// Compiled statement
typedef struct SP_STATEMENT {
    SP_STMT_TYPE    type;            // Statement type
    ULONG           line_number;     // Source line number
    
    union {
        struct {
            VARIABLE_ID var_id;
            EXPRESSION *value;
        } assign;
        
        struct {
            EXPRESSION *condition;
            SP_STATEMENT *then_branch;
            SP_STATEMENT *else_branch;
        } if_stmt;
        
        struct {
            EXPRESSION *condition;
            SP_STATEMENT *body;
        } while_stmt;
        
        struct {
            COMPILED_PLAN *plan;
        } query;
    } stmt_data;
    
    struct SP_STATEMENT *next;       // Next statement
} SP_STATEMENT;

// Compile stored procedure
SP_COMPILED *
CompileStoredProcedure(const WCHAR *sp_text)
{
    SP_PARSER_CONTEXT ctx;
    SP_COMPILED *compiled;
    
    /* Initialize parser */
    InitSPParser(&ctx, sp_text);
    
    /* Parse procedure */
    if (!ParseStoredProcedure(&ctx)) {
        return NULL;
    }
    
    /* Create compiled procedure */
    compiled = AllocateSPCompiled();
    
    /* Compile each statement */
    SP_STATEMENT *current = NULL;
    
    for (PARSE_NODE *node = ctx.parse_tree; node; node = node->next) {
        SP_STATEMENT *stmt = CompileSPStatement(node);
        
        if (!compiled->statements) {
            compiled->statements = stmt;
        } else {
            current->next = stmt;
        }
        current = stmt;
        compiled->statement_count++;
    }
    
    return compiled;
}

// Native compilation (Hekaton)
typedef struct NATIVE_PROC {
    WCHAR          *proc_name;       // Procedure name
    DLL_HANDLE      dll_handle;      // Compiled DLL
    PROC_ENTRY      entry_point;     // Entry point
    
    /* C code generation */
    STRING_BUFFER   c_code;          // Generated C code
    
    /* Compilation flags */
    BOOL            with_native_compilation;
    BOOL            with_schemabinding;
    BOOL            with_execute_as_owner;
} NATIVE_PROC;

// Generate C code for native procedure
void
GenerateNativeCode(NATIVE_PROC *proc, SP_COMPILED *compiled)
{
    /* Generate includes */
    AppendString(&proc->c_code,
        "#include \"sqlserver.h\"\n"
        "#include \"hekaton.h\"\n\n");
    
    /* Generate procedure function */
    AppendFormat(&proc->c_code,
        "HRESULT %S_Native(EXEC_CONTEXT *ctx) {\n",
        proc->proc_name);
    
    /* Generate variable declarations */
    for (ULONG i = 0; i < compiled->variable_count; i++) {
        SP_VARIABLE *var = &compiled->variables[i];
        AppendFormat(&proc->c_code,
            "    %s %s;\n",
            GetCType(var->sql_type),
            var->name);
    }
    
    /* Generate statements */
    for (SP_STATEMENT *stmt = compiled->statements; 
         stmt; 
         stmt = stmt->next) {
        GenerateStatementCode(&proc->c_code, stmt);
    }
    
    AppendString(&proc->c_code,
        "    return S_OK;\n"
        "}\n");
    
    /* Compile to DLL */
    CompileToDLL(proc);
}
```

---

# Python Bytecode Compilation

## CPython Bytecode Implementation

### Python Bytecode Structure
```c
// Python code object
typedef struct {
    PyObject_HEAD
    int co_argcount;            // #arguments, except *args
    int co_posonlyargcount;     // #positional-only arguments
    int co_kwonlyargcount;      // #keyword-only arguments
    int co_nlocals;             // #local variables
    int co_stacksize;           // Stack size
    int co_flags;               // CO_* flags
    int co_firstlineno;         // First source line number
    PyObject *co_code;          // Bytecode instructions
    PyObject *co_consts;        // Constants used
    PyObject *co_names;         // Names used
    PyObject *co_varnames;      // Local variable names
    PyObject *co_freevars;      // Free variables
    PyObject *co_cellvars;      // Cell variables
    PyObject *co_filename;      // Filename
    PyObject *co_name;          // Function name
    PyObject *co_lnotab;        // Line number table
} PyCodeObject;

// Python bytecode instructions (3.11+)
enum opcode {
    /* Stack manipulation */
    POP_TOP = 1,
    ROT_TWO = 2,
    ROT_THREE = 3,
    DUP_TOP = 4,
    DUP_TOP_TWO = 5,
    ROT_FOUR = 6,
    
    /* Unary operations */
    UNARY_POSITIVE = 10,
    UNARY_NEGATIVE = 11,
    UNARY_NOT = 12,
    UNARY_INVERT = 15,
    
    /* Binary operations */
    BINARY_ADD = 23,
    BINARY_SUBTRACT = 24,
    BINARY_MULTIPLY = 25,
    BINARY_TRUE_DIVIDE = 27,
    BINARY_FLOOR_DIVIDE = 28,
    BINARY_MODULO = 29,
    BINARY_POWER = 30,
    
    /* Comparison */
    COMPARE_OP = 107,
    
    /* Jumps */
    JUMP_FORWARD = 110,
    JUMP_IF_FALSE_OR_POP = 111,
    JUMP_IF_TRUE_OR_POP = 112,
    JUMP_ABSOLUTE = 113,
    POP_JUMP_IF_FALSE = 114,
    POP_JUMP_IF_TRUE = 115,
    
    /* Loads */
    LOAD_CONST = 100,
    LOAD_NAME = 101,
    LOAD_FAST = 124,
    LOAD_CLOSURE = 135,
    LOAD_DEREF = 136,
    LOAD_GLOBAL = 116,
    LOAD_ATTR = 106,
    
    /* Stores */
    STORE_NAME = 90,
    STORE_FAST = 125,
    STORE_DEREF = 137,
    STORE_GLOBAL = 97,
    STORE_ATTR = 95,
    
    /* Functions */
    CALL_FUNCTION = 131,
    MAKE_FUNCTION = 132,
    RETURN_VALUE = 83,
    
    /* Loops */
    GET_ITER = 68,
    FOR_ITER = 93,
    BREAK_LOOP = 80,
    CONTINUE_LOOP = 119,
    
    /* Exceptions */
    RAISE_VARARGS = 130,
    SETUP_FINALLY = 122,
    POP_EXCEPT = 89,
    
    /* With statement */
    SETUP_WITH = 143,
    WITH_CLEANUP_START = 81,
    WITH_CLEANUP_FINISH = 82,
    
    /* Async */
    GET_AWAITABLE = 73,
    GET_AITER = 50,
    GET_ANEXT = 51,
    
    /* Extended args */
    EXTENDED_ARG = 144,
    
    /* Cache/optimization */
    CACHE = 0,
    PRECALL = 166,
    ADAPTIVE = 0
};

// Compile Python AST to bytecode
static PyCodeObject *
assemble(struct compiler *c, int addNone)
{
    basicblock *b, *entryblock;
    struct assembler a;
    int i, j, nblocks;
    PyCodeObject *co = NULL;
    
    /* Make sure every block is reachable */
    entryblock = NULL;
    for (b = c->u->u_blocks; b != NULL; b = b->b_list) {
        if (b->b_iused == 0) {
            continue;
        }
        if (entryblock == NULL) {
            entryblock = b;
        }
    }
    
    if (entryblock == NULL) {
        return NULL;
    }
    
    /* Optimize bytecode */
    if (optimize_cfg(c, entryblock, c->c_optimize) < 0) {
        return NULL;
    }
    
    /* Compute stack depth */
    if (compute_stack_depth(c, entryblock) < 0) {
        return NULL;
    }
    
    /* Emit bytecode */
    if (assemble_init(&a, nblocks, c->u->u_firstlineno) < 0) {
        goto error;
    }
    
    for (b = entryblock; b != NULL; b = b->b_next) {
        for (i = 0; i < b->b_iused; i++) {
            if (!assemble_emit(&a, &b->b_instr[i])) {
                goto error;
            }
        }
    }
    
    /* Create code object */
    co = makecode(c, &a);
    
error:
    assemble_free(&a);
    return co;
}

// Bytecode optimization
static int
optimize_basic_block(basicblock *bb, PyObject *consts)
{
    struct instr *inst, *last_inst;
    int i;
    
    for (i = 0; i < bb->b_iused; i++) {
        inst = &bb->b_instr[i];
        
        /* Constant folding */
        if (i >= 1) {
            last_inst = &bb->b_instr[i-1];
            
            if (last_inst->i_opcode == LOAD_CONST &&
                inst->i_opcode == LOAD_CONST) {
                
                /* Two constants loaded - can we fold? */
                if (i + 1 < bb->b_iused) {
                    struct instr *next = &bb->b_instr[i+1];
                    
                    if (next->i_opcode == BINARY_ADD ||
                        next->i_opcode == BINARY_MULTIPLY) {
                        /* Fold constants */
                        PyObject *v = PyTuple_GET_ITEM(consts, 
                                                       last_inst->i_arg);
                        PyObject *w = PyTuple_GET_ITEM(consts,
                                                       inst->i_arg);
                        PyObject *result;
                        
                        if (next->i_opcode == BINARY_ADD) {
                            result = PyNumber_Add(v, w);
                        } else {
                            result = PyNumber_Multiply(v, w);
                        }
                        
                        if (result != NULL) {
                            /* Replace with single LOAD_CONST */
                            int index = compiler_add_const(consts, result);
                            last_inst->i_arg = index;
                            
                            /* Remove redundant instructions */
                            memmove(&bb->b_instr[i], &bb->b_instr[i+2],
                                   (bb->b_iused - i - 2) * sizeof(struct instr));
                            bb->b_iused -= 2;
                            i--;
                        }
                    }
                }
            }
        }
        
        /* Peephole optimizations */
        switch (inst->i_opcode) {
            case JUMP_IF_FALSE_OR_POP:
            case JUMP_IF_TRUE_OR_POP:
                /* Optimize conditional jumps */
                if (i > 0) {
                    last_inst = &bb->b_instr[i-1];
                    if (last_inst->i_opcode == COMPARE_OP) {
                        /* Can optimize comparison + jump */
                        inst->i_opcode = (inst->i_opcode == JUMP_IF_FALSE_OR_POP) ?
                                        POP_JUMP_IF_FALSE : POP_JUMP_IF_TRUE;
                    }
                }
                break;
                
            case LOAD_GLOBAL:
                /* Check if can convert to LOAD_FAST */
                if (inst->i_arg < PyTuple_GET_SIZE(co->co_varnames)) {
                    /* Global that shadows local - use LOAD_FAST */
                    inst->i_opcode = LOAD_FAST;
                }
                break;
        }
    }
    
    return 0;
}

// Python 3.11+ adaptive bytecode
typedef struct {
    uint16_t counter;           // Execution counter
    uint16_t type_version;      // Type version for guards
} _PyAdaptiveEntry;

// Specialize bytecode at runtime
int
_PyCode_Quicken(PyCodeObject *code)
{
    _Py_CODEUNIT *instructions = (_Py_CODEUNIT *)PyBytes_AS_STRING(code->co_code);
    int len = Py_SIZE(code->co_code) / sizeof(_Py_CODEUNIT);
    
    for (int i = 0; i < len; i++) {
        int opcode = _Py_OPCODE(instructions[i]);
        
        switch (opcode) {
            case LOAD_ATTR:
                /* Replace with adaptive version */
                instructions[i] = _Py_MAKECODEUNIT(LOAD_ATTR_ADAPTIVE, 
                                                  _Py_OPARG(instructions[i]));
                break;
                
            case LOAD_GLOBAL:
                instructions[i] = _Py_MAKECODEUNIT(LOAD_GLOBAL_ADAPTIVE,
                                                  _Py_OPARG(instructions[i]));
                break;
                
            case BINARY_ADD:
                instructions[i] = _Py_MAKECODEUNIT(BINARY_ADD_ADAPTIVE, 0);
                break;
                
            case CALL_FUNCTION:
                instructions[i] = _Py_MAKECODEUNIT(CALL_ADAPTIVE,
                                                  _Py_OPARG(instructions[i]));
                break;
        }
    }
    
    return 0;
}

// JIT compilation hook (future)
typedef struct {
    PyCodeObject *code;
    void *jit_code;             // Native code
    size_t jit_size;            // Size of native code
} JitCompiledCode;

// Compile hot code to native
void *
compile_to_native(PyCodeObject *code)
{
    // This would interface with LLVM or another JIT compiler
    // Currently Python uses adaptive bytecode instead
    return NULL;
}
```

---

# Comparative Analysis

## Compilation Strategies Comparison

### Architecture Comparison Table

| Database/Language | Intermediate Form | Compilation Strategy | Optimization Level | Execution Model |
|------------------|------------------|---------------------|-------------------|-----------------|
| **FirebirdSQL** | BLR (Binary Language Representation) | Single-pass compilation to bytecode | Moderate - constant folding, dead code elimination | Stack-based interpreter |
| **PostgreSQL** | Parse Tree → Query Tree → Plan Tree | Multi-stage with rewriting | Extensive - cost-based, JIT for expressions | Tree walker with optional JIT |
| **MySQL/MariaDB** | Item tree → Prepared statements | Lazy compilation with caching | Limited - basic rewrites | Tree interpreter |
| **SQL Server** | Parse Tree → Query Plan | Aggressive optimization with plan cache | Very extensive - statistics-based, native compilation | Iterator model with native option |
| **Python** | AST → Bytecode | Single-pass with peephole optimization | Moderate - constant folding, adaptive bytecode | Stack-based VM with specialization |

### Key Design Decisions

```c
// Comparison of execution models

// 1. FirebirdSQL - Stack-based bytecode
struct BLR_Execution {
    // Pros:
    // - Compact representation
    // - Fast interpretation
    // - Easy to serialize/cache
    
    // Cons:
    // - Limited optimization opportunities
    // - Stack manipulation overhead
    
    UCHAR* bytecode;
    Stack* eval_stack;
    
    void execute() {
        while (*bytecode != blr_end) {
            switch (*bytecode++) {
                case blr_add:
                    Value b = pop(eval_stack);
                    Value a = pop(eval_stack);
                    push(eval_stack, add(a, b));
                    break;
            }
        }
    }
};

// 2. PostgreSQL - Tree walker with JIT
struct PG_Execution {
    // Pros:
    // - Flexible optimization
    // - Can JIT hot paths
    // - Good for complex expressions
    
    // Cons:
    // - Higher memory usage
    // - Tree traversal overhead
    
    ExprState* expression;
    
    Datum execute() {
        if (expression->jit_compiled) {
            return expression->jit_func();
        }
        
        for (int i = 0; i < expression->steps_len; i++) {
            ExprEvalStep* step = &expression->steps[i];
            switch (step->opcode) {
                case EEOP_FUNCEXPR:
                    step->resvalue = step->d.func.fn_addr(
                        step->d.func.fcinfo_data);
                    break;
            }
        }
        return expression->resvalue;
    }
};

// 3. SQL Server - Native compilation
struct MSSQL_Execution {
    // Pros:
    // - Native performance
    // - No interpretation overhead
    // - Excellent for hot procedures
    
    // Cons:
    // - Compilation overhead
    // - Memory usage for compiled code
    // - Limited flexibility
    
    union {
        QUERY_PLAN* interpreted_plan;
        void (*native_func)(EXEC_CONTEXT*);
    };
    bool is_native;
    
    void execute(EXEC_CONTEXT* ctx) {
        if (is_native) {
            native_func(ctx);
        } else {
            execute_plan_tree(interpreted_plan, ctx);
        }
    }
};

// 4. Python - Adaptive bytecode
struct Python_Execution {
    // Pros:
    // - Runtime specialization
    // - Good cache locality
    // - Balances compilation time vs performance
    
    // Cons:
    // - Warmup time needed
    // - Complex implementation
    
    _Py_CODEUNIT* bytecode;
    _PyAdaptiveEntry* cache;
    
    PyObject* execute() {
        for (int i = 0; i < code_length; i++) {
            int opcode = GET_OPCODE(bytecode[i]);
            
            if (IS_ADAPTIVE(opcode)) {
                if (cache[i].counter++ > THRESHOLD) {
                    specialize_instruction(&bytecode[i], &cache[i]);
                }
            }
            
            dispatch_instruction(opcode, GET_ARG(bytecode[i]));
        }
    }
};
```

---

# Optimization Strategies

## Common Optimization Techniques

### 1. Constant Folding and Propagation
```c
// All systems implement this differently

// FirebirdSQL - During BLR generation
if (node->type == NODE_ADD &&
    node->left->type == NODE_LITERAL &&
    node->right->type == NODE_LITERAL) {
    
    // Fold at compile time
    int result = node->left->value + node->right->value;
    stuff_byte(gen, blr_literal);
    stuff_int(gen, result);
} else {
    // Generate normal add
    GEN_expression(gen, node->left);
    GEN_expression(gen, node->right);
    stuff_byte(gen, blr_add);
}

// PostgreSQL - During planning
if (IsA(node, OpExpr)) {
    OpExpr *op = (OpExpr *)node;
    
    if (list_length(op->args) == 2) {
        Node *left = linitial(op->args);
        Node *right = lsecond(op->args);
        
        if (IsA(left, Const) && IsA(right, Const)) {
            // Evaluate at plan time
            Datum result = OidFunctionCall2(op->opfuncid,
                                          ((Const *)left)->constvalue,
                                          ((Const *)right)->constvalue);
            return (Node *)makeConst(..., result);
        }
    }
}
```

### 2. Common Subexpression Elimination
```c
// SQL Server approach
typedef struct CSE_Entry {
    EXPRESSION* expr;
    TEMP_VAR* temp_var;
    int use_count;
} CSE_Entry;

void eliminate_common_subexpressions(QUERY_PLAN* plan) {
    HashMap* expr_map = create_hashmap();
    
    // First pass - identify common subexpressions
    traverse_plan(plan, [&](EXPRESSION* expr) {
        uint64_t hash = hash_expression(expr);
        CSE_Entry* entry = hashmap_get(expr_map, hash);
        
        if (entry && expressions_equal(entry->expr, expr)) {
            entry->use_count++;
        } else {
            entry = allocate_cse_entry(expr);
            hashmap_put(expr_map, hash, entry);
        }
    });
    
    // Second pass - replace with temp variables
    traverse_plan(plan, [&](EXPRESSION** expr_ptr) {
        uint64_t hash = hash_expression(*expr_ptr);
        CSE_Entry* entry = hashmap_get(expr_map, hash);
        
        if (entry && entry->use_count > 1) {
            if (!entry->temp_var) {
                // Create compute scalar for temp
                entry->temp_var = create_temp_var();
                insert_compute_scalar(plan, entry->temp_var, entry->expr);
            }
            
            // Replace with temp var reference
            *expr_ptr = create_var_reference(entry->temp_var);
        }
    });
}
```

### 3. Adaptive/Specialization Strategy
```c
// Python-style adaptive optimization
typedef struct {
    uint16_t opcode;
    uint16_t arg;
    uint16_t counter;
    uint16_t type_cache;
} AdaptiveInstruction;

void specialize_load_attr(AdaptiveInstruction* instr, PyObject* obj) {
    PyTypeObject* type = Py_TYPE(obj);
    
    if (type == &PyDict_Type) {
        // Specialize for dictionary
        instr->opcode = LOAD_ATTR_DICT;
        instr->type_cache = type->tp_version_tag;
    } else if (type->tp_dictoffset == 0) {
        // No __dict__, use slot
        int offset = get_slot_offset(type, instr->arg);
        if (offset >= 0) {
            instr->opcode = LOAD_ATTR_SLOT;
            instr->type_cache = offset;
        }
    } else {
        // Generic instance attribute
        instr->opcode = LOAD_ATTR_INSTANCE;
        instr->type_cache = type->tp_version_tag;
    }
}

// Apply to SQL execution
typedef struct {
    SQL_OPCODE opcode;
    union {
        struct { int column_index; } project;
        struct { COMPARISON_FUNC func; } compare;
        struct { JOIN_METHOD method; } join;
    } specialized;
    uint32_t execution_count;
    uint32_t last_cardinality;
} AdaptiveOperator;

void adapt_join_method(AdaptiveOperator* op, 
                      int left_rows, int right_rows) {
    if (op->execution_count++ < WARMUP_THRESHOLD) {
        return;
    }
    
    // Choose join method based on actual cardinalities
    if (left_rows < 100 && right_rows < 100) {
        op->specialized.join.method = NESTED_LOOP;
    } else if (right_rows > left_rows * 10) {
        op->specialized.join.method = HASH_JOIN;
    } else if (is_sorted(left) && is_sorted(right)) {
        op->specialized.join.method = MERGE_JOIN;
    }
    
    op->last_cardinality = left_rows * right_rows;
}
```

### 4. JIT Compilation Strategy
```c
// PostgreSQL-style expression JIT
typedef struct JitContext {
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMValueRef function;
    
    // Type cache
    LLVMTypeRef int32_type;
    LLVMTypeRef int64_type;
    LLVMTypeRef float_type;
    LLVMTypeRef bool_type;
} JitContext;

LLVMValueRef compile_expression(JitContext* jit, ExprNode* expr) {
    switch (expr->type) {
        case T_Const:
            return LLVMConstInt(jit->int64_type, 
                              ((Const*)expr)->constvalue, false);
            
        case T_Var:
            return LLVMBuildLoad(jit->builder,
                               get_var_pointer(jit, ((Var*)expr)->varno),
                               "var");
            
        case T_OpExpr: {
            OpExpr* op = (OpExpr*)expr;
            LLVMValueRef left = compile_expression(jit, linitial(op->args));
            LLVMValueRef right = compile_expression(jit, lsecond(op->args));
            
            switch (op->opno) {
                case INT4_ADD_OP:
                    return LLVMBuildAdd(jit->builder, left, right, "add");
                case INT4_MUL_OP:
                    return LLVMBuildMul(jit->builder, left, right, "mul");
                case INT4_EQ_OP:
                    return LLVMBuildICmp(jit->builder, LLVMIntEQ, 
                                        left, right, "eq");
            }
        }
    }
}

// Compile hot procedure to native code
void* jit_compile_procedure(StoredProcedure* proc) {
    JitContext jit;
    
    // Initialize LLVM
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    
    jit.module = LLVMModuleCreateWithName(proc->name);
    jit.builder = LLVMCreateBuilder();
    
    // Create function
    LLVMTypeRef func_type = LLVMFunctionType(
        LLVMInt32Type(), NULL, 0, false);
    jit.function = LLVMAddFunction(jit.module, proc->name, func_type);
    
    // Create entry block
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(jit.function, "entry");
    LLVMPositionBuilderAtEnd(jit.builder, entry);
    
    // Compile procedure body
    for (Statement* stmt = proc->body; stmt; stmt = stmt->next) {
        compile_statement(&jit, stmt);
    }
    
    // Optimize
    LLVMPassManagerRef pm = LLVMCreateFunctionPassManagerForModule(jit.module);
    LLVMAddConstantPropagationPass(pm);
    LLVMAddInstructionCombiningPass(pm);
    LLVMAddReassociatePass(pm);
    LLVMAddGVNPass(pm);
    LLVMAddCFGSimplificationPass(pm);
    LLVMRunFunctionPassManager(pm, jit.function);
    
    // JIT compile
    LLVMExecutionEngineRef engine;
    LLVMCreateJITCompilerForModule(&engine, jit.module, 2, NULL);
    
    return LLVMGetFunctionAddress(engine, proc->name);
}
```

## Recommendations for AI Implementation

### Optimal Strategy Selection

```c
typedef enum {
    BYTECODE_STACK,      // Simple, compact (like BLR)
    BYTECODE_REGISTER,   // Faster but complex (like Lua)
    TREE_WALKER,         // Flexible, good for complex ops (like PostgreSQL)
    HYBRID_ADAPTIVE,     // Runtime specialization (like Python 3.11)
    JIT_COMPILED        // Maximum performance (like SQL Server native)
} ExecutionStrategy;

ExecutionStrategy choose_strategy(
    QueryCharacteristics* characteristics)
{
    // For simple OLTP queries
    if (characteristics->is_simple_oltp) {
        return BYTECODE_STACK;  // Fast interpretation
    }
    
    // For complex analytical queries
    if (characteristics->has_complex_expressions ||
        characteristics->has_window_functions) {
        return TREE_WALKER;  // Better for complex operations
    }
    
    // For frequently executed procedures
    if (characteristics->execution_frequency > JIT_THRESHOLD) {
        return JIT_COMPILED;  // Amortize compilation cost
    }
    
    // For varied workloads
    if (characteristics->workload_varies) {
        return HYBRID_ADAPTIVE;  // Adapt to actual usage
    }
    
    // Default
    return BYTECODE_STACK;
}

// Implementation recommendation structure
typedef struct {
    // Parsing strategy
    bool use_recursive_descent;  // Simple, predictable
    bool use_parser_generator;   // Complex grammars
    
    // IR design
    bool use_ssa_form;          // Better optimization
    bool use_continuation_passing; // Better for async
    
    // Optimization level
    int constant_folding;        // Always beneficial
    int dead_code_elimination;   // Usually worth it
    int inline_expansion;        // For small functions
    int loop_optimizations;      // For analytical queries
    
    // Execution model
    bool support_vectorization;  // For batch processing
    bool support_parallelism;    // For large queries
    bool support_streaming;      // For large results
    
} ImplementationStrategy;
```

This comprehensive comparison shows that:

1. **FirebirdSQL's BLR** is simple and efficient for OLTP workloads
2. **PostgreSQL's multi-stage** approach excels at complex queries
3. **MySQL's prepared statements** optimize for repeated execution
4. **SQL Server's native compilation** provides maximum performance
5. **Python's adaptive bytecode** balances compilation time vs runtime performance

For an AI implementation, I'd recommend starting with a bytecode approach like BLR for simplicity, then adding adaptive optimization and JIT compilation for hot paths as the system matures.