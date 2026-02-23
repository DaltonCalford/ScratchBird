#coding:utf-8

"""
ID:          issue-2608
ISSUE:       2608
TITLE:       New value of column is accessable before update
DESCRIPTION:
JIRA:        CORE-2177
FBTEST:      bugs.core_0903
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TEST (
    T1 INTEGER,
    T2 INTEGER,
    T3 INTEGER,
    T4 INTEGER
);

Insert into test (T1, T2, T3, T4) values ( 0, 0 , 0, 0);

SET TERM ^ ;

CREATE OR ALTER PROCEDURE CH_TEST (
    num integer)
as
begin

update TEST
 set
   T1= T1 + T2 + T3 + T4 + :NUM
   ,T2= T1 + T2 + T3 + T4 + :NUM
   ,T3= T1 + T2 + T3 + T4 + :NUM
   ,T4= T1 + T2 + T3 + T4 + :NUM
 ;

end^

SET TERM ; ^
"""

db = db_factory(init=init_script)

test_script = """execute procedure ch_test( 1 );
select * from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
          T1           T2           T3           T4
============ ============ ============ ============
           1            1            1            1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

