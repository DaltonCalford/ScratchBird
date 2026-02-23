#coding:utf-8

"""
ID:          issue-3118
ISSUE:       3118
TITLE:       Storage of malformed blob is allowed when copying from a blob with NONE/OCTETS charset
DESCRIPTION:
JIRA:        CORE-2722
FBTEST:      bugs.core_2722
"""

import pytest
from firebird.qa import *

init_script = """create table t (b1 blob sub_type text, b2 blob sub_type text character set utf8);
"""

db = db_factory(init=init_script)

test_script = """-- This is correct and raise "Malformed string" error
insert into t (b2) values (x'F0');

insert into t (b1) values (x'F0');

-- This should raise second "Malformed string" error
update t set b2 = b1;
"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 22000
Malformed string
Statement failed, SQLSTATE = 22000
Malformed string
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

