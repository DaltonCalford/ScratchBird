#coding:utf-8

"""
ID:          issue-6098
ISSUE:       6098
TITLE:       Inconsistent results when working with GLOBAL TEMPORARY TABLE ON COMMIT PRESERVE ROWS
DESCRIPTION:
  Samples were provided by Vlad, privately.
  Confirmed bug on 3.0.4.32972, 4.0.0.955; SUPERSERVER only (see also note in the ticket)
JIRA:        CORE-5837
FBTEST:      bugs.core_5837
"""

import pytest
from firebird.qa import *

init_script = """
    recreate global temporary table gtt(id int) on commit preserve rows;
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    with act.db.connect() as con1, act.db.connect() as con2:
        c2 = con2.cursor()
        # Following 'select count' is MANDATORY for reproduce:
        c2.execute('select count(*) from gtt').fetchall()
        #
        c1 = con1.cursor()
        c1.execute('insert into gtt(id) values(?)', [1])
        c1.execute('insert into gtt(id) values(?)', [1])
        #
        c2.execute('insert into gtt(id) values(?)', [2])
        #
        con1.rollback()
        #
        c2.execute('insert into gtt(id) select 2 from rdb$types rows 200', [2])
        con2.commit()
        #
        c1.execute('insert into gtt(id) values(?)', [11])
        c1.execute('insert into gtt(id) values(?)', [11])
        #
        con1.rollback()
    # This test does not need to assert anything, it passes if we get here without error
