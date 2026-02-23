#coding:utf-8

"""
ID:          issue-2445
ISSUE:       2445
TITLE:       NOT NULL procedure parameters
DESCRIPTION:
JIRA:        CORE-2008
FBTEST:      bugs.core_2008
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter procedure test_procedure(id int not null) as begin end;
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select rdb$parameter_name p_name, rdb$null_flag n_flag
    from rdb$procedure_parameters
    where rdb$procedure_name=upper('test_procedure');
"""

act = isql_act('db', test_script)

expected_stdout = """
    P_NAME                          ID
    N_FLAG                          1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

