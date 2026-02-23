#coding:utf-8

"""
ID:          issue-3706
ISSUE:       3706
TITLE:       Error in autonomous transaction with empty exception handler: can insert duplicate values into PK/UK column (leads to unrestorable backup)
DESCRIPTION:
JIRA:        CORE-3340
FBTEST:      bugs.core_3340
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table tmp(id int not null primary key using index tmp_id_pk);
    commit;
    set transaction no wait isolation level read committed;
    set term ^;
    execute block as
    begin
          insert into tmp values(1);
          insert into tmp values(2);
          in autonomous transaction do
          begin
              insert into tmp values(1);
              when any do begin
                  -- nop --
              end
          end
    end^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select id from tmp;
    select count(*) from tmp;
    commit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 1
    ID 2
    
    COUNT 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

