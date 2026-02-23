#coding:utf-8

"""
ID:          issue-4959
ISSUE:       4959
TITLE:       internal Firebird consistency check (cannot find tip page (165), file: tra.cpp line: 2375)
DESCRIPTION:
JIRA:        CORE-4645
FBTEST:      bugs.core_4645
"""

import pytest
from firebird.qa import *
from firebird.driver import DbAccessMode

db = db_factory()

act = python_act('db')

script = """
commit;
set transaction read committed;
set term ^;
execute block as
    declare n int = 20000;
begin
    while (n>0) do
        in autonomous transaction do
        select :n-1 from rdb$database into n;
end
^
set term ;^
commit;
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.connect_server() as srv:
        srv.database.set_access_mode(database=act.db.db_path, mode=DbAccessMode.READ_ONLY)
    act.isql(switches=[], input=script)
    assert act.clean_stdout == act.clean_expected_stdout
