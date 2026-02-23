#coding:utf-8

"""
ID:          issue-3765
ISSUE:       3765
TITLE:       Allow write operations to temporary tables in read only transactions
DESCRIPTION:
JIRA:        CORE-3399
FBTEST:      bugs.core_3399
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3399-read_only.fbk', async_write=False)

test_script = """
    -- ======= from the ticket: ========
    -- Implementation allows:
    -- 1) writes into all kind of GTT's in read only transactions in read write database
    -- and also
    -- 2) make writabe GTT ON COMMIT DELETE in read only transactions in read only database.
    -- =================================
    -- Database will be in the state "force write, no reserve, read only".
    -- This test verifies only SECOND issue from ticket: allow GTT with attribute "on commit DELETE rows"
    -- to be writeable when database is READ-ONLY.
    commit;
    set transaction read only;
    insert into gtt_del_rows(id) values(1);
    select * from gtt_del_rows;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
         ID
    =======
          1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

