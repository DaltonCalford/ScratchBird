#coding:utf-8

"""
ID:          table.alter-09
TITLE:       ALTER TABLE - DROP (with data)
DESCRIPTION:
FBTEST:      functional.table.alter.09
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE test( id INTEGER NOT NULL,
                   text VARCHAR(32));
commit;
INSERT INTO test(id,text) VALUES(0,'text 1');
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """ALTER TABLE test DROP text;
SELECT * FROM test;
"""

act = isql_act('db', test_script)

expected_stdout = """          ID
============

0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
