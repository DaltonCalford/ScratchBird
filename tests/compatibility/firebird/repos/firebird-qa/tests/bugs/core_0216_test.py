#coding:utf-8

"""
ID:          issue-544
ISSUE:       544
TITLE:       Too many grants lose privileges
DESCRIPTION:
  Issuing more than 2000 grants on any one object causes
  an internal buffer flow in generating the access
  control list that actually enforces the rights.
JIRA:        CORE-216
FBTEST:      bugs.core_0216
"""

import pytest
from firebird.qa import *

init_script = """create table T (PK integer);
create table LOG(PK integer);
"""

db = db_factory(page_size=4096, init=init_script)

act = python_act('db')

@pytest.mark.version('>=2.5')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        # Create 4000 triggers on table T
        i = 1
        cmd = """create trigger LOGT_%d for T after insert as
        begin
          insert into log (PK) values (new.pk);
        end
        """
        while i <= 4000:
            c.execute(cmd % i)
            i += 1
        con.commit()
        # Grants
        i = 1
        cmd = """GRANT INSERT ON LOG TO TRIGGER LOGT_%d"""
        while i <= 4000:
            c.execute(cmd % i)
            i += 1
        con.commit()


