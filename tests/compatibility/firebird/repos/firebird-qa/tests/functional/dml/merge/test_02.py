#coding:utf-8

"""
ID:          dml.merge-02
FBTEST:      functional.dml.merge.02
TITLE:       MERGE statement can have only one RETURNING which must be after all WHEN sub-statements
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table ta(id int primary key, x int, y int);
    recreate table tb(id int primary key, x int, y int);
    commit;

    insert into ta(id, x, y) values(1, 10, 100);
    insert into tb(id, x, y) values(1, 10, 100);
    commit;

    -- [ 1 ] must PASS:
    merge into tb t
    using ta s on s.id = t.id
    when NOT matched then insert values(-s.id, -s.x, -s.y) ------------------------- (a)
    when matched then
        delete
        returning old.id as deleted_id, old.x as deleted_x, old.y as deleted_y ----- (b)
    ;
    rollback;

    -- [ 2 ] must FAIL with
    -- Statement failed, SQLSTATE = 42000 / ... / -Token unknown / -when

    merge into tb t
    using ta s on s.id = t.id
    when matched then delete returning old.id, old.x, old.y ----------------------- (b)
    when NOT matched then insert values(-s.id, -s.x, -s.y) ------------------------ (a)
    ;
"""

act = isql_act('db', test_script, substitutions=[('-Token unknown .*', ''), ('[ \t]+', ' ')])

expected_stdout = """
    DELETED_ID                      1
    DELETED_X                       10
    DELETED_Y                       100
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 4, column 5
    -when
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
