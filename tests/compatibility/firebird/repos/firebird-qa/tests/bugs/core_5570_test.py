#coding:utf-8

"""
ID:          issue-5837
ISSUE:       5837
TITLE:       Negative infinity (double) shown incorrectly without sign in isql
DESCRIPTION:
  Bug was in ISQL. We do insert in the table with two DP fields special values which
  are "-1.#INF" and "1.#INF" (at least in such view they are represented in the trace).
  These values are defined in Python class Decimal as literals '-Infinity' and 'Infinity'.
  After this we try to query this table. Expected result: "minus" sign should be shown
  leftside of negative infinity.
JIRA:        CORE-5570
FBTEST:      bugs.core_5570
"""

import pytest
from decimal import Decimal
from firebird.qa import *

init_script = """
      recreate table test(x double precision, y double precision);
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
    X                               -Infinity
    Y                               Infinity
    Records affected: 1
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
     with act.db.connect() as con:
          c = con.cursor()
          c.execute('insert into test(x, y) values(?, ?)', [Decimal('-Infinity'), Decimal('Infinity')])
          con.commit()
     act.expected_stdout = expected_stdout
     act.isql(switches=[], input="set list on; set count on; select * from test;")
     assert act.clean_stdout == act.clean_expected_stdout
