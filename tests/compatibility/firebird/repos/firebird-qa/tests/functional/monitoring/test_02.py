#coding:utf-8

"""
ID:          monitoring-tables-02
TITLE:       Get data about active statements from current attachment (WHERE-filter: mon$statements.mon$state=1).
DESCRIPTION:
FBTEST:      functional.monitoring.02
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set width usr 31;
    set list on;
    select a.mon$user usr, s.mon$sql_text sql_text
    from mon$attachments a
        join mon$statements s on a.mon$attachment_id = s.mon$attachment_id
    where
        a.mon$attachment_id = current_connection
        and s.mon$state = 1;
"""

act = isql_act('db', test_script, substitutions=[('SQL_TEXT.*', '')])

expected_stdout = """
    USR                             SYSDBA
    SQL_TEXT                        0:1
    select a.mon$user usr, s.mon$sql_text sql_text
    from mon$attachments a
        join mon$statements s on a.mon$attachment_id = s.mon$attachment_id
    where
        a.mon$attachment_id = current_connection
        and s.mon$state = 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
