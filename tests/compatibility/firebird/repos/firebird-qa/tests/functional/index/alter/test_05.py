#coding:utf-8

"""
ID:          index.alter-05
TITLE:       ALTER INDEX - INACTIVE FOREIGN KEY
DESCRIPTION: An index participating in FOREIGN KEY constraint cannot be deactivated
FBTEST:      functional.index.alter.05
NOTES:
    [08.08.2024] pzotov
    Splitted expected* text because system triggers now are created in C++/GDML code
    See https://github.com/FirebirdSQL/firebird/pull/8202
    Commit (05-aug-2024 13"45):
    https://github.com/FirebirdSQL/firebird/commit/0cc8de396a3c2bbe13b161ecbfffa8055e7b4929
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(
        id int primary key using index test_pk
       ,pid int references test(id) using index test_fk
    );
"""

db = db_factory(init = init_script)

test_script = "alter index test_fk inactive;"

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 27000
    unsuccessful metadata update
    -ALTER INDEX TEST_FK failed
    -action cancelled by trigger (2) to preserve data integrity
    -Cannot deactivate index used by an integrity constraint
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER INDEX "PUBLIC"."TEST_FK" failed
    -Cannot deactivate index used by an integrity constraint
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
