#coding:utf-8

"""
ID:          issue-2149
ISSUE:       2149
TITLE:       Unable to restore a database with inactive indices if any SP/trigger contains an explicit plan
DESCRIPTION:
    We create table and indices for it.
    Then we create trigger for this table, view, procedure, function and package - and all of them have DDL
    which explicitly uses 'TEST ORDER <index>' in execution plan.
    Such database then backed up and restored with command switch '-i(nactive)'.
    Restore is logged and we check that this log does not contain 'gbak:error' message (and eventually completes OK).
    Restored database must contain all created DB objects, i.e. we must have ability to explicitly specify them in SQL.
    Table trigger (containing explicit PLAN clause in its DDL) also must exist and remain active.
    Before this bug was fixed:
    1) log of restore contained:
       gbak: ERROR:Error while parsing function FN_WORKER's BLR
       gbak: ERROR: index TEST_X cannot be used in the specified plan
    2) restored database had NO indices that were explicitly specified in any DDL and any attempt to use appropriate
       DB object failed with SQLSTATE = 42S02/39000/42000 ('Table/Procedure/Function} unknown').
JIRA:        CORE-1725
NOTES:
    [28.10.2024] pzotov
        1. Test was fully re-implemented.
           We do NOT extract metadata before and after restore (in order to compare it):
           in FB 6.x 'gbak -i' leads to 'create INACTIVE index ...' statements in generated SQL
           (see https://github.com/FirebirdSQL/firebird/issues/8091 - "Ability to create an inactive index").
           Comparison of metadata that was before and after restore has no much sense.
           Rather, we have to check SQL/DML that attempt to use DB object which DDL contain
           explicitly specified execution plan.
           All such actions must raise error related to invalid BLR, but *not* error about missing DB object.
           BTW: it looks strange that such messages contain "-there is no index TEST_X for table TEST".
           Such index definitely DOES exist but it is inactive.
        2. Bug existed up to 17-jan-2019.
           It was fixed by commits related to other issues, namely:
               3.x: a74130019af89012cc1e04ba18bbc9c4a69e1a5d // 17.01.2019
               4.x: fea7c61d9741dc142fa020bf3aa93af7e52e2002 // 17.01.2019
               5.x: fea7c61d9741dc142fa020bf3aa93af7e52e2002 // 18.01.2019
               ("Attempted to fix CORE-2440, CORE-5118 and CORE-5900 together (expression indices contain NULL keys after restore).")
        Checked on:
        6.0.0.511-c4bc943; 5.0.2.1547-1e08f5e; 4.0.0.1384-fea7c61 (17-jan-2019, just after fix); 3.0.13.33793-3e62713
    [30.06.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
    [13.02.2026] pzotov
        Adjusted output for 5.x and 6.x to actual (changed after #14c6de6e "Improvement #8895...").
        Confirmed by Vlad, 13.02.2026 1006.
        Checked on 6.0.0.1428 5.0.4.1757.
"""

import locale
import re
from collections import defaultdict
from difflib import unified_diff
from pathlib import Path

import pytest
from firebird.qa import *

