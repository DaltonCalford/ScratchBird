#coding:utf-8

"""
ID:          issue-3058
ISSUE:       3058
TITLE:       Incorrect "string right truncation" error with NONE column and multibyte connection charset
DESCRIPTION:
JIRA:        CORE-2651
FBTEST:      bugs.core_2651
"""

import pytest
from firebird.qa import *

init_script = """create table TEST_NONE
(VARCHAR_1 VARCHAR(1) CHARACTER SET NONE);

insert into test_none values (ascii_char(1));
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect(charset='CP943C') as con:
        c = con.cursor()
        try:
            c.execute("select * from TEST_NONE")
        except:
            pytest.fail("Test FAILED")


