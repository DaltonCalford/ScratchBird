#coding:utf-8

"""
ID:          issue-1249
ISSUE:       1249
TITLE:       Sorting is allowed for blobs and arrays
DESCRIPTION:
NOTES:
  For now we test that such operations raise an exception, as we restored the legacy
  behavior until we're able to implement DISTINCT for blobs properly,
JIRA:        CORE-859
FBTEST:      bugs.core_0859
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """create table t (i integer, b blob sub_type text, a integer [5]);
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        # Use with to free the Statement immediately
        with c.prepare('select * from t order by b'):
            pass
        with pytest.raises(DatabaseError, match='.*Datatype ARRAY is not supported for sorting operation.*'):
            c.prepare('select * from t order by a')
        # Use with to free the Statement immediately
        with c.prepare('select b, count(*) from t group by b'):
            pass
        with pytest.raises(DatabaseError, match='.*Datatype ARRAY is not supported for sorting operation.*'):
            c.prepare('select a, count(*) from t group by a')
    # Passed.
