-- DDL of objects that are used in aux. database mon-stat-gathering-3_0.fdb
---------------------------------------------------------------------------
create or alter view v_agg_stat_main as select 1 id from rdb$database;
create or alter view v_agg_stat_tabs as select 1 id from rdb$database;
create or alter procedure sp_truncate_stat as begin end;
create or alter procedure sp_gather_stat as begin end;
recreate sequence g_gather_stat;
commit;
recreate table stat_log_main(
    rowset int -- for grouping records that related to the same measurement
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
   -----------------------------------------------------------------------------
   ,dump_trn bigint default current_transaction
   ,mult smallint check(mult in(-1,1))
   ,dts timestamp default 'now'
   ,constraint stat_log_main_dump_trn_unq unique(dump_trn) using index stat_log_main_dump_trn_unq
);
commit;

-- 29.08.2014
recreate table stat_log_tabs(
    rowset int -- for grouping records that related to the same measurement
   ,table_name char(31) character set unicode_fss
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
   ,table_id smallint
   ,is_system_table smallint
   ,rel_type smallint
   ,dump_trn bigint default current_transaction
   ,mult smallint check(mult in(-1,1))
   ,dts timestamp default 'now'
);
commit;

------------------------------------------------

set term ^;
create or alter procedure sp_truncate_stat as
begin
  delete from stat_log_tabs;
  delete from stat_log_main;
end
^
create or alter procedure sp_gather_stat as
    declare v_rowset int = 1;
    declare v_mult smallint;
begin
    v_rowset = (gen_id(g_gather_stat,1)+1)/2;
    v_mult = iif( mod( gen_id(g_gather_stat,0), 2) = 1, -1, 1 );
    insert into stat_log_main(
        rowset
        -- mon$io_stats:
       ,pg_reads
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
       ,mult
    )
    select
         :v_rowset
        -- mon$io_stats:
        ,i.mon$page_reads
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
        ,:v_mult
    from mon$attachments a
    --left join mon$statements s on a.mon$attachment_id = s.mon$attachment_id     
    left join mon$memory_usage u on a.mon$stat_id=u.mon$stat_id     
    left join mon$io_stats i on a.mon$stat_id=i.mon$stat_id     
    left join mon$record_stats r on a.mon$stat_id=r.mon$stat_id     
    where     
      a.mon$attachment_id = current_connection;

    -- 29.08.2014: use also mon$table_stats to analyze per table:
    insert into stat_log_tabs(
         rowset
        ,table_name         
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
        ,stat_id
        ,mult
    )
    select
         :v_rowset
        ,t.mon$table_name
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
        ,a.mon$stat_id
        ,:v_mult
    from mon$record_stats r
    join mon$table_stats t on r.mon$stat_id = t.mon$record_stat_id
    join mon$attachments a on t.mon$stat_id = a.mon$stat_id
    where
        a.mon$attachment_id = current_connection;
end
^
set term ;^
commit;

create or alter view v_agg_stat_tabs as
select
    b.rowset
    ,b.table_name
    ,sum( b.mult * b.rec_seq_reads ) natural_reads
    ,sum( b.mult * b.rec_idx_reads ) indexed_reads
    ,sum( b.mult * b.rec_inserts ) inserts
    ,sum( b.mult * b.rec_updates ) updades
    ,sum( b.mult * b.rec_deletes ) deletes
    ,sum( b.mult * b.rec_backouts ) backouts
    ,sum( b.mult * b.rec_purges ) purges
    ,sum( b.mult * b.rec_expunges ) expunges
    ,sum( b.mult * b.rec_rpt_reads ) repeated_reads
    ,sum( b.mult * b.bkv_reads ) backvers_reads
    ,sum( b.mult * b.frg_reads ) fragment_reads
    ,sum( b.mult * b.bkv_per_seq_idx_rpt ) backvers_reads_ratio
    ,sum( b.mult * b.frg_per_seq_idx_rpt ) fragment_reads_ratio
from stat_log_tabs b
group by
    b.rowset
    ,b.table_name
;
commit;

create or alter view v_agg_stat_main as
select
    b.rowset
    ,sum( b.mult * b.pg_reads ) page_reads
    ,sum( b.mult * b.pg_writes ) page_writes
    ,sum( b.mult * b.pg_fetches ) page_fetches
    ,sum( b.mult * b.pg_marks ) page_marks
    ,sum( b.mult * b.rec_seq_reads ) natural_reads
    ,sum( b.mult * b.rec_idx_reads ) indexed_reads
    ,sum( b.mult * b.rec_inserts ) inserts
    ,sum( b.mult * b.rec_updates ) updades
    ,sum( b.mult * b.rec_deletes ) deletes
    ,sum( b.mult * b.rec_backouts ) backouts
    ,sum( b.mult * b.rec_purges ) purges
    ,sum( b.mult * b.rec_expunges ) expunges
from stat_log_main b
group by
    b.rowset
;
commit;

