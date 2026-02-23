#coding:utf-8

"""
ID:          issue-3728-complex
ISSUE:       3728
TITLE:       Incorrect results when left join on subquery with constant column
DESCRIPTION:
  This test verifies PSQL issues that were accumulated in miscenaleous tickets.
NOTES:
[02.05.2021]
  created separate code for FB 4.x because of fixed #6778
  (see https://github.com/FirebirdSQL/firebird/issues/6778 for details).
  NB: final UPDATE statement (which is after DELETE one) must NOT issue exception
  in FB 4.x, so the record deletion will not undone and we can not see table data
  in expected_stdout for FB 4.x!
JIRA:        CORE-3362
FBTEST:      bugs.core_3362_complex
"""

import pytest
from firebird.qa import *

substitutions = [('[ \t]+', ' '), ('line: [\\d]+[,]{0,1} col: [\\d]+', '')]

db = db_factory()

# version: 3.0.1

test_script_1 = """
    -- see also:
    -- https://www.sql.ru/forum/1319017/obnovlenie-zapisi-po-kursoru
    -- Discussed  13.11.2019 with hvlad and dimitr (related to CORE-5794)
    recreate table test (
        id    int not null
        ,data1 int
        ,data2 int
        ,data3 int
        ,data4 int
    );
    set term ^;
    create or alter procedure sp_set_ctx(a_point varchar(20), a_data1 int, a_data2 int, a_data3 int, a_data4 int) as
    begin
        -- Store values of cursor fields in the context variable which name is passed here as 'a_point'.
        rdb$set_context(
            'USER_SESSION',
            a_point,
                      coalesce(cast( a_data1 as char(6)),'#null#')
            || ' ' || coalesce(cast( a_data2 as char(6)),'#null#')
            || ' ' || coalesce(cast( a_data3 as char(6)),'#null#')
            || ' ' || coalesce(cast( a_data4 as char(6)),'#null#')
        );
    end
    ^

    create or alter procedure sp_test1a as
    begin
        -- ::: NOTE :::
        -- Only IMPLICIT cursors are stable in 3.0+.
        -- #############
        -- Do _NOT_ try to check following statements using explicit cursor
        -- (i.e. OPEN <C>; FETCH ...; CLOSE <C>)
        for
            select t.id, t.data1, t.data2, t.data3, t.data4 from test t where t.id = 1
            as cursor c
        do begin

            execute procedure sp_set_ctx('point_0', c.data1, c.data2, c.data3, c.data4 );

            update test t set t.data1 = 100001 where current of c;
            -- make "photo" of all cursor fields:
            execute procedure sp_set_ctx('point_1', c.data1, c.data2, c.data3, c.data4 );

            -- at this point value of c.data1 remains NULL from cursor POV because
            -- "UPDATE WHERE CURRENT OF C" sees record as it was no changes at all:
            update test t set t.data2 = 100002 where current of c;
            -- make "photo" of all cursor fields:
            execute procedure sp_set_ctx('point_2', c.data1, c.data2, c.data3, c.data4 );

            -- at this point value of c.data1 and c.data2 remain NULL from cursor POV because
            -- "UPDATE WHERE CURRENT OF C" sees record as it was no changes at all:
            update test t set t.data3 = 100003 where current of c;
            -- make "photo" of all cursor fields:
            execute procedure sp_set_ctx('point_3', c.data1, c.data2, c.data3, c.data4 );

            delete from test t where current of c; -- this must prevent following UPDATE from execution
            execute procedure sp_set_ctx('point_4', c.data1, c.data2, c.data3, c.data4 );

            -- this must fail with "no current record for fetch operation":
            update test t set t.data4 = 100004 where current of c;
            execute procedure sp_set_ctx('point_5', c.data1, c.data2, c.data3, c.data4 );


        end
    end
    ^
    set term ;^

    commit;
    insert into test (id) values (1);
    commit;

    set bail off;
    set list on;

    execute procedure sp_test1a;
    select * from test;
    select mon$variable_name as ctx_name, mon$variable_value ctx_value from mon$context_variables where mon$attachment_id = current_connection;
"""

act_1 = isql_act('db', test_script_1, substitutions=substitutions)

expected_stdout_1 = """
    ID                              1
    DATA1                           <null>
    DATA2                           <null>
    DATA3                           <null>
    DATA4                           <null>

    CTX_NAME                        point_0
    CTX_VALUE                       #null# #null# #null# #null#
    CTX_NAME                        point_1
    CTX_VALUE                       100001 #null# #null# #null#
    CTX_NAME                        point_2
    CTX_VALUE                       #null# 100002 #null# #null#
    CTX_NAME                        point_3
    CTX_VALUE                       #null# #null# 100003 #null#
    CTX_NAME                        point_4
    CTX_VALUE                       #null# #null# #null# #null#
"""

