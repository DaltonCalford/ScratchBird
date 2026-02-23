#coding:utf-8

"""
ID:          issue-7466
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7466
TITLE:       Add COMPILE trace events - ability to see execution plan of every PSQL statement.
DESCRIPTION:
    Separate test for check appearance of execution plans of every statement inside compiled PSQL unit, see:
    https://github.com/FirebirdSQL/firebird/pull/7466#issue-1564439735
    Test creates stored procedure with several satements in it that use tables. Statements have no any sense.
    We check here that compiling of this SP leads to appearing in the trace execution plans for every statement.
NOTES:
    [18-aug-2023] pzotov
        1. It must be noted that the term 'COMPILE' means parsing of BLR code into an execution tree, i.e. this action
           occurs when unit code is loaded into metadata cache. 
        2. All subqueries like EXISTS() or IN() (and their "not" form) will be displayed in the trace as "separate" block
           followed by block with "Select expression" or "Cursor". This seems not readable but currently it is so.
        3. Number of lines and columns for 'Subquery' will be shown only when this subquery is used as part PSQL statement
           (i.e. not as part of SQL query) -- see 'decode()' below:
           =======
            Sub-query (line 37, column 26)
                -> Singularity Check
            ...
           =======
        4. Plans, of course, can be changed in the future, so this test must be adjusted if this will occur.
        
        Thanks to dimitr for explanations.
        Discussed with dimitr, letters 18.08.2023.

        Checked on 5.0.0.1164
    [08-sep-2023] pzotov
        1. Changed plan output: it is desirable to show indentations but they are 'swallowed' when act.clean_stdout is displayed.
           Because of that, explained plan lines are 'padded' with dot character to their original length.
        2. Adjusted execution plan for one of queries to actual: one need to replace "Range Scan" with "List Scan" if we have
           subquery with IN-list which refers to some columns from outer query.
           See: https://github.com/FirebirdSQL/firebird/commit/5df6668c7bf5a4b27e15f687f8c6cc40e260ced8
           (Allow computable but non-invariant lists to be used for index lookup)
        Checked on 5.0.0.1200
    [19-dec-2023] pzotov
        Removed 'rand()' in order to have predictable values in table column. Use mod() instead.
        Unstable outcomes started since 6.0.0.180 (18.12.2023).
        It seems that following commits caused this:
            https://github.com/FirebirdSQL/firebird/commit/ae427762d5a3e740b69c7239acb9e2383bc9ca83 // 5.x
            https://github.com/FirebirdSQL/firebird/commit/f647dfd757de3c4065ef2b875c95d19311bb9691 // 6.x
    [04-feb-2025]
        Adjusted execution plan for EXISTS() part of recursive query: "List Scan" was replaced with "Range Scan" for 
        "and m0a.x in (dx.y, dx.z)". This change caused by commit 0cc77c89 ("Fix #8109: Plan/Performance regression ...")
    [05.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.909; 5.0.3.1668.
"""
import locale
import re
import pytest
from firebird.qa import *

db = db_factory()

substitutions = [
                    (' \\(line \\d+, column \\d+\\)', '(line, column)' )
                   ,( '\\d+\\s+ms.*', '0 ms')
                ]

act = python_act('db', substitutions = substitutions)

trace = ['log_initfini = false',
         'log_errors = true',
         'time_threshold = 0',
         'log_procedure_compile = true',
         'print_plan = true',
         'explain_plan = true',
         ]

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

@pytest.mark.trace
@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    test_script = f"""
        recreate table tdetl(id int);
        recreate table tmain(id int primary key using index tmain_pk, x int);
        recreate table tdetl(id int primary key using index tdetl_pk, pid int references tmain using index tdetl_fk, y int, z int);

        insert into tmain(id,x)
        select i, -100 + mod(i,200) from (select row_number()over() i from rdb$types rows 200);

        insert into tdetl(id, pid, y,z)
        select i, 1+mod(i,10), mod(i,30), mod(i,70) from (select row_number()over() i from rdb$types,rdb$types rows 1000);
        commit;

        create index tmain_x on tmain(x);
        create index tdetl_y on tdetl(y);
        create index tdetl_z on tdetl(z);

        set statistics index tdetl_fk;
        commit;

        set term ^;
        create or alter procedure sp_test returns(id int, c int)
        as
            declare k cursor for (
                select m4.id, d4.y, d4.z
                from tmain m4
                cross join lateral (
                    select y, z
                    from tdetl dx
                    where
                        dx.pid = m4.id
                        and m4.x between dx.y and dx.z
                ) d4
                where exists(select count(*) from tdetl dy group by dy.pid having count(*) > 2)
            );
        begin

            if ( not exists(select * from tmain m0 where m0.x > 0) ) then
                exception;

            ----------------------------

            with recursive
            r as (
               select 0 as i, d0.id, d0.pid
               from tdetl d0
               where d0.pid is null
               UNION ALL
               select r.i + 1, dx.id, dx.pid
               from tdetl dx
               join r on dx.pid = r.id
               where exists(
                   select * from tmain m0a
                   where
                       m0a.id <> dx.pid
                       -- ::: NB ::: Rest part is IN-list with computable but non-invariant elements.
                       -- Execution plan for this kind was changed 07-sep-2023, see:
                       -- https://github.com/FirebirdSQL/firebird/commit/5df6668c7bf5a4b27e15f687f8c6cc40e260ced8
                       -- (Allow computable but non-invariant lists to be used for index lookup)
                       -- See also: tests/functional/tabloid/test_e260ced8.py
                       -- ########################################################################################
                       -- NOTE!
                       -- Before 03-feb-2025 "Index "TMAIN_X" List Scan (full match)" was here, but since 0cc77c89
                       -- bitmap_Or for two scans will be performed ("Index "TMAIN_X" Range Scan (full match)").
                       and m0a.x in (dx.y, dx.z) -- ### ATTENTION ###
               )
            )
            select count(*) from r where r.i > 2
            into c;

            ----------------------------
            c = decode( (select mod(count(*), 3) from tmain m1a)
                        ,0, (select min(x) from tmain m1b)
                        ,1, (select min(d1b.pid) from tdetl d1b)
                        ,2, (select max(d1c.pid) from tdetl d1c)
                );

            ----------------------------
            for
                select m2.id, count(*)
                from tmain m2
                join tdetl d using(id)
                where m2.x > 0
                group by 1
                into id, c
            do
                suspend;
            ----------------------------
            for
                select m3.id, 0
                from tmain m3
                where
                    m3.x > 0 and
                    not exists(select * from tdetl d where d.pid = m3.id)
                into id, c
            do
                suspend;
        end
        ^
        set term ;^
        commit;
    """

    with act.trace(db_events=trace, encoding = locale.getpreferredencoding(), encoding_errors='utf8'):
        act.isql(switches = ['-q'], input = test_script, combine_output = True, io_enc = locale.getpreferredencoding())

    # Parse trace log:
    start_show = 0
    for line in act.trace_log:
        if line.startswith("^^^"):
            start_show = 1
            continue
        if start_show and line.rstrip():
            print( replace_leading(line,'.') )

    expected_stdout_5x = """
        Sub-query (invariant)
        ....-> Filter
        ........-> Aggregate
        ............-> Table "TDETL" as "K DY" Access By ID
        ................-> Index "TDETL_FK" Full Scan
        Cursor "K"(line, column)
        ....-> Filter (preliminary)
        ........-> Nested Loop Join (inner)
        ............-> Table "TMAIN" as "K M4" Full Scan
        ............-> Filter
        ................-> Table "TDETL" as "K D4 DX" Access By ID
        ....................-> Bitmap And
        ........................-> Bitmap And
        ............................-> Bitmap
        ................................-> Index "TDETL_Z" Range Scan (lower bound: 1/1)
        ............................-> Bitmap
        ................................-> Index "TDETL_Y" Range Scan (upper bound: 1/1)
        ........................-> Bitmap
        ............................-> Index "TDETL_FK" Range Scan (full match)
        Sub-query
        ....-> Filter
        ........-> Table "TMAIN" as "M0" Access By ID
        ............-> Bitmap
        ................-> Index "TMAIN_X" Range Scan (lower bound: 1/1)
        Sub-query
        ....-> Filter
        ........-> Table "TMAIN" as "R M0A" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "TMAIN_X" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "TMAIN_X" Range Scan (full match)
        Select Expression(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Filter
        ................-> Recursion
        ....................-> Filter
        ........................-> Table "TDETL" as "R D0" Access By ID
        ............................-> Bitmap
        ................................-> Index "TDETL_FK" Range Scan (full match)
        ....................-> Filter
        ........................-> Table "TDETL" as "R DX" Access By ID
        ............................-> Bitmap
        ................................-> Index "TDETL_FK" Range Scan (full match)
        Sub-query(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table "TMAIN" as "M1A" Full Scan
        Sub-query(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table "TMAIN" as "M1B" Access By ID
        ................-> Index "TMAIN_X" Full Scan
        Sub-query(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table "TDETL" as "D1B" Access By ID
        ................-> Index "TDETL_FK" Full Scan
        Sub-query(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table "TDETL" as "D1C" Full Scan
        Select Expression(line, column)
        ....-> Aggregate
        ........-> Nested Loop Join (inner)
        ............-> Filter
        ................-> Table "TMAIN" as "M2" Access By ID
        ....................-> Index "TMAIN_PK" Full Scan
        ........................-> Bitmap
        ............................-> Index "TMAIN_X" Range Scan (lower bound: 1/1)
        ............-> Filter
        ................-> Table "TDETL" as "D" Access By ID
        ....................-> Bitmap
        ........................-> Index "TDETL_PK" Unique Scan
        Sub-query
        ....-> Filter
        ........-> Table "TDETL" as "D" Access By ID
        ............-> Bitmap
        ................-> Index "TDETL_FK" Range Scan (full match)
        Select Expression(line, column)
        ....-> Filter
        ........-> Table "TMAIN" as "M3" Access By ID
        ............-> Bitmap
        ................-> Index "TMAIN_X" Range Scan (lower bound: 1/1)
        ......0 ms
    """

    expected_stdout_6x = """
        Sub-query (invariant)
        ....-> Filter
        ........-> Aggregate
        ............-> Table "PUBLIC"."TDETL" as "K" "DY" Access By ID
        ................-> Index "PUBLIC"."TDETL_FK" Full Scan
        Cursor "K"(line, column)
        ....-> Filter (preliminary)
        ........-> Nested Loop Join (inner)
        ............-> Table "PUBLIC"."TMAIN" as "K" "M4" Full Scan
        ............-> Filter
        ................-> Table "PUBLIC"."TDETL" as "K" "D4" "DX" Access By ID
        ....................-> Bitmap And
        ........................-> Bitmap And
        ............................-> Bitmap
        ................................-> Index "PUBLIC"."TDETL_Z" Range Scan (lower bound: 1/1)
        ............................-> Bitmap
        ................................-> Index "PUBLIC"."TDETL_Y" Range Scan (upper bound: 1/1)
        ........................-> Bitmap
        ............................-> Index "PUBLIC"."TDETL_FK" Range Scan (full match)
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."TMAIN" as "M0" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TMAIN_X" Range Scan (lower bound: 1/1)
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."TMAIN" as "R" "M0A" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TMAIN_X" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TMAIN_X" Range Scan (full match)
        Select Expression(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Filter
        ................-> Recursion
        ....................-> Filter
        ........................-> Table "PUBLIC"."TDETL" as "R" "D0" Access By ID
        ............................-> Bitmap
        ................................-> Index "PUBLIC"."TDETL_FK" Range Scan (full match)
        ....................-> Filter
        ........................-> Table "PUBLIC"."TDETL" as "R" "DX" Access By ID
        ............................-> Bitmap
        ................................-> Index "PUBLIC"."TDETL_FK" Range Scan (full match)
        Sub-query(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table "PUBLIC"."TMAIN" as "M1A" Full Scan
        Sub-query(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table "PUBLIC"."TMAIN" as "M1B" Access By ID
        ................-> Index "PUBLIC"."TMAIN_X" Full Scan
        Sub-query(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table "PUBLIC"."TDETL" as "D1B" Access By ID
        ................-> Index "PUBLIC"."TDETL_FK" Full Scan
        Sub-query(line, column)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table "PUBLIC"."TDETL" as "D1C" Full Scan
        Select Expression(line, column)
        ....-> Aggregate
        ........-> Nested Loop Join (inner)
        ............-> Filter
        ................-> Table "PUBLIC"."TMAIN" as "M2" Access By ID
        ....................-> Index "PUBLIC"."TMAIN_PK" Full Scan
        ........................-> Bitmap
        ............................-> Index "PUBLIC"."TMAIN_X" Range Scan (lower bound: 1/1)
        ............-> Filter
        ................-> Table "PUBLIC"."TDETL" as "D" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."TDETL_PK" Unique Scan
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."TDETL" as "D" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TDETL_FK" Range Scan (full match)
        Select Expression(line, column)
        ....-> Filter
        ........-> Table "PUBLIC"."TMAIN" as "M3" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."TMAIN_X" Range Scan (lower bound: 1/1)
        ......0 ms
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
