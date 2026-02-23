#coding:utf-8

"""
ID:          issue-7208
ISSUE:       7208
TITLE:       Trace: provide performance statistics for DDL statements
DESCRIPTION:
    Test creates all kinds of DDL object, makes altering some of them and then drops all these objects.
    Every DDL statement is supplied with two special tags: "/* DDL_BEG */" and "/*  DDL_END */" at the start and end of it.
    These tags allow to filter only start and final line of DDL, thus simplifying further parsing.
    We launch trace session and run these DDLs. After that, we search in the trace log DDL statements (first and line lines)
    and also we check that after /* DDL_END */ trace log contains two mandatory lines:
        "0 records fetched"
        "Table   ... Natural  ... Purge   Expunge"
    After these lines trace log must have at least one line with some RDB$ table and non-zero statistics.
    Only one line with statistics is taken in account for one DDL (because their quantity can differ between FB versions).
    Concrete values of NR, IR, Inserts are ignored because they can change, so each line from statistics looks just like
    short prefix: 'RDB' (in expected output).
NOTES:
    [24.02.2023] pzotov
        Checked on 5.0.0.958, 4.0.3.2903 -- all fine.
    [13.07.2025] pzotov
        Adjusted patterns: one need to take in account SCHEMA prefix that presents for each table
        in the trace (since 6.0.0.834), e.g.:
            Table                              Natural     Index
            ****************************************************
            "SYSTEM"."RDB$DATABASE"                 10
            "SYSTEM"."RDB$RELATIONS"                          10
            "SYSTEM"."RDB$SCHEMAS"                  20        10
        See 'p_rdb_table_with_stat'.
        Checked on 6.0.0.970; 5.0.3.1683; 4.0.6.3221
"""

import locale
import re
import pytest
from firebird.qa import *

test_sql = '''
    create sequence g_common /* DDL_BEG */ /* DDL_END */;
    create collation name_coll for utf8 from unicode case insensitive /* DDL_BEG */ /* DDL_END */;
    create exception ex_invalid_value 'Invalid value detected' /* DDL_BEG */ /* DDL_END */;
    create domain dm_idb as bigint /* DDL_BEG */ /* DDL_END */;
    create table test(id dm_idb not null, x dm_idb) /* DDL_BEG */ /* DDL_END */;
    alter table test add constraint test_pk primary key(id) /* DDL_BEG */ /* DDL_END */;
    create index test_x on test(x) /* DDL_BEG */ /* DDL_END */;
    alter index test_x inactive /* DDL_BEG */ /* DDL_END */;
    alter index test_x active /* DDL_BEG */ /* DDL_END */;
    create view v_test as select * from test /* DDL_BEG */ /* DDL_END */;

    set term ^;
    create trigger trg_test_bi before insert on test /* DDL_BEG */ as
    begin
        new.id = coalesce(new.id, gen_id(g_common,1));
    end /* DDL_END */
    ^
    create procedure standalone_proc(a_id dm_idb, a_x dm_idb) /* DDL_BEG */ as
    begin
        update test set x =:a_x where id = :a_id;
    end /* DDL_END */
    ^    
    create function standalone_func returns int /* DDL_BEG */ as
    begin
        update test set id = rand()*10000000;
  	    return (select max(id) from test);
    end /* DDL_END */
    ^

    create package pg_test /* DDL_BEG */ as
    begin
  	    procedure packaged_selectable_sp returns(id int);
  	    function packaged_func returns int;
  	    procedure packaged_nonselected_sp;
    end /* DDL_END */
    ^

    create package body pg_test /* DDL_BEG */ as
    begin
      	procedure packaged_selectable_sp returns(id int) as
      	begin
      		for select id from test as cursor c
      		do begin
      			update test set id = -id * (select count(*) from rdb$database)
      			where current of c;
      			suspend;
      		end
      	end

      	procedure packaged_nonselected_sp as
      	begin
      		for select id from test as cursor c
      		do begin
      			update test set id = -id * (select count(*) from rdb$database)
      			where current of c;
      		end
      	end

      	function packaged_func returns int as
      	begin
      		update test set id = rand()*10000000;
      		return (select min(id) from test);
      	end
    end /* DDL_END */
    ^
    set term ;^
    commit;

    drop package pg_test /* DDL_BEG */ /* DDL_END */;
    drop function standalone_func /* DDL_BEG */ /* DDL_END */;
    drop procedure standalone_proc /* DDL_BEG */ /* DDL_END */;
    drop trigger trg_test_bi /* DDL_BEG */ /* DDL_END */;
    drop view v_test /* DDL_BEG */ /* DDL_END */;
    drop index test_x /* DDL_BEG */ /* DDL_END */;
    commit;
    drop table test /* DDL_BEG */ /* DDL_END */;
    drop domain dm_idb /* DDL_BEG */ /* DDL_END */;
    drop exception ex_invalid_value /* DDL_BEG */ /* DDL_END */;
    drop collation name_coll /* DDL_BEG */ /* DDL_END */;
    drop sequence g_common /* DDL_BEG */ /* DDL_END */;
    commit;

'''

