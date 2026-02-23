#coding:utf-8

"""
ID:          trigger.alter-09
TITLE:       ALTER TRIGGER - POSITION
DESCRIPTION:
  Test by checking trigger seqeunce
FBTEST:      functional.trigger.alter.09
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id INTEGER NOT NULL CONSTRAINT unq UNIQUE,
                   text VARCHAR(32));
SET TERM ^;
CREATE TRIGGER tg1 FOR test BEFORE INSERT POSITION 1
AS
BEGIN
  new.text=new.text||'tg1 ';
END ^

CREATE TRIGGER tg2 FOR test BEFORE INSERT POSITION 10
AS
BEGIN
  new.text=new.text||'tg2 ';
END ^
SET TERM ;^
commit;
"""

db = db_factory(init=init_script)

test_script = """ALTER TRIGGER tg2 POSITION 0;
INSERT INTO test VALUES(0,'');
COMMIT;
SELECT text FROM test;
"""

act = isql_act('db', test_script)

expected_stdout = """TEXT
================================

tg2 tg1
"""

@pytest.mark.skip("Covered by 'test_alter_dml_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
