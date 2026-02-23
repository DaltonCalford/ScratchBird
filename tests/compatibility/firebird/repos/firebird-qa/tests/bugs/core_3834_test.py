#coding:utf-8

"""
ID:          issue-4176
ISSUE:       4176
TITLE:       Usage of a NATURAL JOIN with a derived table crashes the server
DESCRIPTION:
JIRA:        CORE-3834
FBTEST:      bugs.core_3834
NOTES:
    [07.04.2022] pzotov
    1. No need to use .fbk from original ticket: FB 2.5.0 crashes when executing trivial query to mon$ tables.
      File 'core3834.fbk' can be removed from repo.
    2. FB 5.0.0.455 and later: data sources with equal cardinality now present in the HASH plan in order they are specified in the query.
       Reversed  order was used before this build. Because of this, two cases of expected stdout must be taken in account, see variables
       'fb3x_checked_stdout' and 'fb5x_checked_stdout'.
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set planonly;
    -- This query caused FB 2.5.0 to crash:
    select *
    from (select * from mon$attachments a_mon_att_inner)
    a_mon_att_outer natural join mon$statements a_mon_sttm
    where a_mon_sttm.mon$stat_id = ?;
    quit;
"""

act = isql_act('db', test_script)

fb3x_checked_stdout = """
    PLAN HASH (A_MON_STTM NATURAL, A_MON_ATT_OUTER A_MON_ATT_INNER NATURAL)
"""

fb5x_checked_stdout = """
    PLAN HASH (A_MON_ATT_OUTER A_MON_ATT_INNER NATURAL, A_MON_STTM NATURAL)
"""

fb6x_checked_stdout = """
    PLAN HASH ("A_MON_ATT_OUTER" "A_MON_ATT_INNER" NATURAL, "A_MON_STTM" NATURAL) 
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = fb3x_checked_stdout if act.is_version('<5') else fb5x_checked_stdout if act.is_version('<6') else fb6x_checked_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
