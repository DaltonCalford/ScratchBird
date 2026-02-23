#coding:utf-8

"""
ID:          issue-6232
ISSUE:       6232
TITLE:       Firebird crashes due to concurrent operations with expression indices
DESCRIPTION:
  Scenario for reproducing was given by Vlad, letter 25-02-2020 19:15.
  Unfortuinately, this crash can occur only in developer build rather than release one.

  Although issues from ticket can NOT be reproduced, it was encountered in 2.5.0.26074
  that statements from here lead DB to be corrupted:
      Error while commiting transaction:
      - SQLCODE: -902
      - database file appears corrupt...
      - wrong page type
      - page 0 is of wrong type (expected 6, found 1)
      -902
      335544335
  No such problem in 2.5.1 and above.
  Decided to add this test just for check that DB will not be corrupted.
  TICKET ISSUE REMAINS IRREPRODUCIBLE (checked on following SuperServer builds: 2.5.1, 2.5.6, 2.5.9, 3.0.6, 4.0.0).
JIRA:        CORE-5980
FBTEST:      bugs.core_5980
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con1, act.db.connect() as con2:
        con1.execute_immediate('recreate table t1(id int)')
        con1.execute_immediate('create index t1_idx on t1 computed by (id + 0)')
        con1.commit()
        c = con1.cursor()
        c.execute('insert into t1(id) values(?)', [1])
        con1.commit()
        # this lead to corruption of database in 2.5.0
        # page 0 is of wrong type (expected 6, found 1)
        con2.execute_immediate('alter index t1_idx active')
        con2.commit()
