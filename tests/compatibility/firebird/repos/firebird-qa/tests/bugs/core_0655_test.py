#coding:utf-8

"""
ID:          issue-1021
ISSUE:       1021
TITLE:       Blob Type 1 compatibility with VarChar
DESCRIPTION:
JIRA:        CORE-655
FBTEST:      bugs.core_0655
"""

import pytest
from firebird.qa import *


init_script = """create table t1 (f1 BLOB SUB_TYPE 1 SEGMENT SIZE 80);
insert into t1 values ('Firebird ');
"""

db = db_factory(page_size=4096, init=init_script)

test_script = """select cast(lower(f1) as varchar(20)) lf1, cast(upper(f1) as varchar(20)) uf1, cast(trim(f1)||'2.1'  as varchar(20))tf1, cast(f1||'2.1' as varchar(20)) cf1, cast(substring(f1 from 1 for 5) as varchar(20)) sf1 from t1;
"""

act = isql_act('db', test_script)

expected_stdout = """Database:  test.fdb, User: SYSDBA
SQL>
LF1                  UF1                  TF1                  CF1                  SF1
==================== ==================== ==================== ==================== ====================
firebird             FIREBIRD             Firebird2.1          Firebird 2.1         Fireb

SQL>"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

