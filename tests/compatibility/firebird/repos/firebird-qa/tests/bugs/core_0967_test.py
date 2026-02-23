#coding:utf-8

"""
ID:          issue-1370
ISSUE:       1370
TITLE:       SQL with incorrect characters (outside the ASCII range) may be processed wrongly
DESCRIPTION:
JIRA:        CORE-967
FBTEST:      bugs.core_0967
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """create table t (i integer);
insert into t values (0);
commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.intl
@pytest.mark.version('>=2.1')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        with pytest.raises(DatabaseError, match="Dynamic SQL Error\n-SQL error code = -104\n-Token unknown - line 1, column 17\n.*") as excinfo:
            c.execute('update t set i=1'+chr(238)+' where 1=0')



