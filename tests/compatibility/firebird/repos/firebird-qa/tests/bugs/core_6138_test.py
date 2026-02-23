#coding:utf-8

"""
ID:          issue-6387
ISSUE:       6387
TITLE:       Inconsistent behavior regarding visibility of master record on detail inserts
DESCRIPTION:
  Confirmed bug on: 3.0.5.33152 (built 14.09.19), 4.0.0.1598 (built 08.09.19):
  no error raised when second attach tried to insert record into child table
  after first attach did commit (but main record was not visible to 2nd attach
  because of SNAPSHOT isolation level).
JIRA:        CORE-6138
FBTEST:      bugs.core_6138
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, DatabaseError

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    custom_tpb = tpb(isolation=Isolation.CONCURRENCY, lock_timeout=0)
    with act.db.connect() as con1, act.db.connect() as con2:
        con2.begin(custom_tpb)

        con1.execute_immediate('create table a (id int primary key)')
        con1.execute_immediate('create table b (id int primary key, id_a int, constraint fk_b__a foreign key(id_a) references a(id) on update cascade on delete cascade)')
        con1.commit()

        con1.begin(custom_tpb)
        cur1 = con1.cursor()
        cur1.execute('insert into a(id) values( ? )', [1])

        con2.commit()
        con2.begin(custom_tpb)
        cur2 = con2.cursor()
        cur2.execute('select id from a')

        con1.commit()

        with pytest.raises(DatabaseError, match='.*violation of FOREIGN KEY constraint.*'):
            cur2.execute('insert into b (id, id_a) values (?, ?)', [1, 1])
