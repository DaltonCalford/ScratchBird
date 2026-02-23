#coding:utf-8

"""
ID:          issue-8123
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8123
TITLE:       Procedure manipulation can lead to wrong dependencies removal
DESCRIPTION:
    Test verifies ticket notes for standalone procedure, standalone function and package which contains SP.
    Before fix problem did exist in procedure (standalone and packaged): DROP command led to missed record
    in rdb$dependencies and if we further try to drop TABLE then error with wrong SQLSTATE raised:
        "SQLSTATE = 42S22 / invalid request BLR at offset 5 / -column N1 is not defined in table ..."
    Expected error:
        "SQLSTATE = 42000 / ... / -cannot delete ... / -there are 1 dependencies"
NOTES:
    [21.05.2024] pzotov
        Confirmed bug on 6.0.0.357-bf6c467 (regular daily snapshot, 18-may-2024).
        Checked on intermediate snapshots 6.0.0.357-f94343e, 5.0.1.1404-88bf561.
    [06.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.914; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- DO NOT set bail on!
    set list on;
    set count on;

    create table tb_test1 (n0 int, n1 integer, n2 integer computed by (n1));
    commit;

    create procedure sp_test (i1 type of column tb_test1.n2) as begin end;
    commit;

    drop procedure sp_test;
    commit;

    select
        rdb$depended_on_name
        ,rdb$field_name
        ,rdb$dependent_type
        ,rdb$depended_on_type
    from rdb$dependencies
    where rdb$depended_on_name = upper('tb_test1') and rdb$field_name = upper('n1');
    commit;

    alter table tb_test1 drop n1;
    commit;
    
    --------------------------------------------------------------------------------

    create table tb_test2 (n0 int, n1 integer, n2 integer computed by (n1));
    commit;
    set term ^;
    create function fn_test (i1 type of column tb_test2.n2) returns int as
    begin
        return 1;
    end
    ^
    set term ;^
    commit;

    drop function fn_test;
    commit;

    select
        rdb$depended_on_name
        ,rdb$field_name
        ,rdb$dependent_type
        ,rdb$depended_on_type
    from rdb$dependencies
    where rdb$depended_on_name = upper('tb_test2') and rdb$field_name = upper('n1');
    commit;

    alter table tb_test2 drop n1;
    commit;

    -----------------------------------------------------------------------------------

    create table tb_test3 (n0 int, n1 integer, n2 integer computed by (n1));
    commit;

    set term ^;
    create or alter package pg_test as
    begin
        procedure sp_worker (i1 type of column tb_test3.n2);
    end
    ^
    recreate package body pg_test as
    begin
        procedure sp_worker (i1 type of column tb_test3.n2) as
        begin

        end
    end
    ^
    set term ;^
    commit;

    drop package pg_test;
    commit;

    select
        rdb$depended_on_name
        ,rdb$field_name
        ,rdb$dependent_type
        ,rdb$depended_on_type
    from rdb$dependencies
    where rdb$depended_on_name = upper('tb_test3') and rdb$field_name = upper('n1');
    commit;

    alter table tb_test3 drop n1;
    commit;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=5.0.1')
def test_1(act: Action):

    expected_stdout_5x = """
        RDB$DEPENDED_ON_NAME TB_TEST1
        RDB$FIELD_NAME N1
        RDB$DEPENDENT_TYPE 3
        RDB$DEPENDED_ON_TYPE 0
        Records affected: 1 

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN TB_TEST1.N1
        -there are 1 dependencies


        RDB$DEPENDED_ON_NAME TB_TEST2
        RDB$FIELD_NAME N1
        RDB$DEPENDENT_TYPE 3
        RDB$DEPENDED_ON_TYPE 0
        Records affected: 1

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN TB_TEST2.N1
        -there are 1 dependencies


        RDB$DEPENDED_ON_NAME TB_TEST3
        RDB$FIELD_NAME N1
        RDB$DEPENDENT_TYPE 3
        RDB$DEPENDED_ON_TYPE 0
        Records affected: 1

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN TB_TEST3.N1
        -there are 1 dependencies
    """

    expected_stdout_6x = """
        RDB$DEPENDED_ON_NAME TB_TEST1
        RDB$FIELD_NAME N1
        RDB$DEPENDENT_TYPE 3
        RDB$DEPENDED_ON_TYPE 0
        Records affected: 1

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN "PUBLIC"."TB_TEST1"."N1"
        -there are 1 dependencies

        RDB$DEPENDED_ON_NAME TB_TEST2
        RDB$FIELD_NAME N1
        RDB$DEPENDENT_TYPE 3
        RDB$DEPENDED_ON_TYPE 0
        Records affected: 1

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN "PUBLIC"."TB_TEST2"."N1"
        -there are 1 dependencies

        RDB$DEPENDED_ON_NAME TB_TEST3
        RDB$FIELD_NAME N1
        RDB$DEPENDENT_TYPE 3
        RDB$DEPENDED_ON_TYPE 0
        Records affected: 1

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN "PUBLIC"."TB_TEST3"."N1"
        -there are 1 dependencies
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
