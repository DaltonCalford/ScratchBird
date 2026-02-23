#coding:utf-8

"""
ID:          issue-3659
ISSUE:       3659
TITLE:       New pseudocolumn (RDB$RECORD_VERSION) to get number of the transaction that created a record version
DESCRIPTION:
JIRA:        CORE-3291
FBTEST:      bugs.core_3291
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(id int);
    insert into test values(1) returning current_transaction - rdb$record_version as diff_ins;
    commit;
    update test set id=-id returning current_transaction - rdb$record_version as diff_upd;
    commit;
    delete from test returning sign(current_transaction - rdb$record_version) as diff_del;
"""

act = isql_act('db', test_script)

expected_stdout = """
    DIFF_INS                        0
    DIFF_UPD                        0
    DIFF_DEL                        1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

