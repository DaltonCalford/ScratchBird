#coding:utf-8

"""
ID:          issue-615
ISSUE:       615
TITLE:       Blob comparison with constant
DESCRIPTION:
JIRA:        CORE-284
FBTEST:      bugs.core_0284
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T1 (PK INTEGER NOT NULL, COL1 BLOB SUB_TYPE TEXT);
commit;
insert into T1 (PK,COL1) values (1,'text');
insert into T1 (PK,COL1) values (2,'');
commit;
"""

db = db_factory(init=init_script)

test_script = """select * from T1 where COL1 = '';
select * from T1 where COL1 = 'text';
commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
          PK              COL1
============ =================
           2              80:1
==============================================================================
COL1:

==============================================================================


          PK              COL1
============ =================
           1              80:0
==============================================================================
COL1:
text
==============================================================================

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

