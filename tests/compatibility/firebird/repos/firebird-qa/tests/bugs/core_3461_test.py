#coding:utf-8

"""
ID:          issue-3822
ISSUE:       3822
TITLE:       DDL operations fail after backup/restore
DESCRIPTION:
JIRA:        CORE-3461
FBTEST:      bugs.core_3461
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3461.fbk')

test_script = """
    set autoddl off;
    set term ^ ;
    drop table test_tbl ^
    alter procedure test_proc(id integer) as begin end ^
    alter table test_tbl2 add id2 integer ^
    alter procedure test_tbl_proc as
    declare id integer;
    declare id2 integer;
    begin
      select id, id2 from test_tbl2 into :id, :id2;
    end ^
    commit^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
