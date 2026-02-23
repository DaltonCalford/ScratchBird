#coding:utf-8

"""
ID:          issue-2038
ISSUE:       2038
TITLE:       Nested OR conditions may lead to incorrest results
DESCRIPTION:
JIRA:        CORE-5268
FBTEST:      bugs.core_5268
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Reproduced wrong result on WI-T4.0.0.238.
    -- (note on single difference between sp_test and sp_test2: parenthesis that enclose 'OR' expression.
    -- Checked on LI-T4.0.0.242 after fix was committed - all works fine.

    create or alter procedure sp_test(a_inp varchar(50)) as begin end;
    create or alter procedure sp_test2(a_inp varchar(50)) as begin end;
    recreate table testtab (
        feld_a varchar(50),
        feld_b varchar(10),
        feld_c char(10)
    );

    recreate table result (
        feld_a varchar(50),
        feld_b varchar(10),
        feld_c char(10)
    );

    create index testtab_idx1 on testtab (feld_b);
    create index testtab_idx2 on testtab (feld_a);
    commit;

    insert into testtab (feld_a, feld_b, feld_c) values ('aaaa', 'bb', 'cc ');
    insert into testtab (feld_a, feld_b, feld_c) values ('aaa', 'aa', 'aaa ');
    insert into testtab (feld_a, feld_b, feld_c) values ('uuu', 'uuu', 'uuu ');
    insert into testtab (feld_a, feld_b, feld_c) values ('uuu', 'uu', 'uu ');


    commit;


    -- test sqls
    -- wrong: should get at least 3 records, but gives only 1
    set term ^;
    create or alter procedure sp_test(a_inp varchar(50)) as
    begin
        delete from result;
        insert into result
        select *
        from testtab t
        where
          :a_inp='123456' -- a_inp = 'blub' for testcase
          or
          (
            upper(t.feld_b) starting with upper('u')
            or
            t.feld_a starting with 'a' /* with index */
          )
        ;
    end
    ^

    create or alter procedure sp_test2(a_inp varchar(50)) as
    begin
        delete from result;
        insert into result
        select *
        from testtab t
        where
          :a_inp='123456' -- a_inp = 'blub' for testcase
          or
          upper(t.feld_b) starting with upper('u')
          or
          t.feld_a starting with 'a' /* with index */
       ;
    end
    ^
    set term ;^
    commit;

    set list on;

    execute procedure sp_test('blub');
    select '1' as result, r.* from result r;

    execute procedure sp_test2('blub');
    select '2' as result, r.* from result r;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT                          1
    FELD_A                          aaaa
    FELD_B                          bb
    FELD_C                          cc

    RESULT                          1
    FELD_A                          aaa
    FELD_B                          aa
    FELD_C                          aaa

    RESULT                          1
    FELD_A                          uuu
    FELD_B                          uuu
    FELD_C                          uuu

    RESULT                          1
    FELD_A                          uuu
    FELD_B                          uu
    FELD_C                          uu


    RESULT                          2
    FELD_A                          aaaa
    FELD_B                          bb
    FELD_C                          cc

    RESULT                          2
    FELD_A                          aaa
    FELD_B                          aa
    FELD_C                          aaa

    RESULT                          2
    FELD_A                          uuu
    FELD_B                          uuu
    FELD_C                          uuu

    RESULT                          2
    FELD_A                          uuu
    FELD_B                          uu
    FELD_C                          uu
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

