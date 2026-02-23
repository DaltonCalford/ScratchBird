#coding:utf-8

"""
ID:          issue-2658
ISSUE:       2658
TITLE:       Implement domain check of input parameters of execute block
DESCRIPTION:
JIRA:        CORE-2230
FBTEST:      bugs.core_2230
NOTES:
    [26.06.2025] pzotov
    Slightly refactored: made code more readable.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    create domain dm_test as integer not null check (value in (0, 1));
"""

db = db_factory(init=init_script)
act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur = con.cursor()
        ps, rs = None, None
        try:
            ps = cur.prepare('execute block (a_input_value dm_test = ?) returns (y integer) as begin y = a_input_value; suspend; end')
            for x in (1, 11):
                rs = cur.execute(ps, (x,))
                for r in cur:
                    print(r[0])
        except DatabaseError as e:
            print(e.__str__())
            for x in e.gds_codes:
                print(x)
        finally:
            if rs:
                rs.close()
            if ps:
                ps.free()

    expected_stdout_5x = """
        1
        validation error for variable A_INPUT_VALUE, value "11"
        335544879
    """

    expected_stdout_6x = """
        1
        validation error for variable "A_INPUT_VALUE", value "11"
        335544879
    """
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
