#coding:utf-8

"""
ID:          tabloid.test_dsqlrequests
TITLE:       DROP TABLE fails if executing in ES and DML at the same execute block is performed in autonomous transaction
DESCRIPTION:
    Regression detected in 5.0.0.435, build 19.03.2022
    https://github.com/FirebirdSQL/firebird/commit/6214b12028501cdc2875ad7d0deb62c4822060b8
    Checked on 5.0.0.964, 4.0.3.2903, 3.0.11.33664.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    recreate table test (id int primary key)^
    commit^
    execute block as
    begin
      execute statement 'drop table test';
      in autonomous transaction do
        execute statement ( 'insert into test values ( ? ) ') (1);
    end^
    commit^
"""

act = isql_act('db', test_script)

expected_stderr = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
