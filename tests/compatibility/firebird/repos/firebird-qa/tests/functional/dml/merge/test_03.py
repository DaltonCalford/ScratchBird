#coding:utf-8

"""
ID:          dml.merge-03
FBTEST:      functional.dml.merge.03
TITLE:       MERGE ... RETURNING must refer either ALIAS of the table (if it is defined) or context variables OLD and NEW
DESCRIPTION:
NOTES:
    [08.07.2025] pzotov
    Added 'FIELD_NAME' to be evaluated and substituted in expected_* on appropriate FB major version (prior/since FB 6.x).
    Checked on 6.0.0.914; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table test_a(id int primary key, x int);
    recreate table test_b(id int primary key, x int);
    commit;
    insert into test_a(id, x) values(1, 100);
    insert into test_b(id, x) values(1, 100);
    commit;

    --set echo on;

    -- [ 1 ] must FAIL with "SQLSTATE = 42S22 / ... / -Column unknown -TEST_B.ID"
    merge into test_b t
    using test_a s on s.id = t.id
    when matched then
        delete returning test_b.id, test_b.x
    ;

    rollback;

    -- [ 2 ] must PASS:
    merge into test_b t
    using test_a s on s.id = t.id
    when matched then
        delete
        returning old.id as old_id, t.x as old_t_x
    ;

    rollback;

    -- [ 3 ] must PASS:
    merge into test_b t
    using test_a s on s.id = t.id
    when matched then
        update set t.id = -s.id - 1, t.x = - s.x - 1
        returning old.id as old_id, old.x as old_x, new.id as new_id, t.x as new_x
    ;

    rollback;
"""

act = isql_act('db', test_script, substitutions=[('-At line .*', ''), ('[ \t]+', ' ')])


@pytest.mark.version('>=4.0')
def test_1(act: Action):
    FIELD_NAME = 'TEST_B.ID' if act.is_version('<6') else  '"TEST_B"."ID"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42S22
        Dynamic SQL Error
        -SQL error code = -206
        -Column unknown
        -{FIELD_NAME}
        OLD_ID 1
        OLD_T_X 100
        OLD_ID 1
        OLD_X 100
        NEW_ID -2
        NEW_X -101
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