init_script = """
    set bail on;

    create or alter procedure sp_init as begin end;
    create or alter procedure sp_main as begin end;
    create or alter procedure sp_worker as begin end;

    create or alter function fn_init returns int as begin end;
    create or alter function fn_main returns int as begin end;
    create or alter function fn_worker returns int as begin end;

    create table test(id int primary key, x int, y int);
    create index test_x on test(x);
    create descending index test_y on test(y);
    commit;

    insert into test(id, x, y) select row_number()over(), rand()*5, rand()*100 from rdb$types;
    commit;

    create or alter view v_init as
        select count(*) as cnt from test group by x
        rows 1
    ;

    create or alter view v_worker as
        select count(*) as cnt
        from test
        group by y
        plan (TEST ORDER TEST_Y)
        union all
        select cnt from v_init
    ;
    commit;


    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION','INITIAL_DDL', '1');
    end
    ^

    create or alter procedure sp_init as
        declare c int;
    begin
        select count(*) from test group by x
        rows 1
        into c
        ;
    end
    ^

    create or alter procedure sp_main as
    begin
        execute procedure sp_worker;
    end
    ^

    create or alter procedure sp_worker as
        declare c int;
    begin
        select sum(cnt)
        from (
            select count(*) as cnt
            from test group by x
            plan (TEST ORDER TEST_X)
            union all
            select cnt from v_worker
        )
        into c
        ;
    end
    ^
    create or alter function fn_init returns int as
    begin
        return ( select count(*) from test );
    end
    ^
    create or alter function fn_worker returns int as
    begin
        return (
            select sum(cnt)
            from (
                select count(*) as cnt
                from test group by x
                plan (TEST ORDER TEST_X)
                union all
                select cnt from v_worker
            )
        );
    end
    ^
    create or alter function fn_main returns int as
    begin
        return fn_worker();
    end
    ^

    create or alter package pg_test as
    begin
        function pg_fn_worker returns int;
        procedure pg_sp_worker;
    end
    ^
    recreate package body pg_test as
    begin
        function pg_fn_worker returns int as
        begin
            return (
                select sum(cnt)
                from (
                    select count(*) as cnt
                    from test group by x
                    plan (TEST ORDER TEST_X)
                    union all
                    select cnt from v_worker
                )
            );
        end

        procedure pg_sp_worker as
            declare c int;
        begin
            select sum(cnt)
            from (
                select count(*) as cnt
                from test group by x
                plan (TEST ORDER TEST_X)
                union all
                select cnt from v_worker
            )
            into c
            ;
        end

    end
    ^
    create or alter trigger test_bi for test active before insert position 0 as
        declare c int;
    begin
        if ( rdb$get_context('USER_SESSION','INITIAL_DDL') is null ) then
        begin
            select sum(cnt)
            from (
                select count(*) as cnt
                from test group by x
                plan (TEST ORDER TEST_X)
                union all
                select cnt from v_worker
            )
            into c;
        end
    end
    ^
    set term ;^
    commit;
"""

substitutions = [('[ \t]+', ' '), ('(-)?invalid request BLR at offset \\d+', 'invalid request BLR at offset')]

db = db_factory(init = init_script)
act = python_act('db', substitutions = substitutions)

