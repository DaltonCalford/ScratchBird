#coding:utf-8

"""
ID:          issue-1495
ISSUE:       1495
TITLE:       SINGULAR buggy when nulls present
DESCRIPTION:
JIRA:        CORE-1073
FBTEST:      bugs.core_1073
"""

import pytest
from firebird.qa import *

init_script = """create table t (a integer);
commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

def check(step, cur, statement, exp):
   r = cur.execute(statement).fetchone()
   if (exp and (r is None)) or (not exp and (r is not None)):
      pytest.fail(f'Test FAILED in step {step}, expectation {exp}')

@pytest.mark.version('>=3')
def test_1(act: Action):
   with act.db.connect() as con:
      c = con.cursor()
      #
      p_singular = 'select 1 from rdb$database where singular(select * from t where a = 1)'
      n_singular = 'select 1 from rdb$database where not(singular(select * from t where a = 1))'
      p_nsingular = 'select 1 from rdb$database where not singular( select * from t where a = 1)'
      n_nsingular = 'select 1 from rdb$database where not(not singular(select * from t where a = 1))'
      #
      ins = 'insert into t values (%s)'
      #
      # Step 1
      #
      c.execute(ins % '2')
      c.execute(ins % 'null')
      con.commit()
      #
      check(1, c, p_singular, False)
      check(1, c, n_singular, True)
      check(1, c, p_nsingular, True)
      check(1, c, n_nsingular, False)
      #
      c.execute('delete from t')
      con.commit()
      #
      # Step 2
      #
      c.execute(ins % '1')
      c.execute(ins % 'null')
      con.commit()
      #
      check(2, c, p_singular, True)
      check(2, c, n_singular, False)
      check(2, c, p_nsingular, False)
      check(2, c, n_nsingular, True)
      #
      c.execute('delete from t')
      con.commit()
      #
      # Step 3
      #
      c.execute(ins % '1')
      c.execute(ins % 'null')
      c.execute(ins % '1')
      con.commit()
      #
      check(3, c, p_singular, False)
      check(3, c, n_singular, True)
      check(3, c, p_nsingular, True)
      check(3, c, n_nsingular, False)
      #
      c.execute('delete from t')
      con.commit()
      #
      # Step 4
      #
      c.execute(ins % '1')
      c.execute(ins % '1')
      c.execute(ins % 'null')
      con.commit()
      #
      check(4, c, p_singular, False)
      check(4, c, n_singular, True)
      check(4, c, p_nsingular, True)
      check(4, c, n_nsingular, False)
      #
      c.execute('delete from t')
      con.commit()