expected_stderr_1 = """
    Statement failed, SQLSTATE = 22000
    no current record for fetch operation
    -At procedure 'SP_TEST1A'
"""

@pytest.mark.version('>=3.0.1,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.expected_stderr = expected_stderr_1
    act_1.execute()
    assert (act_1.clean_stderr == act_1.clean_expected_stderr and
            act_1.clean_stdout == act_1.clean_expected_stdout)

# version: 4.0

test_script_2 = """
    -- see also:
    -- Discussed  13.11.2019 with hvlad and dimitr (related to CORE-5794)
    recreate table test (
        id    int not null
        ,data1 int
        ,data2 int
        ,data3 int
        ,data4 int
    );
    set term ^;
    create or alter procedure sp_set_ctx(a_point varchar(20), a_data1 int, a_data2 int, a_data3 int, a_data4 int) as
    begin
        -- Store values of cursor fields in the context variable which name is passed here as 'a_point'.
        rdb$set_context(
            'USER_SESSION',
            a_point,
                      coalesce(cast( a_data1 as char(6)),'#null#')
            || ' ' || coalesce(cast( a_data2 as char(6)),'#null#')
            || ' ' || coalesce(cast( a_data3 as char(6)),'#null#')
            || ' ' || coalesce(cast( a_data4 as char(6)),'#null#')
        );
    end
    ^

    create or alter procedure sp_test1a as
    begin
        -- ::: NOTE :::
        -- Only IMPLICIT cursors are stable in 3.0+.
        -- #############
        -- Do _NOT_ try to check following statements using explicit cursor
        -- (i.e. OPEN <C>; FETCH ...; CLOSE <C>)
        for
            select t.id, t.data1, t.data2, t.data3, t.data4 from test t where t.id = 1
            as cursor c
        do begin

            execute procedure sp_set_ctx('point_0', c.data1, c.data2, c.data3, c.data4 );

            update test t set t.data1 = 100001 where current of c;
            -- make "photo" of all cursor fields:
            execute procedure sp_set_ctx('point_1', c.data1, c.data2, c.data3, c.data4 );

            -- at this point value of c.data1 remains NULL from cursor POV because
            -- "UPDATE WHERE CURRENT OF C" sees record as it was no changes at all:
            update test t set t.data2 = 100002 where current of c;
            -- make "photo" of all cursor fields:
            execute procedure sp_set_ctx('point_2', c.data1, c.data2, c.data3, c.data4 );

            -- at this point value of c.data1 and c.data2 remain NULL from cursor POV because
            -- "UPDATE WHERE CURRENT OF C" sees record as it was no changes at all:
            update test t set t.data3 = 100003 where current of c;
            -- make "photo" of all cursor fields:
            execute procedure sp_set_ctx('point_3', c.data1, c.data2, c.data3, c.data4 );

            delete from test t where current of c;
            execute procedure sp_set_ctx('point_4', c.data1, c.data2, c.data3, c.data4 );


            -- 02.05.2021: on FB 3.x following UPDATE statement raises exception
            -- "SQLSTATE = 22000 / no current record for fetch operation"
            -- But on FB 4.x this was fixed since build 4.0.0.2448 and no exception will be here
            -- See also: https://github.com/FirebirdSQL/firebird/issues/6778
            -- This means that deletion of record in the table TEST will not be undone
            -- and we must NOT see its data in expected_stdout section!

            update test t set t.data4 = 100004 where current of c;
            execute procedure sp_set_ctx('point_5', c.data1, c.data2, c.data3, c.data4 );

        end
    end
    ^
    set term ;^

    commit;
    insert into test (id) values (1);
    commit;

    set bail off;
    set list on;

    execute procedure sp_test1a;
    select * from test;
    select mon$variable_name as ctx_name, mon$variable_value ctx_value from mon$context_variables where mon$attachment_id = current_connection;
"""

act_2 = isql_act('db', test_script_2, substitutions=substitutions)

expected_stdout_2 = """
    CTX_NAME point_0
    CTX_VALUE #null# #null# #null# #null#
    CTX_NAME point_1
    CTX_VALUE 100001 #null# #null# #null#
    CTX_NAME point_2
    CTX_VALUE #null# 100002 #null# #null#
    CTX_NAME point_3
    CTX_VALUE #null# #null# 100003 #null#
    CTX_NAME point_4
    CTX_VALUE #null# #null# #null# #null#
    CTX_NAME point_5
    CTX_VALUE #null# #null# #null# #null#
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.execute()
    assert act_2.clean_stdout == act_2.clean_expected_stdout