tmp_fbk= temp_file('tmp_core_1725.fbk')
tmp_fdb = temp_file('tmp_core_1725.fdb')

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, capsys):

    outcomes_map = defaultdict(str)

    act.gbak(switches=['-b', act.db.dsn, str(tmp_fbk)])

    # restore _WITHOUT_ building indices:
    act.gbak(switches=['-rep', '-i', '-v', str(tmp_fbk), str(tmp_fdb) ], combine_output = True, io_enc = locale.getpreferredencoding())

    watching_patterns = [re.compile(x, re.IGNORECASE) for x in (r'gbak:\s?ERROR(:)?\s?', r'gbak:finis.*\s+going home', r'gbak:adjust.*\s+flags')]

    for line in act.clean_stdout.splitlines():
        for p in watching_patterns:
            if p.search(line):
                outcomes_map['restore_log'] += line+'\n'
    act.reset()

    ###########################################################################

    check_metadata = """
        set list on;
        set count on;

        select ri.rdb$index_name, ri.rdb$index_inactive from rdb$indices ri where ri.rdb$relation_name = upper('test') and ri.rdb$index_name starting with upper('test');

        select p.rdb$package_name, p.rdb$procedure_name as sp_name, p.rdb$valid_blr as sp_valid_blr
        from rdb$procedures p
        where p.rdb$system_flag is distinct from 1
        order by p.rdb$package_name, p.rdb$procedure_name
        ;

        select f.rdb$package_name, f.rdb$function_name as fn_name, f.rdb$valid_blr as fn_valid_blr
        from rdb$functions f
        where f.rdb$system_flag is distinct from 1
        order by f.rdb$package_name, f.rdb$function_name
        ;

        select rt.rdb$trigger_name, rt.rdb$trigger_inactive, rt.rdb$valid_blr as tg_valid_blr
        from rdb$triggers rt
        where
            rt.rdb$system_flag is distinct from 1 and
            rt.rdb$relation_name = upper('test')
        ;

        set count off;
    """
    act.isql(switches=['-nod', '-q', str(tmp_fdb)], input = check_metadata, credentials = True, charset = 'utf8', connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())
    for line in act.clean_stdout.splitlines():
        outcomes_map['check_metadata'] += line+'\n'
    act.reset()

    ###########################################################################

    check_avail_db_objects = """
        set list on;
        set echo on;
        select * from v_worker;

        execute procedure sp_main;

        select fn_main() from rdb$database;

        execute procedure pg_test.pg_sp_worker;

        select pg_test.pg_fn_worker() from rdb$database;

        insert into test(id, x, y) values(-1, -1, -1) returning id, x, y;
    """
    act.isql(switches=['-nod', '-q', str(tmp_fdb)], input = check_avail_db_objects, credentials = True, charset = 'utf8', connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())

    for line in act.clean_stdout.splitlines():
        outcomes_map['check_avail_db_objects'] += line+'\n'
    act.reset()

    for k,v in outcomes_map.items():
        print(k)
        for p in v.splitlines():
            print(p)
        print('')

    ###########################################################################

    expected_stdout_4x = """
        restore_log
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
        check_metadata
        RDB$INDEX_NAME TEST_X
        RDB$INDEX_INACTIVE 1
        RDB$INDEX_NAME TEST_Y
        RDB$INDEX_INACTIVE 1
        Records affected: 2
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_INIT
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_MAIN
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_WORKER
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME PG_TEST
        SP_NAME PG_SP_WORKER
        SP_VALID_BLR 1
        Records affected: 4
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_INIT
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_MAIN
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_WORKER
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME PG_TEST
        FN_NAME PG_FN_WORKER
        FN_VALID_BLR 1
        Records affected: 4
        RDB$TRIGGER_NAME TEST_BI
        RDB$TRIGGER_INACTIVE 0
        TG_VALID_BLR 1
        Records affected: 1
        check_avail_db_objects
        select * from v_worker;
        Statement failed, SQLSTATE = 42000
        invalid request BLR at offset
        -there is no index TEST_Y for table TEST
        execute procedure sp_main;
        Statement failed, SQLSTATE = 2F000
        Error while parsing procedure SP_MAIN's BLR
        -Error while parsing procedure SP_WORKER's BLR
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
        select fn_main() from rdb$database;
        Statement failed, SQLSTATE = 2F000
        Error while parsing function FN_MAIN's BLR
        -Error while parsing function FN_WORKER's BLR
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
        execute procedure pg_test.pg_sp_worker;
        Statement failed, SQLSTATE = 2F000
        Error while parsing procedure PG_TEST.PG_SP_WORKER's BLR
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
        select pg_test.pg_fn_worker() from rdb$database;
        Statement failed, SQLSTATE = 2F000
        Error while parsing function PG_TEST.PG_FN_WORKER's BLR
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
        insert into test(id, x, y) values(-1, -1, -1) returning id, x, y;
        Statement failed, SQLSTATE = 42000
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
    """

    expected_stdout_5x = """
        restore_log
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
        check_metadata
        RDB$INDEX_NAME TEST_X
        RDB$INDEX_INACTIVE 1
        RDB$INDEX_NAME TEST_Y
        RDB$INDEX_INACTIVE 1
        Records affected: 2
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_INIT
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_MAIN
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_WORKER
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME PG_TEST
        SP_NAME PG_SP_WORKER
        SP_VALID_BLR 1
        Records affected: 4
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_INIT
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_MAIN
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_WORKER
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME PG_TEST
        FN_NAME PG_FN_WORKER
        FN_VALID_BLR 1
        Records affected: 4
        RDB$TRIGGER_NAME TEST_BI
        RDB$TRIGGER_INACTIVE 0
        TG_VALID_BLR 1
        Records affected: 1
        check_avail_db_objects
        select * from v_worker;
        Statement failed, SQLSTATE = 42000
        invalid request BLR at offset
        -there is no index TEST_Y for table TEST
        execute procedure sp_main;
        Statement failed, SQLSTATE = 2F000
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
        -Error while parsing procedure SP_WORKER's BLR
        -Error while parsing procedure SP_MAIN's BLR
        select fn_main() from rdb$database;
        Statement failed, SQLSTATE = 2F000
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
        -Error while parsing function FN_WORKER's BLR
        -Error while parsing function FN_MAIN's BLR
        execute procedure pg_test.pg_sp_worker;
        Statement failed, SQLSTATE = 2F000
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
        -Error while parsing procedure PG_TEST.PG_SP_WORKER's BLR
        select pg_test.pg_fn_worker() from rdb$database;
        Statement failed, SQLSTATE = 2F000
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
        -Error while parsing function PG_TEST.PG_FN_WORKER's BLR
        insert into test(id, x, y) values(-1, -1, -1) returning id, x, y;
        Statement failed, SQLSTATE = 42000
        invalid request BLR at offset
        -there is no index TEST_X for table TEST
    """

    expected_stdout_6x = """
        restore_log
        gbak:finishing, closing, and going home
        gbak:adjusting the ONLINE and FORCED WRITES flags
        check_metadata
        RDB$INDEX_NAME TEST_X
        RDB$INDEX_INACTIVE 1
        RDB$INDEX_NAME TEST_Y
        RDB$INDEX_INACTIVE 1
        Records affected: 2
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_INIT
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_MAIN
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        SP_NAME SP_WORKER
        SP_VALID_BLR 1
        RDB$PACKAGE_NAME PG_TEST
        SP_NAME PG_SP_WORKER
        SP_VALID_BLR 1
        Records affected: 4
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_INIT
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_MAIN
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME <null>
        FN_NAME FN_WORKER
        FN_VALID_BLR 1
        RDB$PACKAGE_NAME PG_TEST
        FN_NAME PG_FN_WORKER
        FN_VALID_BLR 1
        Records affected: 4
        RDB$TRIGGER_NAME TEST_BI
        RDB$TRIGGER_INACTIVE 0
        TG_VALID_BLR 1
        Records affected: 1
        check_avail_db_objects
        select * from v_worker;
        Statement failed, SQLSTATE = 42000
        invalid request BLR at offset
        -there is no index "PUBLIC"."TEST_Y" for table "PUBLIC"."TEST"
        execute procedure sp_main;
        Statement failed, SQLSTATE = 2F000
        invalid request BLR at offset
        -there is no index "PUBLIC"."TEST_X" for table "PUBLIC"."TEST"
        -Error while parsing procedure "PUBLIC"."SP_WORKER"'s BLR
        -Error while parsing procedure "PUBLIC"."SP_MAIN"'s BLR
        select fn_main() from rdb$database;
        Statement failed, SQLSTATE = 2F000
        invalid request BLR at offset
        -there is no index "PUBLIC"."TEST_X" for table "PUBLIC"."TEST"
        -Error while parsing function "PUBLIC"."FN_WORKER"'s BLR
        -Error while parsing function "PUBLIC"."FN_MAIN"'s BLR
        execute procedure pg_test.pg_sp_worker;
        Statement failed, SQLSTATE = 2F000
        invalid request BLR at offset
        -there is no index "PUBLIC"."TEST_X" for table "PUBLIC"."TEST"
        -Error while parsing procedure "PUBLIC"."PG_TEST"."PG_SP_WORKER"'s BLR
        select pg_test.pg_fn_worker() from rdb$database;
        Statement failed, SQLSTATE = 2F000
        invalid request BLR at offset
        -there is no index "PUBLIC"."TEST_X" for table "PUBLIC"."TEST"
        -Error while parsing function "PUBLIC"."PG_TEST"."PG_FN_WORKER"'s BLR
        insert into test(id, x, y) values(-1, -1, -1) returning id, x, y;
        Statement failed, SQLSTATE = 42000
        invalid request BLR at offset
        -there is no index "PUBLIC"."TEST_X" for table "PUBLIC"."TEST"
    """

    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
