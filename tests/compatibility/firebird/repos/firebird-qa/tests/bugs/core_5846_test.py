#coding:utf-8

"""
ID:          issue-6107
ISSUE:       6107
TITLE:       CREATE VIEW issues "Implementation of text subtype 512 not located"
DESCRIPTION:
JIRA:        CORE-5846
FBTEST:      bugs.core_5846
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    alter character set utf8 set default collation unicode;
    create table table1( fld1 char(1) character set none );

    -- NB: 'expected_stderr' must remain empty as result of following command.
    create view view1 as
    select fld1 from table1
    ;
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.execute()
