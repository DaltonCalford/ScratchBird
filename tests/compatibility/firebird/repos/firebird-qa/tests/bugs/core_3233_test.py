#coding:utf-8

"""
ID:          issue-3605
ISSUE:       3605
TITLE:       LIKE, STARTING and CONTAINING fail if second operand >= 32K
DESCRIPTION:
JIRA:        CORE-3233
FBTEST:      bugs.core_3233
"""

import pytest
from firebird.qa import *

init_script = """create table blobz (zin blob sub_type 1);
insert into blobz (zin) values ('woord');
commit;
"""

db = db_factory(init=init_script)

test_script = """select 1 from blobz where zin like cast(cast('woord' as char(32766)) as blob sub_type 1) || '!' or zin='woord';
select 1 from blobz where zin like cast(cast('woord' as char(32767)) as blob sub_type 1) || '!' or zin='woord';
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT
============
           1

    CONSTANT
============
           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

