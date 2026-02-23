#coding:utf-8

"""
ID:          issue-1330
ISSUE:       1330
TITLE:       Bug in DSQL parameter
DESCRIPTION:
JIRA:        CORE-336
FBTEST:      bugs.core_0929
"""
from firebird.driver import DatabaseError
import pytest
from firebird.qa import *

init_script = """
    create table test (mydate date not null primary key);
    commit;
    insert into test values (current_date);
    insert into test values (current_date + 1);
    insert into test values (current_date + 2);
    insert into test values (current_date + 3);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            cur.prepare('SELECT * FROM TEST WHERE MYDATE + CAST(? AS INTEGER) >= ?')
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()

