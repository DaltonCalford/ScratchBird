#coding:utf-8

"""
ID:          issue-6290
ISSUE:       6290
TITLE:       Metadata script extracted using ISQL is invalid/incorrect when table has COMPUTED BY field
DESCRIPTION:
  'collate' clause must present in DDL of computed_by field, otherwise extracted metadata script will be correct.
JIRA:        CORE-6040
FBTEST:      bugs.core_6040
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table users (
        f01 varchar(32) character set win1252 not null collate win_ptbr
        ,f02 computed by ( f01 collate win_ptbr )
    );
"""

db = db_factory(init=init_script, charset='win1252')

act = python_act('db')

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    act.isql(switches=['-x'], charset='win1252')
    meta = act.stdout
    #
    with act.db.connect() as con:
        con.execute_immediate('drop table users')
        con.commit()
    #
    act.reset()
    act.isql(switches=[], charset='win1252', input=meta)
