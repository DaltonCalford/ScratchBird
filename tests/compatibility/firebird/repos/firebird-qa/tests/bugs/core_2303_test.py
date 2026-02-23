#coding:utf-8

"""
ID:          issue-2727
ISSUE:       2727
TITLE:       Include PLAN in mon$statements
DESCRIPTION:
JIRA:        CORE-2303
FBTEST:      bugs.core_2303
NOTES:
    [26.06.2025] pzotov
    Slightly refactored: made code more readable.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

TAG_TEXT = 'TAG_FOR_SEARCH'

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur1 = con.cursor()
        cur2 = con.cursor()
        ps = None
        try:
            ps = cur1.prepare(f'select 1 /* {TAG_TEXT} */ from rdb$database')
            cur2.execute(f"select mon$sql_text, mon$explained_plan from mon$statements s where s.mon$sql_text containing '{TAG_TEXT}' and s.mon$sql_text NOT containing 'mon$statements'")
            for r in cur2:
                print(r[0])
                print(r[1])
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()

    expected_stdout_5x = f"""
        select 1 /* {TAG_TEXT} */ from rdb$database
        Select Expression
            -> Table "RDB$DATABASE" Full Scan
    """

    expected_stdout_6x = f"""
        select 1 /* {TAG_TEXT} */ from rdb$database
        Select Expression
        -> Table "SYSTEM"."RDB$DATABASE" Full Scan
    """
    expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
