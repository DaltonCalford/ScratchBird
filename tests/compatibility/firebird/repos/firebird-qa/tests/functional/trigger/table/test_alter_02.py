#coding:utf-8

"""
ID:          trigger.table.alter-02
TITLE:       ALTER TRIGGER - INACTIVE
DESCRIPTION:
FBTEST:      functional.trigger.table.alter_02
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE test(  id INTEGER NOT NULL CONSTRAINT unq UNIQUE,
                        text VARCHAR(32));
    commit;
    SET TERM ^;
    CREATE TRIGGER tg FOR test BEFORE INSERT
    AS
    BEGIN
    new.id=1;
    END ^
    SET TERM ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    ALTER TRIGGER tg INACTIVE;
    SHOW TRIGGER tg;
"""

act = isql_act('db', test_script, substitutions=[('\\+.*',''),('\\=.*',''),('Trigger text.*','')])

expected_stdout = """
    Triggers on Table TEST:
    TG, Sequence: 0, Type: BEFORE INSERT, Inactive
    AS
    BEGIN
    new.id=1;
    END
"""

@pytest.mark.skip("Covered by 'functional/trigger/alter/test_alter_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
