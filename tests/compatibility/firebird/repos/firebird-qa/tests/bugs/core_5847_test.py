#coding:utf-8

"""
ID:          issue-6108
ISSUE:       6108
TITLE:       "Malformed string" instead of key value in PK violation error message
DESCRIPTION:
JIRA:        CORE-5847
FBTEST:      bugs.core_5847
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    recreate table test(
        uid char(16) character set octets,
        constraint test_uid_pk primary key(uid) using index test_uid_pk
    );
    commit;
    insert into test values( gen_uuid() );
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect(charset='utf8') as con1, act.db.connect() as con2:
        c1 = con1.cursor()
        c2 = con2.cursor()
        for c in [c1, c2]:
            with pytest.raises(DatabaseError, match='.*Problematic key value is.*'):
                c.execute('insert into test(uid) select uid from test rows 1')
