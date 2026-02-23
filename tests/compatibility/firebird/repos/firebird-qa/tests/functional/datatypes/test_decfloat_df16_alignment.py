#coding:utf-8

"""
ID:          decfloat.df16-alignment
ISSUE:       5803
JIRA:        CORE-5535
TITLE:       Check proper alignment of decfloat(16) value if it is shown in ISQL when SET LIST ON
DESCRIPTION:
  See  doc/sql.extensions/README.data_types

  Test is based on letter to Alex, 02.05.2017, 9:38:
  For ISQL 'SET LIST ON' there was auxiliary ("wrong") space character between column name
  for decfloat(16) and its value comparing with decfloat(34).
FBTEST:      functional.datatypes.decfloat_df16_alignment
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table test(
         a varchar(1) default '|'
        ,x34 decfloat(34) default -9.999999999999999999999999999999999E6144
        ,y16 decfloat(16) default -9.999999999999999E384
        ,u34 decfloat(34) default 9.999999999999999999999999999999999E6144
        ,v16 decfloat(16) default 9.999999999999999E384
        ,w varchar(1) default '|'
    );
    commit;
    insert into test default values returning a,x34,y16,u34,v16,w;
"""

act = isql_act('db', test_script)

expected_stdout = """
    A                               |
    X34                             -9.999999999999999999999999999999999E+6144
    Y16                             -9.999999999999999E+384
    U34                              9.999999999999999999999999999999999E+6144
    V16                              9.999999999999999E+384
    W                               |
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
