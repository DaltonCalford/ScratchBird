#coding:utf-8

"""
ID:          dml.update-or-insert-02
FBTEST:      functional.dml.update_or_insert.02
TITLE:       UPDATE OR INSERT
DESCRIPTION: With RETURNING clause
"""

import pytest
from firebird.qa import *

db = db_factory(init="CREATE TABLE TMPTEST( id INTEGER , name VARCHAR(20) , PRIMARY KEY(id));")

test_script = """
SET TERM ^ ;
EXECUTE BLOCK
RETURNS (V integer)
AS
  BEGIN
	UPDATE OR INSERT INTO TMPTEST(id, name) VALUES (1,'ivan' )
	RETURNING id INTO :V;
	SUSPEND;

  END^
SET TERM ; ^
"""

act = isql_act('db', test_script)

expected_stdout = """
           V
============
           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