db = db_factory()

act = python_act('db', substitutions = [('[ \t]+', ' '), (r'("SYSTEM"\.)?(")?RDB\$\S+\s+\d+(\s+\d+)*', 'RDB'), (r'RDB\$\S+\s+\d+(\s+\d+)*', 'RDB')])

expected_stdout_trace = """
SET TRANSACTION
0 records fetched

create sequence g_common /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create collation name_coll for utf8 from unicode case insensitive /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create exception ex_invalid_value 'Invalid value detected' /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create domain dm_idb as bigint /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create table test(id dm_idb not null, x dm_idb) /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

alter table test add constraint test_pk primary key(id) /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create index test_x on test(x) /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

alter index test_x inactive /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

alter index test_x active /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create view v_test as select * from test /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create trigger trg_test_bi before insert on test /* DDL_BEG */ as
end /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create procedure standalone_proc(a_id dm_idb, a_x dm_idb) /* DDL_BEG */ as
end /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create function standalone_func returns int /* DDL_BEG */ as
end /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create package pg_test /* DDL_BEG */ as
end /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

create package body pg_test /* DDL_BEG */ as
end /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

commit
0 records fetched

SET TRANSACTION
0 records fetched

drop package pg_test /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop function standalone_func /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop procedure standalone_proc /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop trigger trg_test_bi /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop view v_test /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop index test_x /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

commit
0 records fetched

SET TRANSACTION
0 records fetched

drop table test /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop domain dm_idb /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop exception ex_invalid_value /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop collation name_coll /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

drop sequence g_common /* DDL_BEG */ /* DDL_END */
0 records fetched
Table                              Natural     Index    Update    Insert    Delete   Backout     Purge   Expunge
RDB

commit
0 records fetched
"""

@pytest.mark.trace
@pytest.mark.version('>=4.0.2')
def test_1(act: Action, capsys):

    trace_cfg_items = [
        'time_threshold = 0',
        'log_errors = true',
        'log_statement_finish = true',
        'print_perf = true',
        'max_sql_length = 32768',
    ]

    with act.trace(db_events = trace_cfg_items, encoding=locale.getpreferredencoding()):
        act.isql(input = test_sql, combine_output = True)

    p_rdb_table_with_stat = re.compile( r'^("SYSTEM"\.)?(")?RDB\$\S+\s+\d+(\s+\d+)*' )

    allowed_patterns = \
    (
         '(SET TRANSACTION)'
        ,'DDL_(BEG|END)'
        ,'0 records fetched'
        ,r'\s+\d+\s+ms(,)?'
        ,r'Table\s+Natural\s+Index\s+Update\s+Insert\s+Delete\s+Backout\s+Purge\s+Expunge'
        ,p_rdb_table_with_stat.pattern # r'^("SYSTEM"\.)?(")?RDB\$\S+\s+\d+'
        ,'^commit$'
    )
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    rdb_tables_found_for_this_ddl = False
    for line in act.trace_log:
        if line.strip():
            if act.match_any(line.strip(), allowed_patterns):
                if p_rdb_table_with_stat.search(line):
                    if not rdb_tables_found_for_this_ddl:
                        print(line.strip())
                        rdb_tables_found_for_this_ddl = True
                else:
                    rdb_tables_found_for_this_ddl = False
                    print(line.strip())
            
    act.expected_stdout = expected_stdout_trace
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
