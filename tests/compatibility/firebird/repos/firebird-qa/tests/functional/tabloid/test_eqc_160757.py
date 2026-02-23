#coding:utf-8

"""
ID:          tabloid.eqc-160757
TITLE:       Check correctness of LEFT JOIN result when left source is table with several
  -vs- single rows and right source is SP.
DESCRIPTION:
FBTEST:      functional.tabloid.eqc_160757
"""

import pytest
from firebird.qa import *

db = db_factory(page_size=4096)

test_script = """
    create or alter procedure sp_test as begin end;

    recreate table test (
        id    int primary key using index test_pk,
        val_a   int,
        val_b  int
    );

    set term ^;
    create or alter procedure sp_test(
        a_id int,
        a_val_1 int,
        a_val_2 int
            )
    returns (
        o_id int,
        o_is_equ varchar(10)
    )
    as
    begin
        o_id=a_id;
        o_is_equ = iif( a_val_1 is not distinct from a_val_2, 'Passed.', 'Failed.');
        suspend;
    end
    ^
    set term ;^
    commit;

    insert into test (id, val_a, val_b ) values (1, 0, 0);
    insert into test (id, val_a, val_b ) values (2, 1, 0);
    insert into test (id, val_a, val_b ) values (3, 0, 1);

    set list on;

    --set echo on;

    select 'test_1' as msg, t.*, p.*
    from  test t
    left join sp_test(t.id,t.val_a,t.val_b) p
    on p.o_id=t.id
    ;

    select 'test_2' as msg, t.*, p.*
    from  test t
    left join sp_test (t.id,t.val_a,t.val_b) p on p.o_id=t.id
    where t.id=2
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             test_1
    ID                              1
    VAL_A                           0
    VAL_B                           0
    O_ID                            1
    O_IS_EQU                        Passed.

    MSG                             test_1
    ID                              2
    VAL_A                           1
    VAL_B                           0
    O_ID                            2
    O_IS_EQU                        Failed.

    MSG                             test_1
    ID                              3
    VAL_A                           0
    VAL_B                           1
    O_ID                            3
    O_IS_EQU                        Failed.

    MSG                             test_2
    ID                              2
    VAL_A                           1
    VAL_B                           0
    O_ID                            2
    O_IS_EQU                        Failed.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
