#coding:utf-8

"""
ID:          trigger.table.alter-11
TITLE:       ALTER TRIGGER - AS
DESCRIPTION: Test by insert data
FBTEST:      functional.trigger.table.alter_11
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE test(  id INTEGER NOT NULL CONSTRAINT unq UNIQUE,
                        text VARCHAR(32));
    SET TERM ^;
    CREATE TRIGGER tg FOR test BEFORE INSERT POSITION 1
    AS
    BEGIN
    new.text=new.text||'tg1 ';
    END ^
    SET TERM ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    SET TERM ^;
    ALTER TRIGGER tg AS
    BEGIN
    new.text='altered trigger';
    END ^

    SET TERM ;^
    INSERT INTO test VALUES(0,null);
    SELECT text FROM test;
"""

act = isql_act('db', test_script, substitutions=[('\\=.*','')])

expected_stdout = """
    TEXT
    ================================

    altered trigger
"""

@pytest.mark.skip("Covered by 'functional/trigger/alter/test_alter_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout