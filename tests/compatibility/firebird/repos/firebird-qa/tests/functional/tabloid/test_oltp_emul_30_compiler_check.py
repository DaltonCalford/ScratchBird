#coding:utf-8

"""
ID:          tabloid.oltp-emul-30-compiler-check
TITLE:       Compiler check. Test ability to compile source code of OLTP-EMUL test.
DESCRIPTION: 
FBTEST:      functional.tabloid.oltp_emul_30_compiler_check
"""

import pytest
from firebird.qa import *

substitutions = [('start at .*', 'start at'), ('finish at .*', 'finish at')]

db = db_factory()

test_script = """
-- This test was created only for daily checking of FB compiler: there were several times
-- in the past when DDL of OLTP-EMUL test could not be compiled because of regressions.
-- Discuss with dimitr: letter for 11-apr-2016 15:34.

-- ##############################
-- Begin of script oltp30_DDL.sql
-- ##############################
-- ::: nb-1 ::: Required FB version: 3.0 and above.
-- ::: nb-2 ::: Use '-nod' switch when run this script from isql
set term ^;
execute block as
begin
  begin
    execute statement 'recreate exception ex_exclusive_required ''At least one concurrent connection detected.''';
    when any do begin end
  end
  begin
    execute statement 'recreate exception ex_not_suitable_fb_version ''This script requires at least Firebird 3.x version''';
    when any do begin end
  end
end
^
set term ;^
COMMIT;

set bail on;
set autoddl off;
set list on;
select 'oltp30_DDL.sql start at ' || current_timestamp as msg from rdb$database;
set list off;


set term ^;
execute block as
begin
    if ( rdb$get_context('SYSTEM','ENGINE_VERSION') starting with '2.' ) then
    begin
        exception ex_not_suitable_fb_version;
    end

    -- NB. From doc/README.monitoring_tables:
    -- columns MON$REMOTE_PID and MON$REMOTE_PROCESS contains non-NULL values
    -- only if the client library has version 2.1 or higher
    -- column MON$REMOTE_PROCESS can contain a non-pathname value
    -- if an application has specified a custom process name via DPB
    if ( exists( select * from mon$attachments a 
                 where a.mon$attachment_id<>current_connection 
                 and a.mon$remote_protocol is not null
                ) 
       ) then
    begin
        exception ex_exclusive_required;
    end
end
^
set term ;^


-- ############################################################################
-- #########################    C L E A N I N G   #############################
-- ############################################################################

-- 1. Separate EB for devastation views with preserving their column names
-- (otherwise can get ISC error 336397288. invalid request BLR at offset 2. context not defined (BLR error).)
-- see letter to dimitr, 29.03.2014 22:43
set term ^;
execute block as
  declare stt varchar(8190);
  declare ref_name varchar(31);
  declare tab_name varchar(31);
  declare view_ddl varchar(8190);
  declare c_view cursor for (
    with
    a as(
      select rf.rdb$relation_name view_name, rf.rdb$field_position fld_pos,  trim(rf.rdb$field_name) fld_name
      --rf.rdb$field_name fld_name
      from rdb$relation_fields rf
      join rdb$relations rr
      on rf.rdb$relation_name=rr.rdb$relation_name
      where
      coalesce(rf.rdb$system_flag,0)=0 and coalesce(rr.rdb$system_flag,0)=0 and rr.rdb$relation_type=1
    )
    select view_name,
           cast( 'create or alter view '||trim(view_name)||' as select '
                 ||list( fld_pos||' '||trim(lower(  fld_name  )) )
                 ||' from rdb$database' as varchar(8190)
               ) view_ddl
    from a
    group by view_name
  );
begin
  open c_view;
  while (1=1) do
  begin
    fetch c_view into tab_name, stt;
    if (row_count = 0) then leave;
    execute statement (:stt);
  end
  close c_view;
end^
set term ;^
COMMIT;

-------------------------------------------------------------------------------

-- 2. Removing all objects from database is they exists:
set term ^;
execute block as
  declare stt varchar(512);
  declare ref_name varchar(31);
  declare tab_name varchar(31);
  --declare view_ddl varchar(8190);

  declare c_trig cursor for
    (select rt.rdb$trigger_name
       from rdb$triggers rt
       where coalesce(rt.rdb$system_flag,0)=0
    );

  declare c_view cursor for
    (select rr.rdb$relation_name
       from rdb$relations rr
      where rr.rdb$relation_type=1 and coalesce(rr.rdb$system_flag,0)=0
    );
  declare c_func cursor for
    (select rf.rdb$function_name
       from rdb$functions rf
      where coalesce(rf.rdb$system_flag,0)=0
    );
  declare c_proc cursor for
    (select rp.rdb$procedure_name
       from rdb$procedures rp
       where coalesce(rp.rdb$system_flag,0)=0
    );
  
  declare c_excp cursor for
    (select re.rdb$exception_name
       from rdb$exceptions re
       where coalesce(re.rdb$system_flag,0)=0
    );
  
  declare c_fk cursor for
    (select rc.rdb$constraint_name, rc.rdb$relation_name
       from rdb$relation_constraints rc
      where rc.rdb$constraint_type ='FOREIGN KEY'
    );
  
  declare c_tabs cursor for -- fixed tables and GTTs
    (select rr.rdb$relation_name
       from rdb$relations rr
      where rr.rdb$relation_type in(0,4,5) and coalesce(rr.rdb$system_flag,0)=0
      -- todo: what about external tables ?
    );
  
  declare c_doms cursor for -- domains
    (select rf.rdb$field_name
      from rdb$fields rf
     where coalesce(rf.rdb$system_flag,0)=0
           and rf.rdb$field_name not starting with 'RDB$'
    );
  
  declare c_coll cursor for -- collations
    (select rc.rdb$collation_name
       from rdb$collations rc
      where coalesce(rc.rdb$system_flag,0)=0
    );
  declare c_gens cursor for -- generators
    (select rg.rdb$generator_name
      from rdb$generators rg
     where coalesce(rg.rdb$system_flag,0)=0
    );
  declare c_role cursor for -- roles
    (select rr.rdb$role_name
      from rdb$roles rr
     where coalesce(rr.rdb$system_flag,0)=0
    );

begin

  -- ################   D R O P   T R I G G E R S  ################
  open c_trig;
  while (1=1) do
  begin
    fetch c_trig into stt;
    if (row_count = 0) then leave;
    stt = 'drop trigger '||stt;
    execute statement (:stt);
  end
  close c_trig;

  -- #########    Z A P   F U N C S    &    P R O C S  ##########
  -- not needed views has been already "zapped", see above separate EB

  open c_func;
  while (1=1) do
  begin
    fetch c_func into stt;
    if (row_count = 0) then leave;
    stt = 'create or alter function '||stt||' returns int as begin return 1; end';
    execute statement (:stt);
  end
  close c_func;

  open c_proc;
  while (1=1) do
  begin
    fetch c_proc into stt;
    if (row_count = 0) then leave;
    stt = 'create or alter procedure '||stt||' as begin end';
    execute statement (:stt);
  end
  close c_proc;

  -- ######################   D R O P    O B J E C T S   ######################

  open c_view;----------------------  d r o p   v i e w s  ---------------------
  while (1=1) do
  begin
    fetch c_view into stt;
    if (row_count = 0) then leave;
    stt = 'drop view '||stt;
    execute statement (:stt);
  end
  close c_view;

  open c_func; --------------------  d r o p   f u c t i o n s  ----------------
  while (1=1) do
  begin
    fetch c_func into stt;
    if (row_count = 0) then leave;
    stt = 'drop function '||stt;
    execute statement (:stt);
  end
  close c_func;

  open c_proc; -----------------  d r o p   p r o c e d u r e s  ---------------
  while (1=1) do
  begin
    fetch c_proc into stt;
    if (row_count = 0) then leave;
    stt = 'drop procedure '||stt;
    execute statement (:stt);
  end
  close c_proc;

  open c_excp; -----------------  d r o p   e x c e p t i o n s  ---------------
  while (1=1) do
  begin
    fetch c_excp into stt;
    if (row_count = 0) then leave;
    stt = 'drop exception '||stt;
    execute statement (:stt);
  end
  close c_excp;

  open c_fk; -----------  d r o p    r e f.   c o n s t r a i n t s ------------
  while (1=1) do
  begin
    fetch c_fk into ref_name, tab_name;
    if (row_count = 0) then leave;
    stt = 'alter table '||tab_name||' drop constraint '||ref_name;
    execute statement (:stt);
  end
  close c_fk;

  open c_tabs; -----------  d r o p    t a b l e s  ------------
  while (1=1) do
  begin
    fetch c_tabs into stt;
    if (row_count = 0) then leave;
    stt = 'drop table '||stt;
    execute statement (:stt);
  end
  close c_tabs;

  open c_doms; -------------------  d r o p    d o m a i n s -------------------
  while (1=1) do
  begin
    fetch c_doms into stt;
    if (row_count = 0) then leave;
    stt = 'drop domain '||stt;
    execute statement (:stt);
  end
  close c_doms;

  open c_coll; ---------------  d r o p    c o l l a t i o n s -----------------
  while (1=1) do
  begin
    fetch c_coll into stt;
    if (row_count = 0) then leave;
    stt = 'drop collation '||stt;
    execute statement (:stt);
  end
  close c_coll;

  open c_gens; -----------------  d r o p    s e q u e n c e s -----------------
  while (1=1) do
  begin
    fetch c_gens into stt;
    if (row_count = 0) then leave;
    stt = 'drop sequence '||stt;
    execute statement (:stt);
  end
  close c_gens;

  open c_role; --------------------  d r o p    r o l e s ----------------------
  while (1=1) do
  begin
    fetch c_role into stt;
    if (row_count = 0) then leave;
    stt = 'drop role '||stt;
    execute statement (:stt);
  end
  close c_role;

end
^
set term ;^



-------------------------------------------------------------------------------
-- #########################    C R E A T I N G   #############################
-------------------------------------------------------------------------------

create sequence g_common;
create sequence g_doc_data;
create sequence g_perf_log;
create sequence g_init_pop;
create sequence g_qdistr;
create sequence g_success_counter; -- used in .bat / .sh for displaying estimated performance value
create sequence g_stop_test; -- serves as signal to self-stop every ISQL attachment its job


-- create collations:
create collation name_coll for utf8 from unicode case insensitive;
create collation nums_coll for utf8 from unicode case insensitive 'NUMERIC-SORT=1';


-- create exceptions
recreate exception ex_context_var_not_found 'required context variable(s): @1 - not found or has invalid value';
recreate exception ex_bad_working_mode_value 'db-level trigger TRG_CONNECT: no found rows for settings.working_mode=''@1'', correct it!';
recreate exception ex_bad_argument 'argument @1 passed to unit @2 is invalid';
recreate exception ex_test_cancellation 'test_has_been_cancelled (either by adding text into external text file ''stoptest'' or by changing value of sequence ''g_stop_test'' to non-zero)';

recreate exception ex_record_not_found 'required record not found, datasource: @1, key: @2';
recreate exception ex_cant_lock_semaphore_record 'can`t lock record in SEMAPHORES table for serialization';
recreate exception ex_cant_lock_row_for_qdistr 'can`t lock any row in `qdistr`: optype=@1, ware_id=@2, qty_required=@3';
recreate exception ex_cant_find_row_for_qdistr 'no rows found for FIFO-distribution: optype=@1, rows in tmp$shopping_cart=@2';

recreate exception ex_no_doc_found_for_handling 'no document found for handling in datasource = ''@1'' with id=@2';
recreate exception ex_no_rows_in_shopping_cart 'shopping_cart is empty, check source ''@1''';

recreate exception ex_not_all_storned_rows_removed 'at least one storned row found in ''qstorned'' table, doc=@1'; -- 4debug, 27.06.2014
recreate exception ex_neg_remainders_encountered 'at least one neg. remainder, ware_id: @1, info: @2'; -- 4debug, 27.06.2014
recreate exception ex_mism_doc_data_qd_qs 'at least one mismatch btw doc_data.id=@1 and qdistr+qstorned: qty=@2, qd_cnt=@3, qs_cnt=@4'; -- 4debug, 07.08.2014
recreate exception ex_orphans_qd_qs_found 'at least one row found for DELETED doc id=@1, snd_id=@2: @3.id=@4';

recreate exception ex_can_not_lock_any_record 'no records could be locked in datasource = ''@1'' with ID >= @2.';
recreate exception ex_can_not_select_random_id 'no id @1 @2 in @3 found within scope @4 ... @5'; -- @1 is '>=' or '<='; @2 = random_selected_value; @3 = data source; @4 = min; @5 = max

recreate exception ex_snapshot_isolation_required 'operation must run only in TIL = SNAPSHOT.';
recreate exception ex_read_committed_isolation_req 'operation must run only in TIL = READ COMMITTED.';
recreate exception ex_nowait_or_timeout_required 'transaction must start in NO WAIT mode or with LOCK_TIMEOUT.';

recreate exception ex_update_operation_forbidden 'update operation not allowed on table @1';
recreate exception ex_delete_operation_forbidden 'delete operation not allowed on table @1 when user-data exists';
recreate exception ex_debug_forbidden_operation 'debug: operation not allowed';



-------------------------------------------------------------------------------

-- create domains
-- ::: NB::: 08.06.2014:
-- not null constraints were taken out due to http://tracker.firebirdsql.org/browse/CORE-4453
create domain dm_dbkey as char(8) character set octets; -- do NOT: not null; -- for variables stored rdb$db_key
create domain dm_ids as smallint; -- IDs for operations and document states (will be assigned explicitly in 'oltp_main_filling.sql' to small values)
create domain dm_idb as bigint; -- not null; -- all IDs

create domain dm_ctxns as varchar(16) character set utf8 check( value in ('','USER_SESSION','USER_TRANSACTION', 'SYSTEM'));
create domain dm_ctxnv as varchar(80) character set none; -- m`on$context_variables.m`on$variable_name
create domain dm_dbobj as varchar(31) character set unicode_fss;
create domain dm_setting_value as varchar(160) character set utf8 collate name_coll; -- special for settings.value field (long lists can be there)
create domain dm_mcode as varchar(3) character set utf8 collate name_coll; -- optypes.mcode: mnemonic for operation
create domain dm_name as varchar(80) character set utf8 collate name_coll; -- character set utf8 not null collate name_coll; -- name of wares, contragents et al
create domain dm_nums as varchar(20) character set utf8 collate nums_coll; -- character set utf8 not null collate nums_coll; -- original (manufacturer) numbers
create domain dm_qty as numeric(12,3) check(value>=0); -- not null check(value>=0);
create domain dm_qtz as numeric(12,3) default 0 check(value>=0); -- default 0 not null check(value>=0);
create domain dm_cost as numeric(12,2); -- temply dis 15.05.2014 for DEBUG! uncomment later:   not null check(value>=0);
create domain dm_vals as numeric(12,2); -- numeric(12,2) not null; -- money_turnover_log.costXXXX, can be < 0
create domain dm_aux as double precision;
create domain dm_sign as smallint default 0 check(value in(-1, 1, 0)) ; -- smallint default 0 not null  check(value in(-1, 1, 0)) ;
create domain dm_account_type as varchar(1) character set utf8 NOT null check( value in('1','2','i','o','c','s') ); -- incoming; outgoing; payment

create domain dm_unit varchar(80);
create domain dm_info varchar(255);
create domain dm_stack varchar(512);
-- Remote address to be written into perf_log, mon_log. 
-- Size should be enough to fit IPv6 and port number or even port text mnemona! 
-- See: http://www.networksorcery.com/enp/protocol/ip/ports04000.htm
-- See also reply from dimitr, letter 11-jan-2016 16:04 (subj: "SOS. M`ON$REMOTE_ADDRESS, ...")
-- Fixed in http://sourceforge.net/p/firebird/code/62802 (only port numbers will serve as "suffixes")
create domain dm_ip varchar(255);



-------------------------------------------------------------------------------
---- aux table for auto stop running attaches:
--recreate table ext_stoptest external 'stoptest.txt' (
--  s char(2)
--);

-------------------------------------------------------------------------------
--  **************   G L O B A L    T E M P.    T A B L E S   *****************
-------------------------------------------------------------------------------
recreate global temporary table tmp$shopping_cart(
   id dm_idb, --  = ware_id
   snd_id bigint, -- nullable! ref to invoice in case when create reserve doc (one invoice for each reserve; 03.06.2014)
   qty numeric(12,3) not null,
   optype_id bigint,
   snd_optype_id bigint,
   rcv_optype_id bigint,
   storno_sub smallint default 1, -- see table rules_for_qistr.storno_sub
   qty_bak numeric(12,3) default 0, -- debug
   dup_cnt int default 0,  -- debug
   cost_purchase dm_cost, -- for sp_sp_fill_shopping_cart when create client order
   cost_retail dm_cost,
   constraint tmp_shopcart_unq unique(id, snd_id) using index tmp_shopcart_unq
) on commit delete rows;

-- 08.01.2015, see sp make_qty_storno, investigatin perf. for NL vs MERGE
--create index tmp_shopcart_rcv_op on tmp$shopping_cart(rcv_optype_id);
--

recreate global temporary table tmp$dep_docs(
  base_doc_id dm_idb,
  dependend_doc_id dm_idb,
  dependend_doc_state dm_idb,
  dependend_doc_dbkey dm_dbkey,
  dependend_doc_agent_id dm_idb,
  -- 29.07.2014 (4 misc debug)
  ware_id dm_idb,
  base_doc_qty dm_qty,
  dependend_doc_qty dm_qty,
  constraint tmp_dep_docs_unq unique(base_doc_id, dependend_doc_id) using index tmp_dep_docs_unq
) on commit delete rows;


recreate global temporary table tmp$result_set(
    snd_id bigint,
    id bigint, 
    storno_sub smallint,
    qdistr_id bigint, 
    qdistr_dbkey dm_dbkey,
    doc_id bigint,
    optype_id bigint,
    oper varchar(80),
    base_doc_id bigint,
    doc_data_id bigint,
    ware_id bigint,
    qty numeric(12,3),
    cost_purchase numeric(12,2),
    cost_retail numeric(12,2),
    qty_clo numeric(12,3),
    qty_clr numeric(12,3),
    qty_ord numeric(12,3),
    qty_sup numeric(12,3),
    qty_inc numeric(12,3),
    qty_avl numeric(12,3),
    qty_res numeric(12,3),
    qty_out numeric(12,3),
    cost_inc numeric(12,2),
    cost_out numeric(12,2),
    qty_acn numeric(12,3),
    cost_acn numeric(12,2),
    state_id bigint,
    agent_id bigint,
    dts_edit timestamp,
    dts_open timestamp,
    dts_fix timestamp,
    dts_clos timestamp,
    state bigint
) on commit delete rows;

create index tmp_result_set_ware_doc on tmp$result_set(ware_id, doc_id);
create index tmp_result_set_doc on tmp$result_set(doc_id);

-- for materializing temp results in some report SPs:
recreate global temporary table tmp$perf_mon(
    unit dm_name,
    cnt_all int,
    cnt_ok int,
    cnt_err int,
    err_prc numeric(6,2),
    ok_min_ms int,
    ok_max_ms int,
    ok_avg_ms int,
    cnt_deadlock int,
    cnt_upd_conf int,
    cnt_lk_confl int,
    cnt_user_exc int,
    cnt_chk_viol int,
    cnt_no_valid int,
    cnt_unq_viol int,
    cnt_fk_viol int,
    cnt_stack_trc int, -- 335544842, 'stack_trace': appears at the TOP of stack in 3.0 SC (strange!)
    cnt_zero_gds int, -- core-4565 (gdscode=0 in when-section! 3.0 SC only)
    cnt_other_exc int,
    first_done timestamp,
    last_done timestamp,
    rollup_level smallint,
    dts_beg timestamp,
    dts_end timestamp
) on commit delete rows;


recreate global temporary table tmp$idx_recalc(
  tab_name dm_dbobj,
  idx_name dm_dbobj,
  idx_stat_befo double precision,
  idx_stat_afte double precision,
  idx_stat_diff computed by( idx_stat_afte - idx_stat_befo ),
  constraint tmp_idx_recalc_idx_name_unq unique(idx_name)
) on commit preserve rows;


recreate global temporary table tmp$mon_log( -- used in tmp_random_run.sql
    unit dm_unit
   ,fb_gdscode int
   ,att_id bigint default current_connection
   ,trn_id bigint
   -----------------------------------------
   ,pg_reads bigint
   ,pg_writes bigint
   ,pg_fetches bigint
   ,pg_marks bigint
   ,rec_inserts bigint
   ,rec_updates bigint
   ,rec_deletes bigint
   ,rec_backouts bigint
   ,rec_purges bigint
   ,rec_expunges bigint
   ,rec_seq_reads bigint
   ,rec_idx_reads bigint
   ---------- counters avaliable only in FB 3.0, since rev. 59953 --------------
   ,rec_rpt_reads bigint -- <<< since rev. 60005, 27.08.2014 18:52
   ,bkv_reads bigint -- mon$backversion_reads, since rev. 60012, 28.08.2014 19:16
   -- since rev. 59953, 05.08.2014 08:46:
   ,frg_reads bigint
   ,rec_locks bigint
   ,rec_waits bigint
   ,rec_confl bigint
   -----------------------------------------------------------------------------
   ,mem_used bigint
   ,mem_alloc bigint
   ,stat_id bigint
   ,server_pid bigint
   ,mult dm_sign
   ,add_info dm_info
   ,dts timestamp default 'now'
   ,rowset bigint -- for grouping records that related to the same measurement
) on commit preserve rows;


-- 29.08.2014: for identifying troubles by analyzing results per each table:
recreate global temporary table tmp$mon_log_table_stats( -- used in tmp_random_run.sql
    unit dm_unit
   ,fb_gdscode int
   ,att_id bigint default current_connection
   ,trn_id bigint
   ,table_id smallint
   ,table_name dm_dbobj  -- filled in SP srv_fill_mon
   -------------------
   ,rec_inserts bigint
   ,rec_updates bigint
   ,rec_deletes bigint
   ,rec_backouts bigint
   ,rec_purges bigint
   ,rec_expunges bigint
   ,rec_seq_reads bigint
   ,rec_idx_reads bigint
   ,rec_rpt_reads bigint
   ,bkv_reads bigint
   ,frg_reads bigint
   ,rec_locks bigint
   ,rec_waits bigint
   ,rec_confl bigint
   ,stat_id bigint
   ,mult dm_sign
   ,rowset bigint -- for grouping records that related to the same measurement
) on commit preserve rows;


-- Used only when config parameter create_with_split_heavy_tabs = 1:
-- stores INITIAL source code of sp_get_random_id as basis for generation
-- of new DB object with this name which almost fully excludes running of
-- dynamic SQL statements for getting random selection of IDs:
recreate global temporary table tmp$autogen$source(
    line_no int,
    text varchar(8190),
    line_type varchar(10)
) on commit delete rows;

-- Used only when config parameter create_with_split_heavy_tabs = 1: library of known
-- views which names should be written explicitly in new sp_get_random_id code:
recreate global temporary table tmp$autogen$rand$calls(
    view_name_for_find varchar(31)
    ,view_name_for_min_id varchar(31)
    ,view_name_for_max_id varchar(31)
) on commit delete rows;



-------------------------------------------------------------------------------
--  ************   D E B U G   T A B L E S (can be taken out after)  **********
-------------------------------------------------------------------------------
-- moved out, see oltp_misc_debug.sql 
-- (this script will be involved only when config parameter create_with_debug_objects = 1)

-------------------------------------------------------------------------------
--  ****************   A P P L I C A T I O N    T A B L E S   *****************
-------------------------------------------------------------------------------
-- Some values which are constant during app work, definitions for worload modes:
-- Values from 'svalue' field will be stored in session-level CONTEXT variables
-- with names defined in field 'mcode' ('C_CUSTOMER_DOC_MAX_ROWS' etc):
recreate table settings(
  working_mode varchar(20) character set utf8,
  mcode dm_name, -- mnemonic code
  context varchar(16) default 'USER_SESSION',
  svalue dm_setting_value,
  init_on varchar(20) default 'connect', -- read this value in context var in trg_connect; 'db_prepare' ==> not needed in runtime
  description dm_info,
  constraint settings_unq unique(working_mode, mcode) using index settings_mode_code,
  constraint settings_valid_ctype check(context in(null,'USER_SESSION','USER_TRANSACTION'))
);


-- lookup table: types of operations
recreate table optypes(
   id dm_ids constraint pk_optypes primary key using index pk_optypes
  ,mcode dm_mcode -- mnemonic code
  ,name dm_name
  ,m_qty_clo dm_sign -- how this op. affects on remainder "clients order"
  ,m_qty_clr dm_sign -- how this op. affects on remainder "REFUSED clients order"
  ,m_qty_ord dm_sign -- how this op. affects on remainder "stock order"
  ,m_qty_sup dm_sign -- how this op. affects on remainder "unclosed invoices from supplier"
  ,m_qty_avl dm_sign -- how this op. affects on remainder "avaliable to be reserved"
  ,m_qty_res dm_sign -- how this op. affects on remainder "in reserve for some customer"
  ,m_cost_inc computed by(iif(m_qty_avl=1,1,0)) -- see field invnt_saldo.cost_inc
  ,m_cost_out computed by(iif(m_qty_res=-1,1,0)) -- see field invnt_saldo.cost_out
  ,m_cust_debt dm_sign -- auxiliary field: affect on mutual settlements with customer
  ,m_supp_debt dm_sign -- auxiliary field: affect on mutual settlements with supplier
  -- kind of this operation: 'i' = incoming; 'o' = outgoing; 'p' = payment
  ,acn_type dm_account_type -- need for FIFO distribution
  ,multiply_rows_for_fifo dm_sign default 0
  ,end_state bigint -- (todo later) state of document after operation is completed (-1 = "not changed")
  -- operation can not change both cost_INC and cost_OUT:
  ,constraint optypes_mcode_unq unique(mcode) using index optypes_mcode_unq
  ,constraint optype_mutual_inc_out check( abs(m_cost_inc)+abs(m_cost_out) < 2 )
  ,constraint optype_mult_pay_only check(
     m_supp_debt=1 and m_cost_inc=1
     or
     m_cust_debt=1 and m_cost_out=1
     or
     m_supp_debt<=0 and m_cust_debt <=0 and (m_cost_inc=0 and m_cost_out=0)
   )
  );


-- Definitions for "value_to_rows" distributions for operations when it's needed:
recreate table rules_for_qdistr(
    mode dm_name -- 'new_doc_only' (rcv='clo'), 'distr_only' (snd='clo', rcv='res'), 'distr+new_doc' (all others)
    ,snd_optype_id dm_ids -- nullable: there is no 'sender' for client order operation (this is start of business chain)
    ,rcv_optype_id dm_ids -- nullable: there is no 'receiver' for reserve write-off (this is end of business chain)
    ,storno_sub smallint -- NB: for rcv_optype_id=3300 there will be TWO records: 1 (snd_op=2100) and 2 (snd_op=1000)
    ,constraint rules_for_qdistr_unq unique(snd_optype_id, rcv_optype_id) using index rules_for_qdistr_unq
);
-- 10.09.2014: investigate performance vs sp_rules_for_qdistr; result: join with TABLE wins.
create index rules_for_qdistr_rcvop on rules_for_qdistr(rcv_optype_id);


-- create tables without ref. constraints
-- doc headers:
recreate table doc_list(
   id dm_idb
  ,optype_id dm_ids
  ,agent_id dm_idb
  ,state_id dm_ids
  ,base_doc_id bigint -- id of document that is 'base' for current (stock order => incoming invoice etc)
  ,cost_purchase dm_cost default 0 -- total in PURCHASING cost; can be ZERO for payment from customers
  ,cost_retail dm_cost default 0 -- total in RETAIL cost; can be zero OUR PAYMENT to suppliers
  ,acn_type dm_account_type
  ,dts_open timestamp default 'now'
  ,dts_fix timestamp -- when changes of CONTENT of this document became disabled
  ,dts_clos timestamp -- when ALL changes of this doc. became disabled
  ,constraint pk_doc_list primary key(id) using index pk_doc_list
  ,constraint dts_clos_greater_than_open check(dts_clos is null or dts_clos > dts_open)
);
create descending index doc_list_id_desc on doc_list(id); -- need for quick select random doc


-- doc detailization (child for doc_list):
recreate table doc_data(
   id dm_idb not null
  ,doc_id dm_idb
  ,ware_id dm_idb
  ,qty dm_qty
  ,cost_purchase dm_cost
  ,cost_retail dm_cost default 0
  ,dts_edit timestamp -- last modification timestamp; do NOT use `default 'now'` here!
  -- PK will be removed from this table if setting 'HALT_TEST_ON_ERRORS' does NOT containing
  -- word ',PK,'. See statements in EB at the ending part of oltp_main_filling.sql:
  ,constraint pk_doc_data primary key(id) using index pk_doc_data
  ,constraint doc_data_doc_ware_unq unique(doc_id, ware_id) using index doc_data_doc_ware_unq
  ,constraint doc_data_qty_cost_both check ( qty>0 and cost_purchase>0 and cost_retail>0 or qty = 0 and cost_purchase = 0 and cost_retail=0 )
);
create descending index doc_data_id_desc on doc_data(id); -- get max in fn_g`et_random_id; 04.06.2014

-- Cost turnovers "log", by contragents + doc_id + operation types
-- (will be agregated in sp_make_cost_storno, with serialized access to this SP)
-- (NB: *not* all operations add rows in this table)
recreate table money_turnover_log(
    id dm_idb not null
   ,doc_id dm_idb
   ,agent_id dm_idb
   ,optype_id dm_ids
   ,cost_purchase dm_vals -- can be < 0 when deleting records in doc_xxx
   ,cost_retail dm_vals -- can be < 0 when deleting records in doc_xxx
   ,dts timestamp default 'now'
);
-- 27.09.2015: refactored SP srv_make_money_saldo
create index money_turnover_log_agent_optype on money_turnover_log(agent_id, optype_id);

-- Result of data aggregation of table money_turnover_log in sp_make_cost_storno
-- This table is updated only in 'serialized' mode by SINGLE attach at a time.
recreate table money_saldo(
  agent_id dm_idb constraint pk_money_saldo primary key using index pk_money_saldo,
  cost_purchase dm_vals,
  cost_retail dm_vals
);

-- lookup table: reference of wares (full price list of manufacturer)
-- This table is filled only once, at the test PREPARING phase, see oltp_data_filling.sql
recreate table wares(
   id dm_idb generated by default as identity constraint pk_wares primary key using index pk_wares
   ,group_id dm_idb
   ,numb dm_nums -- original manufacturer number, provided by supplier (for testing SIMILAR TO perfomnace)
   ,name dm_name -- name of ware (for testing SIMILAR TO perfomnace)
   ,price_purchase dm_cost -- we buy from supplier (non fixed price, can vary - sp_client_order)
   ,price_retail dm_cost -- we sale to customers (non fixed price, can vary - see sp_client_order)
   -- business logic contraint: all wares must have unique numbers:
   ,constraint wares_numb_unq unique(numb)
               using index wares_numb_unq
   );
-- aux. index for randomly selecting during emulating create docs:
create descending index wares_id_desc on wares(id);

-- aux table to check performance of similar_to (when search for STRINGS instead of IDs):
recreate table phrases(
   id dm_idb generated by default as identity constraint pk_phrases primary key using index pk_phrases
   ,pattern dm_name
   ,name dm_name
   ,constraint phrases_unq unique(pattern) using index phrases_unq
   --,constraint fk_words_wares_id foreign key (wares_id) references wares(id)
);
create index phrases_name on phrases(name);
create descending index phrases_id_desc on phrases(id);

-- catalog of views which are used in sp_get_random_id (4debug)
recreate table z_used_views( name dm_dbobj, constraint z_used_views_unq unique(name) using index z_used_views_unq);

-- Inventory registry (~agg. matview: how many wares do we have currently)
-- related 1-to-1 to table `wares`; updated periodically and only in "SERIALIZED manner", see s`rv_make_invnt_saldo
recreate table invnt_saldo(
    id dm_idb generated by default as identity constraint pk_invnt_saldo primary key using index pk_invnt_saldo
   ,qty_clo dm_qty default 0 -- amount that clients ordered us
   ,qty_clr dm_qty default 0 -- amount that was REFUSED by clients (s`p_cancel_client_order)
   ,qty_ord dm_qty default 0 -- amount that we ordered (sent to supplier)
   ,qty_sup dm_qty default 0 -- amount that supplier sent to us (specified in incoming doc)
   ,qty_avl dm_qty default 0 -- amount that is avaliable to be taken (after finish checking of incoming doc)
   ,qty_res dm_qty default 0 -- amount that is reserved for customers (for further sale)
   ,qty_inc dm_qty default 0 -- total amount of incomings
   ,qty_out dm_qty default 0 -- total amount of outgoings
   ,cost_inc dm_cost default 0 -- total cost of incomings (total on closed incoming docs)
   ,cost_out dm_cost default 0 -- total cost of outgoings (total on closed outgoing docs)
   ,qty_acn computed by(qty_avl+qty_res) -- amount "on hand" as it seen by accounter
   ,cost_acn computed by ( cost_inc - cost_out ) -- total cost "on hand"
   ,dts_edit timestamp default 'now' -- last modification timestamp
   ,constraint invnt_saldo_acn_zero check (NOT (qty_acn = 0 and cost_acn<>0 or qty_acn<>0 and cost_acn=0 ))
);


--------------------------------------------------------------------------------
-- Result of "value_to_rows" transformation of AMOUNTS (integers) in doc_data:
-- when we write doc_data.qty=5 then 5 rows with snd_qty=1 will be added to QDistr
-- (these rows will be moved from this table to QStorned during every storning
-- operation, see s`p_make_qty_storno):
recreate table qdistr(
   id dm_idb not null
  ,doc_id dm_idb -- denorm for speed, also 4debug
  ,ware_id dm_idb
  ,snd_optype_id dm_ids -- denorm for speed
  ,snd_id dm_idb -- ==> doc_data.id of "sender"
  ,snd_qty dm_qty
  ,rcv_doc_id bigint -- 30.12.2014, always null, for some debug views
  ,rcv_optype_id dm_ids
  ,rcv_id bigint -- nullable! ==> doc_data.id of "receiver"
  ,rcv_qty numeric(12,3)
  ,snd_purchase dm_cost
  ,snd_retail dm_cost
  ,rcv_purchase dm_cost
  ,rcv_retail dm_cost
  ,trn_id bigint default current_transaction
  ,dts timestamp default 'now'
);

-- 21.09.2015: DDL of indices for QDistr table depends on config parameter 'separ_qd_idx' (0 or 1),
-- definition of index key(s) see in 'oltp_create_with_split_heavy_tabs_0.sql' (for non-splitted QDistr) or in
-- 'oltp_create_with_split_heavy_tabs_1.sql' (when table QDistr is replaced with XQD* tables).
-- ................................................................................................
-- ::: nb ::: PK on this table will be REMOVED at the end of script 'oltp_main_filling.sql'
-- if setting 'LOG_PK_VIOLATION' = '0' and 'HALT_TEST_ON_ERRORS' not containing ',PK,'
alter table qdistr add  constraint pk_qdistr primary key(id) using index pk_qdistr;


-- 22.05.2014: storage for records which are removed from Qdistr when they are 'storned'
-- (will be returned back into qdistr when cancel operation or delete document, see s`p_kill_qty_storno):
recreate table qstorned(
   id dm_idb not null
  ,doc_id dm_idb -- denorm for speed
  ,ware_id dm_idb
  ,snd_optype_id dm_ids -- denorm for speed
  ,snd_id dm_idb -- ==> doc_data.id of "sender"
  ,snd_qty dm_qty
  ,rcv_doc_id dm_idb -- 30.12.2014, for enable to remove PK on doc_data, see S`P_LOCK_DEPENDENT_DOCS
  ,rcv_optype_id dm_ids
  ,rcv_id dm_idb
  ,rcv_qty dm_qty
  ,snd_purchase dm_cost
  ,snd_retail dm_cost
  ,rcv_purchase dm_cost
  ,rcv_retail dm_cost
  ,trn_id bigint default current_transaction
  ,dts timestamp default 'now'
);
create index qstorned_doc_id on qstorned(doc_id); -- confirmed 16.09.2014, see s`p_lock_dependent_docs
create index qstorned_snd_id on qstorned(snd_id); -- confirmed 16.09.2014, see s`p_kill_qty_storno
create index qstorned_rcv_id on qstorned(rcv_id); -- confirmed 16.09.2014, see s`p_kill_qty_storno
-- ::: nb ::: PK on this table will be REMOVED at the end of script 'oltp_main_filling.sql'
-- if setting 'LOG_PK_VIOLATION' = '0' and 'HALT_TEST_ON_ERRORS' not containing ',PK,'
alter table qstorned add  constraint pk_qdstorned primary key(id) using index pk_qdstorned;

-------------------------------------------------------------------------------
-- Result of "value_to_rows" transformation of COSTS in doc_list
-- for payment docs and when we change document state in:
-- s`p_add_invoice_to_stock, s`p_reserve_write_off, s`p_cancel_adding_invoice, s`p_cancel_write_off
recreate table pdistr(
    -- ::: nb ::: PK on this table will be REMOVED at the end of script 'oltp_main_filling.sql'
    -- if setting 'LOG_PK_VIOLATION' = '0' and 'HALT_TEST_ON_ERRORS' not containing ',PK,'
    id dm_idb generated by default as identity constraint pk_pdistr primary key using index pk_pdistr
    ,agent_id dm_idb
    ,snd_optype_id dm_ids -- denorm for speed
    ,snd_id dm_idb -- ==> doc_list.id of "sender"
    ,snd_cost dm_qty
    ,rcv_optype_id dm_ids
    ,trn_id bigint default current_transaction
    ,constraint pdistr_snd_op_diff_rcv_op check( snd_optype_id is distinct from rcv_optype_id )
);
create index pdistr_snd_id on pdistr(snd_id); -- for fast seek when emul cascade deleting in s`p_kill_cost_storno
-- 09.09.2014: attempt to speed-up random choise of non-paid realizations and invoices
-- plus reduce number of doc_list IRs (see v_r`andom_find_non_paid_*, v_min_non_paid_*, v_max_non_paid_*)
create index pdistr_sndop_rcvop_sndid_asc on pdistr (snd_optype_id, rcv_optype_id, snd_id); -- see plan in V_MIN_NON_PAID_xxx
create descending index pdistr_sndop_rcvop_sndid_desc on pdistr (snd_optype_id, rcv_optype_id, snd_id); -- see plan in V_MAX_NON_PAID_xxx
create index pdistr_agent_id on pdistr(agent_id); -- confirmed, 16.09.2014: see s`p_make_cost_storno


-- Storage for records which are removed from Pdistr when they are 'storned'
-- (will returns back into Pdistr when cancel operation - see sp_k`ill_cost_storno):
recreate table pstorned(
    -- ::: nb ::: PK on this table will be REMOVED at the end of script 'oltp_main_filling.sql'
    -- if setting 'LOG_PK_VIOLATION' = '0' and 'HALT_TEST_ON_ERRORS' not containing ',PK,'
    id dm_idb generated by default as identity constraint pk_pstorned primary key using index pk_pstorned
    ,agent_id dm_idb
    ,snd_optype_id dm_ids -- denorm for speed
    ,snd_id dm_idb -- ==> doc_list.id of "sender"
    ,snd_cost dm_cost
    ,rcv_optype_id dm_ids
    ,rcv_id dm_idb
    ,rcv_cost dm_cost
    ,trn_id bigint default current_transaction
    ,constraint pstorned_snd_op_diff_rcv_op check( snd_optype_id is distinct from rcv_optype_id )
);
create index pstorned_snd_id on pstorned(snd_id); -- confirmed, 16.09.2014: see s`p_kill_cost_storno
create index pstorned_rcv_id on pstorned(rcv_id); -- confirmed, 16.09.2014: see s`p_kill_cost_storno

-- Definitions for "value-to-rows" COST distribution:
recreate table rules_for_pdistr(
    mode dm_name -- 'new_doc_only' (rcv='clo'), 'distr_only' (snd='clo', rcv='res'), 'distr+new_doc' (all others)
   ,snd_optype_id dm_ids
   ,rcv_optype_id dm_ids
   ,rows_to_multiply int default 10 -- how many rows to create when new doc of 'snd_optype_id' is created
   ,constraint rules_for_pdistr_unq unique(snd_optype_id, rcv_optype_id) using index rules_for_pdistr_unq
);


-------------------------------------------------------------------------------

-- lookup table: doc_states of documents (filled manually, see oltp_main_filling.sql)
recreate table doc_states(
   id dm_ids constraint pk_doc_states primary key using index pk_doc_states
  ,mcode dm_name  -- mnemonic code
  ,name dm_name
  ,constraint doc_states_mcode_unq unique(mcode) using index doc_states_mcode_unq
  ,constraint doc_states_name_unq unique(name) using index doc_states_name_unq
);

-- lookup table: contragents:
recreate table agents(
   id dm_idb generated by default as identity constraint pk_agents primary key using index pk_agents
  ,name dm_name
  ,is_customer dm_sign default 1
  ,is_supplier dm_sign default 0
  ,is_our_firm dm_sign default 0 -- for OUR orders to supplier (do NOT make reserves after add invoice for such docs)
  ,constraint agents_mutual_excl check(  bin_xor( is_our_firm, bin_or(is_customer, is_supplier) )=1 )
  ,constraint agents_name_unq unique(name) using index agents_name_unq
);
-- aux. index for randomly selecting during emulating create docs:
create descending index agents_id_desc on agents(id);
create index agents_is_supplier on agents(is_supplier);
create index agents_is_our_firm on agents(is_our_firm);

-- groups of wares (filled only once before test, see oltp_data_filling.sql)
recreate table ware_groups(
   id dm_idb constraint pk_ware_groups primary key using index pk_ware_groups
  ,name dm_name
  ,descr blob
  ,constraint ware_groups_name_unq unique(name) using index ware_groups_name_unq
);

-- Tasks like 'make_total_saldo' which should be serialized, i.e. run only
-- in 'SINGLETONE mode' (no two attaches can run such tasks at the same time);
-- Filled in oltp_main_filling.sql
recreate table semaphores(
    id dm_idb constraint pk_semaphores primary key using index pk_semaphores
    ,task dm_name
    ,constraint semaphores_task_unq unique(task) using index semaphores_task_unq
);


-- Log for all changes in doc_data.qty (including DELETION of rows from doc_data).
-- Only INSERTION is allowed to this table for 'common' business operations.
-- Fields qty_diff & cost_diff can be NEGATIVE when document is removed ('cancelled')
-- Aggregating and deleting rows from this table - see s`rv_make_invnt_saldo
recreate table invnt_turnover_log(
    ware_id dm_idb
   ,qty_diff numeric(12,3)  -- can be < 0 when cancelling document
   ,cost_diff numeric(12,2) -- can be < 0 when cancelling document
   ,doc_list_id bigint
   ,doc_pref dm_mcode
   ,doc_data_id bigint
   ,optype_id dm_ids
   ,id dm_idb not null -- FB 3.x: do NOT `generated by default as identity`, we use bulk-getting new IDs (or trigger with gen_id) instead
   ,dts_edit timestamp default 'now' -- last modification timestamp
   ,att_id int default current_connection
   ,trn_id int default current_transaction
   -- finally dis 09.01.2015, not needed for this table: ,constraint pk_invnt_turnover_log primary key(id) using index pk_invnt_turnover_log
);
create index invnt_turnover_log_ware_dd_id on invnt_turnover_log(ware_id, doc_data_id);

-- Aux. table for random choise of app. unit to be performed and overall perfomance report.
-- see script %tmpdir%	mp_random_run.sql which is auto generated by 1run_oltp_emul.bat:
recreate table business_ops(
    unit dm_unit,
    sort_prior int unique,
    info dm_info,
    mode dm_name,
    kind dm_name,
    random_selection_weight smallint,
    constraint bo_unit unique(unit) using index bo_unit_unq
);
create index business_ops_rnd_wgth on business_ops(random_selection_weight); -- 23.07.2014


-- standard Firebird error list with descriptions:
recreate table fb_errors(
   fb_sqlcode int,
   fb_gdscode int,
   fb_mnemona varchar(31),
   fb_errtext varchar(100),
   constraint fb_errors_gds_code_unq unique(fb_gdscode) using index fb_errors_gds_code
);


-- 28.10.2015: source for view z_estimated_perf_per_minute, see oltp_isql_run_worker.bat (.sh):
-- data of estimated overall performance value with detalization to one minute, useful for
-- finding proper value of config parameter 'warm_time'. Values in the field SUCCESS_COUNT are
-- result of total count of business ops that SUCCESSFUL finished, see auto-generated script
-- $tmpdir/tmp_random_run.sql:
-- v_success_ops_increment = cast(rdb$get_context('USER_TRANSACTION', 'BUSINESS_OPS_CNT') as int);
-- result = gen_id( g_success_counter, v_success_ops_increment );
-- Context variable 'BUSINESS_OPS_CNT' is incremented by 1 on every invokation of each unit
-- that implements BUSINESS action: client order, order to supplier, etc, -- see table business_ops
recreate table perf_estimated(
    minute_since_test_start int,
    success_count numeric(12,2),
    att_id int default current_connection,
    dts timestamp default 'now'
);
create index perf_est_minute_since_start on perf_estimated (minute_since_test_start);


-- Log of parsing ISQL statistics
recreate table perf_isql_stat(
    trn_id bigint default current_transaction
    ,isql_current bigint
    ,isql_delta bigint
    ,isql_max bigint
    ,isql_elapsed numeric(12,3)
    ,isql_reads bigint
    ,isql_writes bigint
    ,isql_fetches bigint
    ,sql_state varchar(5)
);
create index perf_isql_stat_trn on perf_isql_stat(trn_id);


-- 23.12.2015 Log of parsed trace for ISQL session #1
recreate table trace_stat(
    unit dm_unit
    ,dts_end timestamp
    ,success smallint
    ,elapsed_ms int
    ,reads bigint
    ,writes bigint
    ,fetches bigint
    ,marks bigint
);

--create index trace_stat_dts_end on trace_stat(dts_end);
--create index trace_stat_unit_dts on trace_stat(unit, dts_end);

-- Log for performance and errors (filled via autonom. tx if exc`eptions occur)
recreate table perf_log(
   id dm_idb -- value from sequence where record has been added into GTT tmp$perf_log
  --,id2 bigint -- value from sequence where record has been written from tmp$perf_log into fixed table perf_log (need for debug)
  ,unit dm_unit -- name of executed SP
  ,exc_unit char(1) -- was THIS unit the place where exception raised ? yes ==> '#'
  ,fb_gdscode int -- how did finish this unit (0 = OK)
  ,trn_id bigint default current_transaction
  ,att_id int default current_connection
  ,elapsed_ms bigint -- do not make it computed_by, updating occur in single point (s`p_add_to_perf_log)
  ,info dm_info -- info for debug
  ,exc_info dm_info -- info about exception (if occured)
  ,stack dm_stack
  ,ip dm_ip  -- rdb$get_context('SYSTEM','CLIENT_ADDRESS'); for IPv6: 'FF80:0000:0000:0000:0123:1234:ABCD:EF12' - enough 39 chars
  ,dts_beg timestamp default 'now' -- current_timestamp
  ,dts_end timestamp
  ,aux1 double precision -- for srv_recalc_idx_stat: new value of index statistics
  ,aux2 double precision -- for srv_recalc_idx_stat: difference after recalc idx stat
  ,dump_trn bigint default current_transaction
);
-- finally dis 09.01.2015, not needed for this table: create index perf_log_id on perf_log(id);
create descending index perf_log_dts_beg_desc on perf_log(dts_beg);
create descending index perf_log_unit on perf_log(unit, elapsed_ms);
-- 4 some analysis, added 25.06.2014:
-- dis 20.08.2014: create index perf_log_att on perf_log(att_id);
create descending index perf_log_trn_desc on perf_log(trn_id); --  descending - for fast access to last actions, e.g. of srv_mon_idx
-- 20.08.2014:
create index perf_log_gdscode on perf_log(fb_gdscode);

-- Table to store single record for every *start* point of any app. unit.
-- When unit finishes NORMALLY (without exc.) this record is removed to fixed
-- storage (perf_log). Otherwise this table will serve as source to 'transfer'
-- uncommitted data to fixed perf_log via autonom. tx and session-level contexts
-- This results all such uncommitted data to be saved even in case of exc`eption.
recreate global temporary table tmp$perf_log(
   id dm_idb
  ,id2 bigint -- == gen_id(g_perf_log, 0) at the end of unit (when doing update)
  ,unit dm_unit
  ,exc_unit char(1)
  ,fb_gdscode int
  ,trn_id bigint default current_transaction
  ,att_id int default current_connection
  ,elapsed_ms bigint
  ,info dm_info
  ,exc_info dm_info
  ,stack dm_stack
  ,ip dm_ip
  ,dts_beg timestamp default 'now' -- current_timestamp
  ,dts_end timestamp
  ,aux1 double precision
  ,aux2 double precision
  ,dump_trn bigint default current_transaction
) on commit delete rows;
create index tmp$perf_log_unit_trn_dts_end on tmp$perf_log(unit, trn_id, dts_end);

-- introduced 09.08.2014, for checking mon$-tables stability: gather stats
-- Also used when context 'traced_units' is not empty (see s`p_add_to_perf_log).
-- see mail: SF.net SVN: firebird:[59967] firebird/trunk/src/jrd
-- (dimitr: Better (methinks) synchronization for the monitoring stuff)
recreate table mon_log(
    unit dm_unit
   ,fb_gdscode int
   ,elapsed_ms int -- added 08.09.2014
   ,trn_id bigint
   ,add_info dm_info
   --------------------
   ,rec_inserts bigint
   ,rec_updates bigint
   ,rec_deletes bigint
   ,rec_backouts bigint
   ,rec_purges bigint
   ,rec_expunges bigint
   ,rec_seq_reads bigint
   ,rec_idx_reads bigint
   ---------- counters avaliable only in FB 3.0, since rev. 59953 --------------
   ,rec_rpt_reads bigint -- <<< since rev. 60005, 27.08.2014 18:52
   ,bkv_reads bigint -- mon$backversion_reads, since rev. 60012, 28.08.2014 19:16
    -- since rev. 59953, 05.08.2014 08:46:
   ,frg_reads bigint
   -- optimal values for (letter by dimitr 27.08.2014 21:30):
   -- bkv_per_rec_reads = 1.0...1.2
   -- frg_per_rec_reads = 0.01...0.1 (better < 0.01), depending on record width; increase page size if high value
   ,bkv_per_seq_idx_rpt computed by ( 1.00 * bkv_reads / nullif((rec_seq_reads + rec_idx_reads + rec_rpt_reads),0) )
   ,frg_per_seq_idx_rpt computed by ( 1.00 * frg_reads / nullif((rec_seq_reads + rec_idx_reads + rec_rpt_reads),0) )
   ,rec_locks bigint
   ,rec_waits bigint
   ,rec_confl bigint
   -----------------------------------------------------------------------------
   ,pg_reads bigint
   ,pg_writes bigint
   ,pg_fetches bigint
   ,pg_marks bigint
   ,mem_used bigint
   ,mem_alloc bigint
   ,server_pid bigint
   ,remote_pid bigint
   ,stat_id bigint
   ,dump_trn bigint default current_transaction
   ,ip dm_ip
   ,usr dm_dbobj
   ,remote_process dm_info
   ,rowset bigint -- for grouping records that related to the same measurement
   ,att_id bigint
   ,id dm_idb generated by default as identity constraint pk_mon_log primary key using index pk_mon_log
   ,dts timestamp default 'now'
   ,sec int
);
create descending index mon_log_rowset_desc on mon_log(rowset);
create index mon_log_gdscode on mon_log(fb_gdscode);
create index mon_log_unit on mon_log(unit);
create index mon_log_dts on mon_log(dts); -- 26.09.2015, for SP srv_mon_stat_per_units


-- 29.08.2014
recreate table mon_log_table_stats(
    unit dm_unit
   ,fb_gdscode int
   ,trn_id bigint
   ,table_name dm_dbobj
   --------------------
   ,rec_inserts bigint
   ,rec_updates bigint
   ,rec_deletes bigint
   ,rec_backouts bigint
   ,rec_purges bigint
   ,rec_expunges bigint
   ,rec_seq_reads bigint
   ,rec_idx_reads bigint
   --------------------
   ,rec_rpt_reads bigint
   ,bkv_reads bigint
   ,frg_reads bigint
   ,bkv_per_seq_idx_rpt computed by ( 1.00 * bkv_reads / nullif((rec_seq_reads + rec_idx_reads + rec_rpt_reads),0) )
   ,frg_per_seq_idx_rpt computed by ( 1.00 * frg_reads / nullif((rec_seq_reads + rec_idx_reads + rec_rpt_reads),0) )
   ,rec_locks bigint
   ,rec_waits bigint
   ,rec_confl bigint
   ,stat_id bigint
   ,rowset bigint -- for grouping records that related to the same measurement
   ,table_id smallint
   ,is_system_table smallint
   ,rel_type smallint
   ,att_id bigint default current_connection
   ,id dm_idb generated by default as identity constraint pk_mon_log_table_stats primary key using index pk_mon_log_table_stats
   ,dts timestamp default 'now'
);
create descending index mon_log_table_stats_rowset on mon_log_table_stats(rowset);
create index mon_log_table_stats_gdscode on mon_log_table_stats(fb_gdscode);
create index mon_log_table_stats_tn_unit on mon_log_table_stats(table_name, unit);
create index mon_log_table_stats_dts on mon_log_table_stats(dts); -- 26.09.2015, for SP srv_mon_stat_per_tables


--------------------------------------------------------------------------------
-- # # # # #                 F O R E I G N    K E Y S                  # # # # #
--------------------------------------------------------------------------------
-- create ref constraints (NB: must be defined AFTER creation parent tables with PK/UK)
-- ::: NB ::: See about cascades caution:
-- sql.ru/forum/1081231/kaskadnoe-udalenie-pochemu-trigger-tablicy-detali-ne-vidit-master-zapisi?hl=
alter table doc_list
  add constraint fk_doc_list_agents foreign key (agent_id) references agents(id)
;

alter table doc_data
   add constraint fk_doc_data_doc_list foreign key (doc_id) references doc_list(id)
       on delete cascade
       using index fk_doc_data_doc_list
;

alter table wares
   add constraint fk_wares_ware_groups foreign key (group_id) references ware_groups(id)
;

-- do NOT: alter table money_turnover_log add constraint fk_money_turnover_log_doc_list foreign key (doc_id) references doc_list(id);
-- (documents can be deleted but it mean that NEW record in money_turnover_log appear with cost < 0!)

-- dis 28.01.2015 0135, no need:
--alter table money_turnover_log
--   add constraint fk_money_turnover_log_agents foreign key (agent_id) references agents(id)
--;



set term ^;

--------------------------------------------------------------------------------
-------    "S y s t e m"    f u n c s   &  s t o r e d     p r o c s   --------
--------------------------------------------------------------------------------

------------  P S Q L     S t o r e d    F u n c t i o n s  -----------------
-- As of current FB-3.x state, deterministic function can use internal 'cache'
-- only while some query is running. Its result is RE-CALCULATED every time when
-- 1) running new query with this func; 2) encounter every new call inside PSQL
-- see sql.ru/forum/actualutils.aspx?action=gotomsg&tid=951736&msg=12787923
create or alter function fn_infinity returns bigint deterministic as
begin
  return 9223372036854775807;
end -- fn_infinity
^

create or alter function fn_is_lock_trouble(a_gdscode int) returns boolean
as
begin
    -- lock_conflict, concurrent_transaction, deadlock, update_conflict
    return a_gdscode in (335544345, 335544878, 335544336,335544451 );
end

^ -- fn_is_lock_trouble

create or alter function fn_is_validation_trouble(a_gdscode int) returns boolean
as
begin
    -- 335544558    check_constraint    Operation violates CHECK constraint @1 on view or table @2.
    -- 335544347    not_valid    Validation error for column @1, value "@2".
    return a_gdscode in ( 335544347,335544558 );
end

^ -- fn_is_validation_trouble

create or alter function fn_is_uniqueness_trouble(a_gdscode int) returns boolean
as
begin
    -- if table has unique constraint: 335544665 unique_key_violation (violation of PRIMARY or UNIQUE KEY constraint "T1_XY" on table "T1")
    -- if table has only unique index: 335544349 no_dup (attempt to store duplicate value (visible to active transactions) in unique index "T2_XY")
    return a_gdscode in ( 335544665,335544349 );
end

^ -- fn_is_uniqueness_trouble

create or alter function fn_halt_sign(a_gdscode int) returns dm_sign
as
    declare function fn_halt_on_severe_error() returns dm_name deterministic as
    begin
        return rdb$get_context('USER_SESSION', 'HALT_TEST_ON_ERRORS');
    end
    declare result dm_sign = 0;
begin
    if ( a_gdscode  = 0 ) then result =1; -- 3.x SC & CS trouble, core-4565!
    -- refactored 13.08.2014 - see setting 'HALT_TEST_ON_ERRORS' (oltp_main_filling.sql)
    if ( result = 0 and fn_halt_on_severe_error() containing 'CK'  ) then
        result = iif( a_gdscode
                      in ( 335544347 -- not_valid    Validation error for column @1, value "@2".
                          ,335544558 -- check_constraint    Operation violates CHECK constraint @1 on view or table @2.
                         )
                     ,1
                     ,0
                    );
     if (result = 0 and fn_halt_on_severe_error() containing 'PK' ) then
         result = iif( a_gdscode
                       in ( 335544665 -- unique_key_violation (violation of PRIMARY or UNIQUE KEY constraint "T1_XY" on table "T1")
                           ,335544349 -- no_dup (attempt to store duplicate value (visible to active transactions) in unique index "T2_XY") - without UNQ constraint
                          )
                      ,1
                      ,0
                     );
     if (result = 0 and fn_halt_on_severe_error() containing 'FK' ) then
         result = iif( a_gdscode
                       in ( 335544466 -- violation of FOREIGN KEY constraint @1 on table @2
                           ,335544838 -- Foreign key reference target does not exist (when attempt to ins/upd in DETAIL table FK-field with value for which parent ID has been changed or deleted - even in uncommitted concurrent Tx)
                           ,335544839 -- Foreign key references are present for the record  (when attempt to upd/del in PARENT table PK-field and rows in DETAIL (no-cascaded!) exists for old value)
                          )
                      ,1
                      ,0
                     );

    if ( result = 0 and fn_halt_on_severe_error() containing 'ST'  ) then
        result = iif( a_gdscode = 335544842, 1, 0); -- trouble in 3.0 SC only: this error appears at the TOP of stack and this prevent following job f Tx

    return result; -- 1 ==> force test to be stopped itself

end

^ -- fn_halt_sign

create or alter function fn_remote_process returns varchar(255) deterministic as
begin
    return rdb$get_context('SYSTEM', 'CLIENT_PROCESS');
end
^

create or alter function fn_remote_address returns dm_ip deterministic as
begin
    return rdb$get_context('SYSTEM','CLIENT_ADDRESS');
end
^

create or alter function fn_is_snapshot returns boolean deterministic as
begin
    return
        fn_remote_process() containing 'IBExpert' 
        or
        rdb$get_context('SYSTEM','ISOLATION_LEVEL') is not distinct from upper('SNAPSHOT');
end
^

------ stored functions for caching data from DOC_STATES table: --------
-- ::: NB ::: as of current FB-3 state, deterministic function will re-calculate
-- it's result on EVERY NEW call of the SAME statement inside the same transaction.
-- www.sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1081535&msg=15694407
-- Also such repeating work will be done on every function call from trigger or SP.
-- So instead of access to table it's better to return value of context variable
-- which has been defined once (at 1st call of this determ. function).
-----------------------------------------------------------------------------
create or alter function fn_doc_open_state returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key type of dm_name = 'DOC_OPEN_STATE';
    declare v_stt varchar(255);
begin
    -- find id for documents state where any changes enabled:
    v_id=rdb$get_context('USER_SESSION', 'FN_DOC_OPEN_STATE');
    if (v_id is null) then begin
        v_stt='select s.id from doc_states s where s.mcode=:x';
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'DOC_STATES', v_key );
        end
        rdb$set_context('USER_SESSION', 'FN_DOC_OPEN_STATE', v_id);
    end
    
    return v_id;
end

^ -- fn_doc_open_state

create or alter function fn_doc_fix_state returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key type of dm_name = 'DOC_FIX_STATE';
    declare v_stt varchar(255);
begin
    -- find id for documents state where no change except payment enabled:
    v_id=rdb$get_context('USER_SESSION', 'FN_DOC_FIX_STATE');
    if (v_id is null) then begin
        v_stt='select s.id from doc_states s where s.mcode=:x';
        --select s.id from doc_states s where s.mcode=:v_key into v_id;
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'DOC_STATES', v_key );
        end

        rdb$set_context('USER_SESSION', 'FN_DOC_FIX_STATE', v_id);
    end

    return v_id;

end

^ -- fn_doc_fix_state

create or alter function fn_doc_clos_state returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key type of dm_name = 'DOC_CLOS_STATE';
    declare v_stt varchar(255);
begin
    -- find id for documents state where ALL changes  disabled:
    v_id=rdb$get_context('USER_SESSION', 'FN_DOC_CLOS_STATE');
    if (v_id is null) then begin
        v_stt='select s.id from doc_states s where s.mcode=:x';
        --select s.id from doc_states s where s.mcode=:v_key into v_id;
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'DOC_STATES', v_key );
        end

        rdb$set_context('USER_SESSION', 'FN_DOC_CLOS_STATE', v_id);
    end
    
    return v_id;
end -- fn_doc_clos_state

^

create or alter function fn_doc_canc_state returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key type of dm_name = 'DOC_CANC_STATE';
    declare v_stt varchar(255);
begin
    -- find id for documents state where ALL changes  disabled:
    v_id=rdb$get_context('USER_SESSION', 'FN_DOC_CANC_STATE');
    if (v_id is null) then begin
        v_stt='select s.id from doc_states s where s.mcode=:x';
        --select s.id from doc_states s where s.mcode=:v_key into v_id;
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'DOC_STATES', v_key );
        end

        rdb$set_context('USER_SESSION', 'FN_DOC_CANC_STATE', v_id);
    end
    
    return v_id;
end

^ -- fn_doc_canc_state

------ stored functions for caching data from OPTYPES table: --------

create or alter function fn_oper_order_by_customer returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "client's order".
    -- Raises ex`ception if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_ORDER_BY_CUSTOMER');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_qty_clo = :x and o.m_qty_clr = 0';
        v_key = 1;
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', v_key );
        end
        rdb$set_context('USER_SESSION', 'FN_OPER_ORDER_BY_CUSTOMER', v_id);
    end

    return v_id;

end

^ -- fn_oper_order_by_customer

create or alter function fn_oper_cancel_customer_order returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "client's order".
    -- Raises ex`ception if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_CANCEL_CUSTOMER_ORDER');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_qty_clr = :x';
        v_key = 1;
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', v_key );
        end
        rdb$set_context('USER_SESSION', 'FN_OPER_CANCEL_CUSTOMER_ORDER', v_id);
    end

    return v_id;

end

^ -- fn_oper_cancel_customer_order

create or alter function fn_oper_order_for_supplier returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "add to stock order, sent to supplier".
    -- Raises ex`ception if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_ORDER_FOR_SUPPLIER');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_qty_ord = :x';
        v_key = 1;
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', v_key );
        end
        rdb$set_context('USER_SESSION', 'FN_OPER_ORDER_FOR_SUPPLIER', v_id);
    end

    return v_id;

end

^ -- fn_oper_order_for_supplier

create or alter function fn_oper_invoice_get returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key1 dm_sign;
    declare v_key2 dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "get invoice from supplier" (invoice need to be checked)
    -- Raises ex`ception if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_INVOICE_GET');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_qty_ord=:x and o.m_qty_sup=:y';
        v_key1 = -1;
        v_key2 = 1;
        execute statement (v_stt) ( x := :v_key1, y := :v_key2 ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', 'm_qty_ord=-1 and m_qty_sup=1' );
        end
        rdb$set_context('USER_SESSION', 'FN_OPER_INVOICE_GET', v_id);
    end
    
    return v_id;

end

^  -- fn_oper_invoice_get

create or alter function fn_oper_invoice_add returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key dm_sign;
    declare v_key1 dm_sign;
    declare v_key2 dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "invoice checked and accepted" (added to avaliable remainders)
    -- Raises exc`eption if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_INVOICE_ADD');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_qty_avl=:x';
        v_key = 1;
        execute statement (v_stt)  ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', 'm_qty_sup=-1 and m_qty_avl=1' );
        end

        rdb$set_context('USER_SESSION', 'FN_OPER_INVOICE_ADD', v_id);
    end
    
    return v_id;
end

^ -- fn_oper_invoice_add 


create or alter function fn_oper_retail_reserve returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key1 dm_sign;
    declare v_key2 dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "retail selling - reserve for customer".
    -- Raises exc`eption if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_RETAIL_RESERVE');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_qty_avl=:x and o.m_qty_res=:y';
        v_key1 = -1;
        v_key2 = 1;
        execute statement (v_stt) ( x := :v_key1, y := :v_key2 ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', 'm_qty_avl=-1 and m_qty_res=1' );
        end
        rdb$set_context('USER_SESSION', 'FN_OPER_RETAIL_RESERVE', v_id);
    end
    
    return v_id;

end

^ -- fn_oper_retail_reserve

create or alter function fn_oper_retail_realization returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key1 dm_sign;
    declare v_key2 dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "retail selling - write-off (realization)".
    -- Raises exc`eption if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_RETAIL_REALIZATION');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_qty_res=:x and o.m_cost_out=:y';
        v_key1 = -1;
        v_key2 = 1;
        execute statement (v_stt) ( x := :v_key1, y := :v_key2 ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', 'm_qty_res=-1 and m_cost_out=1' );
        end
        rdb$set_context('USER_SESSION', 'FN_OPER_RETAIL_REALIZATION', v_id);
    end

    return v_id;

end

^ -- fn_oper_retail_realization

create or alter function fn_oper_pay_to_supplier returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "incoming - we pay to supplier for wares".
    -- Raises exc`eption if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_PAY_TO_SUPPLIER');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_supp_debt=:x';
        v_key = -1;
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', 'm_qty_res=-1 and m_cost_out=1' );
        end
        rdb$set_context('USER_SESSION', 'FN_OPER_PAY_TO_SUPPLIER', v_id);
    end
    
    return v_id;

end

^ -- fn_oper_pay_to_supplier

create or alter function fn_oper_pay_from_customer returns int deterministic  as
    declare v_id type of dm_idb = null;
    declare v_key dm_sign;
    declare v_stt varchar(255);
begin
    -- find id for operation "accept payment for sold wares (target transfer)".
    -- Raises exc`eption if it can`t be found.
    v_id=rdb$get_context('USER_SESSION', 'FN_OPER_PAY_FROM_CUSTOMER');
    if (v_id is null) then begin
        v_stt = 'select o.id from optypes o where o.m_cust_debt=:x';
        v_key = -1;
        execute statement (v_stt) ( x := :v_key ) into v_id;
        if (v_id is null) then
        begin
            exception ex_record_not_found using( 'OPTYPES', 'm_qty_res=-1 and m_cost_out=1' );
        end

        rdb$set_context('USER_SESSION', 'FN_OPER_PAY_FROM_CUSTOMER', v_id);
    end
    
    return v_id;

end

^ -- fn_oper_pay_from_customer

create or alter function fn_mcode_for_oper(a_oper_id dm_idb) returns dm_mcode deterministic
as
    declare v_mnemonic_code type of dm_mcode;
begin
    -- returns mnemonic code for operation ('ORD' for stock order, et al)
    v_mnemonic_code = rdb$get_context('USER_SESSION','OPER_MCODE_'||:a_oper_id);
    if (v_mnemonic_code is null) then begin
        select o.mcode from optypes o where o.id = :a_oper_id into v_mnemonic_code;
        rdb$set_context('USER_SESSION','OPER_MCODE_'||:a_oper_id, v_mnemonic_code);
    end
    return v_mnemonic_code;
end

^ -- fn_mcode_for_oper

create or alter function fn_get_stack(
    a_halt_due_to_error smallint default 0
)
    returns dm_stack
as
    declare v_call_stack dm_stack;
    declare function fn_internal_stack_disabled returns boolean deterministic as
    begin
        return ( coalesce(rdb$get_context('USER_SESSION','ENABLE_MON_QUERY'),0) = 0 );
    end
    declare v_line dm_stack;
    declare v_this dm_dbobj = 'fn_get_stack';
begin
    -- :: NB ::
    -- 1. currently building of stack stack IGNORES procedures which are
    -- placed b`etween top-level SP and 'this' unit. See:
    -- sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1109867&msg=16422390
    -- (resolved, see: ChangeLog, issue "2014-08-12 10:21  hvlad")
    -- 2. mon$call_stack is UNAVALIABLE if some SP is called from trigger and
    -- this trigger, in turn, fires IMPLICITLY due to cascade FK.
    -- 13.08.2014: still UNRESOLVED. See:
    -- sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1109867&msg=16438071
    -- 3. Deadlocks trouble when heavy usage of monitoring was resolved only
    --    in FB-3, approx. 09.08.2014, see letter to dimitr 11.08.2014 11:56
    v_call_stack='';
    if ( fn_remote_process() NOT containing 'IBExpert'
         and a_halt_due_to_error = 0
         and fn_internal_stack_disabled()
       ) then
       --####
       exit;
       --####

    for
        with recursive
        r as (
            select 1 call_level,
                 c.mon$statement_id as stt_id,
                 c.mon$call_id as call_id,
                 c.mon$object_name as obj_name,
                 c.mon$object_type as obj_type,
                 c.mon$source_line as src_row,
                 c.mon$source_column as src_col
             -- NB, 13.08.2014: unavaliable record if SP is called from:
             -- 1) trigger which is fired by CASCADE
             -- 2) dyn SQL (ES)
             -- see:
             -- sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1109867&msg=16438071
             -- sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1109867&msg=16442098
            from mon$call_stack c
            where c.mon$caller_id is null

            UNION ALL

            select r.call_level+1,
                   c.mon$statement_id,
                   c.mon$call_id,
                   c.mon$object_name,
                   c.mon$object_type,
                   c.mon$source_line,
                   c.mon$source_column
            from mon$call_stack c
              join r
                on c.mon$caller_id = r.call_id
        )
        ,b as(
            select h.call_level,
                   h.obj_name,
                   h.obj_type,
                   h.src_row,
                   h.src_col
                   --count(*)over() cnt
            from r h
            join mon$statements s
                 on s.mon$statement_id = h.stt_id
            where s.mon$attachment_id = current_connection
        )
        select obj_name, obj_type, src_row, src_col, call_level
        from b
        -- where k < cnt -- <<< do NOT include THIS sp name in call_stack
        order by call_level -- ::: NB :::
        as cursor c
    do
    begin
        v_line = trim(c.obj_name)||'('||c.src_row||':'||c.src_col||') ==> ';
        if ( char_length(v_call_stack) + char_length(v_line) >= 512 )
        then
            exit;

        if ( v_call_stack NOT containing v_line and v_line NOT containing v_this||'(' )
        then
            v_call_stack = v_call_stack || v_line;
    end
    if ( v_call_stack > '' ) then
       v_call_stack = substring( v_call_stack from 1 for char_length(v_call_stack)-5 );
    return v_call_stack;
end

^ -- fn_get_stack

-- STUB, will be redefined when config parameter 'use_external_to_stop'
-- has some non-empty value of [path+]file of external table that will serve
-- as mean to stop test from outside (i.e. this parameter is UNcommented)
create or alter view v_stoptest as
select 1 as need_to_stop
from rdb$database
where 1 = 0
^

create or alter procedure sp_stoptest
returns(need_to_stop smallint) as
begin
    need_to_stop = sign( gen_id(g_stop_test, 0) );
    if ( need_to_stop = 0 and exists( select * from v_stoptest  ) )
    then
        need_to_stop = -1;
    if ( need_to_stop <> 0 ) then
        -- "+1" => test_time expired, normal finish;
        -- "-1" ==> outside command to premature stop test by adding line into
        --          text file defined by 'ext_stoptest' table or running temp
        --          batch file %tmpdir%stoptest.tmp.bat (1stoptest.tmp.sh)
        suspend;
end
^

create or alter procedure sp_halt_on_error(
    a_char char(1) default '1',
    a_gdscode bigint default null,
    a_trn_id bigint default null,
    a_need_to_stop smallint default null
) as
    declare v_curr_trn bigint;
    declare v_dummy bigint;
    declare v_need_to_stop smallint;
begin
    -- Adding single character + LF into external table (text file) 'stoptest.txt'
    -- when test is needed to stop (either due to test_time expiration or because of
    -- encountering some critical errors like PK/FK violations or negative amount remainders).
    -- Input argument a_char:
    -- '1' ==> call from SP_ADD_TO_ABEND_LOG: unexpected test finish due to PK/FK violation,
    --         and also call from SRV_FIND_QD_QS_MISM when founding mismatches between total
    --         sum of doc_data amounts and count of rows in QDistr + QStorned.
    -- '2' ==> call from SP_CHECK_TO_STOP_WORK: expected test finish due to test_time expired.
    --         In this case argument a_gdscode = -1 and we do NOT need to evaluate call stack.
    -- '5' ==> call from SRV_CHECK_NEG_REMAINDERS: unexpected test finish due to encountering
    --         negative remainder of some ware_id. NB: context var 'QMISM_VERIFY_BITSET' should
    --         have value N for which result of bin_and( N, 2 ) will be 1 in order this checkto be done.
    
    if ( (a_need_to_stop < 0 or gen_id(g_stop_test, 0) <= 0) and fn_remote_process() NOT containing 'IBExpert' )
    then
    begin
        v_curr_trn = coalesce(a_trn_id, current_transaction);

        -- "-1" ==> decision to premature stop all ISQL sessions by issuing EXTERNAl command
        -- (either running $tmpdir/1stoptest.tmp.sh or adding line into external file 'stoptest.txt')
        v_need_to_stop = coalesce( :a_need_to_stop, (select p.need_to_stop from sp_stoptest p rows 1) );

        v_dummy = gen_id( g_stop_test, 2147483647);
        
        in autonomous transaction do
        begin
            -- set elapsed_ms = -1 to skip this record from srv_mon_perf_detailed output:
            insert into perf_log(
                unit
                ,fb_gdscode
                ,ip
                ,trn_id
                ,dts_end
                ,elapsed_ms
                ,stack
                ,exc_unit
                ,exc_info
            ) values(
                'sp_halt_on_error'
                ,:a_gdscode
                ,fn_remote_address()
                ,:v_curr_trn
                ,'now'
                ,-1
                ,fn_get_stack( iif(:a_gdscode>=0, 1, 0) ) -- pass '1' to force write call_stack into perf_log if this is NOT expected test finish
                ,:a_char
                ,iif( -- write info for reporting state of how test finished:
                    :a_gdscode >= 0, 'ABNORMAL: GDSCODE='||coalesce(:a_gdscode,'<?>')
                    ,iif( :v_need_to_stop < 0
                         ,'PREMATURE: EXTERNAL COMMAND.'
                         ,'NORMAL: TEST_TIME EXPIRED.'
                        )
                )
            );

            begin
                -- moved here from sp_check_to_stop_work: avoid excessive start auton. Tx!
                update perf_log p set p.info = 'closed'
                where p.unit = 'perf_watch_interval' and p.info containing 'active'
                order by dts_beg desc -- "+0" ?
                rows 1;
            when any do
                begin
                    -- NOP --
                end
            end
        end
    end
end

^ -- sp_halt_on_error

create or alter procedure sp_flush_tmpperf_in_auton_tx(
    a_starter dm_unit,  -- name of module which STARTED job, = rdb$get_context('USER_SESSION','LOG_PERF_STARTED_BY')
    a_context_rows_cnt int, -- how many 'records' with context vars need to be processed
    a_gdscode int default null
)
as
    declare i smallint;
    declare v_id dm_idb;
    declare v_curr_tx int;
    declare v_exc_unit type of column perf_log.exc_unit;
    declare v_stack dm_stack;
    declare v_dbkey dm_dbkey;
    declare v_remote_addr dm_ip;
begin
    -- Flushes all data from context variables with names 'PERF_LOG_xxx'
    -- which have been set in sp_f`lush_perf_log_on_abend for saving uncommitted
    -- data in tmp$perf_log in case of error. Frees namespace USER_SESSION from
    -- all such vars (allowing them to store values from other records in tmp$perf_log)
    -- Called only from sp_abend_flush_perf_log
    v_curr_tx = current_transaction;
    v_remote_addr = fn_remote_address(); -- out from loop (seems that recalc on every iteration + cost of savepoint when call this fn)

    -- 13.08.2014: we have to get full call_stack in AUTONOMOUS trn!
    -- sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1109867&msg=16422273
    in autonomous transaction do -- *****  A U T O N O M O U S    T x, due to call fn_get_stack *****
    begin
        -- 11.01.2015: decided to profile this:
        insert into perf_log(unit, dts_beg, trn_id, ip)
        values( 't$perf-abend:' || :a_starter,
                'now',
                :v_curr_tx,
                :v_remote_addr
              )
        returning rdb$db_key into v_dbkey; -- will return to this row after loop

        i=0;
        while (i < a_context_rows_cnt) do
        begin
            v_exc_unit =  rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_XUNI');
            if ( v_exc_unit = '#' ) then -- ==> call from unit <U> where exception occured (not from callers of <U>)
                v_stack = fn_get_stack( fn_halt_sign(a_gdscode) );
            else
                v_stack = null;

            insert into perf_log(
                id,
                unit,
                fb_gdscode,
                info,
                exc_unit,
                exc_info,
                dts_beg,
                dts_end,
                elapsed_ms,
                aux1,
                aux2,
                trn_id,
                ip,
                stack
            )
            values(
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_ID'),
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_UNIT'),
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_GDS'),
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_INFO'),
                :v_exc_unit,
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_XNFO'),
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_BEG'),
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_END'),
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_MS'),
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_AUX1'),
                rdb$get_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_AUX2'),
                :v_curr_tx,
                :v_remote_addr,
                :v_stack
            );

            -- free space for new context vars which can be set on later iteration:
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_ID', null);    -- 1
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_UNIT', null);
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_GDS', null);
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_INFO', null);
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_XUNI', null);  -- 5
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_XNFO', null);
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_BEG', null);
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_END', null);
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_MS', null);
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_AUX1', null);  -- 10
            rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :i ||'_AUX2', null);

            i = i + 1;
        end -- while (i < a_context_rows_cnt)

        update perf_log g
        set info = 'gds='|| :a_gdscode||', autonomous Tx: ' ||:i||' rows',
            dts_end = 'now',
            elapsed_ms = datediff( millisecond from dts_beg to cast('now' as timestamp) ),
            aux1 = :i
        where g.rdb$db_key = :v_dbkey;

    end -- in autonom. tx
end

^ -- sp_flush_tmpperf_in_auton_tx

create or alter procedure sp_flush_perf_log_on_abend(
    a_starter dm_unit,  -- name of module which STARTED job, = rdb$get_context('USER_SESSION','LOG_PERF_STARTED_BY')
    a_unit dm_unit, -- name of module where trouble occured
    a_gdscode int default null,
    a_info dm_info default null, -- additional info for debug
    a_exc_info dm_info default null, -- user-def or standard ex`ception description
    a_aux1 dm_aux default null,
    a_aux2 dm_aux default null
)
as
    declare v_cnt smallint;
    declare v_dts timestamp;
    declare v_info dm_info = '';
    declare v_ctx_lim smallint; -- max number of context vars which can be put in one 'batch'
    declare c_max_context_var_cnt int = 1000; -- limitation of Firebird: not more than 1000 context variables
    declare c_std_user_exc int = 335544517; -- std FB code for user defined exceptions
    declare c_gen_inc_step_pf int = 20; -- size of `batch` for get at once new IDs for perf_log (reduce lock-contention of gen page)
    declare v_gen_inc_iter_pf int; -- increments from 1  up to c_gen_inc_step_pf and then restarts again from 1
    declare v_gen_inc_last_pf dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_pf)
    declare v_pf_new_id dm_idb;
begin

    -- called only if ***ABEND*** occurs (from sp`_add_to_abend_log)
    if ( rdb$get_context('USER_TRANSACTION', 'DONE_FLUSH_PERF_LOG_ON_ABEND') is NOT null )
    then
        exit; -- we already done this (just after unit where exc`exption occured)

    v_ctx_lim = cast( rdb$get_context('USER_SESSION', 'CTX_LIMIT_FOR_FLUSH_PERF_LOG') as smallint );
    if ( v_ctx_lim is null ) then
    begin
        -- First, calculate (approx) avaliable limit for creating new ctx vars:
        -- limitation of Firebird: not more than 1000 context variables; take twise less than this limit
        select :c_max_context_var_cnt - sum(c)
        from (
            select count(*) c
            from settings s
            where s.working_mode in(
                'COMMON',
                rdb$get_context('USER_SESSION','WORKING_MODE')
            )
            union all
            select count(*) from optypes    -- look at fn_oper_xxx stored funcs
            union all
            select count(*) from doc_states -- look at fn_doc_xxx_state stored funcs
            union all
            select count(*) from rules_for_qdistr -- look at sp_cache_rules_for_distr('QDISTR')
            union all
            select count(*) from rules_for_pdistr -- look at sp_cache_rules_for_distr('PDISTR')
        )
        into v_ctx_lim;
        -- Get number of ROWS from tmp$perf_log to start flush data after reaching it:
        -- "0.8*" - to be sure that we won`t reach limit;
        -- "/12" - number of context vars for each record of tmp$perf_log (see below)
        v_ctx_lim = cast( (0.8 * v_ctx_lim) / 12.0 as smallint);
        rdb$set_context('USER_SESSION', 'CTX_LIMIT_FOR_FLUSH_PERF_LOG', v_ctx_lim);
    end

    c_gen_inc_step_pf = v_ctx_lim; -- value to increment IDs in PERF_LOG at one call of gen_id
    v_gen_inc_iter_pf = c_gen_inc_step_pf;

    -- Perform `transfer` from tmp$perf_log to 'fixed' perf_log table
    -- in auton. tx, saving fields data in context vars:
    v_cnt = 0;
    v_dts = 'now';
    for
        select
            unit
            ,coalesce( fb_gdscode, :a_gdscode, :c_std_user_exc ) as fb_gdscode
            ,info
            ,exc_unit -- '#' ==> exception occured in the module with name = tmp$perf_log.unit
            ,iif( exc_unit is not null, coalesce( exc_info, :a_exc_info), null ) as exc_info -- fill exc_info only for unit where exc`eption really occured (NOT for unit that calls this 'problem' unit)
            ,dts_beg
            ,coalesce(dts_end, :v_dts) as dts_end
            ,iif(unit = :a_unit, coalesce(aux1, :a_aux1), aux1) as aux1
            ,iif(unit = :a_unit, coalesce(aux2, :a_aux2), aux2) as aux2
        from tmp$perf_log g
        -- ::: NB ::: CORE-4483: "Changed data not visible in WHEN-section if exception occured inside SP that has been called from this code"
        -- We have to save data from tmp$perf_log for ALL units that are now in it!
        as cursor c
    do
    begin
        if ( v_cnt < v_ctx_lim ) then
            -- there is enough place in namespace to create new context vars
            -- instead of starting auton. tx (performance!)
            begin
                v_info = coalesce(c.info, '');
                -- Some unit (e.g. ) could run several times and exc`eption could occured
                --  in Nth  call of that unit (N >= 2). We must add :a_info to v_info
                -- *ONLY* if processed record in tmp$perf_log relates to that Nth call
                -- of unit (where exc`ption occured).
                -- Sample: sp_cancel_adding_invoice => create list of dependent
                -- docs, lock all of them, and then for each of these docs (reserves):
                -- sp_cancel_reserve => trigger doc_list_aiud => sp_kill_qstorno_ret_qs2qd
                if (c.unit = a_unit
                    and
                    c.exc_unit is NOT null
                ) then
                    v_info = left(v_info || trim(iif( v_info>'', '; ', '')) || coalesce(a_info,''), 255);

                if ( v_gen_inc_iter_pf = c_gen_inc_step_pf ) then -- its time to get another batch of IDs
                begin
                    v_gen_inc_iter_pf = 1;
                    -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                    v_gen_inc_last_pf = gen_id( g_perf_log, :c_gen_inc_step_pf );
                end
                v_pf_new_id = v_gen_inc_last_pf - ( c_gen_inc_step_pf - v_gen_inc_iter_pf );
                v_gen_inc_iter_pf = v_gen_inc_iter_pf + 1;

                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_ID', v_pf_new_id);    --  1
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_UNIT', c.unit);       --  2
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_GDS', c.fb_gdscode ); --  3
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_INFO', v_info);       --  4
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_XUNI', c.exc_unit);   --  5
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_XNFO', c.exc_info);   --  6
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_BEG', c.dts_beg);     --  7
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_END', c.dts_end);     --  8
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_MS', datediff(millisecond from c.dts_beg to c.dts_end));
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_AUX1', c.aux1);       -- 10
                rdb$set_context('USER_SESSION', 'PERF_LOG_'|| :v_cnt ||'_AUX2', c.aux2);       -- 11
                v_cnt = v_cnt + 1;
            end
        else -- it's time to "flush" data from context vars to fixed table pref_log using auton tx
            begin
                -- namespace usage should be reduced ==> flush data from context vars
                execute procedure sp_flush_tmpperf_in_auton_tx(a_starter, v_cnt, a_gdscode);
                v_cnt = 0;
            end
    end -- cursor for all rows of tmp$perf_log

    if (v_cnt > 0) then
    begin
        -- flush (again!) to perf_log data from rest of context vars (v_cnt now can be >0 and < c_limit):
        execute procedure sp_flush_tmpperf_in_auton_tx( a_starter, v_cnt, a_gdscode);
    end

    -- create new ctx in order to prevent repeat of transfer on next-level stack:
    rdb$set_context('USER_TRANSACTION', 'DONE_FLUSH_PERF_LOG_ON_ABEND','1');

end

^ -- sp_flush_perf_log_on_abend

-- STUBS for two SP, they will be defined later, need in s`p_add_to_perf_log (30.08.2014)
create or alter procedure srv_fill_mon(a_rowset bigint default null) returns(rows_added int) as
begin
  suspend;
end

^ -- srv_fill_mon (stub!)

create or alter procedure srv_fill_tmp_mon(
    a_rowset dm_idb,
    a_ignore_system_tables smallint default 1,
    a_unit dm_unit default null,
    a_info dm_info default null,
    a_gdscode int default null
)
returns(
    rows_added int
)
as begin
  suspend;
end

^ -- srv_fill_tmp_mon (stub!)

--------------------------------------------------------------------------------

create or alter procedure srv_log_mon_for_traced_units(
    a_unit dm_unit,
    a_gdscode integer default null,
    a_info dm_info default null
)
as
  declare v_rowset bigint;
  declare v_dummy bigint;
begin
    if (
          --rdb$get_context('SYSTEM','ENGINE_VERSION') starting with '3.' -- new mon counters were introduced only in 3.0!
         --and fn_remote_process() containing 'IBExpert'
         rdb$get_context('USER_SESSION', 'ENABLE_MON_QUERY') = 1
         and
         rdb$get_context('USER_SESSION','TRACED_UNITS') containing ','||a_unit||',' -- this is call from some module which we want to analyze
       ) then
    begin
        -- Gather all avaliable mon info about caller module: add pair of row sets
        -- (for beg and end) and then calculate DIFFERENCES of mon. counters with
        -- logging in tables `mon_log` and `mon_log_table_stats`.
        -- NOT work in 2.5 due to bulk of deadlocks when intensive monitoring using
        v_rowset = rdb$get_context('USER_SESSION','MON_ROWSET_'||a_unit);
        if ( v_rowset is null  ) then
            begin
                -- define context var which will identify rowset field       
                -- in mon_log and mon_log_table_stats:                       
                -- (this value is ised after call app. unit):               
                v_rowset = gen_id(g_common,1);
                rdb$set_context('USER_SESSION','MON_ROWSET_'||a_unit, v_rowset);
                -- gather mon$ tables: add FIRST row to GTT tmp$mon_log,
                -- all counters will be written as NEGATIVE values
                in autonomous transaction do
                select count(*)                                             
                from srv_fill_tmp_mon
                (                                       
                      :v_rowset -- :a_rowset
                     ,1         -- :a_ignore_system_tables
                     ,:a_unit   -- :a_unit
                )
                into v_dummy;                                                
            end
        else -- add second row to GTT, all counters will be written as POSITIVE values:
            begin
                rdb$set_context('USER_SESSION','MON_ROWSET_'||a_unit, null);
                in autonomous transaction do -- NB: add in AT both when v_abend = true / false, otherwise records in tmp$mon$log_* remains when rollback (01.09.2014)
                begin
                    select count(*)                                             
                    from srv_fill_tmp_mon
                    (:v_rowset -- :a_rowset
                     ,1        -- :a_ignore_system_tables
                     ,:a_unit
                     ,:a_info
                     ,:a_gdscode
                    ) into v_dummy;

                    -- TOTALLING mon counters for this unit:
                    -- insert into mon_log(...)
                    -- select sum(...) from tmp$mon_log t
                    -- where t.rowset = :a_rowset group by t.rowset
                    select count(*) from srv_fill_mon( :v_rowset )
                    into v_dummy;
                end

            end

    end -- engine = '3.x' and remote_process containing 'IBExpert' and ctx TRACED_UNITS containing ',<a_unit>,'

end

^ -- srv_log_mon_for_traced_units

-------------------------------------------------------------------------------

create or alter procedure sp_add_perf_log (
    a_is_unit_beginning dm_sign,
    a_unit dm_unit,
    a_gdscode integer default null,
    a_info dm_info default null,
    a_aux1 dm_aux default null,
    a_aux2 dm_aux default null
) as
    declare v_curr_tx bigint;
    declare v_dts timestamp;
    declare v_save_dts_beg timestamp;
    declare v_save_dts_end timestamp;
    declare v_save_gtt_cnt int;
    declare v_id dm_idb;
    declare v_unit dm_unit;
    declare v_info dm_info;
    declare c_gen_inc_step_pf int = 20; -- size of `batch` for get at once new IDs for perf_log (reduce lock-contention of gen page)
    declare v_gen_inc_iter_pf int; -- increments from 1  up to c_gen_inc_step_pf and then restarts again from 1
    declare v_gen_inc_last_pf dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_pf)
    declare v_pf_new_id dm_idb;
begin
    -- Registration of all STARTs and FINISHes (both normal and failed)
    -- for all application SPs and some service units:
    v_curr_tx = current_transaction;
    v_dts = cast('now' as timestamp);

    -- Gather all avaliable mon info about caller module if its name belongs
    -- to list specified in `TRACED_UNITS` context var: add pair of row sets
    -- (for beg and end) and then calculate DIFFERENCES of mon. counters with
    -- logging in tables `mon_log` and `mon_log_table_stats`.
    execute procedure srv_log_mon_for_traced_units( a_unit, a_gdscode, a_info );

    if ( not exists(select * from tmp$perf_log) ) then
    begin
       rdb$set_context('USER_SESSION','LOG_PERF_STARTED_BY', a_unit);
       a_is_unit_beginning = 1;
    end

    if ( a_is_unit_beginning = 1 ) then -- this is call from ENTRY of :a_unit
        begin
            insert into tmp$perf_log(
                 unit,
                 info,
                 ip,
                 trn_id,
                 dts_beg
            )
            values(
                 :a_unit,
                 :a_info,
                 fn_remote_address(),
                 :v_curr_tx,
                 :v_dts
            );
            -- save info about last started unit (which can raise exc):
            rdb$set_context('USER_TRANSACTION','TPLOG_LAST_UNIT', a_unit);
            rdb$set_context('USER_TRANSACTION','TPLOG_LAST_BEG', v_dts);
            rdb$set_context('USER_TRANSACTION','TPLOG_LAST_INFO', v_info);
        end
    else -- a_is_unit_beginning = 0 ==> this is _NORMAL_ finish of :a_unit (i.e. w/o exception)
        begin
            update tmp$perf_log t set
                info = left(coalesce( info, '' ) || coalesce( trim(iif( info>'', '; ', '') || :a_info), ''), 255),
                dts_end = :v_dts,
                -- dis 12.01.2015, not need more: id2 = gen_id(g_perf_log, 1), -- 4 debug! 28.09.2014
                elapsed_ms = datediff(millisecond from dts_beg to :v_dts),
                aux1 = :a_aux1,
                aux2 = :a_aux2
            where -- Bitmap Index "TMP$PERF_LOG_UNIT_TRN" Range Scan (full match)
                t.unit = :a_unit
                and t.trn_id = :v_curr_tx
                and dts_end is NULL -- we are looking for record that was added at the BEG of this unit call
            ;

            if ( a_unit = rdb$get_context('USER_SESSION','LOG_PERF_STARTED_BY') ) then
            begin
                v_gen_inc_iter_pf = c_gen_inc_step_pf;

                -- Finish of top-level unit (which start business op.):
                -- MOVE *ALL* data currently stored in GTT tmp$perf_log to fixed table perf_log

                v_save_dts_beg = 'now'; -- for logging time and number of moved records
                v_save_gtt_cnt = 0;

                for
                    select
                         unit
                        ,exc_unit
                        ,fb_gdscode
                        ,trn_id
                        ,att_id
                        ,elapsed_ms
                        ,info
                        ,exc_info
                        ,stack
                        ,ip
                        ,dts_beg
                        ,dts_end
                        ,aux1
                        ,aux2
                    from tmp$perf_log g
                    as cursor ct
                do begin
                    if ( v_gen_inc_iter_pf = c_gen_inc_step_pf ) then -- its time to get another batch of IDs
                    begin
                        v_gen_inc_iter_pf = 1;
                        -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                        v_gen_inc_last_pf = gen_id( g_perf_log, :c_gen_inc_step_pf );
                    end
                    v_pf_new_id = v_gen_inc_last_pf - ( c_gen_inc_step_pf - v_gen_inc_iter_pf );
                    v_gen_inc_iter_pf = v_gen_inc_iter_pf + 1;

                    insert into perf_log(
                        id
                        ,unit, exc_unit
                        ,fb_gdscode, trn_id, att_id, elapsed_ms
                        ,info, exc_info, stack
                        ,ip, dts_beg, dts_end
                        ,aux1, aux2
                    ) values (
                        :v_pf_new_id
                        ,ct.unit, ct.exc_unit
                        ,ct.fb_gdscode, ct.trn_id, ct.att_id, ct.elapsed_ms
                        ,ct.info, ct.exc_info, ct.stack
                        ,ct.ip, ct.dts_beg, ct.dts_end
                        ,ct.aux1, ct.aux2
                    );
                    v_save_gtt_cnt = v_save_gtt_cnt + 1;

                    delete from tmp$perf_log where current of ct;
                end -- end of loop moving rows from GTT tmp$perf_log to fixed perf_log

                v_save_dts_end = 'now';

                -- Add info about timing and num. of record (tmp$ --> fixed):
                insert into perf_log(
                        id,
                        unit, info, dts_beg, dts_end, elapsed_ms, ip, aux1)
                values( iif( :v_gen_inc_iter_pf < :c_gen_inc_step_pf, :v_pf_new_id+1, gen_id( g_perf_log, 1 )  ),
                        't$perf-norm:'||:a_unit,
                        'ok saved '||:v_save_gtt_cnt||' rows',
                        :v_save_dts_beg,
                        :v_save_dts_end,
                        datediff( millisecond from :v_save_dts_beg to :v_save_dts_end ),
                        fn_remote_address(),
                        :v_save_gtt_cnt
                      );
            end --  a_unit = rdb$get_context('USER_SESSION','LOG_PERF_STARTED_BY')
        end -- a_is_unit_beginning = 0
end

^ -- sp_add_perf_log

create or alter procedure sp_upd_in_perf_log(
    a_unit dm_unit,
    a_gdscode int default null,
    a_info dm_info default null
) as
begin
    -- need in case when we want to update info in just added row
    -- (e.g. info about selected doc etc)
    update tmp$perf_log t set
        t.fb_gdscode = coalesce(t.fb_gdscode, :a_gdscode),
        t.info = coalesce( t.info, '' ) || coalesce( trim(iif( t.info>'', '; ', '') || :a_info), '')
    where
        t.unit = :a_unit
        and t.trn_id = current_transaction
        and t.dts_end is NULL
        and coalesce(t.info,'') NOT containing coalesce(trim(:a_info),'');
end

^  -- sp_upd_in_perf_log

-- stub, will be overwritten, see below:
create or alter procedure zdump4dbg(
    a_doc_list_id bigint default null,
    a_doc_data_id bigint default null,
    a_ware_id bigint default null
) as begin
  -- ::: NB ::: This SP is overwritten in script 'oltp_misc_debug.sql' which
  -- is called ONLY if config parameter 'create_with_debug_objects' is set to 1.
  -- Open oltpNN_config.*** file and change this parameter if you want this
  -- proc and some other aux tables (named with "Z_" prefix) to be created.
end
^ -- zdump4dbg (STUB!)

create or alter procedure sp_add_to_abend_log(
       a_exc_info dm_info,
       a_gdscode int default null,
       a_info dm_info default null,
       a_caller dm_unit default null,
       a_halt_due_to_error smallint default 0 --  1 ==> forcely extract FULL STACK ignoring settings, because of error + halt test
) as
    declare v_last_unit dm_unit;
    declare v_last_info dm_info;
    declare v_last_beg timestamp;
    declare v_last_end timestamp;
begin
    -- SP for register info about e`xception occured in application module.
    -- When each module starts, it call sp_add_to_perf_log and creates record in
    -- perf_log table for this event. If some e`xception occurs in that module
    -- than code jumps into when_any section with call of this SP.
    -- Now we have to call sp_add_to_perf_log with special argument ('!abend!')
    -- signalling that all data from GTT tmp$perf_log should be saved now via ATx.
    if ( a_gdscode is NOT null and nullif(a_exc_info, '') is null ) then -- this is standard error
    begin
        select f.fb_mnemona
        from fb_errors f
        where f.fb_gdscode = :a_gdscode
        into a_exc_info;
    end
    -- For displaying in ISQL session logs:
    rdb$set_context('USER_SESSION','ADD_INFO', left( coalesce(a_exc_info, 'no-mnemona'), 255));


    v_last_unit = rdb$get_context('USER_TRANSACTION','TPLOG_LAST_UNIT');

    if ( a_caller = v_last_unit ) then
    begin
        -- CORE-4483. "Changed data not visible in WHEN-section if exception
        -- occured inside SP that has been called from this code" ==> last record
        -- in tmp$perf_log that has been added in SP_ADD_PERF_LOG which has been
        -- called at the START point of this SP CALLER, will be backed out (REMOVED!)
        -- when exception occurs later in the intermediate point of caller, so
        -- HERE we get tmp$perf_log WITHOUT last record!
        -- 10.01.2015: replaced 'update' with 'update or insert': record in
        -- tmp$perf_log can be 'lost' in case of exc in caller before we come
        -- in this SP (sp_cancel_client_order => sp_lock_selected_doc).
        v_last_beg = rdb$get_context('USER_TRANSACTION','TPLOG_LAST_BEG');
        v_last_end = cast('now' as timestamp);
        v_last_info = rdb$get_context('USER_TRANSACTION','TPLOG_LAST_INFO');

        update tmp$perf_log t
        set
            fb_gdscode = :a_gdscode,
            info = :a_info, --  coalesce( :v_last_info, '<null-1>'),
            exc_unit = '#', -- exc_unit: direct CALLER of this SP is the SOURCE of raised exception
            dts_end = :v_last_end,
            elapsed_ms = datediff(millisecond from :v_last_beg to :v_last_end)
        where
            t.unit = rdb$get_context('USER_TRANSACTION','TPLOG_LAST_UNIT')
            and t.trn_id = current_transaction
            and dts_end is NULL; -- index key: UNIT,TRN_ID,DTS_END

        if ( row_count = 0 ) then
            insert into tmp$perf_log(
                 unit
                ,fb_gdscode
                ,info
                ,exc_unit
                ,dts_beg
                ,dts_end
                ,elapsed_ms
                ,trn_id
            ) values (
                 :a_caller
                ,:a_gdscode
                ,:a_info --- coalesce( :v_last_info, '<null-2>')
                ,'#' -- ==> module :a_caller IS the source of raised exception
                ,:v_last_beg
                ,:v_last_end
                ,datediff(millisecond from :v_last_beg to :v_last_end)
                ,current_transaction
            );

        -- before 10.01.2015:
        -- update tmp$perf_log set ... where t.unit = :a_caller and and t.trn_id = current_transaction and dts_end is NULL;
        rdb$set_context('USER_TRANSACTION','TPLOG_LAST_UNIT', null);

        -- Save uncommitted data from tmp$perf_log to perf_log (via autonom. tx):
        -- NB: All records in GTT tmp$perf_log are visible ONLY at the "deepest" point
        -- when exc` occured. If SP_03 add records to tmp$perf_log and then raises exc
        -- then all its callers (SP_00==>SP_01==>SP_02) will NOT see these record because
        -- these changes will be rolled back when exc. comes into these caller.
        -- So, we must flush records from GTT to fixed table only in the "deepest" point,
        -- i.e. just in that SP where exc raises.
        execute procedure sp_flush_perf_log_on_abend(
            rdb$get_context('USER_SESSION','LOG_PERF_STARTED_BY'), -- unit which start this job
            a_caller,
            a_gdscode,
            a_info, -- info for analysis
            a_exc_info -- info about user-defined or standard e`xception which occured now
        );

    end --  a_caller = v_last_unit 

    -- ########################   H A L T   T E S T   ######################
    if ( a_halt_due_to_error = 1 ) then
    begin
        execute procedure sp_halt_on_error('1', a_gdscode);
        if ( fn_halt_sign(a_gdscode)=1 ) then -- 27.07.2014 1003
        begin
             execute procedure zdump4dbg;
        end
    end
    -- #####################################################################

end

^ -- sp_add_to_abend_log

set term ;^


set term ^;

create or alter procedure sp_check_ctx(
    ctx_nmspace_01 dm_ctxns,
    ctx_varname_01 dm_ctxnv,
    ctx_nmspace_02 dm_ctxns = '',
    ctx_varname_02 dm_ctxnv = '',
    ctx_nmspace_03 dm_ctxns = '',
    ctx_varname_03 dm_ctxnv = '',
    ctx_nmspace_04 dm_ctxns = '',
    ctx_varname_04 dm_ctxnv = '',
    ctx_nmspace_05 dm_ctxns = '',
    ctx_varname_05 dm_ctxnv = '',
    ctx_nmspace_06 dm_ctxns = '',
    ctx_varname_06 dm_ctxnv = '',
    ctx_nmspace_07 dm_ctxns = '',
    ctx_varname_07 dm_ctxnv = '',
    ctx_nmspace_08 dm_ctxns = '',
    ctx_varname_08 dm_ctxnv = '',
    ctx_nmspace_09 dm_ctxns = '',
    ctx_varname_09 dm_ctxnv = '',
    ctx_nmspace_10 dm_ctxns = '',
    ctx_varname_10 dm_ctxnv = ''
)
as
  declare msg varchar(512) = '';
begin
    -- Check for each non-empty pair that corresponding context variable
    -- EXISTS in it's namespace. Raises exception and pass to it list of pairs
    -- which does not exists.

    if (ctx_nmspace_01>'' and rdb$get_context( upper(ctx_nmspace_01), upper(ctx_varname_01) ) is null  ) then
        msg = msg||upper(ctx_nmspace_01)||':'||coalesce(upper(ctx_varname_01),'''null''');
    
    if (ctx_nmspace_02>'' and rdb$get_context( upper(ctx_nmspace_02), upper(ctx_varname_02) ) is null  ) then
        msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_02)||':'||coalesce(upper(ctx_varname_02),'''null''');
    
    if (ctx_nmspace_03>'' and rdb$get_context( upper(ctx_nmspace_03), upper(ctx_varname_03) ) is null  ) then
       msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_03)||':'||coalesce(upper(ctx_varname_03),'''null''');
    
    if (ctx_nmspace_04>'' and rdb$get_context( upper(ctx_nmspace_04), upper(ctx_varname_04) ) is null  ) then
       msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_04)||':'||coalesce(upper(ctx_varname_04),'''null''');
    
    if (ctx_nmspace_05>'' and rdb$get_context( upper(ctx_nmspace_05), upper(ctx_varname_05) ) is null  ) then
        msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_05)||':'||coalesce(upper(ctx_varname_05),'''null''');
    
    if (ctx_nmspace_06>'' and rdb$get_context( upper(ctx_nmspace_06), upper(ctx_varname_06) ) is null  ) then
        msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_06)||':'||coalesce(upper(ctx_varname_06),'''null''');
    
    if (ctx_nmspace_07>'' and rdb$get_context( upper(ctx_nmspace_07), upper(ctx_varname_07) ) is null  ) then
        msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_07)||':'||coalesce(upper(ctx_varname_07),'''null''');
    
    if (ctx_nmspace_08>'' and rdb$get_context( upper(ctx_nmspace_08), upper(ctx_varname_08) ) is null  ) then
        msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_08)||':'||coalesce(upper(ctx_varname_08),'''null''');
    
    if (ctx_nmspace_09>'' and rdb$get_context( upper(ctx_nmspace_09), upper(ctx_varname_09) ) is null  ) then
        msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_09)||':'||coalesce(upper(ctx_varname_09),'''null''');
    
    if (ctx_nmspace_10>'' and rdb$get_context( upper(ctx_nmspace_10), upper(ctx_varname_10) ) is null  ) then
        msg = msg||iif(msg='', '', '; ')||upper(ctx_nmspace_10)||':'||coalesce(upper(ctx_varname_10),'''null''');

    if (msg<>'') then
    begin
        execute procedure sp_add_to_abend_log( left( msg, 255 ), null, '', 'sp_check_ctx' );
        exception ex_context_var_not_found using( msg );
    end
end -- sp_check_ctx

^
set term ;^


set term ^;
create or alter procedure sp_check_nowait_or_timeout as
    declare msg varchar(255);
    declare function fn_internal() returns int deterministic as
    begin
        return rdb$get_context('SYSTEM', 'LOCK_TIMEOUT');
    end
begin
    if ( fn_remote_process() containing 'IBExpert' ) then exit; -- 4debug

    -- Must be called from all SPs which are at 'top' level of data handling.
    -- Checks that current Tx is running with NO WAIT or LOCK_TIMEOUT.
    -- Otherwise raises error
    if  ( fn_internal() < 0 ) then -- better than call every time rdb$get_context('SYSTEM', 'LOCK_TIMEOUT') up to 4 times!
    begin
        msg = 'NO WAIT or LOCK_TIMEOUT required!';
        execute procedure sp_add_to_abend_log( msg, null, null, 'sp_check_nowait_or_timeout' );
        exception ex_nowait_or_timeout_required;
    end
end
^
set term ;^


set term ^;

create or alter procedure sp_check_to_stop_work as
    declare v_dts_beg timestamp;
    declare v_dts_end timestamp;
    declare v_need_to_stop smallint;
begin
    -- Must be called from all SPs which are at 'top' level of data handling.
    -- Checks that special external *TEXT* file is EMPTY, otherwise raise exc.
    -- Script tmp_random_run.sql (generated by 1run_oltp_emul.bat) contains
    -- 'set bail on' before each call of application unit, so if here we raise
    -- ex`ception EX_TEST_CANCELLATION than script tmp_random_run.sql will be
    -- cancelledthis immediatelly. The word "EX_TEST_CANCELLATION" will appear
    -- in .err file - log of errors for each ISQL session.
    -- Batch `oltp_isql_run_worker.bat` checks .err file for this word and if it
    -- is found there - all the batch will be finished via "goto test_canc" + exit

    if ( fn_remote_process() containing 'IBExpert' ) then exit; -- 4debug; 23.07.2014

    if ( rdb$get_context('USER_SESSION','PERF_WATCH_END') is null ) then
        begin
            -- this record is added in 1run_oltp_emul.bat before FIRST attach
            -- will begin it's work:
            -- PLAN (P ORDER PERF_LOG_DTS_BEG_DESC INDEX (PERF_LOG_UNIT))
            select p.dts_beg, p.dts_end
            from perf_log p
            where p.unit = 'perf_watch_interval' and p.info containing 'active'
            order by dts_beg + 0 desc -- !! 24.09.2014, speed !! (otherwise dozen fetches!)
            rows 1
            into v_dts_beg, v_dts_end;
            rdb$set_context('USER_SESSION','PERF_WATCH_BEG', v_dts_beg);
            rdb$set_context('USER_SESSION','PERF_WATCH_END', coalesce(v_dts_end, dateadd(3 hour to current_timestamp) ) );
        end
    else
        begin
            v_dts_end = rdb$get_context('USER_SESSION','PERF_WATCH_END');
        end

    v_need_to_stop = null;
    select p.need_to_stop from sp_stoptest p rows 1 into v_need_to_stop;

    if ( cast('now' as timestamp) > v_dts_end -- NORMAL finish because of test_time expiration
         or
         v_need_to_stop < 0 -- External force all ISQL sessions PREMATURE be stopped itself (either by running $tmpdir/1stoptest.tmp batch or by adding line to 'stoptest.txt')
       )
    then
        begin
           execute procedure sp_halt_on_error('2', -1, current_transaction, :v_need_to_stop);
           exception ex_test_cancellation; -- E X C E P T I O N:  C A N C E L   T E S T
        end
end

^ -- sp_check_to_stop_work 

create or alter procedure sp_init_ctx
as
    declare v_name type of dm_name;
    declare v_context type of column settings.context;
    declare v_value type of dm_setting_value;
    declare v_counter int = 0;
    declare msg varchar(255);
begin
    -- Called from db-level trigger on CONNECT. Reads table 'settings' and
    -- assigns values to context variables (in order to avoid further DB scans).

    -- ::: NOTE ABOUT POSSIBLE PROBLEM WHEN CONNECT TO DATABASE :::
    -- On Classic installed on *nix one may get exception on connect to database
    -- with following text:
    --     Statement failed, SQLSTATE = 2F000
    --     Error while parsing procedure SP_INIT_CTX's BLR
    --     -Error while parsing procedure SP_ADD_TO_ABEND_LOG's BLR
    --     -Error while parsing procedure SP_FLUSH_PERF_LOG_ON_ABEND's BLR
    --     -I/O error during "open O_CREAT" operation for file ""
    --     -Error while trying to create file
    -- This exception is caused by TRG_CONNECT trigger on database connect event.
    -- When this trigger calls SP_INIT_CTX, which then can attempt to add data
    -- into GTT table TMP$PERF_LOG. File which must store data for this GTT
    -- is created in the folder defined by FIREBIRD_TMP env. variable.
    -- Exception will occur when this folder is undefined xinetd daemon has
    -- no rights to create files in it.
    -- SOLUTION: check script /etc/init.d/xinetd - it should contain text like:
    -- #########
    -- FIREBIRD_TMP = /tmp/firebird
    -- export FIREBIRD_TMP, ...

    if (rdb$get_context('USER_SESSION','WORKING_MODE') is null) then
    begin
        rdb$set_context(
            'USER_SESSION',
            'WORKING_MODE',
            (select s.svalue
            from settings s
            where s.working_mode = 'INIT' and s.mcode = 'WORKING_MODE')
                       );
    end

    if ( rdb$get_context('USER_SESSION','WORKING_MODE') is not null
       and
       exists (select * from settings s
                where s.working_mode = rdb$get_context('USER_SESSION','WORKING_MODE')
              )
     ) then
    begin
        -- initializes all needed context variables (scan `setting` table)
        for
            select upper(s.mcode), upper(s.context), s.svalue
            from settings s
            where s.context in('USER_SESSION','USER_TRANSACTION')
                  and
                  ( s.working_mode = rdb$get_context('USER_SESSION','WORKING_MODE')
                    and s.init_on = 'connect' -- 03.09.2014: exclude 'C_NUMBER_OF_AGENTS', 'C_WARES_MAX_ID' - need only in init db building
                    or
                    s.working_mode = 'COMMON'
                  )
            into
                v_name, v_context, v_value
        do begin
            rdb$set_context(v_context, v_name, v_value);
            v_counter = v_counter + 1;
        end
    end
    if (v_counter = 0 and exists (select * from settings s) ) then
    begin
        msg = 'Context variable ''WORKING_MODE'' is invalid.';
        execute procedure sp_add_to_abend_log( msg, null, null, 'sp_init_ctx' );
        exception ex_bad_working_mode_value
        using ( coalesce( '>'||rdb$get_context('USER_SESSION','WORKING_MODE')||'<', '<null>') );
        -- "-db-level trigger TRG_CONNECT: no found rows for settings.working_mode='>****<', correct it!"
    end
end

^ -- sp_init_ctx

-- STUB! Redefinition see in file oltp_misc_debug.sql
create or alter procedure z_remember_view_usage (
    a_view_for_search dm_dbobj,
    a_view_for_min_id dm_dbobj default null,
    a_view_for_max_id dm_dbobj default null
) as
    declare i smallint;
    declare v_ctxn dm_ctxnv;
    declare v_name dm_dbobj;
begin

end

^ -- z_remember_view_usage (STUB!)s

create or alter procedure sp_get_random_id (
    a_view_for_search dm_dbobj,
    a_view_for_min_id dm_dbobj default null,
    a_view_for_max_id dm_dbobj default null,
    a_raise_exc dm_sign default 1, -- raise exc`eption if no record will be found
    a_can_skip_order_clause dm_sign default 0, -- 17.07.2014 (for some views where document is taken into processing and will be REMOVED from scope of this view after Tx is committed)
    a_find_using_desc_index dm_sign default 0,  -- 11.09.2014: if 1, then query will be: "where id <= :a order by id desc"
    a_count_to_generate int default 1 -- 09.10.2015: how many values to generate and return as resultset (to reduce number of ES preparing)
)
returns (
    id_selected bigint
)
as
    declare i smallint;
    declare v_stt varchar(255);
    declare id_min double precision;
    declare id_max double precision;
    declare v_rows int;
    declare id_random bigint;
    declare msg dm_info;
    declare v_info dm_info;
    declare v_this dm_dbobj = 'sp_get_random_id';
    declare v_ctxn dm_ctxnv;
    declare v_name dm_dbobj;
    declare fn_internal_max_rows_usage int;
begin
    -- Selects random record from view <a_view_for_search>
    -- using select first 1 id from ... where id >= :id_random order by id.
    -- Aux. parameters:
    -- # a_view_for_min_id and a_view_for_max_id -- separate views that
    --   might be more effective to find min & max LIMITS than scan using a_view_for_search.
    -- # a_raise_exc (default=1) - do we raise exc`eption if record not found.
    -- # a_can_skip_order_clause (default=0) - can we SKIP including of 'order by' clause
    --   in statement which will be passed to ES ? (for some cases we CAN do it for efficiency)
    -- # a_find_using_desc_index - do we construct ES for search using DESCENDING index
    --   (==> it will use "where id <= :r order by id DESC" rather than "where id >= :r order by id ASC")
    -- [only when TIL = RC] Repeats <fn_internal_retry_count()> times if result is null
    -- (possible if bounds of IDs has been changed since previous call)

    v_this = trim(a_view_for_search);

    -- max difference b`etween min_id and max_id to allow scan random id via
    -- select id from <a_view_for_search> rows :x to :y, where x = y = random_int
    fn_internal_max_rows_usage = cast( rdb$get_context('USER_SESSION','RANDOM_SEEK_VIA_ROWS_LIMIT') as int);

    -- Use either stub or non-empty executing code (depends on was 'oltp_dump.sql' compiled or no):
    -- save fact of usage views in the table `z_used_views`:
    execute procedure z_remember_view_usage(a_view_for_search, a_view_for_min_id, a_view_for_max_id);

    a_view_for_min_id = coalesce( a_view_for_min_id, a_view_for_search );
    a_view_for_max_id = coalesce( a_view_for_max_id, a_view_for_min_id, a_view_for_search );

    if ( rdb$get_context('USER_TRANSACTION', upper(:a_view_for_min_id)||'_ID_MIN' ) is null
       or
       rdb$get_context('USER_TRANSACTION', upper(:a_view_for_max_id)||'_ID_MAX' ) is null
     ) then
    begin
        execute procedure sp_add_perf_log(1, a_view_for_min_id );
        -- v`iew z_get_min_max_id may be used to see average, min and max elapsed time
        -- of this sttm:
        v_stt='select min(id)-0.5 from '|| a_view_for_min_id;
        execute statement (:v_stt) into id_min; -- do via ES in order to see statistics in the TRACE!
        execute procedure sp_add_perf_log(0, a_view_for_min_id, null, 'id_min='||coalesce(id_min,'<?>') );

        if ( id_min is NOT null ) then -- ==> source <a_view_for_min_id> is NOT empty
        begin

            execute procedure sp_add_perf_log(1, a_view_for_max_id );
            -- v`iew z_get_min_max_id may be used to see average, min and max elapsed time
            -- of this sttm:
            v_stt='select max(id)+0.5 from '|| a_view_for_max_id;
            execute statement (:v_stt) into id_max; -- do via ES in order to see statistics in the TRACE!
            execute procedure sp_add_perf_log(0, a_view_for_max_id, null, 'id_max='||coalesce(id_max,'<?>') );

            if ( id_max is NOT null  ) then -- ==> source <a_view_for_max_id> is NOT empty
            begin
                -- Save values for subsequent calls of this func in this tx (minimize DB access)
                -- (limit will never change in SNAPSHOT and can change with low probability in RC):
                rdb$set_context('USER_TRANSACTION', upper(:a_view_for_min_id)||'_ID_MIN', :id_min);
                rdb$set_context('USER_TRANSACTION', upper(:a_view_for_max_id)||'_ID_MAX', :id_max);
        
                if ( id_max - id_min < fn_internal_max_rows_usage ) then
                begin
                    -- when difference b`etween id_min and id_max is not too high, we can simple count rows:
                    execute statement 'select count(*) from '||a_view_for_search into v_rows;
                    rdb$set_context('USER_TRANSACTION', upper(:a_view_for_search)||'_COUNT', v_rows );
                end
            end -- id_max is NOT null 
        end -- id_min is NOT null
    end
    else begin
        -- minimize database access! Performance on 10'000 loops: 1485 ==> 590 ms
        id_min=cast( rdb$get_context('USER_TRANSACTION', upper(:a_view_for_min_id)||'_ID_MIN' ) as double precision);
        id_max=cast( rdb$get_context('USER_TRANSACTION', upper(:a_view_for_max_id)||'_ID_MAX' ) as double precision);
        v_rows=cast( rdb$get_context('USER_TRANSACTION', upper(:a_view_for_search)||'_COUNT') as int);
    end

    if ( id_max - id_min < fn_internal_max_rows_usage ) then
        begin
            v_stt='select id from '||a_view_for_search||' rows :x to :y'; -- ::: nb ::: `ORDER` clause not needed here!
        end
    else
        begin
            -- 17.07.2014: for some cases it is ALLOWED to query random ID without "ORDER BY"
            -- clause because this ID will be handled in such manner that it will be REMOVED
            -- after this handling from the scope of view! Samples of such cases are:
            -- sp_cancel_supplier_order, sp_cancel_supplier_invoice, sp_cancel_customer_reserve
            v_stt='select id from '
                ||a_view_for_search
                ||iif(a_find_using_desc_index = 0, ' where id >= :x', ' where id <= :x');
            if ( a_can_skip_order_clause = 0 ) then
                v_stt = v_stt || iif(a_find_using_desc_index = 0, ' order by id     ', ' order by id desc');
            v_stt = v_stt || ' rows 1';
        end

    i = a_count_to_generate;
    while ( i > 0 ) do
    begin
        id_selected = null;
        if ( id_max - id_min < fn_internal_max_rows_usage ) then
            begin
                id_random = ceiling( rand() * v_rows );
                execute statement (:v_stt) (x := id_random, y := id_random) into id_selected;
            end
        else
            begin
                id_random = cast( id_min + rand() * (id_max - id_min) as bigint);
                execute statement (:v_stt) (x := id_random) into id_selected;
            end

        if ( id_selected is null and coalesce(a_raise_exc, 1) = 1 ) then
            begin
        
                v_info = 'view: '||:a_view_for_search;
                if ( id_min is NOT null ) then
                   v_info = v_info || ', id_min=' || id_min || ', id_max='||id_max;
                else
                   v_info = v_info || ' - EMPTY';
        
                v_info = v_info ||', id_rnd='||coalesce(id_random,'<null>');
        
                -- 'no id @1 @2 in @3 found within scope @4 ... @5'; -- @1 is '>=' or '<='; @2 = random_selected_value; @3 = data source; @4 = min; @5 = max
                exception ex_can_not_select_random_id
                    using(
                             iif(a_find_using_desc_index = 0,'>=','<=')
                            ,coalesce(id_random,'<?>')
                            ,a_view_for_search
                            ,coalesce(id_min,'<?>')
                            ,coalesce(id_max,'<?>')
                         );
            end
        else
            suspend;

        i = i - 1;
    end

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            v_stt,
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_get_random_id

create or alter procedure sp_lock_selected_doc(
    doc_list_id type of dm_idb,
    a_view_for_search dm_dbobj, --  'v_reserve_write_off',
    a_selected_doc_id type of dm_idb default null
) as
    declare v_dbkey dm_dbkey = null;
    declare v_id dm_idb;
    declare v_stt varchar(255);
    declare v_exc_info dm_info;
    declare v_info dm_info;
    declare v_this dm_dbobj = 'sp_lock_selected_doc';
begin
    -- Seeks selected id in doc_list with checking existence
    -- of this ID in a_view_for_search (if need).
    -- Raises exc if not found, otherwise tries to lock ID in doc_list

    v_info = 'doc_id='||coalesce(doc_list_id, '<?>')||', src='||a_view_for_search;
    execute procedure sp_add_perf_log(1, v_this, null, v_info);

    v_stt =
          'select rdb$db_key from doc_list'
        ||' where '
        ||' id = :x'
        ||' and ( :y is null' -- ==> doc_list_id was defined via sp_get_random_id
        ||' or'
        ||' exists( select id from '||a_view_for_search||' v where v.id = :x )'
        ||')';

    execute statement (v_stt) ( x := :doc_list_id, y := :a_selected_doc_id ) into v_dbkey;

    if ( v_dbkey is null ) then
    begin
        -- no document found for handling in datasource = ''@1'' with id=@2';
        exception ex_no_doc_found_for_handling using( a_view_for_search, :doc_list_id );
    end
    rdb$set_context('USER_SESSION','ADD_INFO','doc='||v_id||': try to lock'); -- to be displayed in log of 1run_oltp_emul.bat

    select id from doc_list h
    where h.rdb$db_key = :v_dbkey
    for update with lock
    into v_id; -- trace rows: deadlock; update conflicts with conc.; 335544878 conc tran number is ...; at proc <this>
    rdb$set_context('USER_SESSION','ADD_INFO','doc='||v_id||': captured Ok'); -- to be displayed in log of 1run_oltp_emul.bat

    execute procedure sp_add_perf_log(0, v_this);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log( '', gdscode, v_info, v_this );
        --########
        exception; -- all number of retries exceeded: raise concurrent_transaction OR deadlock
        --########
    end
end

^ -- sp_lock_selected_doc

create or alter procedure sp_cache_rules_for_distr( a_table dm_dbobj )
returns(
    mode dm_name,
    snd_optype_id  bigint,
    rcv_optype_id  bigint,
    rows_to_multiply int
)
as
    declare v_ctx_prefix type of dm_ctxnv;
    declare v_stt varchar(255);
    declare i int;
    declare v_mode dm_name;
    declare v_snd_optype_id type of dm_idb;
    declare v_rcv_optype_id type of dm_idb;
    declare v_rows_to_multiply int;
begin
    if ( upper(coalesce(a_table,'')) not in ( upper('QDISTR'), upper('PDISTR') ) )
    then
      exception ex_bad_argument using( coalesce(a_table,'<null>'), 'sp_cache_rules_for_distr' ); --  'argument @1 passed to unit @2 is invalid';

    -- cache records from rules_for_Qdistr and rules_for_Pdistr in context variables
    -- for fast output of them (without database access)

    v_ctx_prefix = 'MEM_TABLE_'||upper(a_table)||'_'; -- 'MEM_TABLE_QDISTR' or 'MEM_TABLE_PDISTR'
    v_stt='select mode, snd_optype_id, rcv_optype_id' || iif( upper(a_table)=upper('QDISTR'),', storno_sub',', rows_to_multiply ' )
          ||' from rules_for_'||a_table;
    if ( rdb$get_context('USER_SESSION', v_ctx_prefix||'CNT') is null ) then
    begin
        i = 1;
        for
            execute statement( v_stt )
            into v_mode, v_snd_optype_id, v_rcv_optype_id, v_rows_to_multiply
        do begin
            rdb$set_context(
            'USER_SESSION'
            ,v_ctx_prefix||i
            ,rpad( v_mode ,80,' ')
             || coalesce( cast(v_snd_optype_id as char(18)), rpad('', 18,' ') )
             || coalesce( cast(v_rcv_optype_id as char(18)), rpad('', 18,' ') )
             || coalesce( cast(v_rows_to_multiply as char(10)), rpad('', 10,' ') )
            );
            rdb$set_context('USER_SESSION', v_ctx_prefix||'CNT', i);
            i = i+1;
        end
    end
    i = 1;
    while ( i <= cast(rdb$get_context('USER_SESSION', v_ctx_prefix||'CNT') as int) )
    do begin
        mode = trim( substring( rdb$get_context('USER_SESSION', v_ctx_prefix||i) from 1 for 80 ) );
        snd_optype_id = cast( nullif(trim(substring( rdb$get_context('USER_SESSION', v_ctx_prefix||i) from 81 for 18 )), '') as dm_idb);
        rcv_optype_id = cast( nullif(trim(substring( rdb$get_context('USER_SESSION', v_ctx_prefix||i) from 99 for 18 )), '') as dm_idb);
        rows_to_multiply = cast( nullif(trim(substring( rdb$get_context('USER_SESSION', v_ctx_prefix||i) from 117 for 10 )), '') as int);
        suspend;
        i = i+1;
    end
end

^ -- sp_cache_rules_for_distr

create or alter procedure sp_rules_for_qdistr
returns(
    mode dm_name,
    snd_optype_id  bigint,
    rcv_optype_id  bigint,
    storno_sub smallint -- 28.07.2014
) as
begin
  for
      select p.mode,p.snd_optype_id, p.rcv_optype_id,
             p.rows_to_multiply -- 28.07.2014
      from sp_cache_rules_for_distr('QDISTR') p
      into mode, snd_optype_id, rcv_optype_id,
           storno_sub -- 28.07.2014
  do
      suspend;
end

^ -- sp_rules_for_qdistr

create or alter procedure sp_rules_for_pdistr
returns(
    snd_optype_id  bigint,
    rcv_optype_id  bigint,
    rows_to_multiply int
)
as
begin
  for
      select p.snd_optype_id, p.rcv_optype_id, p.rows_to_multiply
      from sp_cache_rules_for_distr('PDISTR') p
      into snd_optype_id, rcv_optype_id, rows_to_multiply
  do
      suspend;
end

^ -- sp_rules_for_pdistr

set term ;^



-- STUB, need for sp_multiply_rows_for_qdistr; will be redefined after (see below):
create or alter view v_our_firm as select 1 id from rdb$database
;

-- Views for usage in procedure s~p_multiply_rows_for_qdistr.
-- Their definition will be REPLACED with 'select * from XQD_1000_1200' and
-- 'select * from XQD_1000_3300' if config 'create_with_split_heavy_tabs' = 1

create or alter view v_qdistr_multiply_1 as
select * from qdistr
;

create or alter view v_qdistr_multiply_2 as
select * from qdistr
;

-- 07.09.2015: probe to replace ES in all cases of fn_get_rand_id:
create or alter view name$to$substutite$min$id$ as select 1 id from rdb$database;
create or alter view name$to$substutite$max$id$ as select 1 id from rdb$database;
create or alter view name$to$substutite$search$ as select 1 id from rdb$database;

-- Updatable views (one-to-one data projections) for handling rows in heavy loaded
-- tables QDistr/QStorned (or in XQD_*, XQS_* when config par. create_with_split_heavy_tabs = 1):
-- See usage in SP_KILL_QSTORNO_RET_QS2QD and also in redirection procedures that
-- can be replaced when 'create_with_split_heavy_tabs=1':
-- sp_ret_qs2qd_on_canc_wroff, sp_ret_qs2qd_on_canc_reserve,
-- sp_ret_qs2qd_on_canc_invoice, sp_ret_qs2qd_on_canc_supp_order

create or alter view v_qdistr_source_1 as
select *
from qdistr
;

create or alter view v_qdistr_source_2 as
select *
from qdistr
;

create or alter view v_qdistr_target_1 as
select *
from qdistr
;

create or alter view v_qdistr_target_2 as
select *
from qdistr
;

create or alter view v_qstorned_target_1 as
select *
from qstorned
;

create or alter view v_qstorned_target_2 as
select *
from qstorned
;

create or alter view v_qdistr_name_for_del as
select *
from qdistr
;

create or alter view v_qdistr_name_for_ins as
select *
from qdistr
;

create or alter view v_qstorno_name_for_del as
select *
from qstorned
;

create or alter view v_qstorno_name_for_ins as
select *
from qstorned
;



set term ^;
create or alter procedure sp_multiply_rows_for_qdistr(
    a_doc_list_id dm_idb,
    a_optype_id dm_idb,
    a_clo_for_our_firm dm_idb,
    a_qty_sum dm_qty
) as
    declare c_gen_inc_step_qd int = 100; -- size of `batch` for get at once new IDs for QDistr (reduce lock-contention of gen page)
    declare v_gen_inc_iter_qd int; -- increments from 1  up to c_gen_inc_step_qd and then restarts again from 1
    declare v_gen_inc_last_qd dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_qd)
    declare v_doc_data_id dm_idb;
    declare v_ware_id dm_idb;
    declare v_qty_for_distr dm_qty;
    declare v_purchase_for_distr type of dm_cost;
    declare v_retail_for_distr type of dm_cost;
    declare v_rcv_optype_id type of dm_idb;
    declare n_rows_to_add int;
    declare v_qty_for_one_row type of dm_qty;
    declare v_qty_acc type of dm_qty;
    declare v_purchase_acc type of dm_cost;
    declare v_retail_acc type of dm_cost;
    declare v_dbkey dm_dbkey;
    declare v_info dm_info;
    declare v_this dm_dbobj = 'sp_multiply_rows_for_qdistr';
    declare v_storno_sub smallint;
begin
    -- Performs "value-to-rows" filling of QDISTR table: add rows which
    -- later will be "storned" (removed from qdistr to qstorned)

    v_info = 'dh='||a_doc_list_id||', q_sum='||a_qty_sum;
    execute procedure sp_add_perf_log(1, v_this, null, v_info);

    v_gen_inc_iter_qd = 1;
    c_gen_inc_step_qd = (1 + a_qty_sum) * iif(a_clo_for_our_firm=1, 1, 2) + 1;
    -- take bulk IDs at once (reduce lock-contention for GEN page):
    v_gen_inc_last_qd = gen_id( g_qdistr, :c_gen_inc_step_qd );

    -- Cursor: how many distributions must be done for this doc if it is "sender" ?
    -- =2 for customer order (if agent <> our firm!!):
    --    it will be storned by stock order
    --    and later by customer reserve
    -- =1 for all other operations:
    for
        select r.rcv_optype_id, c.snd_id, c.id as ware_id, c.qty, c.cost_purchase, c.cost_retail, r.storno_sub
        from rules_for_qdistr r
        cross join tmp$shopping_cart c
        where
            r.snd_optype_id = :a_optype_id
            and (
                :a_clo_for_our_firm = 0
                or
                -- do NOT multiply rows for rcv_op = 'RES' if current doc = client order for OUR firm!
                :a_clo_for_our_firm = 1 and r.rcv_optype_id <> 3300 -- fn_oper_retail_reserve()
            )
        into v_rcv_optype_id, v_doc_data_id, v_ware_id, v_qty_for_distr, v_purchase_for_distr, v_retail_for_distr, v_storno_sub
    do
    begin
        v_qty_acc = 0;
        v_purchase_acc = 0;
        v_retail_acc = 0;
        n_rows_to_add = ceiling( v_qty_for_distr );
        while( n_rows_to_add > 0 ) do
        begin
            v_qty_for_one_row = iif( n_rows_to_add > v_qty_for_distr, n_rows_to_add - v_qty_for_distr, 1 );
            if ( v_storno_sub = 1 ) then
                insert into v_qdistr_multiply_1 (
                    id,
                    doc_id,
                    ware_id,
                    snd_optype_id,
                    rcv_optype_id,
                    snd_id,
                    snd_qty,
                    snd_purchase,
                    snd_retail)
                values(
                    :v_gen_inc_last_qd - ( :c_gen_inc_step_qd - :v_gen_inc_iter_qd ), -- iter=1: 12345 - (100-1); iter=2: 12345 - (100-2); ...; iter=100: 12345 - (100-100)
                    :a_doc_list_id,
                    :v_ware_id,
                    :a_optype_id,
                    :v_rcv_optype_id,
                    :v_doc_data_id,
                    :v_qty_for_one_row,
                    :v_purchase_for_distr * :v_qty_for_one_row / :v_qty_for_distr,
                    :v_retail_for_distr * :v_qty_for_one_row / :v_qty_for_distr
                )
                returning
                    rdb$db_key,
                    :v_qty_acc + snd_qty,
                    :v_purchase_acc + snd_purchase,
                    :v_retail_acc + snd_retail
                into
                    v_dbkey,
                    :v_qty_acc,
                    :v_purchase_acc,
                    :v_retail_acc
                ;
            else
                insert into v_qdistr_multiply_2 (
                    id,
                    doc_id,
                    ware_id,
                    snd_optype_id,
                    rcv_optype_id,
                    snd_id,
                    snd_qty,
                    snd_purchase,
                    snd_retail)
                values(
                    :v_gen_inc_last_qd - ( :c_gen_inc_step_qd - :v_gen_inc_iter_qd ), -- iter=1: 12345 - (100-1); iter=2: 12345 - (100-2); ...; iter=100: 12345 - (100-100)
                    :a_doc_list_id,
                    :v_ware_id,
                    :a_optype_id,
                    :v_rcv_optype_id,
                    :v_doc_data_id,
                    :v_qty_for_one_row,
                    :v_purchase_for_distr * :v_qty_for_one_row / :v_qty_for_distr,
                    :v_retail_for_distr * :v_qty_for_one_row / :v_qty_for_distr
                )
                returning
                    rdb$db_key,
                    :v_qty_acc + snd_qty,
                    :v_purchase_acc + snd_purchase,
                    :v_retail_acc + snd_retail
                into
                    v_dbkey,
                    :v_qty_acc,
                    :v_purchase_acc,
                    :v_retail_acc
                ;

            n_rows_to_add = n_rows_to_add - 1;
            v_gen_inc_iter_qd = v_gen_inc_iter_qd + 1;

            if ( n_rows_to_add = 0 and
                 ( v_qty_acc <> v_qty_for_distr
                   or v_purchase_acc <> v_purchase_for_distr
                   or v_retail_acc <> v_retail_for_distr
                 )
               ) then
               if ( v_storno_sub = 1 ) then
                   update v_qdistr_multiply_1 q set
                       q.snd_qty = q.snd_qty + ( :v_qty_for_distr - :v_qty_acc ),
                       q.snd_purchase = q.snd_purchase + ( :v_purchase_for_distr - :v_purchase_acc ),
                       q.snd_retail = q.snd_retail + ( :v_retail_for_distr - :v_retail_acc )
                   where q.rdb$db_key = :v_dbkey;
               else
                   update v_qdistr_multiply_2 q set
                       q.snd_qty = q.snd_qty + ( :v_qty_for_distr - :v_qty_acc ),
                       q.snd_purchase = q.snd_purchase + ( :v_purchase_for_distr - :v_purchase_acc ),
                       q.snd_retail = q.snd_retail + ( :v_retail_for_distr - :v_retail_acc )
                   where q.rdb$db_key = :v_dbkey;
            else
                if ( v_gen_inc_iter_qd = c_gen_inc_step_qd ) then -- its time to get another batch of IDs
                begin
                    v_gen_inc_iter_qd = 1;
                    -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                    v_gen_inc_last_qd = gen_id( g_qdistr, :c_gen_inc_step_qd );
                end
        end -- while( n_rows_to_add > 0 )
    end -- cursor on doc_data cross join rules_for_qdistr

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^   -- sp_multiply_rows_for_qdistr

create or alter procedure sp_multiply_rows_for_pdistr(
    a_doc_list_id dm_idb,
    a_agent_id dm_idb,
    a_optype_id dm_idb,
    a_cost_for_distr type of dm_cost
) as
    declare v_rcv_optype_id type of dm_idb;
    declare n_rows_to_add int;
    declare v_dbkey dm_dbkey;
    declare v_cost_acc type of dm_cost;
    declare v_cost_for_one_row type of dm_cost;
    declare v_cost_div int;
    declare v_id dm_idb;
    declare v_key type of dm_unit;
    declare v_info dm_info;
    declare v_this dm_dbobj = 'sp_multiply_rows_for_pdistr';
    declare function fn_internal_min_cost_4_split returns int deterministic as
    begin
        return cast(rdb$get_context('USER_SESSION', 'C_MIN_COST_TO_BE_SPLITTED' ) as int);
    end
begin
    -- Performs "cost-to-rows" filling of PDISTR table: add rows which
    -- later will be "storned" (removed from pdistr to pstorned)
    v_info = 'dh='||a_doc_list_id||', op='||a_optype_id||', $='||a_cost_for_distr;
    execute procedure sp_add_perf_log(1, v_this, null, v_info);

    for
        select r.rcv_optype_id, iif( :a_cost_for_distr < fn_internal_min_cost_4_split(), 1, r.rows_to_multiply )
        from sp_rules_for_pdistr r
        where r.snd_optype_id = :a_optype_id
        into v_rcv_optype_id, n_rows_to_add
    do
    begin
        v_cost_acc = 0;
        v_cost_div = round( a_cost_for_distr / n_rows_to_add, -2 ); -- round to handreds
        while( v_cost_acc < a_cost_for_distr ) do
        begin
            v_cost_for_one_row = iif( a_cost_for_distr > v_cost_div, v_cost_div, a_cost_for_distr );

            insert into pdistr(
                agent_id,
                snd_optype_id,
                snd_id,
                snd_cost,
                rcv_optype_id
            )
            values(
                :a_agent_id,
                :a_optype_id,
                :a_doc_list_id,
                :v_cost_for_one_row,
                :v_rcv_optype_id
            )
            returning
                rdb$db_key,
                :v_cost_acc + snd_cost
            into
                v_dbkey,
                :v_cost_acc
            ;

            if ( v_cost_acc > a_cost_for_distr ) then
               update pdistr p set
                   p.snd_cost = p.snd_cost + ( :a_cost_for_distr - :v_cost_acc )
               where p.rdb$db_key = :v_dbkey;
        end
    end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^   -- sp_multiply_rows_for_pdistr

create or alter procedure sp_make_cost_storno(
    a_doc_id dm_idb, -- sp_add_invoice_to_stock: invoice_id;  sp_reserve_write_off: reserve_id
    a_optype_id dm_idb,
    a_agent_id dm_idb,
    a_cost_diff dm_cost
)
as
    declare v_pass smallint;
    declare not_storned_cost type of dm_cost;
    declare v_storned_cost type of dm_cost;
    declare v_storned_acc type of dm_cost;
    declare v_storned_doc_optype_id type of dm_idb;
    declare v_this dm_dbobj = 'sp_make_cost_storno';
    declare v_rows int = 0;
    declare v_lock int = 0;
    declare v_skip int = 0;
    declare v_sign dm_sign;
begin
    -- Performs attempt to make storno of:
    -- 1) payment docs by cost of stock document which state is changed
    --    to "closed"(sp_add_invoice_to_stock or sp_reserve_write_off)
    -- 2) old stock documents when adding new payment (sp_pay_from_customer, sp_pay_to_supplier)
    -- ::: nb ::: If record in "source" table (pdistr) can`t be locked - SKIP it
    -- and try to lock next one (in order to reduce number of lock conflicts)

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    v_storned_acc = 0;
    v_pass = 1;
    v_sign = iif( bin_and(current_transaction, 1)=0, 1, -1);

    while ( v_pass <= 2 ) do
    begin
        select r.snd_optype_id -- iif( :v_pass=1, r.snd_optype_id, r.rcv_optype_id )
        from sp_rules_for_pdistr r
        where iif( :v_pass = 1, r.rcv_optype_id, r.snd_optype_id ) = :a_optype_id
        into v_storned_doc_optype_id; -- sp_add_invoice_to_stock ==> v_storned_doc_optype_id = fn_oper_pay_to_supplier()
    
        not_storned_cost = iif( v_pass=1, :a_cost_diff, v_storned_acc);

        if ( not_storned_cost <= 0 ) then leave;

        for
            select
                p.rdb$db_key as dbkey,
                p.id,
                p.agent_id,
                p.snd_optype_id,
                p.snd_id,
                p.snd_cost as cost_to_be_storned,
                p.rcv_optype_id,
                :a_doc_id as rcv_id
            from pdistr p
            where p.agent_id = :a_agent_id and p.snd_optype_id = :v_storned_doc_optype_id
            order by
                p.snd_id+0 -- 23.07.2014: PLAN SORT (P INDEX (PDISTR_AGENT_ID)
                ,:v_sign * p.id -- attempt to reduce lock conflicts: odd and even Tx handling the same doc must have a chance do not encounter locked rows at all
            --order by p.id (wrong if new pdistr.id is generated via sequence when records returns from pstorned)
            as cursor c
        do
        begin
            v_rows = v_rows + 1;

            -- 26.10.2015. Additional begin..end block needs for providing DML
            -- 'atomicity' of BOTH tables pdistr & pstorned! Otherwise changes
            -- can become inconsistent if online validation will catch table-2
            -- after this code finish changes on table-1 but BEFORE it will
            -- start to change table-2.
            -- See CORE-4973 (example of WRONG code which did not used this addi block!)
            begin

                -- Explicitly lock record; skip to next if it is already locked
                -- (see below `when` section: supress all lock_conflict kind exc)
                -- benchmark: sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1108762&msg=16393721
                update pdistr set id = id where current of c; -- faster than 'where rdb$db_key = ...'
    
                v_storned_cost = minvalue( :not_storned_cost, c.cost_to_be_storned );
        
                -- move into `storage` table *PART* of prepayment that is now storned
                -- by just created customer reserve:
                -- :: nb :: pstorned PK = (id, rcv_id) - compound!
                if ( v_pass = 1 ) then
                begin
                    insert into pstorned(
                        agent_id,
                        snd_optype_id,
                        snd_id,
                        snd_cost,
                        rcv_optype_id,
                        rcv_id,
                        rcv_cost
                    )
                    values(
                        c.agent_id,
                        c.snd_optype_id,
                        c.snd_id,
                        :v_storned_cost, -- s.cost_to_be_storned,
                        c.rcv_optype_id,
                        c.rcv_id,
                        :v_storned_cost
                    );
                end
    
                if ( c.cost_to_be_storned = v_storned_cost ) then
                    delete from pdistr p where current of c;
                else
                    -- leave this record for futher storning (it has rest of cost > 0!):
                    update pdistr p set p.snd_cost = p.snd_cost - :v_storned_cost where current of c;
        
                not_storned_cost = not_storned_cost - v_storned_cost;
                v_lock = v_lock + 1;
    
                if ( v_pass = 1 ) then
                    v_storned_acc = v_storned_acc + v_storned_cost; -- used in v_pass = 2
    
                if ( not_storned_cost <= 0 ) then leave;
            end -- atomicity of changes several tables (CORE-4973!)
        when any do
            -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
            -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
            -- catched it's kind of exception!
            -- 1) tracker.firebirdsql.org/browse/CORE-3275
            --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
            -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
            begin
                if ( fn_is_lock_trouble( gdscode ) ) then
                    -- suppress this exc! we'll skip to next row of pdistr
                    v_skip = v_skip + 1;
                else -- some other ex`ception
                    --#######
                    exception;  -- ::: nb ::: anonimous but in when-block!
                    --#######
            end

        end -- cursor on pdistr
    
        v_pass = v_pass + 1;
    end -- v_pass=1..2

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(
        0,
        v_this,
        null,
        'dh='||coalesce(:a_doc_id,'<?>')
        ||', pd ('||iif(:v_sign=1,'asc','dec')||'): capt='||:v_lock||', skip='||:v_skip||', scan='||:v_rows
    );

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||a_doc_id,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^ -- sp_make_cost_storno

create or alter procedure sp_kill_cost_storno( a_deleted_or_cancelled_doc_id dm_idb ) as
    declare agent_id dm_idb;
    declare snd_optype_id dm_idb;
    declare snd_id dm_idb;
    declare storned_cost dm_cost;
    declare rcv_optype_id dm_idb;
    declare v_msg dm_info;
    declare v_this dm_dbobj = 'sp_kill_cost_storno';
    declare cs cursor for (
        select
            s.agent_id,
            iif(:a_deleted_or_cancelled_doc_id = s.rcv_id, s.snd_optype_id, s.rcv_optype_id) as snd_optype_id,
            iif(:a_deleted_or_cancelled_doc_id = s.rcv_id, s.snd_id, s.rcv_id) as snd_id,
            iif(:a_deleted_or_cancelled_doc_id = s.rcv_id, s.rcv_cost, s.snd_cost) as storned_cost,
            iif(:a_deleted_or_cancelled_doc_id = s.rcv_id, s.rcv_optype_id, s.snd_optype_id) as rcv_optype_id
        from pstorned s
        where
            :a_deleted_or_cancelled_doc_id in (s.rcv_id, s.snd_id)
    );
begin
    -- Called from trg D`OC_LIST_AIUD for operations:
    -- 1) delete document which cost was storned before (e.g. payment to supplier / from customer)
    -- 2) s`p_cancel_adding_invoice, s`p_cancel_write_off (i.e. REVERT state of doc)
    -- :a_deleted_or_cancelled_doc_id = doc
    -- 1) which is removed now
    -- XOR
    -- 2) which operation is cancelled now

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    open cs;
    while (1=1) do
    begin
        fetch cs into agent_id, snd_optype_id, snd_id, storned_cost, rcv_optype_id;
        if ( row_count = 0) then leave;
        -- ::: nb ::: Revert sequence of these two commands if use `as cursor C`.
        -- See CORE-4488 ("Cursor references are not variables, they're not cached
        -- when reading. Instead, they represent the current state of the record")

        -- 04.08.2014: though no updates in statistics for 'select ... for update with lock' engine DOES them!
        -- See benchmark and issue by dimitr:
        -- sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1108762&msg=16394591
        delete from pstorned where current of cs;
        -- insert using variables instead of cursor ref (CORE-4488):
        insert into pdistr
              ( agent_id,   snd_optype_id,   snd_id,   snd_cost,       rcv_optype_id )
        values( :agent_id, :snd_optype_id,  :snd_id,  :storned_cost,   :rcv_optype_id );
    end
    close cs;

    delete from pdistr p where p.snd_id = :a_deleted_or_cancelled_doc_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||a_deleted_or_cancelled_doc_id);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_msg,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_kill_cost_storno

create or alter procedure srv_log_dups_qd_qs( -- needed only in 3.0, SuperCLASSIC, in sep...oct 2014
    a_unit dm_dbobj,
    a_gdscode int,
    a_inserting_table dm_dbobj,
    a_inserting_id type of dm_idb,
    a_inserting_info dm_info
)
as
    declare v_curr_tx bigint;
    declare v_get_stt varchar(512);
    declare v_put_stt varchar(512);
    declare v_doc_id dm_idb;
    declare v_ware_id dm_idb;
    declare v_snd_optype_id dm_idb;
    declare v_snd_id dm_idb;
    declare v_snd_qty dm_qty;
    declare v_rcv_optype_id dm_idb;
    declare v_rcv_id dm_idb;
    declare v_rcv_qty dm_qty;
    declare v_snd_purchase dm_cost;
    declare v_snd_retail dm_cost;
    declare v_rcv_purchase dm_cost;
    declare v_rcv_retail dm_cost;
    declare v_trn_id dm_idb;
    declare v_dts timestamp;
begin
    -- 09.10.2014, continuing trouble with PK violations in 3.0 SuperCLASSIC.
    -- Add log info using auton Tx when PK violation occurs in QDistr or QStorned.
    -- 08.01.2014: replace wrong algorithm that ignored invisible data for auton Tx
    v_curr_tx = current_transaction;
    v_get_stt = 'select doc_id, ware_id, snd_optype_id, snd_id, snd_qty,'
            ||'rcv_optype_id, rcv_id, rcv_qty, snd_purchase, snd_retail,'
            ||'rcv_purchase,rcv_retail,trn_id,dts'
            ||' from '|| a_inserting_table ||' q'
            ||' where q.id = :x';

    v_put_stt = 'insert into '
            || iif( upper(a_inserting_table)=upper('QDISTR'), 'ZQdistr', 'ZQStorned' )
            ||'( id, doc_id, ware_id, snd_optype_id, snd_id, snd_qty,'
            ||'  rcv_optype_id, rcv_id, rcv_qty, snd_purchase, snd_retail,'
            ||'  rcv_purchase, rcv_retail, trn_id, dts, dump_att, dump_trn'
            ||') values '
            ||'(:id,:doc_id,:ware_id,:snd_optype_id,:snd_id,:snd_qty,'
            ||' :rcv_optype_id,:rcv_id,:rcv_qty,:snd_purchase,:snd_retail,'
            ||' :rcv_purchase,:rcv_retail,:trn_id,:dts,:dump_att,:dump_trn'
            ||')';


    execute statement (v_get_stt) ( x := a_inserting_id )
    into
        v_doc_id,
        v_ware_id,
        v_snd_optype_id,
        v_snd_id,
        v_snd_qty,
        v_rcv_optype_id,
        v_rcv_id,
        v_rcv_qty,
        v_snd_purchase,
        v_snd_retail,
        v_rcv_purchase,
        v_rcv_retail,
        v_trn_id,
        v_dts;

    in autonomous transaction do
    begin

        insert into perf_log( unit, exc_unit, fb_gdscode, trn_id, info, stack )
        values ( :a_unit, 'U', :a_gdscode, :v_curr_tx, :a_inserting_info, fn_get_stack( 1 ) );

        execute statement ( v_put_stt )
        (
            id  := a_inserting_id,
            doc_id := v_doc_id,
            ware_id := v_ware_id,
            snd_optype_id := v_snd_optype_id,
            snd_id := v_snd_id,
            snd_qty := v_snd_qty,
            rcv_optype_id := v_rcv_optype_id,
            rcv_id := v_rcv_id,
            rcv_qty := v_rcv_qty,
            snd_purchase := v_snd_purchase,
            snd_retail := v_snd_retail,
            rcv_purchase := v_rcv_purchase,
            rcv_retail := v_rcv_retail,
            trn_id := v_trn_id,
            dts := v_dts,
            dump_att := current_connection,
            dump_trn := v_curr_tx
        );

    end -- in auton Tx
end

^ -- srv_log_dups_qd_qs

create or alter procedure sp_kill_qstorno_ret_qs2qd(
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_deleting_doc dm_sign,
    a_aux_handling dm_sign default 0
)
as
    declare c_gen_inc_step_nt int = 100; -- size of `batch` for get at once new IDs for QDistr (reduce lock-contention of gen page)
    declare v_gen_inc_iter_nt int; -- increments from 1  up to c_gen_inc_step_nt and then restarts again from 1
    declare v_gen_inc_last_nt dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_nt)
    declare v_this dm_dbobj = 'sp_kill_qstorno_ret_qs2qd';
    declare v_call dm_dbobj;
    declare v_info dm_info;
    declare v_suffix dm_info;
    declare i int  = 0;
    declare k int  = 0;
    declare v_dd_id dm_idb;
    declare v_id dm_idb;
    declare v_doc_id dm_idb;
    declare v_doc_optype dm_idb;
    declare v_dd_ware_id dm_idb;
    declare v_dd_qty dm_qty;
    declare v_dd_cost dm_qty;
    declare v_doc_pref dm_mcode;
    declare v_snd_optype_id dm_idb;
    declare v_snd_id dm_idb;
    declare v_snd_qty dm_qty;
    declare v_rcv_optype_id dm_idb;
    declare v_rcv_id dm_idb;
    declare v_rcv_qty dm_qty;
    declare v_snd_purchase dm_cost;
    declare v_snd_retail dm_cost;
    declare v_rcv_purchase dm_cost;
    declare v_rcv_retail dm_cost;
    declare v_log_cursor dm_dbobj; -- 4debug only
    declare v_ret_cursor dm_dbobj; -- 4debug only
    declare v_oper_retail_realization dm_idb;
    declare v_old_rcv_optype type of dm_idb;

    declare c_dd_rows_for_doc cursor for (
        -- used to immediatelly delete record in doc_data when document
        -- is to be deleted (avoid scanning doc_data rows in FK-trigger again)
        select d.id, d.ware_id, d.qty, d.cost_purchase
        from doc_data d
        where d.doc_id = :a_doc_id
    );

    declare c_qd_rows_for_doc cursor for (
        select id
        from v_qdistr_name_for_del q -- this name will be replaced with 'autogen_qdNNNN' when config 'create_with_split_heavy_tabs=1'
        where
            q.ware_id = :v_dd_ware_id
            and q.snd_optype_id = :a_old_optype
            and q.rcv_optype_id = :v_old_rcv_optype
            and q.snd_id = :v_dd_id
    );

    declare c_ret_qs2qd_by_rcv cursor for (
        select
             qs.id
            ,qs.doc_id
            ,qs.snd_optype_id
            ,qs.snd_id
            ,qs.snd_qty
            ,qs.rcv_optype_id
            ,null as rcv_id
            ,null as rcv_qty
            ,qs.snd_purchase
            ,qs.snd_retail
            ,null as rcv_purchase
            ,null as rcv_retail
        from v_qstorno_name_for_del qs -- this name will be replaced with 'autogen_qdNNNN' when config 'create_with_split_heavy_tabs=1'
        where qs.rcv_id = :v_dd_id -- for all cancel ops except sp_cancel_wroff
    );

    declare c_ret_qs2qd_by_snd cursor for (
        select
             qs.id
            ,qs.doc_id
            ,qs.snd_optype_id
            ,qs.snd_id
            ,qs.snd_qty
            ,qs.rcv_optype_id
            ,null as rcv_id
            ,null as rcv_qty
            ,qs.snd_purchase
            ,qs.snd_retail
            ,null as rcv_purchase
            ,null as rcv_retail
        from v_qstorno_name_for_del qs -- this name will be replaced with 'autogen_qdNNNN' when config 'create_with_split_heavy_tabs=1'
        where qs.snd_id = :v_dd_id -- for sp_cancel_wroff
    );
begin
    -- Aux SP, called from sp_kill_qty_storno for
    -- 1) sp_cancel_wroff (a_deleting = 0!) or
    -- 2) all doc removals (sp_cancel_xxx, a_deleting = 1)

    v_call = v_this;
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_call,null);

    v_oper_retail_realization = fn_oper_retail_realization();
    v_doc_pref = fn_mcode_for_oper(a_old_optype);

    select r.rcv_optype_id
    from rules_for_qdistr r
    where
        r.snd_optype_id = :a_old_optype
        and coalesce(r.storno_sub,1) = 1 -- nb: old_op=2000 ==> storno_sub=NULL!
    into v_old_rcv_optype;

    v_gen_inc_iter_nt = 1;
    v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );-- take bulk IDs at once (reduce lock-contention for GEN page)

    -- only for logging in perf_log.info name of handling cursor:
    v_ret_cursor = iif(a_old_optype <> fn_oper_retail_realization(), 'c_ret_qs2qd_by_rcv', 'c_ret_qs2qd_by_snd');

    -- return from QStorned to QDistr records which were previously moved
    -- (when currently deleting doc was created).
    -- Use explicitly declared cursor for immediate removing row from doc_data
    -- when document is to be deleted:
    open c_dd_rows_for_doc;
    while (1=1) do
    begin
        fetch c_dd_rows_for_doc
        into v_dd_id, v_dd_ware_id, v_dd_qty, v_dd_cost;
        if ( row_count = 0 ) then leave;

        if ( a_deleting_doc = 1 and a_aux_handling = 0 ) then
        begin
            v_log_cursor = 'c_qd_rows_for_doc'; -- 4debug
            open c_qd_rows_for_doc;
            while (1=1) do
            begin
                fetch c_qd_rows_for_doc into v_id;
                if ( row_count = 0 ) then leave;
                i = i+1; -- total number of processed rows
                delete from v_qdistr_name_for_del where current of c_qd_rows_for_doc;
            end
            close c_qd_rows_for_doc;
        end
        ----------------------------------------------------------
        if ( a_old_optype <> v_oper_retail_realization ) then
            open c_ret_qs2qd_by_rcv; -- from qstorned qs where qs.RCV_id = :v_dd_id
        else
            open c_ret_qs2qd_by_snd; -- from qstorned where qs.SND_id = :v_dd_id

        v_log_cursor = v_ret_cursor;
        while (1=1) do
        begin
            if ( a_old_optype <> v_oper_retail_realization ) then
                fetch c_ret_qs2qd_by_rcv into
                     v_id
                    ,v_doc_id
                    ,v_snd_optype_id
                    ,v_snd_id
                    ,v_snd_qty
                    ,v_rcv_optype_id
                    ,v_rcv_id
                    ,v_rcv_qty
                    ,v_snd_purchase
                    ,v_snd_retail
                    ,v_rcv_purchase
                    ,v_rcv_retail;
            else
                fetch c_ret_qs2qd_by_snd into
                     v_id
                    ,v_doc_id
                    ,v_snd_optype_id
                    ,v_snd_id
                    ,v_snd_qty
                    ,v_rcv_optype_id
                    ,v_rcv_id
                    ,v_rcv_qty
                    ,v_snd_purchase
                    ,v_snd_retail
                    ,v_rcv_purchase
                    ,v_rcv_retail;

            if ( row_count = 0 ) then leave;
            i = i+1; -- total number of processed rows

            v_suffix = ', id=' || :v_id || ', doc_id=' || :v_doc_id;

            -- debug info for logging in srv_log_dups_qd_qs if PK
            -- violation will occur on INSERT INTO QDISTR statement
            -- (remained for possible analysis):
            v_call = v_this || ':try_del_qstorned';
            v_info = v_ret_cursor
                || ': try DELETE in qStorned'
                || ' where ' || iif(v_ret_cursor = 'c_ret_qs2qd_by_rcv', 'rcv_id =', 'snd_id =') || :v_dd_id
                || v_suffix
            ;

            -- We can try to delete record in QStorned *before* inserting
            -- data in QDistr: all fields from cursor now are in variables.
            -- ::: NB ::: (measurements 28.01-05.02.2015)
            -- replacing qStorned with "unioned-view" based on N tables
            -- and applying "where id = :a" leads to performance DEGRADATION
            -- due to need to have index on ID field in each underlying table.

            -- execute procedure sp_add_perf_log(1, v_call, null, v_info, v_id); -- 10.02.2015, debug
            -- rdb$set_context('USER_TRANSACTION','DBG_RETQS2QD_TRY_DEL_QSTORNO_ID', v_id);

            if ( a_old_optype <> v_oper_retail_realization ) then
                delete from v_qstorno_name_for_del where current of c_ret_qs2qd_by_rcv;
            else
                delete from v_qstorno_name_for_del where current of c_ret_qs2qd_by_snd;

            --rdb$set_context('USER_TRANSACTION','DBG_RETQS2QD_OK_DEL_QSTORNO_ID', v_id);
            --execute procedure sp_add_perf_log(0, v_call);

            -- debug info for logging in srv_log_dups_qd_qs if PK
            -- violation will occur on INSERT INTO QDISTR statement
            -- (remained for possible analysis):
            v_info = v_ret_cursor || ': try INSERT in qDistr' || v_suffix;
            v_call = v_this || ':try_ins_qdistr';

            -- execute procedure sp_add_perf_log(1, v_call, null, v_info, v_id); -- 10.02.2015, debug
            -- rdb$set_context('USER_TRANSACTION','DBG_RETQS2QD_TRY_INS_QDISTR_ID', v_id);

            insert into v_qdistr_name_for_ins(
                id,
                doc_id,
                ware_id,
                snd_optype_id,
                snd_id,
                snd_qty,
                rcv_optype_id,
                rcv_id,
                rcv_qty,
                snd_purchase,
                snd_retail,
                rcv_purchase,
                rcv_retail
            )
            values(
                 :v_id
                ,:v_doc_id
                ,:v_dd_ware_id
                ,:v_snd_optype_id
                ,:v_snd_id
                ,:v_snd_qty
                ,:v_rcv_optype_id
                ,:v_rcv_id
                ,:v_rcv_qty
                ,:v_snd_purchase
                ,:v_snd_retail
                ,:v_rcv_purchase
                ,:v_rcv_retail
            );
            --rdb$set_context('USER_TRANSACTION','DBG_RETQS2QD_OK_INS_QDISTR_ID', v_id);
            --execute procedure sp_add_perf_log(0, v_call);

        when any do
            begin
                if ( fn_is_uniqueness_trouble(gdscode) ) then
                    -- ###############################################
                    -- PK violation on INSERT INTO QDISTR, log this:
                    -- ###############################################
                    -- 12.02.2015: the reason of PK violations is unpredictable order
                    -- of UNDO, ultimately explained by dimitr, see letters in e-mail.
                    -- Also: sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1142271&msg=17257984
                    execute procedure srv_log_dups_qd_qs( -- 09.10.2014: add log info using auton Tx
                        :v_call,
                        gdscode,
                        'qdistr',
                        :v_id,
                        :v_info
                    );

                exception; -- ::: nb ::: anonimous but in when-block!
            end

        end -- cursor c_ret_qs2qd_by  _rcv | _snd

        if ( a_old_optype <> v_oper_retail_realization ) then
            close c_ret_qs2qd_by_rcv;
        else
            close c_ret_qs2qd_by_snd;

        -- 20.09.2014: move here from trigger on doc_list
        -- (reduce scans of doc_data)
        if ( a_aux_handling = 0 ) then
        begin
            insert into invnt_turnover_log(
                 id -- explicitly assign this field in order NOT to call gen_id in trigger (use v_gen_... counter instead)
                ,ware_id
                ,qty_diff
                ,cost_diff
                ,doc_list_id
                ,doc_pref
                ,doc_data_id
                ,optype_id
            ) values (
                 :v_gen_inc_last_nt - ( :c_gen_inc_step_nt - :v_gen_inc_iter_nt )
                ,:v_dd_ware_id
                ,-(:v_dd_qty)
                ,-(:v_dd_cost)
                ,:a_doc_id
                ,:v_doc_pref
                ,:v_dd_id
                ,:a_old_optype
            );

            v_gen_inc_iter_nt = v_gen_inc_iter_nt + 1;
            if ( v_gen_inc_iter_nt = c_gen_inc_step_nt ) then -- its time to get another batch of IDs
            begin
                v_gen_inc_iter_nt = 1;
                -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );
            end
        end

        if ( a_deleting_doc = 1 and a_aux_handling = 0 ) then
            delete from doc_data where current of c_dd_rows_for_doc;

    end -- cursor on doc_data rows for a_doc_id
    close c_dd_rows_for_doc;

    -- add to performance log timestamp about start/finish this unit:
    v_info =
        'qs->qd: doc='||a_doc_id||', op='||a_old_optype
        ||', ret_rows='||i
        ||', cur='||v_ret_cursor
    ;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    v_call = v_this;
    execute procedure sp_add_perf_log(0, v_call,null,v_info);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info, -- 'qs->qd, doc='||a_doc_id||': try ins qd.id='||coalesce(v_id,'<?>')||', v_dd_id='||coalesce(v_dd_id,'<?>'),
            v_call, -- ::: NB ::: do NOT use 'v_this' !! name of last started unit must be actual, see sp_add_to_abend_log!
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_kill_qstorno_ret_qs2qd

create or alter procedure sp_ret_qs2qd_on_canc_wroff(
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_deleting_doc dm_sign,
    a_aux_handling dm_sign default 0
)
as
begin
    -- This proc serves just as trivial 'redirector' when config parameter 'create_with_split_heavy_tabs = 0'.
    -- Otherwise it will be OVERWRITTEN: its code will be replaced with one from sp_kill_qstorno_ret_qs2qd
    -- and replacing of 'QDistr' and 'QStorned' sources with appropriate to current cancel operation.

    execute procedure sp_kill_qstorno_ret_qs2qd( :a_doc_id, :a_old_optype, :a_deleting_doc );
end

^ -- sp_ret_qs2qd_on_canc_wroff

create or alter procedure sp_ret_qs2qd_on_canc_reserve(
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_deleting_doc dm_sign,
    a_aux_handling dm_sign default 0
)
as
begin
    -- This proc serves just as trivial 'redirector' when config parameter 'create_with_split_heavy_tabs = 0'.
    -- Otherwise it will be OVERWRITTEN: its code will be replaced with one from sp_kill_qstorno_ret_qs2qd
    -- and replacing of 'QDistr' and 'QStorned' sources with appropriate to current cancel operation.

    execute procedure sp_kill_qstorno_ret_qs2qd( :a_doc_id, :a_old_optype, :a_deleting_doc );
end

^ -- sp_ret_qs2qd_on_canc_reserve

create or alter procedure sp_ret_qs2qd_on_canc_res_aux(
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_deleting_doc dm_sign,
    a_aux_handling dm_sign default 1
)
as
begin
    -- This proc remains EMPTY when config parameter 'create_with_split_heavy_tabs = 0'.
    -- Otherwise it will be OVERWRITTEN: its code will be replaced with one from sp_kill_qstorno_ret_qs2qd
    -- and replacing of 'QDistr' and 'QStorned' sources with autogen_qd1000 and autogen_qs1000.
    exit;   
end

^ -- sp_ret_qs2qd_on_canc_res_aux

create or alter procedure sp_ret_qs2qd_on_canc_invoice(
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_deleting_doc dm_sign,
    a_aux_handling dm_sign default 0
)
as
begin
    -- This proc serves just as trivial 'redirector' when config parameter 'create_with_split_heavy_tabs = 0'.
    -- Otherwise it will be OVERWRITTEN: its code will be replaced with one from sp_kill_qstorno_ret_qs2qd
    -- and replacing of 'QDistr' and 'QStorned' sources with appropriate to current cancel operation.

    execute procedure sp_kill_qstorno_ret_qs2qd( :a_doc_id, :a_old_optype, :a_deleting_doc );
end

^ -- sp_ret_qs2qd_on_canc_invoice

create or alter procedure sp_ret_qs2qd_on_canc_supp_order(
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_deleting_doc dm_sign,
    a_aux_handling dm_sign default 0
)
as
begin
    -- This proc serves just as trivial 'redirector' when config parameter 'create_with_split_heavy_tabs = 0'.
    -- Otherwise it will be OVERWRITTEN: its code will be replaced with one from sp_kill_qstorno_ret_qs2qd
    -- and replacing of 'QDistr' and 'QStorned' sources with appropriate to current cancel operation.

    execute procedure sp_kill_qstorno_ret_qs2qd( :a_doc_id, :a_old_optype, :a_deleting_doc );
end

^ -- sp_ret_qs2qd_on_canc_supp_order

create or alter procedure sp_qd_handle_on_reserve_upd_sts ( -- old name: s~p_kill_qstorno_mov_qd2qs(
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_new_optype dm_idb
) as
    declare c_gen_inc_step_nt int = 100; -- size of `batch` for get at once new IDs for QDistr (reduce lock-contention of gen page)
    declare v_gen_inc_iter_nt int; -- increments from 1  up to c_gen_inc_step_nt and then restarts again from 1
    declare v_gen_inc_last_nt dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_nt)

    declare v_this dm_dbobj = 'sp_qd_handle_on_reserve_upd_sts';
    declare v_info dm_info;
    declare v_curr_tx bigint;
    declare i int  = 0;
    declare v_dd_id dm_idb;
    declare v_dd_ware_id dm_qty;
    declare v_dd_qty dm_qty;
    declare v_dd_cost dm_qty;
    declare v_doc_pref dm_mcode;

    declare v_old_rcv_optype type of dm_idb;
    declare v_storno_sub smallint;
    declare v_id dm_idb;
    declare v_doc_id dm_idb;
    declare v_doc_optype dm_idb;
    declare v_ware_id dm_idb;
    declare v_snd_optype_id dm_idb;
    declare v_snd_id dm_idb;
    declare v_snd_qty dm_qty;
    declare v_rcv_optype_id dm_idb;
    declare v_rcv_id dm_idb;
    declare v_rcv_qty dm_qty;
    declare v_snd_purchase dm_cost;
    declare v_snd_retail dm_cost;
    declare v_rcv_purchase dm_cost;
    declare v_rcv_retail dm_cost;

    declare c_mov_from_qd2qs cursor for (
        -- rows which will be MOVED from qdistr to qstorned
        select
             qd.id
            ,qd.doc_id
            ,qd.ware_id
            ,qd.snd_optype_id
            ,qd.snd_id
            ,qd.snd_qty
            ,qd.rcv_optype_id
            ,qd.rcv_id
            ,qd.rcv_qty
            ,qd.snd_purchase
            ,qd.snd_retail
            ,qd.rcv_purchase
            ,qd.rcv_retail
        from v_qdistr_name_for_del qd -- this name will be replaced when config parameter create_with_split_heavy_tabs = 1
        where
            qd.ware_id = :v_dd_ware_id
            and qd.snd_optype_id = :a_old_optype
            and qd.rcv_optype_id = :v_old_rcv_optype
            and qd.snd_id = :v_dd_id
    );

begin
    -- Aux SP, called from sp_kill_qty_storno ONLY for sp_reserve_write_off
    -- (change state of customer reserve to 'waybill' when wares are written-off)

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    v_doc_pref = fn_mcode_for_oper(a_new_optype);

    v_gen_inc_iter_nt = 1;
    v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );-- take bulk IDs at once (reduce lock-contention for GEN page)

    -- return from QStorned to QDistr records which were previously moved
    -- (when currently deleting doc was created):
    for
        select d.id, r.rcv_optype_id, r.storno_sub, d.ware_id, d.qty,  d.cost_purchase
        from doc_data d
        cross join rules_for_qdistr r
        where d.doc_id = :a_doc_id and r.snd_optype_id = :a_old_optype
    into v_dd_id, v_old_rcv_optype, v_storno_sub, v_dd_ware_id, v_dd_qty, v_dd_cost
    do
    begin
        if ( coalesce(v_storno_sub,1) = 1 ) then
        begin
            insert into invnt_turnover_log(
                 id -- explicitly assign this field in order NOT to call gen_id in trigger (use v_gen_... counter instead)
                ,ware_id
                ,qty_diff
                ,cost_diff
                ,doc_list_id
                ,doc_pref
                ,doc_data_id
                ,optype_id
            ) values (
                 :v_gen_inc_last_nt - ( :c_gen_inc_step_nt - :v_gen_inc_iter_nt ) -- iter=1: 12345 - (100-1); iter=2: 12345 - (100-2); ...; iter=100: 12345 - (100-100)
                ,:v_dd_ware_id
                ,:v_dd_qty
                ,:v_dd_cost
                ,:a_doc_id
                ,:v_doc_pref
                ,:v_dd_id
                ,:a_new_optype
            );
            v_gen_inc_iter_nt = v_gen_inc_iter_nt + 1;
            if ( v_gen_inc_iter_nt = c_gen_inc_step_nt ) then -- its time to get another batch of IDs
            begin
                v_gen_inc_iter_nt = 1;
                -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );
            end
        end

        open c_mov_from_qd2qs; -- from qstorned qs where qs.rcv_id = :v_dd_id
        while (1=1) do
        begin
            fetch c_mov_from_qd2qs into
                 v_id
                ,v_doc_id
                ,v_ware_id
                ,v_snd_optype_id
                ,v_snd_id
                ,v_snd_qty
                ,v_rcv_optype_id
                ,v_rcv_id
                ,v_rcv_qty
                ,v_snd_purchase
                ,v_snd_retail
                ,v_rcv_purchase
                ,v_rcv_retail;
            if ( row_count = 0 ) then leave;
            i = i + 1;
            -- moved here 16.09.2014: all cursor fields are stored now in variables
            delete from v_qdistr_name_for_del where current of c_mov_from_qd2qs;

            -- for logging in autonom. Tx if PK violation occurs in subsequent sttmt:
            v_info = 'qd->qs, c_mov_from_qd2qs: try ins qStorned.id='||:v_id;

            -- S P _ R E S E R V E _ W R I T E _ O F F
            -- (FINAL point of ware turnover ==> remove data from qdistr in qstorned)
            insert into v_qstorno_name_for_ins ( -- this name will be replaced when config parameter create_with_split_heavy_tabs = 1
                id,
                doc_id,
                ware_id,
                snd_optype_id,
                snd_id,
                snd_qty,
                rcv_optype_id,
                rcv_id,
                rcv_qty,
                snd_purchase,
                snd_retail,
                rcv_purchase,
                rcv_retail
            )
            values(
                :v_id,
                :v_doc_id,
                :v_ware_id,
                :v_snd_optype_id,
                :v_snd_id,
                :v_snd_qty,
                :v_rcv_optype_id,
                :v_rcv_id,
                :v_rcv_qty,
                :v_snd_purchase,
                :v_snd_retail,
                :v_rcv_purchase,
                :v_rcv_retail
            );

        when any do
            begin
                if ( fn_is_uniqueness_trouble(gdscode) ) then
                    -- temply, 09.10.2014 2120: resume investigate trouble with PK violation
                    execute procedure srv_log_dups_qd_qs(
                        :v_this,
                        gdscode,
                        'qstorned',
                        :v_id,
                        :v_info
                    );
                exception; -- ::: nb ::: anonimous but in when-block!
            end

        end -- cursor c_mov_from_qd2qs

        close c_mov_from_qd2qs;
    end -- cursor on doc_data

    -- add to performance log timestamp about start/finish this unit:
    v_info = 'qd->qs, doc='||a_doc_id||', rows='||i;
    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this,null,v_info);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'qs->qd, doc='||a_doc_id||': try ins qs.id='||coalesce(v_id,'<?>')||', v_dd_id='||coalesce(v_dd_id,'<?>'||', old_op='||a_old_optype ),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_qd_handle_on_reserve_upd_sts

create or alter procedure sp_qd_handle_on_cancel_clo (
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_new_optype dm_idb)
as
    declare c_gen_inc_step_nt int = 100; -- size of `batch` for get at once new IDs for QDistr (reduce lock-contention of gen page)
    declare v_gen_inc_iter_nt int; -- increments from 1  up to c_gen_inc_step_nt and then restarts again from 1
    declare v_gen_inc_last_nt dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_nt)
    declare v_this dm_dbobj = 'sp_qd_handle_on_cancel_clo';
    declare v_info dm_info;
    declare v_id dm_idb;
    declare v_snd_id dm_idb;
    declare v_dd_rows int  = 0;
    declare v_dd_qsum int = 0;
    declare v_del_sign dm_sign;
    declare v_qd_rows int = 0;
    declare v_qs_rows int = 0;
    declare v_dd_id dm_idb;
    declare v_dd_ware_id dm_qty;
    declare v_dd_qty dm_qty;
    declare v_dd_cost dm_qty;
    declare v_doc_pref dm_mcode;
    declare v_log_mult dm_sign;
    declare v_log_oper dm_idb;
    declare v_rcv_optype_id type of dm_idb;
    declare v_storno_sub smallint;

    declare c_qd_rows_sub_1 cursor for (
        select q.id, q.snd_id
        from v_qdistr_target_1 q -- this name will be replaced when config parameter create_with_split_heavy_tabs = 1
        where
            q.ware_id = :v_dd_ware_id
            and q.snd_optype_id = :a_old_optype
            and q.rcv_optype_id = :v_rcv_optype_id
            and q.snd_id = :v_dd_id
    );
    declare c_qd_rows_sub_2 cursor for (
        select q.id, q.snd_id
        from v_qdistr_target_2 q -- this name will be replaced when config parameter create_with_split_heavy_tabs = 1
        where
            q.ware_id = :v_dd_ware_id
            and q.snd_optype_id = :a_old_optype
            and q.rcv_optype_id = :v_rcv_optype_id
            and q.snd_id = :v_dd_id
    );

    declare c_qs_rows_sub_1 cursor for (
        select qs.id, qs.snd_id
        from v_qstorned_target_1 qs  -- this name will be replaced when config parameter create_with_split_heavy_tabs = 1
        where qs.snd_id = :v_dd_id
    );
    declare c_qs_rows_sub_2 cursor for (
        select qs.id, qs.snd_id
        from v_qstorned_target_2 qs  -- this name will be replaced when config parameter create_with_split_heavy_tabs = 1
        where qs.snd_id = :v_dd_id
    );

begin

    -- Aux SP, called from sp_kill_qty_storno ONLY for:
    -- 1) sp_cancel_client_order; 2) sp_add_invoice_to_stock (apply and cancel)

    -- add to performance log timestamp about start/finish this unit:
    v_info = iif( a_new_optype = fn_oper_cancel_customer_order(), 'DEL', 'UPD' )
             || ' in qdistr, doc='||a_doc_id
             || ', old_op='||a_old_optype
             || iif( a_new_optype <> fn_oper_cancel_customer_order(), ', new_op='||a_new_optype, '');
    execute procedure sp_add_perf_log(1, v_this, null, v_info);

    v_log_oper = iif( a_new_optype = fn_oper_invoice_get(), fn_oper_invoice_add(), a_new_optype);
    v_log_mult = iif( a_new_optype = fn_oper_invoice_get(), -1, 1);
    v_doc_pref = fn_mcode_for_oper(v_log_oper);
    v_del_sign = 1; -- iif(a_new_optype = fn_oper_cancel_customer_order(), 1, 0);

    v_gen_inc_iter_nt = 1;
    v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );-- take bulk IDs at once (reduce lock-contention for GEN page)

    for
        select r.rcv_optype_id, r.storno_sub
        from rules_for_qdistr r
        where r.snd_optype_id = :a_old_optype
        --and coalesce(r.storno_sub,1) = 1 -- do NOT! old_op=1000 ==> two rows (storno_sub=1 and 2) - needs to be processed for sp_cancel_client_order
        into v_rcv_optype_id, v_storno_sub
    do
    begin
        for
            select d.id, d.ware_id, d.qty, d.cost_purchase
            from doc_data d
            where d.doc_id = :a_doc_id
            into v_dd_id, v_dd_ware_id, v_dd_qty, v_dd_cost
        do
        begin
            v_dd_rows = v_dd_rows + 1;
            v_dd_qsum = v_dd_qsum + v_dd_qty;
            -- 20.09.2014: move here from trigger on doc_list
            -- (reduce scans of doc_data)
            if ( coalesce(v_storno_sub,1) = 1 ) then
            begin
                insert into invnt_turnover_log(
                     id -- explicitly specify this field in order NOT to call gen_id in trigger (use v_gen_... counter instead)
                    ,ware_id
                    ,qty_diff
                    ,cost_diff
                    ,doc_list_id
                    ,doc_pref
                    ,doc_data_id
                    ,optype_id
                ) values (
                     :v_gen_inc_last_nt - ( :c_gen_inc_step_nt - :v_gen_inc_iter_nt ) -- iter=1: 12345 - (100-1); iter=2: 12345 - (100-2); ...; iter=100: 12345 - (100-100)
                    ,:v_dd_ware_id
                    ,:v_log_mult * :v_dd_qty
                    ,:v_log_mult * :v_dd_cost
                    ,:a_doc_id
                    ,:v_doc_pref
                    ,:v_dd_id
                    ,:v_log_oper
                );

                v_gen_inc_iter_nt = v_gen_inc_iter_nt + 1;
                if ( v_gen_inc_iter_nt = c_gen_inc_step_nt ) then -- its time to get another batch of IDs
                begin
                    v_gen_inc_iter_nt = 1;
                    -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                    v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );
                end
            end

            open c_qd_rows_sub_1;
            while (1=1) do
            begin
                fetch c_qd_rows_sub_1 into v_id, v_snd_id;
                if ( row_count = 0 ) then leave;
                v_qd_rows = v_qd_rows + 1;
                delete from v_qdistr_target_1
                where current of c_qd_rows_sub_1;
            end
            close c_qd_rows_sub_1;

            open c_qd_rows_sub_2;
            while (1=1) do
            begin
                fetch c_qd_rows_sub_2 into v_id, v_snd_id;
                if ( row_count = 0 ) then leave;
                v_qd_rows = v_qd_rows + 1;
                delete from v_qdistr_target_2
                where current of c_qd_rows_sub_2;
            end
            close c_qd_rows_sub_2;

            open c_qs_rows_sub_1;
            while (1=1) do
            begin
                fetch c_qs_rows_sub_1 into v_id, v_snd_id;
                if ( row_count = 0 ) then leave;
                v_qs_rows = v_qs_rows+1;
                delete from v_qstorned_target_1
                where current of c_qs_rows_sub_1;
            end
            close c_qs_rows_sub_1;

            open c_qs_rows_sub_2;
            while (1=1) do
            begin
                fetch c_qs_rows_sub_2 into v_id, v_snd_id;
                if ( row_count = 0 ) then leave;
                v_qs_rows = v_qs_rows+1;
                delete from v_qstorned_target_2
                where current of c_qs_rows_sub_2;
            end
            close c_qs_rows_sub_2;

        end
    end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'dd_qsum='||v_dd_qsum||', rows: dd='||v_dd_rows||', qd='||v_qd_rows||', qs='||v_qs_rows );

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_qd_handle_on_cancel_clo

create or alter procedure sp_qd_handle_on_invoice_upd_sts (
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_new_optype dm_idb)
as
    declare c_gen_inc_step_nt int = 100; -- size of `batch` for get at once new IDs for QDistr (reduce lock-contention of gen page)
    declare v_gen_inc_iter_nt int; -- increments from 1  up to c_gen_inc_step_nt and then restarts again from 1
    declare v_gen_inc_last_nt dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_nt)
    declare v_this dm_dbobj = 'sp_qd_handle_on_invoice_upd_sts';
    declare v_info dm_info;

    declare v_qd_id dm_idb;
    declare v_qd_doc_id dm_idb;
    declare v_qd_ware_id dm_idb;

    declare v_qd_snd_id dm_idb;
    declare v_qd_snd_qty dm_qty;
    declare v_qd_rcv_doc_id bigint;
    declare v_qd_rcv_optype_id bigint;
    declare v_qd_rcv_id bigint;
    declare v_qd_rcv_qty numeric(12,3);
    declare v_qd_snd_purchase dm_cost;
    declare v_qd_snd_retail dm_cost;
    declare v_qd_rcv_purchase dm_cost;
    declare v_qd_rcv_retail dm_cost;

    declare v_dd_rows int  = 0;
    declare v_dd_qsum int = 0;

    declare v_qd_rows int = 0;
    declare v_dd_id dm_idb;
    declare v_dd_ware_id dm_qty;
    declare v_dd_qty dm_qty;
    declare v_dd_cost dm_qty;
    declare v_doc_pref dm_mcode;
    declare v_log_mult dm_sign;
    declare v_log_oper dm_idb;
    declare v_rcv_optype_id type of dm_idb;
    declare v_storno_sub smallint;

    declare c_qd_rows_for_doc cursor for (
        select --q.id, q.snd_id
            id,
            doc_id,
            ware_id,
            snd_id,
            snd_qty,
            rcv_doc_id,
            rcv_optype_id,
            rcv_id,
            rcv_qty,
            snd_purchase,
            snd_retail,
            rcv_purchase,
            rcv_retail
        from v_qdistr_name_for_del q -- name of this datasource will be replaced when config 'create_with_split_heavy_tabs=1'
        where
            q.ware_id = :v_dd_ware_id
            and q.snd_optype_id = :a_old_optype
            and q.rcv_optype_id = :v_rcv_optype_id
            and q.snd_id = :v_dd_id
    );

begin

    -- Aux SP, called from sp_kill_qty_storno ONLY for
    -- sp_add_invoice_to_stock (apply and cancel)
    -- #######################

    -- Old name: s~p_kill_qstorno_handle_qd4dd

    -- add to performance log timestamp about start/finish this unit:
    v_info = 'UPD in qdistr, doc='||a_doc_id
             || ', old_op='||a_old_optype
             || ', new_op='||a_new_optype;
    execute procedure sp_add_perf_log(1, v_this, null, v_info);


    v_log_oper = iif( a_new_optype = fn_oper_invoice_get(), fn_oper_invoice_add(), a_new_optype);
    v_log_mult = iif( a_new_optype = fn_oper_invoice_get(), -1, 1);
    v_doc_pref = fn_mcode_for_oper(v_log_oper);

    v_gen_inc_iter_nt = 1;
    v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt ); -- take bulk IDs at once (reduce lock-contention for GEN page)

    for
        select r.rcv_optype_id, r.storno_sub
        from rules_for_qdistr r
        where r.snd_optype_id = :a_old_optype
        into v_rcv_optype_id, v_storno_sub -- 'v_rcv_optype_id' see in WHERE condition in c_qd_rows_for_doc
    do
    begin
        for
            select d.id, d.ware_id, d.qty, d.cost_purchase
            from doc_data d
            where d.doc_id = :a_doc_id
            into v_dd_id, v_dd_ware_id, v_dd_qty, v_dd_cost
        do
        begin
            v_dd_rows = v_dd_rows + 1;
            v_dd_qsum = v_dd_qsum + v_dd_qty;
            -- 20.09.2014: move here from trigger on doc_list
            -- (reduce scans of doc_data)
            if ( coalesce(v_storno_sub,1) = 1 ) then
            begin
                insert into invnt_turnover_log(
                     id -- explicitly specify this field in order NOT to call gen_id in trigger (use v_gen_... counter instead)
                    ,ware_id
                    ,qty_diff
                    ,cost_diff
                    ,doc_list_id
                    ,doc_pref
                    ,doc_data_id
                    ,optype_id
                ) values (
                     :v_gen_inc_last_nt - ( :c_gen_inc_step_nt - :v_gen_inc_iter_nt ) -- iter=1: 12345 - (100-1); iter=2: 12345 - (100-2); ...; iter=100: 12345 - (100-100)
                    ,:v_dd_ware_id
                    ,:v_log_mult * :v_dd_qty
                    ,:v_log_mult * :v_dd_cost
                    ,:a_doc_id
                    ,:v_doc_pref
                    ,:v_dd_id
                    ,:v_log_oper
                );

                v_gen_inc_iter_nt = v_gen_inc_iter_nt + 1;
                if ( v_gen_inc_iter_nt = c_gen_inc_step_nt ) then -- its time to get another batch of IDs
                begin
                    v_gen_inc_iter_nt = 1;
                    -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                    v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );
                end
            end

            open c_qd_rows_for_doc;
            while (1=1) do
            begin
                fetch c_qd_rows_for_doc
                into -- v_qd_id, v_qd_snd_id;
                     v_qd_id
                    ,v_qd_doc_id
                    ,v_qd_ware_id
                    ,v_qd_snd_id
                    ,v_qd_snd_qty
                    ,v_qd_rcv_doc_id
                    ,v_qd_rcv_optype_id
                    ,v_qd_rcv_id
                    ,v_qd_rcv_qty
                    ,v_qd_snd_purchase
                    ,v_qd_snd_retail
                    ,v_qd_rcv_purchase
                    ,v_qd_rcv_retail;
                if ( row_count = 0 ) then leave;
                v_qd_rows = v_qd_rows+1;

                -- sp_add_invoice_to_stock: apply and cancel
                -- #########################################

                -- 31.08.2015: replaced 'update qdistr' to del_ins algorithm,
                -- so this can be applied later for replacing to 'autogen_qdNNNN'.

                delete from v_qdistr_name_for_del where current of c_qd_rows_for_doc; -- name will be replaced when config 'create_with_split_heavy_tabs=1'

                insert into v_qdistr_name_for_ins( -- name will be replaced when config 'create_with_split_heavy_tabs=1'
                    id,
                    doc_id,
                    ware_id,
                    snd_optype_id,
                    snd_id,
                    snd_qty,
                    rcv_doc_id,
                    rcv_optype_id,
                    rcv_id,
                    rcv_qty,
                    snd_purchase,
                    snd_retail,
                    rcv_purchase,
                    rcv_retail
                ) values(
                     :v_qd_id
                    ,:v_qd_doc_id
                    ,:v_qd_ware_id
                    ,:a_new_optype ----------- !!
                    ,:v_qd_snd_id
                    ,:v_qd_snd_qty
                    ,:v_qd_rcv_doc_id
                    ,:v_qd_rcv_optype_id
                    ,:v_qd_rcv_id
                    ,:v_qd_rcv_qty
                    ,:v_qd_snd_purchase
                    ,:v_qd_snd_retail
                    ,:v_qd_rcv_purchase
                    ,:v_qd_rcv_retail
                );

            end
            close c_qd_rows_for_doc;
        end
    end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'dd_qsum='||v_dd_qsum||', rows: dd='||v_dd_rows||', qd='||v_qd_rows );

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^  -- sp_qd_handle_on_invoice_upd_sts

-- Aux SP that serves just as "redirector" to sp_qd_handle_on_invoice_upd_sts when config 'create_with_split_heavy_tabs=0'
-- Source of this SP will be replaced to reflect actual value of autogen_qdNNNN when config 'create_with_split_heavy_tabs=1'
create or alter procedure sp_qd_handle_on_invoice_adding (
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_new_optype dm_idb
) as
begin
    execute procedure sp_qd_handle_on_invoice_upd_sts( :a_doc_id, :a_old_optype, :a_new_optype );
end

^ -- sp_qd_handle_on_invoice_adding (REDIRECTOR)

-- Aux SP that serves just as "redirector" to sp_qd_handle_on_invoice_upd_sts when config 'create_with_split_heavy_tabs=0'
-- Source of this SP will be replaced to reflect actual value of autogen_qdNNNN when config 'create_with_split_heavy_tabs=1'
create or alter procedure sp_qd_handle_on_invoice_reopen (
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_new_optype dm_idb
) as
begin
    execute procedure sp_qd_handle_on_invoice_upd_sts( :a_doc_id, :a_old_optype, :a_new_optype );
end

^ -- sp_qd_handle_on_invoice_reopen (REDIRECTOR)

create or alter procedure sp_kill_qty_storno (
    a_doc_id dm_idb,
    a_old_optype dm_idb,
    a_new_optype dm_idb,
    a_updating dm_sign,
    a_deleting dm_sign)
as
    declare v_this dm_dbobj = 'sp_kill_qty_storno';
    declare v_info dm_info;
    declare v_dbkey dm_dbkey;
begin
    -- Called from doc_list_biud for deleting doc or updating with changing it state.
    -- 1. For sp_reserve_write_off (FINAL point of ware turnover):
    --    remove data from qdistr into qstorned)
    -- 2. For CANCEL operations (deleting doc or change its state to "previous"):
    --    return storned rows from qstorned to qdistr
    -- 3. For sp_add_invoice_to_stock: change IDs in qdistr.snd_op XOR rcv_op to
    --    new ID (2000-->2100)

    if ( NOT (a_deleting = 1 or a_updating = 1 and a_new_optype is distinct from a_old_optype) )
    then
      --#####
        exit;
      --#####

    v_info = 'dh='|| a_doc_id
             ||' '||iif(a_updating=1,'UPD','DEL')
             ||iif(a_updating=1, ' old_op='||a_old_optype||' new_op='||a_new_optype, ' op='||a_old_optype);

    execute procedure sp_add_perf_log(1, v_this,null, v_info);

    if ( a_updating = 1 and a_new_optype is distinct from a_old_optype ) then
    begin
        ----------------   c h a n g e    o p t y p e _ i d   ------------------
        -- see s`p_cancel_client_order; sp_add_invoice_to_stock;
        -- sp_reserve_write_off, s`p_cancel_write_off
        -- ==> they all change doc_data.optype_id
        if ( a_new_optype = fn_oper_cancel_customer_order() ) then
            begin
                -- S P _ C A N C E L _ C L I E N T _ O R D E R
                -- Kill all records for this doc both in QDistr & QStorned
                -- delete rows in qdistr for currently cancelled client order:
                execute procedure sp_qd_handle_on_cancel_clo( :a_doc_id, :a_old_optype, fn_oper_cancel_customer_order() );
            end

        else if ( a_old_optype = fn_oper_retail_realization() and a_new_optype = fn_oper_retail_reserve() ) then
            -- S P _ C A N C E L _ W R I T E _ O F F
            -- return from QStorned to QDistr records which were previously moved
            -- (when currently deleting doc was created):
            execute procedure sp_ret_qs2qd_on_canc_wroff( :a_doc_id, :a_old_optype, :a_deleting );
            -- prev: direct call execute procedure s~p_kill_qstorno_ret_qs2qd( :a_doc_id, :a_old_optype, :a_deleting );

        else if ( a_old_optype = fn_oper_retail_reserve() and a_new_optype = fn_oper_retail_realization() ) then
            -- S P _ R E S E R V E _ W R I T E _ O F F
            execute procedure sp_qd_handle_on_reserve_upd_sts( :a_doc_id, :a_old_optype, :a_new_optype );
            -- prev: direct call of s~p_kill_qstorno_mov_qd2qs( :a_doc_id, :a_old_optype, :a_new_optype);

        else -- all other updates of doc state, except s`p_cancel_write_off
            -- update rows in qdistr for currently selected doc (3dr arg <> fn_oper_cancel_cust_order):    
            if ( a_old_optype = fn_oper_invoice_get() ) then
                -- S P _ A D D _ I N V O I C E _ T O _ S T O C K
                execute procedure sp_qd_handle_on_invoice_adding( :a_doc_id, :a_old_optype, :a_new_optype );
            else if ( a_old_optype = fn_oper_invoice_add() ) then
                -- S P _ C A N C E L _ A D D I N G _ I N V O I C E
                execute procedure sp_qd_handle_on_invoice_reopen( :a_doc_id, :a_old_optype, :a_new_optype );
            else
                exception ex_bad_argument using(':a_old_optype='||:a_old_optype, v_this );
            -- before: execute procedure s~p_kill_qstorno_handle_qd4dd( :a_doc_id, :a_old_optype, :a_new_optype );

    end -- a_updating = 1 and a_new_optype is distinct from a_old_optype

    if ( a_deleting = 1 ) then -- all other operations that delete document
    begin
        -- return from QStorned to QDistr records which were previously moved
        -- (when currently deleting doc was created):
        if ( :a_old_optype = fn_oper_invoice_get() ) then
            execute procedure sp_ret_qs2qd_on_canc_invoice( :a_doc_id, :a_old_optype, :a_deleting );
        else if ( :a_old_optype = fn_oper_order_for_supplier() ) then
            execute procedure sp_ret_qs2qd_on_canc_supp_order( :a_doc_id, :a_old_optype, :a_deleting );
        else if ( :a_old_optype = fn_oper_retail_reserve() ) then
            begin
                execute procedure sp_ret_qs2qd_on_canc_res_aux( :a_doc_id, :a_old_optype, :a_deleting );
                execute procedure sp_ret_qs2qd_on_canc_reserve( :a_doc_id, :a_old_optype, :a_deleting );
            end
        else
            exception ex_bad_argument using(':a_old_optype='||:a_old_optype, v_this );
        -- prev: direct call of s~p_kill_qstorno_ret_qs2qd( :a_doc_id, :a_old_optype, :a_deleting );
    end -- a_deleting = 1

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- iif( fn_is_uniqueness_trouble(gdscode), 1, 0) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_kill_qty_storno


set term ;^
-- View for using in SRV_FIND_QD_QS_MISM as alias of table 'QDistr'.
-- Will be altered in 'oltp_autogen_ddl.sql' when config 'create_with_split_heavy_tabs=1'.
create or alter view v_qdistr_source as
select *
from qdistr
;

-- View for using in SRV_FIND_QD_QS_MISM as alias of table 'QStorned'.
-- Will be altered in 'oltp_autogen_ddl.sql' when config 'create_with_split_heavy_tabs=1'.
create or alter view v_qstorned_source as
select *
from qstorned
;


set term ^;

create or alter procedure srv_find_qd_qs_mism(
    a_doc_list_id type of dm_idb,
    a_optype_id type of dm_idb,
    a_after_deleting_doc dm_sign -- 1==> doc has been deleted, must check orphans only in qdistr+qstorned
) as
    declare v_this dm_dbobj = 'srv_find_qd_qs_mism';
    declare v_gdscode int;
    declare v_dd_mismatch_id bigint;
    declare v_dd_mismatch_qty dm_qty;
    declare v_qd_qs_orphan_src dm_dbobj; -- name of table where orphan row(s) found
    declare v_qd_qs_orphan_doc dm_idb;
    declare v_qd_qs_orphan_id dm_idb;
    declare v_qd_qs_orphan_sid dm_idb;
    declare v_qd_sum dm_qty;
    declare v_qs_sum dm_qty;
    declare v_info dm_info = '';
    declare c_chk_violation_code int = 335544558; -- check_constraint
    declare v_dh_id dm_idb;
    declare v_dd_id dm_idb;
    declare v_ware_id dm_idb;
    declare v_dd_qty dm_qty;
    declare v_all_qty_sum dm_qty;
    declare v_all_qd_sum dm_qty;
    declare v_all_qs_sum dm_qty;
    declare v_snd_optype_id dm_idb;
    declare v_rcv_optype_id dm_idb;
    declare v_rows_handled int;

    declare c_qd_qs_orphans cursor for ( -- used after deletion of doc: search for orphans in qd & qs
        -- deciced neither add index on qdistr.doc_id nor modify qdistr PK and set its key = {id, doc_id} (performance)
        select r.doc_data_id, r.ware_id
        from tmp$result_set r -- ::: NB ::: this table must be always filled in SPs which REMOVES (cancel) doc with wares
        where r.doc_id = :a_doc_list_id
    );

    declare c_dd_qty_match_qd_qs_counts cursor for (
        select f.dd_id, f.ware_id, f.dd_qty, f.qd_cnt, f.qs_cnt
        from (
            select e.dd_id, e.ware_id, e.qty as dd_qty, e.qd_cnt, coalesce(sum(qs.snd_qty),0) as qs_cnt
            -- PLAN SORT (JOIN (JOIN (SORT (E D D INDEX (FK_DOC_DATA_DOC_LIST)), E QD INDEX (QDISTR_SNDOP_RCVOP_SNDID_DESC)), QS INDEX (QSTORNED_SND_ID)))
            from (
                select
                    d.id as dd_id
                    ,d.ware_id
                    ,d.qty
                    ,coalesce(sum(qd.snd_qty),0) qd_cnt
                    --,iif(:a_optype_id=3400, 3300, :a_optype_id) as snd_optype_id
                    --,:v_snd_optype_id as snd_optype_id -- core-4927, affected 2.5 only
                from doc_data d
                left join v_qdistr_source qd on
                    -- {ware,snd_optype_id,rcv_optype_id} ==> Index Range Scan (full match, since )
                    -- Full match since 01.09.2015 2355, see:
                    -- http://sourceforge.net/p/firebird/code/62176 (3.0)
                    -- http://sourceforge.net/p/firebird/code/62177 (2.5.5)
                    qd.ware_id = d.ware_id
                    -- 07.09.2015. In 2.5, before core-4927 (http://sourceforge.net/p/firebird/code/62200):
                    -- we had to avoid usage of "iif()" for evaluating column that will be involved in
                    -- JOIN with "unioned" view: it (IIF function) prevented the condition from being
                    -- pushed into the union for better optimization. This lead to unneccessary index
                    -- scans of tables from unioned-view that sor sure did not contain req. data.
                    -- Thus we use here parameter ":v_snd_optype_id" which will be evaluated BEFORE
                    -- in separate IIF statement:
                    and qd.snd_optype_id = :v_snd_optype_id
                    and qd.rcv_optype_id = :v_rcv_optype_id
                    and qd.snd_id = d.id
                where
                    d.doc_id  = :a_doc_list_id
                group by d.id, d.ware_id, d.qty
            ) e
            left join v_qstorned_source qs on
                -- NB: we can skip this join if previous one produces ERROR in results:
                -- sum of amounts in doc_data rows has to be NOT LESS than sum(snd_qty) on qdistr
                -- (except CANCELLED client order for which qdistr must NOT contain any row for this doc)
                (
                    :v_snd_optype_id <> 1100 and e.qty >= e.qd_cnt
                    or
                    :v_snd_optype_id = 1100 and e.qd_cnt = 0
                )
                and qs.snd_id = e.dd_id
            group by e.dd_id, e.ware_id, e.qty, e.qd_cnt
        ) f
    );

begin

    -- Search for mismatches b`etween doc_data.qty and number of records in
    -- v_qdistr + v_qstorned, for all operation. Algorithm for deleted ("cancelled")
    -- document differs from document that was just created or updated its state:
    -- we need found orphan rows in v_qdistr + v_qstorned when document has been removed
    -- (vs. checking sums of doc_data.qty and sum(qty) when doc. is created/updated)
    -- Log mismatches if they found and raise exc`eption.
    -- ### NB ### GTT tmp$result_set need to be fulfilled with data of doc that
    -- is to be deleted, BEFORE this deletion issues. It's bad (strong link between
    -- modules) but this is the only way to avoid excessive indexing of v_qdistr & v_qstorned.

    v_info = 'dh='||a_doc_list_id||', op='||a_optype_id;
    execute procedure sp_add_perf_log(1, v_this); -- , null, v_info);

    v_dd_mismatch_id = null;
    -- This value is used in CURSOR c_dd_qty_match_qd_qs_counts as argument of join:
    v_snd_optype_id = iif(:a_optype_id=3400, 3300, :a_optype_id);

    select r.rcv_optype_id
    from rules_for_qdistr r
    where r.snd_optype_id = :a_optype_id and coalesce(r.storno_sub,1)=1
    into v_rcv_optype_id;

    if ( a_after_deleting_doc = 1 ) then -- call after doc has been deleted: NO rows in doc_data but we must check orphan rows in qd&qs for this doc!
        begin
            select h.id from doc_list h where h.id = :a_doc_list_id into v_dh_id;
            if ( v_dh_id is NOT null or not exists(select * from tmp$result_set)  ) then
                exception ex_bad_argument using('a_after_deleting_doc', v_this) ; -- 'argument @1 passed to unit @2 is invalid';

            v_rows_handled = 0;
            open c_qd_qs_orphans;
            while ( v_dh_id is null ) do
            begin
                fetch c_qd_qs_orphans into v_dd_id, v_ware_id; -- get data of doc which have been saved in tmp$result_set
                if ( row_count = 0 ) then leave;
                v_rows_handled = v_rows_handled + 1;

                select first 1 'v_qdistr' as src, qd.doc_id, qd.snd_id, qd.id
                from v_qdistr_source qd
                where
                    -- {ware,snd_optype_id,rcv_optype_id} ==> Index Range Scan (full match, since )
                    -- Full match since 01.09.2015 2355, see:
                    -- http://sourceforge.net/p/firebird/code/62176 (3.0)
                    -- http://sourceforge.net/p/firebird/code/62177 (2.5.5)
                    qd.ware_id = :v_ware_id
                    and qd.snd_optype_id = :v_snd_optype_id
                    and qd.rcv_optype_id = :v_rcv_optype_id
                    -- This is mandatory otherwise get lot of different docs for the same {ware,snd_optype_id,rcv_optype_id}:
                    and qd.snd_id = :v_dd_id
                into v_qd_qs_orphan_src, v_qd_qs_orphan_doc, v_qd_qs_orphan_sid, v_qd_qs_orphan_id;

                if ( v_qd_qs_orphan_id is null ) then -- run 2nd check only if there are NO row in QDistr
                    select first 1 'v_qstorned' as src, qs.doc_id, qs.snd_id, qs.id
                    from v_qstorned_source qs
                    where qs.snd_id = :v_dd_id
                    into v_qd_qs_orphan_src, v_qd_qs_orphan_doc, v_qd_qs_orphan_sid, v_qd_qs_orphan_id;

                if ( v_qd_qs_orphan_id is NOT null ) then
                begin
                    v_info = trim(v_info)
                        || ': orphan '||v_qd_qs_orphan_src||'.id='||v_qd_qs_orphan_id
                        || ' for deleted doc='||v_qd_qs_orphan_doc
                        || ', snd_id='||v_qd_qs_orphan_sid
                        || ', ware='||v_ware_id;
                    leave;
                end
            end
            close c_qd_qs_orphans;
            if ( v_qd_qs_orphan_id is null ) then
                v_info = trim(v_info)||': no data in qd+qs for deleted '||v_rows_handled||' rows';

        end
    else ------------------  _N O T_   a f t e r    d e l e t i n g  -------
        begin
            v_all_qty_sum = 0;
            v_all_qd_sum = 0;
            v_all_qs_sum = 0;
        
            v_rows_handled = 0;
            open c_dd_qty_match_qd_qs_counts;
            while ( 1 = 1 ) do
            begin
                fetch c_dd_qty_match_qd_qs_counts
                into v_dd_id, v_ware_id, v_dd_qty, v_qd_sum, v_qs_sum;
                -- e.dd_id, e.qty, e.qd_cnt, coalesce(sum(qs.snd_qty),0) as qs_cnt
                if (row_count = 0) then leave;

                v_rows_handled = v_rows_handled + v_qd_sum + v_qs_sum;
                v_all_qty_sum = v_all_qty_sum + v_dd_qty; -- total AMOUNT in ALL rows of document
                v_all_qd_sum = v_all_qd_sum + v_qd_sum; -- number of rows in QDistr for ALL wares of document
                v_all_qs_sum = v_all_qs_sum + v_qs_sum; -- number of rows in v_qstorned for ALL wares of document

                if ( a_optype_id <> 1100 and v_dd_qty <> v_qd_sum + v_qs_sum -- error, immediately stop check if so!
                     or
                     a_optype_id = 1100 and v_qd_sum + v_qs_sum > 0 -- it's WRONG when we cancel client order and some rows still exist in qdistr or v_qstorned for this doc!
                   ) then
                begin
                    v_dd_mismatch_id = v_dd_id;
                    v_dd_mismatch_qty = v_dd_qty;
                    leave;
                end
            end
            close c_dd_qty_match_qd_qs_counts;

            if ( v_dd_mismatch_id is NOT null ) then
                begin
                    v_info=trim(v_info)
                           || ': dd='||v_dd_mismatch_id
                           || ', ware='||v_ware_id
                           || ', sum_qty='||cast(v_all_qty_sum as int)
                           || ', problem_qty='||cast(v_dd_mismatch_qty as int);
                    if ( a_optype_id <> 1100 ) then
                        v_info = v_info
                           || ' - mism: qd_cnt='||cast(v_qd_sum as int)
                           || iif( v_qs_sum >=0, ', qs_cnt='||cast(v_qs_sum as int), ', qs_cnt=n/a');
                    else
                        v_info = v_info
                           || ' - no_removal: qd_cnt='||cast(v_qd_sum as int)
                           || iif( v_qs_sum >=0, ', qs_cnt='||cast(v_qs_sum as int), ', qs_cnt=n/a');
                end
            else -- ok
                v_info = trim(v_info)
                    ||', sum_qty='||cast(v_all_qty_sum as int)
                    ||', cnt_qds='||cast((v_all_qd_sum + v_all_qs_sum) as int)
                    ||', rows_handled='||v_rows_handled;
        
        end -- block for NOT after deleting doc

    if ( a_after_deleting_doc = 0 and v_dd_mismatch_id is NOT null
         or
         a_after_deleting_doc = 1 and v_qd_qs_orphan_id is NOT null
       ) then
    begin
        -- dump dirty data:
        execute procedure zdump4dbg(:a_doc_list_id); --,:a_doc_data_id);
        --#######
        if ( a_after_deleting_doc = 1) then
            exception ex_orphans_qd_qs_found using( a_doc_list_id, v_qd_qs_orphan_sid, v_qd_qs_orphan_src, v_qd_qs_orphan_id );
            -- 'at least one row found for DELETED doc id=@1, snd_id=@2: @3.id=@4';
        else
            exception ex_mism_doc_data_qd_qs using( v_dd_mismatch_id, v_dd_mismatch_qty, v_qd_sum, v_qs_sum );
            -- at least one mismatch btw doc_data.id=@1 and qdistr+v_qstorned: qty=@2, qd_cnt=@3, qs_cnt=@4
            --#######
    end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'ok, '||v_info);

when any do
    begin
        v_gdscode = iif( :v_dd_mismatch_id is null, gdscode, :c_chk_violation_code);
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            :v_gdscode,
            'MISMATCH, '||v_info,
            v_this,
            fn_halt_sign(:v_gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --########
        exception;  -- ::: nb ::: anonimous but in when-block!
        --########
    end
end

^ -- srv_find_qd_qs_mism

create or alter procedure sp_add_money_log(
    a_doc_id dm_idb,
    a_old_mult dm_sign,
    a_old_agent_id dm_idb,
    a_old_op dm_idb,
    a_old_purchase type of dm_cost, -- NB: must allow negative values!
    a_old_retail type of dm_cost,     -- NB: must allow negative values!
    a_new_mult dm_sign,
    a_new_agent_id dm_idb,
    a_new_op dm_idb,
    a_new_purchase type of dm_cost, -- NB: must allow negative values!
    a_new_retail type of dm_cost      -- NB: must allow negative values!
)
as
begin
    -- called from trg d`oc_list_aiud for every operation which affects to contragent saldo
    if ( a_old_mult <> 0 ) then
        insert into money_turnover_log ( -- "log" of all changes in doc_list.cost_xxx
            doc_id,
            agent_id,
            optype_id,
            cost_purchase, -- NB: this field must allow negative values!
            cost_retail   -- NB: this field must allow negative values!
        )
        values(
            :a_doc_id, -- ref to doc_list
            :a_old_agent_id,
            :a_old_op,
            -:a_old_purchase,
            -:a_old_retail
        );

    if ( a_new_mult <> 0  ) then
        insert into money_turnover_log ( -- "log" of all changes in doc_list.cost_xxx
            doc_id,
            agent_id,
            optype_id,
            cost_purchase, -- NB: this field must allow negative values!
            cost_retail   -- NB: this field must allow negative values!
        )
        values(
            :a_doc_id, -- ref to doc_list
            :a_new_agent_id,
            :a_new_op,
            :a_new_purchase,
            :a_new_retail
        );
end

^ -- sp_add_money_log

------------------------------------------------------------------------------

create or alter procedure sp_lock_dependent_docs(
    a_base_doc_id dm_idb,
    a_base_doc_oper_id dm_idb default null -- = (for invoices which are to be 'reopened' - old_oper_id)
)
as
    declare v_rcv_optype_id dm_idb;
    declare v_dependend_doc_id dm_idb;
    declare v_dependend_doc_state dm_idb;
    declare v_catch_bitset bigint;
    declare v_dbkey dm_dbkey;
    declare v_info dm_info;
    declare v_list dm_info;
    declare v_this dm_dbobj = 'sp_lock_dependent_docs';
begin

    v_info = 'base_doc='||a_base_doc_id;
    execute procedure sp_add_perf_log(1, v_this, null, v_info);

    if ( a_base_doc_oper_id is null ) then
        select h.optype_id
        from doc_list h
        where h.id = :a_base_doc_id
        into a_base_doc_oper_id;

    v_rcv_optype_id = decode(a_base_doc_oper_id,
                             fn_oper_invoice_add(),  fn_oper_retail_reserve(),
                             fn_oper_order_for_supplier(), fn_oper_invoice_get(),
                             null
                            );
    delete from tmp$dep_docs d where d.base_doc_id = :a_base_doc_id;
    for
        select x.dependend_doc_id, h.state_id, h.rdb$db_key
        -- 30.12.2014: PLAN JOIN (SORT (X Q INDEX (QSTORNED_DOC_ID)), H INDEX (PK_DOC_LIST))
        -- (added field rcv_doc_id in table qstorned, now can remove join with doc_data!)
        from (
            -- Checked plan 13.07.2014:
            -- PLAN (Q ORDER QSTORNED_RCV_ID INDEX (QSTORNED_DOC_ID))
            select q.rcv_doc_id dependend_doc_id
            from v_qstorned_source q
            where
                q.doc_id = :a_base_doc_id -- choosen invoice which is to be re-opened
                and q.snd_optype_id = :a_base_doc_oper_id
                and q.rcv_optype_id = :v_rcv_optype_id
            group by 1
        ) x
        join doc_list h on x.dependend_doc_id = h.id
        into v_dependend_doc_id, v_dependend_doc_state, v_dbkey
    do begin
        -- immediatelly try to lock record in order to avoid wast handling
        -- of 99 docs and get fault on 100th one!
        -- (see s`p_cancel_adding_invoice, s`p_cancel_supplier_order)
        v_info = 'try lock dependent doc_id='||v_dependend_doc_id;
        select h.rdb$db_key
        from doc_list h
        where h.rdb$db_key = :v_dbkey -- lock_conflict can occur here!
        for update with lock
        into v_dbkey;

        begin
            -- NB:  BASE_DOC_ID,DEPENDEND_DOC_ID  ==> UNIQ index in tmp$dep_docs
            insert into tmp$dep_docs( base_doc_id, dependend_doc_id, dependend_doc_state)
            values( :a_base_doc_id, :v_dependend_doc_id, :v_dependend_doc_state );
        when any do
            -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
            -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
            -- catched it's kind of exception!
            -- 1) tracker.firebirdsql.org/browse/CORE-3275
            --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
            -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
            begin
              -- supress dup ex`ception - it is normal in this case!
              if ( NOT fn_is_uniqueness_trouble(gdscode) ) then exception;
            end
        end
    end

    v_catch_bitset = cast(rdb$get_context('USER_SESSION','QMISM_VERIFY_BITSET') as bigint);
    -- See oltp_main_filling.sql for definition of bitset var `QMISM_VERIFY_BITSET`:
    -- bit#0 := 1 ==> perform calls of srv_catch_qd_qs_mism in d`oc_list_aiud => sp_add_invnt_log
    --                in order to register mismatches b`etween doc_data.qty and total number of rows
    --                in qdistr + qstorned for doc_data.id
    -- bit#1 := 1 ==> perform calls of SRV_CATCH_NEG_REMAINDERS from INVNT_TURNOVER_LOG_AI
    --                (instead of totalling turnovers to `invnt_saldo` table)
    -- bit#2 := 1 ==> allow dump dirty data into z-tables for analysis, see sp zdump4dbg, in case
    --                when some 'bad exception' occurs (see ctx var `HALT_TEST_ON_ERRORS`)
    if ( bin_and( v_catch_bitset, 7 ) > 0 ) then -- ==> any of bits #0..2 is ON
       begin
           v_list=left( ( select list(d.dependend_doc_id) from tmp$dep_docs d where d.base_doc_id=:a_base_doc_id ), 255-char_length(v_info)-15);
           v_info = 'depDocsLst='|| coalesce(nullif(v_list,''),'<empty>');
       end
    else
       begin
           v_info = 'depDocsCnt='||( select count(*) from tmp$dep_docs d where d.base_doc_id=:a_base_doc_id );
       end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, v_info );

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_lock_dependent_docs

-- 29.07.2014: STUB, need for debug view z_invoices_to_be_adopted, will be redefined in oltp30_sp.sql:
create or alter procedure sp_get_clo_for_invoice( a_selected_doc_id dm_idb )
returns (
    clo_doc_id type of dm_idb,
    clo_agent_id type of dm_idb
)
as begin
  suspend;
end
^
set term ;^


------------------------------------------------------------------------------

set term ^;

-- several debug proc for catch cases when negative remainders encountered
create or alter procedure srv_check_neg_remainders( -- 28.09.2014, instead s`rv_catch_neg
    a_doc_list_id dm_idb,
    a_optype_id dm_idb
) as
declare v_id bigint;
    declare v_catch_bitset bigint;
    declare v_curr_tx bigint;
    declare c_chk_violation_code int = 335544558; -- check_constraint
    declare v_msg dm_info = '';
    declare v_info dm_info = '';
    declare v_this dm_dbobj = 'srv_check_neg_remainders';
begin
    -- called from d`oc_list_aiud
    -- #########################
    -- See oltp_main_filling.sql for definition of bitset var `QMISM_VERIFY_BITSET`:
    -- bit#0 := 1 ==> perform calls of srv_catch_qd_qs_mism in d`oc_list_aiud => sp_add_invnt_log
    --                in order to register mismatches b`etween doc_data.qty and total number of rows
    --                in qdistr + qstorned for doc_data.id
    -- bit#1 := 1 ==> perform calls of SRV_CATCH_NEG_REMAINDERS from INVNT_TURNOVER_LOG_AI
    --                (instead of totalling turnovers to `invnt_saldo` table)
    -- bit#2 := 1 ==> allow dump dirty data into z-tables for analysis, see sp zdump4dbg, in case
    --                when some 'bad exception' occurs (see ctx var `HALT_TEST_ON_ERRORS`)
    v_catch_bitset = cast(rdb$get_context('USER_SESSION','QMISM_VERIFY_BITSET') as bigint);
    if ( bin_and( v_catch_bitset, 2 ) = 0 ) then -- job of this SP meaningless because of totalling
        --####
          exit;
        --####

    -- do NOT add calls of sp_check_to_stop_work or sp_check_nowait_or_timeout:
    -- this SP is called very often!

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    v_msg ='dh='||:a_doc_list_id || ', op='||fn_mcode_for_oper( :a_optype_id );

    v_id = null;
    select first 1
        n.ware_id
        ,:v_msg
        || ', w='||n.ware_id||', dd='||n.dd_id
        || ', neg:'
        || trim( trailing from iif( n.qty_ord<0,' q_ord='||n.qty_ord,'' ) )
        || trim( trailing from iif( n.qty_sup<0,' q_sup='||n.qty_sup,'' ) )
        || trim( trailing from iif( n.qty_avl<0,' q_avl='||n.qty_avl,'' ) )
        || trim( trailing from iif( n.qty_res<0,' q_res='||n.qty_res,'' ) )
        || trim( trailing from iif( n.qty_inc<0,' q_inc='||n.qty_inc,'' ) )
        || trim( trailing from iif( n.qty_out<0,' q_out='||n.qty_out,'' ) )
        || trim( trailing from iif( n.qty_clo<0,' q_clo='||n.qty_clo,'' ) )
        || trim( trailing from iif( n.qty_clr<0,' q_clr='||n.qty_clr,'' ) )
        || trim( trailing from iif( n.qty_acn<0,' q_acn='||n.qty_acn,'' ) )
        || trim( trailing from iif( n.cost_acn<0,' $_acn='||n.cost_acn,'' ) )
    from --v_saldo_invnt n
    (
        select
            ng.ware_id
            ,min(ng.doc_data_id) as dd_id -- no matter min or max
            ,sum(o.m_qty_clo * ng.qty_diff) qty_clo
            ,sum(o.m_qty_clr * ng.qty_diff) qty_clr
            ,sum(o.m_qty_ord * ng.qty_diff) qty_ord
            ,sum(o.m_qty_sup * ng.qty_diff) qty_sup
            ,sum(o.m_qty_avl * ng.qty_diff) qty_avl
            ,sum(o.m_qty_res * ng.qty_diff) qty_res
            ,sum(o.m_cost_inc * ng.qty_diff) qty_inc
            ,sum(o.m_cost_out * ng.qty_diff) qty_out
            ,sum(o.m_cost_inc * ng.cost_diff) cost_inc
            ,sum(o.m_cost_out * ng.cost_diff) cost_out
            -- amount "on hand" as it seen by accounter:
            ,sum(o.m_qty_avl * ng.qty_diff) + sum(o.m_qty_res * ng.qty_diff) qty_acn
            -- total cost "on hand" in purchasing prices:
            ,sum(o.m_cost_inc * ng.cost_diff) - sum(o.m_cost_out * ng.cost_diff) cost_acn
        from invnt_turnover_log ng
        join optypes o on ng.optype_id=o.id
        join doc_data d on ng.ware_id = d.ware_id -- ng.doc_data_id = d.id
        where d.doc_id = :a_doc_list_id
        --where ng.ware_id = :a_ware_id
        group by
            ng.ware_id
            --,ng.doc_data_id
    ) n
    where
           n.qty_ord<0 or n.qty_sup<0 or n.qty_avl<0 or n.qty_res<0
        or n.qty_inc<0 or n.qty_out<0 or n.qty_clo<0 or n.qty_clr<0
        or n.qty_acn<0 or n.cost_acn<0
    into v_id, v_info;
    
    if ( v_id is not null) then
    begin
        v_curr_tx = current_transaction;
        in autonomous transaction do
        begin -- 26.09.2014 2222, temply
            insert into perf_log(
                unit,
                exc_unit,
                fb_gdscode,
                trn_id,
                info,
                exc_info,
                stack,
                dump_trn
            ) values (
                :v_this,
                '!',
               :c_chk_violation_code,
               :v_curr_tx,
               :v_info,
               :v_info, --'ex_neg_remainders_encountered',
               :v_this, -- fn_get_stack(),
               current_transaction
            );
            execute procedure sp_halt_on_error('5', :c_chk_violation_code, :v_curr_tx); -- temply
        end
        -- 335544558 check_constraint    Operation violates CHECK constraint @1 on view or table @2.
        execute procedure sp_add_to_abend_log(
          'ex_neg_remainders_encountered',
          :c_chk_violation_code,
          v_info,
          v_this,
          1 -- ::: nb ::: force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        -- 4 debug: dump dirty data:
        execute procedure zdump4dbg; -- (null, a_doc_data_id, v_id);

        --########
        exception ex_neg_remainders_encountered using( v_id, v_info ); -- 'at least one remainder < 0, ware_id=@1';
        --########
    end
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(0, v_this,null, 'ok, '||v_msg);

when any do
    begin
        --########
        exception;  -- ::: nb ::: anonimous but in when-block!
        --########
    end
end

^ -- srv_check_neg_remainders

set term ;^


-------------------------------------------------------------------------------
-- ############################   V I E W S   #################################
-------------------------------------------------------------------------------

-- 07.09.2015: probe to replace ES in all cases of fn_get_rand_id:
create or alter view name$to$substutite$min$id$ as select 1 id from rdb$database;
create or alter view name$to$substutite$max$id$ as select 1 id from rdb$database;
create or alter view name$to$substutite$search$ as select 1 id from rdb$database;

create or alter view v_cancel_client_order as
-- source for random choose client_order document to be cancelled
select h.id
from doc_list h
where
    h.optype_id = 1000 -- fn_oper_order_by_customer
;

create or alter view v_cancel_supplier_order as
-- Source for random choose supplier order doc to be cancelled
select h.id
from doc_list h
where
    h.optype_id = 1200 -- fn_oper_order_for_supplier
;


create or alter view v_cancel_supplier_invoice as
-- source for random choose invoice from supplier that will be cancelled
select h.id
from doc_list h
where
    h.optype_id = 2000 -- fn_oper_invoice_get
;

create or alter view v_add_invoice_to_stock as
select h.id
from doc_list h
where
    h.optype_id = 2000 -- fn_oper_invoice_get
;

create or alter view v_cancel_adding_invoice as
select h.id
from doc_list h
where
    h.optype_id = 2100 -- fn_oper_invoice_add
;

create or alter view v_cancel_customer_reserve as
-- source for s`p_cancel_customer_reserve: random take customer reserve
-- and CANCEL it (i.e. DELETE from doc_list)
select h.id
from doc_list h
where
    h.optype_id = 3300 -- fn_oper_retail_reserve
;

create or alter view v_reserve_write_off as
-- source for random take customer reserve and make WRITE-OFF (i.e. 'close' it):
select h.id, h.agent_id, h.state_id, h.dts_open, h.dts_clos, h.cost_retail
from doc_list h
where
    h.optype_id = 3300 -- fn_oper_retail_reserve
;

create or alter view v_cancel_write_off as
-- source for random take CLOSED customer reserve and CANCEL writing-off
-- (i.e. 'reopen' this doc for changes or removing):
select h.id
from doc_list h
where
    h.optype_id = 3400 -- fn_oper_retail_realization
;

create or alter view v_cancel_payment_to_supplier as
-- source for random taking INVOICE with some payment and CANCEL all payments:
select h.id, h.agent_id, h.cost_purchase
from doc_list h
where
    h.optype_id = 4000 -- fn_oper_pay_to_supplier
;

create or alter view v_cancel_customer_prepayment as
select h.id, h.agent_id, h.cost_retail
from doc_list h
where
    h.optype_id = 5000 -- fn_oper_pay_from_customer
;


create or alter view v_all_customers as
-- source for random taking agent from ALL customers
-- (see sp_customer_prepayment in case when all customer reserve docs are full paid)
select a.id
from agents a
where a.is_customer=1
;

create or alter view v_all_suppliers as
select a.id
from agents a
where a.is_supplier=1
;

create or alter view v_our_firm as
select a.id
from agents a
where a.is_our_firm=1
;

create or alter view v_all_wares as
-- source for random choose ware_id in SP_CLIENT_ORDER => SP_FILL_SHOPPING_CART
-- Plan in 3.0 (checked 06.02.2015):
-- PLAN (C ORDER TMP_SHOPCART_UNQ) // prevent from bitmap building in tmp$shopping_cart for each row of wares!
-- PLAN (A NATURAL)
select a.id
from wares a
where not exists(select * from tmp$shopping_cart c where c.id = a.id order by c.id) -- 19.09.2014
;

------------------------------
-- 12.09.2014 1920: refactoring v_random_find_xxx views for avl_res, clo_ord and ord_sup
-- use `wares` as driver table instead of scanning qdistr for seach doc_data.id
-- Performance increased from ~1250 up to ~2000 bop / minute (!)
------------------------------

-- ############## A V L => R E S ###############
-- Views for operation 'Create customer reserve from AVALIABLE remainders'
-- #############################################

create or alter view v_random_find_avl_res
as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choose ware_id in sp_customer_reserve => sp_fill_shopping_cart:
-- take record from invoice which has been added to stock and add it to set for
-- new customer reserve - from avaliable remainders (not linked with client order)
-- Checked 12.09.2014:
--PLAN (V TMP$SHOPPING_CART INDEX (TMP_SHOPCART_UNQ))
--PLAN (V QDISTR ORDER QDISTR_WARE_SNDOP_RCVOP) -- ::: NB ::: no bitmap here (speed!)
--PLAN (V W ORDER WARES_ID_DESC)
select w.id
from wares w
where
    not exists(select * from tmp$shopping_cart c where c.id = w.id order by c.id)
    and exists(
        select * from qdistr q
        where
            q.ware_id = w.id
            and q.snd_optype_id = 2100
            and q.rcv_optype_id = 3300
        order by ware_id, snd_optype_id, rcv_optype_id -- supress building index bitmap on QDistr!
    );

create or alter view v_min_id_avl_res as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choose ware_id in sp_customer_reserve => sp_fill_shopping_cart:
-- take record from invoice which has been added to stock and add it to set for
-- new customer reserve - from ***AVALIABLE*** remainders (not linked with client order)
    select w.id
    from wares w
    where
        exists(
            select * from qdistr q
            where
                q.ware_id = w.id
                and q.snd_optype_id = 2100
                and q.rcv_optype_id = 3300
            -- 3.0 only: supress building index bitmap on QDistr:
            order by ware_id, snd_optype_id, rcv_optype_id -- do NOT use this 'order by' in 2.5!
        )
    order by w.id asc -- ascend, we search for MINIMAL id
    rows 1
;

create or alter view v_max_id_avl_res as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choise record from client order to be added into supplier order
--PLAN (Q ORDER QDISTR_WARE_SNDOP_RCVOP)
--PLAN (W ORDER WARES_ID_DESC)
    select w.id
    from wares w
    where
        exists(
            select * from qdistr q
            where
                q.ware_id = w.id
                and q.snd_optype_id = 2100
                and q.rcv_optype_id = 3300
            -- 3.0 only: supress building index bitmap on QDistr:
            order by ware_id, snd_optype_id, rcv_optype_id -- do NOT use this 'order by' in 2.5!
        )
    order by w.id desc -- descend, we search for MAXIMAL id
    rows 1
;

-- ############## C L O => O R D ###############
-- Views for operation 'Create order to SUPPLIER on the basis of CUSTOMER orders'
-- #############################################

create or alter view v_random_find_clo_ord as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choise record from client order to be added into supplier order
-- Checked 12.09.2014
--PLAN (V TMP$SHOPPING_CART INDEX (TMP_SHOPCART_UNQ))
--PLAN (V QDISTR ORDER QDISTR_WARE_SNDOP_RCVOP) -- ::: NB ::: no bitmap here (speed!)
--PLAN (V W ORDER WARES_ID_DESC)
select w.id
from wares w
where
    not exists(select * from tmp$shopping_cart c where c.id = w.id order by c.id)
    and exists(
        select * from qdistr q
        where
            q.ware_id = w.id
            and q.snd_optype_id = 1000
            and q.rcv_optype_id = 1200
        -- 3.0 only: supress building index bitmap on QDistr
        order by ware_id, snd_optype_id, rcv_optype_id
    );

create or alter view v_min_id_clo_ord as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choise record from client order to be added into supplier order
    select w.id
    from wares w
    where
        exists(
            select * from qdistr q
            where
                q.ware_id = w.id
                and q.snd_optype_id = 1000
                and q.rcv_optype_id = 1200
            -- 3.0 only: supress building index bitmap on QDistr:
            order by ware_id, snd_optype_id, rcv_optype_id -- do NOT use this 'order by' in 2.5!
        )
    order by w.id asc -- ascend, we search for MINIMAL id
    rows 1
;

create or alter view v_max_id_clo_ord as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choise record from client order to be added into supplier order
    select w.id
    from wares w
        where exists(
            select * from qdistr q
            where
                q.ware_id = w.id
                and q.snd_optype_id = 1000
                and q.rcv_optype_id = 1200
            -- 3.0 only: supress building index bitmap on QDistr:
            order by ware_id, snd_optype_id, rcv_optype_id -- do NOT use this 'order by' in 2.5!
        )
    order by w.id desc -- descend, we search for MAXIMAL id
    rows 1
;

-- ############## O R D => S U P ###############
-- Views for operation 'Create invoice from supplier on the basis of OUR orders to him'
-- #############################################

create or alter view v_random_find_ord_sup as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choise record from supplier order to be added into invoice by him
-- Checked 12.09.2014:
--PLAN (V TMP$SHOPPING_CART INDEX (TMP_SHOPCART_UNQ))
--PLAN (V QDISTR ORDER QDISTR_WARE_SNDOP_RCVOP) -- ::: NB ::: no bitmap here (speed!)
--PLAN (V W ORDER WARES_ID_DESC)
    select w.id
    from wares w
    where
        not exists(select * from tmp$shopping_cart c where c.id = w.id order by c.id)
        and exists(
            select * from qdistr q
            where
                q.ware_id = w.id
                and q.snd_optype_id = 1200 -- fn_oper_order_for_supplier()
                and q.rcv_optype_id = 2000
            order by ware_id, snd_optype_id, rcv_optype_id -- supress building index bitmap on QDistr!
        )
;

create or alter view v_min_id_ord_sup as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choise record from client order to be added into supplier order
    select w.id
    from wares w
    where
        exists(
            select * from qdistr q
            where
                q.ware_id = w.id
                and q.snd_optype_id = 1200
                and q.rcv_optype_id = 2000
            -- 3.0 only: supress building index bitmap on QDistr:
            order by ware_id, snd_optype_id, rcv_optype_id -- do NOT use this 'order by' in 2.5!
        )
    order by w.id asc -- ascend, we search for MINIMAL id
    rows 1
;

create or alter view v_max_id_ord_sup as
-- 08.07.2014. used in dynamic sql in sp_get_random_id, see it's call in sp_fill_shopping_cart
-- source for random choise record from client order to be added into supplier order
    select w.id
    from wares w
        where exists(
            select * from qdistr q
            where
                q.ware_id = w.id
                and q.snd_optype_id = 1200
                and q.rcv_optype_id = 2000
            -- 3.0 only: supress building index bitmap on QDistr:
            order by ware_id, snd_optype_id, rcv_optype_id -- do NOT use this 'order by' in 2.5!
        )
    order by w.id desc -- descend, we search for MAXIMAL id
    rows 1
;

-- ############## C L O => R E S ###############
-- Views for operation 'Create reserve based on client order'
-- #############################################

create or alter view v_random_find_clo_res as
-- 08.09.2015: remove join from here, reduce number of IRs at ~1.5x
    select h.id
    from doc_list h
    where h.optype_id = 1000
    and exists(
        select *
        from doc_data d where d.doc_id = h.id
        and exists(
            select *
            from qdistr q
            where
                q.ware_id = d.ware_id
                and q.snd_optype_id = 1000 -- fn_oper_order_by_customer()
                and q.rcv_optype_id = 3300 -- fn_oper_retail_reserve()
                and q.snd_id = d.id
            -- prevent from building bitmap, 3.0 only:
            order by q.ware_id, q.snd_optype_id, q.rcv_optype_id
        )
        order by d.doc_id -- prevent from building bitmap, 3.0 only
    )
;

-------------------------------------------------------------------------------

create or alter view v_min_id_clo_res as
-- 22.09.2014
--PLAN (Q ORDER QDISTR_SNDOP_RCVOP_SNDID_DESC)
--PLAN JOIN (H ORDER PK_DOC_LIST, D INDEX (FK_DOC_DATA_DOC_LIST))
-- 05.09.2015
--PLAN (Q ORDER QDISTR_WARE_SNDOP_RCVOP)
--PLAN JOIN (H ORDER PK_DOC_LIST, D INDEX (FK_DOC_DATA_DOC_LIST))
select h.id
from doc_list h
join doc_data d on h.id = d.doc_id
where h.optype_id = 1000
and exists(
    select *
    from qdistr q
    where
        q.ware_id = d.ware_id
        and q.snd_optype_id = 1000 -- fn_oper_order_by_customer()
        and q.rcv_optype_id = 3300 -- fn_oper_retail_reserve()
        and q.snd_id = d.id
    order by q.ware_id, q.snd_optype_id, q.rcv_optype_id
)
order by h.id
rows 1
;

-------------------------------------------------------------------------------

create or alter view v_max_id_clo_res as
-- 22.09.2014
--PLAN (Q ORDER QDISTR_SNDOP_RCVOP_SNDID_DESC)
--PLAN JOIN (H ORDER DOC_LIST_ID_DESC, D INDEX (FK_DOC_DATA_DOC_LIST))
-- 05.09.2015
--PLAN (Q ORDER QDISTR_WARE_SNDOP_RCVOP)
--PLAN JOIN (H ORDER DOC_LIST_ID_DESC, D INDEX (FK_DOC_DATA_DOC_LIST))
select h.id
from doc_list h
join doc_data d on h.id = d.doc_id
where h.optype_id = 1000
and exists(
    select *
    from qdistr q
    where
        q.ware_id = d.ware_id
        and q.snd_optype_id = 1000 -- fn_oper_order_by_customer()
        and q.rcv_optype_id = 3300 -- fn_oper_retail_reserve()
        and q.snd_id = d.id
    order by q.ware_id, q.snd_optype_id, q.rcv_optype_id
)
order by h.id desc
rows 1;

-------------------------------------------------------------------------------

create or alter view v_random_find_non_paid_invoice as
-- 09.09.2014. Used in dynamic SQL in sp_get_random_id, see SP_PAYMENT_COMMON
-- Source for random choose document of accepted invoice (optype=2100)
-- which still has some cost to be paid (==> has records in PDistr)
-- Introduced instead of v_p`distr_non_paid_realization to avoid scans doc_list
select p.snd_id as id -- this value match doc_list.id
from pdistr p
where
    p.snd_optype_id = 2100 -- fn_oper_invoice_add()
    and p.rcv_optype_id = 4000 -- fn_oper_pay_to_supplier()
;

-------------------------------------------------------------------------------

create or alter view v_min_non_paid_invoice as
-- 09.09.2014: source for fast get min snd_id (==> doc_list.id) before making
-- random choise of accepted invoice (optype=2100) which still has some
-- cost to be paid (==> has records in PDistr)
-- PLAN (P ORDER PDISTR_SNDOP_RCVOP_SNDID_ASC)
select p.snd_id as id -- this value match doc_list.id
from pdistr p
where
    p.snd_optype_id = 2100 -- fn_oper_invoice_add()
    and p.rcv_optype_id = 4000 -- fn_oper_pay_to_supplier()
order by
    snd_id
rows 1
;

-------------------------------------------------------------------------------

create or alter view v_max_non_paid_invoice as
-- 09.09.2014: source for fast get MAX snd_id (==> doc_list.id) before making
-- random choise of accepted invoice (optype=2100) which still has some
-- cost to be paid (==> has records in PDistr)
-- PLAN (P ORDER PDISTR_SNDOP_RCVOP_SNDID_DESC)
select p.snd_id as id -- this value match doc_list.id
from pdistr p
where
    p.snd_optype_id = 2100 -- fn_oper_invoice_add()
    and p.rcv_optype_id = 4000 -- fn_oper_pay_to_supplier()
order by
    p.snd_optype_id desc, p.rcv_optype_id desc, p.snd_id desc
rows 1
;

-------------------------------------------------------------------------------

create or alter view v_random_find_non_paid_realizn as
-- 09.09.2014. Used in dynamic SQL in sp_get_random_id, see SP_PAYMENT_COMMON
-- Source for random choose document of written-off realization (optype=3400)
-- which still has some cost to be paid (==> has records in PDistr)
-- Introduced instead of v_p`distr_non_paid_realization to avoid scans doc_list
select p.snd_id as id -- this value match doc_list.id
from pdistr p
where
    p.snd_optype_id = 3400 -- fn_oper_retail_realization()
    and p.rcv_optype_id = 5000 -- fn_oper_pay_from_customer()
;

-------------------------------------------------------------------------------

create or alter view v_min_non_paid_realizn as
-- 09.09.2014: source for fast get min snd_id (==> doc_list.id) before making
-- random choise of written-off realization (optype=3400) which still has some
-- cost to be paid (==> has records in PDistr)
-- PLAN (P ORDER PDISTR_SNDOP_RCVOP_SNDID_ASC)
select p.snd_id as id -- this value match doc_list.id
from pdistr p
where
    p.snd_optype_id = 3400 -- fn_oper_retail_realization()
    and p.rcv_optype_id = 5000 -- fn_oper_pay_from_customer()
order by
    snd_id
rows 1
;

-------------------------------------------------------------------------------

create or alter view v_max_non_paid_realizn as
-- 09.09.2014: source for fast get MAX snd_id (==> doc_list.id) before making
-- random choise of written-off realization (optype=3400) which still has some
-- cost to be paid (==> has records in PDistr)
-- PLAN (P ORDER PDISTR_SNDOP_RCVOP_SNDID_DESC)
select p.snd_id as id -- this value match doc_list.id
from pdistr p
where
    p.snd_optype_id = 3400 -- fn_oper_retail_realization()
    and p.rcv_optype_id = 5000 -- fn_oper_pay_from_customer()
order by
    p.snd_optype_id desc, p.rcv_optype_id desc, p.snd_id desc
rows 1
;


-------------------------------------------------------------------------------

create or alter view v_saldo_invnt as
-- 21.04.2014
-- ::: NB ::: this view can return NEGATIVE remainders in qty_xxx
-- if parallel attaches call of sp_make_invnt_saldo
-- (because of deleting rows in invnt_turnover_log in this SP)
-- !!! look at table INVNT_SALDO for actual remainders !!!
select
    ng.ware_id
    ,sum(o.m_qty_clo * ng.qty_diff) qty_clo
    ,sum(o.m_qty_clr * ng.qty_diff) qty_clr
    ,sum(o.m_qty_ord * ng.qty_diff) qty_ord
    ,sum(o.m_qty_sup * ng.qty_diff) qty_sup
    ,sum(o.m_qty_avl * ng.qty_diff) qty_avl
    ,sum(o.m_qty_res * ng.qty_diff) qty_res
    ,sum(o.m_cost_inc * ng.qty_diff) qty_inc
    ,sum(o.m_cost_out * ng.qty_diff) qty_out
    ,sum(o.m_cost_inc * ng.cost_diff) cost_inc
    ,sum(o.m_cost_out * ng.cost_diff) cost_out
    -- amount "on hand" as it seen by accounter:
    ,sum(o.m_qty_avl * ng.qty_diff) + sum(o.m_qty_res * ng.qty_diff) qty_acn
    -- total cost "on hand" in purchasing prices:
    ,sum(o.m_cost_inc * ng.cost_diff) - sum(o.m_cost_out * ng.cost_diff) cost_acn
from invnt_turnover_log ng
join optypes o on ng.optype_id + 0 = o.id + 0 -- ==> for 3.0 only: hash join, reduce number of`optypes` scans!
group by 1
;



---------------------------

create or alter view v_doc_detailed as
-- Used in all app unit for returning final resultset to client.
-- Also very useful for debugging
select
    h.id doc_id,
    h.optype_id,
    o.mcode as oper,
    h.base_doc_id,
    d.id doc_data_id,
    d.ware_id,
    d.qty,
    coalesce(d.cost_purchase, h.cost_purchase) cost_purchase, -- cost in purchase price
    coalesce(d.cost_retail, h.cost_retail) cost_retail, -- cost in retail price
    n.qty_clo, -- amount of ORDERED by customer (which we not yet sent to supplier)
    n.qty_clr, -- amount of REFUSED by customer
    n.qty_ord, -- amount that we have already sent to supplier
    n.qty_sup, -- amount in invoices of supplier, in <open> state
    n.qty_inc, -- amount of incomings (when invoices were added)
    n.qty_avl, -- amount avaliable to be sold (usially - due to refused client orders)
    n.qty_res, -- amount in reserve to be sold to customer
    n.qty_out, -- amount of written-off
    n.cost_inc, -- total cost of incomes, in purchase prices
    n.cost_out, -- total cost of outgoings, in purchase prices
    n.qty_acn,
    n.cost_acn,
    h.state_id,
    h.agent_id,
    d.dts_edit,
    h.dts_open,
    h.dts_fix,
    h.dts_clos,
    s.mcode state
from doc_list h
    join optypes o on h.optype_id = o.id
    join doc_states s on h.state_id=s.id
    left join doc_data d on h.id = d.doc_id
    -- ::: NB ::: do NOT remove "left" from here otherwise performance will degrade
    -- (FB will not push predicate inside view; 22.04.2014)
    LEFT join v_saldo_invnt n on d.ware_id=n.ware_id
--left join sp_saldo_invnt(d.ware_id) n on 1=1 -- speed
;

-------------------------------------------------------------------------------
-- ###   v i e w s    f o r    m o n i t o r   g a t h e r e d   d a t a   ####
-------------------------------------------------------------------------------
-- Following views are used in 'oltp_isql_run_worker.bat' during its first
-- launched ISQL session makes final report. These views will contain data
-- only when config parameter mon_unit_perf=1.
create or alter view z_mon_stat_per_units as
-- 29.08.2014: data from measuring statistics per each unit
-- (need FB rev. >= 60013: new mon$ counters were introduced, 28.08.2014)
-- 25.01.2015: added rec_locks, rec_confl.
-- 06.02.2015: reorder columns, made all `max` values most-right
select
     m.unit
    ,count(*) iter_counts
    -------------- speed -------------
    ,avg(m.elapsed_ms) avg_elap_ms
    ,avg(1000.00 * ( (m.rec_seq_reads + m.rec_idx_reads + m.bkv_reads ) / nullif(m.elapsed_ms,0))  ) avg_rec_reads_sec
    ,avg(1000.00 * ( (m.rec_inserts + m.rec_updates + m.rec_deletes ) / nullif(m.elapsed_ms,0))  ) avg_rec_dmls_sec
    ,avg(1000.00 * ( m.rec_backouts / nullif(m.elapsed_ms,0))  ) avg_bkos_sec
    ,avg(1000.00 * ( m.rec_purges / nullif(m.elapsed_ms,0))  ) avg_purg_sec
    ,avg(1000.00 * ( m.rec_expunges / nullif(m.elapsed_ms,0))  ) avg_xpng_sec
    ,avg(1000.00 * ( m.pg_fetches / nullif(m.elapsed_ms,0)) ) avg_fetches_sec
    ,avg(1000.00 * ( m.pg_marks / nullif(m.elapsed_ms,0)) ) avg_marks_sec
    ,avg(1000.00 * ( m.pg_reads / nullif(m.elapsed_ms,0)) ) avg_reads_sec
    ,avg(1000.00 * ( m.pg_writes / nullif(m.elapsed_ms,0)) ) avg_writes_sec
    -------------- reads ---------------
    ,avg(m.rec_seq_reads) avg_seq
    ,avg(m.rec_idx_reads) avg_idx
    ,avg(m.rec_rpt_reads) avg_rpt
    ,avg(m.bkv_reads) avg_bkv
    ,avg(m.frg_reads) avg_frg
    ,avg(m.bkv_per_seq_idx_rpt) avg_bkv_per_rec
    ,avg(m.frg_per_seq_idx_rpt) avg_frg_per_rec
    ---------- modifications ----------
    ,avg(m.rec_inserts) avg_ins
    ,avg(m.rec_updates) avg_upd
    ,avg(m.rec_deletes) avg_del
    ,avg(m.rec_backouts) avg_bko
    ,avg(m.rec_purges) avg_pur
    ,avg(m.rec_expunges) avg_exp
    --------------- io -----------------
    ,avg(m.pg_fetches) avg_fetches
    ,avg(m.pg_marks) avg_marks
    ,avg(m.pg_reads) avg_reads
    ,avg(m.pg_writes) avg_writes
    ----------- locks and conflicts ----------
    ,avg(m.rec_locks) avg_locks
    ,avg(m.rec_confl) avg_confl
    ,datediff( minute from min(m.dts) to max(m.dts) ) workload_minutes
    --- 06.02.2015 moved here all MAX values, separate them from AVG ones: ---
    ,max(m.rec_seq_reads) max_seq
    ,max(m.rec_idx_reads) max_idx
    ,max(m.rec_rpt_reads) max_rpt
    ,max(m.bkv_reads) max_bkv
    ,max(m.frg_reads) max_frg
    ,max(m.bkv_per_seq_idx_rpt) max_bkv_per_rec
    ,max(m.frg_per_seq_idx_rpt) max_frg_per_rec
    ,max(m.rec_inserts) max_ins
    ,max(m.rec_updates) max_upd
    ,max(m.rec_deletes) max_del
    ,max(m.rec_backouts) max_bko
    ,max(m.rec_purges) max_pur
    ,max(m.rec_expunges) max_exp
    ,max(m.pg_fetches) max_fetches
    ,max(m.pg_marks) max_marks
    ,max(m.pg_reads) max_reads
    ,max(m.pg_writes) max_writes
    ,max(m.rec_locks) max_locks
    ,max(m.rec_confl) max_confl
from mon_log m
group by unit
;

--------------------------------------------------------------------------------

create or alter view z_mon_stat_per_tables
as
-- 29.08.2014: data from measuring statistics per each unit+table
-- (new table MON$TABLE_STATS required, see srv_fill_mon, srv_fill_tmp_mon)
-- 25.01.2015: added rec_locks, rec_confl;
-- ::: do NOT add `bkv_per_seq_idx_rpt` and `frg_per_seq_idx_rpt` into WHERE
-- clause with check sum > 0, because they can be NULL, see DDL!
-- 06.02.2015: reorder columns, made all `max` values most-right
select
     t.table_name
    ,t.unit
    ,count(*) iter_counts
    --------------- reads ---------------
    ,avg(t.rec_seq_reads) avg_seq
    ,avg(t.rec_idx_reads) avg_idx
    ,avg(t.rec_rpt_reads) avg_rpt
    ,avg(t.bkv_reads) avg_bkv
    ,avg(t.frg_reads) avg_frg
    ,avg(t.bkv_per_seq_idx_rpt) avg_bkv_per_rec
    ,avg(t.frg_per_seq_idx_rpt) avg_frg_per_rec
    ---------- modifications ----------
    ,avg(t.rec_inserts) avg_ins
    ,avg(t.rec_updates) avg_upd
    ,avg(t.rec_deletes) avg_del
    ,avg(t.rec_backouts) avg_bko
    ,avg(t.rec_purges) avg_pur
    ,avg(t.rec_expunges) avg_exp
    ----------- locks and conflicts ----------
    ,avg(t.rec_locks) avg_locks
    ,avg(t.rec_confl) avg_confl
    ,datediff( minute from min(t.dts) to max(t.dts) ) elapsed_minutes
    --- 06.02.2015 moved here all MAX values, separate them from AVG ones: ---
    ,max(t.rec_seq_reads) max_seq
    ,max(t.rec_idx_reads) max_idx
    ,max(t.rec_rpt_reads) max_rpt
    ,max(t.bkv_reads) max_bkv
    ,max(t.frg_reads) max_frg
    ,max(t.bkv_per_seq_idx_rpt) max_bkv_per_rec
    ,max(t.frg_per_seq_idx_rpt) max_frg_per_rec
    ,max(t.rec_inserts) max_ins
    ,max(t.rec_updates) max_upd
    ,max(t.rec_deletes) max_del
    ,max(t.rec_backouts) max_bko
    ,max(t.rec_purges) max_pur
    ,max(t.rec_expunges) max_exp
    ,max(t.rec_locks) max_locks
    ,max(t.rec_confl) max_confl
from mon_log_table_stats t
where
      t.rec_seq_reads
    + t.rec_idx_reads
    + t.rec_rpt_reads
    + t.bkv_reads
    + t.frg_reads
    + t.rec_inserts
    + t.rec_updates
    + t.rec_deletes
    + t.rec_backouts
    + t.rec_purges
    + t.rec_expunges
    + t.rec_locks
    + t.rec_confl
    > 0
group by t.table_name,t.unit
;

-------------------------------------------------------------------------------

create or alter view z_estimated_perf_per_minute as
-- Do NOT delete! 28.10.2015.
-- This view is used in oltp_isql_run_worker.bat (.sh) when it creates final report.
-- Table PERF_ESTIMATED is filled up by temply created .sql which scans log
-- of 1st ISQL session (which, in turn, makes final report). This log contains
-- rows like this:
-- EST_OVERALL_AT_MINUTE_SINCE_BEG         0.00      0
-- - where 1st number is estimated performance value and 2nd is datediff(minute)
-- from test start to the moment when each business transaction SUCCESSFULLY finished.
-- Data in this view is performance value in *dynamic* but with detalization per
-- ONE minute, from time when all ISQL sessions start (rather then all other reports
-- which make starting point after database was warmed up).
-- This report can help to find proper value of warm-time in oltpNN_config.
select
    e.minute_since_test_start
    ,avg(e.success_count) avg_estimated
    ,min(e.success_count) / nullif(avg(e.success_count), 0) min_to_avg_ratio
    ,max(e.success_count) / nullif(avg(e.success_count), 0) max_to_avg_ratio
    ,count(e.success_count) rows_aggregated
    ,count(distinct e.att_id) distinct_attachments -- 22.12.2015: helps to ensure that all ISQL sessions were alive in every minute of test work time
from perf_estimated e
where e.minute_since_test_start>0
group by e.minute_since_test_start
;



-------------------------------------------------------------------------------
--######################   d e b u g    v i e w s   ############################
-------------------------------------------------------------------------------

create or alter view z_current_test_settings as
-- This view is used in 1run_oltp_emulbat (.sh) to display current settings before test run.
-- Do NOT delete it!
select s.mcode as setting_name, s.svalue as setting_value, 'init' as stype
from settings s
where s.working_mode='INIT' and s.mcode='WORKING_MODE'

UNION ALL

select '--- Detalization for WORKING_MODE: ---' as setting_name, '' as setting_value, 'inf1' as stype
from rdb$database
UNION ALL

select '    ' || t.mcode as setting_name, t.svalue as setting_value, 'mode' as stype
from settings s
join settings t on s.svalue=t.working_mode
where s.working_mode='INIT' and s.mcode='WORKING_MODE'

UNION ALL

select '--- Main test settings: ---' as setting_name, '' as setting_value, 'inf2' as stype
from rdb$database
UNION ALL

select setting_name, setting_value, 'main' as stype
from (
    select '    ' || s.mcode as setting_name, s.svalue as setting_value
    from settings s
    where
        s.working_mode='COMMON'
        and s.mcode
            in (
                 'BUILD_WITH_SPLIT_HEAVY_TABS'
                ,'BUILD_WITH_SEPAR_QDISTR_IDX'
                ,'BUILD_WITH_QD_COMPOUND_ORDR'
                ,'ENABLE_MON_QUERY'
                ,'TRACED_UNITS'
                ,'HALT_TEST_ON_ERRORS'
                ,'QMISM_VERIFY_BITSET'
                ,'LOG_PK_VIOLATION'
                ,'ENABLE_RESERVES_WHEN_ADD_INVOICE'
               )
    order by setting_name
) x
;

--------------------------------------------------------------------------------

create or alter view z_settings_pivot as
-- vivid show of all workload settings (pivot rows to separate columns
-- for each important kind of setting). Currently selected workload mode
-- is marked by '*' and displayed in UPPER case, all other - in lower.
-- The change these settings open oltp_main_filling.sql and find EB
-- with 'v_insert_settings_statement' variable
select
    iif(s.working_mode=c.svalue,'* '||upper(s.working_mode), lower(s.working_mode) ) as working_mode
    ,cast(max( iif(mcode = upper('c_wares_max_id'), s.svalue, null ) ) as int) as wares_cnt
    ,cast(max( iif(mcode = upper('c_customer_doc_max_rows'), s.svalue, null ) ) as int) as cust_max_rows
    ,cast(max( iif(mcode = upper('c_supplier_doc_max_rows'), s.svalue, null ) ) as int) as supp_max_rows
    ,cast(max( iif(mcode = upper('c_customer_doc_max_qty'), s.svalue, null ) ) as int) as cust_max_qty
    ,cast(max( iif(mcode = upper('c_supplier_doc_max_qty'), s.svalue, null ) ) as int) as supp_max_qty
    ,cast(max( iif(mcode = upper('c_number_of_agents'), s.svalue, null ) ) as int) as agents_cnt
from settings s
left join (select s.svalue from settings s where s.mcode='working_mode') c on s.working_mode=c.svalue
where s.mcode
in (
     'c_wares_max_id'
    ,'c_customer_doc_max_rows'
    ,'c_supplier_doc_max_rows'
    ,'c_customer_doc_max_qty'
    ,'c_supplier_doc_max_qty'
    ,'c_number_of_agents'
)
group by s.working_mode, c.svalue
order by
    iif(s.working_mode starting with 'DEBUG',  0,
    iif(s.working_mode starting with 'SMALL',  1,
    iif(s.working_mode starting with 'MEDIUM', 2,
    iif(s.working_mode starting with 'LARGE',  3,
    iif(s.working_mode starting with 'HEAVY',  5,
    null) ) ) ) )
    nulls last
   ,s.working_mode
;

--------------------------------------------------------------------------------

create or alter view z_qd_indices_ddl as

-- This view is used in 1run_oltp_emulbat (.sh) to display current DDL of QDistr xor XQD* indices.
-- Do NOT delete it!

with recursive
r as (
    select
        ri.rdb$relation_name tab_name
        ,ri.rdb$index_name idx_name
        ,rs.rdb$field_name fld_name
        ,rs.rdb$field_position fld_pos
        ,cast( trim(rs.rdb$field_name) as varchar(512)) as idx_key
    from rdb$indices ri
    join rdb$index_segments rs using ( rdb$index_name )
    left join (
        select cast(t.svalue as int) as svalue
        from settings t
        where t.working_mode='COMMON' and t.mcode='BUILD_WITH_SPLIT_HEAVY_TABS'
    ) t on 1=1
    where
        rs.rdb$field_position = 0
        and (
            t.svalue = 0 and trim( ri.rdb$relation_name ) is not distinct from 'QDISTR'
            or
            t.svalue = 1 and trim( ri.rdb$relation_name ) starts with 'XQD_'
        )

    UNION ALL

    select
        r.tab_name
        ,r.idx_name
        ,rs.rdb$field_name
        ,rs.rdb$field_position
        ,r.idx_key || ',' || trim(rs.rdb$field_name) 
    from r
    join rdb$indices ri
        on r.idx_name = ri.rdb$index_name
    join rdb$index_segments rs
        on
            ri.rdb$index_name = rs.rdb$index_name
            and r.fld_pos +1 = rs.rdb$field_position
)
select r.tab_name, r.idx_name, max(r.idx_key) as idx_key
from r
group by r.tab_name, r.idx_name
;

--------------------------------------------------------------------------------

create or alter view z_halt_log as -- upd 28.09.2014
select p.id, p.fb_gdscode, p.unit, p.trn_id, p.dump_trn, p.att_id, p.exc_unit, p.info, p.ip, p.dts_beg, e.fb_mnemona, p.exc_info,p.stack
from perf_log p
join (
    select g.trn_id, g.fb_gdscode
    from perf_log g
    -- 335544558    check_constraint    Operation violates CHECK constraint @1 on view or table @2.
    -- 335544347    not_valid    Validation error for column @1, value "@2".
    -- if table has unique constraint: 335544665 unique_key_violation (violation of PRIMARY or UNIQUE KEY constraint "T1_XY" on table "T1")
    -- if table has only unique index: 335544349 no_dup (attempt to store duplicate value (visible to active transactions) in unique index "T2_XY")
    where g.fb_gdscode in (      0 -- 3.0 SC trouble, core-4565 (gdscode can come in when-section with value = 0!)
                                ,335544347, 335544558 -- not_valid or viol. of check constr.
                                ,335544665, 335544349 -- viol. of UNQ constraint or just unq. index (without binding to unq constr)
                                ,335544466 -- viol. of FOREIGN KEY constraint @1 on table @2
                                ,335544838 -- Foreign key reference target does not exist (when attempt to ins/upd in DETAIL table FK-field with value which doesn`t exists in PARENT)
                                ,335544839 -- Foreign key references are present for the record  (when attempt to upd/del in PARENT table PK-field and rows in DETAIL (no-cascaded!) exists for old value)
                          )
    group by 1,2
) g
on p.trn_id = g.trn_id
left join fb_errors e on p.fb_gdscode = e.fb_gdscode
order by p.id
;

--------------------------------------------------------------------------------

create or alter view z_agents_tunrover_saldo as
-- 4 misc reports and debug, do not delete: agent turnovers and sums; only in 3.0
select
    m.agent_id, m.doc_id, o.mcode, o.acn_type
    ,o.m_supp_debt * m.cost_purchase vol_supp
    ,o.m_cust_debt * m.cost_retail vol_cust
    ,sum(o.m_supp_debt * m.cost_purchase)over(partition by m.agent_id) sum_supp
    ,sum(o.m_cust_debt * m.cost_retail  )over(partition by m.agent_id) sum_cust
from money_turnover_log m
join optypes o on m.optype_id = o.id
;

------------------------------------------------------------------------------
-- create or alter view z_mon_stat_per_units ==> moved in 'oltp_misc_debug.sql'
-- create or alter view z_mon_stat_per_tables ==> moved in 'oltp_misc_debug.sql'
------------------------------------------------------------------------------



--------------------------------------------------------------------------------
-- ########################   T R I G G E R S   ################################
--------------------------------------------------------------------------------

set term ^;
-- not needed in 3.0, see DDL of their `ID` field ('generated as identity'):
--create or alter trigger wares_bi for wares active
--before insert position 0
--as
--begin
--   new.id = coalesce(new.id, gen_id(g_common, 1) );
--end
--^
--
--create or alter trigger phrases_bi for phrases active
--before insert position 0
--as
--begin
--   new.id = coalesce(new.id, gen_id(g_common, 1) );
--end
--^
--
--create or alter trigger agents_bi for agents active
--before insert position 0
--as
--begin
--   new.id = coalesce(new.id, gen_id(g_common, 1) );
--end
--^
--
--create or alter trigger invnt_saldo_bi for invnt_saldo active
--before insert position 0
--as
--begin
--   new.id = coalesce(new.id, gen_id(g_common, 1) );
--end
--^

create or alter trigger money_turnover_log_bi for money_turnover_log active before insert position 0 as
begin
    new.id = coalesce(new.id, gen_id(g_common, 1) ); -- new.id is NOT null for all docs except payments
end

^ -- money_turnover_log_bi

create or alter trigger perf_log_bi for perf_log active before insert position 0 as
begin
    new.id = coalesce(new.id, gen_id(g_perf_log, 1) );
end

^ -- perf_log_bi
-- not needed in 3.0, see DDL of their `ID` field ('generated as identity'):
--create or alter trigger pdistr_bi for pdistr
--active before insert position 0 as
--begin
--    new.id = coalesce(new.id, gen_id(g_common,1));
--end
--
--^ -- pdistr_bi
--
--create or alter trigger pstorned_bi for pstorned
--active before insert position 0 as
--begin
--    new.id = coalesce(new.id, gen_id(g_common,1));
--end
--
--^ -- pstorned_bi

set term ;^

set term ^;

--------------------------------------------------------------------------------

create or alter trigger doc_list_biud for doc_list
active before insert or update or delete position 0
as
    declare v_msg dm_info = '';
    declare v_info dm_info = '';
    declare v_this dm_dbobj = 'doc_list_biud';
    declare v_affects_on_inventory_balance smallint;
    declare v_old_op type of dm_idb;
    declare v_new_op type of dm_idb;
begin

    if ( inserting ) then
        new.id = coalesce(new.id, gen_id(g_common,1));

    v_info = 'dh='|| iif(not inserting, old.id, new.id)
             || ', op='||iif(inserting,'INS',iif(updating,'UPD','DEL'))
             || iif(not inserting, ' old='||old.optype_id, '')
             || iif(not deleting,  ' new='||new.optype_id, '');

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log( 1, v_this, null, v_info );

    if (inserting) then
    begin
        new.state_id = coalesce(new.state_id, fn_doc_open_state());
        -- 'i'=incoming; 'o' = outgoing; 's' = payment to supplier; 'c' = payment from customer
        new.acn_type = (select o.acn_type from optypes o where o.id = new.optype_id);
        v_msg = v_msg || ' new.id='||new.id||', acn_type='||coalesce(new.acn_type,'<?>');
    end

    if ( not deleting ) then
        begin
            if ( new.state_id <> fn_doc_open_state() ) then
            begin
                new.dts_fix = coalesce(new.dts_fix, 'now');
                new.dts_clos = iif( new.state_id in( fn_doc_clos_state(), fn_doc_canc_state() ), 'now', null);
            end
            if ( new.state_id = fn_doc_open_state() ) then -- 31.03.2014
            begin
                new.dts_fix = null;
                new.dts_clos = null;
            end
        end
    else
        v_msg = v_msg || ' doc='||old.id||', op='||old.optype_id;

    -- add to invnt_turnover_log
    -- rows that are 'totalled' in doc_data when make doc content in sp_create_doc_using_fifo
    -- (there are multiple rows from qdistr and multiple calls to sp_add_doc_data for each one)
    v_old_op=iif(inserting, null, old.optype_id);
    v_new_op=iif(deleting,  null, new.optype_id);

    select max(maxvalue(abs(o.m_qty_clo), abs(o.m_qty_clr), abs(o.m_qty_ord), abs(o.m_qty_sup), abs(o.m_qty_avl), abs(o.m_qty_res))) q_mult_abs_max
    from optypes o
    where o.id in( :v_old_op, :v_new_op )
    into v_affects_on_inventory_balance;

    -- 20.09.2014 1825: remove calls of s`p_add_invnt_log to SPs (reduce scans of doc_data)
    -- 16.09.2014: moved here from d`oc_data_biud:
    if ( v_affects_on_inventory_balance > 0 and (deleting or updating and new.optype_id is distinct from old.optype_id) )
    then
        execute procedure sp_kill_qty_storno(
            old.id,
            old.optype_id,
            iif( deleting, null, new.optype_id),
            iif( updating, 1, 0),
            iif( deleting, 1, 0)
        );

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this,null,v_msg);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            'error in '||v_this,
            gdscode,
            v_msg,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- doc_list_biud

-------------------------------------------------------------------------------

create or alter trigger doc_list_aiud for doc_list
active after insert or update or delete position 0
as
    declare v_affects_on_monetary_balance smallint;
    declare v_affects_on_inventory_balance smallint;
    declare v_doc_id dm_idb;
    declare v_old_op type of dm_idb;
    declare v_new_op type of dm_idb;
    declare v_old_mult type of dm_sign = null;
    declare v_new_mult type of dm_sign = null;
    declare v_affects_on_customer_saldo smallint;
    declare v_affects_on_supplier_saldo smallint;
    declare v_oper_changing_cust_saldo type of dm_idb;
    declare v_oper_changing_supp_saldo type of dm_idb;
    declare v_cost_diff type of dm_cost;
    declare v_msg type of dm_info = '';
    declare v_this dm_dbobj = 'doc_list_aiud';
    declare v_catch_bitset bigint;
begin

    -- AFTER trigger on master table (THIS) will fired BEFORE any triggers on detail (doc_data)!
    -- www.sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1081231&msg=15685218
    v_affects_on_monetary_balance = 1;
    v_affects_on_inventory_balance = 1;
    v_doc_id=iif(deleting, old.id, new.id);
    v_old_op=iif(inserting, null, old.optype_id);
    v_new_op=iif(deleting,  null, new.optype_id);

    v_msg = 'dh='|| iif(not inserting, old.id, new.id)
             || ', op='||iif(inserting,'INS',iif(updating,'UPD','DEL'))
             || iif(not inserting, ' old='||old.optype_id, '')
             || iif(not deleting,  ' new='||new.optype_id, '');

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log( 1, v_this , null, v_msg );

    select
        iif(m_cust_4old<>0, m_cust_4old, m_supp_4old), -- they are mutually excluded: only ONE can be <> 0
        iif(m_cust_4new<>0, m_cust_4new, m_supp_4new), -- they are mutually excluded: only ONE can be <> 0
        iif(m_cust_4old<>0, v_old_op, iif(m_cust_4new<>0, v_new_op, 0)) v_oper_changing_cust_saldo,
        iif(m_supp_4old<>0, v_old_op, iif(m_supp_4new<>0, v_new_op, 0)) v_oper_changing_supp_saldo,
        iif(m_cust_4old<>0 or m_cust_4new<>0, 1, 0) v_affects_on_customer_saldo,
        iif(m_supp_4old<>0 or m_supp_4new<>0, 1, 0) v_affects_on_supplier_saldo,
        q_mult_abs_max
    from(
        select
            max(iif(o.id=:v_old_op, o.m_cust_debt, null)) m_cust_4old,
            max(iif(o.id=:v_old_op, o.m_supp_debt, null)) m_supp_4old,
            max(iif(o.id=:v_new_op, o.m_cust_debt, null)) m_cust_4new,
            max(iif(o.id=:v_new_op, o.m_supp_debt, null)) m_supp_4new,
            max(iif(o.id=:v_old_op, :v_old_op, null)) v_old_op,
            max(iif(o.id=:v_new_op, :v_new_op, null)) v_new_op,
            max(maxvalue(abs(o.m_qty_clo), abs(o.m_qty_clr), abs(o.m_qty_ord), abs(o.m_qty_sup), abs(o.m_qty_avl), abs(o.m_qty_res))) q_mult_abs_max
        from optypes o
        where o.id in( :v_old_op, :v_new_op )
    )
    into
        v_old_mult,
        v_new_mult,
        v_oper_changing_cust_saldo,
        v_oper_changing_supp_saldo,
        v_affects_on_customer_saldo,
        v_affects_on_supplier_saldo,
        v_affects_on_inventory_balance;

    ---------------
    if ( v_affects_on_inventory_balance > 0
         and
         (
             inserting and new.cost_purchase > 0
             or
             updating
             and ( new.cost_purchase is distinct from old.cost_purchase
                   or
                   new.optype_id is distinct from old.optype_id
                )
             or deleting
         )
       ) then
    begin
        v_catch_bitset = cast(rdb$get_context('USER_SESSION','QMISM_VERIFY_BITSET') as bigint);
        if ( bin_and( v_catch_bitset, 1 ) <> 0 ) then
        begin
            -- check that number of rows in qdistr+qstorned exactly equals
            -- add to perf_log row with exc. info about mismatch, gds=335544558
            execute procedure srv_find_qd_qs_mism( iif(deleting, old.id, new.id), iif(deleting, :v_old_op, :v_new_op), iif(deleting, 1, 0) );
        end

        if ( bin_and( v_catch_bitset, 2 ) <> 0 ) then
        begin
            --execute procedure srv_catch_neg_remainders( new.ware_id, new.optype_id, new.doc_list_id, new.doc_data_id, new.qty_diff );
            execute procedure srv_check_neg_remainders(  iif(deleting, old.id, new.id), iif(deleting, :v_old_op, :v_new_op) );
        end
    end
    ---------------

    if ( coalesce(v_old_mult,0)=0 and coalesce(v_new_mult,0)=0
     ) then -- this op does NOT affect on MONETARY turnovers (of customer or supplier)
        --####
        v_affects_on_monetary_balance = 0;
        --####
    
    if ( v_affects_on_monetary_balance <> 0 ) then
    begin
        if (
           new.cost_purchase is distinct from old.cost_purchase
           or
           new.cost_retail is distinct from old.cost_retail
         )
        then -- creating new doc or deleting it
            begin
                ----------
                if ( v_oper_changing_cust_saldo <> 0 or v_oper_changing_supp_saldo <> 0 ) then
                begin
                    if (  inserting or updating ) then
                        begin
                
                            if ( v_oper_changing_cust_saldo <> 0 ) then
                                v_cost_diff = new.cost_retail - iif(inserting, 0, old.cost_retail);
                            else
                                v_cost_diff = new.cost_purchase - iif(inserting, 0, old.cost_purchase);
    
                            -- 1: add rows for v_cost_diff for being storned further
                            execute procedure sp_multiply_rows_for_pdistr(
                                new.id,
                                new.agent_id,
                                v_new_op,
                                v_cost_diff
                            );
    
                            -- 2: storn old docs by v_cost_diff ( fn_oper_pay_to_supplier, fn_oper_pay_from_customer )
                            execute procedure sp_make_cost_storno( new.id, :v_new_op, new.agent_id, :v_cost_diff );
    
                        end -- ins or upd
                    else --- deleting in doc_list
                        begin
                            -- return back records from pstorned to pdistr
                            -- ::: nb ::: use MERGE instead of insert because partial
                            -- cost storning (when move PART of cost from pdistr to pstorned)
                            execute procedure sp_kill_cost_storno( old.id );
                        end -- deleting
    
                end -- v_oper_changing_cust_saldo<>0 or v_oper_changing_supp_saldo<>0
    
                ------------------- add to money_turnover_log ----------------------
                execute procedure sp_add_money_log(
                    iif(not deleting, new.id, old.id),
                    0, -- v_old_mult,
                    0, -- old.agent_id,
                    0, -- v_old_op,
                    0, -- old.cost_purchase,
                    0, -- old.cost_retail,
                    1, -- v_new_mult,
                    iif(not deleting, new.agent_id, old.agent_id),
                    iif(not deleting, new.optype_id, old.optype_id), --v_new_op,
                    iif(not deleting, new.cost_purchase - coalesce(old.cost_purchase,0), -old.cost_purchase), --  new.cost_purchase,
                    iif(not deleting, new.cost_retail - coalesce(old.cost_retail,0), - old.cost_retail) -- new.cost_retail
                );
    
            end
             -- ########################################
        else -- cost_purchase and cost_retail - the same (sp_add_invoice_to_stock; sp_reserve_write_off; s`p_cancel_adding_invoice; s`p_cancel_write_off)
             -- ########################################
    
            if (updating) then
            begin
              if ( --new.agent_id is distinct from old.agent_id -- todo later, not implemented yet
                   --or
                   new.optype_id is distinct from old.optype_id
                 ) then
              begin
                -----------
                if ( v_oper_changing_cust_saldo <> 0 or v_oper_changing_supp_saldo <> 0  ) then
                begin
                    if ( v_new_op in (v_oper_changing_cust_saldo, v_oper_changing_supp_saldo) ) then
                    begin
                        -- F O R W A R D   operation: sp_add_invoice_to_stock; sp_reserve_write_off
                        v_cost_diff = iif( v_oper_changing_cust_saldo <> 0, new.cost_retail, new.cost_purchase );
    
                        -- 1: add rows for rest of cost for being storned by further docs
                        execute procedure sp_multiply_rows_for_pdistr(
                            new.id,
                            new.agent_id,
                            v_new_op,
                            v_cost_diff
                        );
                        -- 2: storn old docs by v_cost_diff  (sp_add_invoice_to_stock; sp_reserve_write_off)
                        execute procedure sp_make_cost_storno( new.id, :v_new_op, new.agent_id, :v_cost_diff );
    
                    end
                    else -- R E V E R T   operation: s`p_cancel_adding_invoice, s`p_cancel_write_off
                    begin
                       -- AFTER trigger on master table (THIS) will fired BEFORE any triggers on detail (doc_data)!
                       -- http://www.sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1081231&msg=15685218
                       -- return back records from pstorned to pdistr
                       -- ::: nb ::: use MERGE instead of insert because partial
                       -- cost storning (when move PART of cost from pdistr to pstorned)
                       execute procedure sp_kill_cost_storno( old.id );
                   end -- v_new_op in (v_oper_changing_cust_saldo, v_oper_changing_supp_saldo) ==> true / false
    
                end -- ( v_oper_changing_cust_saldo <> 0 or v_oper_changing_supp_saldo <> 0  )
    
                ------------------- add to money_turnover_log ----------------------
                execute procedure sp_add_money_log(
                    old.id,
                    v_old_mult,
                    old.agent_id,
                    v_old_op,
                    old.cost_purchase,
                    old.cost_retail,
                    v_new_mult,
                    new.agent_id,
                    v_new_op,
                    new.cost_purchase,
                    new.cost_retail
                );
            end -- changes occur in agent_id or optype_id
        end -- updating

    end -- v_affects_on_monetary_balance <> 0

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this);

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            'error in '||v_this,
            gdscode,
            v_msg,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- doc_list_aiud

set term ;^


-------------------------------------------------------------------------------

set term ^;
-- NOTE: currently tis trigger is created with INactive state.
-- It will be Active at the end of all database building process, see 'oltp_data_filling.sql'
create or alter trigger trg_connect inactive on connect as
begin
    execute procedure sp_init_ctx;
    if ( rdb$get_context ('SYSTEM', 'NETWORK_PROTOCOL') is null ) then
    begin
       insert into perf_log(unit, info )
       values( 'trg_connect', 'attach using NON-TCP protocol' );
    end
end

^ -- trg_connect

set term ;^


--------------------------------------------------------------------------------
-- ####################   C O R E    P R O C s    a n d   F U N C s   ##########
--------------------------------------------------------------------------------
set term ^;

create or alter procedure sp_add_doc_list(
    a_gen_id type of dm_idb, -- preliminary obtained from sequence (used in s`p_make_qty_storno)
    a_optype_id type of dm_idb,
    a_agent_id type of dm_idb,
    a_new_state type of dm_idb default null,
    a_base_doc_id type of dm_idb default null, -- need only for customer reserve which linked to client order
    a_new_cost_purchase type of dm_cost default 0,
    a_new_cost_retail type of dm_cost default 0
) returns(
    id dm_idb,
    dbkey dm_dbkey
)
as
begin
    -- add single record into doc_list (document header)
    insert into doc_list(
        id, -- 06.09.2014 2200
        optype_id,
        agent_id,
        state_id,
        base_doc_id,
        cost_purchase,
        cost_retail
    )
    values(
        coalesce(:a_gen_id, gen_id(g_common,1)),
        :a_optype_id,
        :a_agent_id,
        :a_new_state,
        :a_base_doc_id,
        :a_new_cost_purchase,
        :a_new_cost_retail
    )
    returning id, rdb$db_key
    into id, dbkey;

    rdb$set_context('USER_SESSION','ADD_INFO','doc='||id||': created Ok'); -- to be displayed in log of 1run_oltp_emul.bat (debug)

    if ( rdb$get_context('USER_TRANSACTION','INIT_DATA_POP') = 1 )
    then -- now we only run INITIAL data filling, see 1run_oltp_emul.bat
        -- 18.07.2014: added gen_id to analyze in init data populate script,
        -- see 1run_oltp_emul.bat 
        id = id + 0 * gen_id(g_init_pop, 1);

    suspend;
end

^   --  sp_add_doc_list

------------------------------------------------

create or alter procedure sp_add_doc_data(
    a_doc_id dm_idb,
    a_optype_id dm_idb,
    a_gen_dd_id dm_idb, -- preliminary calculated ID for new record in doc_data (reduce lock-contention of GEN page)
    a_gen_nt_id dm_idb, -- preliminary calculated ID for new record in invnt_turnover_log (reduce lock-contention of GEN page)
    a_ware_id dm_idb,
    a_qty type of dm_qty,
    a_cost_purchase type of dm_cost,
    a_cost_retail type of dm_cost
) returns(
    id dm_idb,
    dbkey dm_dbkey
)
as
    declare v_this dm_dbobj = 'sp_add_doc_data';
begin
    -- add to performance log timestamp about start/finish this unit:
    -- uncomment if need analyze perormance in mon_log tables
    -- + update settings set svalue=',sp_make_qty_storno,sp_add_doc_data,pdetl_add,' where mcode='TRACED_UNITS';
    -- execute procedure sp_add_perf_log(1, v_this, null, 'a_gen_dd_id='||trim(coalesce(a_gen_dd_id||'=>ins','null'))||', a_dbkey: '||trim(iif(a_dbkey is null,'isNull','hasVal=>upd')) );

    -- insert single record into doc_data
    -- :: NB :: update & "if row_count = 0 ? => insert" is much effective
    -- then insert & "when uniq_violation ? => update" (no backouts, less fetches)
    if ( a_gen_dd_id is NOT null ) then
        insert into doc_data(
            id,
            doc_id,
            ware_id,
            qty,
            cost_purchase,
            cost_retail,
            dts_edit)
        values(
            :a_gen_dd_id,
            :a_doc_id,
            :a_ware_id,
            :a_qty,
            :a_cost_purchase,
            :a_cost_retail,
            'now')
        returning id, rdb$db_key into id, dbkey;
    else
        begin
            update doc_data t set
                t.qty = t.qty + :a_qty,
                t.cost_purchase = t.cost_purchase + :a_cost_purchase,
                t.cost_retail = t.cost_retail + :a_cost_retail,
                t.dts_edit = 'now'
            where t.doc_id = :a_doc_id and t.ware_id = :a_ware_id
            returning t.id, t.rdb$db_key into id, dbkey;

            if ( row_count = 0 ) then
                insert into doc_data( doc_id, ware_id, qty, cost_purchase, cost_retail, dts_edit)
                values( :a_doc_id, :a_ware_id, :a_qty, :a_cost_purchase, :a_cost_retail, 'now')
                returning id, rdb$db_key into id, dbkey;
        end
    ----------------------------------------------------------------------------
    -- 20.09.2014: move here from trigger on doc_list
    -- (reduce scans of doc_data)
    if ( :a_qty <> 0 ) then
        insert into invnt_turnover_log(
             id
            ,ware_id
            ,qty_diff
            ,cost_diff
            ,doc_list_id
            ,doc_pref
            ,doc_data_id
            ,optype_id
        ) values (
            :a_gen_nt_id
            ,:a_ware_id
            ,:a_qty
            ,:a_cost_purchase
            ,:a_doc_id
            ,fn_mcode_for_oper(:a_optype_id)
            ,:id
            ,:a_optype_id
        );

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    -- uncomment if need analyze perormance in mon_log tables:
    --execute procedure sp_add_to_perf_log(v_this,null,'out: id='||id);

    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'out: id='||coalesce(id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_add_doc_data

create or alter function fn_get_random_quantity(
    a_ctx_max_name dm_ctxnv
)
returns dm_qty as
    declare v_min double precision;
    declare v_max double precision;
begin
  v_min = 0.5;
  v_max = cast( rdb$get_context('USER_SESSION',a_ctx_max_name) as int) + 0.5;
  return cast( v_min + rand()* (v_max - v_min)  as int);

end

^ -- fn_get_random_quantity

create or alter function fn_get_random_cost(
    a_ctx_min_name dm_ctxnv,
    a_ctx_max_name dm_ctxnv,
    a_make_check_before smallint default 1
)
returns dm_cost as
    declare v_min double precision;
    declare v_max double precision;
begin
  if (a_make_check_before = 1) then
        execute procedure sp_check_ctx(
            'USER_SESSION',a_ctx_min_name,
            'USER_SESSION',a_ctx_max_name
      );
  v_min = cast( rdb$get_context('USER_SESSION',a_ctx_min_name) as int) - 0.5;
  v_max = cast( rdb$get_context('USER_SESSION',a_ctx_max_name) as int) + 0.5;
  return cast( v_min + rand()* (v_max - v_min)  as dm_cost);

end

^ -- fn_get_random_cost

create or alter function fn_get_random_customer returns bigint as
begin
    return (select id_selected from sp_get_random_id('v_all_customers', null, null, 0) );
end
^

create or alter function fn_get_random_supplier returns bigint as
begin
    return (select id_selected from sp_get_random_id('v_all_suppliers', null, null, 0) );
end

^ -- fn_get_random_customer

------------------------------------------------------------------------------

create or alter procedure sp_make_qty_storno(
    a_optype_id dm_idb
    ,a_agent_id dm_idb
    ,a_state_id type of dm_idb default null
    ,a_client_order_id type of dm_idb default null
    ,a_rows_in_shopcart int default null
    ,a_qsum_in_shopcart dm_qty default null
)
returns (
    doc_list_id bigint
)
as
    declare c_gen_inc_step_qd int = 100; -- size of `batch` for get at once new IDs for QDistr (reduce lock-contention of gen page)
    declare v_gen_inc_iter_qd int; -- increments from 1  up to c_gen_inc_step_qd and then restarts again from 1
    declare v_gen_inc_last_qd dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_qd)
    declare c_gen_inc_step_dd int = 20; -- size of `batch` for get at once new IDs for doc_data (reduce lock-contention of gen page)
    declare v_gen_inc_iter_dd int; -- increments from 1  up to c_gen_inc_step_dd and then restarts again from 1
    declare v_gen_inc_last_dd dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_dd)
    declare c_gen_inc_step_nt int = 20; -- size of `batch` for get at once new IDs for invnt_turnover_log (reduce lock-contention of gen page)
    declare v_gen_inc_iter_nt int; -- increments from 1  up to c_gen_inc_step_dd and then restarts again from 1
    declare v_gen_inc_last_nt dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_dd)
    declare v_inserting_table dm_dbobj;
    declare v_id type of dm_idb;
    declare v_curr_tx bigint;
    declare v_ware_id dm_idb;
    declare v_dh_new_id bigint;
    declare v_dd_new_id bigint;
    declare v_nt_new_id dm_idb;
    declare v_dd_dbkey dm_dbkey;
    declare v_dd_clo_id dm_idb;
    declare v_doc_data_purchase_sum dm_cost;
    declare v_doc_data_retail_sum dm_cost;
    declare v_doc_list_purchase_sum dm_cost;
    declare v_doc_list_retail_sum dm_cost;
    declare v_doc_list_dbkey dm_dbkey;
    declare v_rows_added int;
    declare v_storno_sub smallint;
    declare v_qty_storned_acc type of dm_qty;
    declare v_qty_required type of dm_qty;
    declare v_qty_could_storn type of dm_qty;
    declare v_snd_optype_id type of dm_idb;
    declare v_rcv_optype_id type of dm_idb;
    declare v_next_rcv_op type of dm_idb;
    declare v_this dm_dbobj = 'sp_make_qty_storno';
    declare v_call dm_dbobj;
    declare v_info dm_info;
    declare v_rows int = 0;
    declare v_lock int = 0;
    declare v_skip int = 0;
    declare v_dummy bigint;
    declare v_sign dm_sign;
    declare v_cq_id dm_idb;
    declare v_cq_snd_list_id dm_idb;
    declare v_cq_snd_data_id dm_idb;
    declare v_cq_snd_qty dm_qty;
    declare v_cq_snd_purchase dm_cost;
    declare v_cq_snd_retail dm_cost;
    declare v_cq_snd_optype_id dm_idb;
    declare v_cq_rcv_optype_id type of dm_idb;
    declare v_cq_trn_id dm_idb;
    declare v_cq_dts timestamp;

    declare c_shop_cart cursor for (
        select
            id,
            dd_clo_id,
            snd_optype_id,
            rcv_optype_id,
            qty,
            storno_sub
        from (
                select
                    c.id,
                    cast(null as dm_idb) as dd_clo_id,  -- 22.09.2014
                    c.snd_optype_id,
                    c.rcv_optype_id,
                    c.qty,
                    c.storno_sub
                from tmp$shopping_cart c

                UNION ALL

                select
                    c.id,
                    c.snd_id, -- 22.09.2014, for clo_res
                    r.snd_optype_id,
                    c.rcv_optype_id,
                    c.qty,
                    r.storno_sub
                from tmp$shopping_cart c
                INNER join rules_for_qdistr r
                  on :a_client_order_id is NOT null
                     -- only in 3.0: hash join (todo: check perf when NL, create indices)
                     and c.rcv_optype_id + 0 = r.rcv_optype_id + 0 -- PLAN HASH (R NATURAL, C NATURAL)
                     and r.storno_sub = 2
        ) u
        order by id, storno_sub
    );
    ----------------------------------------------------------------------------
    -- 22.09.2014: two separate cursors for diff storno_sub
    declare c_make_amount_distr_1 cursor for (
        select
             q.id
            ,q.doc_id as snd_list_id
            ,q.snd_id as snd_data_id
            ,q.snd_qty
            ,q.snd_purchase
            ,q.snd_retail
            ,q.snd_optype_id
            ,q.rcv_optype_id
            ,q.trn_id
            ,q.dts
        -- 'v_qdistr_source_1' initially this is one-to-one projection of 'QDistr' table. 
        -- But it can be replaced with 'AUTOGEN_QDnnnn' when config create_with_split_heavy_tabs = 1.
        from v_qdistr_source_1 q 
        where
            q.ware_id = :v_ware_id -- find invoices to be storned storning by new customer reserve, and all other ops except storning client orders
            and q.snd_optype_id = :v_snd_optype_id
            and q.rcv_optype_id = :v_rcv_optype_id
        order by
            q.doc_id
              + 0 -- handle docs in FIFO order
            ,:v_sign * q.id -- attempt to reduce locks: odd and even Tx handles rows in opposite manner (for the same doc) thus have a chance do not encounter locked rows at all
    );

    declare c_make_amount_distr_2 cursor for (
        select
             q.id
            ,q.doc_id as snd_list_id
            ,q.snd_id as snd_data_id
            ,q.snd_qty
            ,q.snd_purchase
            ,q.snd_retail
            ,q.snd_optype_id
            ,q.rcv_optype_id
            ,q.trn_id
            ,q.dts
        -- 'v_qdistr_source_2' initially this is one-to-one projection of 'QDistr' table.
        -- But it can be replaced with 'AUTOGEN_QD1000' when config create_with_split_heavy_tabs = 1.
        from v_qdistr_source_2 q
        where
            q.ware_id = :v_ware_id -- find client orders to be storned by new customer reserve
            and q.snd_optype_id = :v_snd_optype_id
            and q.rcv_optype_id = :v_rcv_optype_id
            and q.snd_id = :v_dd_clo_id 
        order by
            q.doc_id
              + 0 -- handle docs in FIFO order
            ,:v_sign * q.id -- attempt to reduce locks: odd and even Tx handles rows in opposite manner (for the same doc) thus have a chance do not encounter locked rows at all
    );
begin

    -- Issue about QDistr & QStorned: for each SINGLE record from doc_data with
    -- qty=<N> table QDistr initially contains <N> DIFFERENT records (if no storning
    -- yet occur for that amount from doc_data).
    -- Each storning takes off some records from this set and "moves" them into
    -- table QStorned. This SP *does* such storning.
    ----------------------------------------------------------------------------
    -- Performs attempt to make distribution of AMOUNTS which were added to "sender" docs
    -- and then 'multiplied' (added in QDISTR table) using "value-to-rows"
    -- algorithm in sp_multiply_rows_for_qdistr. If some row is locked now,
    -- SUPRESS exc`eption and skip to next one. If required amount can NOT
    -- be satisfied, it will be reduced (in tmp$shopping_cart) or even REMOVED
    -- at all from tmp$shopping_cart (without raising exc: we must minimize them)
    -- ::: NB :::
    -- Method: "try_to_lock_src => upd_confl ? skip : {ins_target & del_in_src}"
    -- is more than 3 times FASTER than: "ins_tgt => uniq_viol ? skip : del_in_src"
    -- (see benchmark in letter to dimitr 26.08.2014 13:00)
    -- 01.09.2014: refactored, remove cursor on doc_data (huge values of IDX_READS!)
    -- 02.09.2014: move here code block from  sp_create_doc_using_fifo, further reduce scans of doc_data
    -- 06.09.2014: doc_data: 3 idx_reads per each unique ware_id (one here, two in SP s`rv_find_qd_qs_mism)

    select r.rcv_optype_id
    from rules_for_qdistr r
    where r.snd_optype_id = :a_optype_id
    into v_next_rcv_op;

    v_call = v_this;
    -- doc_list.id must be defined PRELIMINARY, before cursor that handles with qdistr:
    v_dh_new_id = gen_id(g_common, 1);

    v_info =
        'op='||a_optype_id
        ||', next_op='||coalesce(v_next_rcv_op,'<?>')
        ||coalesce(', clo='||a_client_order_id, '');
    execute procedure sp_add_perf_log(1, v_call, null, v_info);

    v_qty_could_storn = 0;
    v_rows_added = 0;
    v_doc_list_purchase_sum = 0;
    v_doc_list_retail_sum = 0;

    v_gen_inc_iter_dd = 1;
    c_gen_inc_step_dd = coalesce( 1 + a_rows_in_shopcart, 20 ); -- adjust value to increment IDs in DOC_DATA at one call of gen_id
    v_gen_inc_last_dd = gen_id( g_doc_data, :c_gen_inc_step_dd );-- take bulk IDs at once (reduce lock-contention for GEN page)

    v_gen_inc_iter_qd = 1;
    c_gen_inc_step_qd = coalesce( 1 + a_qsum_in_shopcart, 100 ); -- adjust value to increment IDs in QDISTR at one call of gen_id
    v_gen_inc_last_qd = gen_id( g_qdistr, :c_gen_inc_step_qd );-- take bulk IDs at once (reduce lock-contention for GEN page)

    v_gen_inc_iter_nt = 1;
    c_gen_inc_step_nt = coalesce( 1 + a_rows_in_shopcart, 20 ); -- adjust value to increment IDs in INVNT_TURNOVER_LOG at one call of gen_id
    v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );-- take bulk IDs at once (reduce lock-contention for GEN page)

    v_sign = iif( bin_and(current_transaction, 1)=0, 1, -1);

    -- rules_for_qdistr.storno_sub = 2 - storno data of clo when creating customer RESERVE:
    -- MODE              SND_OPTYPE_ID    RCV_OPTYPE_ID    STORNO_SUB
    -- mult_rows_only    1000             3300             2
    -- Result of cursor c_shop_cart for ware_id=1 when call from sp_customer_reserve:
    -- ID    SND_OPTYPE_ID    RCV_OPTYPE_ID    QTY    STORNO_SUB
    -- 1          2100             3300         1         1
    -- 1          1000             3300         1         2
    open c_shop_cart;
    while (1=1) do
    begin
        fetch c_shop_cart
        into v_ware_id, v_dd_clo_id, v_snd_optype_id, v_rcv_optype_id, v_qty_required, v_storno_sub;
        if ( row_count = 0 ) then leave;

        v_qty_could_storn = iif(v_storno_sub=1,  0, v_qty_could_storn);
        v_qty_required = iif(v_storno_sub=1,  v_qty_required, v_qty_could_storn);

        v_dd_dbkey = iif(v_storno_sub=1,  null, v_dd_dbkey);
        v_dd_new_id = iif(v_storno_sub=1,  null, v_dd_new_id);

        v_qty_storned_acc = 0; -- how many units will provide required Qty from CURRENT LINE of shopping cart
        v_doc_data_purchase_sum = 0;
        v_doc_data_retail_sum = 0;

        if ( v_storno_sub = 1 ) then
            open c_make_amount_distr_1;
        else
            open c_make_amount_distr_2;
        ------------------------------------------------------------------------
        while ( :v_qty_storned_acc < :v_qty_required ) do
        begin
            if ( v_storno_sub = 1 ) then
                fetch c_make_amount_distr_1
                into
                    v_cq_id,v_cq_snd_list_id,v_cq_snd_data_id
                    ,v_cq_snd_qty,v_cq_snd_purchase,v_cq_snd_retail
                    ,v_cq_snd_optype_id,v_cq_rcv_optype_id
                    ,v_cq_trn_id,v_cq_dts;
            else
                fetch c_make_amount_distr_2
                into
                    v_cq_id,v_cq_snd_list_id,v_cq_snd_data_id
                    ,v_cq_snd_qty,v_cq_snd_purchase,v_cq_snd_retail
                    ,v_cq_snd_optype_id,v_cq_rcv_optype_id
                    ,v_cq_trn_id,v_cq_dts;

            if ( row_count = 0 ) then leave;
            v_info =  'fetch '
                ||iif( v_storno_sub = 1, 'c_make_amount_distr_1', 'c_make_amount_distr_2')
                ||', qd.id='||v_cq_id;
            v_rows = v_rows + 1; -- total ATTEMPTS to make delete/update in QDistr

            -- ### A.C.H.T.U.N.G ###
            -- Make increment of `v_gen_inc_iter_**` ALWAYS BEFORE any lock-conflict statements
            -- (otherwise duplicates will appear in ID because of suppressing lock-conflict ex`c.)
            -- #####################
            if ( v_storno_sub = 1 ) then
            begin -- calculate subsequent value for doc_data.id from previously obtained batch:
                if ( v_gen_inc_iter_qd >= c_gen_inc_step_qd ) then -- its time to get another batch of IDs
                begin
                    v_gen_inc_iter_qd = 1;
                    -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                    v_gen_inc_last_qd = gen_id( g_qdistr, :c_gen_inc_step_qd );
                end

                if ( v_qty_storned_acc = 0 ) then
                begin
                    -- NO rows could be locked in QDistr (by now) for providing
                    -- QTY from current line of shopping cart ==> we did not yet
                    -- inserted row into doc_data with :v_ware_id ==> get subseq.
                    -- value for :v_dd_new_id from `pool`:
                    if ( v_gen_inc_iter_dd >= c_gen_inc_step_dd ) then -- its time to get another batch of IDs
                    begin
                        v_gen_inc_iter_dd = 1;
                        -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                        v_gen_inc_last_dd = gen_id( g_doc_data, :c_gen_inc_step_dd );
                    end
                    v_dd_new_id = v_gen_inc_last_dd - ( c_gen_inc_step_dd - v_gen_inc_iter_dd );
                    v_gen_inc_iter_dd = v_gen_inc_iter_dd + 1;
                end
            end


            -- 26.10.2015. Additional begin..end block needs for providing DML
            -- 'atomicity' of BOTH tables pdistr & pstorned! Otherwise changes
            -- can become inconsistent if online validation will catch table-2
            -- after this code finish changes on table-1 but BEFORE it will
            -- start to change table-2.
            -- See CORE-4973 (example of WRONG code which did not used this addi block!)
            begin
                -- We can place delete sttmt BEFORE insert because of using explicit cursor and
                -- fetch old fields data (which is to be moved into QStorned) into declared vars:
                if ( v_storno_sub = 1 ) then
                    begin
                        v_call = v_this || ':try_del_qdsub1';
                        -- execute procedure sp_add_perf_log(1, v_call, null, v_info); -- 10.02.2015
                        -- rdb$set_context('USER_TRANSACTION','DBG_MAKE_STSUB1_TRY_DEL_QD_ID', v_cq_id);
    
                        delete from v_qdistr_source_1 q where current of c_make_amount_distr_1; --- lock_conflict can occur here
    
                        -- rdb$set_context('USER_TRANSACTION','DBG_MAKE_STSUB1_OK_DEL_QD_ID', v_cq_id);
                        -- execute procedure sp_add_perf_log(0, v_call);
                    end --  v_storno_sub = 1
                else
                    begin
    
                        v_call = v_this || ':try_del_qdsub2';
                        -- execute procedure sp_add_perf_log(1, v_call, null, v_info);
                        -- rdb$set_context('USER_TRANSACTION','DBG_MAKE_STSUB2_TRY_DEL_QD_ID', v_cq_id);
    
                        -- When config parameter 'create_with_split_heavy_tabs' is 1 then 'v_qdistr_source_2' should be changed to 'XQD_*'
                        delete from v_qdistr_source_2 q where current of c_make_amount_distr_2; --- lock_conflict can occur here
    
                        -- rdb$set_context('USER_TRANSACTION','DBG_MAKE_STSUB2_OK_DEL_QD_ID', v_cq_id);
                        -- execute procedure sp_add_perf_log(0, v_call);
                    end --  v_storno_sub = 2
    
                if ( v_storno_sub = 1 ) then -- ==>  distr_mode containing 'new_doc'
                begin
                    v_inserting_table = 'qdistr';
                    -- iter=1: v_id = 12345 - (100-1); iter=2: 12345 - (100-2); ...
                    v_id = v_gen_inc_last_qd - ( c_gen_inc_step_qd - v_gen_inc_iter_qd );
    
                    -- debug info for logging in srv_log_dups_qd_qs if PK
                    -- violation will occur on INSERT INTO QSTORNED statement
                    -- (remained for possible analysis):
                    v_call = v_this || ':try_ins_qdsub1';
                    v_info = v_info || ', try INSERT into QDistr id='||v_id;
    
                    -- execute procedure sp_add_perf_log(1, v_call, null, v_info); -- 10.02.2015, debug
                    -- rdb$set_context('USER_TRANSACTION','DBG_MAKE_STSUB1_TRY_INS_QD_ID', v_id);
    
                    insert into v_qdistr_target_1 (
                        id,
                        doc_id,
                        ware_id,
                        snd_optype_id,
                        rcv_optype_id,
                        snd_id,
                        snd_qty,
                        snd_purchase,
                        snd_retail)
                    values(
                        :v_id,
                        :v_dh_new_id,
                        :v_ware_id,
                        :a_optype_id,
                        :v_next_rcv_op,
                        :v_dd_new_id,
                        :v_cq_snd_qty,
                        :v_cq_snd_purchase,
                        :v_cq_snd_retail
                    );
    
                    -- rdb$set_context('USER_TRANSACTION','DBG_MAKE_STSUB1_OK_INS_QD_ID', v_id);
                    -- execute procedure sp_add_perf_log(0, v_call);
    
                    v_gen_inc_iter_qd = v_gen_inc_iter_qd + 1;
                    v_info = v_info || ' - ok';
                end --  v_storno_sub = 1
    
                v_inserting_table = 'qstorned';
                v_id =  v_cq_id;
    
                -- debug info for logging in srv_log_dups_qd_qs if PK
                -- violation will occur on INSERT INTO QSTORNED statement
                -- (remained for possible analysis):
                v_info = v_info||', try INSERT into QStorned: id='||:v_id;
                v_call = v_this || ':try_ins_qStorn';
    
                -- execute procedure sp_add_perf_log(1, v_call, null, v_info); -- 10.02.2015, debug
                -- rdb$set_context('USER_TRANSACTION','DBG_MAKE_QSTORN_TRY_INS_QS_ID', v_id);
    
                if ( v_storno_sub = 1 )  then
                    insert into v_qstorned_target_1 (
                         id,
                         doc_id, ware_id, dts, -- do NOT specify field `trn_id` here! 09.10.2014 2120
                         snd_optype_id, snd_id, snd_qty,
                         rcv_optype_id,
                         rcv_doc_id, -- 30.12.2014
                         rcv_id,
                         snd_purchase, snd_retail
                    ) values (
                        :v_id
                        ,:v_cq_snd_list_id, :v_ware_id, :v_cq_dts -- dis 09.10.2014 2120: :v_cq_trn_id,
                        ,:v_cq_snd_optype_id, :v_cq_snd_data_id,:v_cq_snd_qty
                        ,:v_cq_rcv_optype_id
                        ,:v_dh_new_id -- 30.12.2014
                        ,:v_dd_new_id
                        ,:v_cq_snd_purchase,:v_cq_snd_retail
                    );
                else
                    insert into v_qstorned_target_2 (
                        id,
                        doc_id, ware_id, dts, -- do NOT specify field `trn_id` here! 09.10.2014 2120
                        snd_optype_id, snd_id, snd_qty,
                        rcv_optype_id,
                        rcv_doc_id, -- 30.12.2014
                        rcv_id,
                        snd_purchase, snd_retail
                    ) values (
                        :v_id
                        ,:v_cq_snd_list_id, :v_ware_id, :v_cq_dts -- dis 09.10.2014 2120: :v_cq_trn_id,
                        ,:v_cq_snd_optype_id, :v_cq_snd_data_id,:v_cq_snd_qty
                        ,:v_cq_rcv_optype_id
                        ,:v_dh_new_id -- 30.12.2014
                        ,:v_dd_new_id
                        ,:v_cq_snd_purchase,:v_cq_snd_retail
                    );
    
    
                v_info = v_info || ' - ok';
    
                v_qty_storned_acc = v_qty_storned_acc + v_cq_snd_qty; -- ==> will be written in doc_data.qty (actual amount that could be gathered)
                v_lock = v_lock + 1; -- total number of SUCCESSFULY locked records
    
                if ( v_storno_sub = 1 ) then
                begin
                    -- increment sums that will be written into doc_data line:
                    v_qty_could_storn = v_qty_could_storn + v_cq_snd_qty;
                    v_doc_data_purchase_sum = v_doc_data_purchase_sum + v_cq_snd_purchase;
                    v_doc_data_retail_sum = v_doc_data_retail_sum + v_cq_snd_retail;
                end
            end -- begin..end for atomicity of changes several tables (CORE-4973!)
        when any do
            -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
            -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
            -- catched it's kind of exception!
            -- 1) tracker.firebirdsql.org/browse/CORE-3275
            --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
            -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
            begin
                if ( fn_is_lock_trouble(gdscode) ) then
                    -- suppress this kind of exc`eption and
                    -- skip to next record!
                    v_skip = v_skip + 1;
                else
                    begin
                        -- ###############################################
                        -- PK violation on INSERT INTO QSTORNED, log this:
                        -- ###############################################
                        if ( fn_is_uniqueness_trouble(gdscode) ) then
                            -- 12.02.2015: the reason of PK violations is unpredictable order
                            -- of UNDO, ultimately explained by dimitr, see letters in e-mail.
                            -- Also: sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1142271&msg=17257984
                            execute procedure srv_log_dups_qd_qs( -- 09.10.2014: add log info using auton Tx
                                :v_call,
                                gdscode,
                                :v_inserting_table,
                                :v_id,
                                :v_info
                            );

                        exception; -- ::: nb ::: anonimous but in when-block!
                    end
            end
        end -- cursor on QDistr find rows for storning amount from tmp$shopping_cart for current v_ware_id, and MAKE such storning (move them in QStorned table)
        if ( v_storno_sub = 1 ) then
            close c_make_amount_distr_1;
        else
            close c_make_amount_distr_2;


        if ( v_dd_new_id is not null and v_storno_sub = 1 ) then
        begin
            if ( v_qty_storned_acc > 0 ) then
                begin
                    if (doc_list_id is null) then
                    begin
                        -- add new record in doc_list (header)
                        execute procedure sp_add_doc_list(
                            :v_dh_new_id
                            ,:a_optype_id
                            ,:a_agent_id
                            ,:a_state_id
                            ,:a_client_order_id
                        )
                        returning_values :doc_list_id, :v_doc_list_dbkey;
                    end

                    if ( v_gen_inc_iter_nt = c_gen_inc_step_nt ) then -- its time to get another batch of IDs
                    begin
                        v_gen_inc_iter_nt = 1;
                        -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
                        v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );
                    end
                    v_nt_new_id = v_gen_inc_last_nt - ( c_gen_inc_step_nt - v_gen_inc_iter_nt );
                    v_gen_inc_iter_nt = v_gen_inc_iter_nt + 1;

                    -- single update of doc_data for each ware after scanning N records in qdistr:
                    -- (remove call of sp_multiply_rows_for_qdistr from d`oc_data_aiud):
                    execute procedure sp_add_doc_data(
                        :v_dh_new_id -- preliminary defined above
                        ,:a_optype_id
                        ,:v_dd_new_id -- preliminary calculated above (to reduce lock-contention of GEN page)
                        ,:v_nt_new_id -- preliminary calculated above (to reduce lock-contention of GEN page)
                        ,:v_ware_id
                        ,:v_qty_could_storn
                        ,:v_doc_data_purchase_sum
                        ,:v_doc_data_retail_sum
                    )
                    returning_values :v_dummy, :v_dd_dbkey;
        
                    v_rows_added = v_rows_added + 1;
                    -- increment sums that will be written into doc header:
                    v_doc_list_purchase_sum = v_doc_list_purchase_sum + v_doc_data_purchase_sum;
                    v_doc_list_retail_sum = v_doc_list_retail_sum + v_doc_data_retail_sum;
                end
        end

    end -- cursor on tmp$shopping_cart c [join rules_for_qdistr r]
    close c_shop_cart;


    if ( :doc_list_id is NOT null and v_rows_added > 0) then --  v_lock > 0 ) then
        begin
            -- single update of doc header (not for every row added in doc_data)
            -- Trigger d`oc_list_aiud will call sp_add_invnt_log to add rows to invnt_turnover_log
            update doc_list h set
                h.cost_purchase = :v_doc_list_purchase_sum,
                h.cost_retail = :v_doc_list_retail_sum,
                h.dts_fix = iif( :a_state_id = fn_doc_fix_state(), 'now', h.dts_fix)
            where h.rdb$db_key = :v_doc_list_dbkey;

        end -- ( :doc_list_id is NOT null )
    else
        begin
            v_info =
                fn_mcode_for_oper(a_optype_id)
                ||iif(a_optype_id = fn_oper_retail_reserve(), ', clo='||coalesce( a_client_order_id, '<null>'), '')
                ||',  rows in tmp$cart: '||(select count(*) from tmp$shopping_cart);
    
            if ( a_client_order_id is null ) then -- ==> all except call of sp_customer_reserve for client order
                exception ex_cant_find_row_for_qdistr using( a_optype_id, (select count(*) from tmp$shopping_cart) );
            --'no rows found for FIFO-distribution: optype=@1, rows in tmp$shopping_cart=@2';
        end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    v_call = v_this;
    execute procedure sp_add_perf_log(0, v_call, null, 'dh='||coalesce(:doc_list_id,'<?>')||', qd ('||iif(:v_sign=1,'asc','dec')||'): capt='||v_lock||', skip='||v_skip||', scan='||v_rows||'; dd: add='||v_rows_added );

    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_call,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_make_qty_storno

--------------------------------------------------------------------------------

create or alter procedure sp_split_into_words(
    a_text dm_name,
    a_dels varchar(50) default ',.<>/?;:''"[]{}`~!@#$%^&*()-_=+\\|/',
    a_special char(1) default ' '
)
returns (
  word dm_name
) as
begin
-- Aux SP, used only in oltp_data_filling.sql to filling table PATTERNS
-- with miscelan combinations of words to be used in SIMILAR TO testing.
for
    with recursive
    j as( -- loop #1: transform list of delimeters to rows
        select s,1 i, substring(s from 1 for 1) del
        from(
          select replace(:a_dels,:a_special,'') s
          from rdb$database
        )
        
        UNION ALL
        
        select s, i+1, substring(s from i+1 for 1)
        from j
        where substring(s from i+1 for 1)<>''
    )

    ,d as(
        select :a_text s, :a_special sp from rdb$database
    )
    ,e as( -- loop #2: perform replacing each delimeter to `space`
        select d.s, replace(d.s, j.del, :a_special) s1, j.i, j.del
        from d join j on j.i=1

        UNION ALL

        select e.s, replace(e.s1, j.del, :a_special) s1, j.i, j.del
        from e
        -- nb: here 'column unknown: e.i' error will be on old builds of 2.5,
        -- e.g: WI-V2.5.2.26540 (letter from Alexey Kovyazin, 24.08.2014 14:34)
        join j on j.i = e.i + 1
    )
    ,f as(
        select s1 from e order by i desc rows 1
    )
    
    ,r as ( -- loop #3: perform split text into single words
        select iif(t.k>0, substring(t.s from t.k+1 ), t.s) s,
             iif(t.k>0,position( del, substring(t.s from t.k+1 )),-1) k,
             t.i,
             t.del,
             iif(t.k>0,left(t.s, t.k-1),t.s) word
        from(
          select f.s1 s, d.sp del, position(d.sp, s1) k, 0 i from f cross join d
        )t

        UNION ALL

        select iif(r.k>0, substring(r.s from r.k+1 ), r.s) s,
             iif(r.k>0,position(r.del, substring(r.s from r.k+1 )),-1) k,
             r.i+1,
             r.del,
             iif(r.k>0,left(r.s, r.k-1),r.s) word
        from r
        where r.k>=0
    )
    select word from r where word>''
    into
        word
do
    suspend;
end

^ -- sp_split_into_words

create or alter procedure srv_random_unit_choice(
    a_included_modes dm_info default '',
    a_included_kinds dm_info default '',
    a_excluded_modes dm_info default '',
    a_excluded_kinds dm_info default ''
)
returns(
    unit dm_name,
    sort_prior int,
    rnd_weight int,
    r double precision,
    c int,
    n int
) as
    declare r_max int;
    declare v_last_recalc_idx_dts timestamp;
    declare v_last_recalc_idx_minutes_ago int;
    declare v_this dm_dbobj = 'srv_random_unit_choice';
    declare c_unit_for_mon_query dm_dbobj = 'srv_fill_mon'; -- do NOT change the name of thios SP!
    declare function fn_internal_enable_mon_query  returns smallint deterministic as
    begin
        return ( cast(rdb$get_context('USER_SESSION', 'ENABLE_MON_QUERY') as smallint) );
    end
begin
    -- refactored 18.07.2014 (for usage in init data pop)
    -- sample: select * from srv_random_unit_choice( '','creation,state_next','','removal' )
    -- (will return first randomly choosen record related to creation of document
    -- or to changing its state in 'forward' way; excludes all cancellations and change
    -- doc states in 'backward')
    a_included_modes = coalesce( a_included_modes, '');
    a_included_kinds = coalesce( a_included_kinds, '');
    a_excluded_modes = coalesce( a_excluded_modes, '');
    a_excluded_kinds = coalesce( a_excluded_kinds, '');

    select g.dts_beg
    from perf_log g where g.unit = 'srv_recalc_idx_stat'
    order by g.dts_beg desc rows 1
    into v_last_recalc_idx_dts;
    -- FB 3.0: on database with size = 101Gb, non-cached:
    -- 454 ms, 92 read(s), 95 fetch(es)
    -- Table                             Natural     Index
    -- ****************************************************
    -- PERF_LOG                                          1

    if ( rdb$get_context('USER_SESSION','PERF_WATCH_BEG')is not null ) then -- see SP_CHECK_TO_STOP_WORK
        v_last_recalc_idx_dts = maxvalue(v_last_recalc_idx_dts, cast(rdb$get_context('USER_SESSION','PERF_WATCH_BEG') as timestamp) );

    v_last_recalc_idx_minutes_ago = coalesce( datediff(minute from v_last_recalc_idx_dts to cast('now' as timestamp)), cast( rdb$get_context('USER_SESSION', 'RECALC_IDX_MIN_INTERVAL') as int ) );

    r_max = rdb$get_context('USER_SESSION', 'BOP_RND_MAX');
    if ( r_max is null ) then
    begin
        select max( b.random_selection_weight ) from business_ops b into r_max;
        rdb$set_context('USER_SESSION', 'BOP_RND_MAX', r_max);
    end

    r = rand() * r_max;
    delete from tmp$perf_log p where p.stack = :v_this;

    insert into tmp$perf_log(unit, aux1, aux2, stack)
    select o.unit, o.sort_prior, o.random_selection_weight, :v_this
    from business_ops o
    where o.random_selection_weight >= :r
        and (fn_internal_enable_mon_query() = 1 or o.unit <> :c_unit_for_mon_query)
        and ( -- 27.11.2015: skip call of index statistics recalc if it was done not so far:
                o.unit <> 'srv_recalc_idx_stat'
                or
                o.unit = 'srv_recalc_idx_stat'
                and
                    :v_last_recalc_idx_minutes_ago
                    >=
                    cast( rdb$get_context('USER_SESSION', 'RECALC_IDX_MIN_INTERVAL') as int )
            )
        and (:a_included_modes = '' or :a_included_modes||',' containing trim(o.mode)||',' )
        and (:a_included_kinds = '' or :a_included_kinds||',' containing trim(o.kind)||',' )
        and (:a_excluded_modes = '' or :a_excluded_modes||',' NOT containing trim(o.mode)||',' )
        and (:a_excluded_kinds = '' or :a_excluded_kinds||',' NOT containing trim(o.kind)||',' )
    ;
    c = row_count;
    n = cast( 0.5+rand()*(c+0.5) as int );
    n = minvalue(maxvalue(1, n),c);

    select p.unit, p.aux1, p.aux2
    from tmp$perf_log p
    where p.aux2 >= :r
    order by rand()
    rows :n to :n -- get SINGLE row!
    into unit, sort_prior, rnd_weight;

    delete from tmp$perf_log p where p.stack = :v_this; -- 18.08.2014! cleanup this temply created data!

    suspend;

end


^ -- srv_random_unit_choice

---------------------------------------------------------------------------

create or alter procedure srv_diag_pay_distr( -- ::: NB ::: 3.0 only!
    a_doc_id dm_idb default null
) returns(
    result varchar(3), -- 'ok.' | 'err'
    ptab varchar(8),
    ptab_id int,
    snd_oper dm_mcode,
    snd_id dm_idb,
    rcv_oper dm_mcode,
    rcv_id dm_idb,
    ptab_cost dm_cost,
    ptab_sum_cost dm_cost,
    payment_id dm_idb, 
    payment_cost dm_cost
)
as
begin
    for
        select
             iif( iif( h.optype_id=fn_oper_pay_from_customer(), h.cost_retail, h.cost_retail) = sum(ps.cost)over( partition by iif(ps.ptab='pdistr',ps.snd_optype_id,ps.rcv_optype_id)), 'ok.','err' ) result
            ,cast(ps.ptab as varchar(8)) as ptab
            ,ps.id as ptab_id
            ,ps.snd_oper
            ,ps.snd_id as ptab_snd_id
            ,ps.rcv_oper
            ,ps.rcv_id as ptab_rcv_id
            ,ps.cost as ptab_cost
            ,sum(ps.cost)over( partition by iif(ps.ptab='pdistr',ps.snd_optype_id,ps.rcv_optype_id)) ptab_sum_cost
            --,ps.trn_id as ptab_trn
            ,h.id as payment_id
            ,iif( h.optype_id=fn_oper_pay_from_customer(), h.cost_retail, h.cost_retail) as payment_cost
        from (
            select
                'pdistr' ptab,
                pd.trn_id,
                pd.snd_optype_id,
                so.mcode as snd_oper,
                pd.rcv_optype_id,
                ro.mcode as rcv_oper,
                pd.id,
                pd.snd_id,
                pd.snd_cost cost, -- NOT YET storned value
                cast(null as int) as rcv_id
            from pdistr pd
            --join doc_list h on pd.snd_id = h.id and pd.snd_optype_id = h.optype_id
            join optypes so on pd.snd_optype_id=so.id
            join optypes ro on pd.rcv_optype_id=ro.id
            where (pd.snd_id = :a_doc_id or :a_doc_id is null) -- OR'ed optimization, 3.0 only

            UNION ALL

            select
                'pstorned',
                ps.trn_id,
                ps.snd_optype_id,
                so.mcode as snd_oper,
                ps.rcv_optype_id,
                ro.mcode as rcv_oper,
                ps.id,
                ps.snd_id,
                ps.rcv_cost, -- ALREADY STORNED value
                ps.rcv_id
            from pstorned ps
            --join doc_list h on ps.rcv_id = h.id and ps.rcv_optype_id = h.optype_id
            join optypes so on ps.snd_optype_id=so.id
            join optypes ro on ps.rcv_optype_id=ro.id
            where (ps.rcv_id = :a_doc_id or :a_doc_id is null) -- OR'ed optimization, 3.0 only
        ) ps
        join doc_list h on h.id = iif(ps.ptab='pdistr',ps.snd_id,ps.rcv_id)
    into
        result
        ,ptab
        ,ptab_id
        ,snd_oper
        ,snd_id
        ,rcv_oper
        ,rcv_id
        ,ptab_cost
        ,ptab_sum_cost
        ,payment_id
        ,payment_cost
    do suspend;
end

^ -- srv_diag_pay_distr

create or alter procedure sys_list_to_rows (
    A_LST blob sub_type 1 segment size 80,
    A_DEL char(1) = ',')
returns (
    LINE integer,
    EOF integer,
    ITEM varchar(8192))
AS
  declare pos_ int;
  declare noffset int = 1;
  declare beg int;
  declare buf varchar(30100);
begin
  -- Splits blob to lines by single char delimiter.
  -- adapted from here:
  -- http://www.sql.ru/forum/actualthread.aspx?bid=2&tid=607154&pg=2#6686267

  if (a_lst is null) then exit;
  line=0;

  while (0=0) do begin
    buf = substring(a_lst from noffset for 30100);
    pos_ = 1; beg = 1;
    while (pos_ <= char_length(buf) and pos_ <= 30000) do begin
      if (substring(buf from pos_ for 1) = :a_del) then begin
        if (pos_ > beg) then
          item = substring(buf from beg for pos_ - beg);
        else
          item = ''; --null;
        suspend;
        line=line+1;
        beg = pos_ + 1;
      end
      pos_ = pos_ + 1;
    end
    if (noffset + pos_ - 2 = char_length(a_lst)) then leave;
    noffset = noffset + beg - 1;
    if (noffset > char_length(a_lst)) then leave;
  end

  if (pos_ > beg) then begin
    item = substring(buf from beg for pos_ - beg);
    eof=-1;
  end
  else begin
    item = '';
    eof=-1;
  end
  suspend;

end

^  -- sys_list_to_rows

create or alter procedure sys_get_proc_ddl (
    a_proc varchar(31),
    a_mode smallint = 1,
    a_include_setterm smallint = 1)
returns (
    src varchar(32760))
as
begin
    if ( a_proc is null or
         not singular(select * from rdb$procedures p where p.rdb$procedure_name starting with upper(:a_proc))
       ) then
    begin
        src = '-- invalid input argument a_proc = ' || coalesce('"'||trim(a_proc)||'"', '<null>');
        suspend;
        exception ex_bad_argument using( coalesce('"'||trim(a_proc)||'"', '<null>'), 'sys_get_proc_ddl' );
    end

    for
        -- Extracts metadata of STORED PROCSEDURES to be executed as statements in isql.
        -- Samples:
        -- select src from sys_get_proc_ddl('', 0) -- output all procs with EMPTY body (preparing to update)
        -- select src from sys_get_proc_ddl('', 1) -- output all procs with ODIGIN body (finalizing update)
        
        with
        s as(
            select
                m.mon$sql_dialect db_dialect
                ,:a_mode mode -- -1=only SP name and its parameters, 0 = name+parameters+empty body, 1=full text
                ,:a_include_setterm add_set_term -- 1 => include `set term ^;` clause
                ,r.rdb$character_set_name db_default_cset
                ,p.rdb$procedure_name p_nam
                ,ascii_char(10) d
                ,replace(cast(p.rdb$procedure_source as blob sub_type 1), ascii_char(13), '') p_src
                ,(
                    select
                        coalesce(sum(iif(px.rdb$parameter_type=0,1,0))*1000 + sum(iif(px.rdb$parameter_type=1,1,0)),0)
                    from rdb$procedure_parameters px
                    where px.rdb$procedure_name = p.rdb$procedure_name
                ) pq -- cast(pq/1000 as int) = qty of IN-args, mod(pq,1000) = qty of OUT args
            from mon$database m -- put it FIRST in the list of sources!
            join rdb$database r on 1=1
            join rdb$procedures p on 1=1
            where p.rdb$procedure_name starting with upper(:a_proc) -- substitute with some name if needed
        )
        --select * from s
        ,r as(
            select
                db_dialect
                ,mode
                ,add_set_term
                ,db_default_cset
                ,p_nam
                ,p.line as i
                ,p.item as word
                ,d
                ,pq
                ,p_src
                ,cast(pq/1000 as int) pq_in
                ,mod(pq,1000) pq_ou
                ,p.eof k
            from s
            left join sys_list_to_rows(p_src, d) p on 1=1
        )
        --select * from r
        
        ,p as(
            select
                db_dialect
                ,mode
                ,add_set_term
                ,db_default_cset
                ,p_nam,i
                ,word
                ,r.pq_in
                ,r.pq_ou
                ,pt -- ip=0, op=1
                ,pp.rdb$field_source ppar_fld_src
                ,pp.rdb$parameter_name par_name
                ,pp.rdb$parameter_number par_num
                ,pp.rdb$parameter_type par_ty
                ,pp.rdb$null_flag p_not_null -- 1==> not null
                ,pp.rdb$parameter_mechanism ppar_mechan -- 1=type of (table.column, domain, other...)
                ,pp.rdb$relation_name ppar_rel_name
                ,pp.rdb$field_name par_fld
                ,case f.rdb$field_type
                    when 7 then 'smallint'
                    when 8 then 'integer'
                    when 10 then 'float'
                    --when 14 then 'char(' || cast(cast(f.rdb$field_length / iif(ce.rdb$character_set_name=upper('utf8'),4,1) as int) as varchar(5)) || ')'
                    when 14 then 'char(' || cast(cast(f.rdb$field_length / ce.rdb$bytes_per_character as int) as varchar(5)) || ')'
                    when 16 then -- dialect 3 only
                        case f.rdb$field_sub_type
                            when 0 then 'bigint'
                            when 1 then 'numeric(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                            when 2 then 'decimal(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                            else 'unknown'
                        end
                    when 12 then 'date'
                    when 13 then 'time'
                    when 27 then -- dialect 1 only
                        case f.rdb$field_scale
                            when 0 then 'double precision'
                            else 'numeric(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                        end
                    when 35 then iif(db_dialect=1, 'date', 'timestamp')
                    when 37 then 'varchar(' || cast(cast(f.rdb$field_length / ce.rdb$bytes_per_character as int) as varchar(5)) || ')'
                    when 261 then 'blob sub_type ' || f.rdb$field_sub_type || ' segment size ' || f.rdb$segment_length
                    else 'unknown'
                end
                as fddl
                ,f.rdb$character_set_id fld_source_cset_id
                ,f.rdb$collation_id fld_coll_id
                ,ce.rdb$character_set_name fld_src_cset_name
                ,co.rdb$collation_name fld_collation
                ,cast(f.rdb$default_source as varchar(1024)) fld_default_src
                ,cast(pp.rdb$default_source as varchar(1024)) ppar_default_src   -- ppar_default_src
                ,k -- k=-1 ==> last line of sp
            from r
            join (
                select -2 pt from rdb$database -- 'set term ^;'
                union all select -1 from rdb$database -- header stmt: 'create or alter procedure ...('
                union all select  0 from rdb$database -- input pars
                union all select  5 from rdb$database -- 'returns ('
                union all select 10 from rdb$database -- output pars
                union all select 20 from rdb$database -- 'as'
                union all select 50 from rdb$database -- source code
                union all select 100 from rdb$database -- '^set term ;^'
            ) x on
                -- `i`=line of body, 0='begin'
                i = 0 and x.pt = -1 -- header
                or i =0 and x.pt = 0 and pq_in > 0 -- input args, if exists
                or i =0 and x.pt in(5,10) and pq_ou > 0 -- output args, if exists ('returns(' line)
                or i =0 and x.pt = 20 -- 'AS'
                or pt = 50
                or i = 0 and x.pt in(-2, 100) and add_set_term = 1 -- 'set term ^;', final '^set term ;^'
            left join rdb$procedure_parameters pp on
                r.p_nam = pp.rdb$procedure_name
                and (x.pt = 0 and pp.rdb$parameter_type = 0 or x.pt = 10 and pp.rdb$parameter_type = 1)
            --x.pt=pp.rdb$parameter_type-- =0 => in, 1=out
            left join rdb$fields f on
                pp.rdb$field_source = f.rdb$field_name
            left join rdb$collations co on
                f.rdb$character_set_id = co.rdb$character_set_id
                and f.rdb$collation_id = co.rdb$collation_id
            left join rdb$character_sets ce on
                co.rdb$character_set_id = ce.rdb$character_set_id
        )
        --select * from p
        
        ,fin as(
            select
                db_dialect
                ,mode
                ,add_set_term
                ,db_default_cset
                ,p_nam
                ,i
                ,par_num
                ,case
                 when pt=-2 then 'set term ^;'
                 when pt=100 then '^set term ;^'
                 when pt=-1 then 'create or alter procedure '||trim(p_nam)||iif(pq_in>0,' (','')
                 when pt=5 then 'returns ('
                 when pt=20 then 'AS'
                 when pt in(0,10) then --in or out argument definition
                     '    '
                     ||trim(par_name)||' '
                     ||lower(trim( iif(nullif(p.ppar_mechan,0) is null, -- ==> parameter is defined with direct reference to base type, NOT like 'type of ...'
                                       iif(ppar_fld_src starting with 'RDB$', p.fddl, ppar_fld_src),
                                       ' type of '||coalesce('column '||trim(ppar_rel_name)||'.'||trim(par_fld), ppar_fld_src)
                                      )
                                 )
                            ) -- parameter type
                     ||iif(nullif(p.ppar_mechan,0) is not null -- parameter is defined as: "type of column|domain"
                           or
                           ppar_fld_src NOT starting with 'RDB$' -- parameter is defined by DOMAIN: "a_id dm_idb"
                           or
                           nullif(p.fld_src_cset_name,upper('NONE')) is null -- field of non-text type or charset was not specified
                           --coalesce(p.fld_src_cset_name, p.db_default_cset) is not distinct from p.db_default_cset
                           ,trim(trailing from iif(p.p_not_null=1, ' not null', ''))
                           ,' character set '||trim(p.fld_src_cset_name)
                             ||trim(trailing from iif(p.p_not_null=1, ' not null', ''))
                             ||iif(p.fld_collation is distinct from p.fld_src_cset_name, ' collate '||trim(p.fld_collation), '')
                          )
                     ||coalesce(
                          ' '||trim(
                               iif( ppar_fld_src starting with upper('RDB$'), ----- adding "default ..." clause
                                    coalesce(ppar_default_src, fld_default_src), -- this is only for 2.5; on 3.0 default values always are stored in ppar_default_src
                                    ppar_default_src
                                  )
                              )
                        ,'')
                     ||iif(pt=0 and par_num=pq_in-1 or pt=10 and par_num=pq_ou-1,')',',')
                  when k=-1 then coalesce(nullif(word,'')||';','') -- nb: some sp can finish with empty line!
                  else word
                end word
                ,pt
                ,ppar_fld_src
                ,par_name
                ,par_ty
                ,pq_in
                ,pq_ou
                --,f.rdb$field_type ftyp ,f.rdb$field_length flen,f.rdb$field_scale fdec
                ,p.fddl
                ,p.fld_src_cset_name
                ,p.fld_collation
                ,k
                --,'#'l,f.*
            from p
            left join rdb$fields f on p.ppar_fld_src = f.rdb$field_name
        )
        --select * from fin order by p_nam,pt,par_num,i
        
        select --mode,p_nam,
            cast(
            case
             when mode<=0 then
               case when pt <50 /*is not null*/ then word
                    when pt in(-2, 100) and add_set_term = 1 then word
                    when mode = 0 and i = 0 and pt < 100 then ' begin'||iif(k = -1, ' end','')
                    when mode = 0 and i = 1 then iif(pq_ou>0, '  suspend;', '  exit;')
                    when mode = 0 and k = -1 then 'end;' -- last line of body
               end
             else word
             end
            as varchar(8192)) -- blob can incorrectly displays (?)
             as src
        --,f.* -- do not! implementation exceeded
        from fin f
        where mode<=0 and (i in(0,1) or k=-1 /*or pt in(-2, 100) and strm=1*/ ) or mode=1
        order by p_nam,pt,par_num,i
        into src
    do
        suspend;

end 

^ -- sys_get_proc_ddl

create or alter procedure sys_get_func_ddl (
    a_func varchar(31),
    a_mode smallint = 1,
    a_include_setterm smallint = 1)
returns (
    src varchar(32760))
as
begin
    for
        -- Extracts metadata of STORED PROCSEDURES to be executed as statements in isql.
        -- Samples:
        -- select src from sys_get_proc_ddl('', 0) -- output all procs with EMPTY body (preparing to update)
        -- select src from sys_get_proc_ddl('', 1) -- output all procs with ODIGIN body (finalizing update)
        with
        s as(

            select
                m.mon$sql_dialect db_dialect
                ,:a_mode mode -- -1=only SP name and its parameters, 0 = name+parameters+empty body, 1=full text
                ,:a_include_setterm add_set_term -- 1 => include `set term ^;` clause
                ,r.rdb$character_set_name db_default_cset
                ,p.rdb$function_name p_nam
                ,coalesce(p.rdb$deterministic_flag,0) p_det
                ,ascii_char(10) d
                ,replace(cast(p.rdb$function_source as blob sub_type 1), ascii_char(13), '') p_src
                ,(
                    select
                        --coalesce(sum(iif(px.rdb$parameter_type=0,1,0))*1000 + sum(iif(px.rdb$parameter_type=1,1,0)),0)
                        1000 * count( px.rdb$argument_position ) + 1
                    from rdb$function_arguments px
                    where px.rdb$function_name = p.rdb$function_name
                    and px.rdb$argument_position > 0 -- take in account only INPUT args, because OUT always = 1
                ) pq -- cast(pq/1000 as int) = qty of IN-args, mod(pq,1000) = qty of OUT args
            from mon$database m -- put it FIRST in the list of sources!
            join rdb$database r on 1=1
            join rdb$functions p on 1=1
            where p.rdb$function_name starting with upper(:a_func) -- substitute with some name if needed
            --upper( 'fn$get$random$id$subst$names' )-- upper(:a_proc) -- substitute with some name if needed

        )
        --select * from s
        ,r as(
            select
                db_dialect
                ,mode
                ,add_set_term
                ,db_default_cset
                ,p_nam
                ,p.line as i
                ,p.item as word
                ,d
                ,pq
                ,p_src
                ,cast(pq/1000 as int) pq_in
                ,mod(pq,1000) pq_ou
                ,s.p_det
                ,p.eof k
            from s
            left join sys_list_to_rows(p_src, d) p on 1=1
        )
        --        select * from r
        
        ,p as(
            select
                db_dialect
                ,mode
                ,add_set_term
                ,db_default_cset
                ,p_nam,i
                ,word
                ,r.pq_in
                ,r.pq_ou
                ,r.p_det
                ,pt -- ip=0, op=1
                ,pp.rdb$field_source ppar_fld_src
                ,pp.rdb$argument_name par_name
                ,pp.rdb$argument_position par_num
                -- ?! ,pp.rdb$parameter_type par_ty
                ,pp.rdb$null_flag p_not_null -- 1==> not null
                ,pp.rdb$argument_mechanism ppar_mechan -- 1=type of (table.column, domain, other...)
                ,pp.rdb$relation_name ppar_rel_name
                ,pp.rdb$field_name par_fld
                ,case f.rdb$field_type
                    when 7 then 'smallint'
                    when 8 then 'integer'
                    when 10 then 'float'
                    --when 14 then 'char(' || cast(cast(f.rdb$field_length / iif(ce.rdb$character_set_name=upper('utf8'),4,1) as int) as varchar(5)) || ')'
                    when 14 then 'char(' || cast(cast(f.rdb$field_length / ce.rdb$bytes_per_character as int) as varchar(5)) || ')'
                    when 16 then -- dialect 3 only
                        case f.rdb$field_sub_type
                            when 0 then 'bigint'
                            when 1 then 'numeric(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                            when 2 then 'decimal(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                            else 'unknown'
                        end
                    when 12 then 'date'
                    when 13 then 'time'
                    when 27 then -- dialect 1 only
                        case f.rdb$field_scale
                            when 0 then 'double precision'
                            else 'numeric(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                        end
                    when 35 then iif(db_dialect=1, 'date', 'timestamp')
                    when 37 then 'varchar(' || cast(cast(f.rdb$field_length / ce.rdb$bytes_per_character as int) as varchar(5)) || ')'
                    when 261 then 'blob sub_type ' || f.rdb$field_sub_type || ' segment size ' || f.rdb$segment_length
                    else 'unknown'
                end
                as fddl
                ,f.rdb$character_set_id fld_source_cset_id
                ,f.rdb$collation_id fld_coll_id
                ,ce.rdb$character_set_name fld_src_cset_name
                ,co.rdb$collation_name fld_collation
                ,cast(f.rdb$default_source as varchar(1024)) fld_default_src
                ,cast(pp.rdb$default_source as varchar(1024)) ppar_default_src   -- ppar_default_src
                ,k -- k=-1 ==> last line of sp
            from r
            join (
                select -2 pt from rdb$database -- 'set term ^;'
                union all select -1 from rdb$database -- header stmt: 'create or alter procedure ...('
                union all select  0 from rdb$database -- input pars
                union all select  5 from rdb$database -- 'returns <...> [deterministic]'
                union all select 10 from rdb$database -- output pars
                union all select 20 from rdb$database -- 'as'
                union all select 50 from rdb$database -- source code
                union all select 100 from rdb$database -- '^set term ;^'
            ) x on
                -- `i`=line of body, 0='begin'
                i = 0 and x.pt = -1 -- header
                or i =0 and x.pt = 0 and pq_in > 0 -- input args, if exists
                or i =0 and x.pt in(5,10) and pq_ou > 0 -- output argument, always single ( for line: "returns ..." )
                or i =0 and x.pt = 20 -- 'AS'
                or pt = 50
                or i = 0 and x.pt in(-2, 100) and add_set_term = 1 -- 'set term ^;', final '^set term ;^'
            left join rdb$function_arguments pp on -- rdb$procedure_parameters pp on
                r.p_nam = pp.rdb$function_name --  rdb$procedure_name
                and ( x.pt = 0 and pp.rdb$argument_position >= 1 -- input arg
                      or
                      x.pt = 10 and pp.rdb$argument_position = 0 -- single output arg
                    )
            --x.pt=pp.rdb$parameter_type-- =0 => in, 1=out
            left join rdb$fields f on
                pp.rdb$field_source = f.rdb$field_name
            left join rdb$collations co on
                f.rdb$character_set_id = co.rdb$character_set_id
                and f.rdb$collation_id = co.rdb$collation_id
            left join rdb$character_sets ce on
                co.rdb$character_set_id = ce.rdb$character_set_id
        )
        --select * from p
        
        ,fin as(
            select
                db_dialect
                ,mode
                ,add_set_term
                ,db_default_cset
                ,p.ppar_mechan
                ,p_nam
                ,i
                ,par_num
                ,p_det
                ,case
                 when pt=-2 then 'set term ^;'
                 when pt=100 then '^set term ;^'
                 when pt=-1 then 'create or alter function '||trim(p_nam)||iif(pq_in>0,' (','')
                 when pt=5 then 'returns '
                 when pt=20 then 'AS'
                 when pt in(0,10) then --in or out argument definition
                     '    '
                     ||trim( coalesce( lower(par_name), '') )||' '
                     ||lower(trim( iif(nullif(p.ppar_mechan,0) is null, -- ==> parameter is defined with direct reference to base type, NOT like 'type of ...'
                                       iif(ppar_fld_src starting with 'RDB$', p.fddl, ppar_fld_src),
                                       ' type of '||coalesce('column '||trim(ppar_rel_name)||'.'||trim(par_fld), ppar_fld_src)
                                      )
                                 )
                            ) -- parameter type
                     ||iif(nullif(p.ppar_mechan,0) is not null -- parameter is defined as: "type of column|domain"
                           or
                           ppar_fld_src NOT starting with 'RDB$' -- parameter is defined by DOMAIN: "a_id dm_idb"
                           or
                           nullif(p.fld_src_cset_name,upper('NONE')) is null -- field of non-text type or charset was not specified
                           --coalesce(p.fld_src_cset_name, p.db_default_cset) is not distinct from p.db_default_cset
                           ,trim(trailing from iif(p.p_not_null=1, ' not null', ''))
                           ,' character set '||trim(p.fld_src_cset_name)
                             ||trim(trailing from iif(p.p_not_null=1, ' not null', ''))
                             ||iif(p.fld_collation is distinct from p.fld_src_cset_name, ' collate '||trim(p.fld_collation), '')
                          )
                     ||coalesce(
                          ' '||trim(
                               iif( ppar_fld_src starting with upper('RDB$'), ----- adding "default ..." clause
                                    coalesce(ppar_default_src, fld_default_src), -- this is only for 2.5; on 3.0 default values always are stored in ppar_default_src
                                    ppar_default_src
                                  )
                              )
                        ,'')
                     ||trim(trailing from iif( pt=0, iif( par_num = pq_in, ')', ','), iif( p_det = 1, ' deterministic', '') ) )
                  when k=-1 then coalesce(nullif(word,'')||';','') -- nb: some sp can finish with empty line!
                  else word
                end word
                ,pt
                ,ppar_fld_src
                ,par_name
                --,par_ty
                ,pq_in
                ,pq_ou
                --,f.rdb$field_type ftyp ,f.rdb$field_length flen,f.rdb$field_scale fdec
                ,p.fddl
                ,p.fld_src_cset_name
                ,p.fld_collation
                ,k
                --,'#'l,f.*
            from p
            left join rdb$fields f on p.ppar_fld_src = f.rdb$field_name
        )
        --  select * from fin order by p_nam,pt,par_num,i
        
        select --mode,p_nam,
            cast(
            case
             when mode<=0 then
               case when pt <50 /*is not null*/ then word
                    when pt in(-2, 100) and add_set_term = 1 then word
                    when mode = 0 and i = 0 and pt < 100 then ' begin'||iif(k = -1, ' end','')
                    when mode = 0 and i = 1 then 'return null;'
                    when mode = 0 and k = -1 then 'end' || iif(add_set_term = 1, '', ';') -- last line of body
               end
             else word
             end
            as varchar(8190)) -- varchar(8192)) -- blob can incorrectly displays (?)
             as src
        --,f.* -- do not! implementation exceeded
        from fin f
        where mode<=0 and (i in(0,1) or k=-1 /*or pt in(-2, 100) and strm=1*/ ) or mode=1
        order by p_nam,pt,par_num,i        
        into src
    do
        suspend;

end 

^ -- sys_get_func_ddl

create or alter procedure sys_get_view_ddl (
    A_VIEW varchar(31) = '',
    A_MODE smallint = 1)
returns (
    SRC varchar(8192))
AS
begin
    -- Extracts metadata of VIEWS to be executed as statements in isql.
    -- Samples:
    -- select src from sys_get_view_ddl('', 0) -- output all views with EMPTY body (preparing to update)
    -- select src from sys_get_view_ddl('', 1) -- output all views with ODIGIN body (finalizing update)
    
    for
        with
        inp as(select :a_view a_view, :a_mode mode from rdb$database)
        ,s as(
            select
                m.mon$sql_dialect di
                ,i.mode mode -- 1=> fill
                ,1 strm -- 1 => include `set term ^;` clause
                ,r.rdb$character_set_name cs
                ,p.rdb$relation_name v_name
                ,(select count(*) from rdb$relation_fields rf where p.rdb$relation_name=rf.rdb$relation_name) fq -- count of fields
                ,ascii_char(10) d
                ,replace(cast(p.rdb$view_source as blob sub_type 1), ascii_char(13), '') ||ascii_char(10)||';' p_src
            from mon$database m -- put it FIRST in the list of sources!
            join rdb$database r on 1=1
            join rdb$relations p on 1=1
            join inp i on 1=1
            where coalesce(p.rdb$system_flag,0) = 0
            and p.rdb$view_source is not null -- views; do NOT: p.rdb$relation_type=1 !!
            and  (i.a_view='' or p.rdb$relation_name = upper(i.a_view))
        )
        ,r as(
            select --* --s.*,'#'l,p.*,rf.*
                di
                ,mode
                ,strm
                ,cs
                ,v_name
                ,fq
                ,p.item word
                ,p.line i
                ,p.eof k
                ,rf.rdb$field_position fld_pos
                ,x.rt
                ,rf.rdb$field_name v_fld
                ,rf.rdb$field_source v_src
                ,case f.rdb$field_type
                    when 7 then 'smallint'
                    when 8 then 'integer'
                    when 10 then 'float'
                    when 14 then 'char(' || cast(cast(f.rdb$field_length / ce.rdb$bytes_per_character as int) as varchar(5)) || ')'
                    when 16 then -- dialect 3 only
                    case f.rdb$field_sub_type
                        when 0 then 'bigint'
                        when 1 then 'numeric(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                        when 2 then 'decimal(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                        else 'unknown'
                    end
                    when 12 then 'date'
                    when 13 then 'time'
                    when 27 then -- dialect 1 only
                    case f.rdb$field_scale
                        when 0 then 'double precision'
                        else 'numeric(15,' || cast(-f.rdb$field_scale as varchar(6)) || ')'
                    end
                    when 35 then iif(di=1, 'date', 'timestamp')
                    when 37 then 'varchar(' || cast(cast(f.rdb$field_length / ce.rdb$bytes_per_character as int) as varchar(5)) || ')'
                    when 261 then 'blob sub_type ' || f.rdb$field_sub_type || ' segment size ' || f.rdb$segment_length
                    else 'unknown'
                end fddl
                ,f.rdb$character_set_id fld_cset
                ,f.rdb$collation_id fld_coll_id
                ,ce.rdb$character_set_name cset_name
                ,co.rdb$collation_name fld_collation
            from s
            left join sys_list_to_rows(p_src, d) p
                on p.line=0 or mode=1
            left join
            (
                select -2 rt from rdb$database -- create or alter view
                union all select -1 from rdb$database -- for fields of view
                union all select 0 from rdb$database -- body
                union all select 99 from rdb$database -- final ';'
            ) x on
                p.line=0 and x.rt in(-2,-1,99) or x.rt=0 and mode=1
            left join rdb$relation_fields rf on -- fields of the view
                ( /*mode=0 or mode=1 and*/ p.line=0 and x.rt=-1)
                and s.v_name=rf.rdb$relation_name
            left join rdb$fields f on
                rf.rdb$field_source=f.rdb$field_name
            left join rdb$collations co on
                f.rdb$character_set_id=co.rdb$character_set_id
                and f.rdb$collation_id=co.rdb$collation_id
            left join rdb$character_sets ce on
                co.rdb$character_set_id=ce.rdb$character_set_id
        )
        --select * from r order by v_name,rt,i,fld_pos
        
        ,fin as(
            select
                di
                ,mode
                ,strm
                ,cs
                ,v_name
                ,fq
                ,case
                    when 1=1 or mode=0 then
                        case
                            when rt=-2 then
                                'create or alter view '||trim(v_name)||iif( mode=0, ' as select',' (' )
                            when mode=1 and rt=-1 then -- rt=-1: fields of view
                                trim(v_fld)||trim(iif(fld_pos+1=fq,') as',','))
                            when mode=0 and rt=-1 then
                                iif(fld_pos=0, '', ', ')||
                                case when
                                        lower(fddl) in ('smallint','integer','bigint','double precision','float')
                                        or lower(fddl) starting with 'numeric'
                                        or lower(fddl) starting with 'decimal'
                                    then '0 as '||v_fld
                                    when
                                        lower(fddl) starting with 'varchar'
                                        or lower(fddl) starting with 'char'
                                        or lower(fddl) starting with 'blob'
                                    then ''''' as '||v_fld
                                    when
                                        lower(fddl) in ('timestamp','date')
                                    then 'cast(''now'' as '||lower(fddl)||') as '||v_fld
                                end
                            when rt=0 then word
                            when rt=99 then iif(mode=0,'from rdb$database;',';') -- final row
                        end
                    when mode=1 then
                        case
                            when rt=-1 then 'create or alter view '||trim(v_name)||' as '
                            else word||iif(k=-1 and right(word,1)<>';', ';','')
                        end
                 end
                 as word
                ,i
                ,k
                ,rt
                ,v_fld
                ,v_src
                ,fld_pos
                ,fddl
                ,fld_cset
                ,fld_coll_id
                ,cset_name
                ,fld_collation
            from r
            where word not in(';')
        )
        select word
        from fin
        order by v_name, rt, i,fld_pos
        into src
    do
        suspend;

end  

^ -- sys_get_view_ddl

create or alter procedure sys_get_indx_ddl(
    a_relname varchar(31) = '')
returns (
    src varchar(8192))
as
begin
  -- extract DDLs of all indices EXCEPT those which are participated
  -- in PRIMARY KEYS
  for
    with
    inp as(select :a_relname nm from rdb$database)
    ,pk_defs as( -- determine primary keys
      select
        rc.rdb$relation_name rel_name
        ,rc.rdb$constraint_name pk_name
        ,rc.rdb$index_name pk_idx
      from rdb$relation_constraints rc
      where rc.rdb$constraint_type containing 'PRIMARY'
    )
    --select * from pk_defs
    
    ,ix_defs as(
      select
       ri.rdb$relation_name rel_name
      ,rc.rdb$constraint_name con_name
      ,rc.rdb$constraint_type con_type
      ,ri.rdb$index_id idx_id
      ,ri.rdb$index_name idx_name
      ,ri.rdb$unique_flag unq
      ,ri.rdb$index_type des
      ,ri.rdb$foreign_key fk
      ,ri.rdb$system_flag sy
      ,rs.rdb$field_name fld
      ,rs.rdb$field_position pos
      ,ri.rdb$expression_source ix_expr
      ,p.pk_idx
      from rdb$indices  ri
      join inp i on (ri.rdb$relation_name = upper(i.nm) or i.nm='')
      left join rdb$relation_constraints rc on ri.rdb$index_name=rc.rdb$index_name
      left join pk_defs p on ri.rdb$relation_name=p.rel_name and ri.rdb$index_name=p.pk_idx
      left join rdb$index_segments rs on ri.rdb$index_name=rs.rdb$index_name
      where
      ri.rdb$system_flag=0
      and p.pk_idx is null -- => this index is not participated in PK
      order by rel_name,idx_id, pos
    )
    --select * from ix_defs
    ,ix_grp as(
      select rel_name,con_name,con_type,idx_id,idx_name,unq,des,fk,ix_key,ix_expr
      ,r.rdb$relation_name parent_rname
      ,r.rdb$constraint_name parent_cname
      ,r.rdb$constraint_type parent_ctype
      ,iif(r.rdb$constraint_type='PRIMARY KEY'
      ,(select cast(list(trim(pk_fld),',') as varchar(8192)) from
        (select rs.rdb$field_name pk_fld
           from rdb$index_segments rs
          where rs.rdb$index_name=t.fk
          order by rs.rdb$field_position
        )u
       )
       ,null) parent_pkey
      from(
        select rel_name,con_name,con_type,idx_id,idx_name,unq,des,fk
              ,cast(list(trim(fld),',') as varchar(8192)) ix_key
              ,cast(ix_expr as varchar(8192)) ix_expr
        from ix_defs
        group by rel_name,con_name,con_type,idx_id,idx_name,unq,des,fk,ix_expr
      )t
      left join rdb$relation_constraints r on t.fk=r.rdb$index_name
    )
    --select * from ix_grp

    ,fin as(
    select
      rel_name,con_name,con_type,idx_id,idx_name,unq,des,fk
      ,parent_rname,parent_cname,parent_ctype,parent_pkey
      ,case
        when con_type='UNIQUE' then
            'alter table '
            ||trim(rel_name)
            ||' add '||trim(con_type)
            ||'('||trim(ix_key)||')'
            ||iif(idx_name like 'RDB$%', '', ' using index '||trim(idx_name))
            ||';'
        when con_type='FOREIGN KEY' and con_name like 'INTEG%' then
            'alter table '
            ||trim(rel_name)
            ||' add '||trim(con_type)
            ||'('||trim(ix_key)||') references '
            ||trim(parent_rname)
            ||'('||trim(coalesce(parent_pkey,ix_key)) ||')'
            ||iif(idx_name like 'RDB$FOREIGN%', '', ' using index '||trim(idx_name))
            ||';'
        when con_type='FOREIGN KEY' then
            'alter table '
            ||trim(rel_name)
            ||' add constraint '||trim(con_name)||' '||trim(con_type)
            ||'('||trim(ix_key)||') references '
            ||trim(parent_rname)||'('||trim(parent_pkey)||')'
            ||' using index '||trim(idx_name)
            ||';'
       end ct_ddl
      ,'create '
      ||trim(iif(unq=1,' unique','')
      ||iif(des=1,' descending',''))
      ||' index '||trim(idx_name)
      ||' on '||trim(rel_name)
      ||' '||iif(ix_expr is null,'('||trim(ix_key)||')', 'computed by ('||trim(ix_expr)||')' )
      ||';' ix_ddl
    from ix_grp
    )
    select coalesce(ct_ddl, ix_ddl) idx_ddl --, f.*
    from fin f
  into
    src
  do
    suspend;
end 

^ -- sys_get_indx_ddl

create or alter procedure sys_get_fb_arch (
     a_connect_with_usr varchar(31) default 'SYSDBA'
    ,a_connect_with_pwd varchar(31) default 'masterkey'
) returns(
    fb_arch varchar(50)
) as
    declare cur_server_pid int;
    declare ext_server_pid int;
    declare att_protocol varchar(255);
    declare v_test_sttm varchar(255);
    declare v_fetches_beg bigint;
    declare v_fetches_end bigint;
begin
    
    -- Aux SP for detect FB architecture.

    select a.mon$server_pid, a.mon$remote_protocol
    from mon$attachments a
    where a.mon$attachment_id = current_connection
    into cur_server_pid, att_protocol;

    if ( att_protocol is null ) then
        fb_arch = 'Embedded';
    else if ( upper(current_user) = upper('SYSDBA')
              and rdb$get_context('SYSTEM','ENGINE_VERSION') NOT starting with '2.5' 
              and exists(select * from mon$attachments a 
                         where a.mon$remote_protocol is null
                               and upper(a.mon$user) in ( upper('Cache Writer'), upper('Garbage Collector'))
                        ) 
            ) then
        fb_arch = 'SuperServer';
    else
        begin
            v_test_sttm =
                'select a.mon$server_pid + 0*(select 1 from rdb$database)'
                ||' from mon$attachments a '
                ||' where a.mon$attachment_id = current_connection';

            select i.mon$page_fetches
            from mon$io_stats i
            where i.mon$stat_group = 0  -- db_level
            into v_fetches_beg;
        
            execute statement v_test_sttm
            on external
                 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
            as
                 user a_connect_with_usr
                 password a_connect_with_pwd
                 role left('R' || replace(uuid_to_char(gen_uuid()),'-',''),31)
            into ext_server_pid;
        
            in autonomous transaction do
            select i.mon$page_fetches
            from mon$io_stats i
            where i.mon$stat_group = 0  -- db_level
            into v_fetches_end;
        
            fb_arch = iif( cur_server_pid is distinct from ext_server_pid, 
                           'Classic', 
                           iif( v_fetches_beg is not distinct from v_fetches_end, 
                                'SuperClassic', 
                                'SuperServer'
                              ) 
                         );
        end

    fb_arch = fb_arch || ' ' || rdb$get_context('SYSTEM','ENGINE_VERSION');

    suspend;

end 

^ -- sys_get_fb_arch

create or alter function fn$get$random$id$subst$names (
    a_view_for_search dm_dbobj,
    a_view_for_min_id dm_dbobj default null,
    a_view_for_max_id dm_dbobj default null,
    a_raise_exc dm_sign default 1, -- raise exc`eption if no record will be found
    a_can_skip_order_clause dm_sign default 0, -- 17.07.2014 (for some views where document is taken into processing and will be REMOVED from scope of this view after Tx is committed)
    a_find_using_desc_index dm_sign default 0  -- 11.09.2014: if 1, then query will be: "where id <= :a order by id desc"
)
returns bigint
as

    declare i smallint;
    declare v_stt varchar(255);
    declare id_min double precision;
    declare id_max double precision;
    declare v_rows int;
    declare id_random bigint;
    declare id_selected bigint = null;
    declare msg dm_info;
    declare v_info dm_info;
    declare v_this dm_dbobj = 'fn_get_random_id';
    declare v_ctxn dm_ctxnv;
    declare v_name dm_dbobj;
    declare fn_internal_max_rows_usage int;
    declare v_is_known smallint = 0;
begin
    -- Selects random record from view <a_view_for_search>
    -- using select first 1 id from ... where id >= :id_random order by id.
    -- Aux. parameters:
    -- # a_view_for_min_id and a_view_for_max_id -- separate views that
    --   might be more effective to find min & max LIMITS than scan using a_view_for_search.
    -- # a_raise_exc (default=1) - do we raise exc`eption if record not found.
    -- # a_can_skip_order_clause (default=0) - can we SKIP including of 'order by' clause
    --   in statement which will be passed to ES ? (for some cases we CAN do it for efficiency)
    -- # a_find_using_desc_index - do we construct ES for search using DESCENDING index
    --   (==> it will use "where id <= :r order by id DESC" rather than "where id >= :r order by id ASC")
    -- [only when TIL = RC] Repeats <fn_internal_retry_count()> times if result is null
    -- (possible if bounds of IDs has been changed since previous call)

    v_this = trim(a_view_for_search);

    -- max difference b`etween min_id and max_id to allow scan random id via
    -- select id from <a_view_for_search> rows :x to :y, where x = y = random_int
    fn_internal_max_rows_usage = cast( rdb$get_context('USER_SESSION','RANDOM_SEEK_VIA_ROWS_LIMIT') as int);

    -- Use either stub or non-empty executing code (depends on was 'oltp_dump.sql' compiled or no):
    -- save fact of usage views in the table `z_used_views`:
    execute procedure z_remember_view_usage(a_view_for_search, a_view_for_min_id, a_view_for_max_id);

    a_view_for_min_id = coalesce( a_view_for_min_id, a_view_for_search );
    a_view_for_max_id = coalesce( a_view_for_max_id, a_view_for_min_id, a_view_for_search );

    -- Label: $name_to_substutite_start_of_loop. Do not delete this line!

    if ( upper(a_view_for_search) = upper(trim('name$to$substutite$search$'))  ) then
    begin
        v_is_known = 1;
        if ( rdb$get_context('USER_TRANSACTION', upper(trim('name$to$substutite$min$id$'))||'_ID_MIN' ) is null
               or
               rdb$get_context('USER_TRANSACTION', upper(trim('name$to$substutite$max$id$'))||'_ID_MAX' ) is null
             ) then
            begin
                execute procedure sp_add_perf_log(1, a_view_for_min_id);
    
                if ( a_view_for_min_id is not null ) then
                    select min(id)-0.5 from name$to$substutite$min$id$ into id_min;
                else
                    select min(id)-0.5 from name$to$substutite$search$ into id_min;
    
                execute procedure sp_add_perf_log(0, a_view_for_min_id, null, 'static SQL, id_min='||coalesce(id_min,'<?>') );
        
                if ( id_min is NOT null ) then -- ==> source <a_view_for_min_id> is NOT empty
                begin
        
                    execute procedure sp_add_perf_log(1, a_view_for_max_id );
    
                    if ( a_view_for_max_id is not null ) then
                        select max(id)+0.5 from name$to$substutite$max$id$ into id_max;
                    else
                        select max(id)+0.5 from name$to$substutite$search$ into id_max;
    
                    execute procedure sp_add_perf_log(0, a_view_for_max_id, null, 'static SQL, id_max='||coalesce(id_max,'<?>') );
        
                    if ( id_max is NOT null  ) then -- ==> source <a_view_for_max_id> is NOT empty
                    begin
                        -- Save values for subsequent calls of this func in this tx (minimize DB access)
                        -- (limit will never change in SNAPSHOT and can change with low probability in RC):
                        rdb$set_context('USER_TRANSACTION', upper(trim('name$to$substutite$min$id$')) || '_ID_MIN', :id_min);
                        rdb$set_context('USER_TRANSACTION', upper(trim('name$to$substutite$max$id$')) || '_ID_MAX', :id_max);
                
                        if ( id_max - id_min < fn_internal_max_rows_usage ) then
                        begin
                            -- when difference b`etween id_min and id_max is not too high, we can simple count rows:
                            select count(*) from name$to$substutite$search$ into v_rows;
                            rdb$set_context('USER_TRANSACTION', upper(trim('name$to$substutite$search$')) || '_COUNT', v_rows );
                        end
                    end -- id_max is NOT null 
                end -- id_min is NOT null
            end
            else
                begin
                    -- minimize database access! Performance on 10'000 loops: 1485 ==> 590 ms
                    id_min=cast( rdb$get_context('USER_TRANSACTION', upper(trim('name$to$substutite$min$id$')) || '_ID_MIN' ) as double precision);
                    id_max=cast( rdb$get_context('USER_TRANSACTION', upper(trim('name$to$substutite$max$id$')) || '_ID_MAX' ) as double precision);
                    v_rows=cast( rdb$get_context('USER_TRANSACTION', upper(trim('name$to$substutite$search$')) || '_COUNT') as int);
                end
        
            if ( id_max - id_min < fn_internal_max_rows_usage ) then
                begin
                    --id_random = cast( 1 + rand() * (v_rows - 1) as int); -- WRONG when data skewed to low values; 30.07.2014
                    id_random = ceiling( rand() * (v_rows) );
                    select id
                    from name$to$substutite$search$
                    -- ::: nb ::: `ORDER` clause not needed here
                    rows :id_random to :id_random
                    into id_selected;
                end
            else
                begin
                    -- 17.07.2014: for some cases it is ALLOWED to query random ID without "ORDER BY"
                    -- clause because this ID will be handled in such manner that it will be REMOVED
                    -- after this handling from the scope of view! Samples of such cases are:
                    -- sp_cancel_supplier_order, sp_cancel_supplier_invoice, sp_cancel_customer_reserve
                    id_random = cast( id_min + rand() * (id_max - id_min) as bigint);

                    if ( a_can_skip_order_clause = 0 and a_find_using_desc_index = 0 ) then
                        select id
                        from name$to$substutite$search$
                        where id >= :id_random
                        order by id ASC
                        rows 1
                        into id_selected;
                    else if ( a_can_skip_order_clause = 0 and a_find_using_desc_index = 1 ) then
                        select id
                        from name$to$substutite$search$
                        where id <= :id_random
                        order by id DESC
                        rows 1
                        into id_selected;
                    else if ( a_can_skip_order_clause = 1 and a_find_using_desc_index = 0 ) then
                        select id
                        from name$to$substutite$search$
                        where id >= :id_random
                        rows 1
                        into id_selected;
                    else if ( a_can_skip_order_clause = 1 and a_find_using_desc_index = 1 ) then
                        select id
                        from name$to$substutite$search$
                        where id <= :id_random
                        rows 1
                        into id_selected;
                    else
                        exception ex_bad_argument using('a_can_skip_order_clause and/or a_find_using_desc_index', v_this);
        
                end

    end -- upper(a_view_for_search) = upper(trim('name$to$substutite$min$id$'))

    -- Label: $name_to_substutite_end_of_loop. Do not delete this line!

    if ( v_is_known = 0 ) then  -- passed view not from list of known names
    begin -- use ES (old code)

        if ( rdb$get_context('USER_TRANSACTION', upper(:a_view_for_min_id)||'_ID_MIN' ) is null
           or
           rdb$get_context('USER_TRANSACTION', upper(:a_view_for_max_id)||'_ID_MAX' ) is null
         ) then
        begin
            execute procedure sp_add_perf_log(1, a_view_for_min_id );
            -- v`iew z_get_min_max_id may be used to see average, min and max elapsed time
            -- of this sttm:
            v_stt='select min(id)-0.5 from '|| a_view_for_min_id;
            execute statement (:v_stt) into id_min; -- do via ES in order to see statistics in the TRACE!
            execute procedure sp_add_perf_log(0, a_view_for_min_id, null, 'dyn SQL, id_min='||coalesce(id_min,'<?>') );
    
            if ( id_min is NOT null ) then -- ==> source <a_view_for_min_id> is NOT empty
            begin
    
                execute procedure sp_add_perf_log(1, a_view_for_max_id );
                -- v`iew z_get_min_max_id may be used to see average, min and max elapsed time
                -- of this sttm:
                v_stt='select max(id)+0.5 from '|| a_view_for_max_id;
                execute statement (:v_stt) into id_max; -- do via ES in order to see statistics in the TRACE!
                execute procedure sp_add_perf_log(0, a_view_for_max_id, null, 'dyn SQL, id_max='||coalesce(id_max,'<?>') );
    
                if ( id_max is NOT null  ) then -- ==> source <a_view_for_max_id> is NOT empty
                begin
                    -- Save values for subsequent calls of this func in this tx (minimize DB access)
                    -- (limit will never change in SNAPSHOT and can change with low probability in RC):
                    rdb$set_context('USER_TRANSACTION', upper(:a_view_for_min_id)||'_ID_MIN', :id_min);
                    rdb$set_context('USER_TRANSACTION', upper(:a_view_for_max_id)||'_ID_MAX', :id_max);
            
                    if ( id_max - id_min < fn_internal_max_rows_usage ) then
                    begin
                        -- when difference b`etween id_min and id_max is not too high, we can simple count rows:
                        execute statement 'select count(*) from '||a_view_for_search into v_rows;
                        rdb$set_context('USER_TRANSACTION', upper(:a_view_for_search)||'_COUNT', v_rows );
                    end
                end -- id_max is NOT null 
            end -- id_min is NOT null
        end
        else begin
            -- minimize database access! Performance on 10'000 loops: 1485 ==> 590 ms
            id_min=cast( rdb$get_context('USER_TRANSACTION', upper(:a_view_for_min_id)||'_ID_MIN' ) as double precision);
            id_max=cast( rdb$get_context('USER_TRANSACTION', upper(:a_view_for_max_id)||'_ID_MAX' ) as double precision);
            v_rows=cast( rdb$get_context('USER_TRANSACTION', upper(:a_view_for_search)||'_COUNT') as int);
        end
    
        if ( id_max - id_min < fn_internal_max_rows_usage ) then
            begin
                v_stt='select id from '||a_view_for_search||' rows :x to :y'; -- ::: nb ::: `ORDER` clause not needed here!
                --id_random = cast( 1 + rand() * (v_rows - 1) as int); -- WRONG when data skewed to low values; 30.07.2014
                id_random = ceiling( rand() * (v_rows) );
                execute statement (:v_stt) (x := id_random, y := id_random) into id_selected;
            end
        else
            begin
                -- 17.07.2014: for some cases it is ALLOWED to query random ID without "ORDER BY"
                -- clause because this ID will be handled in such manner that it will be REMOVED
                -- after this handling from the scope of view! Samples of such cases are:
                -- sp_cancel_supplier_order, sp_cancel_supplier_invoice, sp_cancel_customer_reserve
                v_stt='select id from '
                    ||a_view_for_search
                    ||iif(a_find_using_desc_index = 0, ' where id >= :x', ' where id <= :x');
                if ( a_can_skip_order_clause = 0 ) then
                    v_stt = v_stt || iif(a_find_using_desc_index = 0, ' order by id     ', ' order by id desc');
                v_stt = v_stt || ' rows 1';
                id_random = cast( id_min + rand() * (id_max - id_min) as bigint);
    
                -- execute procedure sp_add_perf_log(1, a_view_for_search );
                -- do via ES in order to see statistics in the TRACE:
                execute statement (:v_stt) (x := id_random) into id_selected;
                -- execute procedure sp_add_perf_log(0, a_view_for_search, null, 'id_sel='||coalesce(id_selected,'<?>') );
            end

    end -- v_is_known = 0 ==> use ES (old code, when passed name of view NOT from known list)


    if ( id_selected is null and coalesce(a_raise_exc, 1) = 1 ) then
    begin

        v_info = 'view: ' || a_view_for_search; -- name$to$substutite$search$';
        if ( id_min is NOT null ) then
           v_info = v_info || ', id_min=' || id_min || ', id_max='||id_max;
        else
           v_info = v_info || ' - EMPTY';

        v_info = v_info ||', id_rnd='||coalesce(id_random,'<null>');

        -- 'no id @1 @2 in @3 found within scope @4 ... @5'; -- @1 is '>=' or '<='; @2 = random_selected_value; @3 = data source; @4 = min; @5 = max
        exception ex_can_not_select_random_id
            using(
                     iif(a_find_using_desc_index = 0,'>=','<=')
                    ,coalesce(id_random,'<?>')
                    ,a_view_for_search
                    ,coalesce(id_min,'<?>')
                    ,coalesce(id_max,'<?>')
                 );

    end

    return id_selected;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            v_stt,
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end -- fn$get$random$id$subst$names 

^

create or alter procedure sys_get_run_info(a_mode varchar(12)) returns(
    dts varchar(12)
    ,trn varchar(14)
    ,unit dm_unit
    ,msg varchar(20)
    ,add_info varchar(40)
    ,elapsed_ms varchar(10)
)
as
begin
    -- Aux SP for output info about unit that is to be performed now.
    -- used in batch 'oltpNN_worker.bat'
    dts = substring(cast(current_timestamp as varchar(24)) from 12 for 12);
    unit = rdb$get_context('USER_SESSION','SELECTED_UNIT');
    if ( a_mode='start' ) then
        begin
            trn = 'tra_'||coalesce(current_transaction,'<?>');
            msg = 'start';
            add_info = 'att_'||current_connection;
        end
    else
        begin
            trn = 'tra_'||rdb$get_context('USER_SESSION','APP_TRANSACTION');
            msg = rdb$get_context('USER_SESSION', 'RUN_RESULT');
            add_info = rdb$get_context('USER_SESSION','ADD_INFO');
            elapsed_ms =
                lpad(
                           cast(
                                 datediff(
                                   millisecond
                                   from cast(left(rdb$get_context('USER_SESSION','BAT_PHOTO_UNIT_DTS'),24) as timestamp)
                                   to   cast(right(rdb$get_context('USER_SESSION','BAT_PHOTO_UNIT_DTS'),24) as timestamp)
                                        )
                                as varchar(10)
                               )
                          ,10
                          ,' '
                        );
        end
    suspend;
end

^ -- sys_get_run_info

create or alter procedure sys_timestamp_to_ansi (a_dts timestamp default 'now')
returns ( ansi_dts varchar(15) ) as
begin
    ansi_dts =
        cast(extract( year from a_dts)*10000 + extract(month from a_dts) * 100 + extract(day from a_dts) as char(8))
         || '_'
         || substring(cast(cast(1000000 + extract(hour from a_dts) * 10000 + extract(minute from a_dts) * 100 + extract(second from a_dts) as int) as char(7)) from 2);
    suspend;
end

^ -- sys_timestamp_to_ansi

set term ;^
set list on;
set echo off;
select 'oltp30_DDL.sql finish at ' || current_timestamp as msg from rdb$database;
set list off;


-- ###########################################################
-- End of script oltp30_DDL.sql; next to be run: oltp30_SP.sql
-- ########################################################### 

-- #################################################
-- Begin of script oltp30_SP.sql (application units)
-- #################################################
-- ::: nb ::: Required FB version: 3.0 and above
set autoddl off;
set list on;
select 'oltp30_sp.sql start at ' || current_timestamp as msg from rdb$database;
set list off;


set term ^;
execute block as
begin
  begin
    execute statement 'recreate exception ex_exclusive_required ''At least one concurrent connection detected.''';
    when any do begin end
  end
  begin
    execute statement 'recreate exception ex_not_suitable_fb_version ''This script requires at least Firebird 3.x version''';
    when any do begin end
  end
end
^
set term ;^


set term ^;
execute block as
begin
    if ( rdb$get_context('SYSTEM','ENGINE_VERSION') starting with '2.' ) then
    begin
        exception ex_not_suitable_fb_version;
    end

    -- NB. From doc/README.monitoring_tables:
    -- columns MON$REMOTE_PID and MON$REMOTE_PROCESS contains non-NULL values
    -- only if the client library has version 2.1 or higher
    -- column MON$REMOTE_PROCESS can contain a non-pathname value
    -- if an application has specified a custom process name via DPB
    if ( exists( select * from mon$attachments a
                 where a.mon$attachment_id<>current_connection
                 and a.mon$remote_protocol is not null
                )
       ) then
    begin
        exception ex_exclusive_required;
    end
end
^
set term ;^


set term ^;

create or alter procedure srv_increment_tx_bops_counter
as
begin
    rdb$set_context( 'USER_TRANSACTION', 'BUSINESS_OPS_CNT', coalesce( cast(rdb$get_context('USER_TRANSACTION', 'BUSINESS_OPS_CNT') as int), 0) + 1);
end

^ -- srv_increment_tx_bops_counter

create or alter procedure sp_fill_shopping_cart(
    a_optype_id dm_idb,
    a_rows2add int default null, 
    a_maxq4row int default null)
returns(
    row_cnt int, -- number of rows added to tmp$shop_cart
    qty_sum dm_qty -- total on QTY field in tmp$shop_cart
)
as
    declare v_doc_rows int;
    declare v_id dm_idb;
    declare v_ware_id type of dm_idb;
    declare v_qty dm_qty;
    declare v_cost_purchase dm_cost;
    declare v_cost_retail dm_cost;
    declare v_snd_optype_id dm_idb;
    declare v_storno_sub smallint;
    declare v_ctx_max_rows type of dm_ctxnv;
    declare v_ctx_max_qty type of dm_ctxnv;
    declare v_stt varchar(255);
    declare v_pattern type of dm_name;
    declare v_source_for_random_id dm_dbobj;
    declare v_source_for_min_id dm_dbobj;
    declare v_source_for_max_id dm_dbobj;
    declare v_raise_exc_on_nofind dm_sign;
    declare v_can_skip_order_clause dm_sign;
    declare v_find_using_desc_index dm_sign;
    declare v_this dm_dbobj = 'sp_fill_shopping_cart';
    declare v_info dm_info = '';
begin
    -- Fills "shopping cart" table with wares ID for futher handling.
    -- If context var 'ENABLE_FILL_PHRASES' = 1 then does it via SIMILAR TO
    -- by searching phrases (patterns) in wares.name table.
    -- Used in apps that CREATE new documents (client order, customer reserve etc)

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    v_ctx_max_rows = iif( a_optype_id in ( fn_oper_order_for_supplier(), fn_oper_invoice_get() ),
                          'C_SUPPLIER_DOC_MAX_ROWS',
                          'C_CUSTOMER_DOC_MAX_ROWS'
                        );
    v_ctx_max_qty = iif( a_optype_id in ( fn_oper_order_for_supplier(), fn_oper_invoice_get() ),
                          'C_SUPPLIER_DOC_MAX_QTY',
                          'C_CUSTOMER_DOC_MAX_QTY'
                        );

    v_doc_rows =  coalesce( a_rows2add, fn_get_random_quantity( v_ctx_max_rows ) ) ;

    v_source_for_random_id =
        decode( a_optype_id,
                fn_oper_order_by_customer(),  'v_all_wares',
                fn_oper_order_for_supplier(), 'v_random_find_clo_ord',
                fn_oper_invoice_get(),        'v_random_find_ord_sup',
                fn_oper_retail_reserve(),     'v_random_find_avl_res',
                'unknown_source'
              );

    v_source_for_min_id =
        decode( a_optype_id,
                fn_oper_order_for_supplier(), 'v_min_id_clo_ord',
                fn_oper_invoice_get(),        'v_min_id_ord_sup',
                fn_oper_retail_reserve(),     'v_min_id_avl_res',
                null
              );

    v_source_for_max_id =
        decode( a_optype_id,
                fn_oper_order_for_supplier(), 'v_max_id_clo_ord',
                fn_oper_invoice_get(),        'v_max_id_ord_sup',
                fn_oper_retail_reserve(),     'v_max_id_avl_res',
                null
              );

    -- 17.07.2014: for some cases we allow to skip 'ORDER BY ID' clause
    -- in sp_get_random_id when it will generate SQL expr for ES,
    -- because all such randomly choosen IDs are handled in so way
    -- that thay will be unavaliable in the source view
    -- after this handling after it successfully ends.
    -- Since 11.09.2014, start usage id desc in sp_get_random_id,
    -- bitmaps building is very expensive ==> always set this var = 0.
    v_can_skip_order_clause = 0;

    -- 19.07.2014, see DDL of views v_random_find_xxx:
    -- they have `where not exists(select * from ... c id >= :id_rand_selected)`,
    -- so it can be that search will NOT found any ID due to unhappy result of random
    -- selection of 'anchor-ID' (no IDs will be found in 'where id >= :id_selected'
    -- due to all of them have been removed etc) ==> we suppress e`xc in such case!
    v_raise_exc_on_nofind =
        decode( a_optype_id,
                fn_oper_order_by_customer(),  0,
                fn_oper_order_for_supplier(), 0,
                fn_oper_invoice_get(),        0,
                fn_oper_retail_reserve(),     0,
                1
              );

    v_find_using_desc_index =
        decode( a_optype_id,
                fn_oper_order_for_supplier(), 1,
                fn_oper_invoice_get(),        1,
                fn_oper_retail_reserve(),     1,
                0
              );

    select
        r.snd_optype_id
        ,r.storno_sub
    from rules_for_qdistr r
    where
        r.rcv_optype_id = :a_optype_id
        and r.mode containing 'new_doc' 
    into v_snd_optype_id, v_storno_sub;

    v_info = 'view='||v_source_for_random_id||', rows='||v_doc_rows||', oper='||a_optype_id;

    delete from tmp$shopping_cart where 1=1;
    row_cnt = 0;
    qty_sum = 0;

    for
        select p.id_selected
        from
                sp_get_random_id(
                    :v_source_for_random_id,
                    :v_source_for_min_id,
                    :v_source_for_max_id,
                    :v_raise_exc_on_nofind, -- 19.07.2014: 0 ==> do NOT raise exception if not able to find any ID in view :v_source_for_random_id
                    :v_can_skip_order_clause, -- 17.07.2014: if = 1, then 'order by id' will be SKIPPED in statement inside fn
                    :v_find_using_desc_index, -- 11.09.2014, performance of select id from v_xxx order by id DESC rows 1
                    :v_doc_rows
                ) p
         into v_ware_id
    do
    begin
        v_qty = coalesce(a_maxq4row, fn_get_random_quantity( v_ctx_max_qty ));
        if ( a_optype_id = fn_oper_order_by_customer() ) then
        begin
            -- Define cost of ware being added in customer order,
            -- in purchasing and retailing prices (allow them to vary):
            select
                 round( w.price_purchase + rand() * 300, -2) * :v_qty
                ,round( w.price_retail + rand() * 300, -2) * :v_qty
            from wares w
            where w.id = :v_ware_id
            into v_cost_purchase, v_cost_retail;
        end

        if ( v_ware_id is not null ) then
        begin
            -- All the views v_r`andom_finx_xxx have checking clause like
            -- "where NOT exists(select * from tmp$shopping_cart c where ...)"
            -- so we can immediatelly do INSERT rather than update+check row_count=0
            insert into tmp$shopping_cart(
                id,
                snd_optype_id,
                rcv_optype_id,
                qty,
                storno_sub,
                cost_purchase,
                cost_retail
            )
            values (
                :v_ware_id,
                :v_snd_optype_id,
                :a_optype_id,
                :v_qty,
                :v_storno_sub,
                :v_cost_purchase,
                :v_cost_retail
            );
            row_cnt = row_cnt + 1; -- out arg, will be used in getting batch IDs for doc_data (reduce lock-contention of GEN page)
            qty_sum = qty_sum + ceiling( v_qty ); -- out arg, will be passed to s`p_multiply_rows_for_pdistr, s`p_make_qty_storno
        
        when any
            do begin
                if ( fn_is_uniqueness_trouble(gdscode) ) then
                    update tmp$shopping_cart t
                    set t.dup_cnt = t.dup_cnt+1 -- 4debug only
                    where t.id = :v_ware_id;
                else
                    exception; -- anonimous but in WHEN block
            end
        end -- v_ware_id not null
    end

--    while ( v_doc_rows > 0 ) do begin
--
--        v_qty = coalesce(a_maxq4row, fn_get_random_quantity( v_ctx_max_qty ));
--
--        if ( a_optype_id = fn_oper_order_by_customer() ) then
--            begin
--                if ( rdb$get_context('USER_SESSION','ENABLE_FILL_PHRASES')='1' -- enable check performance of similar_to
--                     and
--                     exists( select * from phrases )
--                   ) then
--                    begin
--                        -- For checking performance of SIMILAR TO:
--                        -- search using preliminary generated patterns
--                        -- (generation of them see in oltp_fill_data.sql):
--                        select p.pattern from phrases p
--                        where p.id = (select sp_get_random_id('phrases',null,null, :v_raise_exc_on_nofind) from rdb$database)
--                        into v_pattern;
--                        v_stt = 'select id from wares where '||v_pattern||' rows 1';
--                        execute statement(v_stt) into v_ware_id;
--                        if ( v_ware_id is null ) then
--                          exception ex_record_not_found using ('wares', v_pattern);
--                    end
--                else
--                    v_ware_id =
--                    sp_get_random_id(
--                        v_source_for_random_id,
--                        null,
--                        null,
--                        :v_raise_exc_on_nofind
--                    ); -- <<< take random ware from price list
--
--                -- Define cost of ware being added in customer order,
--                -- in purchasing and retailing prices (allow them to vary):
--                select
--                     round( w.price_purchase + rand() * 300, -2) * :v_qty
--                    ,round( w.price_retail + rand() * 300, -2) * :v_qty
--                from wares w
--                where w.id = :v_ware_id
--                into v_cost_purchase, v_cost_retail;
--            end
--        else -- a_optype_id <> fn_oper_order_by_customer()
--            begin
--                v_ware_id =
--                sp_get_random_id(
--                    v_source_for_random_id,
--                    v_source_for_min_id,
--                    v_source_for_max_id,
--                    v_raise_exc_on_nofind, -- 19.07.2014: 0 ==> do NOT raise exception if not able to find any ID in view :v_source_for_random_id
--                    v_can_skip_order_clause, -- 17.07.2014: if = 1, then 'order by id' will be SKIPPED in statement inside fn
--                    v_find_using_desc_index -- 11.09.2014, performance of select id from v_xxx order by id DESC rows 1
--                );
--            end
--
--        if ( v_ware_id is not null ) then
--        begin
--            -- All the views v_r`andom_finx_xxx have checking clause like
--            -- "where NOT exists(select * from tmp$shopping_cart c where ...)"
--            -- so we can immediatelly do INSERT rather than update+check row_count=0
--            insert into tmp$shopping_cart(
--                id,
--                snd_optype_id,
--                rcv_optype_id,
--                qty,
--                storno_sub,
--                cost_purchase,
--                cost_retail
--            )
--            values (
--                :v_ware_id,
--                :v_snd_optype_id,
--                :a_optype_id,
--                :v_qty,
--                :v_storno_sub,
--                :v_cost_purchase,
--                :v_cost_retail
--            );
--            row_cnt = row_cnt + 1; -- out arg, will be used in getting batch IDs for doc_data (reduce lock-contention of GEN page)
--            qty_sum = qty_sum + ceiling( v_qty ); -- out arg, will be passed to s`p_multiply_rows_for_pdistr, s`p_make_qty_storno
--
--        when any
--            do begin
--                if ( fn_is_uniqueness_trouble(gdscode) ) then
--                    update tmp$shopping_cart t
--                    set t.dup_cnt = t.dup_cnt+1 -- 4debug only
--                    where t.id = :v_ware_id;
--                else
--                    exception; -- anonimous but in WHEN block
--            end
--        end -- v_ware_id not null
--        v_doc_rows = v_doc_rows -1;
--
--    end -- while ( v_doc_rows > 0 ) 

    if ( not exists(select * from tmp$shopping_cart) ) then
        exception ex_no_rows_in_shopping_cart using( v_source_for_random_id ); -- 'shopping_cart is empty, check source ''@1'''

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this,null, v_info );

    suspend; -- row_cnt, qty_sum

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^ -- sp_fill_shopping_cart

------------------------------------------------------------------------------

create or alter procedure sp_client_order(
    dbg int default 0,
    dbg_rows2add int default null,
    dbg_maxq4row int default null
)
returns (
    doc_list_id type of dm_idb,
    agent_id type of dm_idb,
    doc_data_id type of dm_idb,
    ware_id type of dm_idb,
    qty type of dm_qty,
    purchase type of dm_cost, -- purchasing cost for qty
    retail type of dm_cost, -- retail cost
    qty_clo type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_clr type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_ord type of dm_qty -- new value of corresponding row in invnt_saldo
)
as
    declare c_gen_inc_step_dd int = 20; -- size of `batch` for get at once new IDs for doc_data (reduce lock-contention of gen page)
    declare v_gen_inc_iter_dd int; -- increments from 1  up to c_gen_inc_step_dd and then restarts again from 1
    declare v_gen_inc_last_dd dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_dd)

    declare c_gen_inc_step_nt int = 20; -- size of `batch` for get at once new IDs for invnt_turnover_log (reduce lock-contention of gen page)
    declare v_gen_inc_iter_nt int; -- increments from 1  up to c_gen_inc_step_dd and then restarts again from 1
    declare v_gen_inc_last_nt dm_idb; -- last got value after call gen_id (..., c_gen_inc_step_dd)

    declare v_oper_order_by_customer dm_idb;
    declare v_nt_new_id dm_idb;
    declare v_clo_for_our_firm dm_sign = 0;
    declare v_rows_added int = 0;
    declare v_qty_sum dm_qty = 0;
    declare v_purchase_sum dm_cost;
    declare v_retail_sum dm_cost;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_dd_new_id bigint;
    declare v_dd_dbkey dm_dbkey;
    declare v_dbkey dm_dbkey;
    declare v_this dm_dbobj = 'sp_client_order';

    declare c_shop_cart cursor for (
        select
            c.id,
            c.qty,
            c.cost_purchase,
            c.cost_retail
        from tmp$shopping_cart c
    );
begin
    -- Selects randomly agent, wares and creates a new document with wares which
    -- we should provide to customer ("CLIENT ORDER"). Business starts from THIS
    -- kind of doc: customer comes in our office and wants to buy / order smthn.
    
    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises ex`ception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    v_oper_order_by_customer = fn_oper_order_by_customer();

    -- Random select contragent for this client order
    -- About 20...30% of orders are for our firm (==> they will NOT move to
    -- 'reserves' after corresponding invoices will be added to stock balance):
    if ( rand()*100 <= cast(rdb$get_context('USER_SESSION', 'ORDER_FOR_OUR_FIRM_PERCENT') as int) ) then
        begin
            v_clo_for_our_firm = 1;
            --agent_id = sp_get_random_id('v_our_firm', null, null, 0);
            select id_selected from sp_get_random_id('v_our_firm', null, null, 0) into agent_id;
        end
    else
        agent_id = fn_get_random_customer();

    execute procedure sp_fill_shopping_cart( v_oper_order_by_customer, dbg_rows2add, dbg_maxq4row )
    returning_values v_rows_added, v_qty_sum;

    if (dbg=1) then exit;

    execute procedure sp_add_doc_list(
        null,
        v_oper_order_by_customer,
        agent_id,
        fn_doc_fix_state()
    )
    returning_values
        :doc_list_id, -- out arg
        :v_dbkey;

    v_gen_inc_iter_dd = 1;
    c_gen_inc_step_dd = 1 + v_rows_added; -- for adding rows in doc_data: size of batch = number of rows in tmp$shop_cart + 1
    v_gen_inc_last_dd = gen_id( g_doc_data, :c_gen_inc_step_dd );-- take bulk IDs at once (reduce lock-contention for GEN page)

    v_gen_inc_iter_nt = 1;
    c_gen_inc_step_nt = 1 + v_rows_added; -- for adding rows in invnt_turnover_log: size of batch = number of rows in tmp$shop_cart + 1
    v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );-- take bulk IDs at once (reduce lock-contention for GEN page)

    v_purchase_sum = 0;
    v_retail_sum = 0;

    -- Process each record in tmp$shopping_cart:
    -- 1) add row to detalization table (doc_data) with amount that client orders;
    -- 2) add for each row from shoping cart SEVERAL rows in QDistr - they form
    --    set of records for futher STORNING by new document(s) in next business
    -- operation (our order to supplier). Number of rows being added in QDistr
    -- equals to doc_data.qty (for simplicity of code these amounts are considered
    -- to be always INTEGER values).
    open c_shop_cart;
    while (1=1) do
    begin
        fetch c_shop_cart into ware_id, qty, purchase, retail;
        if ( row_count = 0 ) then leave;

        if ( v_gen_inc_iter_dd = c_gen_inc_step_dd ) then -- its time to get another batch of IDs
        begin
            v_gen_inc_iter_dd = 1;
            -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
            v_gen_inc_last_dd = gen_id( g_doc_data, :c_gen_inc_step_dd );
        end
        v_dd_new_id = v_gen_inc_last_dd - ( c_gen_inc_step_dd - v_gen_inc_iter_dd );

--        rdb$set_context('USER_TRANSACTION','DBG_DD_D',
--              'v_dd_new_id='||v_dd_new_id
--            ||', v_last_dd='||v_gen_inc_last_dd
--            ||', c_step_dd='||c_gen_inc_step_dd
--            ||', v_iter_dd='||v_gen_inc_iter_dd
--        );
        v_gen_inc_iter_dd = v_gen_inc_iter_dd + 1;

        if ( v_gen_inc_iter_nt = c_gen_inc_step_nt ) then -- its time to get another batch of IDs
        begin
            v_gen_inc_iter_nt = 1;
            -- take subsequent bulk IDs at once (reduce lock-contention for GEN page)
            v_gen_inc_last_nt = gen_id( g_common, :c_gen_inc_step_nt );
        end
        v_nt_new_id = v_gen_inc_last_nt - ( c_gen_inc_step_nt - v_gen_inc_iter_nt );
        v_gen_inc_iter_nt = v_gen_inc_iter_nt + 1;

        execute procedure sp_add_doc_data(
            doc_list_id,
            v_oper_order_by_customer,
            v_dd_new_id, -- preliminary calculated ID for new record in doc_data (reduce lock-contention of GEN page)
            v_nt_new_id, -- preliminary calculated ID for new record in invnt_turnover_log (reduce lock-contention of GEN page)
            ware_id,
            qty,
            purchase,
            retail
        ) returning_values v_dd_new_id, v_dd_dbkey;

        -- Write ref to doc_data.id - it will be used in sp_multiply_rows_for_qdistr:
        update tmp$shopping_cart c set c.snd_id = :v_dd_new_id where current of c_shop_cart;

        -- do NOT use trigger-way updates of doc header for each row being added
        -- in detalization table: it will be run only once after this loop (performance):
        v_purchase_sum = v_purchase_sum + purchase;
        v_retail_sum = v_retail_sum + retail;
    end -- cursor on tmp$shopping_car join wares
    close c_shop_cart;

    if (dbg=2) then exit;
    -- 02.09.2014 2205: remove call of sp_multiply_rows_for_qdistr from t`rigger d`oc_data_aiud
    -- (otherwise fractional values in cumulative qdistr.snd_purchase will be when costs for
    --  same ware in several storned docs differs):
    -- 30.09.2014: move out from for-loop, single call:
    execute procedure sp_multiply_rows_for_qdistr(
        doc_list_id,
        v_oper_order_by_customer,
        v_clo_for_our_firm,
        v_qty_sum -- this is number of _ROWS_ that will be added into QDistr (used to calc. size of 'bulks' of new IDs - minimize call of gen_id in sp_multiply_rows_for_qdistr)
    );
    if (dbg=3) then exit;

    -- Single update of doc header (not for every row added in doc_data table
    -- as it would be via it's trigger).
    -- Trigger d`oc_list_aiud will call sp_add_invnt_log to add rows to invnt_turnover_log
    update doc_list h set
        h.cost_purchase = :v_purchase_sum,
        h.cost_retail = :v_retail_sum
    where h.rdb$db_key  = :v_dbkey;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this,null, 'doc_id='||coalesce(doc_list_id,'<null>')||', rows='||v_rows_added );

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);
    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    if ( v_ibe = 1 ) then
        v_stt = 'select v.agent_id, v.doc_data_id, v.ware_id, v.qty, v.cost_purchase, v.cost_retail'
                ||',v.qty_clo ,v.qty_clr ,v.qty_ord'
                ||' from v_doc_detailed v where v.doc_id = :x';
    else
        v_stt = 'select h.agent_id, d.id, d.ware_id, d.qty, d.cost_purchase, d.cost_retail'
                ||',null      ,null      ,null'
                ||' from doc_data d join doc_list h on d.doc_id = h.id where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement(v_stt) ( x := :doc_list_id )
    into
         agent_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_clo
        ,qty_clr
        ,qty_ord
    do
    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- end of sp_client_order
------------------------------------------------------------------------------

create or alter procedure sp_cancel_client_order(
    a_selected_doc_id type of dm_idb default null,
    dbg int default 0
)
returns (
    doc_list_id type of dm_idb,
    agent_id type of dm_idb,
    doc_data_id type of dm_idb,
    ware_id type of dm_idb,
    qty type of dm_qty,
    purchase type of dm_cost, -- purchasing cost for qty
    retail type of dm_cost, -- retail cost
    qty_clo type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_clr type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_ord type of dm_qty
)
as
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_dummy bigint;
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;
    declare v_this dm_dbobj = 'sp_cancel_client_order';
begin

    -- Moves client order in 'cancelled' state. No rows from such client order
    -- will be ordered to supplier (except those which we already ordered before)

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises ex`ception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    -- Choose random doc of corresponding kind.
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            (select id_selected from
                            sp_get_random_id( 'v_cancel_client_order' -- a_view_for_search
                                              ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                              ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                              ,:c_raise_exc_when_no_found
                                              ,:c_can_skip_order_clause
                                            )
                            )
                          );

    -- Find doc ID (with checking in view v_*** is need) and try to LOCK it.
    -- Raise exc if can`t lock:
    execute procedure sp_lock_selected_doc( doc_list_id, 'v_cancel_client_order', a_selected_doc_id);

    -- 20.05.2014: BLOCK client_order in ALL cases instead of solving question about deletion
    -- Trigger doc_list_biud will (only for deleting doc or updating it's state):
    -- 1) call s`p_kill_qty_storno that returns rows from Q`Storned to Q`distr 
    -- Trigger doc_list_aiud will:
    -- 1) add rows in table i`nvnt_turnover_log (log to be processed by SP s`rv_make_invnt_saldo)
    -- 2) call s`p_multiply_rows_for_pdistr, s`p_make_cost_storno or s`p_kill_cost_storno, s`p_add_money_log
    update doc_list h set
        h.optype_id = fn_oper_cancel_customer_order(),
        h.state_id = fn_doc_canc_state() --"cancelled without revert"
    where h.id = :doc_list_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this,null, 'doc_id='||doc_list_id);

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);
    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    if ( v_ibe = 1 ) then
        v_stt = 'select v.agent_id, v.doc_data_id, v.ware_id, v.qty, v.cost_purchase, v.cost_retail'
                ||',v.qty_clo ,v.qty_clr ,v.qty_ord'
                ||' from v_doc_detailed v where v.doc_id = :x';
    else
        v_stt = 'select h.agent_id, d.id, d.ware_id, d.qty, d.cost_purchase, d.cost_retail'
                ||',null      ,null      ,null'
                ||' from doc_data d join doc_list h on d.doc_id = h.id where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement(v_stt) ( x := :doc_list_id )
    into
         agent_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_clo
        ,qty_clr
        ,qty_ord
    do
    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- end of sp_cancel_client_order

------------------------------------------------------------------------------

create or alter procedure sp_supplier_order(
    dbg int default 0,
    dbg_rows2add int default null,
    dbg_maxq4row int default null
)
returns (
    doc_list_id type of dm_idb,
    agent_id type of dm_idb,
    doc_data_id type of dm_idb,
    ware_id type of dm_idb,
    qty type of dm_qty, -- amount that we ordered for client
    purchase type of dm_cost, -- purchasing cost for qty
    retail type of dm_cost, -- retail cost
    qty_clo type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_ord type of dm_qty -- new value of corresponding row in invnt_saldo
)
as
    declare v_id bigint;
    declare v_rows_added int;
    declare v_qty_sum dm_qty;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_dummy bigint;
    declare v_this dm_dbobj = 'sp_supplier_order';
begin

    -- Processes several client orders and creates OUR order of wares to randomly
    -- selected supplier (i.e. we expect these wares to be supplied by him).
    -- Makes storning of corresp. amounts, so preventing duplicate including in further
    -- supplier orders such wares (part of their total amounts) which was already ordered.
    -- This operation is NEXT after client order in 'business chain'.

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises ex`ception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    -- choose randomly contragent that will be supplier for this order:
    agent_id = fn_get_random_supplier();

    execute procedure sp_fill_shopping_cart( fn_oper_order_for_supplier(), dbg_rows2add, dbg_maxq4row )
    returning_values v_rows_added, v_qty_sum;

    if (dbg=1) then exit;

    -- 1. Find rows in QDISTR (and silently try to LOCK them) which can provide
    --    required amounts in tmp$shopping_cart, in FIFO manner.
    -- 2. Perform "STORNING" of them (moves these rows from QDISTR to QSTORNED)
    -- 3. Create new document: header (doc_list) and detalization (doc_data).
    execute procedure sp_make_qty_storno(
        fn_oper_order_for_supplier()
        ,agent_id
        ,fn_doc_open_state()
        ,null
        ,v_rows_added -- used there for 'smart' definition of value to increment gen
        ,v_qty_sum -- used there for 'smart' definition of value to increment gen
    ) returning_values doc_list_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||coalesce(doc_list_id,'<null>'));

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);

    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    if ( v_ibe = 1 ) then
       v_stt = 'select v.agent_id, v.doc_data_id, v.ware_id, v.qty, v.cost_purchase, v.cost_retail'
               ||' ,v.qty_clo ,v.qty_ord'
               ||' from v_doc_detailed v where v.doc_id = :x';
    else
       v_stt = 'select h.agent_id, d.id, d.ware_id, d.qty, d.cost_purchase, d.cost_retail'
               ||' ,null     ,null'
               ||' from doc_data d join doc_list h on d.doc_id = h.id where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement(v_stt) ( x := :doc_list_id )
    into
         agent_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_clo
        ,qty_ord
    do
        suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- end of sp_supplier_order

-----------------------------------------

create or alter procedure sp_supplier_invoice (
    dbg int = 0,
    dbg_rows2add int default null,
    dbg_maxq4row int default null
)
returns (
    doc_list_id type of dm_idb,
    agent_id type of dm_idb,
    doc_data_id type of dm_idb,
    ware_id type of dm_idb,
    qty type of dm_qty,
    purchase type of dm_cost,
    retail type of dm_cost,
    qty_clo type of dm_qty,
    qty_ord type of dm_qty,
    qty_sup type of dm_qty
)
as
    declare v_rows_added int;
    declare v_qty_sum dm_qty;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_this dm_dbobj = 'sp_supplier_invoice';
begin

    -- Simulates activity of our SUPPLIER when he got from us several orders:
    -- process randomly chosen wares from our orders and add them into INVOICE -
    -- the document that we consider as preliminary income (i.e. NOT yet accepted).
    -- Makes storning of corresp. amounts, so preventing duplicate including in further
    -- supplier invoices such wares (part of their total amounts) which was already
    -- included in this invoice.
    -- This operation is NEXT after our order to supplier in 'business chain'.

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises exc`eption to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    -- select supplier, random:
    agent_id = fn_get_random_supplier();

    execute procedure sp_fill_shopping_cart( fn_oper_invoice_get(), dbg_rows2add, dbg_maxq4row )
    returning_values v_rows_added, v_qty_sum;

    if (dbg=1) then exit;

    -- 1. Find rows in QDISTR (and silently try to LOCK them) which can provide required
    --    amounts in tmp$shopping_cart, in FIFO manner.
    -- 2. Perform "STORNING" of them (moves these rows from QDISTR to QSTORNED)
    -- 3. Create new document: header (doc_list) and detalization (doc_data).
    execute procedure sp_make_qty_storno(
        fn_oper_invoice_get()
        ,agent_id
        ,fn_doc_open_state()
        ,null
        ,v_rows_added -- used there for 'smart' definition of value to increment gen
        ,v_qty_sum -- used there for 'smart' definition of value to increment gen
    ) returning_values doc_list_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||coalesce(doc_list_id,'<null>'));

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);
    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    if ( v_ibe = 1 ) then
       v_stt = 'select v.agent_id, v.doc_data_id, v.ware_id, v.qty, v.cost_purchase, v.cost_retail'
               ||' ,v.qty_clo ,v.qty_ord ,v.qty_sup'
               ||' from v_doc_detailed v where v.doc_id = :x';
    else
       v_stt = 'select h.agent_id, d.id, d.ware_id, d.qty, d.cost_purchase, d.cost_retail'
               ||' ,null     ,null       ,null'
               ||' from doc_data d join doc_list h on d.doc_id = h.id where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement(v_stt) ( x := :doc_list_id )
    into
         agent_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_clo
        ,qty_ord
        ,qty_sup
    do
        suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^ -- end of sp_supplier_invoice

-----------------------------------------

create or alter procedure sp_cancel_supplier_invoice(
    a_selected_doc_id type of dm_idb default null,
    a_skip_lock_attempt dm_sign default 0 -- 1==> do NOT call sp_lock_selected_doc because this doc is already locked (see call from sp_cancel_adding_invoice)
)
returns(
    doc_list_id type of dm_idb, -- id of created invoice
    agent_id type of dm_idb, -- id of supplier
    doc_data_id type of dm_idb, -- id of created records in doc_data
    ware_id type of dm_idb, -- id of wares that we will get from supplier
    qty type of dm_qty, -- amount that supplier will send to us
    purchase type of dm_cost, -- total purchasing cost for qty
    retail type of dm_cost, -- assigned retail cost
    qty_clo type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_ord type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_sup type of dm_qty  -- new value of corresponding row in invnt_saldo
)
as
    declare v_dummy bigint;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;
    declare v_this dm_dbobj = 'sp_cancel_supplier_invoice';
begin

    -- Randomly chooses invoice from supplier (NOT yet accepted) and CANCEL it
    -- by REMOVING all its data + record in docs header table. It occurs when
    -- we mistakenly created such invoice and now have to cancel this operation.
    -- All wares which were in such invoice will be enabled again to be included
    -- in new (another) invoice which we create after this - due to removing info
    -- about amounts storning that was done before when invoice was created.

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises exc`eption to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    -- Choose random doc of corresponding kind.
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            ( select id_selected from
                              sp_get_random_id(  'v_cancel_supplier_invoice' -- a_view_for_search
                                                ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                                ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                                ,:c_raise_exc_when_no_found
                                                ,:c_can_skip_order_clause
                                              )
                            )
                          );
    -- upd. log with doc id whic is actually handling now:
    execute procedure sp_upd_in_perf_log( v_this, null, 'dh='||doc_list_id);

    -- Try to LOCK just selected doc, raise exc if can`t:
    if (  NOT (a_selected_doc_id is NOT null and a_skip_lock_attempt = 1) ) then
        execute procedure sp_lock_selected_doc( doc_list_id, 'v_cancel_supplier_invoice', a_selected_doc_id);

    -- 17.07.2014: add cond for indexed scan to minimize fetches when multiple
    -- calls of this SP from sp_cancel_adding_invoice:
    delete from tmp$result_set r where r.doc_id = :doc_list_id;

    -- save data which is to be deleted (NB! this action became MANDATORY for
    -- checking in srv_find_qd_qs_mism, do NOT delete it!):
    insert into tmp$result_set( doc_id, agent_id, doc_data_id, ware_id, qty, cost_purchase, cost_retail)
    select :doc_list_id, h.agent_id, d.id, d.ware_id, d.qty, d.cost_purchase, d.cost_retail
    from doc_data d
    join doc_list h on d.doc_id = h.id
    where d.doc_id = :doc_list_id; -- invoice which is to be removed now

    -- Trigger doc_list_biud will (only for deleting doc or updating it's state):
    -- 1) call s`p_kill_qty_storno that returns rows from Q`Storned to Q`distr 
    -- Trigger doc_list_aiud will:
    -- 1) add rows in table i`nvnt_turnover_log (log to be processed by SP s`rv_make_invnt_saldo)
    -- 2) call s`p_multiply_rows_for_pdistr, s`p_make_cost_storno or s`p_kill_cost_storno, s`p_add_money_log
    delete from doc_list h where h.id = :doc_list_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null);

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);
    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    v_stt = 'select r.doc_id,r.agent_id,r.doc_data_id,r.ware_id,r.qty,r.cost_purchase,r.cost_retail';

    if ( v_ibe = 1 ) then
       v_stt = v_stt || ' ,n.qty_clo ,n.qty_ord ,n.qty_sup';
    else
       v_stt = v_stt || ' ,null     ,null       ,null';

    v_stt = v_stt ||' from tmp$result_set r';
    if ( v_ibe = 1 ) then
       v_stt = v_stt || ' left join v_saldo_invnt n on r.ware_id = n.ware_id';

    -- 17.07.2014: add cond for indexed scan to minimize fetches when multiple
    -- calls of this SP from s`p_cancel_supplier_order:
    v_stt = v_stt || ' where r.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement (v_stt) ( x := :doc_list_id )
    into
        doc_list_id
        ,agent_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_clo
        ,qty_ord
        ,qty_sup
    do
        suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_cancel_supplier_invoice

-----------------------------------------

create or alter procedure sp_fill_shopping_cart_clo_res(
    a_client_order_id dm_idb
)
returns (
    row_cnt int, -- number of rows added to tmp$shop_cart
    qty_sum dm_qty -- total on QTY field in tmp$shop_cart
)
as
    declare v_oper_invoice_add dm_idb;
    declare v_oper_retail_reserve dm_idb;
    declare v_oper_order_by_customer dm_idb;
    declare v_ware_id dm_idb;
    declare v_dd_id dm_idb;
    declare v_clo_qty_need_to_reserve dm_qty;
    declare v_this dm_dbobj = 'sp_fill_shopping_cart_clo_res';
begin
    -- Aux. SP: fills tmp$shopping_cart with data from client_order, but take in
    -- account only those amounts which still need to be reserved.

    execute procedure sp_add_perf_log(1, v_this, null, 'clo='||a_client_order_id);

    v_oper_invoice_add =  fn_oper_invoice_add();
    v_oper_order_by_customer = fn_oper_order_by_customer();
    v_oper_retail_reserve = fn_oper_retail_reserve();
    qty_sum = 0; -- out arg
    for
        select
            d.ware_id,
            d.id as dd_id, -- 22.09.2014: for processing in separate cursor in sp_make_qty_distr that used index on snd_op, rcv_op, snd_id
            sum(q.snd_qty) as clo_qty_need_to_reserve -- rest of init amount in client order that still needs to be reserved
        -- 16.09.2014 PLAN SORT (JOIN (D INDEX (FK_DOC_DATA_DOC_LIST), Q INDEX (QDISTR_SNDOP_RCVOP_SNDID_DESC)))
        -- (much faster than old: from qdistr where q.doc_id = :a_client_order_id and snd_op = ... and rcv_op = ...)
        from doc_data d
        LEFT -- !! force to fix plan with 'doc_data' as drive table, see CORE-4926
        join v_qdistr_source q on
             -- :: NB :: full match on index range scan must be here!
             q.ware_id = d.ware_id
             and q.snd_optype_id = :v_oper_order_by_customer
             and q.rcv_optype_id = :v_oper_retail_reserve
             and q.snd_id = d.id --- :: NB :: full match on index range scan must be here!
        where
            d.doc_id = :a_client_order_id
            and q.id is not null
        group by d.ware_id, d.id
    into v_ware_id, v_dd_id, v_clo_qty_need_to_reserve
    do begin
        insert into tmp$shopping_cart(
            id,
            snd_id, -- 22.09.2014: for handling qdistr in separate cursor wher storno_sub=2!
            snd_optype_id,
            rcv_optype_id,
            qty,
            storno_sub
        )
        values (
            :v_ware_id,
            :v_dd_id,
            :v_oper_invoice_add,
            :v_oper_retail_reserve,
            :v_clo_qty_need_to_reserve, -- :: NB :: this is the REST of initially ordered amount (i.e. LESS or equal to origin value in doc_data.qty for clo!)
            1
        );
        row_cnt = row_cnt + 1; -- out arg
        qty_sum = qty_sum + v_clo_qty_need_to_reserve; -- out arg
    end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'rc='||row_count );

    suspend; -- row_cnt, qty_sum

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'clo='||a_client_order_id,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_fill_shopping_cart_clo_res

-----------------------------------------

create or alter procedure sp_customer_reserve(
    a_client_order_id type of dm_idb default null,
    dbg integer default 0)
returns (
    doc_list_id type of dm_idb,
    client_order_id type of dm_idb,
    doc_data_id type of dm_idb,
    ware_id type of dm_idb,
    qty type of dm_qty,
    purchase type of dm_cost,
    retail type of dm_cost,
    qty_ord type of dm_qty,
    qty_avl type of dm_qty,
    qty_res type of dm_qty
)
as
    declare v_rows_added int;
    declare v_qty_sum dm_qty;
    declare v_dbkey dm_dbkey;
    declare v_agent_id type of dm_idb;
    declare v_raise_exc_on_nofind dm_sign;
    declare v_can_skip_order_clause dm_sign;
    declare v_find_using_desc_index dm_sign;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_this dm_dbobj = 'sp_customer_reserve';
begin

    -- Takes several wares, adds them into
    -- tmp$shopping_cart and and creates new document that reserves
    -- these wares for customer, in amount that is currently avaliable
    -- If parameter a_client_order_id is NOT_null then fill tmp$shopping_cart
    -- with wares from THAT client order rather than random choosen wares set
    -- Document 'customer_reserve' can appear in business chain in TWO places:
    -- 1) at the beginning (when customer comes to us and wants to buy some wares
    --    which we have just now);
    -- 2) after we accept invoice which has wares from client order - in that case
    --    we need to reserve wares for customer(s) as soon as possible and we do
    --    it in the same Tx with accepting invoice (==> this will be 'heavy' Tx).

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises exc`eption to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(
        1,
        v_this,
        null,
        iif( a_client_order_id is null, 'from avaliable remainders', 'for clo_id='||a_client_order_id )
    );

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    if ( a_client_order_id = -1 ) then -- create reserve from avaliable remainders
        a_client_order_id = null;
    else if ( a_client_order_id is null ) then
        begin
            v_raise_exc_on_nofind = 0;   -- do NOT raise exc if random seacrh will not find any record
            v_can_skip_order_clause = 0; -- do NOT skip `order by` clause in sp_get_random_id (if order by id DESC will be used!)
            v_find_using_desc_index = 0; -- 22.09.2014; befo: 1; -- use 'order by id DESC' (11.09.2014)
            -- First of all try to search among client_orders which have
            -- at least one row with NOT_fully reserved ware.
            -- Call sp_get_random_id with arg NOT to raise exc`eption if
            -- it will not found such documents:
            select id_selected
            from
                sp_get_random_id(
                    'v_random_find_clo_res',
                    'v_min_id_clo_res',
                    'v_max_id_clo_res',
                    :v_raise_exc_on_nofind,
                    :v_can_skip_order_clause,
                    :v_find_using_desc_index
                )
            into
                a_client_order_id;
        end

    v_qty_sum = 0;
    while (1=1) do begin -- ...............   m a i n   l o o p  .................
  
        delete from tmp$shopping_cart where 1=1;

        if (a_client_order_id is null) then -- 'common' reserve, NOT related to client order
        begin
            -- ######  R E S E R V E    A V A L I A B L E    W A R E S  #####
            execute procedure sp_fill_shopping_cart( fn_oper_retail_reserve() )
            returning_values v_rows_added, v_qty_sum;
            
            -- select customer, random:
            v_agent_id = fn_get_random_customer();
        end
        else begin -- reserve based on client order: scan its wares which still need to be reserved
        
            -- ##########   R E S E R V E    F O R    C L I E N T    O R D E R  ######
            
            select h.rdb$db_key, h.agent_id
            from doc_list h
            where h.id = :a_client_order_id
            into v_dbkey, v_agent_id;
            
            if (v_dbkey is null) then exception ex_no_doc_found_for_handling using('doc_list', :a_client_order_id);

            -- fill tmp$shopping_cart with client_order data
            -- (NB: sp_make_qty_storno will put in reserve only those amounts
            -- for which there are at least one row in qdistr, so we can put in
            -- tmp$shopp_cart ALL rows from client order and no filter them now):
            execute procedure sp_fill_shopping_cart_clo_res( :a_client_order_id )
            returning_values v_rows_added, v_qty_sum;

        end -- a_client_order_id order NOT null
  
        if (dbg=1) then leave;
    
        -- 1. Find rows in QDISTR (and silently try to LOCK them) which can provide required
        --    amounts in tmp$shopping_cart, in FIFO manner.
        -- 2. Perform "STORNING" of them (moves these rows from QDISTR to QSTORNED)
        -- 3. Create new document: header (doc_list) and detalization (doc_data).
        if ( v_qty_sum > 0 ) then
        begin
            execute procedure sp_make_qty_storno(
                fn_oper_retail_reserve()
                ,v_agent_id
                ,fn_doc_open_state()
                ,a_client_order_id
                ,v_rows_added
                ,v_qty_sum
            ) returning_values doc_list_id; -- out arg
        end
        leave;

    end -- while (1=1) -- ...............   m a i n   l o o p  .................

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, coalesce(doc_list_id,'<null>') );

    if ( dbg=4 ) then exit;

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);
    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    if ( v_ibe = 1 ) then
        v_stt = 'select v.base_doc_id, v.doc_data_id, v.ware_id, v.qty,v.cost_purchase, v.cost_retail'
                ||',v.qty_ord ,v.qty_avl ,v.qty_res'
                ||' from v_doc_detailed v where v.doc_id = :x';
    else
        v_stt = 'select h.base_doc_id, d.id, d.ware_id, d.qty,d.cost_purchase, d.cost_retail'
                ||',null      ,null      ,null     '
                ||' from doc_data d join doc_list h on d.doc_id = h.id where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement (v_stt) ( x := :doc_list_id )
    into
         client_order_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_ord
        ,qty_avl
        ,qty_res
    do
        suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^ -- sp_customer_reserve

--------------------------------------------------------------------------------

create or alter procedure sp_cancel_customer_reserve(
    a_selected_doc_id type of dm_idb default null,
    a_skip_lock_attempt dm_sign default 0 -- 1==> do NOT call sp_lock_selected_doc because this doc is already locked (see call from sp_cancel_adding_invoice)
)
returns (
    doc_list_id type of dm_idb, -- id of new created reserve doc
    client_order_id type of dm_idb, -- id of client order (if current reserve was created with link to it)
    doc_data_id type of dm_idb, -- id of created records in doc_data
    ware_id type of dm_idb, -- id of wares that we resevre for customer
    qty type of dm_qty, -- amount that we can reserve (not greater than invnt_saldo.qty_avl)
    purchase type of dm_cost, -- cost in purchasing prices
    retail type of dm_cost, -- cost in retailing prices
    qty_ord type of dm_qty, -- new value of corresp. row
    qty_avl type of dm_qty, -- new value of corresp. row
    qty_res type of dm_qty -- new value of corresp. row
) as
    declare v_linked_client_order type of dm_idb;
    declare v_stt varchar(255);
    declare v_ibe smallint;
    declare v_dummy bigint;
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;
    declare v_this dm_dbobj = 'sp_cancel_customer_reserve';
begin

    -- Randomly chooses customer reserve (which is NOT yet sold) and CANCEL it
    -- by REMOVING all its data + record in docs header table. It occurs when
    -- we mistakenly created such reserve and now have to cancel this operation.
    -- All wares which were in such reserve will be enabled to be reserved for
    -- other customer - due to removing info about storning that was done before
    -- when this customer reserve was created.

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises ex`ception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);

    -- Choose random doc of corresponding kind.
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            ( select id_selected from
                              sp_get_random_id(
                                'v_cancel_customer_reserve' -- a_view_for_search
                                ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                ,:c_raise_exc_when_no_found
                                ,:c_can_skip_order_clause
                                            )
                            )
                          );

    -- Try to LOCK just selected doc, raise exc if can`t:
    if (  NOT (a_selected_doc_id is NOT null and a_skip_lock_attempt = 1) ) then
        execute procedure sp_lock_selected_doc( doc_list_id, 'v_cancel_customer_reserve', a_selected_doc_id);

    select
        h.base_doc_id
    from doc_list h
    where
        h.id = :doc_list_id
    into
        v_linked_client_order; -- not null ==> this reserve was filled with wares from client order

    -- 17.07.2014: add cond for indexed scan to minimize fetches when multiple
    -- calls of this SP from sp_cancel_adding_invoice:
    delete from tmp$result_set r where r.doc_id = :doc_list_id;

    -- save data which is to be deleted (NB! this action became MANDATORY for
    -- checking in srv_find_qd_qs_mism, do NOT delete it!):
    insert into tmp$result_set(
        doc_id,
        base_doc_id,
        doc_data_id,
        ware_id,
        qty,
        cost_purchase,
        cost_retail
    )
    select
        :doc_list_id,
        :v_linked_client_order,
        d.id,
        d.ware_id,
        d.qty,
        d.cost_purchase,
        d.cost_retail
    from doc_data d
    where d.doc_id = :doc_list_id; -- customer reserve which is to be deleted now

    -- Remove selected customer reserve.
    -- Trigger d`oc_list_biud will (only for deleting doc or updating it's state):
    -- 1) call s`p_kill_qty_storno that returns rows from Q`Storned to Q`distr 
    -- Trigger d`oc_list_aiud will:
    -- 1) add rows in table i`nvnt_turnover_log (log to be processed by SP s`rv_make_invnt_saldo)
    -- 2) call s`p_multiply_rows_for_pdistr, s`p_make_cost_storno or s`p_kill_cost_storno, s`p_add_money_log
    delete from doc_list h where h.id = :doc_list_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||doc_list_id);

    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    v_stt = 'select r.doc_id,r.base_doc_id,r.doc_data_id,r.ware_id,r.qty,r.cost_purchase,r.cost_retail';
    if ( v_ibe = 1 ) then
       v_stt = v_stt || ' ,n.qty_ord,       n.qty_avl,       n.qty_res';
    else
       v_stt = v_stt || ' ,null as qty_ord, null as qty_avl, null as qty_res';

    v_stt = v_stt ||' from tmp$result_set r';
    if ( v_ibe = 1 ) then
       v_stt = v_stt || ' left join v_saldo_invnt n on r.ware_id = n.ware_id';

    -- 17.07.2014: add cond for indexed scan to minimize fetches when multiple
    -- calls of this SP from sp_cancel_adding_invoice:
    v_stt = v_stt || ' where r.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement (v_stt) ( x := :doc_list_id)
    into
        doc_list_id
        ,client_order_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_ord
        ,qty_avl
        ,qty_res
    do
        suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^ -- sp_cancel_customer_reserve

-----------------------------------------

create or alter procedure sp_cancel_write_off(
    a_selected_doc_id type of dm_idb default null,
    a_skip_lock_attempt dm_sign default 0 -- 1==> do NOT call sp_lock_selected_doc because this doc is already locked (see call from sp_cancel_adding_invoice)
)
returns (
    doc_list_id type of dm_idb, -- id of invoice being added to stock
    client_order_id type of dm_idb, -- id of client order (if current reserve was created with link to it)
    doc_data_id type of dm_idb, -- id of created records in doc_data
    ware_id type of dm_idb, -- id of wares that we will get from supplier
    qty type of dm_qty,
    purchase type of dm_cost,
    retail type of dm_cost,
    qty_avl type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_res type of dm_qty,  -- new value of corresponding row in invnt_saldo
    qty_out type of dm_qty  -- new value of corresponding row in invnt_saldo
)
as
    declare v_dummy bigint;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_agent_id type of dm_idb;
    declare v_linked_client_order type of dm_idb;
    declare v_this dm_dbobj = 'sp_cancel_write_off';
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;
begin

    -- Randomly chooses waybill (ex. customer reserve after it was sold) and
    -- MOVES ("returns") it back to state "customer reserve" thus cancelling
    -- write-off operation that was previously done with these wares.
    -- All wares which were in such waybill will be returned back on stock and
    -- will be reported as 'reserved'. So, we only change the STATE of document
    -- rather its content.
    -- Total cost of realization will be added (INSERTED) into money_turnover_log table
    -- with "-" sign to be gathered later in service sp_make_money_saldo
    -- that calculates balance of contragents.

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises ex`ception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);

    -- Choose random doc of corresponding kind ("closed" customer reserve)
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            ( select id_selected from
                              sp_get_random_id(
                                'v_cancel_write_off' -- a_view_for_search
                                ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                ,:c_raise_exc_when_no_found
                                ,:c_can_skip_order_clause
                                              )
                            )
                          );

    -- Try to LOCK just selected doc, raise exc if can`t:
    if (  NOT (a_selected_doc_id is NOT null and a_skip_lock_attempt = 1) ) then
        execute procedure sp_lock_selected_doc( doc_list_id, 'v_cancel_write_off', a_selected_doc_id);

    -- Change STATE of document back to "Reserve".
    -- Trigger doc_list_biud will (only for deleting doc or updating it's state):
    -- 1) call s`p_kill_qty_storno that returns rows from Q`Storned to Q`distr 
    -- Trigger doc_list_aiud will:
    -- 1) add rows in table i`nvnt_turnover_log (log to be processed by SP s`rv_make_invnt_saldo)
    -- 2) call s`p_multiply_rows_for_pdistr, s`p_make_cost_storno or s`p_kill_cost_storno, s`p_add_money_log
    update doc_list h
    set
        h.state_id = fn_doc_open_state(), -- return to prev. docstate
        h.optype_id = fn_oper_retail_reserve(), -- return to prev. optype
        dts_fix = null,
        dts_clos = null
    where
        h.id = :doc_list_id
    returning
        h.base_doc_id
    into
        client_order_id; -- out arg

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||doc_list_id);

    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    v_stt = 'select d.doc_id, d.ware_id, d.qty, d.cost_purchase, d.cost_retail';
    if ( v_ibe = 1 ) then
       v_stt = v_stt || ',d.doc_data_id ,d.qty_avl ,d.qty_res  ,d.qty_out from v_doc_detailed d';
    else
       v_stt = v_stt || ',d.id         ,null      ,null       ,null      from doc_data d';

    v_stt = v_stt || ' where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement (v_stt) ( x := :doc_list_id )
    into
        doc_list_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,doc_data_id
        ,qty_avl
        ,qty_res
        ,qty_out
    do
        suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_cancel_write_off

-------------------------------------------------------------------------------

create or alter procedure sp_get_clo_for_invoice( a_invoice_doc_id dm_idb )
returns (
    clo_doc_id type of dm_idb,
    clo_agent_id type of dm_idb -- 23.07.2014
)
as
    declare v_dbkey dm_dbkey;
    declare v_qty_acc dm_qty;
    declare v_qty_sup type of dm_qty;
    declare v_snd_qty dm_qty;
    declare v_qty_clo_still_not_reserved dm_qty;
    declare v_clo_doc_id dm_idb;
    declare v_clo_agent_id dm_idb;
    declare v_ware_id dm_idb;
    declare v_cnt int = 0;
    declare v_this dm_dbobj = 'sp_get_clo_for_invoice';
    declare v_oper_order_by_customer dm_idb;
    declare v_oper_retail_reserve dm_idb;
begin

    -- Aux SP: find client orders which have at least one unit of amount of
    -- some ware that still not reserved for customer.
    -- This SP is called when we finish checking invoice data and move invoice
    -- to state "Accepted". We need then find for immediate RESERVING such
    -- wares which customers waiting for.

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    --?! 06.02.2015 2020, performance affect ?
    v_oper_order_by_customer =  fn_oper_order_by_customer();
    v_oper_retail_reserve = fn_oper_retail_reserve();

    delete from tmp$dep_docs d where d.base_doc_id = :a_invoice_doc_id;

    -- :: NB :: We need handle rows via CURSOR here because of immediate leave
    -- from cursor when limit (invoice doc_data.qty as v_qty_sup) will be exceeded
    -- FB 3.0 analitycal function sum()over(order by) which get running total
    -- is inefficient here (poor performance)
    for
        select d.ware_id, d.qty
        from doc_data d
        where d.doc_id = :a_invoice_doc_id -- invoice which we are closing now
        into v_ware_id, v_qty_sup
    do begin
        v_qty_acc = 0;

        -- Gather REMAINDER of initial amount in ALL client orders
        -- that still not yet reserved.
        -- 05.09.2015. Note: we have to stop scrolling on QDistr for each ware
        -- from invoice as soon as number of scrolled records will be >= v_qty_sup
        -- (because we can`t put in reserve more than we got from supplier;
        -- also because of performance: there are usially **LOT** of rows in QDistr
        -- for the same value of {ware, snd_op, rcv_op})
        for
        select
            q.doc_id as clo_doc_id, -- id of customer order
            q.snd_qty as clo_qty -- always = 1 (in current implementation)
        from v_qdistr_source q
        where
            -- :: NB :: PARTIAL match on index range scan will be here.
            -- For that reason we have to STOP scrolling as soon as possible!
            q.ware_id = :v_ware_id
            and q.snd_optype_id = :v_oper_order_by_customer
            and q.rcv_optype_id = :v_oper_retail_reserve
            and not exists(
                select * from tmp$dep_docs t
                where
                    t.base_doc_id = :a_invoice_doc_id
                    and t.dependend_doc_id = q.doc_id
                -- prevent from building index bitmap (has effect only in 3.0; do NOT repeat in 2.5!):
                order by t.base_doc_id, t.dependend_doc_id
            )
        order by q.ware_id, q.snd_optype_id, q.rcv_optype_id, q.snd_id -- ==> 3.0: plan_order, avoid bild bitmap
        into v_clo_doc_id, v_snd_qty
        do begin
            v_qty_acc = v_qty_acc + v_snd_qty;
            v_cnt = v_cnt + 1;

            update or insert into tmp$dep_docs(
                base_doc_id,
                dependend_doc_id)
            values (
                :a_invoice_doc_id,
                :v_clo_doc_id)
            matching(base_doc_id, dependend_doc_id);

            if ( v_qty_acc >= v_qty_sup ) then leave; -- we can`t put in reserve more than we got from supplier
        when any do -- added 10.09.2014: strange 'concurrent transaction' error occured on GTT!
            -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
            -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
            -- catched it's kind of exception!
            -- 1) tracker.firebirdsql.org/browse/CORE-3275
            --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
            -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
            begin
                if ( NOT fn_is_uniqueness_trouble(gdscode) ) then exception;
            end
        end

    end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||a_invoice_doc_id||', gather_qd_rows='||v_cnt);

    for
        select f.dependend_doc_id, h.agent_id
        from tmp$dep_docs f
        join doc_list h on
                f.dependend_doc_id = h.id
                and h.optype_id = :v_oper_order_by_customer -- 31.07.2014: exclude cancelled customer orders!
        where f.base_doc_id = :a_invoice_doc_id
        -- not needed! >>> group by f.dependend_doc_id, h.agent_id
        into clo_doc_id, clo_agent_id
    do
        suspend;

when any do  -- added 10.09.2014: strange 'concurrent transaction' error occured on INSERT!
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(a_invoice_doc_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end


^  -- sp_get_clo_for_invoice

-------------------------------------------------------------------------------

create or alter procedure sp_add_invoice_to_stock(
    a_selected_doc_id type of dm_idb default null,
    a_cancel_mode dm_sign default 0,
    a_skip_lock_attempt dm_sign default 0, -- 1==> do NOT call sp_lock_selected_doc because this doc is already locked (see call from s`p_cancel_supplier_order)
    dbg int default 0
)
returns (
    doc_list_id type of dm_idb, -- id of invoice being added to stock
    agent_id type of dm_idb, -- id of supplier
    doc_data_id type of dm_idb, -- id of created records in doc_data
    ware_id type of dm_idb, -- id of wares that we will get from supplier
    qty type of dm_qty, -- amount that supplier will send to us
    purchase type of dm_cost, -- how much we must pay to supplier for this ware
    qty_sup type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_avl type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_res type of dm_qty,  -- new value of corresponding row in invnt_saldo
    res_ok int, -- number of successfully created reserves for client orders
    res_err int, -- number of FAULTS when attempts to create reserves for client orders
    res_nul int -- 4debug: number of mismatches between estimated and actually created reserves
)
as
    declare v_dummy bigint;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_info dm_info;
    declare v_new_doc_state type of dm_idb;
    declare v_old_oper_id type of dm_idb;
    declare v_new_oper_id type of dm_idb;
    declare v_client_order type of dm_idb;
    declare v_linked_reserve_id type of dm_idb;
    declare v_linked_reserve_state type of dm_idb;
    declare v_view_for_search dm_dbobj;
    declare v_this dm_dbobj = 'sp_add_invoice_to_stock';
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;

    declare function fn_internal_enable_reserving() returns boolean deterministic as
    begin
      return rdb$get_context('USER_SESSION', 'ENABLE_RESERVES_WHEN_ADD_INVOICE')='1';
    end

begin

    -- This SP implements TWO tasks (see parameter `a_cancel_mode`):
    -- 1) MOVES invoice to the state "ACCEPTED" after we check its content;
    -- 2) CANCEL previously accepted invoice and MOVES it to the state "TO BE CHECKED".
    -- For "1)" it will also find all client orders which have at least one unit
    -- of amount that still not reserved and CREATE customer reserve(s).
    -- Total cost of invoice will be added (INSERTED) into money_turnover_log table
    -- with "+" sign to be gathered later in service sp_make_money_saldo that
    -- calculates balance of contragents.
    -- For "2)" it will find all customer reserves and waybills ('closed reserves')
    -- and firstly CANCEL all of them and, if no errors occur, will then cancel
    -- currently selected invoice.
    -- Total cost of cancelled invoice will be added (INSERTED) into money_turnover_log table
    -- with "-" sign to be gathered later in service sp_make_money_saldo that
    -- calculates balance of contragents.
    -- ::: NB-1 ::: This SP supresses lock-conflicts which can be occur when trying
    -- to CREATE customer reserves in module sp_make_qty_storno which storning amounts.
    -- In such case amount will be stored in 'avaliable' remainder.
    -- ::: NB-2 ::: This SP does NOT supress lock-conflicts when invoice is to be
    -- CANCELLED (i.e. moved back to state 'to be checked') - otherwise we get
    -- negative values in 'reserved' or 'sold' kinds of stock remainder.

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises ex`ception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- input arg a_cancel_mode = 0 ==> ADD invoice to stock and 'fix' it;
    --                   otherwise ==> CANCEL adding and return to 'open' state
    ----------------------------------------------------------------------------
    -- add to performance log timestamp about start/finish this unit:
    if ( a_cancel_mode = 1 ) then v_this = 'sp_cancel_adding_invoice';

    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    -- check that special context var EXISTS otherwise raise exc:
    execute procedure sp_check_ctx('USER_SESSION', 'ENABLE_RESERVES_WHEN_ADD_INVOICE');
    
    v_new_doc_state = iif( a_cancel_mode = 0, fn_doc_fix_state(),  fn_doc_open_state() );
    v_old_oper_id = iif( a_cancel_mode = 0, fn_oper_invoice_get(), fn_oper_invoice_add() );
    v_new_oper_id = iif( a_cancel_mode = 0, fn_oper_invoice_add(), fn_oper_invoice_get() );
    v_view_for_search = iif( a_cancel_mode = 0, 'v_add_invoice_to_stock', 'v_cancel_adding_invoice' );
    
    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);

    -- Choose random doc of corresponding kind.
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            (select id_selected
                             from sp_get_random_id( :v_view_for_search -- a_view_for_search
                                                    ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                                    ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                                    ,:c_raise_exc_when_no_found
                                                    ,:c_can_skip_order_clause
                                                  )
                            )
                          );


    execute procedure sp_upd_in_perf_log(v_this, null, 'doc_id='||doc_list_id); -- 06.07.2014, 4debug

    -- Try to LOCK just selected doc, raise exc if can`t:
    if (  NOT (a_selected_doc_id is NOT null and a_skip_lock_attempt = 1) ) then
        execute procedure sp_lock_selected_doc( doc_list_id, v_view_for_search, a_selected_doc_id);

    res_ok = 0;
    res_err = 0;
    res_nul = 0;

    while (1=1) do begin -- ................  m a i n    l o o p   ...............

        if ( a_cancel_mode = 1 ) then
        begin
            -- search all RESERVES (including those that are written-off) which
            -- stornes some amounts from currently selected invoice and lock them
            -- (add to tmp$dep_docs.dependend_doc_id)
            execute procedure sp_lock_dependent_docs( :doc_list_id, :v_old_oper_id );
            -- result: tmp$dep_docs.dependend_doc_id filled by ID of all locked
            -- RESERVES which depends on currently selected invoice.
            -- Extract set of reserve docs that storned amounts from current
            -- invoice and cancel them:
            for
                select d.dependend_doc_id, d.dependend_doc_state
                from tmp$dep_docs d
                where d.base_doc_id = :doc_list_id
            into
                v_linked_reserve_id, v_linked_reserve_state
            do begin
                -- if we are here then ALL dependend docs have been SUCCESSFULLY locked.
                if ( v_linked_reserve_state <> fn_doc_open_state() ) then
                    select count(*) from sp_cancel_write_off( :v_linked_reserve_id, 1 ) into v_dummy;

                select count(*) from sp_cancel_customer_reserve(:v_linked_reserve_id, 1 ) into v_dummy;

                res_ok = res_ok + 1;

                -- do NOT supress any lock_conflict ex`ception here
                -- otherwise get negative remainders!

            end
        end -- block for CANCELLING mode

        -- Change info in doc header for INVOICE.
        -- 1. trigger d`oc_list_biud will call sp_kill_qty_storno which:
        --    update qdistr.snd_optype_id (or rcv_optype_id)
        --    where qd.snd_id = doc_data.id or qd.rcv_id = doc_data.id
        -- 2. trigger d`oc_list_aiud will:
        -- 2.1 add rows into invnt_turnover_log
        -- 2.2 add rows into money_turnover_log
        update doc_list h
        set h.optype_id = :v_new_oper_id,  -- iif( a_cancel_mode = 0, fn_oper_invoice_add(), fn_oper_invoice_get() );
            h.state_id = :v_new_doc_state, -- iif( :a_cancel_mode = 0 , :fn_doc_clos_state , :v_new_doc_state),
            dts_fix = iif( :a_cancel_mode = 0, 'now', null )
        where h.id = :doc_list_id;

        if (dbg=1) then leave;

        -- build unique list of client orders which still need to reserve some wares
        -- and create for each item of this list new reserve that is linked to client_order.
        v_client_order = null;
        if (a_cancel_mode = 0) then  -- create reserve docs (avaliable remainders exists after adding this invoice)
        begin
        
            if (dbg=3) then leave;

            if (fn_internal_enable_reserving() ) then
            begin
                for
                    select p.clo_doc_id
                    from sp_get_clo_for_invoice( :doc_list_id  ) p
                    where not exists(
                        select * from v_our_firm v
                        where v.id = p.clo_agent_id
                        -- 3.0: fixed 16.12.2014, revision 60368
                        -- "Postfix for CORE-1550 Unnecessary index scan happens
                        --- when the same index is mapped to both WHERE and ORDER BY clauses."
                        order by v.id -- <<< can do this since 16.12.2014
                    )
                    into v_client_order
                do begin
                    -- reserve immediatelly all avaliable wares for each found client order:
                    select min(doc_list_id) from sp_customer_reserve( :v_client_order, iif(:dbg=4, 2, null) )
                    into v_linked_reserve_id;

                    if (  v_linked_reserve_id is null ) then
                        res_nul = res_nul + 1;
                    else
                        res_ok = res_ok + 1;

                when any do
                    -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
                    -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
                    -- catched it's kind of exception!
                    -- 1) tracker.firebirdsql.org/browse/CORE-3275
                    --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
                    -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
                    begin
                        if ( fn_is_lock_trouble(gdscode) ) then
                            begin
                                execute procedure sp_add_to_abend_log(
                                    'can`t create res',
                                    gdscode,
                                    'clo_id='||coalesce(v_client_order, '<null>'),
                                    v_this
                                );
                                res_err = res_err + 1;
                            end
                        else
                            begin
                                execute procedure sp_add_to_abend_log('', gdscode, 'doc_id='||doc_list_id, v_this );
                                --########
                                exception;  -- ::: nb ::: anonimous but in when-block!
                            end
                    end
                end -- cursor select clo_doc_id from sp_get_clo_for_invoice( :doc_list_id  )
            end -- ENABLE_RESERVES_WHEN_ADD_INVOICE ==> '1'
        end -- a_cancel_mode = 0
        leave;
    end -- while (1=1)  -- ................  m a i n    l o o p   ...............

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'dh='||doc_list_id, res_ok, res_nul);

    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    if (  v_ibe = 1 ) then
        v_stt = 'select v.agent_id, v.doc_data_id, v.ware_id, v.qty, v.cost_purchase, v.qty_sup, v.qty_avl, v.qty_res'
                ||' from v_doc_detailed v where v.doc_id = :x';
    else
        v_stt = 'select h.agent_id, d.id,          d.ware_id, d.qty, d.cost_purchase, null,     null,       null '
                ||' from doc_data d join doc_list h on d.doc_id = h.id where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement (v_stt) ( x := :doc_list_id )
     into
         agent_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,qty_sup
        ,qty_avl
        ,qty_res
    do
    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^ -- end of sp_add_invoice_to_stock

create or alter procedure sp_cancel_adding_invoice(
    a_selected_doc_id type of dm_idb default null,
    a_skip_lock_attempt dm_sign default 0, -- 1==> do NOT call sp_lock_selected_doc because this doc is already locked (see call from s`p_cancel_supplier_order)
    dbg int default 0
)
returns (
    doc_list_id type of dm_idb, -- id of invoice being added to stock
    agent_id type of dm_idb, -- id of supplier
    doc_data_id type of dm_idb, -- id of created records in doc_data
    ware_id type of dm_idb, -- id of wares that we will get from supplier
    qty type of dm_qty, -- amount that supplier will send to us
    purchase type of dm_cost, -- how much we must pay to supplier for this ware
    qty_sup type of dm_qty, -- new value of corresponding row in invnt
    qty_avl type of dm_qty, -- new value of corresponding row in invnt
    qty_res type of dm_qty,  -- new value of corresponding row in invnt_saldo
    res_ok int, -- number of successfully CANCELLED reserves for client orders
    res_err int -- number of FAULTS when attempts to CANCEL reserves for client orders
)
as
    declare v_dummy bigint;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_this dm_dbobj = 'sp_cancel_adding_invoice';
begin

    -- MOVES invoice from state 'accepted' to state 'to be checked'.
    -- Delegates all this work to sp_add_invoice_to_stock.

    -- add to performance log timestamp about start/finish this unit:
    -- no need, see s`p_add_invoice:    execute procedure s`p_add_to_perf_log(v_this);

    select min(doc_list_id), min(res_ok), min(res_err)
    from sp_add_invoice_to_stock(
             :a_selected_doc_id,
             1, -- <<<<<<<<< sign to CANCEL document <<<<<
             :a_skip_lock_attempt,
             :dbg
         )
    into doc_list_id, res_ok, res_err;

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);

    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    if ( v_ibe = 1 ) then
        v_stt = 'select v.agent_id, v.doc_data_id, v.ware_id, v.qty, v.cost_purchase, v.qty_sup, v.qty_avl, v.qty_res'
                ||' from v_doc_detailed v where v.doc_id = :x';
    else
        v_stt = 'select h.agent_id, d.id,          d.ware_id, d.qty, d.cost_purchase, null,      null,      null'
                ||' from doc_data d join doc_list h on d.doc_id = h.id where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement (v_stt) ( x := :doc_list_id )
    into
         agent_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,qty_sup
        ,qty_avl
        ,qty_res
    do
        suspend;

    -- add to performance log timestamp about start/finish this unit:
    -- no need, see s`p_add_invoice:    execute procedure s`p_add_to_perf_log(v_this, null, 'doc_id='||doc_list_id);
when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end -- sp_cancel_adding_invoice

^
--------------------------------------------------------------------------------

create or alter procedure sp_cancel_supplier_order(
    a_selected_doc_id type of dm_idb default null)
returns (
    doc_list_id type of dm_idb,
    agent_id type of dm_idb,
    doc_data_id type of dm_idb,
    ware_id type of dm_idb,
    qty type of dm_qty, -- amount that we ordered for client
    purchase type of dm_cost, -- purchasing cost for qty
    retail type of dm_cost, -- retail cost
    qty_clo type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_ord type of dm_qty -- new value of corresponding row in invnt_saldo
)
as
    declare v_dummy bigint;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_info dm_info = '';
    declare v_linked_invoice_id bigint;
    declare v_linked_invoice_state bigint;
    declare v_linked_reserve_id bigint;
    declare v_linked_reserve_state bigint;
    declare v_this dm_dbobj = 'sp_cancel_supplier_order';
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;
begin

    -- Randomly chooses our order to supplier and CANCEL it by REMOVING all its
    -- data + record in docs header table. It occurs when we mistakenly created
    -- such order and now have to cancel this operation.
    -- All wares which were in such supplier order will be enabled again
    -- to be included in new (another) order which we create after this.
    -- ::: NB :::
    -- Before cancelling supplier order we need to find all INVOICES which have
    -- at least one unit of amounts that was participated in storning process of
    -- currently selected order. All these invoices need to be:
    -- 1) moved from state 'accepted' to state 'to be checked' (if need);
    -- 2) cancelled at all (i.e. removed from database).
    -- Because each 'accepted' invoice can be cancelled only when all customer
    -- reserves and waybills are cancelled first, we need, in turn, to find all
    -- these documents and cancel+remove them.
    -- For that reason this SP is most 'heavy' vs any others: it can fail with
    -- 'lock-conflict' up to 75% of calls in concurrent environment.

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises ex`ception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);

    -- Choose random doc of corresponding kind.
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            (select id_selected from
                              sp_get_random_id( 'v_cancel_supplier_order' -- a_view_for_search
                                                ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                                ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                                ,:c_raise_exc_when_no_found
                                                ,:c_can_skip_order_clause
                                              )
                            )
                          );
    v_info = 'dh='||doc_list_id;
    -- upd. log with doc id whic is actually handling now:
    --execute procedure sp_upd_in_perf_log( v_this, null, 'dh='||doc_list_id);

    -- Try to LOCK just selected doc, raise exc if can`t:
    execute procedure sp_lock_selected_doc( doc_list_id, 'v_cancel_supplier_order', a_selected_doc_id);

    -- Since 08.08.2014: first get and lock *ALL* dependent docs - both invoices and reserves
    -- Continue handling of them only after we get ALL locks!
    -- 1. lock all INVOICES that storned amounts from currently selected supp_order:
    execute procedure sp_lock_dependent_docs( :doc_list_id, fn_oper_order_for_supplier() );
    -- result: tmp$dep_docs.dependend_doc_id filled by ID of all locked dependent invoices

    -- 2. for each of invoices search all RESERVES (including those that are written-off)
    -- and also lock them (add to tmp$dep_docs.dependend_doc_id)
    for
        select
            d.dependend_doc_id as linked_invoice_id
        from tmp$dep_docs d
        where d.base_doc_id = :doc_list_id
        order by d.base_doc_id+0
        into v_linked_invoice_id
    do begin
        execute procedure sp_lock_dependent_docs(:v_linked_invoice_id, fn_oper_invoice_add());
    end
    -- result: tmp$dep_docs.dependend_doc_id filled by ID of all locked RESERVES
    -- which depends on invoices.

    -- 3. Scan tmp$dep_docs filtering only RESERVES and cancel them
    -- (do NOT delegate this job to sp_cancel_adding_invoice...)
    for
        select d.dependend_doc_id, d.dependend_doc_state
        from tmp$dep_docs d
        where d.base_doc_id <> :doc_list_id
        group by 1,2 -- ::: NB ::: one reserve can depends on SEVERAL invoices!
        into v_linked_reserve_id, v_linked_reserve_state
    do begin
        if ( v_linked_reserve_state <> fn_doc_open_state() ) then
            -- a_skip_lock_hdr = 1  ==> do NOT try to lock doc header, it was ALREADY locked in sp_lock_dependent_docs
            select count(*) from sp_cancel_write_off( :v_linked_reserve_id, 1 ) into v_dummy;

        -- a_skip_lock_hdr = 1  ==> do NOT try to lock doc header, it was ALREADY locked in sp_lock_dependent_docs
        select count(*) from sp_cancel_customer_reserve(:v_linked_reserve_id, 1 ) into v_dummy;

        -- do NOT supress any lock_conflict ex`ception here
        -- otherwise get negative remainders!
    end

    -- 4. Scan tmp$dep_docs filtering only INVOICES and cancel them:
    for
        select d.dependend_doc_id, d.dependend_doc_state
        from tmp$dep_docs d
        where d.base_doc_id = :doc_list_id
        into v_linked_invoice_id, v_linked_invoice_state
    do begin
        if ( v_linked_invoice_state <> fn_doc_open_state() ) then
            -- a_skip_lock_hdr = 1  ==> do NOT try to lock doc header, it was ALREADY locked in sp_lock_dependent_docs
            select count(*) from sp_cancel_adding_invoice( :v_linked_invoice_id, 1 ) into v_dummy;

        -- a_skip_lock_hdr = 1  ==> do NOT try to lock doc header, it was ALREADY locked in sp_lock_dependent_docs
        select count(*) from sp_cancel_supplier_invoice( :v_linked_invoice_id, 1 ) into v_dummy;

        -- do NOT supress any lock_conflict ex`ception here
        -- otherwise get negative remainders!

    end

    -- 17.07.2014: add cond for indexed scan to minimize fetches:
    delete from tmp$result_set r where r.doc_id = :doc_list_id;

    -- save data which is to be deleted (NB! this action became MANDATORY for
    -- checking in srv_find_qd_qs_mism, do NOT delete it!):
    insert into tmp$result_set( doc_id, agent_id, doc_data_id, ware_id, qty, cost_purchase, cost_retail)
    select
        :doc_list_id,
        h.agent_id,
        d.id,
        d.ware_id,
        d.qty,
        d.cost_purchase,
        d.cost_retail
    from doc_data d
    join doc_list h on d.doc_id = h.id
    where d.doc_id = :doc_list_id; -- supplier order which is to be removed now

    -- 1. Trigger doc_list_biud will (only for deleting doc or updating it's state)
    -- call s`p_kill_qty_storno that returns rows from Q`Storned to Q`distr.
    -- 2. FK cascade will remove records from table doc_data.
    -- 3. Trigger doc_list_aiud will:
    -- 3.1) add rows in table i`nvnt_turnover_log (log to be processed by SP s`rv_make_invnt_saldo)
    -- 3.2) call s`p_multiply_rows_for_pdistr, s`p_make_cost_storno or s`p_kill_cost_storno, s`p_add_money_log
    delete from doc_list h where h.id = :doc_list_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this,null, v_info);

    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    v_stt = 'select r.agent_id,r.doc_data_id,r.ware_id,r.qty,r.cost_purchase,r.cost_retail';
    if ( v_ibe = 1 ) then
        v_stt = v_stt ||' ,n.qty_clo,n.qty_ord'
                      ||' from tmp$result_set r left join v_saldo_invnt n on r.ware_id = n.ware_id';
    else
        v_stt = v_stt ||' ,null     ,null from tmp$result_set r';

    -- 17.07.2014: add cond for indexed scan to minimize fetches:
    v_stt = v_stt || ' where r.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement (v_stt) ( x := :doc_list_id )
    into
         agent_id
        ,doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_clo
        ,qty_ord
    do
        suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            v_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );
        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_cancel_supplier_order

--------------------------------------------------------------------------------

create or alter procedure sp_reserve_write_off(a_selected_doc_id type of dm_idb default null)
returns (
    doc_list_id type of dm_idb, -- id of customer reserve doc
    client_order_id type of dm_idb, -- id of client order (if current reserve was created with link to it)
    doc_data_id type of dm_idb, -- id of processed records in doc_data
    ware_id type of dm_idb, -- id of ware
    qty type of dm_qty, -- amount that is written-offf
    purchase type of dm_cost, -- cost in purchasing prices
    retail  type of dm_cost, -- cost in retailing prices
    qty_avl type of dm_qty, -- new value of corresponding row in invnt_saldo
    qty_res type of dm_qty,  -- new value of corresponding row in invnt_saldo
    qty_out type of dm_qty  -- new value of corresponding row in invnt_saldo
)
as
    declare v_linked_client_order type of dm_idb;
    declare v_ibe smallint;
    declare v_stt varchar(255);
    declare v_dummy bigint;
    declare v_this dm_dbobj = 'sp_reserve_write_off';
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;
begin

    -- Randomly choose customer reserve and MOVES it to the state 'sold',
    -- so this doc becomes 'waybill' (customer can take out his wares since
    -- that moment).
    -- Total cost of realization will be added (INSERTED) into money_turnover_log table
    -- with "+" sign to be gathered later in service sp_make_money_saldo
    -- that calculates balance of contragents.

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    v_ibe = iif( fn_remote_process() containing 'IBExpert', 1, 0);

    -- Choose random doc of corresponding kind.
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            (select id_selected from
                             sp_get_random_id( 'v_reserve_write_off' -- a_view_for_search
                                              ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                              ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                              ,:c_raise_exc_when_no_found
                                              ,:c_can_skip_order_clause
                                            )
                            )
                          );

    -- Try to LOCK just selected doc, raise exc if can`t:
    execute procedure sp_lock_selected_doc( doc_list_id, 'v_reserve_write_off', a_selected_doc_id);

    -- Change info in doc header for CUSTOMER RESERVE.
    -- 1. Trigger doc_list_biud will (only for deleting doc or updating it's state)
    -- call s`p_kill_qty_storno that returns rows from Q`Storned to Q`distr.
    -- 2. FK cascade will remove records from table doc_data.
    -- 3. Trigger doc_list_aiud will:
    -- 3.1) add rows in table i`nvnt_turnover_log (log to be processed by SP s`rv_make_invnt_saldo)
    -- 3.2) call s`p_multiply_rows_for_pdistr, s`p_make_cost_storno or s`p_kill_cost_storno, s`p_add_money_log
    update doc_list h
    set
        h.state_id = fn_doc_fix_state(), -- goto "next" docstate: 'waybill'
        h.optype_id = fn_oper_retail_realization() -- goto "next" optype ==> add row to money_turnover_log
    where h.id = :doc_list_id
    returning h.base_doc_id
    into client_order_id; -- out arg

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||doc_list_id);

    -- 16.07.2014: make ES more 'smart': we do NOT need any records from view
    -- v_doc_detailed (==> v_saldo_invnt!) if there is NO debug now (performance!)
    if ( v_ibe = 1 ) then
        v_stt = 'select v.doc_data_id,v.ware_id,v.qty,v.cost_purchase,v.cost_retail'
                ||',v.qty_avl,v.qty_res,v.qty_out'
                ||' from v_doc_detailed v where v.doc_id = :x';
    else
        v_stt = 'select d.id,d.ware_id,d.qty,d.cost_purchase,d.cost_retail'
                ||',null     ,null     ,null'
                ||' from doc_data d where d.doc_id = :x';

    -- final resultset (need only in IBE, for debug purposes):
    for
        execute statement (v_stt) ( x := :doc_list_id )
    into
         doc_data_id
        ,ware_id
        ,qty
        ,purchase
        ,retail
        ,qty_avl
        ,qty_res
        ,qty_out
    do
        suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end -- sp_reserve_write_off

^
-------------------------------------------------------------------------------
-- ###########################  P A Y M E N T S  ##############################
-------------------------------------------------------------------------------

create or alter procedure sp_payment_common(
    a_payment_oper dm_idb, -- fn_oper_pay_from_customer() or  fn_oper_pay_to_supplier()
    a_selected_doc_id type of dm_idb default null,
    a_total_pay type of dm_cost default null
)
returns (
    source_doc_id type of dm_idb, -- id of doc which is paid (reserve or invoice)
    agent_id type of dm_idb,
    current_pay_sum type of dm_cost
)
as
    declare v_stt varchar(255);
    declare v_source_for_random_id dm_dbobj;
    declare v_source_for_min_id dm_dbobj;
    declare v_source_for_max_id dm_dbobj;
    declare v_can_skip_order_clause smallint;
    declare v_find_using_desc_index dm_sign;
    declare view_to_search_agent dm_dbobj;
    declare v_non_paid_total type of dm_cost;
    declare v_round_to smallint;
    declare v_id bigint;
    declare v_dummy bigint;
    declare v_this dm_dbobj = 'sp_payment_common';
begin

    -- Aux SP - common for both payments from customers and our payments
    -- to suppliers.
    -- If parameter `a_selected_doc_id` is NOT null than we create payment
    -- that is LINKED to existent doc of realization (for customer) or incomings
    -- (for supplier). Otherwise this is ADVANCE payment.
    -- This SP tries firstly to find 'linked' document for payment an returns it
    -- in out argument 'source_doc_id' if it was found. Otherwise it only randomly
    -- choose agent + total cost of payment and return them.

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this,null);

    -- added 09.09.2014 due to new views for getting bounds & random find:
    v_source_for_random_id =
        decode( a_payment_oper,
                fn_oper_pay_from_customer(), 'v_random_find_non_paid_realizn',
                fn_oper_pay_to_supplier(),   'v_random_find_non_paid_invoice',
                'unknown_source'
              );

    v_source_for_min_id =
        decode( a_payment_oper,
                fn_oper_pay_from_customer(), 'v_min_non_paid_realizn',
                fn_oper_pay_to_supplier(),   'v_min_non_paid_invoice',
                null
              );

    v_source_for_max_id =
        decode( a_payment_oper,
                fn_oper_pay_from_customer(), 'v_max_non_paid_realizn',
                fn_oper_pay_to_supplier(),   'v_max_non_paid_invoice',
                null
              );

    v_can_skip_order_clause =
        decode( a_payment_oper,
                fn_oper_pay_from_customer(), 1,
                fn_oper_pay_to_supplier(),   1,
                0
              );
    v_find_using_desc_index =
        decode( a_payment_oper,
                fn_oper_pay_from_customer(), 1,
                fn_oper_pay_to_supplier(),   1,
                0
              );

    view_to_search_agent = iif( a_payment_oper = fn_oper_pay_from_customer(), 'v_all_customers', 'v_all_suppliers');
    v_round_to = iif( a_payment_oper = fn_oper_pay_from_customer(), -2, -3);

    if ( :a_selected_doc_id is null ) then
        begin
            select id_selected
            from sp_get_random_id(
                                   :v_source_for_random_id,
                                   :v_source_for_min_id,
                                   :v_source_for_max_id,
                                   0, -- 19.07.2014: 0 ==> do NOT raise exception if not able to find any ID in view :v_source_for_random_id
                                   :v_can_skip_order_clause, -- 17.07.2014: if = 1, then 'order by id' will be SKIPPED in statement inside fn
                                   :v_find_using_desc_index -- 11.09.2014
                                 )
            into source_doc_id;
            if ( source_doc_id is not null ) then
            begin
                select agent_id from doc_list h where h.id = :source_doc_id into agent_id;
            end
        end
    else
        select :a_selected_doc_id, h.agent_id
        from doc_list h
        where h.id = :a_selected_doc_id
        into source_doc_id, agent_id;

    if ( source_doc_id is not null ) then
        begin
            -- Find doc ID (with checking in view v_*** if need) and try to LOCK it.
            -- Raise exc if no found or can`t lock:
            -- ::: do NOT ::: execute procedure sp_lock_selected_doc( source_doc_id, 'doc_list', a_selected_doc_id);

            select h.agent_id from doc_list h where h.id = :a_selected_doc_id into agent_id;
            if ( agent_id is null ) then
                exception ex_no_doc_found_for_handling using('doc_list', a_selected_doc_id);
                -- no document found for handling in datasource = '@1' with id=@2

            if ( a_total_pay is null ) then
                begin
                    select sum( p.snd_cost ) from pdistr p where p.snd_id = :source_doc_id into v_non_paid_total;
                    current_pay_sum = round( v_non_paid_total, v_round_to );
                    if (current_pay_sum < v_non_paid_total) then
                    begin
                        current_pay_sum = current_pay_sum + power(10, abs(v_round_to));
                    end
                end
            else
                current_pay_sum = a_total_pay;

        end
    else -- source_doc_id is null
        begin
            select id_selected from sp_get_random_id( :view_to_search_agent, null, null, 0 ) into agent_id;
            if ( a_total_pay is null ) then
                begin
                    if (a_payment_oper = fn_oper_pay_from_customer() ) then
                        current_pay_sum = round(fn_get_random_cost('C_PAYMENT_FROM_CLIENT_MIN_TOTAL', 'C_PAYMENT_FROM_CLIENT_MAX_TOTAL'), v_round_to); -- round to hundreds
                    else
                        current_pay_sum = round(fn_get_random_cost('C_PAYMENT_TO_SUPPLIER_MIN_TOTAL', 'C_PAYMENT_TO_SUPPLIER_MAX_TOTAL'), v_round_to); -- round to thousands
                end
            else
                current_pay_sum = a_total_pay;
        end

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this,null,'doc_id='||coalesce(source_doc_id, '<null>') );

    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(source_doc_id, '<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_payment_common

--------------------------------------------------------------------------------

create or alter procedure sp_pay_from_customer(
    a_selected_doc_id type of dm_idb default null,
    a_total_pay type of dm_cost default null,
    dbg int default 0
)
returns (
    agent_id type of dm_idb,
    prepayment_id type of dm_idb, -- id of prepayment that was done here
    realization_id type of dm_idb, -- id of reserve realization doc that 'receives' this advance
    current_pay_sum type of dm_cost
)
as
    declare v_dbkey dm_dbkey;
    declare v_this dm_dbobj = 'sp_pay_from_customer';
begin

    -- Implementation for payment from customer to us.
    -- Randomly choose invoice that is not yet fully paid (by customer) and creates
    -- payment document (with sum that can be equal or LESS than rest of value
    -- that should be 100% paid).

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    execute procedure sp_payment_common(
        fn_oper_pay_from_customer(),
        a_selected_doc_id,
        a_total_pay
    ) returning_values realization_id, agent_id, current_pay_sum;

    -- add new record in doc_list (header)
    execute procedure sp_add_doc_list(
        null,
        fn_oper_pay_from_customer(),
        agent_id,
        null,
        null,
        0,
        current_pay_sum
    )
    returning_values :prepayment_id, :v_dbkey;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'payment_id='||prepayment_id);

    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(prepayment_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^  -- sp_pay_from_customer

create or alter procedure sp_cancel_pay_from_customer(
    a_selected_doc_id type of dm_idb default null
)
returns (
    doc_list_id type of dm_idb, -- id of selected doc (prepayment that is deleted)
    agent_id type of dm_idb, -- id of customer
    prepayment_sum type of dm_cost -- customer's payment (in retailing prices)
)
as
    declare v_dummy bigint;
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;
    declare v_this dm_dbobj = 'sp_cancel_pay_from_customer';
begin

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
     -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    -- Choose random doc of corresponding kind.
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            (select id_selected from
                             sp_get_random_id( 'v_cancel_customer_prepayment' -- a_view_for_search
                                              ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                              ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                              ,:c_raise_exc_when_no_found
                                              ,:c_can_skip_order_clause
                                             )
                            )
                          );

    -- Try to LOCK just selected doc, raise exc if can`t:
    execute procedure sp_lock_selected_doc( doc_list_id, 'v_cancel_customer_prepayment', a_selected_doc_id);

    select agent_id, cost_retail
    from doc_list h
    where h.id = :doc_list_id
    into agent_id, prepayment_sum;
    
    -- finally, remove prepayment doc (decision about corr. `money_turnover_log` - see trigger doc_list_aiud)
    delete from doc_list h where h.id = :doc_list_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||doc_list_id);

    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_cancel_pay_from_customer


create or alter procedure sp_pay_to_supplier(
    a_selected_doc_id type of dm_idb default null,
    a_total_pay type of dm_cost default null,
    dbg int default 0
)
returns (
    agent_id type of dm_idb,
    prepayment_id type of dm_idb, -- id of prepayment that was done here
    invoice_id type of dm_idb, -- id of open supplier invoice(s) that 'receives' this advance
    current_pay_sum type of dm_cost -- total sum of prepayment (advance)
)
as
    declare v_dbkey dm_dbkey;
    declare v_round_to smallint;
    declare v_id type of dm_idb;
    declare v_this dm_dbobj = 'sp_pay_to_supplier';
begin

    -- Implementation for our payment to supplier.
    -- Randomly choose invoice that is not yet fully paid (by us) and creates
    -- payment document (with sum that can be equal or LESS than rest of value
    -- that should be 100% paid).

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    execute procedure sp_payment_common(
        fn_oper_pay_to_supplier(),
        a_selected_doc_id,
        a_total_pay
    ) returning_values invoice_id, agent_id, current_pay_sum;

    -- add new record in doc_list (header)
    execute procedure sp_add_doc_list(
        null,
        fn_oper_pay_to_supplier(),
        agent_id,
        null,
        null,
        current_pay_sum,
        0
    )
    returning_values :prepayment_id, :v_dbkey;

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(0, v_this, null, 'payment_id='||prepayment_id);

    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(prepayment_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end
end

^  -- sp_pay_to_supplier

create or alter procedure sp_cancel_pay_to_supplier(
    a_selected_doc_id type of dm_idb default null
)
returns (
    doc_list_id type of dm_idb, -- id of selected doc
    agent_id type of dm_idb, -- id of customer
    prepayment_sum type of dm_cost
)
as
    declare v_dummy bigint;
    declare v_this dm_dbobj = 'sp_cancel_pay_to_supplier';
    declare c_raise_exc_when_no_found dm_sign = 1;
    declare c_can_skip_order_clause dm_sign = 0;
begin

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    -- Choose random doc of corresponding kind.
    -- 25.09.2014: do NOT set c_can_skip_order_clause = 1,
    -- performance degrades from ~4900 to ~1900.
    doc_list_id = coalesce( :a_selected_doc_id,
                            (select id_selected from
                             sp_get_random_id( 'v_cancel_payment_to_supplier' -- a_view_for_search
                                              ,null -- a_view_for_min_id ==> the same as a_view_for_search
                                              ,null -- a_view_for_max_id ==> the same as a_view_for_search
                                              ,:c_raise_exc_when_no_found
                                              ,:c_can_skip_order_clause
                                            )
                            )
                          );

    -- Try to LOCK just selected doc, raise exc if can`t:
    execute procedure sp_lock_selected_doc( doc_list_id, 'v_cancel_payment_to_supplier', a_selected_doc_id);

    select agent_id, cost_purchase
    from doc_list h
    where h.id = :doc_list_id
    into agent_id, prepayment_sum;

    -- finally, remove prepayment doc (decision about corr. `money_turnover_log` - see trigger doc_list_aiud)
    delete from doc_list h where h.id = :doc_list_id;

    -- add to performance log timestamp about start/finish this unit
    -- (records from GTT tmp$perf_log will be MOVED in fixed table perf_log):
    execute procedure sp_add_perf_log(0, v_this, null, 'doc_id='||doc_list_id);

    suspend;

when any do
    begin
        -- in a`utonomous tx:
        -- 1) add to tmp$perf_log error info + timestamp,
        -- 2) move records from tmp$perf_log to perf_log
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            'doc_id='||coalesce(doc_list_id,'<null>'),
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- sp_cancel_pay_to_supplier

--------------------------------------------------------------------------------
-- #######################   S E R V I C E   U N I T S   #######################
--------------------------------------------------------------------------------

create or alter procedure srv_make_invnt_saldo(
    a_selected_ware_id type of dm_idb default null
)
returns (
    msg dm_info,
    ins_rows int,
    upd_rows int,
    del_rows int
)
as
    declare v_semaphore_id type of dm_idb;
    declare v_deferred_to_next_time boolean = false;
    declare v_gdscode int = null;
    declare v_catch_bitset bigint;
    declare v_exc_on_chk_violation smallint;
    declare v_this dm_dbobj = 'srv_make_invnt_saldo';
    declare s_qty_clo type of dm_qty;
    declare s_qty_clr type of dm_qty;
    declare s_qty_ord type of dm_qty;
    declare s_qty_sup type of dm_qty;
    declare s_qty_avl type of dm_qty;
    declare s_qty_res type of dm_qty;
    declare s_qty_inc type of dm_qty;
    declare s_qty_out type of dm_qty;
    declare s_cost_inc type of dm_cost;
    declare s_cost_out type of dm_cost;
    declare v_rc int;
    declare v_err_msg dm_info;
    declare v_neg_info dm_info;
    declare c_chk_violation_code int = 335544558; -- check_constraint
    declare c_semaphores cursor for ( select id from semaphores s where s.task = :v_this rows 1);
begin
    -- Gathers all turnovers for wares in 'invnt_turnover_log' table and makes them total
    -- to merge in table 'invnt_saldo'
    -- Original idea: sql.ru/forum/964534/hranimye-agregaty-bez-konfliktov-i-blokirovok-recept?hl=
    -- 21.08.2014: refactored in order to maximal detailed info (via cursor)
    -- and SKIP problem wares (with logging first ware which has neg. remainder)
    -- and continue totalling for other ones.

    v_catch_bitset = cast(rdb$get_context('USER_SESSION','QMISM_VERIFY_BITSET') as bigint);
    -- bit#0 := 1 ==> perform calls of srv_catch_qd_qs_mism in doc_list_aiud => sp_add_invnt_log
    -- bit#1 := 1 ==> perform calls of srv_catch_neg_remainders from invnt_turnover_log_ai
    --                (instead of totalling turnovers to `invnt_saldo` table)
    -- bit#2 := 1 ==> allow dump dirty data into z-tables for analysis, see sp zdump4dbg, in case
    --                when some 'bad exception' occurs (see ctx var `HALT_TEST_ON_ERRORS`)
    if ( bin_and( v_catch_bitset, 2 ) = 2 ) then
        -- instead of totalling turnovers (invnt_turnover_log => group_by => invnt_saldo)
        -- we make verification of remainders after every time invnt_turnover_log is
        -- changed, see: INVNT_TURNOVER_LOG_AI => SRV_CATCH_NEG_REMAINDERS
        --####
          exit;
        --####

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;

    if ( not fn_is_snapshot() )
    then
        exception ex_snapshot_isolation_required;


    -- Ensure that current attach is the ONLY one which tries to make totals.
    -- Use locking record from `semaphores` table to serialize access to this
    -- code:
    begin
        v_semaphore_id = null;
        open c_semaphores;
        while (1=1) do
        begin
            fetch c_semaphores into v_semaphore_id;
            if ( row_count = 0 ) then
                exception ex_record_not_found using('semaphores', v_this);
            update semaphores set id = id where current of c_semaphores;
            leave;
        end
        close c_semaphores;

    when any do
        -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
        -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
        -- catched it's kind of exception!
        -- 1) tracker.firebirdsql.org/browse/CORE-3275
        --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
        -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
        begin
            if ( fn_is_lock_trouble(gdscode) ) then
                begin
                    -- concurrent_transaction ==> if select for update failed;
                    -- deadlock ==> if attempt of UPDATE set id=id failed.
                    v_gdscode = gdscode;
                    del_rows = -gdscode;
                    v_deferred_to_next_time = true;
                end
            else
                exception;  -- ::: nb ::: anonimous but in when-block! (check will it be really raised! find topic in sql.ru)
        end
    end

    if ( v_deferred_to_next_time ) then
    begin
        -- Info to be stored in context var. A`DD_INFO, see below call of sp_add_to_abend_log (in W`HEN ANY section):
        msg = 'can`t lock semaphores.id='|| coalesce(v_semaphore_id,'<?>') ||', deferred';
        exception ex_cant_lock_semaphore_record msg;
    end

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    ins_rows = 0;
    upd_rows = 0;
    del_rows = 0;
    v_neg_info = '';
    v_exc_on_chk_violation = iif( rdb$get_context('USER_SESSION', 'HALT_TEST_ON_ERRORS') containing ',CK,', 1, 0);

    for
        select
            ware_id,
            qty_clo, qty_clr, qty_ord, qty_sup,
            qty_avl, qty_res, qty_inc, qty_out,
            cost_inc, cost_out
        from v_saldo_invnt sn -- result MUST be totalled by WARE_ID (see DDL of this view)
        as cursor cn
    do
    begin
        s_qty_clo=0; s_qty_clr=0; s_qty_ord=0; s_qty_sup=0;
        s_qty_avl=0; s_qty_res=0; s_qty_inc=0; s_qty_out=0;
        s_cost_inc=0; s_cost_out=0;

        select
            qty_clo, qty_clr, qty_ord, qty_sup,
            qty_avl, qty_res, qty_inc, qty_out,
            cost_inc, cost_out
        from invnt_saldo t
        where t.id = cn.ware_id
        into
             s_qty_clo, s_qty_clr, s_qty_ord, s_qty_sup
            ,s_qty_avl, s_qty_res, s_qty_inc,s_qty_out
            ,s_cost_inc, s_cost_out;

        v_rc = row_count; -- 0=> will be INSERT, otherwise UPDATE
        -- these values WILL be written in invnt_saldo:
        s_qty_clo = s_qty_clo + cn.qty_clo;
        s_qty_clr = s_qty_clr + cn.qty_clr;
        s_qty_ord = s_qty_ord + cn.qty_ord;
        s_qty_sup = s_qty_sup + cn.qty_sup;
        s_qty_avl = s_qty_avl + cn.qty_avl;
        s_qty_res = s_qty_res + cn.qty_res;
        s_qty_inc = s_qty_inc + cn.qty_inc;
        s_qty_out = s_qty_out + cn.qty_out;
        s_cost_inc = s_cost_inc + cn.cost_inc;
        s_cost_out = s_cost_out + cn.cost_out;

        v_err_msg='';
        -- Check all new values before writing into invnt_saldo for matching
        -- rule of non-negative remainders, to be able DETAILED LOG of any
        -- violation (we can`t get any info about data that violates rule when
        -- exception raising):
        if ( s_qty_clo < 0 ) then v_err_msg = v_err_msg||' clo='||s_qty_clo;
        if ( s_qty_clr < 0 ) then v_err_msg = v_err_msg||' clr='||s_qty_clr;
        if ( s_qty_ord < 0 ) then v_err_msg = v_err_msg||' ord='||s_qty_ord;
        if ( s_qty_sup < 0 ) then v_err_msg = v_err_msg||' sup='||s_qty_sup;
        if ( s_qty_avl < 0 ) then v_err_msg = v_err_msg||' avl='||s_qty_avl;
        if ( s_qty_res < 0 ) then v_err_msg = v_err_msg||' res='||s_qty_res;
        if ( s_qty_inc < 0 ) then v_err_msg = v_err_msg||' inc='||s_qty_inc;
        if ( s_qty_out < 0 ) then v_err_msg = v_err_msg||' out='||s_qty_out;
        if ( s_cost_inc < 0 ) then v_err_msg = v_err_msg||' $inc='||s_cost_inc;
        if ( s_cost_out < 0 ) then v_err_msg = v_err_msg||' $out='||s_cost_out;

        if ( v_err_msg >  '' and v_neg_info = '' ) then
            -- register info only for FIRST ware when negative remainder found:
            v_neg_info = 'ware='||cn.ware_id||v_err_msg;

        if ( v_neg_info > '' ) then
        begin
            -- ::: NB ::: do NOT raise exc`eption! Let all wares which have NO troubles
            -- be totalled and removed from invnt_turnover_log (=> reduce size of this table)
            rdb$set_context( 'USER_SESSION','ADD_INFO', v_neg_info ); -- to be displayed in log of 1run_oltp_emul.bat
            msg = v_neg_info||'; '||msg;
            execute procedure sp_upd_in_perf_log(
                v_this,
                c_chk_violation_code,
                msg
            );
        end

        if ( v_err_msg = '' -- all remainders will be CORRECT => can write
             or
             v_exc_on_chk_violation = 1 -- allow attempt to write incorrect remainder in order to raise not_valid e`xception and auto-cancel test itself
           ) then
        begin
            update or insert into invnt_saldo(
                id
                ,qty_clo,qty_clr,qty_ord,qty_sup
                ,qty_avl,qty_res,qty_inc,qty_out
                ,cost_inc,cost_out
            ) values (
                cn.ware_id
                ,:s_qty_clo,:s_qty_clr,:s_qty_ord,:s_qty_sup
                ,:s_qty_avl,:s_qty_res,:s_qty_inc,:s_qty_out
                ,:s_cost_inc,:s_cost_out
            )
            matching(id);
    
            delete from invnt_turnover_log ng
            where ng.ware_id = cn.ware_id;
    
            del_rows = del_rows + row_count;
            ins_rows = ins_rows + iif( v_rc=0, 1, 0 );
            upd_rows = upd_rows + iif( v_rc=0, 0, 1 );

        end -- v_err_msg = ''
    end --  cursor on v_saldo_invnt

    msg = 'i='||ins_rows||', u='||upd_rows||', d='||del_rows;
    if ( v_neg_info = '' ) then
        rdb$set_context('USER_SESSION','ADD_INFO', msg); -- to be displayed in result log of isql

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(0, v_this, v_gdscode, msg );

    suspend;

when any do
    begin
        -- NB: proc sp_add_to_abend_log will set rdb$set_context('USER_SESSION','A`DD_INFO', msg)
        -- in order to show this additional info in ISQL log after operation will finish:
        execute procedure sp_add_to_abend_log(
            msg, -- ==> context var. ADD_INFO will be = "can`t lock semaphores.id=..., deferred" - to be shown in ISQL log
            gdscode,
            v_neg_info,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- end of srv_make_invnt_saldo

--------------------------------------------------------------------------------

create or alter procedure srv_make_money_saldo(
    a_selected_agent_id type of dm_idb default null
)
returns (
    msg dm_info,
    ins_rows int,
    upd_rows int,
    del_rows int
)
as
    declare v_semaphore_id type of dm_ids;
    declare v_deferred_to_next_time boolean = false;
    declare v_gdscode int = null;
    declare v_dbkey dm_dbkey;
    declare agent_id type of dm_ids;
    declare m_cust_debt dm_sign;
    declare m_supp_debt dm_sign;
    declare cost_purchase type of dm_cost;
    declare cost_retail type of dm_cost;
    declare v_dts_beg timestamp;
    declare v_dummy bigint;
    declare v_this dm_dbobj = 'srv_make_money_saldo';
    declare c_semaphores cursor for ( select id from semaphores s where s.task = :v_this rows 1);
begin

    -- Gathers all turnovers for agents in 'money_turnover_log' table and makes them total
    -- to merge in table 'money_saldo'
    -- Original idea by Dimitry Sibiryakov:
    -- sql.ru/forum/964534/hranimye-agregaty-bez-konfliktov-i-blokirovok-recept?hl=

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;
    
    if ( not fn_is_snapshot() )
    then
        exception ex_snapshot_isolation_required;

    -- Ensure that current attach is the ONLY one which tries to make totals.
    -- Use locking record from `semaphores` table to serialize access to this
    -- code:
    begin
        open c_semaphores;
        while (1=1) do
        begin
            fetch c_semaphores into v_semaphore_id;
            if ( row_count = 0 ) then
                exception ex_record_not_found using('semaphores', v_this);
            update semaphores set id = id where current of c_semaphores;
            leave;
        end
        close c_semaphores;
    when any do
        -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
        -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
        -- catched it's kind of exception!
        -- 1) tracker.firebirdsql.org/browse/CORE-3275
        --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
        -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
        begin
            if ( fn_is_lock_trouble(gdscode) ) then
                begin
                    -- concurrent_transaction ==> if select for update failed;
                    -- deadlock ==> if attempt of UPDATE set id=id failed.
                    v_gdscode = gdscode;
                    v_deferred_to_next_time = true;
                end
            else
                exception;  -- ::: nb ::: anonimous but in when-block! (check will it be really raised! find topic in sql.ru)
        end
    end

    if ( v_deferred_to_next_time ) then
    begin
        -- Info to be stored in context var. A`DD_INFO, see below call of sp_add_to_abend_log (in W`HEN ANY section):
        msg = 'can`t lock semaphores.id='|| coalesce(v_semaphore_id,'<?>') ||', deferred';
        exception ex_cant_lock_semaphore_record msg;
    end

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    ins_rows = 0;
    upd_rows = 0;
    del_rows = 0;
    v_dts_beg = 'now';
    for
        select x.agent_id,
                sum( o.m_supp_debt * x.sum_purchase ) sum_purchase,
                sum( o.m_cust_debt * x.sum_retail ) sum_retail
        from (
            select
                m.agent_id,
                m.optype_id,
                sum( m.cost_purchase ) sum_purchase,
                sum( m.cost_retail ) sum_retail
            from money_turnover_log m
            -- 27.09.2015: added index on (agent_id, optype_id)
            group by m.agent_id, m.optype_id
        ) x
        join optypes o on x.optype_id = o.id
        group by x.agent_id
    into
        agent_id,
        cost_purchase,
        cost_retail
    do begin

        delete from money_turnover_log m
        where m.agent_id = :agent_id;
        del_rows = del_rows + row_count;

        update money_saldo
        set cost_purchase = cost_purchase + :cost_purchase,
            cost_retail = cost_retail +  :cost_retail
        where agent_id = :agent_id;

        if ( row_count = 0 ) then
            begin
                insert into money_saldo( agent_id, cost_purchase, cost_retail )
                values( :agent_id, :cost_purchase, :cost_retail);

                ins_rows = ins_rows + 1;
            end
        else
            upd_rows = upd_rows + row_count;

    end -- cursor for money_turnover_log m join optypes o on m.optype_id = o.id

    msg = 'i='||ins_rows||', u='||upd_rows||', d='||del_rows
          ||', ms='||datediff(millisecond from v_dts_beg to cast('now' as timestamp) );
    rdb$set_context('USER_SESSION','ADD_INFO', msg);  -- to be displayed in result log of isql

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(0, v_this, v_gdscode, msg );

    suspend;

when any do
    begin
        -- NB: proc sp_add_to_abend_log will set rdb$set_context('USER_SESSION','A`DD_INFO', msg)
        -- in order to show this additional info in ISQL log after operation will finish:
        execute procedure sp_add_to_abend_log(
            msg,  -- ==> context var. ADD_INFO will be = "can`t lock semaphores.id=..., deferred" - to be shown in ISQL log
            gdscode,
            'agent_id='||agent_id,
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- srv_make_money_saldo

--------------------------------------------------------------------------------

create or alter procedure srv_recalc_idx_stat returns(
    tab_name dm_dbobj,
    idx_name dm_dbobj,
    elapsed_ms int
)
as
    declare msg dm_info;
    declare v_semaphore_id type of dm_idb;
    declare v_deferred_to_next_time boolean = false;
    declare v_dummy bigint;
    declare idx_stat_befo double precision;
    declare v_gdscode int = null;
    declare v_this dm_dbobj = 'srv_recalc_idx_stat';
    declare v_start timestamp;
    declare c_semaphores cursor for ( select id from semaphores s where s.task = :v_this rows 1);
begin

    -- Refresh index statistics for most changed tables.
    -- Needs to be run in regular basis otherwise ineffective plans
    -- can be generated when doing inner joins!

    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises ex`ception to stop test:
    execute procedure sp_check_to_stop_work;

    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;

    -- Use locking record from `semaphores` table to synchronize access to this
    -- code:
    begin
        v_semaphore_id = null;
        open c_semaphores;
        while (1=1) do
        begin
            fetch c_semaphores into v_semaphore_id;
            if ( row_count = 0 ) then
                exception ex_record_not_found using('semaphores', v_this);
            update semaphores set id = id where current of c_semaphores;
            leave;
        end
        close c_semaphores;
    when any do
        -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
        -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
        -- catched it's kind of exception!
        -- 1) tracker.firebirdsql.org/browse/CORE-3275
        --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
        -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
        begin
            if ( fn_is_lock_trouble(gdscode) ) then
                begin
                    -- concurrent_transaction ==> if select for update failed;
                    -- deadlock ==> if attempt of UPDATE set id=id failed.
                    v_deferred_to_next_time = true;
                    v_gdscode = gdscode;
                end
            else
                exception; -- ::: nb ::: anonimous but in when-block!
        end
    end

    if ( v_deferred_to_next_time ) then
    begin
       -- Info to be stored in context var. A`DD_INFO, see below call of sp_add_to_abend_log (in W`HEN ANY section):
        msg = 'can`t lock semaphores.id='|| coalesce(v_semaphore_id,'<?>') ||', deferred';
        exception ex_cant_lock_semaphore_record msg;
        exit;
    end

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    -- increment number of total business routine calls within this Tx,
    -- in order to display estimated overall performance in ISQL session
    -- logs (see generated $tmpdir/tmp_random_run.sql).
    -- Instead of querying perf_log join business_ops it was decided to
    -- use only context variables in user_tran namespace:
    execute procedure srv_increment_tx_bops_counter;

    for
        select ri.rdb$relation_name, ri.rdb$index_name, ri.rdb$statistics
        from rdb$indices ri
        where
            coalesce(ri.rdb$system_flag,0)=0
            -- make recalc only for most used tables:
            and ri.rdb$relation_name in ( 'DOC_DATA', 'DOC_LIST', 'QDISTR', 'QSTORNED', 'PDISTR', 'PSTORNED')
        order by ri.rdb$relation_name, ri.rdb$index_name
    into
        tab_name, idx_name, idx_stat_befo
    do begin
        -- Check that table `ext_stoptest` (external text file) is EMPTY,
        -- otherwise raises ex`ception to stop test:
        execute procedure sp_check_to_stop_work;

        execute procedure sp_add_perf_log(1, v_this||'_'||idx_name);

        v_start='now';

        execute statement( 'set statistics index '||idx_name )
        with autonomous transaction -- again since 27.11.2015 (c`ommit for ALL indices at once is too long for huge databases!)
        ; 

        elapsed_ms = datediff(millisecond from v_start to cast('now' as timestamp)); -- 15.09.2015

        execute procedure sp_add_perf_log(0, v_this||'_'||idx_name,null,tab_name, idx_stat_befo);
        suspend;
    end

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(0, v_this, v_gdscode);

when any do
    begin
        -- NB: proc sp_add_to_abend_log will set rdb$set_context('USER_SESSION','A`DD_INFO', msg)
        -- in order to show this additional info in ISQL log after operation will finish:
        execute procedure sp_add_to_abend_log(
            msg, -- ==> context var. ADD_INFO will be = "can`t lock semaphores.id=..., deferred" - to be shown in ISQL log
            gdscode,
            null,
            v_this
        );

        --#######
        exception; -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- srv_recalc_idx_stat

--------------------------------------------------------------------------
-- ###########################    R E P O R T S   ########################
--------------------------------------------------------------------------

create or alter procedure srv_get_last_launch_beg_end(
    a_last_hours smallint default 3,
    a_last_mins smallint default 0)
returns (
     last_launch_beg timestamp
    ,last_launch_end timestamp
) as
begin
    -- Auxiliary SP: finds moments of start and finish business operations in perf_log
    -- on timestamp interval that is [L, N] where:
    -- "L" = latest from {-abs( :a_last_hours * 60 + :a_last_mins ), 'perf_watch_interval'}
    -- "N" = latest record in perf_log table
    select maxvalue( x.last_job_start_dts, y.last_job_finish_dts ) as last_job_start_dts
    from (
        select p.dts_beg as last_job_start_dts
        from perf_log p
        where p.unit = 'perf_watch_interval'
        order by dts_beg desc rows 1
    ) x
    cross join
    (
        select dateadd( -abs( :a_last_hours * 60 + :a_last_mins ) minute to p.dts_beg) as last_job_finish_dts
        from perf_log p
        where exists(select 1 from business_ops b where b.unit=p.unit order by b.unit) -- nb: do NOT use inner join here (bad plan with sort)
        order by p.dts_beg desc
        rows 1
    ) y
    into last_launch_beg;

    select p.dts_end as report_end
    from perf_log p
    where
        p.dts_beg >= :last_launch_beg
        and p.dts_end is not null
    order by p.dts_beg desc
    rows 1
    into last_launch_end;
    suspend;
end

^ -- srv_get_last_launch_beg_end

create or alter procedure srv_mon_perf_total(
    a_last_hours smallint default 3,
    a_last_mins smallint default 0)
returns (
    business_action dm_info,
    job_beg varchar(16),
    job_end varchar(16),
    avg_times_per_minute numeric(12,2),
    avg_elapsed_ms int,
    successful_times_done int
)
as
    declare v_sort_prior int;
    declare v_overall_performance double precision;
    declare v_all_minutes int;
    declare v_succ_all_times int;
    declare v_this dm_dbobj = 'srv_mon_perf_total';
begin
    -- MAIN SP for estimating performance: provides number of business operations
    -- per minute which were SUCCESSFULLY finished. Suggested by Alexey Kovyazin.

    a_last_hours = abs( a_last_hours );
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    delete from tmp$perf_log p  where p.stack = :v_this;

    insert into tmp$perf_log(unit, info, id, dts_beg, dts_end, aux1, aux2, stack)
    with
    a as(
        -- reduce needed number of minutes from most last event of some SP starts:
        -- 18.07.2014: handle only data which belongs to LAST job.
        -- Record with p.unit = 'perf_watch_interval' is added in
        -- oltp_isql_run_worker.bat before FIRST isql will be launched
        -- for each mode ('sales', 'logist' etc)
        select maxvalue( x.last_job_start_dts, y.last_job_finish_dts ) as last_job_start_dts
        from (
            select p.dts_beg as last_job_start_dts
            from perf_log p
            where p.unit = 'perf_watch_interval'
            order by dts_beg desc rows 1
        ) x
        join
        (
            select dateadd( -abs( :a_last_hours * 60 + :a_last_mins ) minute to p.dts_beg) as last_job_finish_dts
            from perf_log p
            -- nb: do NOT use inner join here (bad plan with sort)
            where exists(select 1 from business_ops b where b.unit=p.unit order by b.unit) -- "order by" - only for 3.0
            order by p.dts_beg desc
            rows 1
        ) y
        on 1=1
    )
    ,p as(
        select
            g.unit
            ,min( g.dts_beg ) report_beg
            ,max( g.dts_end  ) report_end
            ,count(*) successful_times_done
            ,avg(g.elapsed_ms) successful_avg_ms
        from perf_log g
        join business_ops b on b.unit=g.unit
        join a on g.dts_beg >= a.last_job_start_dts -- only rows which are from THIS job!
        where  -- we must take in account only SUCCESSFULLY finished units, i.e. fb_gdscode is NULL.
            g.fb_gdscode + 0 -- 25.11.2015: suppress making bitmap for this index! almost 90% of rows contain NULL in this field.
            is null
        group by g.unit
    )
    select b.unit, b.info, b.sort_prior, p.report_beg, p.report_end,
           p.successful_times_done, p.successful_avg_ms, :v_this
    from business_ops b
    left join p on b.unit = p.unit;
    -- tmp$perf_log(unit, info, id, dts_beg, dts_end, aux1, aux2)

    -- total elapsed minutes and number of successfully finished SPs for ALL units:
    select nullif(datediff( minute from min_beg to max_end ),0),
           succ_all_times,
           left(cast(min_beg as varchar(24)),16),
           left(cast(max_end as varchar(24)),16)
    from (
        select min(p.dts_beg) min_beg, max(p.dts_end) max_end, sum(p.aux1) succ_all_times
        from tmp$perf_log p
        where p.stack = :v_this
    )
    into v_all_minutes, v_succ_all_times, job_beg, job_end;

    for
        select
             business_action
            ,avg_times_per_minute
            ,avg_elapsed_ms
            ,successful_times_done
            ,sort_prior
        from (
            select
                0 as sort_prior
                ,'*** OVERALL *** for '|| :v_all_minutes ||' minutes: ' as business_action -- 'Average ops/minute = '||:v_all_minutes||' ;'||(sum( aux1 ) / :v_all_minutes)  as business_action
                ,1.00*sum( aux1 ) / :v_all_minutes as avg_times_per_minute
                ,avg(aux2) as avg_elapsed_ms
                ,sum(aux1) as successful_times_done
            from tmp$perf_log p
            where p.stack = :v_this

            UNION ALL
            
            select
                 p.id as sort_prior
                ,p.info as business_action
                ,1.00 * aux1 / maxvalue( 1, datediff( minute from p.dts_beg to p.dts_end ) ) as avg_times_per_minute
                ,aux2 as avg_elapsed_ms
                ,aux1 as successful_times_done
            from tmp$perf_log p
            where p.stack = :v_this
        ) x
        order by x.sort_prior
        into
             business_action
            ,avg_times_per_minute
            ,avg_elapsed_ms
            ,successful_times_done
            ,v_sort_prior
    do begin
        if ( v_sort_prior = 0 ) then -- save value to be written into perf_log
            v_overall_performance = avg_times_per_minute;
        suspend;
    end

    delete from tmp$perf_log p  where p.stack = :v_this;

    begin
        -- 02.11.2015: save overall performance value so it can be used later:
        update perf_log p set aux1 = :v_overall_performance
        where p.unit = 'perf_watch_interval'
        order by dts_beg desc rows 1;
    when any do
        begin
            -- lock/update conflict can be here with another ISQL session with SID #1
            -- (running on other machine) that makes this report at the same time.
            -- We suppress this exception because this record will anyway contain
            -- value that we want to save.
        end
    end
    -- Statistics for database with size = 100 Gb and cleaned OS cache (LI-V3.0.0.32179):
    -- sync
    -- echo 3 > /proc/sys/vm/drop_caches
    -- 20 records fetched
    -- 600187 ms, 233041 read(s), 4 write(s), 3206400 fetch(es), 70 mark(s)
    --
    -- Table                             Natural     Index    Update    Insert    Delete
    -- ***********************************************************************************
    -- RDB$INDICES                                       9
    -- BUSINESS_OPS                           19        38
    -- PERF_LOG                                     369967         1
    -- TMP$PERF_LOG                           76                            19        19

end

^ -- srv_mon_perf_total

create or alter procedure srv_mon_perf_dynamic(
    a_intervals_number smallint default 10,
    a_last_hours smallint default 3,
    a_last_mins smallint default 0)
returns (
     business_action dm_info
    ,interval_no smallint
    ,cnt_ok_per_minute int
    ,cnt_all int
    ,cnt_ok int
    ,cnt_err int
    ,err_prc numeric(12,2)
    ,ok_avg_ms int
    ,interval_beg timestamp
    ,interval_end timestamp
)
as
    declare v_this dm_dbobj = 'srv_mon_perf_dynamic';
begin

    -- 15.09.2014 Get performance results 'in dynamic': split all job time to N
    -- intervals, where N is specified by 1st input argument.
    -- 03.09.2015 Removed cross join perf_log and CTE 'inp_args as i' because
    -- of inefficient plan. Input parameters are injected inside DT.
    -- See: http://www.sql.ru/forum/1173774/select-a-b-from-a-cross-b-order-by-indexed-field-of-a-rows-n-ignorit-rows-n-why

    a_intervals_number = iif( a_intervals_number <= 0, 10, a_intervals_number);
    a_last_hours = abs( a_last_hours );
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    delete from tmp$perf_log p where p.stack = :v_this;
    insert into tmp$perf_log(
        unit
        ,info
        ,id        -- interval_no
        ,dts_beg   -- interval_beg
        ,dts_end   -- interval_end
        ,aux1      -- cnt_ok
        ,aux2       -- cnt_err
        ,elapsed_ms -- ok_avg_ms
        ,stack
    )
    with
    a as(
        -- reduce needed number of minutes from most last event of some SP starts:
        -- 18.07.2014: handle only data which belongs to LAST job.
        -- Record with p.unit = 'perf_watch_interval' is added in
        -- oltp_isql_run_worker.bat before FIRST isql will be launched
        select
            maxvalue( x.last_added_watch_row_dts, y.first_measured_start_dts ) as first_job_start_dts
            ,y.last_job_finish_dts
            ,y.intervals_number
        from (
            select p.dts_beg as last_added_watch_row_dts
            from perf_log p
            where p.unit = 'perf_watch_interval'
            order by dts_beg desc rows 1
        ) x
        join (
            select
                dateadd( p.scan_bak_minutes minute to p.dts_beg) as first_measured_start_dts
                ,p.dts_beg as last_job_finish_dts
                ,p.intervals_number
            from
            ( -- since 03.09.2015:
                select
                    p.*
                    , -abs( :a_last_hours * 60 + :a_last_mins ) as scan_bak_minutes
                    , :a_intervals_number as intervals_number
                from perf_log p
            ) p
            -- nb: do NOT use inner join here (bad plan with sort)
            where exists(select 1 from business_ops b where b.unit=p.unit order by b.unit)
            order by p.dts_beg desc
            rows 1
-- Before 03.09.2015
-- (inefficient plan with nested loops of all pef_log rows + SORT, 'rows 1' was ignored!
-- See: http://www.sql.ru/forum/1173774/select-a-b-from-a-cross-b-order-by-indexed-field-of-a-rows-n-ignorit-rows-n-why
--            select
--                dateadd( i.scan_bak_minutes minute to p.dts_beg) as first_measured_start_dts
--                ,p.dts_beg as last_job_finish_dts
--                ,i.intervals_number
--            from perf_log p
--            join i on 1=1  -- CTE 'i' was: "with i as(select :a_intervals_number, :a_last_hours, :a_last_mins from rdb$database)
--            -- nb: do NOT use inner join here (bad plan with sort)
--            where exists(select 1 from business_ops b where b.unit=p.unit order by b.unit)
--            order by p.dts_beg desc
--            rows 1
        ) y on 1=1
    )
    ,d as(
        select
            a.first_job_start_dts
            ,a.last_job_finish_dts
            ,1+datediff(second from a.first_job_start_dts to a.last_job_finish_dts) / a.intervals_number as sec_for_one_interval
        from a
    )
    --    select * from d

    ,p as(
        select
            g.unit
            ,b.info
            ,1+cast(datediff(second from d.first_job_start_dts to g.dts_beg) / d.sec_for_one_interval as int) as interval_no
            ,count(*) cnt_all
            ,count( iif( g.fb_gdscode is null, 1, null ) ) cnt_ok
            ,count( iif( g.fb_gdscode is NOT null, 1, null ) ) cnt_err
            ,100.00 * count( nullif(g.fb_gdscode,0) ) / count(*) err_prc
            ,avg(  iif( g.fb_gdscode is null, g.elapsed_ms, null ) ) ok_avg_ms
            ,min(d.first_job_start_dts) as first_job_start_dts
            ,min(d.sec_for_one_interval) as sec_for_one_interval
        from perf_log g
        join business_ops b on b.unit = g.unit
        join d on g.dts_beg >= d.first_job_start_dts -- only rows which are from THIS measured test run!
        group by 1,2,3
    )
    ,q as(
        select
            unit
            ,info
            ,interval_no
            ,dateadd( (interval_no-1) * sec_for_one_interval+1 second to first_job_start_dts ) as interval_beg
            ,dateadd( interval_no * sec_for_one_interval second to first_job_start_dts ) as interval_end
            ,cnt_all
            ,cnt_ok
            ,cnt_err
            ,err_prc
            ,ok_avg_ms
        from p
    )
    --select * from q;
    select
        unit
        ,info
        ,interval_no
        ,interval_beg
        ,interval_end
        ,cnt_ok          -- aux1
        ,cnt_err         -- aux2
        ,ok_avg_ms
        ,:v_this
    from q;
    -----------------------------

    for
        select
             business_action
            ,interval_no
            ,cnt_ok_per_minute
            ,cnt_all
            ,cnt_ok
            ,cnt_err
            ,err_prc
            ,ok_avg_ms
            ,interval_beg
            ,interval_end
        from (
            select
                0 as sort_prior
                ,'interval #'||lpad(id, 4, ' ')||', overall' as business_action
                ,id as interval_no
                ,min(dts_beg) as interval_beg
                ,min(dts_end) as interval_end
                ,round(sum(aux1) / nullif(datediff(minute from min(dts_beg) to min(dts_end)),0), 0) cnt_ok_per_minute
                ,sum(aux1 + aux2) as cnt_all
                ,sum(aux1) as cnt_ok
                ,sum(aux2) as cnt_err
                ,100 * sum(aux2) / sum(aux1 + aux2) as err_prc
                ,cast(null as int) as ok_avg_ms
                --,avg(elapsed_ms) as ok_avg_ms
            from tmp$perf_log p
            where p.stack = :v_this
            group by id
        
            UNION ALL
        
            select
                1 as sort_prior
                ,info as business_action
                ,id as interval_no
                ,dts_beg as interval_beg
                ,dts_end as interval_end
                ,aux1 / nullif(datediff(minute from dts_beg to dts_end),0) cnt_ok_per_minute
                ,aux1 + aux2 as cnt_all
                ,aux1 as cnt_ok
                ,aux2 as cnt_err
                ,100 * aux2 / (aux1 + aux2) as err_prc
                ,elapsed_ms as ok_avg_ms
            from tmp$perf_log p
            where p.stack = :v_this
        )
        order by sort_prior, business_action, interval_no
    into
             business_action
            ,interval_no
            ,cnt_ok_per_minute
            ,cnt_all
            ,cnt_ok
            ,cnt_err
            ,err_prc
            ,ok_avg_ms
            ,interval_beg
            ,interval_end
    do suspend;
end

^ -- srv_mon_perf_dynamic

create or alter procedure srv_mon_perf_detailed (
    a_last_hours smallint default 3,
    a_last_mins smallint default 0,
    a_show_detl smallint default 0)
returns (
    unit type of dm_unit,
    cnt_all integer,
    cnt_ok integer,
    cnt_err integer,
    err_prc numeric(6,2),
    ok_min_ms integer,
    ok_max_ms integer,
    ok_avg_ms integer,
    cnt_lk_confl integer,
    cnt_user_exc integer,
    cnt_chk_viol integer,
    cnt_unq_viol integer,
    cnt_fk_viol integer,
    cnt_stack_trc integer, -- 335544842, 'stack_trace': appears at the TOP of stack in 3.0 SC (strange!)
    cnt_zero_gds integer,  -- 03.10.2014: core-4565 (gdscode=0 in when-section! 3.0 SC only)
    cnt_other_exc integer,
    job_beg varchar(16),
    job_end varchar(16)
)
as
begin
    -- SP for detailed performance analysis: count of operations
    -- (NOT only business ops; including BOTH successful and failed ones),
    -- count of errors (including by their types)
    a_last_hours = abs( coalesce(a_last_hours, 3) );
    a_last_mins = coalesce(a_last_mins, 0);
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    delete from tmp$perf_mon where 1=1;

    insert into tmp$perf_mon(
         dts_beg                     -- 1
        ,dts_end
        ,unit
        ,cnt_all
        ,cnt_ok                       -- 5
        ,cnt_err
        ,err_prc
        ,ok_min_ms
        ,ok_max_ms
        ,ok_avg_ms                  -- 10
        ,cnt_chk_viol
        ,cnt_unq_viol
        ,cnt_fk_viol
        ,cnt_lk_confl
        ,cnt_user_exc               -- 15
        ,cnt_stack_trc
        ,cnt_zero_gds
        ,cnt_other_exc
    )
    with
    a as(
        -- reduce needed number of minutes from most last event of some SP starts:
        -- 18.07.2014: handle only data which belongs to LAST job.
        -- Record with p.unit = 'perf_watch_interval' is added in
        -- oltp_isql_run_worker.bat before FIRST isql will be launched
        -- for each mode ('sales', 'logist' etc)
        select maxvalue( x.last_job_start_dts, y.last_job_finish_dts ) as last_job_start_dts
        from (
            select p.dts_beg as last_job_start_dts
            from perf_log p
            where p.unit = 'perf_watch_interval'
            order by dts_beg desc rows 1
        ) x
        join
        (
            select dateadd( -abs( :a_last_hours * 60 + :a_last_mins ) minute to p.dts_beg) as last_job_finish_dts
            from perf_log p
            where exists(select 1 from business_ops b where b.unit=p.unit order by b.unit) -- nb: do NOT use inner join here (bad plan with sort)
            order by p.dts_beg desc
            rows 1
        ) y
        on 1=1
    )
--    a as( select p.dts_beg last_beg from perf_log p order by p.dts_beg desc rows 1 )
    ,r as(
          select min(p.dts_beg) report_beg, max(dts_end) report_end
          from perf_log p
          join a on p.dts_beg >= a.last_job_start_dts
    )
    ,c as (
        select
             r.report_beg
            ,r.report_end
            ,pg.unit
            ,count(*) cnt_all
            ,count( iif( nullif(pg.fb_gdscode,0) is null, 1, null) ) cnt_ok                -- 5
            ,count( nullif(pg.fb_gdscode,0) ) cnt_err
            ,100.00 * count( nullif(pg.fb_gdscode,0) ) / count(*) err_prc
            ,min( iif( nullif(pg.fb_gdscode,0) is null, pg.elapsed_ms, null) ) ok_min_ms
            ,max( iif( nullif(pg.fb_gdscode,0) is null, pg.elapsed_ms, null) ) ok_max_ms
            ,avg( iif( nullif(pg.fb_gdscode,0) is null, pg.elapsed_ms, null) ) ok_avg_ms
            ,count( iif(pg.fb_gdscode in( 335544347, 335544558 ), 1, null ) ) cnt_chk_viol    -- 10
            ,count( iif(pg.fb_gdscode in( 335544665, 335544349 ), 1, null ) ) cnt_unq_viol
            ,count( iif(pg.fb_gdscode in( 335544466, 335544838, 335544839 ), 1, null ) ) cnt_fk_viol
            ,count( iif(pg.fb_gdscode in( 335544345, 335544878, 335544336, 335544451 ), 1, null ) ) cnt_lk_confl
            ,count( iif(pg.fb_gdscode = 335544517, 1, null) ) cnt_user_exc
            ,count( iif(pg.fb_gdscode = 335544842, 1, null) ) cnt_stack_trc                 -- 15
            ,count( iif(pg.fb_gdscode = 0, 1, null) ) cnt_zero_gds
            ,count( iif( pg.fb_gdscode
                         in (
                                335544347, 335544558,
                                335544665, 335544349,
                                335544466, 335544838, 335544839,
                                335544345, 335544878, 335544336, 335544451,
                                335544517,
                                335544842,
                                0
                            )
                          ,null
                          ,pg.fb_gdscode
                       )
                   ) cnt_other_exc
        from perf_log pg
        join r on pg.dts_beg between r.report_beg and r.report_end
        where
            pg.elapsed_ms >= 0 and  -- 24.09.2014: prevent from display in result 'sp_halt_on_error', 'perf_watch_interval' and so on
            pg.unit not starting with 'srv_recalc_idx_stat_'
        group by
             r.report_beg
            ,r.report_end
            ,pg.unit
    )
    select *
    from c;

    insert into tmp$perf_mon(
         rollup_level
        ,unit
        ,cnt_all
        ,cnt_ok
        ,cnt_err
        ,err_prc
        ,ok_min_ms
        ,ok_max_ms
        ,ok_avg_ms
        ,cnt_chk_viol
        ,cnt_unq_viol
        ,cnt_fk_viol
        ,cnt_lk_confl
        ,cnt_user_exc
        ,cnt_stack_trc
        ,cnt_zero_gds
        ,cnt_other_exc
        ,dts_beg
        ,dts_end
    )
    select
         1
        ,unit
        ,sum(cnt_all)
        ,sum(cnt_ok)
        ,sum(cnt_err)
        ,100.00 * sum(cnt_err) / sum(cnt_all)
        ,min(ok_min_ms)
        ,max(ok_max_ms)
        ,max(ok_avg_ms)
        ,sum( cnt_chk_viol ) cnt_chk_viol
        ,sum( cnt_unq_viol ) cnt_unq_viol
        ,sum( cnt_fk_viol ) cnt_fk_viol
        ,sum( cnt_lk_confl ) cnt_lk_confl
        ,sum( cnt_user_exc ) cnt_user_exc
        ,sum( cnt_stack_trc ) cnt_stack_trc
        ,sum( cnt_zero_gds ) cnt_zero_gds
        ,sum( cnt_other_exc ) cnt_other_exc
        ,max( dts_beg )
        ,max( dts_end )
    from tmp$perf_mon
    group by unit; -- overall totals

    if ( :a_show_detl = 0 ) then
        delete from tmp$perf_mon m where m.rollup_level is null;

    -- final resultset (with overall totals first):
    for
        select
            unit, cnt_all, cnt_ok, cnt_err, err_prc, ok_min_ms, ok_max_ms, ok_avg_ms
            ,cnt_chk_viol
            ,cnt_unq_viol
            ,cnt_fk_viol
            ,cnt_lk_confl
            ,cnt_user_exc
            ,cnt_stack_trc
            ,cnt_zero_gds
            ,cnt_other_exc
            ,left(cast(dts_beg as varchar(24)),16)
            ,left(cast(dts_end as varchar(24)),16)
        from tmp$perf_mon
        --order by dy desc nulls first,hr desc, unit
    into unit, cnt_all, cnt_ok, cnt_err, err_prc, ok_min_ms, ok_max_ms, ok_avg_ms
        ,cnt_chk_viol
        ,cnt_unq_viol
        ,cnt_fk_viol
        ,cnt_lk_confl
        ,cnt_user_exc
        ,cnt_stack_trc
        ,cnt_zero_gds
        ,cnt_other_exc
        ,job_beg
        ,job_end
    do
        suspend;

end

^ -- srv_mon_perf_detailed

create or alter procedure srv_mon_business_perf_with_exc (
    a_last_hours smallint default 3,
    a_last_mins smallint default 0)
returns (
    info dm_info,
    unit dm_unit,
    cnt_all integer,
    cnt_ok integer,
    cnt_err integer,
    err_prc numeric(6,2),
    cnt_chk_viol integer,
    cnt_unq_viol integer,
    cnt_lk_confl integer,
    cnt_user_exc integer,
    cnt_other_exc integer,
    job_beg varchar(16),
    job_end varchar(16)
)
AS
declare v_dummy int;
begin

    a_last_hours = abs( coalesce(a_last_hours, 3) );
    a_last_mins = coalesce(a_last_mins, 0);
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    delete from tmp$perf_mon where 1=1;
    -- call to fill tmp$perf_mon with ONLY aggregated data:
    select count(*) from srv_mon_perf_detailed(:a_last_hours, :a_last_mins, 0) into v_dummy;

    for
        select
             o.info,s.unit, s.cnt_all, s.cnt_ok,s.cnt_err,s.err_prc
            ,s.cnt_chk_viol
            ,s.cnt_unq_viol
            ,s.cnt_lk_confl
            ,s.cnt_user_exc
            ,s.cnt_other_exc
            ,left(cast(s.dts_beg as varchar(24)),16)
            ,left(cast(s.dts_end as varchar(24)),16)
        from business_ops o
        left join tmp$perf_mon s on o.unit=s.unit
        order by o.sort_prior
    into
        info
        ,unit
        ,cnt_all
        ,cnt_ok
        ,cnt_err
        ,err_prc
        ,cnt_chk_viol
        ,cnt_unq_viol
        ,cnt_lk_confl
        ,cnt_user_exc
        ,cnt_other_exc
        ,job_beg
        ,job_end
    do
        suspend;

end

^ -- srv_mon_business_perf_with_exc

create or alter procedure srv_mon_exceptions(
    a_last_hours smallint default 3,
    a_last_mins smallint default 0)
returns (
    fb_gdscode int,
    fb_mnemona type of column fb_errors.fb_mnemona,
    unit type of dm_unit,
    cnt int,
    dts_min timestamp,
    dts_max timestamp
)
as
begin
    a_last_hours = abs( a_last_hours );
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    for
        with
        a as(
            -- reduce needed number of minutes from most last event of some SP starts:
            -- 18.07.2014: handle only data which belongs to LAST job.
            -- Record with p.unit = 'perf_watch_interval' is added in
            -- oltp_isql_run_worker.bat before FIRST isql will be launched
            -- for each mode ('sales', 'logist' etc)
            select maxvalue( x.last_job_start_dts, y.last_job_finish_dts ) as last_job_start_dts
            from (
                select p.dts_beg as last_job_start_dts
                from perf_log p
                where p.unit = 'perf_watch_interval'
                order by dts_beg desc rows 1
            ) x
            join
            (
                select dateadd( -abs( :a_last_hours * 60 + :a_last_mins ) minute to p.dts_beg) as last_job_finish_dts
                from perf_log p
                -- nb: do NOT use inner join here (bad plan with sort)
                where exists(select 1 from business_ops b where b.unit=p.unit order by b.unit)
                order by p.dts_beg desc
                rows 1
            ) y
            on 1=1
        )
        select p.fb_gdscode, e.fb_mnemona, p.unit, count(*) cnt, min(p.dts_beg) dts_min, max(p.dts_beg) dts_max
        from perf_log p
        join a on p.dts_beg >= a.last_job_start_dts
        LEFT -- !! some exceptions can missing in fb_errors !!
            join fb_errors e on p.fb_gdscode = e.fb_gdscode
        where
            p.fb_gdscode > 0
            and p.exc_unit='#' -- 10.01.2015, see sp_add_to_abend_log: take in account only those units where exception occured, and skip callers of them
        group by 1,2,3
    into
       fb_gdscode, fb_mnemona, unit, cnt, dts_min, dts_max
    do
        suspend;
end

^ -- srv_mon_exceptions

create or alter procedure srv_mon_perf_trace (
    a_intervals_number smallint default 10,
    a_last_hours smallint default 3,
    a_last_mins smallint default 0
)
returns (
    unit dm_unit
    ,info dm_info
    ,interval_no smallint
    ,cnt_success int
    ,fetches_per_second int
    ,marks_per_second int
    ,reads_to_fetches_prc numeric(6,2)
    ,writes_to_marks_prc numeric(6,2)
    ,interval_beg timestamp
    ,interval_end timestamp
) as
begin

    -- Report based on result of parsing TRACE log which was started by
    -- ISQL session #1 when config parameter trc_unit_perf = 1.
    -- Data for each business operation are displayed separately because
    -- they depends on execution plans and can not be compared each other.
    -- We have to analyze only RATIOS between reads/fetches and writes/marks,
    -- and also values of speed (fetches and marks per second) instead of
    -- absolute their values.

    a_intervals_number = iif( a_intervals_number <= 0, 10, a_intervals_number);
    a_last_hours = abs( a_last_hours );
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    for
        with
        a as(
            -- reduce needed number of minutes from most last event of some SP starts:
            -- 18.07.2014: handle only data which belongs to LAST job.
            -- Record with p.unit = 'perf_watch_interval' is added in
            -- oltp_isql_run_worker.bat before FIRST isql will be launched
            select
                maxvalue( x.last_added_watch_row_dts, y.first_measured_start_dts ) as first_job_start_dts
                ,y.last_job_finish_dts
                ,y.intervals_number
            from (
                select p.dts_beg as last_added_watch_row_dts
                from perf_log p
                where p.unit = 'perf_watch_interval'
                order by dts_beg desc rows 1
            ) x
            join (
                select
                    dateadd( p.scan_bak_minutes minute to p.dts_beg) as first_measured_start_dts
                    ,p.dts_beg as last_job_finish_dts
                    ,p.intervals_number
                from
                ( -- since 03.09.2015:
                    select
                        p.*
                        , -abs( :a_last_hours * 60 + :a_last_mins ) as scan_bak_minutes
                        , :a_intervals_number as intervals_number
                    from perf_log p
                ) p
                -- nb: do NOT use inner join here (bad plan with sort)
                where exists(select 1 from business_ops b where b.unit=p.unit order by b.unit)
                order by p.dts_beg desc
                rows 1
            ) y on 1=1
        )
        ,d as(
            select
                a.first_job_start_dts
                ,a.last_job_finish_dts
                ,1+datediff(second from a.first_job_start_dts to a.last_job_finish_dts) / a.intervals_number as sec_for_one_interval
            from a
        )
        --select * from d
        ,p as(
            select
                t.unit
                ,b.info
                ,1+cast(datediff(second from d.first_job_start_dts to t.dts_end) / d.sec_for_one_interval as int) as interval_no
                ,count(*) cnt_success
                ,avg( 1000 * t.fetches / nullif(t.elapsed_ms,0) ) fetches_per_second
                ,avg( 1000 * t.marks / nullif(t.elapsed_ms,0) ) marks_per_second
                ,avg( 100.00 * t.reads/nullif(t.fetches,0) ) reads_to_fetches_prc
                ,avg( 100.00 * t.writes/nullif(t.marks,0) ) writes_to_marks_prc
                --,count( nullif(t.success,0) ) cnt_ok
                --,count( nullif(t.success,1) ) cnt_err
                --,100.00 * count( nullif(t.success,1) ) / count(*) err_prc
                --,avg(  iif( g.fb_gdscode is null, g.elapsed_ms, null ) ) ok_avg_ms
                ,min(d.first_job_start_dts) as first_job_start_dts
                ,min(d.sec_for_one_interval) as sec_for_one_interval
            from trace_stat t
            join business_ops b on t.unit = b.unit
            join d on t.dts_end between d.first_job_start_dts and d.last_job_finish_dts -- only rows which are from THIS measured test run!
            where t.success = 1
            group by 1,2,3
        )
        --select * from p
        ,q as (
            select
                unit
                ,info
                ,interval_no
                ,cnt_success
                ,fetches_per_second
                ,marks_per_second
                ,reads_to_fetches_prc
                ,writes_to_marks_prc
                ,first_job_start_dts
                ,sec_for_one_interval
                ,dateadd( (interval_no-1) * sec_for_one_interval+1 second to first_job_start_dts ) as interval_beg
                ,dateadd( interval_no * sec_for_one_interval second to first_job_start_dts ) as interval_end
            from p
        )
         --select * from q
        select
            unit
            ,info
            ,interval_no
            ,cnt_success
            ,fetches_per_second
            ,marks_per_second
            ,reads_to_fetches_prc
            ,writes_to_marks_prc
            ,interval_beg
            ,interval_end
        from q
        into
            unit
            ,info
            ,interval_no
            ,cnt_success
            ,fetches_per_second
            ,marks_per_second
            ,reads_to_fetches_prc
            ,writes_to_marks_prc
            ,interval_beg
            ,interval_end
    do
        suspend;
end

^ -- srv_mon_perf_trace

create or alter procedure srv_mon_perf_trace_pivot (
    a_intervals_number smallint default 10,
    a_last_hours smallint default 3,
    a_last_mins smallint default 0
)
returns (
    traced_data varchar(30),
    interval_no smallint,
    sp_client_order bigint,
    sp_cancel_client_order bigint,
    sp_supplier_order bigint,
    sp_cancel_supplier_order bigint,
    sp_supplier_invoice bigint,
    sp_cancel_supplier_invoice bigint,
    sp_add_invoice_to_stock bigint,
    sp_cancel_adding_invoice bigint,
    sp_customer_reserve bigint,
    sp_cancel_customer_reserve bigint,
    sp_reserve_write_off bigint,
    sp_cancel_write_off bigint,
    sp_pay_from_customer bigint,
    sp_cancel_pay_from_customer bigint,
    sp_pay_to_supplier bigint,
    sp_cancel_pay_to_supplier bigint,
    srv_make_invnt_saldo bigint,
    srv_make_money_saldo bigint,
    srv_recalc_idx_stat bigint,
    interval_beg timestamp,
    interval_end  timestamp
) as
begin

    -- Report based on result of parsing TRACE log which was started by
    -- ISQL session #1 when config parameter trc_unit_perf = 1.
    -- Data for each business operation are displayed separately because
    -- they depends on execution plans and can not be compared each other.
    -- We have to analyze only RATIOS between reads/fetches and writes/marks,
    -- and also values of speed (fetches and marks per second) instead of
    -- absolute their values.

    a_intervals_number = iif( a_intervals_number <= 0, 10, a_intervals_number);
    a_last_hours = abs( a_last_hours );
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    for
        with recursive
        a as(
            -- reduce needed number of minutes from most last event of some SP starts:
            -- 18.07.2014: handle only data which belongs to LAST job.
            -- Record with p.unit = 'perf_watch_interval' is added in
            -- oltp_isql_run_worker.bat before FIRST isql will be launched
            select
                maxvalue( x.last_added_watch_row_dts, y.first_trace_statd_start_dts ) as first_job_start_dts
                ,y.last_job_finish_dts
                ,y.intervals_number
            from (
                select p.dts_beg as last_added_watch_row_dts
                from perf_log p
                where p.unit = 'perf_watch_interval'
                order by dts_beg desc rows 1
            ) x
            join (
                select
                    dateadd( p.scan_bak_minutes minute to p.dts_beg) as first_trace_statd_start_dts
                    ,p.dts_beg as last_job_finish_dts
                    ,p.intervals_number
                from
                ( -- since 03.09.2015:
                    select
                        p.*
                        , -abs( :a_last_hours * 60 + :a_last_mins ) as scan_bak_minutes
                        , :a_intervals_number as intervals_number
                    from perf_log p
                ) p
                -- nb: do NOT use inner join here (bad plan with sort)
                where exists(select 1 from business_ops b where b.unit=p.unit order by b.unit)
                order by p.dts_beg desc
                rows 1
            ) y on 1=1
        )
        ,d as(
            select
                a.first_job_start_dts
                ,a.last_job_finish_dts
                ,1+datediff(second from a.first_job_start_dts to a.last_job_finish_dts) / a.intervals_number as sec_for_one_interval
            from a
        )
        --select * from d
        ,p as(
            select
                t.unit
                ,b.info
                ,1+cast(datediff(second from d.first_job_start_dts to t.dts_end) / d.sec_for_one_interval as int) as interval_no
                ,count(*) cnt_success
                ,avg( 1000 * t.fetches / nullif(t.elapsed_ms,0) ) fetches_per_second
                ,avg( 1000 * t.marks / nullif(t.elapsed_ms,0) ) marks_per_second
                ,avg( 100.00 * t.reads/nullif(t.fetches,0) ) reads_to_fetches_prc
                ,avg( 100.00 * t.writes/nullif(t.marks,0) ) writes_to_marks_prc
                --,count( nullif(t.success,0) ) cnt_ok
                --,count( nullif(t.success,1) ) cnt_err
                --,100.00 * count( nullif(t.success,1) ) / count(*) err_prc
                --,avg(  iif( g.fb_gdscode is null, g.elapsed_ms, null ) ) ok_avg_ms
                ,min(d.first_job_start_dts) as first_job_start_dts
                ,min(d.sec_for_one_interval) as sec_for_one_interval
            from trace_stat t
            join business_ops b on t.unit = b.unit
            join d on t.dts_end between d.first_job_start_dts and d.last_job_finish_dts -- only rows which are from THIS trace_statd test run!
            where t.success = 1
            group by 1,2,3
        )
        --select * from p
        ,q as (
            select
                unit
                ,info
                ,interval_no
                ,cnt_success
                ,fetches_per_second
                ,marks_per_second
                ,reads_to_fetches_prc
                ,writes_to_marks_prc
                ,first_job_start_dts
                ,sec_for_one_interval
                ,dateadd( (interval_no-1) * sec_for_one_interval+1 second to first_job_start_dts ) as interval_beg
                ,dateadd( interval_no * sec_for_one_interval second to first_job_start_dts ) as interval_end
            from p
        )
         --select * from q
        , n as (
          select 1 i from rdb$database union all
          select n.i+1 from n where n.i+1<=4
        )

        select
            decode(n.i, 1, 'fetches per second', 2, 'marks per second', 3, 'reads/fetches*100', 'writes/marks*100') as trace_stat
            ,interval_no
            ,max( iif(unit='sp_client_order', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as                  sp_client_order
            ,max( iif(unit='sp_cancel_client_order', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as           sp_cancel_client_order
            ,max( iif(unit='sp_supplier_order', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as                sp_supplier_order
            ,max( iif(unit='sp_cancel_supplier_order', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as         sp_cancel_supplier_order
            ,max( iif(unit='sp_supplier_invoice', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as              sp_supplier_invoice
            ,max( iif(unit='sp_cancel_supplier_invoice', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as       sp_cancel_supplier_invoice
            ,max( iif(unit='sp_add_invoice_to_stock', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as          sp_add_invoice_to_stock
            ,max( iif(unit='sp_cancel_adding_invoice', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as         sp_cancel_adding_invoice
            ,max( iif(unit='sp_customer_reserve', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as              sp_customer_reserve
            ,max( iif(unit='sp_cancel_customer_reserve', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as       sp_cancel_customer_reserve
            ,max( iif(unit='sp_reserve_write_off', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as             sp_reserve_write_off
            ,max( iif(unit='sp_cancel_write_off', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as              sp_cancel_write_off
            ,max( iif(unit='sp_pay_from_customer', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as             sp_pay_from_customer
            ,max( iif(unit='sp_cancel_pay_from_customer', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as      sp_cancel_pay_from_customer
            ,max( iif(unit='sp_pay_to_supplier', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as               sp_pay_to_supplier
            ,max( iif(unit='sp_cancel_pay_to_supplier', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as        sp_cancel_pay_to_supplier
            ,max( iif(unit='srv_make_invnt_saldo', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as             srv_make_invnt_saldo
            ,max( iif(unit='srv_make_money_saldo', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as             srv_make_money_saldo
            ,max( iif(unit='srv_recalc_idx_stat', decode(n.i, 1, fetches_per_second, 2, marks_per_second, 3, reads_to_fetches_prc, writes_to_marks_prc), null) ) as              srv_recalc_idx_stat
            ,interval_beg
            ,interval_end
        from q
        cross join n
        group by n.i, interval_no, interval_beg, interval_end
        into
            traced_data
            ,interval_no
            ,sp_client_order
            ,sp_cancel_client_order
            ,sp_supplier_order
            ,sp_cancel_supplier_order
            ,sp_supplier_invoice
            ,sp_cancel_supplier_invoice
            ,sp_add_invoice_to_stock
            ,sp_cancel_adding_invoice
            ,sp_customer_reserve
            ,sp_cancel_customer_reserve
            ,sp_reserve_write_off
            ,sp_cancel_write_off
            ,sp_pay_from_customer
            ,sp_cancel_pay_from_customer
            ,sp_pay_to_supplier
            ,sp_cancel_pay_to_supplier
            ,srv_make_invnt_saldo
            ,srv_make_money_saldo
            ,srv_recalc_idx_stat
            ,interval_beg
            ,interval_end
    do
        suspend;
end

^ -- srv_mon_perf_trace_pivot


create or alter procedure srv_mon_idx
returns (
    tab_name dm_dbobj,
    idx_name dm_dbobj,
    last_stat double precision,
    curr_stat double precision,
    diff_stat double precision,
    last_done timestamp
) as
    declare v_last_recalc_trn bigint;
begin

    select p.trn_id
    from perf_log p
    where p.unit starting with 'srv_recalc_idx_stat_'
    order by p.trn_id desc rows 1
    into v_last_recalc_trn;

    -- SP for analyzing results of index statistics recalculation:
    -- ###########################################################
    for
        select
            t.tab_name
            ,t.idx_name
            ,t.last_stat
            ,r.rdb$statistics
            ,t.last_stat - r.rdb$statistics
            ,t.last_done
        from (
            select
                g.info as tab_name
                ,substring(g.unit from char_length('srv_recalc_idx_stat_')+1 ) as idx_name
                ,g.aux1 as last_stat
                ,g.dts_end as last_done
            from perf_log g
            where g.trn_id = :v_last_recalc_trn
        ) t
        join rdb$indices r on t.idx_name = r.rdb$index_name
    into
        tab_name,
        idx_name,
        last_stat,
        curr_stat,
        diff_stat,
        last_done
    do suspend;
end

^ -- srv_mon_idx
--------------------------------------------------------------------------------

create or alter procedure srv_fill_mon(
    a_rowset bigint default null -- not null ==> gather info from tmp$mo_log (2 rows); null ==> gather info from ALL attachments
)
returns(
    rows_added int
)
as
    declare v_curr_trn bigint;
    declare v_total_stat_added_rows int = 0;
    declare v_table_stat_added_rows int = 0;
    declare v_dummy bigint;
    declare v_info dm_info;
    declare v_this dm_dbobj = 'srv_fill_mon';
begin
    rows_added = -1;

    if ( fn_remote_process() NOT containing 'IBExpert'
         and
         coalesce(rdb$get_context('USER_SESSION', 'ENABLE_MON_QUERY'), 0) = 0
       ) then
    begin
        rdb$set_context( 'USER_SESSION','MON_INFO', 'mon$_dis!'); -- to be displayed in log of 1run_oltp_emul.bat
        suspend;
        --###
        exit;
        --###
    end
    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    v_curr_trn = current_transaction;
    if ( a_rowset is NULL  ) then -- gather data from ALL attachments (separate call of this SP)
        begin
            in autonomous transaction do
            begin
                insert into mon_log(
                    ----------------------- ALL attachments: set #1
                    --dts,
                    sec,
                    usr,
                    att_id,
                    ----------------------- ALL attachments: set #2
                    pg_reads,
                    pg_writes,
                    pg_fetches,
                    pg_marks,
                    ----------------------- ALL attachments: set #3
                    rec_inserts,
                    rec_updates,
                    rec_deletes,
                    rec_backouts,
                    rec_purges,
                    rec_expunges,
                    rec_seq_reads,
                    rec_idx_reads,
                    ----------------------- ALL attachments: set #4
                    rec_rpt_reads,
                    bkv_reads, -- mon$backversion_reads, since rev. 60012, 28.08.2014 19:16
                    frg_reads,
                    ----------------------- ALL attachments: set #5
                    rec_locks,
                    rec_waits,
                    rec_confl,
                    ----------------------- ALL attachments: set #6
                    mem_used,
                    mem_alloc,
                    ----------------------- ALL attachments: set #7
                    stat_id,
                    server_pid,
                    remote_pid,
                    ----------------------- ALL attachments: set #8
                    ip,
                    remote_process,
                    dump_trn,
                    unit,
                    add_info
                )
                -- 09.08.2014
                select     
                    ----------------------- ALL attachments: set #1
                    --current_time dts
                    datediff(second from current_date-1 to current_timestamp ) sec
                    -- mon$attachments(1):
                    ,a.mon$user mon_user
                    ,a.mon$attachment_id attach_id
                    ----------------------- ALL attachments: set #2
                    -- mon$io_stats:
                    ,i.mon$page_reads reads
                    ,i.mon$page_writes writes     
                    ,i.mon$page_fetches fetches     
                    ,i.mon$page_marks marks     
                    ----------------------- ALL attachments: set #3
                    -- mon$record_stats:     
                    ,r.mon$record_inserts ins_cnt
                    ,r.mon$record_updates upd_cnt     
                    ,r.mon$record_deletes del_cnt     
                    ,r.mon$record_backouts bk_outs     
                    ,r.mon$record_purges purges     
                    ,r.mon$record_expunges expunges     
                    ,r.mon$record_seq_reads seq_reads     
                    ,r.mon$record_idx_reads idx_reads     
                    ----------------------- ALL attachments: set #4
                    ,r.mon$record_rpt_reads
                    ,r.mon$backversion_reads -- since rev. 60012, 28.08.2014 19:16
                    ,r.mon$fragment_reads
                    ----------------------- ALL attachments: set #5
                    ,r.mon$record_locks
                    ,r.mon$record_waits
                    ,r.mon$record_conflicts
                    ----------------------- ALL attachments: set #6
                    -- mon$memory_usage:
                    ,u.mon$memory_used used_memory     
                    ,u.mon$memory_allocated alloc_by_OS     
                    ----------------------- ALL attachments: set #7
                    -- mon$attachments(2):
                    ,a.mon$stat_id       stat_id
                    ,a.mon$server_pid    server_PID     
                    ,a.mon$remote_pid    remote_PID     
                    ----------------------- ALL attachments: set #8
                    ,a.mon$remote_address remote_IP     
                    -- aux info:     
                    ,right(a.mon$remote_process,30) remote_process     
                    ,:v_curr_trn
                    ,:v_this
                    ,'all_attaches'
                from mon$attachments a     
                --left join mon$statements s on a.mon$attachment_id = s.mon$attachment_id     
                left join mon$memory_usage u on a.mon$stat_id=u.mon$stat_id     
                left join mon$io_stats i on a.mon$stat_id=i.mon$stat_id     
                left join mon$record_stats r on a.mon$stat_id=r.mon$stat_id     
                where     
                  a.mon$attachment_id<>current_connection 
                  order by 
                  iif( a.mon$user in ('Garbage Collector', 'Cache Writer'  )
                      ,1 
                      , iif( a.mon$remote_process containing 'gfix'
                            ,2 
                            ,iif( a.mon$remote_process containing 'nbackup'
                                  or a.mon$remote_process containing 'gbak'
                                  or a.mon$remote_process containing 'gstat'
                                 ,3 
                                 ,1000+a.mon$attachment_id
                                 )
                            )
                      )
                ;
                v_total_stat_added_rows = row_count;
            end -- in AT
        end
    else -- input arg :a_rowset is NOT null ==> gather data from tmp$mon_log (were added there in calls before and after application unit from tmp_random_run.sql)
        begin
            insert into mon_log(
                ---------------- CURRENT attachment only: set #1
                rowset,
                --dts,
                sec,
                usr,
                att_id,
                trn_id,
                ---------------- CURRENT attachment only: set #2
                pg_reads,
                pg_writes,
                pg_fetches,
                pg_marks,
                ---------------- CURRENT attachment only: set #3
                rec_inserts,
                rec_updates,
                rec_deletes,
                rec_backouts,
                rec_purges,
                rec_expunges,
                --------------- CURRENT attachment only: set #4
                rec_seq_reads,
                rec_idx_reads,
                rec_rpt_reads,
                --------------- CURRENT attachment only: set #5
                bkv_reads, -- mon$backversion_reads, since rev. 60012, 28.08.2014 19:16
                frg_reads,
                --------------- CURRENT attachment only: set #6
                rec_locks,
                rec_waits,
                rec_confl,
                --------------- CURRENT attachment only: set #7
                mem_used,
                mem_alloc,
                --------------- CURRENT attachment only: set #8
                stat_id,
                server_pid,
                remote_pid,
                --------------- CURRENT attachment only: set #9
                ip,
                remote_process,
                dump_trn,
                --------------- CURRENT attachment only: set #10
                unit,
                add_info,
                fb_gdscode,
                elapsed_ms -- added 08.09.2014
            )
            select
                -------------------------------  set #1: dts, sec, usr, att_id
                 t.rowset
                --,current_time
                ,datediff(second from current_date-1 to current_timestamp )
                ,current_user
                ,current_connection
                ,max( t.trn_id )
                ------------ CURRENT attachment only: set #2: pg_reads,pg_writes,pg_fetches,pg_marks
                ,sum( t.mult * t.pg_reads)   -- t.mult = -1 for first meause, +1 for second -- see srv_fill_tmp_mon
                ,sum( t.mult * t.pg_writes)
                ,sum( t.mult * t.pg_fetches)
                ,sum( t.mult * t.pg_marks)
                ------------ CURRENT attachment only: set #3: inserts,updates,deletes,backouts,purges,expunges,
                ,sum( t.mult * t.rec_inserts)
                ,sum( t.mult * t.rec_updates)
                ,sum( t.mult * t.rec_deletes)
                ,sum( t.mult * t.rec_backouts)
                ,sum( t.mult * t.rec_purges)
                ,sum( t.mult * t.rec_expunges)
                ------------ CURRENT attachment only: set #4: seq_reads,idx_reads,rpt_reads
                ,sum( t.mult * t.rec_seq_reads)
                ,sum( t.mult * t.rec_idx_reads)
                ,sum( t.mult * t.rec_rpt_reads) -- <<< since rev. 60005 27.08.2014 18:52
                ------------ CURRENT attachment only: set #5: ver_reads, frg_reads (since rev. 59953 05.08.2014 08:46)
                ,sum( t.mult * t.bkv_reads) -- mon$backversion_reads, since rev. 60012, 28.08.2014 19:16
                ,sum( t.mult * t.frg_reads)
                ------------- CURRENT attachment only: set #6: rec_locks,rec_waits,rec_confl (since rev. 59953)
                ,sum( t.mult * t.rec_locks)
                ,sum( t.mult * t.rec_waits)
                ,sum( t.mult * t.rec_confl)
                -------------- CURRENT attachment only: set #7: mem_used,mem_alloc
                ,sum( t.mult * t.mem_used)
                ,sum( t.mult * t.mem_alloc)
                -------------- CURRENT attachment only: set #8 stat_id,server_pid,remote_pid
                ,max( t.stat_id )
                ,max( t.server_pid )
                ,rdb$get_context('SYSTEM', 'CLIENT_PID')
                --------------- CURRENT attachment only: set #9: ip,remote_process,dump_trn
                ,fn_remote_address() --  rdb$get_context('SYSTEM', 'CLIENT_ADDRESS')
                ,right( fn_remote_process(), 30) -- rdb$get_context('SYSTEM', 'CLIENT_PROCESS')
                ,:v_curr_trn
                --------------- CURRENT attachment only: set #10 unit,add_info
                ,max(unit)
                ,max(add_info)
                ,max(fb_gdscode)
                ,datediff(millisecond from min(t.dts) to max(t.dts) )
            from tmp$mon_log t
            where t.rowset = :a_rowset
            group by t.rowset;

            v_total_stat_added_rows = row_count;

            delete from tmp$mon_log t where t.rowset = :a_rowset;

            -----------------------------------------
            -- 29.08.2014: gather data from tmp$mon_log_table_stats to mon_log_table_stats
            insert into mon_log_table_stats(
                 rowset                     --  1
                ,table_name
                ,att_id
                ,table_id
                ,is_system_table            --  5
                ,rel_type
                ,unit
                ,fb_gdscode
                ,rec_inserts
                ,rec_updates                -- 10
                ,rec_deletes
                ,rec_backouts
                ,rec_purges
                ,rec_expunges
                ,rec_seq_reads              -- 15
                ,rec_idx_reads
                ,rec_rpt_reads
                ,bkv_reads
                ,frg_reads
                ,rec_locks                  -- 20
                ,rec_waits
                ,rec_confl
                ,trn_id
                ,stat_id                    -- 24
            )
            select
                 s.rowset                                      --  1
                ,s.table_name as tab_name -- :: NB :: mon$table_stats has field mon$table_NAME rather than mon$table_ID
                ,current_connection as att_id
                ,max( r.rdb$relation_id ) as tab_id
                ,max( r.rdb$system_flag ) as sys_flag          --  5
                ,max( r.rdb$relation_type ) as rel_type
                ,max( s.unit ) -- can be NULL before random choise of app unit!
                ,sum( s.mult * s.fb_gdscode )   -- t.mult = -1 for first measure, +1 for second -- see srv_fill_tmp_mon
                ,sum( s.mult * s.rec_inserts )
                ,sum( s.mult * s.rec_updates )                 -- 10
                ,sum( s.mult * s.rec_deletes )
                ,sum( s.mult * s.rec_backouts )
                ,sum( s.mult * s.rec_purges )
                ,sum( s.mult * s.rec_expunges )
                ,sum( s.mult * s.rec_seq_reads )               -- 15
                ,sum( s.mult * s.rec_idx_reads )
                ,sum( s.mult * s.rec_rpt_reads )
                ,sum( s.mult * s.bkv_reads )
                ,sum( s.mult * s.frg_reads )
                ,sum( s.mult * s.rec_locks )                   -- 20
                ,sum( s.mult * s.rec_waits )
                ,sum( s.mult * s.rec_confl )
                ,max( s.trn_id )
                ,max( s.stat_id )                              -- 24
            from tmp$mon_log_table_stats s
            join rdb$relations r on s.table_name = r.rdb$relation_name
            where s.rowset = :a_rowset
            group by s.rowset, s.table_name;

            v_table_stat_added_rows = row_count;

            delete from tmp$mon_log_table_stats s where s.rowset = :a_rowset;

        end

    rows_added = v_total_stat_added_rows + v_table_stat_added_rows;
    v_info='rows added: total_stat='||v_total_stat_added_rows||', table_stat='||v_table_stat_added_rows;
    -- ::: nb ::: do NOT use the name 'ADD_INFO', it is reserved to common app unit result!
    rdb$set_context( 'USER_SESSION','MON_INFO', v_info ); -- to be displayed in log of 1run_oltp_emul.bat
    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(0, v_this, null, v_info );

    suspend;

when any do
    begin
        rdb$set_context( 'USER_SESSION','MON_INFO', 'gds='||gdscode );
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            '',
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- srv_fill_mon

--------------------------------------------------------------------------------

create or alter procedure srv_fill_tmp_mon(
    a_rowset dm_idb,
    a_ignore_system_tables smallint default 1,
    a_unit dm_unit default null,
    a_info dm_info default null,
    a_gdscode int default null
)
returns(
    rows_added int
)
as
    declare v_mult dm_sign;
    declare v_curr_trn bigint;
    declare v_this dm_dbobj = 'srv_fill_tmp_mon';
    declare v_total_stat_added_rows int;
    declare v_table_stat_added_rows int;
    declare v_info dm_info;
begin
    rows_added = -1;

    if ( fn_remote_process() NOT containing 'IBExpert'
         and
         coalesce(rdb$get_context('USER_SESSION', 'ENABLE_MON_QUERY'), 0) = 0
       ) then
    begin
        rdb$set_context( 'USER_SESSION','MON_INFO', 'mon$_dis!'); -- to be displayed in log of 1run_oltp_emul.bat
        suspend;
        --###
        exit;
        --###
    end
    -- Check that table `ext_stoptest` (external text file) is EMPTY,
    -- otherwise raises e`xception to stop test:
    execute procedure sp_check_to_stop_work;

    -- add to performance log timestamp about start/finish this unit:
    execute procedure sp_add_perf_log(1, v_this);

    v_mult = iif( exists(select * from tmp$mon_log g where g.rowset is not distinct from :a_rowset), 1, -1);
    v_curr_trn = iif( v_mult = 1, current_transaction, null);

    insert into tmp$mon_log( -- NB: on c`ommit PRESERVE rows!
        -- mon$io_stats:
        pg_reads
       ,pg_writes
       ,pg_fetches
       ,pg_marks
        -- mon$record_stats:     
       ,rec_inserts
       ,rec_updates
       ,rec_deletes
       ,rec_backouts
       ,rec_purges
       ,rec_expunges
       ,rec_seq_reads
       ,rec_idx_reads

       ,rec_rpt_reads
       ,bkv_reads -- mon$backversion_reads, since rev. 60012, 28.08.2014 19:16
       ,frg_reads

       ,rec_locks
       ,rec_waits
       ,rec_confl
       ------------
       ,mem_used
       ,mem_alloc
       ,stat_id
       ,server_pid
       ------------
       ,rowset
       ,unit
       ,add_info
       ,fb_gdscode
       ,mult
       ,trn_id
    )
    select
        -- mon$io_stats:
         i.mon$page_reads
        ,i.mon$page_writes
        ,i.mon$page_fetches
        ,i.mon$page_marks
        -- mon$record_stats:     
        ,r.mon$record_inserts
        ,r.mon$record_updates
        ,r.mon$record_deletes
        ,r.mon$record_backouts
        ,r.mon$record_purges
        ,r.mon$record_expunges
        ,r.mon$record_seq_reads
        ,r.mon$record_idx_reads
    
        ,r.mon$record_rpt_reads
        ,r.mon$backversion_reads -- since rev. 60012, 28.08.2014 19:16
        ,r.mon$fragment_reads
    
        ,r.mon$record_locks
        ,r.mon$record_waits
        ,r.mon$record_conflicts
        ------------------------
        ,u.mon$memory_used
        ,u.mon$memory_allocated
        ,a.mon$stat_id
        ,a.mon$server_pid
        ------------------------
        ,:a_rowset
        ,:a_unit
        ,:a_info
        ,:a_gdscode
        ,:v_mult
        ,:v_curr_trn
    from mon$attachments a
    --left join mon$statements s on a.mon$attachment_id = s.mon$attachment_id     
    left join mon$memory_usage u on a.mon$stat_id=u.mon$stat_id     
    left join mon$io_stats i on a.mon$stat_id=i.mon$stat_id     
    left join mon$record_stats r on a.mon$stat_id=r.mon$stat_id     
    where     
      a.mon$attachment_id = current_connection;

    v_total_stat_added_rows = row_count;

    -- 29.08.2014: use also mon$table_stats to analyze per table:
    insert into tmp$mon_log_table_stats(
        table_name
        ,rec_inserts
        ,rec_updates
        ,rec_deletes
        ,rec_backouts
        ,rec_purges
        ,rec_expunges
        ---------
        ,rec_seq_reads
        ,rec_idx_reads
        ,rec_rpt_reads
        ,bkv_reads
        ,frg_reads
        ---------
        ,rec_locks
        ,rec_waits
        ,rec_confl
        ---------
        ,rowset
        ,unit
        ,fb_gdscode
        ,stat_id
        ,mult
        ,trn_id
    )
    select
        t.mon$table_name
        ,r.mon$record_inserts
        ,r.mon$record_updates
        ,r.mon$record_deletes
        ,r.mon$record_backouts
        ,r.mon$record_purges
        ,r.mon$record_expunges
        -----------
        ,r.mon$record_seq_reads
        ,r.mon$record_idx_reads
        ,r.mon$record_rpt_reads
        ,r.mon$backversion_reads
        ,r.mon$fragment_reads
        -----------
        ,r.mon$record_locks
        ,r.mon$record_waits
        ,r.mon$record_conflicts
        ------------
        ,:a_rowset
        ,:a_unit
        ,:a_gdscode
        ,a.mon$stat_id
        ,:v_mult
        ,:v_curr_trn
    from mon$record_stats r
    join mon$table_stats t on r.mon$stat_id = t.mon$record_stat_id
    join mon$attachments a on t.mon$stat_id = a.mon$stat_id
    where
        a.mon$attachment_id = current_connection
        and ( :a_ignore_system_tables = 0 or t.mon$table_name not starting with 'RDB$' );

    v_table_stat_added_rows = row_count;

    -- add to performance log timestamp about start/finish this unit:
    v_info = 'unit: '||coalesce(a_unit,'<?>')
            || ', rowset='||coalesce(a_rowset,'<?>')
            || ', rows added: total_stat='||v_total_stat_added_rows||', table_stat='||v_table_stat_added_rows;
    execute procedure sp_add_perf_log(0, v_this, null, v_info );

    rows_added = v_total_stat_added_rows + v_table_stat_added_rows; -- out arg

    suspend;

when any do
    begin
        -- ::: nb ::: do NOT use the name 'ADD_INFO', it;s reserved to common app unit result!
        rdb$set_context( 'USER_SESSION','MON_INFO', 'gds='||gdscode ); -- to be displayed in isql output, see 1run_oltp_emul.bat
        execute procedure sp_add_to_abend_log(
            '',
            gdscode,
            '',
            v_this,
            fn_halt_sign(gdscode) -- ::: nb ::: 1 ==> force get full stack, ignoring settings `DISABLE_CALL_STACK` value, and HALT test
        );

        --#######
        exception;  -- ::: nb ::: anonimous but in when-block!
        --#######
    end

end

^ -- srv_fill_tmp_mon

create or alter procedure srv_mon_stat_per_units (
    a_last_hours smallint default 3,
    a_last_mins smallint default 0 )
returns (
    unit dm_unit
   ,iter_counts bigint
   ,avg_elap_ms bigint
   ,avg_rec_reads_sec numeric(12,2)
   ,avg_rec_dmls_sec numeric(12,2)
   ,avg_bkos_sec numeric(12,2)
   ,avg_purg_sec numeric(12,2)
   ,avg_xpng_sec numeric(12,2)
   ,avg_fetches_sec numeric(12,2)
   ,avg_marks_sec numeric(12,2)
   ,avg_reads_sec numeric(12,2)
   ,avg_writes_sec numeric(12,2)
   ,avg_seq bigint
   ,avg_idx bigint
   ,avg_rpt bigint
   ,avg_bkv bigint
   ,avg_frg bigint
   ,avg_bkv_per_rec numeric(12,2)
   ,avg_frg_per_rec numeric(12,2)
   ,avg_ins bigint
   ,avg_upd bigint
   ,avg_del bigint
   ,avg_bko bigint
   ,avg_pur bigint
   ,avg_exp bigint
   ,avg_fetches bigint
   ,avg_marks bigint
   ,avg_reads bigint
   ,avg_writes bigint
   ,avg_locks bigint
   ,avg_confl bigint
   ,max_seq bigint
   ,max_idx bigint
   ,max_rpt bigint
   ,max_bkv bigint
   ,max_frg bigint
   ,max_bkv_per_rec numeric(12,2)
   ,max_frg_per_rec numeric(12,2)
   ,max_ins bigint
   ,max_upd bigint
   ,max_del bigint
   ,max_bko bigint
   ,max_pur bigint
   ,max_exp bigint
   ,max_fetches bigint
   ,max_marks bigint
   ,max_reads bigint
   ,max_writes bigint
   ,max_locks bigint
   ,max_confl bigint
   ,job_beg varchar(16)
   ,job_end varchar(16)
) as
    declare v_report_beg timestamp;
    declare v_report_end timestamp;
begin
    -- SP for detailed performance analysis: count of operations
    -- (NOT only business ops; including BOTH successful and failed ones),
    -- count of errors (including by their types)
    a_last_hours = abs( coalesce(a_last_hours, 3) );
    a_last_mins = coalesce(a_last_mins, 0);
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    select p.last_launch_beg, p.last_launch_end
    from srv_get_last_launch_beg_end( :a_last_hours, :a_last_mins ) p
    into v_report_beg, v_report_end;

    for
        -- 29.08.2014: data from measuring statistics per each unit
        -- (need FB rev. >= 60013: new mon$ counters were introduced, 28.08.2014)
        -- 25.01.2015: added rec_locks, rec_confl.
        -- 06.02.2015: reorder columns, made all `max` values most-right
        select
             m.unit
            ,count(*) iter_counts
            -------------- speed -------------
            ,avg(m.elapsed_ms) avg_elap_ms
            ,avg(1000.00 * ( (m.rec_seq_reads + m.rec_idx_reads + m.bkv_reads ) / nullif(m.elapsed_ms,0))  ) avg_rec_reads_sec
            ,avg(1000.00 * ( (m.rec_inserts + m.rec_updates + m.rec_deletes ) / nullif(m.elapsed_ms,0))  ) avg_rec_dmls_sec
            ,avg(1000.00 * ( m.rec_backouts / nullif(m.elapsed_ms,0))  ) avg_bkos_sec
            ,avg(1000.00 * ( m.rec_purges / nullif(m.elapsed_ms,0))  ) avg_purg_sec
            ,avg(1000.00 * ( m.rec_expunges / nullif(m.elapsed_ms,0))  ) avg_xpng_sec
            ,avg(1000.00 * ( m.pg_fetches / nullif(m.elapsed_ms,0)) ) avg_fetches_sec
            ,avg(1000.00 * ( m.pg_marks / nullif(m.elapsed_ms,0)) ) avg_marks_sec
            ,avg(1000.00 * ( m.pg_reads / nullif(m.elapsed_ms,0)) ) avg_reads_sec
            ,avg(1000.00 * ( m.pg_writes / nullif(m.elapsed_ms,0)) ) avg_writes_sec
            -------------- reads ---------------
            ,avg(m.rec_seq_reads) avg_seq
            ,avg(m.rec_idx_reads) avg_idx
            ,avg(m.rec_rpt_reads) avg_rpt
            ,avg(m.bkv_reads) avg_bkv
            ,avg(m.frg_reads) avg_frg
            ,avg(m.bkv_per_seq_idx_rpt) avg_bkv_per_rec
            ,avg(m.frg_per_seq_idx_rpt) avg_frg_per_rec
            ---------- modifications ----------
            ,avg(m.rec_inserts) avg_ins
            ,avg(m.rec_updates) avg_upd
            ,avg(m.rec_deletes) avg_del
            ,avg(m.rec_backouts) avg_bko
            ,avg(m.rec_purges) avg_pur
            ,avg(m.rec_expunges) avg_exp
            --------------- io -----------------
            ,avg(m.pg_fetches) avg_fetches
            ,avg(m.pg_marks) avg_marks
            ,avg(m.pg_reads) avg_reads
            ,avg(m.pg_writes) avg_writes
            ----------- locks and conflicts ----------
            ,avg(m.rec_locks) avg_locks
            ,avg(m.rec_confl) avg_confl
            --- 06.02.2015 moved here all MAX values, separate them from AVG ones: ---
            ,max(m.rec_seq_reads) max_seq
            ,max(m.rec_idx_reads) max_idx
            ,max(m.rec_rpt_reads) max_rpt
            ,max(m.bkv_reads) max_bkv
            ,max(m.frg_reads) max_frg
            ,max(m.bkv_per_seq_idx_rpt) max_bkv_per_rec
            ,max(m.frg_per_seq_idx_rpt) max_frg_per_rec
            ,max(m.rec_inserts) max_ins
            ,max(m.rec_updates) max_upd
            ,max(m.rec_deletes) max_del
            ,max(m.rec_backouts) max_bko
            ,max(m.rec_purges) max_pur
            ,max(m.rec_expunges) max_exp
            ,max(m.pg_fetches) max_fetches
            ,max(m.pg_marks) max_marks
            ,max(m.pg_reads) max_reads
            ,max(m.pg_writes) max_writes
            ,max(m.rec_locks) max_locks
            ,max(m.rec_confl) max_confl
            ,left(cast(:v_report_beg as varchar(24)),16)
            ,left(cast(:v_report_end as varchar(24)),16)
        from mon_log m
        where m.dts between :v_report_beg and :v_report_end
        group by unit
    into
        unit
       ,iter_counts
       ,avg_elap_ms
       ,avg_rec_reads_sec
       ,avg_rec_dmls_sec
       ,avg_bkos_sec
       ,avg_purg_sec
       ,avg_xpng_sec
       ,avg_fetches_sec
       ,avg_marks_sec
       ,avg_reads_sec
       ,avg_writes_sec
       ,avg_seq
       ,avg_idx
       ,avg_rpt
       ,avg_bkv
       ,avg_frg
       ,avg_bkv_per_rec
       ,avg_frg_per_rec
       ,avg_ins
       ,avg_upd
       ,avg_del
       ,avg_bko
       ,avg_pur
       ,avg_exp
       ,avg_fetches
       ,avg_marks
       ,avg_reads
       ,avg_writes
       ,avg_locks
       ,avg_confl
       ,max_seq
       ,max_idx
       ,max_rpt
       ,max_bkv
       ,max_frg
       ,max_bkv_per_rec
       ,max_frg_per_rec
       ,max_ins
       ,max_upd
       ,max_del
       ,max_bko
       ,max_pur
       ,max_exp
       ,max_fetches
       ,max_marks
       ,max_reads
       ,max_writes
       ,max_locks
       ,max_confl
       ,job_beg
       ,job_end
    do
        suspend;
end

^ -- srv_mon_stat_per_units

create or alter procedure srv_mon_stat_per_tables (
    a_last_hours smallint default 3,
    a_last_mins smallint default 0 )
returns (
    table_name dm_dbobj
   ,unit dm_unit
   ,iter_counts bigint
   ,avg_seq bigint
   ,avg_idx bigint
   ,avg_rpt bigint
   ,avg_bkv bigint
   ,avg_frg bigint
   ,avg_bkv_per_rec numeric(12,2)
   ,avg_frg_per_rec numeric(12,2)
   ,avg_ins bigint
   ,avg_upd bigint
   ,avg_del bigint
   ,avg_bko bigint
   ,avg_pur bigint
   ,avg_exp bigint
   ,avg_locks bigint
   ,avg_confl bigint
   --,elapsed_minutes int
   ,max_seq bigint
   ,max_idx bigint
   ,max_rpt bigint
   ,max_bkv bigint
   ,max_frg bigint
   ,max_bkv_per_rec numeric(12,2)
   ,max_frg_per_rec numeric(12,2)
   ,max_ins bigint
   ,max_upd bigint
   ,max_del bigint
   ,max_bko bigint
   ,max_pur bigint
   ,max_exp bigint
   ,max_locks bigint
   ,max_confl bigint
   ,job_beg varchar(16)
   ,job_end varchar(16)
) as
    declare v_report_beg timestamp;
    declare v_report_end timestamp;
begin
    -- SP for detailed performance analysis: count of operations
    -- (NOT only business ops; including BOTH successful and failed ones),
    -- count of errors (including by their types)
    a_last_hours = abs( coalesce(a_last_hours, 3) );
    a_last_mins = coalesce(a_last_mins, 0);
    a_last_mins = iif( a_last_mins between 0 and 59, a_last_mins, 0 );

    select p.last_launch_beg, p.last_launch_end
    from srv_get_last_launch_beg_end( :a_last_hours, :a_last_mins ) p
    into v_report_beg, v_report_end;

    for
    select
         t.table_name
        ,t.unit
        ,count(*) iter_counts
        --------------- reads ---------------
        ,avg(t.rec_seq_reads) avg_seq
        ,avg(t.rec_idx_reads) avg_idx
        ,avg(t.rec_rpt_reads) avg_rpt
        ,avg(t.bkv_reads) avg_bkv
        ,avg(t.frg_reads) avg_frg
        ,avg(t.bkv_per_seq_idx_rpt) avg_bkv_per_rec
        ,avg(t.frg_per_seq_idx_rpt) avg_frg_per_rec
        ---------- modifications ----------
        ,avg(t.rec_inserts) avg_ins
        ,avg(t.rec_updates) avg_upd
        ,avg(t.rec_deletes) avg_del
        ,avg(t.rec_backouts) avg_bko
        ,avg(t.rec_purges) avg_pur
        ,avg(t.rec_expunges) avg_exp
        ----------- locks and conflicts ----------
        ,avg(t.rec_locks) avg_locks
        ,avg(t.rec_confl) avg_confl
        --,datediff( minute from min(t.dts) to max(t.dts) ) elapsed_minutes
        --- 06.02.2015 moved here all MAX values, separate them from AVG ones: ---
        ,max(t.rec_seq_reads) max_seq
        ,max(t.rec_idx_reads) max_idx
        ,max(t.rec_rpt_reads) max_rpt
        ,max(t.bkv_reads) max_bkv
        ,max(t.frg_reads) max_frg
        ,max(t.bkv_per_seq_idx_rpt) max_bkv_per_rec
        ,max(t.frg_per_seq_idx_rpt) max_frg_per_rec
        ,max(t.rec_inserts) max_ins
        ,max(t.rec_updates) max_upd
        ,max(t.rec_deletes) max_del
        ,max(t.rec_backouts) max_bko
        ,max(t.rec_purges) max_pur
        ,max(t.rec_expunges) max_exp
        ,max(t.rec_locks) max_locks
        ,max(t.rec_confl) max_confl
        ,left(cast(:v_report_beg as varchar(24)),16)
        ,left(cast(:v_report_end as varchar(24)),16)
    from mon_log_table_stats t
    where
        t.dts between :v_report_beg and :v_report_end
        and
          t.rec_seq_reads
        + t.rec_idx_reads
        + t.rec_rpt_reads
        + t.bkv_reads
        + t.frg_reads
        + t.rec_inserts
        + t.rec_updates
        + t.rec_deletes
        + t.rec_backouts
        + t.rec_purges
        + t.rec_expunges
        + t.rec_locks
        + t.rec_confl
        > 0
    group by t.table_name, t.unit
    into
        table_name
       ,unit
       ,iter_counts
       ,avg_seq
       ,avg_idx
       ,avg_rpt
       ,avg_bkv
       ,avg_frg
       ,avg_bkv_per_rec
       ,avg_frg_per_rec
       ,avg_ins
       ,avg_upd
       ,avg_del
       ,avg_bko
       ,avg_pur
       ,avg_exp
       ,avg_locks
       ,avg_confl
       --,elapsed_minutes
       ,max_seq
       ,max_idx
       ,max_rpt
       ,max_bkv
       ,max_frg
       ,max_bkv_per_rec
       ,max_frg_per_rec
       ,max_ins
       ,max_upd
       ,max_del
       ,max_bko
       ,max_pur
       ,max_exp
       ,max_locks
       ,max_confl
       ,job_beg
       ,job_end
    do
        suspend;
end

^ -- srv_mon_stat_per_tables

create or alter procedure srv_get_report_name(
     a_format varchar(20) default 'regular' -- 'regular' | 'benchmark'
    ,a_build varchar(50) default '' -- WI-V3.0.0.32136 or just '32136'
    ,a_num_of_sessions int default -1
    ,a_test_time_minutes int default -1
    ,a_prefix varchar(255) default ''
    ,a_suffix varchar(255) default ''
) returns (
    report_file varchar(255) -- full name of final report
    ,start_at varchar(15) -- '20150223_1527': timestamp of test_time phase start
    ,fb_arch varchar(50) -- 'ss30' | 'sc30' | 'cs30'
    ,overall_perf varchar(50) -- 'score_07548'
    ,fw_setting varchar(20) -- 'fw__on' | 'fw_off'
    ,load_time varchar(50) -- '03h00m'
    ,load_att varchar(50) -- '150_att'
    ,heavy_load_ddl varchar(50) -- only when a_format='benchmark': solid' | 'split'
    ,compound_1st_col varchar(50) -- only when a_format='benchmark': 'most__selective_1st' | 'least_selective_1st'
    ,compound_idx_num varchar(50) -- only when a_format='benchmark': 'one_index' | 'two_indxs'
)
as
    declare v_test_finish_state varchar(50);
    declare v_tab_name dm_dbobj;
    declare v_idx_name dm_dbobj;
    declare v_min_idx_key varchar(255);
    declare v_max_idx_key varchar(255);
    declare v_test_time int;
    declare v_num_of_sessions int;
    declare v_dts_beg timestamp;
    declare v_dts_end timestamp;
    declare k smallint;
    declare v_fb_major_vers varchar(10);
begin

    -- Aux. SP for returning FILE NAME of final report which does contain all
    -- valuable FB, database and test params
    -- Sample:
    -- select * from srv_get_report_name('regular', 31236)
    -- select * from srv_get_report_name('benchmark', 31236)

    select d1 || d2
    from (
        select d1, left(s, position('.' in s)-1) d2
        from (
            select left(r,  position('.' in r)-1) d1, substring(r from 1+position('.' in r)) s
            from (
              select rdb$get_context('SYSTEM','ENGINE_VERSION') r from rdb$database
            )
        )
    ) into v_fb_major_vers; -- '2.5.0' ==> '25'; '3.0.0' ==> '30'; '19.17.1' ==> '1917' :-)

    select p.fb_arch from sys_get_fb_arch p into fb_arch;
    fb_arch =
        iif( fb_arch containing 'superserver' or upper(fb_arch) starting with upper('ss'), 'ss'
            ,iif( fb_arch containing 'superclassic' or upper(fb_arch) starting with upper('sc'), 'sc'
                ,iif( fb_arch containing 'classic' or upper(fb_arch) starting with upper('cs'), 'cs'
                    ,'fb'
                    )
                )
           )
        || v_fb_major_vers -- prev: iif( rdb$get_context('SYSTEM','ENGINE_VERSION') starting with '2.5', '25', '30' )
    ;
    fw_setting='fw' || iif( (select mon$forced_writes from mon$database)= 1,'__on','_off');

    select
        'score_'||lpad( cast( coalesce(aux1,0) as int ), iif( coalesce(aux1,0) < 99999, 5, 18 ) , '0' )
        ,datediff(minute from p.dts_beg to p.dts_end)
        ,p.dts_beg, p.dts_end
    from perf_log p
    where p.unit = 'perf_watch_interval'
    order by p.dts_beg desc
    rows 1
    into overall_perf, v_test_time, v_dts_beg, v_dts_end;

    v_test_finish_state = null;
    if ( a_test_time_minutes = -1 ) then -- call AFTER test finish, when making final report
        begin
            select 'ABEND_GDS_'||p.fb_gdscode
            from perf_log p
            where p.unit = 'sp_halt_on_error' and p.fb_gdscode >= 0
            order by p.dts_beg desc
            rows 1
            into v_test_finish_state; -- will remain NULL if not found ==> test finished NORMAL.
        end
    else -- a_test_time_minutes > = 0
        begin
           -- call from main batch (1run_oltp_emul) just BEFORE all ISQL
           -- sessions will be launched: display *estimated* name of report
            overall_perf = 'score_' || lpad('',5,'X');
            v_test_time = a_test_time_minutes;
        end

    select left(ansi_dts, 13) from sys_timestamp_to_ansi( coalesce(:v_dts_beg, current_timestamp))
    into start_at;

    v_test_time = coalesce(v_test_time,0);
    load_time = lpad(cast(v_test_time/60 as int),2,'_')||'h' || lpad(mod(v_test_time,60),2,'0')||'m';

    if ( a_num_of_sessions = -1 ) then
        -- Use *actual* number of ISQL sessions that were participate in this test run.
        -- This case is used when final report is created AFTER test finish, from oltp_isql_run_worker.bat (.sh):
        select count(distinct e.att_id)
        from perf_estimated e
        into v_num_of_sessions;
    else
        -- Use *declared* number of ISQL sessions that *will* be participate in this test run:
        -- (this case is used when we diplay name of report BEFORE launching ISQL sessions, in 1run_oltp_emul.bat (.sh) script):
        v_num_of_sessions= a_num_of_sessions;

    load_att = lpad( coalesce(v_num_of_sessions, '0'), 3, '_') || '_att';

    k = position('.' in reverse(a_build));
    a_build = iif( k > 0, reverse(left(reverse(a_build), k - 1)), a_build );

    if ( a_format = 'regular' ) then
        -- 20151102_2219_score_06578_build_32136_ss30__0h30m__10_att_fw_off.txt 
        report_file =
            start_at
            || '_' || coalesce( v_test_finish_state, overall_perf )
            || iif( a_build > '', '_build_' || a_build, '' )
            || '_' || fb_arch
            || '_' || load_time
            || '_' || load_att
            || '_' || fw_setting
        ;
    else if (a_format = 'benchmark') then
        begin
            for
                select
                    tab_name,
                    min(idx_key) as min_idx_key,
                    max(idx_key) as max_idx_key
                from z_qd_indices_ddl z
                group by tab_name
                rows 1
            into
                v_tab_name, v_min_idx_key, v_max_idx_key
            do begin
        
                heavy_load_ddl = iif( upper(v_tab_name)=upper('qdistr'), 'solid', 'split' );
        
                if ( upper(v_min_idx_key) starting with upper('ware_id') or upper(v_max_idx_key) starting with upper('ware_id')  ) then
                    compound_1st_col = 'most__sel_1st';
                else if ( upper(v_min_idx_key) starting with upper('snd_optype_id') or upper(v_max_idx_key) starting with upper('snd_optype_id')  ) then
                    compound_1st_col = 'least_sel_1st';
        
                if ( v_min_idx_key = v_max_idx_key ) then
                    compound_idx_num = 'one_index';
                else
                    compound_idx_num = 'two_indxs';
            end
            -- ss30_fw__on_solid_most__sel_1st_two_indxs_loadtime_180m_by_100_att_20151102_0958_20151102_1258.txt
            report_file =
                fb_arch
                || '_' || fw_setting
                || '_' || heavy_load_ddl -- 'solid' | 'split'
                || '_' || compound_1st_col -- 'most__sel_1st' | 'least_sel_1st'
                || '_' || compound_idx_num -- 'one_index' | 'two_indxs'
                || '_' || coalesce( v_test_finish_state, overall_perf )
                || iif( a_build > '', '_build_' || a_build, '' )
                || '_' || load_time
                || '_' || load_att
                || '_' || start_at
            ;
        end

    if ( trim(a_prefix) > '' ) then report_file = trim(a_prefix) || '-' || report_file;

    if ( trim(a_suffix) > '' ) then report_file = report_file || '-' || trim(a_suffix);

    suspend;

end

^ -- srv_get_report_name

create or alter procedure srv_test_work
returns (
    ret_code integer)
as
    declare v_bak_ctx1 int;
    declare v_bak_ctx2 int;
    declare n bigint;
    declare v_clo_id bigint;
    declare v_ord_id bigint;
    declare v_inv_id bigint;
    declare v_res_id bigint;
begin
    -- "express test" for checking that main app units work OK.
    -- NB: all tables must be EMPTY before this SP run.
    v_bak_ctx1 = rdb$get_context('USER_SESSION', 'ORDER_FOR_OUR_FIRM_PERCENT');
    v_bak_ctx2 = rdb$get_context('USER_SESSION', 'ENABLE_RESERVES_WHEN_ADD_INVOICE');

    rdb$set_context('USER_SESSION', 'ORDER_FOR_OUR_FIRM_PERCENT',0);
    rdb$set_context('USER_SESSION', 'ENABLE_RESERVES_WHEN_ADD_INVOICE',1);

    select min(p.doc_list_id) from sp_client_order(0,1,1) p into v_clo_id;
    select count(*) from srv_make_invnt_saldo into n;
    select min(p.doc_list_id) from sp_supplier_order(0,1,1) p into v_ord_id;
    select count(*) from srv_make_invnt_saldo into n;
    select min(p.doc_list_id) from sp_supplier_invoice(0,1,1) p into v_inv_id;
    select count(*) from srv_make_invnt_saldo into n;
    select count(*) from sp_add_invoice_to_stock(:v_inv_id) into n;
    select count(*) from srv_make_invnt_saldo into n;

    select h.id
    from doc_list h
    where h.optype_id = fn_oper_retail_reserve()
    rows 1
    into :v_res_id;

    select count(*) from sp_reserve_write_off(:v_res_id) into n;
    select count(*) from srv_make_invnt_saldo into n;
    select count(*) from sp_cancel_client_order(:v_clo_id) into n;
    select count(*) from srv_make_invnt_saldo into n;
    select count(*) from sp_cancel_supplier_order(:v_ord_id) into n;
    select count(*) from srv_make_invnt_saldo into n;

    rdb$set_context('USER_SESSION', 'ORDER_FOR_OUR_FIRM_PERCENT', v_bak_ctx1);
    rdb$set_context('USER_SESSION', 'ENABLE_RESERVES_WHEN_ADD_INVOICE', v_bak_ctx2);

    ret_code = iif( exists(select * from v_qdistr_source ) or exists(select * from v_qstorned_source ), 1, 0);
    ret_code = iif( exists(select * from invnt_turnover_log), bin_or(ret_code, 2), ret_code );
    ret_code = iif( NOT exists(select * from invnt_saldo), bin_or(ret_code, 4), ret_code );
    
    n = null;
    select s.id
    from invnt_saldo s
    where NOT
    (
        s.qty_clo=1 and s.qty_clr = 1
        and s.qty_ord = 0 and s.qty_sup = 0
        and s.qty_avl = 0 and s.qty_res = 0
        and s.qty_inc = 0 and s.qty_out = 0
    )
    rows 1
    into n;
    
    ret_code = iif( n is NOT null, bin_or(ret_code, 8), ret_code );
    
    suspend;
end

^ -- srv_test_work

set term ;^



set list on;
set echo off;
select 'oltp30_sp.sql finish at ' || current_timestamp as msg from rdb$database;
set list off;


-- ###################################################################
-- End of script oltp30_SP.sql;  next to be run: oltp_main_filling.sql
-- (common for both FB 2.5 and 3.0)
-- ###################################################################

-- ###################################
-- Begin of script oltp_misc_debug.sql  // ###   O P T I O N A L   ###
-- ###################################
-- ::: NB ::: This scipt is COMMON for both FB 2.5 and 3.0 and should be called after oltp_main_filling.sql (if needed)

-- It creates some OPTIONAL debug views and procedures.
-- Need only when some troubles in algorithms are detected.
-- Call of this script should be AFTER running oltpNN_DDL.sql and oltpNN_sp.sql

set list on;
select 'oltp_misc_debug.sql start at ' || current_timestamp as msg from rdb$database;
set list off;



-------------------------------------------------------
--  ************   D E B U G   T A B L E S   **********
-------------------------------------------------------
 -- tables for dump dirty data, 4 debug only
recreate table ztmp_shopping_cart(
   id bigint,
   snd_id bigint,
   qty numeric(12,3),
   optype_id bigint,
   snd_optype_id bigint,
   rcv_optype_id bigint,
   qty_bak numeric(12,3),
   dup_cnt int,
   dump_att bigint,
   dump_trn bigint
);


recreate table ztmp_dep_docs(
  base_doc_id bigint,
  dependend_doc_id bigint,
  dependend_doc_state bigint,
  dependend_doc_dbkey dm_dbkey,
  dependend_doc_agent_id bigint,
  ware_id bigint,
  base_doc_qty numeric(12,3),
  dependend_doc_qty numeric(12,3),
  dump_att bigint,
  dump_trn bigint
);


recreate table zdoc_list(
   id bigint
  ,optype_id bigint
  ,agent_id bigint
  ,state_id bigint
  ,base_doc_id bigint -- id of document that is 'base' for current (stock order => incoming invoice etc)
  ,cost_purchase numeric(12,2) default 0 -- total in PURCHASING cost (can be ZERO for stock orders)
  ,cost_retail numeric(12,2) default 0 -- total in RETAIL cost, can be zero for incoming docs and stock orders
  ,acn_type dm_account_type
  ,dts_open timestamp
  ,dts_fix timestamp -- when changes of CONTENT of this document became disabled
  ,dts_clos timestamp -- when ALL changes of this doc. became disabled
  ,att int
  ,dump_att bigint
  ,dump_trn bigint
);


recreate table zdoc_data(
   id dm_idb
  ,doc_id dm_idb
  ,ware_id dm_idb
  ,qty dm_qty
  ,cost_purchase dm_cost
  ,cost_retail dm_cost
  ,dts_edit timestamp
  ,optype_id dm_idb
  ,dump_att bigint
  ,dump_trn bigint
);

-- 27.06.2014 (need to find cases when negative remainders appear)
recreate table zinvnt_turnover_log(
    ware_id bigint
   ,qty_diff numeric(12,3)
   ,cost_diff numeric(12,2)
   ,doc_list_id bigint
   ,doc_pref dm_mcode
   ,doc_data_id bigint
   ,optype_id bigint
   ,id bigint
   ,dts_edit timestamp
   ,att_id int
   ,trn_id int
   ,dump_att bigint
   ,dump_trn bigint
);

recreate table zqdistr(
   id dm_idb
  ,doc_id dm_idb
  ,ware_id dm_idb
  ,snd_optype_id dm_idb
  ,snd_id dm_idb
  ,snd_qty dm_qty
  ,rcv_optype_id bigint
  ,rcv_id bigint -- nullable! ==> doc_data.id of "receiver"
  ,rcv_qty numeric(12,3)
  ,snd_purchase dm_cost
  ,snd_retail dm_cost
  ,rcv_purchase dm_cost
  ,rcv_retail dm_cost
  ,trn_id bigint
  ,dts timestamp
  ,dump_att bigint
  ,dump_trn bigint
);
create index zqdistr_id on zqdistr(id); -- NON unique!
create index zqdistr_ware_sndop_rcvop on zqdistr(ware_id, snd_optype_id, rcv_optype_id);


recreate table zqstorned(
   id dm_idb
  ,doc_id dm_idb
  ,ware_id dm_idb
  ,snd_optype_id dm_idb
  ,snd_id dm_idb
  ,snd_qty dm_qty
  ,rcv_optype_id dm_idb
  ,rcv_id dm_idb
  ,rcv_qty dm_qty
  ,snd_purchase dm_cost
  ,snd_retail dm_cost
  ,rcv_purchase dm_cost
  ,rcv_retail dm_cost
  ,trn_id bigint
  ,dts timestamp
  ,dump_att bigint
  ,dump_trn bigint
);
create index zqstorned_id on zqstorned(id); -- NON unique!
create index zqstorned_doc_id on zqstorned(doc_id); -- confirmed 16.09.2014, see s`p_lock_dependent_docs
create index zqstorned_snd_id on zqstorned(snd_id); -- confirmed 16.09.2014, see s`p_kill_qty_storno
create index zqstorned_rcv_id on zqstorned(rcv_id); -- confirmed 16.09.2014, see s`p_kill_qty_storno

recreate table zpdistr(
   id dm_idb
  ,agent_id dm_idb
  ,snd_optype_id dm_idb
  ,snd_id dm_idb
  ,snd_cost dm_qty
  ,rcv_optype_id dm_idb
  ,trn_id bigint
  ,dump_att bigint
  ,dump_trn bigint
);
create index zpdistr_id on zpdistr(id); -- NON unique!

recreate table zpstorned(
   id dm_idb
  ,agent_id dm_idb
  ,snd_optype_id dm_idb
  ,snd_id dm_idb
  ,snd_cost dm_cost
  ,rcv_optype_id dm_idb
  ,rcv_id dm_idb
  ,rcv_cost dm_cost
  ,trn_id bigint
  ,dump_att bigint
  ,dump_trn bigint
);
create index zpstorned_id on zpstorned(id); -- NON unique!



set term ^;
create or alter procedure z_remember_view_usage (
    a_view_for_search dm_dbobj,
    a_view_for_min_id dm_dbobj default null,
    a_view_for_max_id dm_dbobj default null
) as
    declare i smallint;
    declare v_ctxn dm_ctxnv;
    declare v_name dm_dbobj;
begin

    i = 1;
    while (i <= 3) do -- a_view_for_search, a_view_for_min_id,  a_view_for_max_id
    begin
        v_name = decode(i, 1, a_view_for_search, 2, a_view_for_min_id ,  a_view_for_max_id  );
        if ( v_name is not null ) then
        begin
            v_ctxn = right(v_name||'_is_used', 80);
            if ( rdb$get_context('USER_SESSION', v_ctxn) is null ) then
            begin
                if (not exists( select * from z_used_views u where u.name = :v_name )) then
                begin
                    insert into z_used_views(name) values( :v_name );
                    rdb$set_context('USER_SESSION', v_ctxn, '1' );
                end
            when any do
                -- ::: nb ::: do NOT use "wh`en gdscode <mnemona>" followed by "wh`en any":
                -- the latter ("w`hen ANY") will handle ALWAYS, even if "w`hen <mnemona>"
                -- catched it's kind of exception!
                -- 1) tracker.firebirdsql.org/browse/CORE-3275
                --    "W`HEN ANY handles exceptions even if they are handled in another W`HEN section"
                -- 2) sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1088890&msg=15879669
                begin
                    if ( not gdscode in ( 335544665,335544349 ) ) then

                        -- #######
                        exception;
                        -- #######
                    -- else ==> yes, supress no_dup exception here --
                end
            end
        end
        i = i + 1;
    end

end
^


--------------------------------------------------------------------------------

create or alter procedure z_get_dependend_docs(
    a_doc_list_id dm_idb,
    a_doc_oper_id dm_idb default null -- = (for invoices which are to be 'reopened' - old_oper_id)
) returns (
  dependend_doc_id dm_idb, 
  dependend_doc_state dm_idb
)
as
    declare v_rcv_optype_id dm_idb;
begin
    -- former: s`p_get_dependend_docs; now need only for debug
    if ( a_doc_oper_id is null ) then
        select h.optype_id
        from doc_list h
        where h.id = :a_doc_list_id
        into a_doc_oper_id;

    v_rcv_optype_id = decode(
                              a_doc_oper_id
                             ,2100,          3300 -- ,fn_oper_invoice_add(),  fn_oper_retail_reserve()
                             ,1200,          2000 -- ,fn_oper_order_for_supplier(), fn_oper_invoice_get()
                             ,null
                            );

    for
        select x.dependend_doc_id, h.state_id
        -- 30.12.2014: PLAN JOIN (SORT (X Q INDEX (QSTORNED_DOC_ID)), H INDEX (PK_DOC_LIST))
        -- (added field rcv_doc_id in table qstorned, now can remove join with doc_data!)
        from (
            -- Checked plan 13.07.2014:
            -- PLAN (Q ORDER QSTORNED_RCV_ID INDEX (QSTORNED_DOC_ID))
            select q.rcv_doc_id dependend_doc_id -- q.rcv_id dependend_doc_data_id
            from v_qstorned_source q
            where
                q.doc_id =  :a_doc_list_id -- choosen invoice which is to be re-opened
                and q.snd_optype_id = :a_doc_oper_id -- fn_oper_invoice_add()
                and q.rcv_optype_id = :v_rcv_optype_id --fn_oper_retail_reserve() -- in ( fn_oper_retail_reserve(), fn_oper_retail_realization() )
            group by 1
        ) x
        join doc_list h on x.dependend_doc_id = h.id
        into dependend_doc_id, dependend_doc_state
    do
        suspend;

end

^ -- z_get_dependend_docs

set term ;^


--------------------------------------------------------------------------------

create or alter view z_perf_trn as
select * from perf_log p where p.trn_id = current_transaction
;

-------------------------------------------------------------------------------

create or alter view z_random_bop as
select b.sort_prior as id, b.unit, b.info
from business_ops b
;

--------------------------------------------------------------------------------

create or alter view v_diag_fk_uk as
-- service view for check data in FK/UNQ indices: search 'orphan' rows in FK
-- or duplicate rows in all PK/UNQ keys (suggestion by DS, 05.05.2014 18:23)
-- ::: NB ::: this view does NOT include in its reultset self-referenced tables!
with recursive
c as (
    select
         rc.rdb$relation_name child_tab
        ,rc.rdb$constraint_name child_fk
        ,rc.rdb$index_name child_idx
        ,ru.rdb$const_name_uq parent_uk
        ,rp.rdb$relation_name parent_tab
        ,rp.rdb$index_name parent_idx
    from rdb$relation_constraints rc
    join rdb$ref_constraints ru on
         rc.rdb$constraint_name = ru.rdb$constraint_name
         and rc.rdb$constraint_type = 'FOREIGN KEY'
    join rdb$relation_constraints rp
         on ru.rdb$const_name_uq = rp.rdb$constraint_name
    where rc.rdb$relation_name <> rp.rdb$relation_name -- prevent from select self-ref PK/FK tables!
)
,d as(
    select
        0 i
        ,child_tab
        ,child_fk
        ,child_idx
        ,parent_uk
        ,parent_tab
        ,parent_idx
    from c c0
    -- filter tables which are NOT parents for any other tables:
    where not exists( select * from c cx where cx.parent_tab= c0.child_tab ) 
    
    union all
    
    select
        d.i+1
        ,c.child_tab
        ,c.child_fk
        ,c.child_idx
        ,c.parent_uk
        ,c.parent_tab
        ,c.parent_idx
    from d
    join c on d.parent_tab = c.child_tab
)
--select * from d where d.child_tab='DOC_DATA'

,e as(
    select distinct
         child_tab
        ,child_fk
        ,child_idx
        ,parent_uk
        ,parent_tab
        ,parent_idx
        ,rsc.rdb$field_name fk_fld
        ,rsp.rdb$field_name uk_fld
    from d
    join rdb$index_segments rsc on d.child_idx = rsc.rdb$index_name
    join rdb$index_segments rsp on d.parent_idx = rsp.rdb$index_name and rsc.rdb$field_position=rsp.rdb$field_position
)
,f as(
    select
        e.child_tab,e.child_fk,e.parent_tab,e.parent_uk
        --,e.fk_fld,e.uk_fld
        ,list( 'd.'||trim(e.fk_fld)||' = m.'||trim(e.uk_fld), ' and ') jcond
        ,list( 'm.'||trim(e.uk_fld)||' is null', ' and ' ) ncond
    from e
    group by e.child_tab,e.child_fk,e.parent_tab,e.parent_uk
)
--select * from f

select
    f.child_fk checked_constraint
   ,'FK' type_of_constraint
   ,'select count(*) from '
        ||trim(f.child_tab)||' d left join '
        ||trim(f.parent_tab)||' m on '
        ||f.jcond
        ||' where '||f.ncond as checked_qry
from f

UNION ALL

select
    uk_idx as checked_constraint
   ,'UK' type_of_constraint
   ,'select count(*) from '||trim(tab_name)||' group by '||trim(uk_lst)||' having count(*)>1' as checked_qry
from(
    select tab_name,uk_idx, list( trim(uk_fld) ) uk_lst
    from(
        select rr.rdb$relation_name tab_name, rc.rdb$index_name uk_idx, rs.rdb$field_name uk_fld
        from rdb$relation_constraints rc
        join rdb$relations rr on rc.rdb$relation_name = rr.rdb$relation_name
        join rdb$index_segments rs on rc.rdb$index_name = rs.rdb$index_name
        where
            rc.rdb$constraint_type in ('PRIMARY KEY', 'UNIQUE')
            and coalesce(rr.rdb$system_flag,0)=0
    )
    group by tab_name,uk_idx
)
-- v_diag_fk_uk
;

-------------------------------------------

create or alter view v_diag_idx_entries as
-- source to check match of all possible counts ortder by table indices
-- and count via natural order (suggestion by DS, 05.05.2014 18:23)
select
  tab_name
  ,idx_name
  ,cast('select count(*) from (select * from '||trim(tab_name)||' order by '||trim(idx_expr||desc_expr)||')' as varchar(255))
   as checked_qry
from(
    select
        tab_name
        ,idx_name
        ,max(iif(idx_type=1,' desc','')) desc_expr
        ,list(trim(coalesce(idx_comp, idx_key))) idx_expr
    from
    (
        select
            ri.rdb$relation_name tab_name
            ,ri.rdb$index_name idx_name
            ,ri.rdb$expression_source idx_comp
            ,ri.rdb$index_type idx_type
            ,rs.rdb$field_name idx_key
        from rdb$indices ri
            join rdb$relations rr on ri.rdb$relation_name = rr.rdb$relation_name
            left join rdb$index_segments rs on ri.rdb$index_name=rs.rdb$index_name
        where coalesce(rr.rdb$system_flag,0)=0 and rr.rdb$relation_type not in(4,5)
        order by ri.rdb$relation_name, rs.rdb$index_name,rs.rdb$field_position
    )
    group by
        tab_name
        ,idx_name
)
-- v_diag_idx_entries
;



------------------------------------------------------------------------

create or alter view z_check_inv_vs_sup as
-- for checking: all qty in INVOICES which supplier has sent us must be
-- LESS or EQUEAL than qty which we've ORDERED to supplier before
-- This view should return records with ERRORS in data.
select
    doc_id,
    doc_data_id,
    ware_id,
    qty as doc_qty,
    qty_sup,
    qty_clo,
    qty_clr,
    qty_ord,
    qty_avl,
    qty_res
from v_add_invoice_to_stock v
join v_doc_detailed f on v.id=f.doc_id
where qty > qty_sup
;




--------------------------------------------------------------------------------

create or alter view z_clean_data as
with recursive
c as (
    select
         rc.rdb$relation_name child_tab
        ,rc.rdb$constraint_name child_fk
        ,ru.rdb$const_name_uq parent_uk
        ,rp.rdb$relation_name parent_tab
    from rdb$relation_constraints rc
    join rdb$ref_constraints ru on
         rc.rdb$constraint_name = ru.rdb$constraint_name
         and rc.rdb$constraint_type = 'FOREIGN KEY'
    join rdb$relation_constraints rp
         on ru.rdb$const_name_uq = rp.rdb$constraint_name
    where rc.rdb$relation_name <> rp.rdb$relation_name
)
--select * from c
,d as(
    select
        0 i
        ,child_tab
        ,child_fk
        ,parent_uk
        ,parent_tab
    from c c0
    where not exists( select * from c cx where cx.parent_tab= c0.child_tab )
    
    union all
    
    select
        d.i+1
        ,c.child_tab
        ,c.child_fk
        ,c.parent_uk
        ,c.parent_tab
    from d
    join c on d.parent_tab = c.child_tab
)
,e as(
    select
        i
        ,child_tab
        ,child_fk
        ,parent_uk
        ,parent_tab
        --,max(i)over() mi
        --,(select max(i) from d) as mi
    from d
)
-- select * from e
,f as(
    select distinct
        0 i
        ,child_tab
    from e where i=0

    UNION DISTINCT

    select
        1
        ,child_tab
    from (select child_tab from e where i > 0 order by i)

    UNION DISTINCT

    select k,parent_tab
    from (
        select
            2 as k
            ,parent_tab
        from e
        order by i desc rows 1
    )
    --- doesn`t work in 3.0 when "(select max(i) from d) as mi", see CTE `e`, 06.02.2015:
    -- select 2 as k,parent_tab from e where i=mi
)
,t as(
    select
        rt.rdb$trigger_name trg_name -- f.child_tab, rt.rdb$trigger_name, rt.rdb$trigger_type
    from f
    join rdb$triggers rt on f.child_tab = rt.rdb$relation_name
    where rt.rdb$system_flag=0 and rt.rdb$trigger_inactive=0
)
select 'alter trigger '||trim(trg_name)||' inactive' sql_expr
from t
union all
select 'delete from '||trim(child_tab)
from f
union all
select 'alter trigger '||trim(trg_name)||' active'
from t
;

----------------------

create or alter view z_idx_stat as
select ri.rdb$relation_name tab_name, ri.rdb$index_name idx_name, nullif(ri.rdb$statistics,0) idx_stat
from rdb$indices ri
where ri.rdb$relation_name not starting with 'RDB$' --and ri.rdb$statistics > 0
order by 3 desc nulls first,1,2
;


--------------------------------------------------------------------------------

create or alter view z_rules_for_qdistr as
-- 4debug
select r.mode, r.snd_optype_id, so.mcode snd_mcode, r.rcv_optype_id, ro.mcode rcv_mcode
from rules_for_qdistr r
left join optypes so on r.snd_optype_id = so.id
left join optypes ro on r.rcv_optype_id = ro.id
;


--------------------------------------------------------------------------------

create or alter view zv_doc_detailed as
-- Debug: analysis of dumped dirty data (filled by SP zdump4dbg in some critical errors)
select
    h.id doc_id,
    h.optype_id,
    o.mcode oper,
    h.base_doc_id,
    d.id doc_data_id,
    d.ware_id,
    d.qty,
    coalesce(d.cost_purchase, h.cost_purchase) cost_purchase, -- cost in purchase price
    coalesce(d.cost_retail, h.cost_retail) cost_retail, -- cost in retail price
    h.state_id,
    h.agent_id,
    d.dts_edit,
    h.dts_open,
    h.dts_fix,
    h.dts_clos,
    s.mcode state,
    h.att
from zdoc_list h
    join optypes o on h.optype_id = o.id
    join doc_states s on h.state_id=s.id
    left join zdoc_data d on h.id = d.doc_id
    -- ::: NB ::: do NOT remove "left" from here otherwise performance will degrade
    -- (FB will not push predicate inside view; 22.04.2014)
    --LEFT join v_saldo_invnt n on d.ware_id=n.ware_id
;
--------------------------------------------------------------------------------
create or alter view zv_saldo_invnt as
-- 21.04.2014
-- ::: NB ::: this view can return NEGATIVE remainders in qty_xxx
-- if parallel attaches call of sp_make_invnt_saldo
-- (because of deleting rows in invnt_turnover_log in this SP)
-- !!! look at table INVNT_SALDO for actual remainders !!!
select
    ng.ware_id
    ,sum(o.m_qty_clo * ng.qty_diff) qty_clo
    ,sum(o.m_qty_clr * ng.qty_diff) qty_clr
    ,sum(o.m_qty_ord * ng.qty_diff) qty_ord
    ,sum(o.m_qty_sup * ng.qty_diff) qty_sup
    ,sum(o.m_qty_avl * ng.qty_diff) qty_avl
    ,sum(o.m_qty_res * ng.qty_diff) qty_res
    ,sum(o.m_cost_inc * ng.qty_diff) qty_inc
    ,sum(o.m_cost_out * ng.qty_diff) qty_out
    ,sum(o.m_cost_inc * ng.cost_diff) cost_inc
    ,sum(o.m_cost_out * ng.cost_diff) cost_out
    -- amount "on hand" as it seen by accounter:
    ,sum(o.m_qty_avl * ng.qty_diff) + sum(o.m_qty_res * ng.qty_diff) qty_acn
    -- total cost "on hand" in purchasing prices:
    ,sum(o.m_cost_inc * ng.cost_diff) - sum(o.m_cost_out * ng.cost_diff) cost_acn
from zinvnt_turnover_log ng
join optypes o on ng.optype_id=o.id
group by 1
;

--------------------------------------------------------------------------------


create or alter view z_mism_dd_qd_qs_orphans as
-- 4 debug: search only those rows in doc_data for which absent any rows in
-- qdistr and qstorned ('lite' diagnostics):
select d.doc_id,h.optype_id,d.id,d.ware_id,d.qty
from doc_data d
join doc_list h on d.doc_id = h.id
left join v_qdistr_source q on d.id=q.snd_id
left join v_qstorned_source s on d.id in(s.snd_id, s.rcv_id)
where h.optype_id<>1100 and q.id is null and s.id is null
;

--------------------------------------------------------------------------------

create or alter view z_mism_dd_qd_qs_sums as
-- 4 debug: search for mismatches be`tween doc_data.qty and number of
-- records in qdistr or qstorned
select d.doc_id, d.id,d.optype_id,d.qty,d.qd_sum,coalesce(sum(qs.snd_qty),0) qs_sum
from(
    select d.doc_id, d.id, d.optype_id, d.qty,coalesce(sum(qd.snd_qty),0) qd_sum
    from (
        select d.doc_id, d.id, d.ware_id, iif(h.optype_id=3400, 3300, h.optype_id) as optype_id, d.qty
        from doc_data d
        join doc_list h on d.doc_id = h.id
    ) d
    inner join rules_for_qdistr p on d.optype_id = p.snd_optype_id + 0 and coalesce(p.storno_sub,1)=1 -- hash join! 3.0 only
    left join v_qdistr_source qd on
        qd.ware_id = d.ware_id
        and qd.snd_optype_id = p.snd_optype_id
        and qd.rcv_optype_id is not distinct from p.rcv_optype_id
    where d.optype_id<>1100 -- client refused from order
    group by d.doc_id, d.id, d.optype_id, d.qty
) d
join rules_for_qdistr p on d.optype_id = p.snd_optype_id + 0 and coalesce(p.storno_sub,1)=1 -- hash join! 3.0 only
left join v_qstorned_source qs on
    d.id=qs.snd_id
    and p.snd_optype_id=qs.snd_optype_id
    and p.rcv_optype_id is not distinct from  qs.rcv_optype_id
group by d.doc_id, d.id,d.optype_id,d.qty,d.qd_sum
having d.qty <> d.qd_sum + coalesce(sum(qs.snd_qty),0)
;

--------------------------------------------------------------------------------

create or alter view z_mism_zdd_zqdzqs as
-- 4 debug: search for mismatches between doc_data.qty and number of
-- records in qdistr or qstorned
select d.doc_id, d.id,d.optype_id,d.qty,d.qd_sum,coalesce(sum(qs.snd_qty),0) qs_sum
from(
    select d.doc_id, d.id, d.optype_id, d.qty,coalesce(sum(qd.snd_qty),0) qd_sum
    from (
        select d.doc_id, d.id, d.ware_id, iif(d.optype_id=3400, 3300, d.optype_id) as optype_id, d.qty
        from zdoc_data d
    ) d
    inner join rules_for_qdistr p on d.optype_id=p.snd_optype_id and coalesce(p.storno_sub,1)=1
    left join zqdistr qd on
        qd.ware_id = d.ware_id
        and qd.snd_optype_id = p.snd_optype_id
        and qd.rcv_optype_id is not distinct from p.rcv_optype_id
    where d.optype_id<>1100 -- client refused from order
    group by d.doc_id, d.id, d.optype_id, d.qty
) d
inner join rules_for_qdistr p on d.optype_id=p.snd_optype_id and coalesce(p.storno_sub,1)=1
left join zqstorned qs on
    d.id=qs.snd_id
    and p.snd_optype_id=qs.snd_optype_id
    and p.rcv_optype_id is not distinct from  qs.rcv_optype_id
group by d.doc_id, d.id,d.optype_id,d.qty,d.qd_sum
having d.qty <> d.qd_sum + coalesce(sum(qs.snd_qty),0)
;

--------------------------------------------------------------------------------
create or alter view z_qdqs as
-- Debug: analysis of dumped dirty data (filled by SP zdump4dbg in some critical errors)
select
    cast(q.src as varchar(8)) as src,
    q.id,
    q.ware_id,
    q.snd_optype_id,
    cast(so.mcode as varchar(3)) snd_op,
    q.doc_id as snd_doc_id,
    sh.agent_id as snd_agent,
    q.snd_id,
    q.snd_qty,
    q.snd_purchase,
    q.snd_retail,
    q.rcv_optype_id,
    cast(ro.mcode as varchar(3)) rcv_op,
    d.doc_id as rcv_doc_id,
    rh.agent_id as rcv_agent,
    q.rcv_id,
    q.rcv_qty,
    q.rcv_purchase,
    q.rcv_retail,
    q.trn_id,
    q.dts
from (
  select 'qdistr' src,q.*
  from qdistr q
  union all
  select 'qstorned', s.*
  from qstorned s
) q
left join doc_data d on q.rcv_id = d.id
left join optypes so on q.snd_optype_id = so.id
left join doc_list sh on q.doc_id=sh.id
left join optypes ro on q.rcv_optype_id = ro.id
left join doc_list rh on d.doc_id=rh.id
order by q.src, q.doc_id, q.id
;

-------------------------------

create or alter view z_zqdzqs as
-- Debug: analysis of dumped dirty data (filled by SP zdump4dbg in some critical errors)
select
    q.src,
    q.id,
    q.ware_id,
    q.snd_optype_id,
    left(so.mcode,3) snd_op,
    q.doc_id as snd_doc_id,
    q.snd_id,
    q.snd_qty,
    q.rcv_optype_id,
    left(ro.mcode,3) rcv_op,
    d.doc_id as rcv_doc_id,
    q.rcv_id,
    q.rcv_qty,
    q.trn_id,
    q.dts,
    q.dump_att,
    q.dump_trn
from (
  select 'zqdistr' src,q.*
  from zqdistr q
  union all
  select 'zqstorned', s.*
  from zqstorned s
) q
left join zdoc_data d on q.rcv_id = d.id
left join optypes so on q.snd_optype_id = so.id
left join optypes ro on q.rcv_optype_id = ro.id
order by q.src, q.doc_id, q.id
;
-------------------------------

create or alter view z_pdps as
-- Debug: analysis of dumped dirty data (filled by SP zdump4dbg in some critical errors)
select
    cast(p.src as varchar(8)) as src,
    p.id,
    p.agent_id,
    p.snd_optype_id,
    cast(so.mcode as varchar(3)) snd_op,
    p.snd_id,
    p.snd_cost,
    p.rcv_optype_id,
    cast(ro.mcode as varchar(3)) rcv_op,
    p.rcv_id,
    p.rcv_cost,
    p.trn_id
from (
    select 'pdistr' src,p.id,p.agent_id,p.snd_optype_id,p.snd_id,p.snd_cost,p.rcv_optype_id, cast(null as bigint) as rcv_id, cast(null as numeric(12,2)) as rcv_cost, p.trn_id
    from pdistr p
    union all
    select 'pstorned', s.id, s.agent_id, s.snd_optype_id, s.snd_id, s.snd_cost, s.rcv_optype_id, s.rcv_id, s.rcv_cost, s.trn_id
    from pstorned s
) p
left join optypes so on p.snd_optype_id = so.id
left join optypes ro on p.rcv_optype_id = ro.id
order by p.src, p.id -- p.agent_id, p.rcv_id, p.id
;
------------------------------

create or alter view z_zpdzps as
-- Debug: analysis of dumped dirty data (filled by SP zdump4dbg in some critical errors)
select
    p.src,
    p.id,
    p.agent_id,
    p.snd_optype_id,
    left(so.mcode,3) snd_op,
    p.snd_id,
    p.snd_cost,
    p.rcv_optype_id,
    left(ro.mcode,3) rcv_op,
    p.rcv_id,
    p.rcv_cost,
    p.trn_id,
    p.dump_att,
    p.dump_trn
from (
    select 'zpdistr' src,p.id,p.agent_id,p.snd_optype_id,p.snd_id,p.snd_cost,p.rcv_optype_id,
          cast(null as bigint) as rcv_id, cast(null as numeric(12,2)) as rcv_cost,
          p.trn_id, p.dump_att, p.dump_trn
    from zpdistr p
    union all
    select 'zpstorned', s.id, s.agent_id, s.snd_optype_id, s.snd_id, s.snd_cost, s.rcv_optype_id,
          s.rcv_id, s.rcv_cost,
          s.trn_id, s.dump_att, s.dump_trn
    from zpstorned s
) p
left join optypes so on p.snd_optype_id = so.id
left join optypes ro on p.rcv_optype_id = ro.id
order by p.src, p.agent_id, p.rcv_id, p.id
;

--------------------------------------------------------------------------------

create or alter view z_slow_get_random_id as
select
    substring(pg.info from 1 for coalesce(nullif(position(';',pg.info)-1,-1),31) ) mode,
    pg.elapsed_ms,
    min(pg.elapsed_ms) ms_min,
    max(pg.elapsed_ms) ms_max,
    count(*) cnt
from perf_log pg
where pg.unit='fn_get_random_id' and pg.elapsed_ms>=3000
group by 1,2
;


--------------------------------------------------------------------------------

create or alter view z_get_min_max_id as
-- 08.02.2015: debug view for efficiency estimation of 'boundary' views which
-- is used for obtaining MIN and MAX ids before subsequent random selection.
-- (v_min/max_id_clo_ord, v_min/max_id_ord_sup etc)
-- Registering in perf_log is in fn_get_random_id.
select
    g.unit
    ,count(iif( coalesce(g.fb_gdscode,0)=0, 1, null ) ) cnt_ok
    ,count(*) cnt_all
    ,avg(g.elapsed_ms) avg_time
    ,min(g.elapsed_ms) min_time
    ,max(g.elapsed_ms) max_time
from perf_log g
where
(
  g.unit starting with 'v_min_id'
  or
  g.unit starting with 'v_max_id'
)
group by 1
order by right(g.unit,6),left(g.unit,4) desc
;

--------------------------------------------------------------------------------

create or alter view z_doc_data_oper_cnt as
-- 19.07.2014, for analyze results of init data population alg
select h.optype_id,o.name op_name,count(*) doc_data_cnt
from doc_list h
join optypes o on h.optype_id=o.id
join doc_data d on h.id=d.doc_id
group by 1,2
;


--------------------------------------------------------------------------------

create or alter view z_doc_list_oper_cnt as
-- 19.07.2014, for analyze results of init data population alg
select h.optype_id,o.name op_name, count(*) doc_list_cnt
from doc_list h
join optypes o on h.optype_id=o.id
group by 1,2
;

--------------------------------------------------------------------------------

create or alter view z_invoices_to_be_adopted as
--  4 debug (performance of sp_add_invoice_to_stock)
select
     invoice_id, total_rows, total_qty
    ,min(p.clo_agent_id) agent_min_id
    ,max(p.clo_agent_id) agent_max_id
    ,count(distinct p.clo_agent_id) agent_diff_cnt
from (
    select h.id invoice_id, count(*) total_rows, sum(qty) total_qty
    from doc_list h
    join doc_data d on h.id=d.doc_id
    where h.optype_id = 2000 -- fn_oper_invoice_get
    group by 1
) x
left join sp_get_clo_for_invoice(x.invoice_id) p on 1=1
group by invoice_id, total_rows, total_qty
order by total_rows desc, total_qty desc
;

--------------------------------------------------------------------------------

create or alter view z_invoices_to_be_cancelled as
--  4 debug (performance of s`p_cancel_adding_invoice)
select h.id invoice_id, count(*) total_rows, sum(qty) total_qty
from doc_list h
join doc_data d on h.id=d.doc_id
where h.optype_id = 2100 -- fn_oper_invoice_add
group by 1
order by 2 desc
;

--------------------------------------------------------------------------------

create or alter view z_ord_inc_res_dependencies as
-- 17.07.2014: get all dependencies (links) b`etween
-- supplier orders (take first 5), invoices and customer reserves
with
s as(
    select v.ord_id, count(*) ord_rows, sum(d0.qty) ord_qty_sum
    from ( select first 5 v.id as ord_id from v_cancel_supplier_order v ) v
    join doc_data d0 on v.ord_id = d0.doc_id
    group by v.ord_id
)
,i as(
    select
         s.ord_id
        ,s.ord_rows
        ,s.ord_qty_sum
        ,p1.dependend_doc_id as inv_id
        ,count(*) inv_rows
        ,sum(di.qty) inv_qty_sum
    from s
    left join z_get_dependend_docs( s.ord_id, 1200 ) p1 on 1=1 -- 1200=fn_oper_order_for_supplier()
    left join doc_data di on p1.dependend_doc_id = di.doc_id
    group by
         s.ord_id
        ,s.ord_rows
        ,s.ord_qty_sum
        ,p1.dependend_doc_id
)
select
    i.ord_id
    ,i.ord_rows
    ,i.ord_qty_sum
    ,i.inv_id
    ,i.inv_rows
    ,i.inv_qty_sum
    ,p2.dependend_doc_id as res_id
    ,count(*) res_rows
    ,sum(dr.qty) res_qty_sum
from i
left join z_get_dependend_docs( i.inv_id, 2100 ) p2 on 1=1-- 2100=fn_oper_invoice_add()
left join doc_data dr on p2.dependend_doc_id = dr.doc_id
group by
    i.ord_id
    ,i.ord_rows
    ,i.ord_qty_sum
    ,i.inv_id
    ,i.inv_rows
    ,i.inv_qty_sum
    ,p2.dependend_doc_id
;



---------------------------------------

set term ^;

create or alter procedure srv_diag_fk_uk
returns(
    checked_constraint type of column rdb$relation_constraints.rdb$constraint_name,
    type_of_constraint type of column v_diag_fk_uk.type_of_constraint,
    failed_rows int
)
as
    declare v_checked_qry varchar(8190);
begin
    -- obtain text of queries for checking data in tables which have
    -- FK and PK/UNQ constraints; counts rows from these tables where
    -- violations of FK or PK/UNQ occur: 'orphan' FK, duplicates in PK/UNQ
    for
        select v.checked_constraint, v.type_of_constraint, cast(v.checked_qry as varchar(8190))
        from v_diag_fk_uk v
    into checked_constraint, type_of_constraint, v_checked_qry
    do begin
       execute statement(v_checked_qry) into failed_rows; -- this must be always 'select count(*) from ...'
       if (failed_rows > 0) then suspend;
    end
end

^ -- srv_diag_fk_uk

----------------------------------------------------------------------

create or alter procedure srv_diag_idx_entries
returns(
    tab_name type of column rdb$relations.rdb$relation_name,
    idx_name type of column rdb$indices.rdb$index_name,
    nat_count bigint,
    idx_count bigint,
    failed_rows bigint
)
as
    declare v_checked_qry varchar(8190);
    declare v_nat_stt varchar(255);
    declare rn bigint;
    declare v_prev_tab type of column v_diag_idx_entries.tab_name = '';
begin
    for
        select v.tab_name, v.idx_name, v.checked_qry
        from v_diag_idx_entries v
        where v.checked_qry not containing 'DOC_NUMB' -- temply, smth wrong with coll num-sort=1 and unique index: FB uses plan natural instead of that index, see: http://www.sql.ru/forum/1093394/select-from-t1-order-by-s-ne-uzaet-uniq-indeks-esli-s-utf8-coll-numeric-sort-1
    into tab_name, idx_name, v_checked_qry
    do begin
        if ( v_prev_tab is distinct from tab_name ) then begin
          v_nat_stt = 'select count(*) from '||tab_name;
          execute statement ( v_nat_stt ) into nat_count;
          v_prev_tab = tab_name;
        end
       execute statement(v_checked_qry) into idx_count; -- this must be always 'select count(*) from ...'
       if ( nat_count <> idx_count ) then begin
           failed_rows = nat_count - idx_count;
           suspend;
       end
    end
end

^  -- srv_diag_idx_entries

----------------------------------------------------------------------

create or alter procedure srv_diag_qty_distr
returns(
    doc_id dm_idb,
    optype_id dm_idb,
    rcv_optype_id dm_idb,
    doc_data_id dm_idb,
    qty dm_qty,
    qdqs_sum dm_qty,
    qdistr_q dm_qty,
    qstorned_q dm_qty
) as
begin
    -- Looks for mismatches between records count in v_qdistr + v_qstorned and doc_data
    -- Must be run ONLY in TIL = SNAPSHOT!
    -- ###################################
    -- Check that current Tx run in NO wait or with lock_timeout.
    -- Otherwise raise error: performance degrades almost to zero.
    execute procedure sp_check_nowait_or_timeout;

    for
        select
            b.doc_id,
            b.optype_id,
            b.rcv_optype_id,
            b.id,
            b.qty,
            b.qdistr_q + coalesce(sum(qs.snd_qty),0) qdqs_sum,
            b.qdistr_q,
            coalesce(sum(qs.snd_qty),0) qstorned_q
        from (
            select d.doc_id, h.optype_id, r.rcv_optype_id, d.id, d.qty --
            ,coalesce(sum(qd.snd_qty),0) qdistr_q
            from doc_data d
            join doc_list h on d.doc_id = h.id
            join rules_for_qdistr r on h.optype_id = r.snd_optype_id
            left join v_qdistr_source qd on
                d.ware_id = qd.ware_id
                and qd.snd_optype_id = r.snd_optype_id
                and qd.rcv_optype_id is not distinct from r.rcv_optype_id
            group by d.doc_id, h.optype_id, r.rcv_optype_id, d.id, d.qty
        ) b
        left join v_qstorned_source qs on b.id = qs.snd_id and b.optype_id=qs.snd_optype_id and b.rcv_optype_id=qs.rcv_optype_id
        group by
            b.doc_id,
            b.optype_id,
            b.rcv_optype_id,
            b.id,
            b.qty,
            b.qdistr_q
        having b.qty <> b.qdistr_q + coalesce(sum(qs.snd_qty),0)
        into
            doc_id,
            optype_id,
            rcv_optype_id,
            doc_data_id,
            qty,
            qdqs_sum,
            qdistr_q,
            qstorned_q
    do suspend;
end

^ -- srv_diag_qty_distr

--------------------------------------------------------------------------------
-- #############    D E B U G:     D U M P    D I R T Y     D A T A  ###########
--------------------------------------------------------------------------------

create or alter procedure zdump4dbg(
       a_doc_list_id bigint default null,
       a_doc_data_id bigint default null,
       a_ware_id bigint default null
)
as
    declare v_catch_bitset bigint;
    declare id bigint;
    declare trn_id bigint;
    declare snd_optype_id bigint;
    declare rcv_optype_id bigint;
    declare qty numeric(12,3);
    declare dup_cnt int;
    declare qty_bak numeric(12,3);
    declare snd_qty numeric(12,3);
    declare rcv_qty numeric(12,3);

    declare snd_id bigint;
    declare rcv_id bigint;
    declare doc_id bigint;
    declare ware_id bigint;
    declare optype_id bigint;
    declare agent_id bigint;
    declare state_id bigint;
    declare dts_open timestamp;
    declare dts_fix timestamp;
    declare dts_clos timestamp;
    declare dts_edit timestamp;
    declare base_doc_id bigint;
    declare acn_type type of dm_account_type;

    declare dependend_doc_id bigint;
    declare dependend_doc_state bigint;
    declare dependend_doc_dbkey dm_dbkey;
    declare dependend_doc_agent_id bigint;
    declare base_doc_qty numeric(12,3);
    declare dependend_doc_qty numeric(12,3);

    declare cost_purchase numeric(12,2);
    declare cost_retail numeric(12,2);
    declare snd_purchase numeric(12,2);
    declare snd_retail numeric(12,2);
    declare rcv_purchase numeric(12,2);
    declare rcv_retail numeric(12,2);
    declare snd_cost numeric(12,2);
    declare rcv_cost numeric(12,2);

    declare v_curr_att int;
    declare v_curr_trn int;
    declare i int;
    declare v_step int = 1000;
    declare v_max_id bigint;
    declare v_perf_semaphore_id bigint;
    declare v_perf_progress_id bigint;
    declare v_this dm_dbobj = 'zdump4dbg';
begin
    -- See oltp_main_filling.sql for definition of bitset var `QMISM_VERIFY_BITSET`:
    -- bit#0 := 1 ==> perform calls of srv_catch_qd_qs_mism in doc_list_aiud => sp_add_invnt_log
    --                in order to register mismatches b`etween doc_data.qty and total number of rows
    --                in v_qdistr_source + v_qstorned_source for doc_data.id
    -- bit#1 := 1 ==> perform calls of SRV_CATCH_NEG_REMAINDERS from INVNT_TURNOVER_LOG_AI
    --                (instead of totalling turnovers to `invnt_saldo` table)
    -- bit#2 := 1 ==> allow dump dirty data into z-tables for analysis, see sp zdump4dbg, in case
    --                when some 'bad exception' occurs (see ctx var `HALT_TEST_ON_ERRORS`)
    v_catch_bitset = cast(rdb$get_context('USER_SESSION','QMISM_VERIFY_BITSET') as bigint);
    if ( bin_and( v_catch_bitset, 4 ) = 0 ) -- dump dirty data DISABLED
    then
        --####
          exit;
        --####

    v_curr_att = current_connection;
    v_curr_trn = current_transaction;
    v_perf_semaphore_id = null;

    -- record with EMPTY is added by 1run_oltp_emul.bat on every new start of test,
    -- it always contains EMPTY string in field `info` at this moment:
    select id from perf_log g
    where g.unit = 'dump_dirty_data_semaphore'
    order by id
    rows 1
    into v_perf_semaphore_id;
    if ( v_perf_semaphore_id is null ) then
    begin
        exit;
    end

    in autonomous transaction do
        update perf_log g set
            g.info = 'start, tra_'||:v_curr_trn,
            dts_beg = 'now',
            dts_end = null
        where g.id = :v_perf_semaphore_id
              and g.dts_beg is null;

    -- jump to when-section if lock_conflict, see below --
    if ( row_count = 0 ) then -- ==> this job was already done by another attach
    begin
        exit;
    end

    -- record for show progress in case of watching from IBE etc:
    in autonomous transaction do
        insert into perf_log(unit, dts_beg) values( 'dump_dirty_data_progress', current_timestamp )
        returning id into v_perf_progress_id;

    -- dumps dirty data into tables for further analysis before halt (4debug only)
    ----------------------------------------------------------------------------
    for
        select c.id,c.snd_optype_id,c.rcv_optype_id,c.qty,c.dup_cnt,c.qty_bak
        from tmp$shopping_cart c
        --as cursor ct
        into id,snd_optype_id,rcv_optype_id,qty,dup_cnt,qty_bak
    do
        in autonomous transaction do
        insert into ztmp_shopping_cart(id, snd_optype_id, rcv_optype_id, qty, dup_cnt, qty_bak)
        values( :id,  :snd_optype_id,  :rcv_optype_id,  :qty,  :dup_cnt, :qty_bak)
    ;
    ----------------------------------------------------------------------------
    for
        select
            base_doc_id,dependend_doc_id,dependend_doc_state,dependend_doc_dbkey
            ,dependend_doc_agent_id,ware_id,base_doc_qty,dependend_doc_qty
        from tmp$dep_docs
        into
            base_doc_id,dependend_doc_id,dependend_doc_state,dependend_doc_dbkey
            ,dependend_doc_agent_id,ware_id,base_doc_qty,dependend_doc_qty
    do
        in autonomous transaction do
        insert into ztmp_dep_docs(
            base_doc_id,dependend_doc_id,dependend_doc_state,dependend_doc_dbkey
            ,dependend_doc_agent_id,ware_id,base_doc_qty,dependend_doc_qty
            ,dump_att
            ,dump_trn
        )
        values(
            :base_doc_id,:dependend_doc_id,:dependend_doc_state,:dependend_doc_dbkey
            ,:dependend_doc_agent_id,:ware_id,:base_doc_qty,:dependend_doc_qty
            ,:v_curr_att
            ,:v_curr_trn
        )
    ;

    ----------------------------------------------------------------------------
    --   dump dirty data from   ### d o c _ l i s t ###
    select 0, max(id) from doc_list into i,v_max_id; -- for verbosing in perf_log.stack
    for
        select
            id
            ,optype_id
            ,agent_id
            ,state_id
            ,dts_open
            ,dts_fix
            ,dts_clos
            ,base_doc_id
            ,acn_type
            ,cost_purchase
            ,cost_retail
        from doc_list h
        where h.id = :a_doc_list_id or :a_doc_list_id is null
        into
            :id
            ,:optype_id
            ,:agent_id
            ,:state_id
            ,:dts_open
            ,:dts_fix
            ,:dts_clos
            ,:base_doc_id
            ,:acn_type
            ,:cost_purchase
            ,:cost_retail
    do
    begin
        in autonomous transaction do
        insert into zdoc_list(
            id
            ,optype_id
            ,agent_id
            ,state_id
            ,dts_open
            ,dts_fix
            ,dts_clos
            ,base_doc_id
            ,acn_type
            ,cost_purchase
            ,cost_retail
        )
        values(
            :id
            ,:optype_id
            ,:agent_id
            ,:state_id
            ,:dts_open
            ,:dts_fix
            ,:dts_clos
            ,:base_doc_id
            ,:acn_type
            ,:cost_purchase
            ,:cost_retail
         );
        if ( mod(i, v_step) = 0 ) then
            in autonomous transaction do
            update perf_log g set g.stack = 'doc_list: id='||:id||', max='||:v_max_id
            where g.id = :v_perf_progress_id;
        i = i + 1;

     end

    ----------------------------------------------------------------------------
    --   dump dirty data from   ### d o c _ d a t a ###
    select 0, max(id) from doc_data into i,v_max_id; -- for verbosing in perf_log.stack
    for
        select
            id
            ,doc_id
            ,ware_id
            ,qty
            ,cost_purchase
            ,cost_retail
            ,dts_edit
        from doc_data d
        where d.id between coalesce(:a_doc_data_id, -9223372036854775807) and coalesce(:a_doc_data_id, 9223372036854775807)
              and
              d.ware_id between coalesce(:a_ware_id, -9223372036854775807) and coalesce(:a_ware_id, 9223372036854775807)
        --as cursor cd
        into
            id
            ,doc_id
            ,ware_id
            ,qty
            ,cost_purchase
            ,cost_retail
            ,dts_edit
    do
    begin
        in autonomous transaction do
        insert into zdoc_data(
            id
            ,doc_id
            ,ware_id
            ,qty
            ,cost_purchase
            ,cost_retail
            ,dts_edit
            ,dump_att
            ,dump_trn
        )
        values(
            :id
            ,:doc_id
            ,:ware_id
            ,:qty
            ,:cost_purchase
            ,:cost_retail
            ,:dts_edit
            ,:v_curr_att
            ,:v_curr_trn
        );
        if ( mod(i, v_step) = 0 ) then
            in autonomous transaction do
            update perf_log g set g.stack = 'doc_data: id='||:id||', max='||:v_max_id
            where g.id = :v_perf_progress_id;
        i = i + 1;
    end
    ----------------------------------------------------------------------------
    -- 27.06.2014 dump dirty data from  ### q d i s t r  ###
    select 0, max(id) from v_qdistr_source into i,v_max_id; -- for verbosing in perf_log.stack
    for
        select
            id
            ,doc_id
            ,ware_id
            ,snd_optype_id
            ,snd_id
            ,snd_qty
            ,rcv_optype_id
            ,rcv_id
            ,rcv_qty
            ,snd_purchase
            ,snd_retail
            ,rcv_purchase
            ,rcv_retail
            ,trn_id
            ,dts
        from v_qdistr_source d
        where d.ware_id between coalesce(:a_ware_id, -9223372036854775807) and coalesce(:a_ware_id, 9223372036854775807)
        --as cursor cq
        into
            id
            ,doc_id
            ,ware_id
            ,snd_optype_id
            ,snd_id
            ,snd_qty
            ,rcv_optype_id
            ,rcv_id
            ,rcv_qty
            ,snd_purchase
            ,snd_retail
            ,rcv_purchase
            ,rcv_retail
            ,trn_id
            ,dts_edit
    do
    begin
        in autonomous transaction do
        insert into zqdistr (
            id
            ,doc_id
            ,ware_id
            ,snd_optype_id
            ,snd_id
            ,snd_qty
            ,rcv_optype_id
            ,rcv_id
            ,rcv_qty
            ,snd_purchase
            ,snd_retail
            ,rcv_purchase
            ,rcv_retail
            ,trn_id
            ,dts
            ,dump_att
            ,dump_trn
        )
        values(
            :id
            ,:doc_id
            ,:ware_id
            ,:snd_optype_id
            ,:snd_id
            ,:snd_qty
            ,:rcv_optype_id
            ,:rcv_id
            ,:rcv_qty
            ,:snd_purchase
            ,:snd_retail
            ,:rcv_purchase
            ,:rcv_retail
            ,:trn_id
            ,:dts_edit
            ,:v_curr_att
            ,:v_curr_trn
        );
        if ( mod(i, v_step) = 0 ) then
            in autonomous transaction do
            update perf_log g set g.stack = 'v_qdistr_source: id='||:id||', max='||:v_max_id
            where g.id = :v_perf_progress_id;
        i = i + 1;
    end
    ----------------------------------------------------------------------------
    -- 27.06.2014 dump dirty data from  ### q s t o r n e d  ###
    select 0, max(id) from v_qstorned_source into i,v_max_id; -- for verbosing in perf_log.stack
    for
        select
            id
            ,doc_id
            ,ware_id
            ,snd_optype_id
            ,snd_id
            ,snd_qty
            ,rcv_optype_id
            ,rcv_id
            ,rcv_qty
            ,snd_purchase
            ,snd_retail
            ,rcv_purchase
            ,rcv_retail
            ,trn_id
            ,dts
        from v_qstorned_source d
        where d.ware_id between coalesce(:a_ware_id, -9223372036854775807) and coalesce(:a_ware_id, 9223372036854775807)
        into
            id
            ,doc_id
            ,ware_id
            ,snd_optype_id
            ,snd_id
            ,snd_qty
            ,rcv_optype_id
            ,rcv_id
            ,rcv_qty
            ,snd_purchase
            ,snd_retail
            ,rcv_purchase
            ,rcv_retail
            ,trn_id
            ,dts_edit
    do
    begin
        in autonomous transaction do
        insert into zqstorned(
            id
            ,doc_id
            ,ware_id
            ,snd_optype_id
            ,snd_id
            ,snd_qty
            ,rcv_optype_id
            ,rcv_id
            ,rcv_qty
            ,snd_purchase
            ,snd_retail
            ,rcv_purchase
            ,rcv_retail
            ,trn_id
            ,dts
            ,dump_att
            ,dump_trn
        )
        values(
            :id
            ,:doc_id
            ,:ware_id
            ,:snd_optype_id
            ,:snd_id
            ,:snd_qty
            ,:rcv_optype_id
            ,:rcv_id
            ,:rcv_qty
            ,:snd_purchase
            ,:snd_retail
            ,:rcv_purchase
            ,:rcv_retail
            ,:trn_id
            ,:dts_edit
            ,:v_curr_att
            ,:v_curr_trn
         );
        if ( mod(i, v_step) = 0 ) then
            in autonomous transaction do
            update perf_log g set g.stack = 'v_qstorned_source: id='||:id||', max='||:v_max_id
            where g.id = :v_perf_progress_id;
        i = i + 1;
    end
    ---------------------------------------------------------------------------
    -- 04.07.2014 dump dirty data from  ### p d i s t r,    p s t o r n e d  ###
    select 0, max(id) from pdistr into i,v_max_id; -- for verbosing in perf_log.stack
    for
        select
            id
            ,agent_id
            ,snd_optype_id
            ,snd_id
            ,snd_cost
            ,rcv_optype_id
            ,trn_id
        from pdistr
        into
            id
            ,agent_id
            ,snd_optype_id
            ,snd_id
            ,snd_cost
            ,rcv_optype_id
            ,trn_id
    do
    begin
        in autonomous transaction do
        insert into zpdistr(
            id
            ,agent_id
            ,snd_optype_id
            ,snd_id
            ,snd_cost
            ,rcv_optype_id
            ,trn_id
            ,dump_att
            ,dump_trn
        )
        values(
            :id
            ,:agent_id
            ,:snd_optype_id
            ,:snd_id
            ,:snd_cost
            ,:rcv_optype_id
            ,:trn_id
            ,:v_curr_att
            ,:v_curr_trn
         );
        if ( mod(i, v_step) = 0 ) then
            in autonomous transaction do
            update perf_log g set g.stack = 'pdistr: id='||:id||', max='||:v_max_id
            where g.id = :v_perf_progress_id;
        i = i + 1;
    end

    ----------------------------------------------------------------------------
    select 0, max(id) from pstorned into i,v_max_id; -- for verbosing in perf_log.stack
    for
        select
            id
            ,agent_id
            ,snd_optype_id
            ,snd_id
            ,snd_cost
            ,rcv_optype_id
            ,rcv_id
            ,rcv_cost
            ,trn_id
        from pstorned
        into
            id
            ,agent_id
            ,snd_optype_id
            ,snd_id
            ,snd_cost
            ,rcv_optype_id
            ,rcv_id
            ,rcv_cost
            ,trn_id
    do
    begin
        in autonomous transaction do
        insert into zpstorned(
            id
            ,agent_id
            ,snd_optype_id
            ,snd_id
            ,snd_cost
            ,rcv_optype_id
            ,rcv_id
            ,rcv_cost
            ,trn_id
            ,dump_att
            ,dump_trn
        )
        values(
            :id
            ,:agent_id
            ,:snd_optype_id
            ,:snd_id
            ,:snd_cost
            ,:rcv_optype_id
            ,:rcv_id
            ,:rcv_cost
            ,:trn_id
            ,:v_curr_att
            ,:v_curr_trn
         );
        if ( mod(i, v_step) = 0 ) then
            in autonomous transaction do
            update perf_log g set g.stack = 'pstorned: id='||:id||', max='||:v_max_id
            where g.id = :v_perf_progress_id;
        i = i + 1;
     end

    in autonomous transaction do
    begin
        update perf_log g
        set g.info = 'finish, tra_'||:v_curr_trn,
            g.dts_end = 'now'
            --stack = fn_get_stack(1)
        where g.id = :v_perf_semaphore_id;
        delete from perf_log g where g.id = :v_perf_progress_id;
    end

when any do
    begin
        -- nop: supress ANY exception! We now dump dirty data due to abnormal case! --
    end
end

^ -- zdump4dbg

set term ;^
 

set list on;
set echo off;
select 'oltp_misc_debug.sql finish at ' || current_timestamp as msg from rdb$database;
set list off;


-- #################################
-- End of script oltp_misc_debug.sql  // ###   O P T I O N A L   ###
-- Next run: oltp_split_heavy_tabs_0 | 1.sql - depending on config parameter 'create_with_split_heavy_tabs'
-- #################################

-- #####################################
-- Begin of script oltp_split_heavy_tabs_1.sql 
-- #####################################
-- ::: NB ::: This script is COMMON for both FB 2.5 and 3.0 and should be called 
-- after oltp_main_filling.sql and oltp_misc_debug.sql 

-- run: isql /3333:oltp30 -i oltp_split_heavy_tabs_1.sql  | sed "s/[ 	]*$//" 1>log.tmp

set list on;
set echo off;
select 'oltp_split_heavy_tabs_1.sql start at ' || current_timestamp as msg from rdb$database;
set list off;

set echo off;

set term ^;

create or alter procedure tmp_init_autogen_qdistr_tables
as
    declare v_ddl_const varchar(1024);

    declare v_idx_expr1 varchar(1024);
    declare v_idx_expr2 varchar(1024);
    declare v_idx_suff1 varchar(31);
    declare v_idx_suff2 varchar(31);
    declare v_ddl_qdidx1 varchar(1024);
    declare v_ddl_qdidx2 varchar(1024);

    declare v_qd_table varchar(31);
    declare v_qd_suffix varchar(31);
    declare v_ddl_qdistr varchar(1024);
    declare v_id bigint;
    declare v_build_with_qd_compound_ordr varchar(31); -- 'most_selective_first' or 'least_selective_first'
    declare v_make_separate_qd_idx smallint;
begin

    -- Called from 1build_oltp_emul.bat  when config setting create_with_split_heavy_tabs = 1, see:
    -- echo execute procedure tmp_init_autogen_qdistr_tables; >> %...%

    v_ddl_const = '
       id dm_idb not null
      ,doc_id dm_idb -- denorm for speed, also 4debug
      ,ware_id dm_idb
      ,snd_optype_id dm_ids -- denorm for speed
      ,snd_id dm_idb -- ==> doc_data.id of "sender"
      ,snd_qty dm_qty
      ,rcv_doc_id bigint -- 30.12.2014, always null, for some debug views
      ,rcv_optype_id dm_ids
      ,rcv_id bigint -- nullable! ==> doc_data.id of "receiver"
      ,rcv_qty numeric(12,3)
      ,snd_purchase dm_cost
      ,snd_retail dm_cost
      ,rcv_purchase dm_cost
      ,rcv_retail dm_cost
      ,trn_id bigint default current_transaction
      ,dts timestamp default ''now''
    ';


    -- This row is created in 1run_oltp_emul.bat, in sub-routine "make_db_objects":
    -- Value is defined by config parameter create_with_split_heavy_tabs = 0 | 1.
    select s.svalue 
    from settings s 
    where s.working_mode='COMMON' and s.mcode='BUILD_WITH_SEPAR_QDISTR_IDX'
    into v_make_separate_qd_idx;
    
    -- This row is created in 1run_oltp_emul.bat, in sub-routine "make_db_objects":
    -- Value is defined by config parameter create_with_compound_columns_order = 'most_selective_first' or 'least_selective_first'
    select s.svalue 
    from settings s 
    where s.working_mode='COMMON' and s.mcode='BUILD_WITH_QD_COMPOUND_ORDR'
    into v_build_with_qd_compound_ordr;
    
    v_idx_expr1 = '';
    v_idx_expr2 = '';
    -- 24.10.2015: do NOT remove 'snd_optype_id' and 'rcv_optype_id' from index key
    -- otherwise excessive index scans will occur in each XQD* table even if it has no
    -- such key. See SP srv_find_qd_qs_mism which is called after each document creation
    -- (this SP, in turn, is called from doc_list_aiud trigger when QMISM_VERIFY_BITSET = 1,
    -- see oltp_main_filling.sql).
    -- See also sp_get_clo_for_invoice - there is query that search only for ware_id, w/o snd_id!
    if ( v_make_separate_qd_idx = 1 ) then
        begin
            if ( upper(v_build_with_qd_compound_ordr) = upper('least_selective_first') ) then
                begin
                    v_idx_expr1 = '(snd_optype_id, rcv_optype_id, ware_id)'; -- do NOT remove snd_optype & rcv_optype!
                    v_idx_expr2 = '(snd_id)';
                    v_idx_suff1 = 'sop_rop_ware';
                    v_idx_suff2 = 'snd';
                end
            else -- 'most_selective_first'
                begin
                    v_idx_expr1 = '(ware_id, snd_optype_id, rcv_optype_id)'; -- do NOT remove snd_optype & rcv_optype!
                    v_idx_expr2 = '(snd_id)';
                    v_idx_suff1 = 'ware_sop_rop';
                    v_idx_suff2 = 'snd';
                end
        end
    else
        begin
            if ( upper(v_build_with_qd_compound_ordr) = upper('least_selective_first') ) then
                begin
                    v_idx_expr1 = '(snd_optype_id, rcv_optype_id, ware_id, snd_id)'; -- do NOT remove snd_optype & rcv_optype!
                    v_idx_suff1 = 'sop_rop_ware_snd';
                end
            else -- 'most_selective_first'
                begin
                    v_idx_expr1 = '(ware_id, snd_optype_id, rcv_optype_id, snd_id)'; -- do NOT remove snd_optype & rcv_optype!
                    v_idx_suff1 = 'ware_sop_rop_snd';
                end
        end

    for
        select '' || q.snd_optype_id || '_' || q.rcv_optype_id
        from rules_for_qdistr q
        where q.snd_optype_id is not null
        into v_qd_suffix --------------------------- '1000_1200'; '1200_2000' etc
    do begin
        v_qd_table = 'xqd_' || v_qd_suffix;
        v_ddl_qdistr = 'recreate table ' || v_qd_table || '(' || v_ddl_const || ')';
        
        v_ddl_qdidx1 = 'create index ' || v_qd_table || '_' || v_idx_suff1 || ' on ' || v_qd_table || v_idx_expr1;
        v_ddl_qdidx2 = 'create index ' || v_qd_table || '_' || v_idx_suff2 || ' on ' || v_qd_table || v_idx_expr2;

        in autonomous transaction do
        begin
            execute statement v_ddl_qdistr;
            if ( not v_ddl_qdidx1 = '' ) then execute statement v_ddl_qdidx1;
            if ( not v_ddl_qdidx2 = '' ) then execute statement v_ddl_qdidx2;
            if ( v_qd_suffix = '1000_3300' ) then -- 13.11.2015: make v_min_id_clo_res much faster
                execute statement 'create index xqd_1000_3300_doc on xqd_1000_3300(doc_id)';

        end
    end
    if ( v_ddl_qdistr is null ) then
       -- This script should be called ***AFTER*** oltp_main_filling.sql which does fill table 'optypes'.
       -- Probably this table currently is empty!
       exception ex_record_not_found;
       --'required record not found, datasource: @1, key: @2';

end -- tmp_init_autogen_qdistr_tables

^ 
set term ;^

set term ^;

create or alter procedure tmp_init_autogen_qstorn_tables
as
    declare v_ddl_const varchar(1024);
    declare v_idx_expr1 varchar(1024);
    declare v_idx_expr2 varchar(1024);
    declare v_idx_expr3 varchar(1024);
    declare v_qs_table varchar(31);
    declare v_qs_suffix varchar(31);
    declare v_ddl_qstorn varchar(1024);
    declare v_ddl_qsidx1 varchar(1024);
    declare v_ddl_qsidx2 varchar(1024);
    declare v_ddl_qsidx3 varchar(1024);
    declare v_id bigint;
begin
    -- Called from 1build_oltp_emul.bat  when config setting create_with_split_heavy_tabs = 1, see:
    -- echo execute procedure tmp_init_autogen_qstorn_tables; >> %...%

    v_ddl_const = '
       id dm_idb not null
      ,doc_id dm_idb -- denorm for speed
      ,ware_id dm_idb
      ,snd_optype_id dm_ids -- denorm for speed
      ,snd_id dm_idb -- ==> doc_data.id of "sender"
      ,snd_qty dm_qty
      ,rcv_doc_id dm_idb -- 30.12.2014, for enable to remove PK on doc_data, see S    P_LOCK_DEPENDENT_DOCS
      ,rcv_optype_id dm_ids
      ,rcv_id dm_idb
      ,rcv_qty dm_qty
      ,snd_purchase dm_cost
      ,snd_retail dm_cost
      ,rcv_purchase dm_cost
      ,rcv_retail dm_cost
      ,trn_id bigint default current_transaction
      ,dts timestamp default ''now''
    ';
    v_idx_expr1='(doc_id)';
    v_idx_expr2='(snd_id)';
    v_idx_expr3='(rcv_id)';
    for
--        select o.id
--        from optypes o
--        where o.acn_type in('1', '2', 'i', 'o') -- all operations that affect on quantity remainders
--        into v_id
        select '' || q.snd_optype_id || '_' || q.rcv_optype_id
        from rules_for_qdistr q
        where q.snd_optype_id is not null
        into v_qs_suffix
    do begin
        v_qs_table = 'xqs_' || v_qs_suffix;
        v_ddl_qstorn = 'recreate table ' || v_qs_table || '(' || v_ddl_const || ')';
        v_ddl_qsidx1 = 'create index '||v_qs_table||'_doc_id on ' || v_qs_table || v_idx_expr1;
        v_ddl_qsidx2 = 'create index '||v_qs_table||'_snd_id on ' || v_qs_table || v_idx_expr2;
        v_ddl_qsidx3 = 'create index '||v_qs_table||'_rcv_id on ' || v_qs_table || v_idx_expr3;
        in autonomous transaction do
        begin
            execute statement v_ddl_qstorn;
            execute statement v_ddl_qsidx1;
            execute statement v_ddl_qsidx2;
            if ( upper(v_qs_table) <> upper('xqs_3300_3400') ) then
                -- 25.11.2015, look at index statistics of 'xqs_3300_3400': 
                -- there are 100% dups in the field 'rcv_id', it has NULL value in all rows.
                -- We have to avoid creation of this index, it's absolutely useless!
                execute statement v_ddl_qsidx3;
        end
    end

    if ( v_ddl_qstorn is null ) then 
       -- This script should be called ***AFTER*** oltp_main_filling.sql which does fill table 'optypes'.
       -- Probably this table currently is empty!
       exception ex_record_not_found;

end -- tmp_init_autogen_qstorn_tables

^  

create or alter procedure tmp_remove_dyn_in_random_id
returns(src varchar(32765)) as
    declare v_name_fin varchar(31);
    declare v_name_min varchar(31);
    declare v_name_max varchar(31);
    declare v_line_type varchar(12);
    declare v_add_comment smallint;
    declare v_body_repl varchar(32765);
    
    declare v_body_line varchar(8190) character set utf8;
    declare v_line_repl varchar(8190) character set utf8;
    declare i int;
    declare v_lf char(10);
begin
    v_lf = ascii_char(10);

    v_line_type = 'std:start';
    v_add_comment = 0;
        delete from tmp$autogen$rand$calls;
        delete from tmp$autogen$rand$calls;
        
        insert into tmp$autogen$rand$calls(
            view_name_for_find
            ,view_name_for_min_id
            ,view_name_for_max_id
        )
        select
        'v_all_wares', null, null
        from rdb$database union all select
        'v_random_find_clo_ord'
        ,'v_min_id_clo_ord'
        ,'v_max_id_clo_ord'
        from rdb$database union all select
        'v_random_find_ord_sup'
        ,'v_min_id_ord_sup'
        ,'v_max_id_ord_sup'
        from rdb$database union all select
        'v_random_find_clo_res'
        ,'v_min_id_clo_res'
        ,'v_max_id_clo_res'
        from rdb$database union all select
        'v_min_id_avl_res'
        ,'v_max_id_avl_res'
        ,'v_random_find_avl_res'
        from rdb$database union all select
        'v_random_find_non_paid_invoice'
        ,'v_min_non_paid_invoice'
        ,'v_max_non_paid_invoice'
        from rdb$database union all select
        'v_random_find_non_paid_realizn'
        ,'v_min_non_paid_realizn'
        ,'v_max_non_paid_realizn'
    
        from rdb$database union all select
        'v_reserve_write_off',null, null
        from rdb$database union all select
        'v_cancel_client_order',null, null
        from rdb$database union all select
        'v_cancel_customer_reserve',null, null
        from rdb$database union all select
        'v_add_invoice_to_stock',null, null
        from rdb$database union all select
    
        'v_cancel_adding_invoice',null, null
        from rdb$database union all select
        'v_cancel_supplier_invoice',null, null
        from rdb$database union all select
        'v_cancel_customer_prepayment',null, null
        from rdb$database union all select
        'v_cancel_payment_to_supplier',null, null
        from rdb$database  union all select
        'v_all_customers',null, null
        from rdb$database union all select
        'v_all_suppliers',null, null
        from rdb$database
        ;
    
      delete from tmp$autogen$source;
      for
          select p.src from sys_get_func_ddl('fn$get$random$id$subst$names',1,1) p
          into v_line_repl
      do begin
        if ( v_line_repl containing '$name_to_substutite_start_of_loop' ) then v_line_type = 'var:subst';
        insert into tmp$autogen$source(line_no, text, line_type) values(:i, :v_line_repl, :v_line_type);
        if ( v_line_repl containing '$name_to_substutite_end_of_loop' ) then v_line_type = 'std:final';
      end
    
      v_body_repl = '';
      for
          select text
          from tmp$autogen$source s
          where s.line_type = 'std:start'
          into v_line_repl
      do begin
         if ( v_add_comment = 0 and trim(v_line_repl) collate unicode_ci starting with 'declare'  ) then
             begin
                  v_line_repl = 'declare "!ACHTUNG_READ_ME_1!" varchar(255) = ''### GENERATED AUTO, BASED ON INITIAL SOURCE OF "FN_GET_RANDOM_ID". DO NOT EDIT ###'';';
                  --suspend;
                  v_add_comment = 1;
             end
         else
             begin
                 v_line_repl = replace(v_line_repl collate unicode_ci, 'fn$get$random$id$subst$names', 'fn_get_random_id');
             end

         if ( char_length(v_body_repl) + char_length(v_line_repl) + 2 < 32765 ) then
            begin
              v_body_repl = v_body_repl || v_line_repl || v_lf;
            end
         else
            begin
                src = v_body_repl;
                suspend;
                v_body_repl = v_line_repl || v_lf;
            end

      end
      src = v_body_repl;
      suspend;
    
      v_body_repl = '';

      for
        select
            view_name_for_find
            ,coalesce(view_name_for_min_id, view_name_for_find)
            ,coalesce(view_name_for_max_id, view_name_for_find)
        from tmp$autogen$rand$calls
        --rows 2
      into
          v_name_fin
          ,v_name_min
          ,v_name_max
      do
      begin
          v_body_repl = '';
          for
              select text
              from tmp$autogen$source s
              where s.line_type = 'var:subst'
              into v_line_repl
          do begin
             v_line_repl = replace(v_line_repl collate unicode_ci, 'name$to$substutite$min$id$', v_name_min);
             v_line_repl = replace(v_line_repl collate unicode_ci, 'name$to$substutite$max$id$', v_name_max);
             v_line_repl = replace(v_line_repl collate unicode_ci, 'name$to$substutite$search$', v_name_fin);

             if ( char_length(v_body_repl) + char_length(v_line_repl) + 2 < 32765 ) then
                begin
                  v_body_repl = v_body_repl || v_line_repl || v_lf;
                end
             else
                begin
                    src = v_body_repl;
                    suspend;
                    v_body_repl = v_line_repl || v_lf;
                end
          end
          src = v_body_repl;
          suspend;
      end
    
      v_body_repl = '';
      for
          select text
          from tmp$autogen$source s
          where s.line_type = 'std:final'
          into v_line_repl
      do begin
         if ( char_length(v_body_repl) + char_length(v_line_repl) + 2 < 32765 ) then
            begin
              v_body_repl = v_body_repl || v_line_repl || v_lf;
            end
         else
            begin
                src = v_body_repl;
                suspend;
                v_body_repl = v_line_repl || v_lf;
            end
      end
      src = v_body_repl;
      suspend;

end -- tmp_remove_dyn_in_random_id

^

set term ;^

set list on;
set echo off;
select 'oltp_split_heavy_tabs_1.sql finish at ' || current_timestamp as msg from rdb$database;

--rollback;

drop exception ex_exclusive_required;
drop exception ex_not_suitable_fb_version;
COMMIT;
select count(*) as cnt_collations from rdb$collations s where s.rdb$system_flag is distinct from 1;
select count(*) as cnt_domains from rdb$fields s where s.rdb$system_flag is distinct from 1 and s.rdb$field_name not starting with 'RDB$';
select count(*) as cnt_exceptions from rdb$exceptions s where s.rdb$system_flag is distinct from 1;
select count(*) as cnt_functions from rdb$functions s where s.rdb$system_flag is distinct from 1;
select count(*) as cnt_generators from rdb$generators s where s.rdb$system_flag = 0; --  do NOT: is distinct from 1; -- can be '6' for autoincrements!
select count(*) as cnt_indices from rdb$indices s where s.rdb$system_flag is distinct from 1;
select count(*) as cnt_procedures from rdb$procedures s where s.rdb$system_flag is distinct from 1;
select count(*) as cnt_tables_or_views from rdb$relations s where s.rdb$system_flag is distinct from 1;
select count(*) as cnt_triggers from rdb$triggers s where s.rdb$system_flag is distinct from 1;
set list off;

"""

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    MSG                             oltp30_DDL.sql start at 2016-04-11 15:50:59.5620
    MSG                             oltp30_DDL.sql finish at 2016-04-11 15:51:01.0780
    MSG                             oltp30_sp.sql start at 2016-04-11 15:51:01.0780
    MSG                             oltp30_sp.sql finish at 2016-04-11 15:51:02.0460
    MSG                             oltp_misc_debug.sql start at 2016-04-11 15:51:02.0460
    MSG                             oltp_misc_debug.sql finish at 2016-04-11 15:51:02.4210
    MSG                             oltp_split_heavy_tabs_1.sql start at 2016-04-11 15:51:02.4210
    MSG                             oltp_split_heavy_tabs_1.sql finish at 2016-04-11 15:51:02.4370

    CNT_COLLATIONS                  2
    CNT_DOMAINS                     21
    CNT_EXCEPTIONS                  22
    CNT_FUNCTIONS                   28
    CNT_GENERATORS                  7
    CNT_INDICES                     85
    CNT_PROCEDURES                  100
    CNT_TABLES_OR_VIEWS             132
    CNT_TRIGGERS                    24
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
