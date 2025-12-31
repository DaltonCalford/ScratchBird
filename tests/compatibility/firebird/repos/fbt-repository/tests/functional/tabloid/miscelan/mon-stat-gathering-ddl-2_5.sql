create or alter view v_agg_stat as select 1 id from rdb$database;
create or alter procedure sp_truncate_stat as begin end;
create or alter procedure sp_gather_stat as begin end;

set term ^;
execute block as
begin
  begin
      execute statement 'drop sequence g_gather_stat';
  when any do begin end
  end
end
^
set term ;^
commit;
create sequence g_gather_stat;

recreate table stat_log(
    rowset int -- for grouping records that related to the same measurement
   ,rec_inserts bigint
   ,rec_updates bigint
   ,rec_deletes bigint
   ,rec_backouts bigint
   ,rec_purges bigint
   ,rec_expunges bigint
   ,rec_seq_reads bigint
   ,rec_idx_reads bigint
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

set term ^;
create or alter procedure sp_truncate_stat as
begin
  delete from stat_log;
end
^
create or alter procedure sp_gather_stat as
    declare v_rowset int = 1;
    declare v_mult smallint;
begin
    v_rowset = (gen_id(g_gather_stat,1)+1)/2;
    v_mult = iif( mod( gen_id(g_gather_stat,0), 2) = 1, -1, 1 );
    insert into stat_log(
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
        ------------------------
        ,u.mon$memory_used
        ,u.mon$memory_allocated
        ,a.mon$stat_id
        ,a.mon$server_pid
        ------------------------
        ,:v_mult
    from mon$attachments a
    left join mon$memory_usage u on a.mon$stat_id=u.mon$stat_id
    left join mon$io_stats i on a.mon$stat_id=i.mon$stat_id     
    left join mon$record_stats r on a.mon$stat_id=r.mon$stat_id     
    where     
      a.mon$attachment_id = current_connection;
end
^
set term ;^
commit;

create or alter view v_agg_stat as
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
from stat_log b
group by
    b.rowset
;
commit;


