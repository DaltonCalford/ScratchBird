#coding:utf-8

"""
ID:          trigger.alter-07
TITLE:       ALTER TRIGGER - AFTER INSERT
DESCRIPTION:
  NB: phrase 'attempted update of read-only column' contains name of table and column ('TEST.ID') on 4.0.x
FBTEST:      functional.trigger.alter.07
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE test( id INTEGER NOT NULL CONSTRAINT unq UNIQUE, text VARCHAR(32));
    SET TERM ^;
    CREATE TRIGGER tg FOR test BEFORE UPDATE
    AS
    BEGIN
      new.id=1;
    END ^
    SET TERM ;^
    commit;

"""

db = db_factory(init=init_script)

test_script = """
    ALTER TRIGGER tg AFTER INSERT;
    SHOW TRIGGER tg;
"""

act = isql_act('db', test_script, substitutions=[('\\+.*', ''), ('\\=.*', ''), ('Trigger text.*', '')])

# version: 2.5.0

expected_stdout_1 = """
    Triggers on Table TEST:
    TG, Sequence: 0, Type: BEFORE UPDATE, Active
    AS
    BEGIN
      new.id=1;
    END
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"""

expected_stderr_1 = """
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column
"""

@pytest.mark.version('>=3.0,<4.0.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.expected_stderr = expected_stderr_1
    act.execute()
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)

# version: 4.0.0

expected_stdout_2 = """
    Triggers on Table TEST:
    TG, Sequence: 0, Type: BEFORE UPDATE, Active
    AS
    BEGIN
      new.id=1;
    END
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"""

expected_stderr_2 = """
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.ID
"""

@pytest.mark.skip("Covered by 'test_alter_dml_basic.py'")
@pytest.mark.version('>=4.0.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.expected_stderr = expected_stderr_2
    act.execute()
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)
